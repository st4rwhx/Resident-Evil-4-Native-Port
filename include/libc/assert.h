#ifndef _ASSERT_H_
#define _ASSERT_H_

#if defined(RE4_PORT) && defined(__cplusplus)
// Portage : liaison C. Ces fonctions sont celles de la libc, et sur une plateforme hote les
// en-tetes du systeme les declarent aussi - avec la liaison C, comme il se doit. Sans cette
// precision, la premiere declaration vue est traitee comme du C++ et les deux entrent en
// conflit des que <cstdio>, <cctype> ou SDL passent par la.
extern "C" {
#endif

#if __STDC_VERSION__ >= 201112L
// The C11 way
#define static_assert(cond, msg) _Static_assert(cond, #msg)
#else
// The old, hacky way
#define static_assert(cond, msg) typedef char static_assertion_##msg[(cond)?1:-1]
#endif

#if defined(RE4_PORT) && defined(__cplusplus)
}
#endif

#endif
