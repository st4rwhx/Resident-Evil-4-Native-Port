#include "types.h"
#include "global.h"
#include "main_mem.h"
#include "db_log.h"
#include "joy.h"
#include "eprintf.h"
#include "file.h"
#include "libgpu.h"

extern "C" {
void OSReport(const char* fmt, ...);
void* OSGetArenaLo();
void* OSGetArenaHi();
void OSSetArenaLo(void* lo);
void OSSetArenaHi(void* hi);
void* OSInitAlloc(void* lo, void* hi, int maxHeaps);
int OSCreateHeap(void* start, void* end);
void OSDestroyHeap(int heap);
void OSSetCurrentHeap(int heap);
void* OSAllocFromHeap(int heap, u32 size);
void OSFreeToHeap(int heap, void* p);
s32 OSCheckHeap(int heap);
void OSSetSaveRegion(void* start, void* end);
extern int __OSCurrHeap;
#ifndef RE4_PORT   // portage : port/port_prelude.h le declare deja, avec la bonne taille
int strcmp(const char* a, const char* b);
#endif
#ifndef RE4_PORT   // portage : port/port_prelude.h le declare deja, avec la bonne taille
char* strcpy(char* dst, const char* src);
#endif
char* strrchr(const char* s, int c);
int sprintf(char* buf, const char* fmt, ...);
#ifndef RE4_PORT   // portage : port/port_prelude.h le declare deja, avec la bonne taille
void* memset(void* dst, int c, unsigned int n);
#endif
}

extern char* pRK;

// Fixed memory map of the debug build.
struct SystemMemMap {
    u32 x0;         // 0x00
    u32 elf_end;    // 0x04
    u32 dvd;        // 0x08
    u32 sound;      // 0x0C
    u32 fifo;       // 0x10
    u32 xfb;        // 0x14
    u32 core;       // 0x18
    u32 option;     // 0x1C
    u32 player;     // 0x20
    u32 weapon;     // 0x24  start of the main heap
    u32 heap_end;   // 0x28
    u32 arena_lo;   // 0x2C  OSGetArenaLo() at boot
    u32 usb;        // 0x30
    u32 debug;      // 0x34
};

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }


MemHeap Heap[MEM_HEAP_NUM];
static SystemMemMap SysMem;
OSHeapDescriptor heap_backup[MEM_HEAP_NUM];  // OSAlloc descriptors of suspended heaps

OSHeapCell* cell_main = NULL;
OSHeapCell* cell_game = NULL;
static OSHeapCell* cell_stage = NULL;
OSHeapCell* cell_dll = NULL;
static u8 Dalloc_flg = 0;
u32 _epy_base = 40;

// La carte memoire ci-dessous est ecrite en adresses absolues du GameCube. Sur le telephone
// elles sont justes : port/os.cpp reserve MEM1 a 0x80000000. Sur PC c'est aurora qui alloue
// MEM1, et pas a cette adresse - sa documentation le dit explicitement. PORT_MEM1 ramene
// chaque constante sur la base reelle ; hors portage, la macro est l'identite et le binaire
// ne change pas.
#ifdef RE4_PORT
#include <stdlib.h>
#include <stdint.h>
#endif

#ifndef PORT_MEM1
#define PORT_MEM1(a) (a)
#endif

static OSHeapDescriptor* HeapHead;
u32 arenaLo;
u32 arenaHi;
u8 CurrentHeap;
static u8 CurrentDbgHeap;
void* pMemTile;
static u32 _epy;

#ifdef RE4_PORT
// RE4 remplace l'operateur new global. Sur GameCube c'etait sans risque : rien d'autre que le
// jeu ne tournait dans le processus. Ici, les initialiseurs statiques de la bibliotheque hote
// s'executent **avant** SystemMemInit et passent par ce meme operateur, alors que le tas du jeu
// n'existe pas encore - ni le journal, d'ou une violation d'acces avant meme main().
//
// Ces allocations-la vont donc a la libc. Ce qui oblige a rendre `delete` capable de choisir,
// et le critere ne peut pas etre un drapeau temporel : un objet cree avant l'initialisation est
// souvent detruit apres. Le critere sur (dans MEM1 ou pas) est, lui, toujours vrai.
//
// Il faut aussi definir les formes **dimensionnees** de delete (C++14). La bibliotheque hote est
// compilee en C++20 et les emet ; sans elles, le compilateur appelle celle par defaut, qui rend
// au tas du systeme un pointeur venu du tas du jeu - ou l inverse. C est exactement le plantage
// qu'on observait : corruption de tas dans un destructeur statique d'aurora.
static bool MemReady = false;

// Le tas du jeu appartient au jeu. La bibliotheque hote a ses propres fils - celui de rendu,
// par exemple - et les y laisser allouer serait faux deux fois : ils consommeraient la memoire
// dimensionnee pour RE4, et ils le feraient sans verrou, alors que ce tas n'en a aucun (sur
// GameCube il suffisait de couper les interruptions). Tout ce qui ne vient pas du fil du jeu
// va donc a la libc.
extern "C" int port_on_game_thread(void);

// Un mot sur la sortie standard, sans passer par <stdio.h> : les en-tetes du jeu
// declarent deja sprintf et consorts, et les deux jeux de declarations se contredisent.
extern "C" void port_note(const char* fmt, ...);

