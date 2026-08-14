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
    // Memoire basse que l'IPL de la console remplit avant le jeu (dolphin/os.h) : le SDK la lit directement.
    // Horloge du bus a 0 = budget DSP d'AX nul (toutes les voix jetees, __AXSyncPBs) et OS_TIMER_CLOCK nul dans
    // toutes les conversions ticks <-> temps (OSTicksToMilliseconds...). Valeurs de la GameCube ; TB_HZ = bus / 4.
    {
        u32 *low = (u32 *) got;
        low[0x28 / 4] = 0x01800000u;    // __OSPhysicalMemSize (24 Mo)
        low[0xF0 / 4] = 0x01800000u;    // __OSSimulatedMemSize
        low[0xF8 / 4] = 162000000u;     // __OSBusClock
        low[0xFC / 4] = 486000000u;     // __OSCoreClock
    }
    g_arena = (u8 *) got;
    g_arena_lo = (u8 *) (unsigned long) ARENA_LO;
    g_arena_hi = (u8 *) (unsigned long) ARENA_HI;
}

// ---------------------------------------------------------------- tas
//
// **Les structures sont celles du SDK, a l octet pres, et ce n est pas du zele.** RE4 ne se
// contente pas d appeler OSAllocFromHeap : il **lit les descripteurs de tas lui-meme**.
// `MemCheckHeapEnd` parcourt `HeapHead[h].allocated` pour trouver le sommet reellement occupe,
// `MemSuspendHeap` recopie un descripteur entier, `MemReplaceHeap` s en sert pour redecouper
// l arene entre deux zones de jeu. Le jeu a pose `HeapHead = (OSHeapDescriptor*) arenaLo` juste
// avant d appeler OSInitAlloc, et attend d y trouver la table.
//
// Une implementation "equivalente" avec ses propres structures marche jusqu au premier
// MemReplaceHeap, puis toutes les allocations echouent en silence - c est ce qui est arrive :
// "alloc[a0]:free[0] trans_ot.cpp(53)", vingt-quatre fois, avant un dereferencement nul.
//
// Disposition (include/main_mem.h, qui la decrit telle que le jeu la lit) :
//   OSHeapCell       { prev, next, size } — en-tete de **0x20 octets**, `size` le comprend
//   OSHeapDescriptor { size, free, allocated } — 12 octets, size < 0 = tas inexistant
struct HeapCell {
    HeapCell *prev;
    HeapCell *next;
    s32 size;            // en-tete comprise
    u8 pad[0x20 - 3 * sizeof(void *) - 0];
};

struct HeapDesc {
    s32 size;
    HeapCell *free;
    HeapCell *allocated;
};

const u32 CELL_HDR = 0x20;      // taille de l en-tete, imposee par le format du SDK
const u32 ALIGN = 32;

HeapDesc *g_heap_array = nullptr;
int g_num_heaps = 0;

inline u32 round_up(u32 v) { return (v + (ALIGN - 1)) & ~(ALIGN - 1); }
inline u8 *round_up_p(void *p) { return (u8 *) (((unsigned long) p + (ALIGN - 1)) & ~(unsigned long) (ALIGN - 1)); }
inline u8 *round_down_p(void *p) { return (u8 *) ((unsigned long) p & ~(unsigned long) (ALIGN - 1)); }

// Retire une cellule d une liste doublement chainee et rend la nouvelle tete.
HeapCell *list_remove(HeapCell *list, HeapCell *c)
{
    if (c->prev)
        c->prev->next = c->next;
    else
        list = c->next;
    if (c->next)
        c->next->prev = c->prev;
    return list;
}

HeapCell *list_push(HeapCell *list, HeapCell *c)
{
    c->prev = nullptr;
    c->next = list;
    if (list)
        list->prev = c;
    return c;
}

// Insertion **triee par adresse** avec fusion des voisins. Sans la fusion, un chargement de
// salle fragmente le tas jusqu a l echec alors que la place existe.
HeapCell *free_insert(HeapCell *list, HeapCell *c)
{
    HeapCell *prev = nullptr;
    HeapCell *cur = list;
    while (cur && cur < c) {
        prev = cur;
        cur = cur->next;
    }
    c->prev = prev;
    c->next = cur;
    if (cur)
        cur->prev = c;
    if (prev)
        prev->next = c;
    else
        list = c;

    if (cur && (u8 *) c + c->size == (u8 *) cur) {
        c->size += cur->size;
        c->next = cur->next;
        if (cur->next)
            cur->next->prev = c;
    }
    if (prev && (u8 *) prev + prev->size == (u8 *) c) {
        prev->size += c->size;
        prev->next = c->next;
        if (c->next)
            c->next->prev = prev;
    }
    return list;
}

} // namespace

// `__OSCurrHeap` est declare par le SDK mais defini dans src/lib/OSAlloc.c, que le portage ne
// lie pas puisqu'il remplace tout OSAlloc. On le fournit ici.
volatile OSHeapHandle __OSCurrHeap = -1;

// ---------------------------------------------------------------- API OS

void OSInit(void)
{
    arena_init();
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
    (void) arenaEnd;
    // La table des descripteurs vit **au debut de l arene**, la ou le jeu a deja pointe
    // `HeapHead`. On rend l adresse qui suit, que le jeu prend pour nouveau bas d arene.
    g_heap_array = (HeapDesc *) arenaStart;
    g_num_heaps = maxHeaps;
    for (int i = 0; i < maxHeaps; ++i) {
        g_heap_array[i].size = -1;
        g_heap_array[i].free = nullptr;
        g_heap_array[i].allocated = nullptr;
    }
    return round_up_p((u8 *) arenaStart + (u32) maxHeaps * sizeof(HeapDesc));
}

