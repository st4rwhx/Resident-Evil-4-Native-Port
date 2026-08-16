#ifndef TYPES_H
#define TYPES_H

typedef signed char s8;
typedef signed short s16;
#if defined(RE4_PORT) && defined(__LP64__)
/* Portage 64 bits (N7, 17/11) : `long` y fait 8 octets ; u32/s32 doivent rester sur 4 (disposition GameCube). */
typedef signed int s32;
#else
typedef signed long s32;
#endif
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
#ifdef RE4_PORT
/* Portage : les pointeurs valides ne sont plus tous dans les 48 Mo du GameCube - la pile et les variables
 * statiques vivent ailleurs. Seuls les decalages de fichier (en dessous de 0x80000000) restent distincts. */
#if defined(__i386__) || defined(__LP64__)
/* 64 bits (N7) : les valeurs 32 bits des pointeurs sont des decalages dans la fenetre de 4 Go (port/ptr32.h), MEM1 a
 * 0x80000000, l'image du programme ailleurs dans la fenetre : meme plage large qu'en x86 32 bits. */
/* x86 32 bits (banc G0, MuMu, 15/11) : le programme est charge vers 0x56000000 et la pile est sous 0xFFFF0000 ; les
 * decalages de fichier restent petits (sous 0x10000000). 0xFFFFFFFF (sentinelle -1) reste invalide. */
#define PORT_PTR_MIN 0x10000000
#define PORT_PTR_MAX 0xFFFDFFFF
#define PORT_PTR_SPAN 0xEFFDFFFF
#else
#define PORT_PTR_MIN 0x80000000
#define PORT_PTR_MAX 0xBFFFFFFF
#define PORT_PTR_SPAN 0x3FFFFFFF
#endif
#else
#define PORT_PTR_MIN 0x80000000
#define PORT_PTR_MAX 0x82FFFFFF
#define PORT_PTR_SPAN 0x02FFFFFF
#endif
#if defined(RE4_PORT) && defined(__LP64__)
typedef unsigned int u32;
#else
typedef unsigned long u32;
#endif
typedef unsigned long long u64;

/* cDmgInfo (cEm+0x324, include/em.h) : le mot `flags_324` recouvre les octets stat, x1, kind, x3. Le code teste
 * « mort / impact en cours » par `flags_324 & 0xFFFF0000` : sur le PowerPC ce sont les octets 0-1 (stat, x1) ; sur une
 * machine petit-boutiste ce sont kind et x3. Or kind (type d'arme) n'est jamais efface : apres le premier impact, tout
 * ennemi passait pour mort (EmIsDead) et les balles l'ignoraient - le Ganado de la maison de r100 ne prenait qu'un tir
 * (16/11, Studio : juge_maison + appeler). Ce masque designe les memes octets 0-1 sur les deux. */
#ifdef RE4_PORT
#define PORT_DMG_HI16 0x0000FFFFu
#else
#define PORT_DMG_HI16 0xFFFF0000u
#endif
typedef float f32;
typedef double f64;
typedef int BOOL;

#ifndef NULL
#define NULL 0
#endif

#ifndef RE4_PORT   // build d'origine : la couleur vue comme un mot, tel quel (portage : port/port_prelude.h)
#define PORT_COLW(p) (*(u32*) (p))
#define PORT_SETCOLW(p, v) (*(u32*) (p) = (v))
#define PORT_HL32(v) (v)
#endif

#endif
