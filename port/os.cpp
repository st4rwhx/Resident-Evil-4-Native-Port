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

#include <dolphin.h>

namespace {

// ---------------------------------------------------------------- arene

// MEM1 du GameCube : 24 Mo. Le jeu y taille ses tas lui-meme via OSInitAlloc/OSCreateHeap,
// donc on lui rend simplement un bloc contigu de cette taille.
const u32 ARENA_SIZE = 24u * 1024u * 1024u;
u8 *g_arena = nullptr;
u8 *g_arena_lo = nullptr;
u8 *g_arena_hi = nullptr;

void arena_init()
{
    if (g_arena)
        return;
    g_arena = (u8 *)std::calloc(1, ARENA_SIZE);
    g_arena_lo = g_arena;
    g_arena_hi = g_arena + ARENA_SIZE;
}

// ---------------------------------------------------------------- tas

struct Block {
    u32 size;       // taille utile, hors en-tete
    Block *next;    // chainage de la liste libre, ou nullptr si alloue
    int heap;
    u32 free;
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
    h.start = (u8 *)start;
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
    timespec ts;
    timespec_get(&ts, TIME_UTC);
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
u32 OSGetResetButtonState(void) { return 0; }
void *OSPhysicalToCached(u32 addr) { arena_init(); return g_arena + (addr & 0x01FFFFFFu); }

void OSPanic(const char *file, int line, const char *msg, ...)
{
    std::printf("[PORT] OSPanic %s:%d : %s\n", file ? file : "?", line, msg ? msg : "");
    std::fflush(stdout);
    std::abort();
}
