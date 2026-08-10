// -*- coding: utf-8 -*-
// Couche OS du portage : arene, tas, temps, interruptions, threads.
//
// Rien ici n'imite le materiel. Le jeu n'a besoin que du **contrat** de l'OS GameCube :
// une arene contigue dont il decoupe ses tas, une horloge monotone au rythme du time base,
// et des sections critiques. Ce fichier fournit ce contrat au-dessus de la libc hote.
//
// Deux choix a expliciter, parce qu'ils sont faux a terme et justes pour booter :
//
//  * **les tas sont de vrais tas, pas des bouchons.** RE4 alloue et libere sans arret pendant
//    le chargement d'une salle ; un allocateur qui ne rend jamais la memoire remplirait l'arene
//    en quelques secondes. Liste libre simple avec fusion des blocs voisins.
//  * **les threads ne sont pas ordonnances.** OSCreateThread enregistre, OSResumeThread ne
//    demarre rien. RE4 s'en sert surtout pour le chargement en fond ; tant que le flux
//    principal tourne, cela suffit a atteindre le premier ecran, et cela se verra tout de suite
//    si ce n'est pas le cas (le jeu attendra un thread qui ne s'execute jamais).
#include "port_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <time.h>

#include <sys/mman.h>

#include <dolphin.h>

namespace {

// ---------------------------------------------------------------- arene

// **MEM1 est place a son adresse d origine, 0x80000000.**
//
// Ce n est pas un detail de confort. RE4 ne demande pas de la memoire a l OS pour y tailler ses
// zones : il les ecrit en dur. `SystemMemInit` pose heap_end a 0x817F4000, le tas son a
// 0x803F0000, le FIFO a 0x80460000, le framebuffer a 0x80578000, et refuse de demarrer si
// `OSGetArenaLo()` depasse 0x8034FFFF - c est le controle "ELF size overflow" de
// main_mem.cpp:100, sur lequel le premier lancement avec le vrai disque s est arrete.
//
// Rendre un bloc quelconque (calloc) ne pouvait donc pas marcher : le jeu n en aurait rien
// fait et aurait ecrit a 0x80000000 de toute facon. Sur Linux 32 bits l espace utilisateur va
// jusqu a 0xBFFFFFFF, donc cette adresse **est** disponible : on la reserve avec mmap MAP_FIXED
// et toutes les adresses absolues du jeu redeviennent valides, sans toucher une ligne de son
// code.
//
// 32 Mo plutot que 24 : SysMem place ses zones usb et debug juste au-dessus de MEM1
// (0x81800000 et 0x8181FB00). Elles ne servent qu au mode developpement, mais les cartographier
// coute une plage d adresses et evite une faute de segmentation lointaine.
const u32 MEM1_BASE = 0x80000000u;
const u32 MEM1_SIZE = 32u * 1024u * 1024u;
const u32 ARENA_SIZE = 24u * 1024u * 1024u;   // ce que la console annonce au jeu

// Ou commence l arene rendue au jeu. Sur console c est la fin du DOL charge ; ici le code vit
// ailleurs, donc on choisit une valeur qui satisfait le controle de main_mem.cpp et laisse la
// memoire basse tranquille (le jeu y range des variables a adresse fixe).
const u32 ARENA_LO = 0x80300000u;
const u32 ARENA_HI = 0x81800000u;

u8 *g_arena = nullptr;
u8 *g_arena_lo = nullptr;
u8 *g_arena_hi = nullptr;

void arena_init()
{
    if (g_arena)
        return;
    void *want = (void *) (unsigned long) MEM1_BASE;
    void *got = ::mmap(want, MEM1_SIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (got == MAP_FAILED || got != want) {
        // Sans MEM1 a la bonne adresse, rien de ce qui suit n a de sens : le jeu ecrirait
        // aussitot hors de toute zone valide. Mieux vaut le dire ici que laisser une faute de
        // segmentation sans explication trois appels plus loin.
        std::printf("[PORT] OSInit : impossible de reserver MEM1 a 0x%08X\n", MEM1_BASE);
        std::fflush(stdout);
        std::abort();
    }
    std::memset(got, 0, MEM1_SIZE);
    g_arena = (u8 *) got;
    g_arena_lo = (u8 *) (unsigned long) ARENA_LO;
    g_arena_hi = (u8 *) (unsigned long) ARENA_HI;
}

// ---------------------------------------------------------------- tas

// L'en-tete est **complete a 32 octets**, et ce n'est pas cosmetique : le bloc rendu est
// `base + sizeof(Block)`, donc si l'en-tete ne fait pas un multiple de 32 le pointeur ne l'est
// pas non plus. GX exige 32 octets pour les listes d'affichage et les textures ; avec un
// en-tete de 16 (u32 + pointeur + int + u32 en ARM 32 bits) tout tenait quand meme et la
// corruption ne serait apparue qu'au rendu, sans rapport visible avec l'allocateur.
// Trouve par port/test_osdvd.cpp sur l'appareil, pas par relecture.
struct Block {
    u32 size;       // taille utile, hors en-tete
    Block *next;    // chainage de la liste libre, ou nullptr si alloue
    int heap;
    u32 free;
    u8 pad[32 - (sizeof(u32) + sizeof(void *) + sizeof(int) + sizeof(u32))];
};

const int MAX_HEAPS = 16;

struct Heap {
    u8 *start;
    u8 *end;
    Block *freelist;
    bool used;
};

Heap g_heaps[MAX_HEAPS];
int g_nheaps = 0;

Block *block_of(void *p) { return (Block *)((u8 *)p - sizeof(Block)); }

void heap_insert_free(Heap &h, Block *b)
{
    b->free = 1;
    // Insertion triee par adresse, puis fusion avec les voisins : sans cela la liste se
    // fragmente et un chargement de salle finit par echouer alors que la place existe.
    Block **pp = &h.freelist;
    while (*pp && *pp < b)
        pp = &(*pp)->next;
    b->next = *pp;
    *pp = b;

    if (b->next && (u8 *)b + sizeof(Block) + b->size == (u8 *)b->next) {
        b->size += sizeof(Block) + b->next->size;
        b->next = b->next->next;
    }
    for (Block *c = h.freelist; c && c != b; c = c->next) {
        if ((u8 *)c + sizeof(Block) + c->size == (u8 *)b) {
            c->size += sizeof(Block) + b->size;
            c->next = b->next;
            break;
        }
    }
}

} // namespace

// `__OSCurrHeap` est declare par le SDK mais defini dans src/lib/OSAlloc.c, que le portage ne
// lie pas puisqu'il remplace tout OSAlloc. On le fournit ici.
volatile OSHeapHandle __OSCurrHeap = -1;

// ---------------------------------------------------------------- API OS

void OSInit(void)
{
    arena_init();
    std::memset(g_heaps, 0, sizeof(g_heaps));
    g_nheaps = 0;
    std::printf("[PORT] OSInit : arene %u Mo a %p\n", ARENA_SIZE / (1024u * 1024u),
                (void *)g_arena);
}

void *OSGetArenaLo(void) { arena_init(); return g_arena_lo; }
void *OSGetArenaHi(void) { arena_init(); return g_arena_hi; }
void OSSetArenaLo(void *lo) { g_arena_lo = (u8 *)lo; }
void OSSetArenaHi(void *hi) { g_arena_hi = (u8 *)hi; }

void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps)
{
    arena_init();
    (void)maxHeaps;
    std::memset(g_heaps, 0, sizeof(g_heaps));
    g_nheaps = 0;
    (void)arenaEnd;
    return arenaStart;   // le jeu reprend l'arene a partir d'ici
}