// Le tas du jeu n'est pas "MEM1" : c'est la plage d'un tas vivant de `Heap[]`. La difference
// compte, parce que MEM1 contient aussi les zones fixes du jeu et les tampons du DVD, et rendre
// une de leurs adresses a OSFreeToHeap insere dans la liste chainee une cellule qui n'en est pas
// une. La sonde RE4_TRACE_HEAP a montre exactement cela : un free de 0x80994a40 alors que le
// seul tas vivant commence a 0x80a03ea0.
//
// `heap_owner_of` rend l'indice du tas qui contient reellement le pointeur - le plus etroit, au
// cas ou un tas serait taille dans un autre - ou -1.
static int heap_owner_of(const void* q)
{
    const u32 a = (u32) (uintptr_t) q;
    int best = -1;
    u32 best_span = 0xFFFFFFFFu;
    for (int k = 0; k < MEM_HEAP_NUM; ++k) {
        if (Heap[k].handle < 0 || Heap[k].end <= Heap[k].start)
            continue;
        if (a < Heap[k].start || a >= Heap[k].end)
            continue;
        const u32 span = Heap[k].end - Heap[k].start;
        if (span < best_span) {
            best_span = span;
            best = k;
        }
    }
    return best;
}

static bool in_game_heap(void* q) { return heap_owner_of(q) >= 0; }

// Trois provenances possibles, trois sorties. Confondre les deux dernieres passe une adresse de
// MEM1 a la libc, ce qui la fait tomber aussitot.
static void port_free(void* q)
{
    if (q == NULL)
        return;
    if (in_game_heap(q)) {
        Mem_free(q);
        return;
    }
    const u32 a = (u32) (uintptr_t) q;
    if (a >= (u32) PORT_MEM1(0x80000000) && a < (u32) PORT_MEM1(0x83000000))
        return;                      // zone fixe de MEM1 : rien a rendre a personne
    free(q);
}

void* operator new(size_t size)
{
    if (!MemReady || !port_on_game_thread())
        return calloc(size ? size : 1, 1);
    return mem_calloc(size, "operator new", 0, 1, MEM_HEAP_CURRENT);
}

void* operator new[](size_t size)
{
    if (!MemReady || !port_on_game_thread())
        return calloc(size ? size : 1, 1);
    return mem_calloc(size, "operator new", 0, 1, MEM_HEAP_CURRENT);
}

void operator delete(void* q, unsigned long long) { port_free(q); }
void operator delete[](void* q, unsigned long long) { port_free(q); }
#else
void* operator new(size_t size)
{
    return mem_calloc(size, "operator new", 0, 1, MEM_HEAP_CURRENT);
}

void* operator new[](size_t size)
{
    return mem_calloc(size, "operator new", 0, 1, MEM_HEAP_CURRENT);
}
#endif

void operator delete(void* p)
{
#ifdef RE4_PORT
    port_free(p);
#else
    Mem_free(p);
#endif
}

void operator delete[](void* p)
{
#ifdef RE4_PORT
    port_free(p);
#else
    Mem_free(p);
#endif
}

void SystemMemInit()
{
    SysMem.heap_end = (u32) PORT_MEM1(0x817F4000);
    SysMem.elf_end = (u32) PORT_MEM1(0x80350000);
    SysMem.dvd = (u32) PORT_MEM1(0x80370000);
    SysMem.sound = (u32) PORT_MEM1(0x803F0000);
    SysMem.fifo = (u32) PORT_MEM1(0x80460000);
    SysMem.xfb = (u32) PORT_MEM1(0x80578000);
    SysMem.core = (u32) PORT_MEM1(0x807AC000);
    SysMem.option = (u32) PORT_MEM1(0x807EC000);
    SysMem.player = (u32) PORT_MEM1(0x80904000);
    SysMem.weapon = (u32) PORT_MEM1(0x80974000);
    SysMem.usb = (u32) PORT_MEM1(0x81800000);
    SysMem.debug = (u32) PORT_MEM1(0x8181FB00);
    SysMem.arena_lo = (u32)(uintptr_t)(OSGetArenaLo());
    if (SysMem.arena_lo > (u32) PORT_MEM1(0x8034FFFF)) {
        OSReport("ELF size overflow\n");
#line 100 "D:/Bio4/Prog/main_mem.cpp"
        HALT();
    }
    arenaLo = SysMem.weapon;
    arenaHi = (u32)(uintptr_t)(OSGetArenaHi());
    if (SysMem.heap_end > arenaHi) {
        arenaHi = SysMem.heap_end;
    }
    arenaLo = (arenaLo + 0x1F) & ~0x1F;
    arenaHi &= ~0x1F;
    HeapHead = (OSHeapDescriptor*)port_ptr64(arenaLo);
    arenaLo = (u32)(uintptr_t)(OSInitAlloc((void*)port_ptr64(arenaLo), (void*)port_ptr64(arenaHi), MEM_HEAP_NUM));
    OSSetArenaLo((void*)port_ptr64(arenaLo));
    OSSetArenaHi((void*)port_ptr64(arenaHi));
    memInitHeapTbl();
    MemCreateHeap(0, arenaLo, SysMem.heap_end);
    MemSetCurrentHeap(0);
    pMemTile = NULL;
#line 145
    pRK = (char*) MEM_ALLOC(0x40, 1, MEM_HEAP_CURRENT);
    OSSetSaveRegion(pRK, pRK + 0x40);
    if (strcmp(pRK, "_reset_keep_") != 0) {
        memclr_asm(pRK, 0x40);
        strcpy(pRK, "_reset_keep_");
        OSReport("RESET_KEEP_WORK memory clear...\n");
    }
#ifdef RE4_PORT
    // A partir d ici le tas du jeu existe : l operateur new reprend son chemin normal.
    MemReady = true;
#endif
}

