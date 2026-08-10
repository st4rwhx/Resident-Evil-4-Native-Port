/* newlib libc/string/strchr.c */
#include "newlib_local.h"

#define UNALIGNED(X) ((long)X & (sizeof(long) - 1))

char *strchr(const char *s1, int i)
{
    const unsigned char *s = (const unsigned char *)s1;
    unsigned char c = (unsigned int)i;
    unsigned long mask, j;
    unsigned long *aligned_addr;

    if (!UNALIGNED(s)) {
        mask = 0;
        for (j = 0; j < LBLOCKSIZE; j++)
            mask = (mask << 8) | c;

        aligned_addr = (unsigned long *)s;
        while (!DETECTNULL(*aligned_addr) && !DETECTCHAR(*aligned_addr, mask))
            aligned_addr++;

        /* The block of bytes currently pointed to by aligned_addr
           contains either a null or the target char, or both.  We
           catch it using the bytewise search.  */

        s = (unsigned char *)aligned_addr;
    }

    while (*s && *s != c)
        s++;
    if (*s == c)
        return (char *)s;
    return NULL;
}