int OSCreateHeap(void *start, void *end)
{
    if (g_nheaps >= MAX_HEAPS || !start || !end || end <= start)
        return -1;
    Heap &h = g_heaps[g_nheaps];
    // Aligner aussi le debut du tas : sinon le premier bloc herite du desalignement de l'arene.
    h.start = (u8 *)(((unsigned long)start + 31u) & ~31ul);
    h.end = (u8 *)end;
    h.used = true;
    Block *b = (Block *)h.start;
    b->size = (u32)((h.end - h.start) - sizeof(Block));
    b->next = nullptr;
    b->heap = g_nheaps;
    b->free = 1;
    h.freelist = b;
    return g_nheaps++;
}

void OSDestroyHeap(int heap)
{
    if (heap >= 0 && heap < g_nheaps)
        g_heaps[heap].used = false;
}

void *OSAllocFromHeap(int heap, u32 size)
{
    if (heap < 0 || heap >= g_nheaps || !g_heaps[heap].used)
        return nullptr;
    Heap &h = g_heaps[heap];
    size = (size + 31u) & ~31u;          // le materiel exige 32 octets d'alignement
    Block **pp = &h.freelist;
    for (; *pp; pp = &(*pp)->next) {
        Block *b = *pp;
        if (b->size < size)
            continue;
        if (b->size >= size + sizeof(Block) + 32u) {   // scinder
            Block *rest = (Block *)((u8 *)b + sizeof(Block) + size);
            rest->size = b->size - size - (u32)sizeof(Block);
            rest->next = b->next;
            rest->heap = heap;
            rest->free = 1;
            b->size = size;
            *pp = rest;
        } else {
            *pp = b->next;
        }
        b->next = nullptr;
        b->heap = heap;
        b->free = 0;
        return (u8 *)b + sizeof(Block);
    }
    std::printf("[PORT] OSAllocFromHeap : tas %d plein, %u octets refuses\n", heap, size);
    return nullptr;
}