void memInitHeapTbl()
{
    int i;

    for (i = 0; i < MEM_HEAP_NUM; i++) {
        Heap[i].handle = -1;
        Heap[i].start = 0;
        Heap[i].end = 0;
        Heap[i].status = 0;
    }
    memclr_asm(heap_backup, sizeof(heap_backup));
}

void MemSuspendHeap(int no)
{
    int h = Heap[no].handle;

    if (memGetHeapSattus(no) == 0 && h >= 0) {
        heap_backup[h] = HeapHead[h];
        HeapHead[h].free = NULL;
        HeapHead[h].allocated = NULL;
        Heap[no].status = 1;
    }
}

void MemSignalHeap(int no)
{
    int h = Heap[no].handle;

    if (memGetHeapSattus(no) == 1 && h >= 0) {
        OSHeapDescriptor* hh = HeapHead;
        OSHeapDescriptor* bk = heap_backup;

        *(OSHeapDescriptor*) (h * sizeof(OSHeapDescriptor) + (u32)(uintptr_t)(hh)) =
            *(OSHeapDescriptor*) (h * sizeof(OSHeapDescriptor) + (u32)(uintptr_t)(bk));
        Heap[no].status = 0;
    }
}

int memGetHeapSattus(int no)
{
    return Heap[no].status;
}

int memCheckHeapActive(int no)
{
    return memGetHeapSattus(no) == 0;
}

int MemSetCurrentHeap(int no)
{
    if (memCheckHeapActive(no) && Heap[no].handle >= 0) {
        CurrentHeap = no;
        OSSetCurrentHeap(Heap[CurrentHeap].handle);
        CurrentDbgHeap = CurrentHeap;
        return 1;
    }
    return 0;
}

int MemSetCurrentDbgHeap(int no)
{
    if (memCheckHeapActive(no) && Heap[no].handle >= 0) {
        CurrentDbgHeap = CurrentHeap;
        return 1;
    }
    return 0;
}

u8 MemGetCurrentHeap()
{
    return CurrentHeap;
}

u8 MemGetCurrentDbgHeap()
{
    return CurrentDbgHeap;
}

u32 MemGetHeapStartAddr(int no)
{
    return Heap[no].start;
}

u32 MemGetHeapEndAddr(int no)
{
    return Heap[no].end;
}

// OPEN (-4): the original places `li r3,0` between the compare and the branch and reloads
// d->allocated for the loop init after the if/else join (ours forwards it); if/else, ternary,
// `end = 0` first and HeapHead[h] index forms tried.
u32 MemCheckHeapEnd(int no)
{
    int h = Heap[no].handle;
    OSHeapDescriptor* d;
    OSHeapCell* cell;
    u32 end;

    if (!memCheckHeapActive(no) || h < 0) {
        return 0;
    }
    d = HeapHead;
    d += h;
    // `d = HeapHead; d += h;` loads HeapHead straight into d (a global pseudo), so the block's
    // local qtys are only h*12 and the loaded `allocated` (the 3-qty partial sort would put h*12
    // first). The two-statement then-arm keeps jump1 from hoisting `end = 0` above the branch, so
    // cse1's path ends at the else arm and the loop init reloads `d->allocated`; jump2 hoists the
    // `li r3,0` between the compare and the branch afterwards.
    if (d->allocated == NULL) {
        cell = d->free;
        end = (u32)(uintptr_t)(cell);
    } else {
        end = 0;
    }
    for (cell = d->allocated; cell != NULL; cell = cell->next) {
        if (end < (u32)(uintptr_t)(cell) + cell->size) {
            end = (u32)(uintptr_t)(cell) + cell->size;
        }
    }
    return end;
}

int MemCreateHeap(int no, u32 start, u32 end)
{
    if (!memCheckHeapActive(no)) {
        return 0;
    }
    if (Heap[no].handle >= 0) {
        MemDestroyHeap(no);
    }
    OSReport("-- MemCreateHeap %d %08x - %08x  ", no, start, end);
    Heap[no].handle = OSCreateHeap((void*)port_ptr64(start), (void*)port_ptr64(end));
    if (Heap[no].handle >= 0) {
        Heap[no].start = start;
        Heap[no].end = end;
        OSReport("succeed!!\n");
        return 1;
    }
    OSReport("failed!!\n");
    return 0;
}

int MemDestroyHeap(int no)
{
    if (!memCheckHeapActive(no)) {
        MemSignalHeap(no);
    }
    OSReport("-- MemDestroyHeap %d  ", no);
    if (Heap[no].handle >= 0) {
        OSDestroyHeap(Heap[no].handle);
        Heap[no].handle = -1;
        OSReport("succeed!!\n");
        return 1;
    }
    OSReport("failed!!\n");
    return 0;
}

