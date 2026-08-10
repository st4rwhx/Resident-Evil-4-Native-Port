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

#ifdef __cplusplus
}
#endif

#endif
