// -*- coding: utf-8 -*-
// En-tete force en tete de **chaque** unite du portage (-include), et de rien d'autre :
// hors RE4_PORT le build matching ne le voit jamais.
//
// Il n'existe que pour une raison, et il vaut mieux qu'elle soit ecrite ici que redecouverte
// plus tard : le compilateur d'origine (Metrowerks, puis ProDG) traite `memcpy`, `strcmp` et
// leurs voisines comme des **intrinseques**, disponibles sans declaration. Le decompile
// reproduit ce code tel quel, donc une trentaine d'unites les appellent sans jamais inclure
// <string.h>. clang exige la declaration.
//
// On declare donc ces fonctions ici, **a la main**, plutot que d'inclure <string.h> de Bionic :
// mesure faite, cet include tire <sys/types.h>, dont dev_t / ino_t / mode_t entrent en conflit
// avec `newlib_stdio.h` du depot et cassent les huit unites newlib (180 redefinitions de
// typedef). Une declaration compatible coute une ligne et ne tire rien.
#ifndef PORT_PRELUDE_H
#define PORT_PRELUDE_H

// Couleur GXColor (octets R, G, B, A) vue comme un mot : le jeu y lit / ecrit 0xRRGGBBAA (ordre du GameCube).
// Sur ARM un *(u32*) donnerait 0xAABBGGRR : ces deux aides gardent l'ordre d'origine (scroll.cpp smxInit, fondus...).
static inline unsigned int PORT_COLW(const void* p)
{
    const unsigned char* b = (const unsigned char*) p;
    return ((unsigned int) b[0] << 24) | ((unsigned int) b[1] << 16) | ((unsigned int) b[2] << 8) | (unsigned int) b[3];
}
static inline void PORT_SETCOLW(void* p, unsigned int v)
{
    unsigned char* b = (unsigned char*) p;
    b[0] = (unsigned char) (v >> 24);
    b[1] = (unsigned char) (v >> 16);
    b[2] = (unsigned char) (v >> 8);
    b[3] = (unsigned char) v;
}

#include "disc_ptr.h"
#include "ptr32.h"   // N7 (17/11) : P32<T> / FnP32<F>, champs pointeurs de 4 octets en 32 et 64 bits

#ifdef __cplusplus
extern "C" {
#endif

// `decltype(sizeof(0))` en C++ et `__SIZE_TYPE__` en C : size_t par construction, sans en-tete.
#ifdef __cplusplus
#define PORT_SIZE_T decltype(sizeof(0))
#else
#define PORT_SIZE_T __SIZE_TYPE__
#endif

void *memcpy(void *, const void *, PORT_SIZE_T);
void *memmove(void *, const void *, PORT_SIZE_T);
void *memset(void *, int, PORT_SIZE_T);
int memcmp(const void *, const void *, PORT_SIZE_T);

PORT_SIZE_T strlen(const char *);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, PORT_SIZE_T);
char *strcat(char *, const char *);
char *strncat(char *, const char *, PORT_SIZE_T);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, PORT_SIZE_T);

// Le mode d un evenement, entre la macro BEGIN_EVENT/END_EVENT et cPlayer::beginEvent() -
// qui le lisait dans r4 sur PowerPC, parce que cUnit::beginEvent prend un int dans le jeu
// d origine et que le decompile ne le declare pas (dette notee dans include/cManager.h).
// Le jeu est mono-thread pour ces appels : la macro ecrit, la fonction lit aussitot.
extern int port_event_mode;

// ---------------------------------------------------------------- donnees du disque
//
// Les donnees de RE4 sont rangees en gros-boutiste, comme son processeur. `DISC_STRUCT`
// se pose sur une structure lue depuis le disque : le compilateur retourne alors les octets
// a **chaque acces**, en lecture comme en ecriture, entiers et flottants compris. Le code du
// jeu reste intact - aucun acces a reecrire dans les 520 000 lignes.
//
// **Seul GCC l implemente.** Clang ne refuse pas l attribut : il l ignore avec un simple
// avertissement, et le programme compile en lisant des valeurs fausses. C est le pire des
// comportements possibles, donc on refuse explicitement de compiler ces structures avec un
// compilateur qui ne sait pas les traiter, plutot que de produire un binaire qui ment.
// Les quatre adresses en dur de RE4.
//
// Le jeu place quatre de ses tampons a des adresses ecrites en clair dans le code
// (core.das, les options, le joueur, les armes ; voir src/game/read.cpp et snd.cpp). Sur le
// telephone ce n'est pas un probleme : port/os.cpp reserve MEM1 a son adresse d'origine,
// 0x80000000, que l'espace utilisateur 32 bits laisse libre. Sur PC, aurora alloue MEM1 ou
// elle peut - sa documentation le dit : « it will not be allocated at the exact 0x80000000
// address, as that cannot be guaranteed » - et publie la base dans OSBaseAddress.
//
// On rebase donc ces quatre adresses, et seulement elles. On passe par une fonction plutot
// que par OSBaseAddress directement pour ne pas redeclarer un symbole d'aurora dans un
// en-tete force-inclus partout, y compris dans du C.
#if defined(RE4_PORT_AURORA)
unsigned long port_mem1_base(void);
#define PORT_MEM1(a) (port_mem1_base() + ((unsigned long) (a) - 0x80000000ul))
#else
// N7 (18/11) : en 64 bits la memoire du jeu est dans la fenetre de 4 Go : base | adresse (en 32 bits port_ptr64 est
// l'identite). `(u32) PORT_MEM1(x)` rend toujours x.
#define PORT_MEM1(a) ((unsigned long) (uintptr_t) port_ptr64((uint32_t) (a)))
#endif

