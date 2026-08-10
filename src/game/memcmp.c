/* newlib libc/string/memcmp.c */
#include "newlib_local.h"

/* Nonzero if either X or Y is not aligned on a "long" boundary.  */
#define UNALIGNED(X, Y) (((long)X & (sizeof(long) - 1)) | ((long)Y & (sizeof(long) - 1)))

/* Threshhold for punting to the byte copier.  */
#define TOO_SMALL(LEN) ((LEN) < LBLOCKSIZE)

int memcmp(const void *m1, const void *m2, size_t n)
{
    unsigned char *s1 = (unsigned char *)m1;
    unsigned char *s2 = (unsigned char *)m2;
    unsigned long *a1;
    unsigned long *a2;

    /* If the size is too small, or either pointer is unaligned,
       then we punt to the byte compare loop.  Hopefully this will
       not turn up in inner loops.  */
    if (!TOO_SMALL(n) && !UNALIGNED(s1, s2)) {
        /* Otherwise, load and compare the blocks of memory one
           word at a time.  */
        a1 = (unsigned long *)s1;
        a2 = (unsigned long *)s2;
        while (n >= LBLOCKSIZE) {
            if (*a1 != *a2)
                break;
            a1++;
            a2++;
            n -= LBLOCKSIZE;
        }

        /* check m mod LBLOCKSIZE remaining characters */

        s1 = (unsigned char *)a1;
        s2 = (unsigned char *)a2;
    }

    while (n--) {
        if (*s1 != *s2)
            return *s1 - *s2;
        s1++;
        s2++;
    }
    return 0;
}