int MemReplaceHeap(int from, int to)
{
    u32 start;
    u32 end;
    OSHeapDescriptor* hd;  // function scope: a global pseudo, allocated after the blocks' local qtys (lis/cell r9)

    if (CurrentHeap == 0) {
        hd = HeapHead + Heap[CurrentHeap].handle;
        cell_main = hd->allocated;
    }
    if (CurrentHeap == 1) {
        hd = HeapHead + Heap[CurrentHeap].handle;
        cell_game = hd->allocated;
    }
    if (CurrentHeap == 2) {
        hd = HeapHead + Heap[CurrentHeap].handle;
        cell_stage = hd->allocated;
    }
    if (CurrentHeap == 3) {
        hd = HeapHead + Heap[CurrentHeap].handle;
        cell_dll = hd->allocated;
    }
    if (!memCheckHeapActive(from)) {
        return 0;
    }
    if (!memCheckHeapActive(to)) {
        return 0;
    }
    if (Heap[from].handle >= 0) {
#ifdef RE4_PORT
        // Diagnostic N3 (r100.das ecrasait le tas 4) : les blocs encore vivants les plus hauts du tas
        // repris, avec l'etiquette « MAD fichier(ligne) » que mem_alloc pose en fin de bloc.
        {
            OSHeapDescriptor* dd = HeapHead + Heap[from].handle;
            for (int pass = 0; pass < 12; ++pass) {
                OSHeapCell* best = NULL;
                static OSHeapCell* shown[12];
                for (OSHeapCell* c = dd->allocated; c; c = c->next) {
                    bool seen = false;
                    for (int k = 0; k < pass; ++k) seen |= shown[k] == c;
                    if (!seen && (!best || c > best)) best = c;
                }
                if (!best) break;
                shown[pass] = best;
                const char* tag = "?";
                u8* e = (u8*) best + best->size;
                for (u8* t = e - 0x20; t < e - 4; ++t)
                    if (t[0] == 0 && t[1] == 'M' && t[2] == 'A' && t[3] == 'D') { tag = (const char*) t + 4; break; }
                OSReport("[TAS] tas %d vivant %08x taille %x %s\n", from, (u32) (unsigned long) best, best->size, tag);
            }
        }
#endif
        start = MemCheckHeapEnd(from);
        end = Heap[from].end;
        MemDestroyHeap(from);
    } else {
        start = Heap[to].start;
        end = Heap[to].end;
    }
    if (start == 0) {
        return 0;
    }
    return MemCreateHeap(to, start, end);
}

void MemClearAllHeap()
{
    u32 i;

    for (i = 0; i < MEM_HEAP_NUM; i++) {
        if (Heap[i].handle >= 0) {
            MemDestroyHeap(i);
        }
    }
}

#ifdef RE4_PORT
// Sonde de tas, sous RE4_TRACE_HEAP=1 : OSCheckHeap apres chaque allocation et chaque
// liberation. Le tas se corrompt de facon non deterministe (les plantages tombent tantot dans
// le moniteur de tas, tantot sur une allocation refusee) ; seule la premiere operation fautive
// designe le coupable.
extern "C" int port_heap_trace(void);

// Le bloc est-il vivant ? OSFreeToHeap suppose que la cellule qu'on lui rend est dans la liste
// des blocs alloues ; lui en rendre une autre - un bloc deja libere, ou une adresse qui n'a
// jamais ete allouee - detruit la liste sans un mot. La console ne s'en protegeait pas, mais
// elle n'avait pas non plus nos deux allocateurs.
static bool port_cell_allocated(int heap, const void* p)
{
    if (heap < 0 || heap >= MEM_HEAP_NUM || Heap[heap].handle < 0 || HeapHead == NULL)
        return false;
    const OSHeapDescriptor* hd = HeapHead + Heap[heap].handle;
    const void* want = (const u8*) p - 0x20;
    int n = 0;
    for (const OSHeapCell* c = hd->allocated; c != NULL; c = c->next) {
        if (++n > 0x40000)
            return false;
        if ((const void*) c == want)
            return true;
    }
    return false;
}

static void port_heap_probe(const char* what, const char* file, int line, u32 size, int heap)
{
    static bool broken = false;
    if (!port_heap_trace() || broken)
        return;
    if (heap == MEM_HEAP_CURRENT)
        heap = CurrentHeap;
    if (heap < 0 || heap >= MEM_HEAP_NUM || Heap[heap].handle < 0)
        return;
    if (OSCheckHeap(Heap[heap].handle) < 0) {
        broken = true;
        port_note("[HEAP] casse apres %s %x octets, tas %d, %s(%d)\n",
                  what, size, heap, file ? file : "?", line);
    }
}
#endif

