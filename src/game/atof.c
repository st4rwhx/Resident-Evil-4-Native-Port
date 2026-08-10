/* newlib libc/stdlib/atof.c */
#include "newlib_local.h"

double atof(const char *s)
{
    return strtod(s, NULL);
}
