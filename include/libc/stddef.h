#ifndef _STDDEF_H_
#define _STDDEF_H_

#if defined(RE4_PORT) && defined(__cplusplus)
// Portage : liaison C. Ces fonctions sont celles de la libc, et sur une plateforme hote les
// en-tetes du systeme les declarent aussi - avec la liaison C, comme il se doit. Sans cette
// precision, la premiere declaration vue est traitee comme du C++ et les deux entrent en
// conflit des que <cstdio>, <cctype> ou SDL passent par la.
extern "C" {
#endif

#define offsetof(type, member)	((size_t) &(((type *) 0)->member))

typedef unsigned int size_t;

#ifndef NULL
#define NULL 0L
#endif

#if defined(RE4_PORT) && defined(__cplusplus)
}
#endif

#endif
