#ifndef _STDIO_H_
#define _STDIO_H_

#ifdef RE4_PORT
// Portage : sur une plateforme hote, la libc est celle du systeme, et on delegue plutot que de
// redeclarer. Redeclarer ne marche pas : MinGW ne declare pas printf avec une liaison C en C++
// mais comme une surcharge en ligne (ANSI stdio), et la moindre redeclaration la contredit,
// quelle que soit la liaison choisie. Le corps ci-dessous reste pour le build matching, ou ces
// fonctions viennent de la libc de ProDG - et ou la disposition de FILE compte vraiment.
#include <stdio.h>
#else


#include <stdarg.h>

typedef struct
{
    int unk0;
    unsigned short unk4b0:7;
    unsigned short unk4b7:3;
    unsigned short unk4b10:2;
    unsigned short unk4b12:1;
    unsigned char filler6[0x14-0x6];
    int unk14;
    int unk18;
    int unk1C;
    int unk20;
    int unk24;
    int unk28;
    unsigned char filler2C[0x30-0x2C];
    int unk30;
    unsigned char filler34[0x3C-0x34];
    int (*unk3C)();
    unsigned char filler40[4];
    int unk44;
} FILE;

int puts(const char *s);
int printf(const char *, ...);
int sprintf(char *s, const char *format, ...);
int vprintf(const char *format, va_list arg);
int vsprintf(char *s, const char *format, va_list arg);

#endif   // RE4_PORT

#endif