void* mem_alloc(u32 size, const char* file, int line, int flag, int heap)
{
    char str[64] = "";
    u8* p;
    char* name;
    u8* tag;

    if (size == 0) {
        return NULL;
    }
    if (heap == MEM_HEAP_CURRENT) {
        heap = CurrentHeap;
    }
    if (!memCheckHeapActive(heap)) {
        return NULL;
    }
    size = (size + 0x1F) & ~0x1F;
    if (file == NULL) {
        p = (u8*) OSAllocFromHeap(Heap[heap].handle, size);
    } else {
        p = (u8*) OSAllocFromHeap(Heap[heap].handle, size + 0x20);
        name = strrchr(file, '/');
        if (name == NULL) {
            name = (char*) file;
        } else {
            name++;
        }
        sprintf(str, "%s(%d)", name, line);
        str[0x1B] = 0;
        if (p != NULL) {
            tag = p + size;
            tag[0] = 0;
            tag[1] = 'M';
            tag[2] = 'A';
            tag[3] = 'D';
            strcpy((char*) tag + 4, str);
        }
    }
    if (flag == 1 && p == NULL) {
#ifdef RE4_PORT
        // pLog n existe pas encore pendant l initialisation, et une allocation ratee est
        // precisement le moment ou l on veut lire le message plutot qu un plantage de plus.
        if (pLog.p == NULL) {
            port_note("[PORT] allocation refusee : %x octets, tas %d (%s)\n",
                      size, heap, str);
        } else
#endif
        pLog->err(0, 0, "alloc[%x]:free[%x] %s", size, OSCheckHeap(Heap[heap].handle), str);
    }
#ifdef RE4_PORT
    if (port_heap_trace() > 1)
        port_note("[HEAP] alloc p=%08x taille=%x %s(%d)\n",
                  (u32) (unsigned long long) p, size, file ? file : "?", line);
    port_heap_probe("alloc", file, line, size, heap);
#endif
    return p;
}

void* mem_calloc(u32 size, const char* file, int line, int flag, int heap)
{
    void* p = mem_alloc(size, file, line, flag, heap);

    if (p != NULL) {
        memclr_asm(p, size);
    }
    return p;
}

void Mem_free(void* p)
{
    Mem_free_h(p, CurrentHeap);
}

void Mem_free_h(void* p, int heap)
{
    if (heap == MEM_HEAP_CURRENT) {
        heap = CurrentHeap;
    }
    if (memCheckHeapActive(heap)) {
#ifdef RE4_PORT
        // Deux ecarts avec la console, tous deux mesures par la sonde RE4_TRACE_HEAP.
        //
        // 1. Un pointeur venu d'ailleurs. Quand `operator new` est appele avant que le tas du
        //    jeu n'existe - ou depuis un thread d'aurora - il rend un bloc de la libc. Le jeu le
        //    libere ensuite par Mem_free comme n'importe quel autre, et OSFreeToHeap insere dans
        //    sa liste chainee une cellule qui n'en est pas une. Le tas est mort a cet instant,
        //    mais il ne tombe que bien plus tard, ailleurs : d'ou les plantages non
        //    deterministes du moniteur de tas.
        //
        // 2. `__OSCurrHeap` vaut -1. OSDestroyHeap le remet a -1 quand on detruit le tas
        //    courant, et MemDestroyHeap(0) le fait au demarrage ; plus rien ne le repositionne.
        //    Sur console MemSetCurrentHeap le tient egal a Heap[CurrentHeap].handle, donc rendre
        //    le bloc a Heap[heap].handle y est exactement equivalent - et ici c'est le seul
        //    handle qui soit juste.
        const int owner = heap_owner_of(p);
        if (owner < 0) {
            // Pas un bloc de tas : soit la libc (operator new avant l'existence du tas, ou
            // depuis un thread d'aurora), soit une zone fixe de MEM1. Dans le second cas on ne
            // peut que laisser le bloc tranquille - le perdre coute moins cher que corrompre le
            // tas, et la trace le dit.
            const u32 a = (u32) (unsigned long long) p;
            if (a >= (u32) PORT_MEM1(0x80000000) && a < (u32) PORT_MEM1(0x83000000)) {
                if (port_heap_trace())
                    port_note("[HEAP] free ignore : %08x hors de tout tas\n", a);
                return;
            }
            free(p);
            return;
        }
        heap = owner;
        if (!port_cell_allocated(heap, p)) {
            // Bloc mort ou inconnu : ne pas le rendre. Ce n'est pas de la prudence gratuite -
            // la sonde a compte jusqu'a seize liberations de ce genre par demarrage, dont
            // quatre fois la meme adresse.
            if (port_heap_trace())
                port_note("[HEAP] free refuse : %08x n'est pas un bloc vivant du tas %d\n",
                          (u32) (unsigned long long) p, heap);
            return;
        }
        if (port_heap_trace()) {
            const int before = OSCheckHeap(Heap[heap].handle);
            OSFreeToHeap(Heap[heap].handle, p);
            const int after = OSCheckHeap(Heap[heap].handle);
            if (before >= 0 && after < 0) {
                const u8* tag = (const u8*) p - 0x20;
                port_note("[HEAP] casse par free p=%08x tas=%d\n",
                          (u32) (unsigned long long) p, heap);
                for (int k = 0; k < MEM_HEAP_NUM; ++k)
                    if (Heap[k].handle >= 0)
                        port_note("[HEAP]   tas %d : handle %d  %08x - %08x\n",
                                  k, Heap[k].handle, Heap[k].start, Heap[k].end);
                (void) tag;
            }
        } else {
            OSFreeToHeap(Heap[heap].handle, p);
        }
        port_heap_probe("free", "Mem_free_h", 0, 0, heap);
#else
        OSFreeToHeap(__OSCurrHeap, p);
#endif
    }
}

void SetDebugAlloc()
{
    Dalloc_flg = 1;
}