void OSFreeToHeap(int heap, void *ptr)
{
    if (!ptr || heap < 0 || heap >= g_nheaps)
        return;
    Block *b = block_of(ptr);
    if (b->free)
        return;                          // double liberation : ignoree, pas fatale
    heap_insert_free(g_heaps[heap], b);
}

int OSSetCurrentHeap(int heap) { int old = __OSCurrHeap; __OSCurrHeap = heap; return old; }
s32 OSCheckHeap(int heap) { (void)heap; return 0; }

// ---------------------------------------------------------------- temps

// Le time base du GameCube avance a un quart de l'horloge bus (162 MHz / 4).
const u64 TB_HZ = 40500000ull;

static u64 host_ns()
{
    // `timespec_get` n'apparait dans la libc d'Android qu'a l'API 29 ; clock_gettime existe
    // partout et CLOCK_MONOTONIC est ce qu'il faut pour une horloge de jeu (insensible aux
    // changements d'heure).
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}

OSTime OSGetTime(void)
{
    static const u64 t0 = host_ns();
    return (OSTime)(((host_ns() - t0) / 1000ull) * (TB_HZ / 1000000ull));
}

OSTick OSGetTick(void) { return (OSTick)OSGetTime(); }

// ---------------------------------------------------------------- sections critiques

// Il n'y a pas d'interruptions a masquer : le portage est mono-flux tant que l'ordonnanceur
// n'existe pas. On rend l'etat precedent pour que OSRestoreInterrupts le retablisse.
namespace { int g_intr = 1; }

BOOL OSDisableInterrupts(void) { int o = g_intr; g_intr = 0; return o; }
BOOL OSEnableInterrupts(void) { int o = g_intr; g_intr = 1; return o; }
BOOL OSRestoreInterrupts(BOOL level) { int o = g_intr; g_intr = level; return o; }

// ---------------------------------------------------------------- divers

u32 OSGetConsoleType(void) { return 0x10000002u; }   // une Retail 2, ce que le jeu attend
u32 OSGetConsoleSimulatedMemSize(void) { return ARENA_SIZE; }
BOOL OSGetResetButtonState(void) { return FALSE; }   // declare BOOL dans os/OSResetSW.h
// OSPhysicalToCached est aussi une **macro** dans os.h : la definir ici ne sert a rien,
// la macro reecrirait le nom. On la laisse au SDK.

void OSPanic(const char *file, int line, const char *msg, ...)
{
    std::printf("[PORT] OSPanic %s:%d : %s\n", file ? file : "?", line, msg ? msg : "");
    std::fflush(stdout);
    std::abort();
}

// ---------------------------------------------------------------- cache verrouille

// Le Gekko sait verrouiller 16 Ko de son cache L1 et les exposer comme de la memoire adressable
// a 0xE0000000. RE4 y range sa **palette de matrices** de skinning (une matrice 4x3 de 0x30
// octets par os), puis la lit depuis le code de transformation.
//
// Sur ARM il n'y a rien a verrouiller, et il n'y a surtout pas besoin : c'est de la memoire
// ordinaire, simplement rapide parce que petite et constamment relue - donc deja dans le cache.
// Ce tampon en tient lieu. Ce qui compte est qu'il soit **le meme** pour l'ecriture (trans.cpp
// range les matrices) et pour la lecture (CalcSk1_x), d'ou le point d'acces unique ci-dessous.
static u8 g_locked_cache[16 * 1024] __attribute__((aligned(32)));

extern "C" void *port_lc_base(void) { return g_locked_cache; }

void LCEnable(void) {}
void LCDisable(void) {}
u32 LCQueueLength(void) { return 0; }
void LCQueueWait(u32) {}
void LCFlushQueue(void) {}

// ---------------------------------------------------------------- cache et MSR

// Le GameCube expose son cache a la main : le jeu vide une plage avant de la donner au GPU, et
// l'invalide avant de la relire. Sur ARM avec un GPU **coherent** en memoire partagee (et sans
// DMA ecrit a la main ici), il n'y a rien a faire - mais il faut quand meme les definir, sinon
// c'est le lien qui echoue, pas le rendu.
void DCFlushRange(void *, u32) {}
void DCFlushRangeNoSync(void *, u32) {}
void DCStoreRange(void *, u32) {}
void DCStoreRangeNoSync(void *, u32) {}
void DCInvalidateRange(void *, u32) {}
void DCZeroRange(void *addr, u32 n) { std::memset(addr, 0, n); }
void DCTouchRange(void *, u32) {}
void ICInvalidateRange(void *, u32) {}
void ICFlashInvalidate(void) {}

// Le registre d'etat machine du PowerPC. Le jeu s'en sert pour basculer les interruptions et le
// mode flottant ; les deux sont deja pris en charge ailleurs dans le portage.
void PPCMtmsr(u32) {}
u32 PPCMfmsr(void) { return 0; }
void PPCSync(void) {}
void PPCHalt(void) { std::abort(); }

BOOL DBIsDebuggerPresent(void) { return FALSE; }
