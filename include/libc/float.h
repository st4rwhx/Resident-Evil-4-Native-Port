#ifndef _FLOAT_H_
#define _FLOAT_H_

#if defined(RE4_PORT) && defined(__cplusplus)
// Portage : liaison C. Ces fonctions sont celles de la libc, et sur une plateforme hote les
// en-tetes du systeme les declarent aussi - avec la liaison C, comme il se doit. Sans cette
// precision, la premiere declaration vue est traitee comme du C++ et les deux entrent en
// conflit des que <cstdio>, <cctype> ou SDL passent par la.
extern "C" {
#endif

#define FLT_EPSILON 1.1920928955078125e-07f

#if defined(RE4_PORT) && defined(__cplusplus)
}
#endif

#endif