// The cell header fields the debug code reads (the real OSHeapCell is 0x20 bytes).
struct MemCellHead {
    OSHeapCell* prev;  // 0x00
    OSHeapCell* next;  // 0x04
    s32 size;          // 0x08
};

void ResetDebugAlloc()
{
    OSHeapCell* cell;
    MemCellHead tmp;

    Dalloc_flg = 0;
    {
        // pointer + index (the pointer is the first `add` operand, tied to HeapHead's register)
        OSHeapDescriptor* hd = HeapHead + Heap[CurrentDbgHeap].handle;
        cell = hd->allocated;
    }
    for (; cell != NULL; cell = tmp.next) {
        tmp = *(MemCellHead*) cell;
        if (strcmp((char*) cell + cell->size - 8, "toolmem") == 0) {
            Debug_free((u8*) cell + 0x20);
        }
    }
}

void* Debug_alloc(u32 size, int flag)
{
    u8* p;

    if (size == 0) {
        return NULL;
    }
    if (!memCheckHeapActive(CurrentDbgHeap)) {
        return NULL;
    }
    if (Dalloc_flg == 1 && flag == 1) {
        size += 8;
    }
    size = (size + 0x1F) & ~0x1F;
    p = (u8*) OSAllocFromHeap(Heap[CurrentDbgHeap].handle, size);
    if (p != NULL) {
        memclr_asm(p, size);
        if (Dalloc_flg == 1 && flag == 1) {
            strcpy((char*) p + size - 8, "toolmem");
        }
    }
    return p;
}

void Debug_free(void* p)
{
    Debug_free_h(p, CurrentDbgHeap);
}

void Debug_free_h(void* p, int heap)
{
    if (heap == MEM_HEAP_CURRENT) {
        heap = CurrentDbgHeap;
    }
    if (memCheckHeapActive(heap)) {
        memclr_asm(p, ((OSHeapCell*) ((u8*) p - 0x20))->size - 0x20);
        OSFreeToHeap(Heap[CurrentDbgHeap].handle, p);
    }
}

void* MemAlloc(u32 size, int flag)
{
    void* p;

    if (!(pG->flags_6C & 0x200000)) {
#line 646
        p = MEM_ALLOC(size, 1, MEM_HEAP_CURRENT);
    } else {
        p = Debug_alloc(size, flag);
    }
    return p;
}

void MemFree(void* p)
{
    if (pG->flags_6C & 0x200000) {
        Debug_free(p);
    } else {
        Mem_free(p);
    }
}

// Debug heap display (debug page 4): one TILE per allocated cell (0x20-byte primitives).
// Matching. Shape (same as datactrl dispDebug): y0/y1 and `x = 498` are function-scope locals
// (y1 is reused for the end marker's height, x is a REG_EQUIV constant that reload rematerialises
// before each `sth x0`, which is what lets the code constant's register be reused), the tile
// stores are in datactrl's order (code, x0, y0, ...; r, g, b), `mt = &tile[0]` precedes the
// end-marker conversion, and the cell loop starts from `hd = HeapHead + handle`.
struct MemTile {
    u32 tag;       // 0x00
    u32 code;      // 0x04
    u8 r, g, b, cd;  // 0x08
    s16 x0, y0;    // 0x0C
    s16 w, h;      // 0x10
    s16 z0;        // 0x14
    u8 pad_16[0x20 - 0x16];
};

struct SysFlagsView {
    u32 flags;  // 0x00  SystemWork::flags
};
extern SysFlagsView* pSysView asm("pSys");
// Reference read: the load stays below the preceding tile stores (see mercenaries.cpp SysRef).
static inline SysFlagsView* SysRef(SysFlagsView*& p) { return p; }
extern u32 MainOt[5];

struct DvdFreeSizeView {
    u32 freeSize;  // 0x00  cDvd::freeSize
    u8 pad_4[0x10];  // (keeps the extern out of small data)
};
extern DvdFreeSizeView DvdView asm("Dvd");

static inline void ISet(int& d, int v) { d = v; }

#define MEM_TAG_OK(tag) ((tag)[0] == 0 && (tag)[1] == 'M' && (tag)[2] == 'A' && (tag)[3] == 'D')

