#ifndef _DOLPHIN_TYPES_H_
#define _DOLPHIN_TYPES_H_

typedef signed   char          s8;
typedef unsigned char          u8;
typedef signed   short int     s16;
typedef unsigned short int     u16;
typedef signed   long          s32;
typedef unsigned long          u32;
typedef signed   long long int s64;
typedef unsigned long long int u64;

typedef float  f32;
typedef double f64;

typedef char *Ptr;

typedef int BOOL;

#define FALSE 0
#define TRUE 1

#if defined(RE4_PORT)
// Portage : ces variables designent des registres materiels et des mots fixes de la memoire
// basse du GameCube. Les en-tetes les **definissent** au lieu de les declarer, ce qui en C
// donne des symboles communs que l'editeur de liens fusionne ; en C++ ce sont autant de vraies
// definitions, et le lien echoue des qu'un en-tete est inclus par deux unites - "duplicate
// symbol: __OSExecParams", vu sur le premier harnais qui compilait deux fichiers.
// `weak` retablit exactement la semantique du C d'origine, en un point unique, sans toucher
// aux trente declarations ni masquer de vrais doublons avec --allow-multiple-definition.
#define AT_ADDRESS(addr) __attribute__((weak))
#elif defined(__MWERKS__)
#define AT_ADDRESS(addr) : (addr)
#elif defined(__GNUC__)
//#define AT_ADDRESS(addr) __attribute__((address((addr))))
#define AT_ADDRESS(addr)  // was removed in GCC. define in linker script instead.
#else
#error unknown compiler
#endif

#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))

#define INT_MAX 2147483647

#ifndef NULL
#define NULL ((void*)0)
#endif

#include "libc/stdio.h"
#include "libc/stdarg.h"
#include "libc/string.h"
#include "libc/ctype.h"

#include "cmath.h"

#endif
