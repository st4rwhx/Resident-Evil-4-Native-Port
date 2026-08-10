/* newlib libc/string/strlen.c */
#include "newlib_local.h"

#define UNALIGNED(X) ((long)X & (LBLOCKSIZE - 1))

size_t strlen(const char *str)
{
    const char *start = str;
    unsigned long *aligned_addr;

    if (!UNALIGNED(str)) {
        aligned_addr = (unsigned long *)str;
        while (!DETECTNULL(*aligned_addr))
            aligned_addr++;
        str = (char *)aligned_addr;
    }

    while (*str)
        str++;
    return str - start;
}