void MemCheckUsedHeap()
{
    char* p = NULL;
    char* buf = p;
    int full = 0;
    int write = 0;
    int rest;
    u32 end;
    u32 start;
    u32 heapEnd;
    u32 size;
    int ey;
    int cnt;
    OSHeapCell* cell;
    OSHeapCell* next;
    OSHeapDescriptor* hd;
    MemTile* mt;
    int r;
    int n;
    u32 y0;
    u32 y1;
    int x;
    static int ey_base = 30;
    static MemTile tile[2];

    x = 498;
    if (pG->debug_mode == 4 && (Joy[0].on & 0x10) && (Joy[0].trg & 0x400)) {
        write = 1;
    }
    if (write == 1) {
        buf = (char*) Debug_alloc(0x2000, 1);
        p = buf;
    }
    if (!memCheckHeapActive(CurrentHeap)) {
        return;
    }
    rest = OSCheckHeap(Heap[CurrentHeap].handle) - 0x10000;
    if (rest >= 0) {
        end = MemCheckHeapEnd(CurrentHeap);
    } else {
        end = SysMem.heap_end;
    }
    start = Heap[CurrentHeap].start;
    heapEnd = Heap[CurrentHeap].end;
    eprintf2(8, 16, 440, 404, 0, 0, "%X", rest);
    r = OSCheckHeap(Heap[CurrentDbgHeap].handle);
    if (r >= 0) {
        eprintf2(8, 16, 440, 420, 0, 0, "%X", r);
    } else {
        eprintf2(8, 16, 440, 420, 2, 0, "%X", r);
    }
    if (Joy[0].rep2 & 0x400000) {
        ey_base -= 16;
    }
    if (Joy[0].rep2 & 0x800000) {
        ey_base += 16;
    }
    if (ey_base >= -2000) {
        n = ey_base;
        if (n > 2000) {
            n = 2000;
        }
    } else {
        n = -2000;
    }
    ISet(ey_base, n);
    ey = ey_base;
    size = heapEnd - start;
    mt = (MemTile*) pMemTile;
    cnt = 0;
    hd = HeapHead + Heap[CurrentHeap].handle;
    for (cell = hd->allocated; cell != NULL; cell = cell->next) {
        y0 = (u32) ((f32) ((u32)(uintptr_t)(cell) - start) * 400.0f / (f32) size);
        y1 = (u32) ((f32) ((u32)(uintptr_t)(cell) + cell->size - start) * 400.0f / (f32) size) + 1;

        u8* tag = (u8*) cell + cell->size - 0x20;
        if ((s32)(uintptr_t)(tag) >= 0 || (u32)(uintptr_t)(tag) > PORT_PTR_MAX) {
            break;
        }
        if (MEM_TAG_OK(tag)) {
            eprintf2(8, 14, 230, ey, 0, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
        } else {
            eprintf2(8, 14, 230, ey, 0, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
        }
        if (write == 1 && cell->size - 0x20 > 0x1000) {
            p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
        }
        ey += 14;
        next = cell->next;
        if (next != NULL && next->next != NULL &&
            ((s32) next->next >= 0 || (u32) next->next > PORT_PTR_MAX)) {
            pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 770
            HALT();
        }
        if (pMemTile != NULL && full == 0) {
            mt->code = 4;
            mt->x0 = x;
            mt->y0 = y0 + 30;
            mt->w = 5;
            mt->h = y1 - y0;
            mt->z0 = full;
            if (rest >= 0) {
                mt->r = 0x60;
                mt->g = 0x60;
                mt->b = 0x80;
            } else {
                mt->r = 0xFF;
                mt->g = 0x20;
                mt->b = 0x20;
            }
            mt->cd = 0xFF;
            if (SysRef(pSysView)->flags & 0x40000000) {
                mt->y0 = (s16) ((f32) mt->y0 / 1.3333334f + 56.0f);
                mt->h = (s16) ((f32) mt->h / 1.3333334f);
            }
            AddPrim(&MainOt[1], (u32*) mt);
            mt++;
            if (cnt++ == 0x1FF) {
                full = 1;
            }
        }
    }
    if (pMemTile == NULL || full == 1) {
        mt = &tile[0];
        y1 = (u32) ((f32) (end - start) * 400.0f / (f32) size);
        mt->code = 4;
        mt->x0 = x;
        mt->y0 = 30;
        mt->z0 = 0;
        mt->w = 5;
        mt->h = y1;
        if (rest >= 0) {
            mt->r = 0x60;
            mt->g = 0x60;
            mt->b = 0x80;
        } else {
            mt->r = 0xFF;
            mt->g = 0x20;
            mt->b = 0x20;
        }
        mt->cd = 0xFF;
        if (SysRef(pSysView)->flags & 0x40000000) {
            mt->y0 = (s16) ((f32) mt->y0 / 1.3333334f + 56.0f);
            mt->h = (s16) ((f32) mt->h / 1.3333334f);
        }
        AddPrim(&MainOt[1], (u32*) mt);
    }
    if (CurrentHeap == 4) {
        for (cell = cell_dll; cell != NULL; cell = cell->next) {
            u8* tag = (u8*) cell + cell->size - 0x20;
            if ((s32)(uintptr_t)(tag) >= 0 || (u32)(uintptr_t)(tag) > PORT_PTR_MAX) {
                break;
            }
            if (MEM_TAG_OK(tag)) {
                eprintf2(8, 14, 230, ey, 22, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            } else {
                eprintf2(8, 14, 230, ey, 22, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
            }
            ey += 14;
            if (write == 1 && cell->size - 0x20 > 0x1000) {
                p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
            }
            next = cell->next;
            if (next != NULL && next->next != NULL &&
                ((s32) next->next >= 0 || (u32) next->next > PORT_PTR_MAX)) {
                pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
                pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 870
                HALT();
            }
        }
        for (cell = cell_stage; cell != NULL; cell = cell->next) {
            u8* tag = (u8*) cell + cell->size - 0x20;
            if ((s32)(uintptr_t)(tag) >= 0 || (u32)(uintptr_t)(tag) > PORT_PTR_MAX) {
                break;
            }
            if (MEM_TAG_OK(tag)) {
                eprintf2(8, 14, 230, ey, 22, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            } else {
                eprintf2(8, 14, 230, ey, 22, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
            }
            ey += 14;
            if (write == 1 && cell->size - 0x20 > 0x1000) {
                p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
            }
            next = cell->next;
            if (next != NULL && next->next != NULL &&
                ((s32) next->next >= 0 || (u32) next->next > PORT_PTR_MAX)) {
                pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
                pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 904
                HALT();
            }
        }
        for (cell = cell_game; cell != NULL; cell = cell->next) {
            u8* tag = (u8*) cell + cell->size - 0x20;
            if ((s32)(uintptr_t)(tag) >= 0 || (u32)(uintptr_t)(tag) > PORT_PTR_MAX) {
                break;
            }
            if (MEM_TAG_OK(tag)) {
                eprintf2(8, 14, 230, ey, 18, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            } else {
                eprintf2(8, 14, 230, ey, 18, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
            }
            if (write == 1 && cell->size - 0x20 > 0x1000) {
                p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
            }
            ey += 14;
            next = cell->next;
            if (next != NULL && next->next != NULL &&
                ((s32) next->next >= 0 || (u32) next->next > PORT_PTR_MAX)) {
                pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
                pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 938
                HALT();
            }
        }
        for (cell = cell_main; cell != NULL; cell = cell->next) {
            u8* tag = (u8*) cell + cell->size - 0x20;
            if ((s32)(uintptr_t)(tag) >= 0 || (u32)(uintptr_t)(tag) > PORT_PTR_MAX) {
                break;
            }
            if (MEM_TAG_OK(tag)) {
                eprintf2(8, 14, 230, ey, 20, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            } else {
                eprintf2(8, 14, 230, ey, 20, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
            }
            if (write == 1 && cell->size - 0x20 > 0x1000) {
                p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
            }
            ey += 14;
            next = cell->next;
            if (next != NULL && next->next != NULL &&
                ((s32) next->next >= 0 || (u32) next->next > PORT_PTR_MAX)) {
                pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
                pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 974
                HALT();
            }
        }
    }
    {
        mt = &tile[1];
        mt->code = 4;
        mt->x0 = x;
        mt->y0 = 30;
        mt->z0 = 0;
        mt->w = 5;
        mt->h = 400;
        mt->b = mt->g = mt->r = 0x20;
        mt->cd = 0xFF;
        if (SysRef(pSysView)->flags & 0x40000000) {
            mt->y0 = 78;
            mt->h = 300;
        }
        AddPrim(&MainOt[1], (u32*) mt);
    }
    eprintf2(10, 16, 30, 0x38, 0, 4, "elf_end   %8x", SysMem.arena_lo);
    eprintf2(10, 16, 30, 0x58, 0, 4, "DVD       %8x", SysMem.elf_end);
    eprintf2(10, 16, 30, 0x68, 0, 4, "SOUND     %8x", SysMem.dvd);
    eprintf2(10, 16, 30, 0x78, 0, 4, "FIFO      %8x", SysMem.sound);
    eprintf2(10, 16, 30, 0x88, 0, 4, "XFB       %8x", SysMem.fifo);
    eprintf2(10, 16, 30, 0x98, 0, 4, "CORE      %8x", SysMem.xfb);
    eprintf2(10, 16, 30, 0xA8, 0, 4, "OPTION    %8x", SysMem.core);
    eprintf2(10, 16, 30, 0xB8, 0, 4, "PLAYER    %8x", SysMem.option);
    eprintf2(10, 16, 30, 0xC8, 0, 4, "WEAPON    %8x", SysMem.player);
    eprintf2(10, 16, 30, 0xD8, 0, 4, "HEAP_TOP  %8x", SysMem.weapon);
    eprintf2(10, 16, 30, 0xE8, 0, 4, "HEAP_SIZE %x", SysMem.heap_end - SysMem.weapon);
    eprintf2(10, 16, 30, 0x108, 0, 4, "NOW_HEAP  %8x", start);
    eprintf2(10, 16, 30, 0x118, 0, 4, " SIZE     %x", size);
    eprintf2(10, 16, 30, 0x128, 0, 4, " REST     %x", rest);
    eprintf2(10, 16, 30, 0x148, 0, 4, "FST_SIZE  %x", DvdView.freeSize);
    eprintf2(10, 16, 30, 0x168, 0, 4, "USB       %8x", SysMem.usb);
    eprintf2(10, 16, 30, 0x178, 0, 4, "DEBUG     %8x", SysMem.debug);
    if (Joy[0].rep2 & 0x400000) {
        _epy_base -= 16;
    }
    if (Joy[0].rep2 & 0x800000) {
        _epy_base += 16;
    }
    _epy = _epy_base;
    if (write == 1) {
        HDWrite("d:\\bio4\\prog\\memlog.txt", buf, p - buf);
        Debug_free(buf);
    }
}

// Dead-stripped helper: only its string and statics survive (STRIP_UNUSED).
static void memSetCheck()
{
    static int memSetErrCnt = 0;

    memSetErrCnt++;
    OSReport("memset error: ");
}

// main_sub's .bss starts 8-aligned; the split object carries the 4-byte pad.
#ifndef RE4_PORT   // portage : placement en sections, sans objet hors du build matching
#ifndef RE4_PORT   // portage : placement en sections, sans objet hors du build matching
#ifndef RE4_PORT   // portage : placement en sections, sans objet hors du build matching
#ifndef RE4_PORT   // portage : placement en sections, sans objet hors du build matching
asm(".section .bss; .balign 8");
#endif
#endif
#endif
#endif