int OSCreateHeap(void *start, void *end)
{
    if (!g_heap_array)
        return -1;
    u8 *lo = round_up_p(start);
    u8 *hi = round_down_p(end);
    if (hi <= lo + (long) CELL_HDR)
        return -1;

    for (int i = 0; i < g_num_heaps; ++i) {
        HeapDesc &d = g_heap_array[i];
        if (d.size >= 0)
            continue;
        HeapCell *c = (HeapCell *) lo;
        c->prev = nullptr;
        c->next = nullptr;
        c->size = (s32) (hi - lo);
        d.size = (s32) (hi - lo);
        d.free = c;
        d.allocated = nullptr;
        return i;
    }
    return -1;
}

void OSDestroyHeap(int heap)
{
    if (g_heap_array && heap >= 0 && heap < g_num_heaps)
        g_heap_array[heap].size = -1;
}

void OSAddToHeap(int heap, void *start, void *end)
{
    if (!g_heap_array || heap < 0 || heap >= g_num_heaps)
        return;
    u8 *lo = round_up_p(start);
    u8 *hi = round_down_p(end);
    if (hi <= lo + (long) CELL_HDR)
        return;
    HeapCell *c = (HeapCell *) lo;
    c->size = (s32) (hi - lo);
    HeapDesc &d = g_heap_array[heap];
    d.free = free_insert(d.free, c);
    d.size += c->size;
}

void *OSAllocFromHeap(int heap, u32 size)
{
    if (!g_heap_array || heap < 0 || heap >= g_num_heaps)
        return nullptr;
    HeapDesc &d = g_heap_array[heap];
    if (d.size < 0)
        return nullptr;

    const u32 want = round_up(size + CELL_HDR);
    for (HeapCell *c = d.free; c; c = c->next) {
        if ((u32) c->size < want)
            continue;
        // Scinder seulement si le reste peut porter une cellule utile ; sinon on donne tout,
        // ce que fait aussi le SDK.
        const u32 rest = (u32) c->size - want;
        if (rest >= CELL_HDR + ALIGN) {
            HeapCell *tail = (HeapCell *) ((u8 *) c + want);
            tail->size = (s32) rest;
            tail->prev = c->prev;
            tail->next = c->next;
            if (c->prev)
                c->prev->next = tail;
            else
                d.free = tail;
            if (c->next)
                c->next->prev = tail;
            c->size = (s32) want;
        } else {
            d.free = list_remove(d.free, c);
        }
        d.allocated = list_push(d.allocated, c);
        return (u8 *) c + CELL_HDR;
    }
    return nullptr;
}

void OSFreeToHeap(int heap, void *ptr)
{
    if (!ptr || !g_heap_array || heap < 0 || heap >= g_num_heaps)
        return;
    HeapDesc &d = g_heap_array[heap];
    HeapCell *c = (HeapCell *) ((u8 *) ptr - CELL_HDR);
    d.allocated = list_remove(d.allocated, c);
    d.free = free_insert(d.free, c);
}

int OSSetCurrentHeap(int heap) { int old = __OSCurrHeap; __OSCurrHeap = heap; return old; }

// Le jeu imprime ce retour quand une allocation echoue : il doit dire la place libre reelle,
// sinon le message ne renseigne sur rien ("free[0]" quel que soit l etat du tas).
s32 OSCheckHeap(int heap)
{
    if (!g_heap_array || heap < 0 || heap >= g_num_heaps || g_heap_array[heap].size < 0)
        return -1;
    s32 total = 0;
    for (const HeapCell *c = g_heap_array[heap].free; c; c = c->next)
        total += c->size;
    return total;
}

u32 OSReferentSize(void *ptr)
{
    if (!ptr)
        return 0;
    const HeapCell *c = (const HeapCell *) ((u8 *) ptr - CELL_HDR);
    return (u32) c->size - CELL_HDR;
}

void OSVisitAllocated(void (*visitor)(void *, u32))
{
    if (!g_heap_array || !visitor)
        return;
    for (int i = 0; i < g_num_heaps; ++i)
        for (HeapCell *c = g_heap_array[i].allocated; c; c = c->next)
            visitor((u8 *) c + CELL_HDR, (u32) c->size - CELL_HDR);
}

void OSDumpHeap(int heap)
{
    std::printf("[PORT] tas %d : %d octets libres\n", heap, (int) OSCheckHeap(heap));
}

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
    // 40,5 MHz = 405 tops par 10 000 ns. Avant le 15/11 : (TB_HZ / 1000000) en entiers = 40, l'horloge du jeu
    // retardait de 1,25 % (battement de 60 images lu 1977 ms au lieu de 2002).
    return (OSTime)((host_ns() - t0) * 405ull / 10000ull);
}

OSTick OSGetTick(void) { return (OSTick)OSGetTime(); }

// ---------------------------------------------------------------- sections critiques

// Il n'y a pas d'interruptions a masquer : le portage est mono-flux tant que l'ordonnanceur
// n'existe pas. On rend l'etat precedent pour que OSRestoreInterrupts le retablisse.
namespace { int g_intr = 1; }

// Le GPU signale ses fins de dessin et ses jetons ici (port/gx_sdk.cpp) : le SDK teste son drapeau
// juste apres avoir masque (GXWaitDrawDone), et l'ecriture qui les demande les remet a zero apres coup.
extern "C" void port_gx_deliver(void);
extern "C" void port_audio_deliver(void);   // port/audio_port.cpp : interruptions AI et fin d'image du DSP
BOOL OSDisableInterrupts(void) { int o = g_intr; port_gx_deliver(); port_audio_deliver(); g_intr = 0; return o; }
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