#if defined(RE4_PORT)
#  if defined(__GNUC__) && !defined(__clang__)
#    define DISC_STRUCT __attribute__((scalar_storage_order("big-endian")))
#    define PORT_HAVE_DISC_STRUCT 1
#  else
#    define DISC_STRUCT PORT_DISC_STRUCT_NEEDS_GCC
#    define PORT_HAVE_DISC_STRUCT 0
#  endif
#else
#  define DISC_STRUCT
#  define PORT_HAVE_DISC_STRUCT 0
#endif


#ifdef __cplusplus
}
#endif


// Intrinseques PowerPC du compilateur d'origine, appeles par les bibliotheques de la decompilation
// (include/fake_tgmath.h : __frsqrte puis trois iterations de Newton, __fabsf). Ici en C portable.
#ifndef PORT_PPC_INTRINSICS
#define PORT_PPC_INTRINSICS
static inline double __frsqrte(double x) { return 1.0 / __builtin_sqrt(x); }
static inline float __fabsf(float x) { return __builtin_fabsf(x); }
// rlwimi : rotation a gauche de `sh`, puis insertion sous le masque des bits mb..me (bit 0 = poids fort),
// le reste vient de `a`. cntlzw : zeros de tete (32 pour zero). Composent les registres du GPU (SDK GX).
static inline unsigned int __port_ppc_mask(int mb, int me) {
    const unsigned int hi = 0xFFFFFFFFu >> (mb & 31), lo = 0xFFFFFFFFu << (31 - (me & 31));
    return (mb <= me) ? (hi & lo) : (hi | lo);
}
static inline unsigned int __rlwimi(unsigned int a, unsigned int s, int sh, int mb, int me) {
    const unsigned int r = (sh & 31) ? ((s << (sh & 31)) | (s >> (32 - (sh & 31)))) : s;
    const unsigned int m = __port_ppc_mask(mb, me);
    return (r & m) | (a & ~m);
}
static inline unsigned int __cntlzw(unsigned int x) { return x ? (unsigned int)__builtin_clz(x) : 32u; }
#endif

// Paires de demi-mots du DSP (adresses Hi/Lo des blocs de voix AX, rapport de reechantillonnage) lues ou ecrites
// comme un mot de 32 bits par le SDK et le pilote de Capcom : en gros-boutiste la moitie haute vient d'abord en
// memoire ; en petit-boutiste il faut echanger les deux moities pour que le champ Hi recoive bien la moitie haute.
#define PORT_HL32(v) ((unsigned int) ((((unsigned int) (v)) << 16) | (((unsigned int) (v)) >> 16)))


// Cadence (15/11) : sur GameCube, l'interruption de balayage (59,94 Hz) appelle postVSyncCallback, qui fait avancer
// vsync_cnt ; le jeu attend dans des boucles vides `while (vsync_cnt < n) {}`. Le portage n'a pas d'interruption, et
// l'optimiseur supprimait ces boucles (le jeu tournait a la vitesse de l'affichage). PORT_VSYNC_WAIT dort jusqu'au
// prochain balayage et appelle le rappel pour chaque balayage ecoule (port/vi.cpp).
#ifdef __cplusplus
extern "C" void port_vi_wait_field(void);
#define PORT_VSYNC_WAIT(cond) do { while (cond) port_vi_wait_field(); } while (0)
#endif


// Listes d'affichage GX construites par le jeu lui-meme (effets Espgen42/43/45, eff_sys) : le flux GX est gros-boutiste,
// le jeu y ecrit compteurs, indices de sommets et coordonnees flottantes en mots natifs (15/11).
#define PORT_DL16(d, v) do { const unsigned int port_dl16_ = (unsigned int) (v); (d)[0] = (unsigned char) (port_dl16_ >> 8); (d)[1] = (unsigned char) port_dl16_; } while (0)
#define PORT_DLF32(d, v) do { union { float f; unsigned int u; } port_dlf_; port_dlf_.f = (float) (v); (d)[0] = (unsigned char) (port_dlf_.u >> 24); (d)[1] = (unsigned char) (port_dlf_.u >> 16); (d)[2] = (unsigned char) (port_dlf_.u >> 8); (d)[3] = (unsigned char) port_dlf_.u; } while (0)

#endif