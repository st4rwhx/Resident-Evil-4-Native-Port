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
#define PORT_MEM1(a) ((unsigned long) (a))
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

#endif
