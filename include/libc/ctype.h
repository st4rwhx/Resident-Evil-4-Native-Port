#ifndef _DOLPHIN_LIBC_CTYPE_H_
#define _DOLPHIN_LIBC_CTYPE_H_

#ifdef RE4_PORT
// Portage : sur une plateforme hote, la libc est celle du systeme, et on delegue plutot que de
// redeclarer. Redeclarer ne marche pas : MinGW ne declare pas printf avec une liaison C en C++
// mais comme une surcharge en ligne (ANSI stdio), et la moindre redeclaration la contredit,
// quelle que soit la liaison choisie. Le corps ci-dessous reste pour le build matching, ou ces
// fonctions viennent de la libc de ProDG - et ou la disposition de FILE compte vraiment.
#include <ctype.h>
#else


int tolower(int c);

#endif   // RE4_PORT

#endif // _DOLPHIN_LIBC_CTYPE_H_
