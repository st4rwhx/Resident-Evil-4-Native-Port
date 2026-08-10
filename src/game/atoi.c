/* newlib libc/stdlib/atoi.c */
#include "newlib_local.h"

int atoi(const char *s)
{
    return (int)strtol(s, NULL, 10);
}
