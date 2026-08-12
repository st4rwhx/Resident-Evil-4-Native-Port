#ifndef TYPES_H
#define TYPES_H

typedef signed char s8;
typedef signed short s16;
typedef signed long s32;
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
#ifdef RE4_PORT
/* Portage : les pointeurs valides ne sont plus tous dans les 48 Mo du GameCube - la pile et les variables
 * statiques vivent ailleurs. Seuls les decalages de fichier (en dessous de 0x80000000) restent distincts. */
#define PORT_PTR_MAX 0xBFFFFFFF
#define PORT_PTR_SPAN 0x3FFFFFFF
#else
#define PORT_PTR_MAX 0x82FFFFFF
#define PORT_PTR_SPAN 0x02FFFFFF
#endif
typedef unsigned long u32;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef int BOOL;

#ifndef NULL
#define NULL 0
#endif

#ifndef RE4_PORT   // build d'origine : la couleur vue comme un mot, tel quel (portage : port/port_prelude.h)
#define PORT_COLW(p) (*(u32*) (p))
#define PORT_SETCOLW(p, v) (*(u32*) (p) = (v))
#endif

#endif
