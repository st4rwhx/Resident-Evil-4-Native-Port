/* newlib libc/string/memset.c */
#include "newlib_local.h"

#define UNALIGNED(X) ((long)X & (LBLOCKSIZE - 1))
#define TOO_SMALL(LEN) ((LEN) < LBLOCKSIZE)

void *memset(void *m, int c, size_t n)
{
    char *s = (char *)m;
    int i;
    unsigned long buffer;
    unsigned long *aligned_addr;

    if (!TOO_SMALL(n) && !UNALIGNED(m)) {
        /* If we get this far, we know that n is large and m is word-aligned. */

        aligned_addr = (unsigned long *)m;

        /* Store C into each char sized location in BUFFER so that
           we can set large blocks quickly.  */
        c &= 0xff;
        if (LBLOCKSIZE == 4) {
            buffer = (c << 8) | c;
            buffer |= (buffer << 16);
        } else {
            buffer = 0;
            for (i = 0; i < LBLOCKSIZE; i++)
                buffer = (buffer << 8) | c;
        }

        while (n >= LBLOCKSIZE * 4) {
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            n -= 4 * LBLOCKSIZE;
        }

        while (n >= LBLOCKSIZE) {
            *aligned_addr++ = buffer;
            n -= LBLOCKSIZE;
        }
        /* Pick up the remainder with a bytewise loop.  */
        s = (char *)aligned_addr;
    }

    while (n--) {
        *s++ = (char)c;
    }

    return m;
}
