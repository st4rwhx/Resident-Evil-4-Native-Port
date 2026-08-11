#ifndef _LIMITS_H_
#define _LIMITS_H_

#if defined(RE4_PORT) && defined(__cplusplus)
// Portage : liaison C. Ces fonctions sont celles de la libc, et sur une plateforme hote les
// en-tetes du systeme les declarent aussi - avec la liaison C, comme il se doit. Sans cette
// precision, la premiere declaration vue est traitee comme du C++ et les deux entrent en
// conflit des que <cstdio>, <cctype> ou SDL passent par la.
extern "C" {
#endif

#define CHAR_BIT 8

#define SCHAR_MIN -128
#define SCHAR_MAX 127
#define UCHAR_MAX 255

#define SHRT_MIN -32768
#define SHRT_MAX 32767
#define USHRT_MAX 65535

#define INT_MIN -2147483648
#define INT_MAX 2147483647
#define UINT_MAX 4294967295

#define LONG_MIN -2147483648
#define LONG_MAX 2147483647
#define ULONG_MAX 4294967295

#if defined(RE4_PORT) && defined(__cplusplus)
}
#endif

#endif
