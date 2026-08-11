#ifndef _WCHAR_H_
#define _WCHAR_H_

#if defined(RE4_PORT) && defined(__cplusplus)
// Portage : liaison C. Ces fonctions sont celles de la libc, et sur une plateforme hote les
// en-tetes du systeme les declarent aussi - avec la liaison C, comme il se doit. Sans cette
// precision, la premiere declaration vue est traitee comme du C++ et les deux entrent en
// conflit des que <cstdio>, <cctype> ou SDL passent par la.
extern "C" {
#endif

#include <stdio.h>

typedef unsigned short wchar_t;

int fwide(FILE *stream, int mode);

#if defined(RE4_PORT) && defined(__cplusplus)
}
#endif

#endif
