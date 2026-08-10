/* newlib libc/string/strncpy.c */
#include "newlib_local.h"

#define UNALIGNED(X, Y) (((long)X & (sizeof(long) - 1)) | ((long)Y & (sizeof(long) - 1)))
#define TOO_SMALL(LEN) ((LEN) < sizeof(long))

char *strncpy(char *dst0, const char *src0, size_t count)
{
    char *dst = dst0;
    const char *src = src0;
    long *aligned_dst;
    const long *aligned_src;

    /* If SRC and DEST is aligned and count large enough, then copy words.  */
    if (!UNALIGNED(src, dst) && !TOO_SMALL(count)) {
        aligned_dst = (long *)dst;
        aligned_src = (long *)src;

        /* SRC and DEST are both "long int" aligned, try to do "long int"
           sized copies.  */
        while (count >= sizeof(long int) && !DETECTNULL(*aligned_src)) {
            count -= sizeof(long int);
            *aligned_dst++ = *aligned_src++;
        }

        dst = (char *)aligned_dst;
        src = (char *)aligned_src;
    }

    while (count > 0) {
        --count;
        if ((*dst++ = *src++) == '\0')
            break;
    }

    while (count-- > 0)
        *dst++ = '\0';

    return dst0;
}
