/* newlib libc/string/memmove.c */
#include "newlib_local.h"

/* Nonzero if either X or Y is not aligned on a "long" boundary.  */
#define UNALIGNED(X, Y) (((long)X & (sizeof(long) - 1)) | ((long)Y & (sizeof(long) - 1)))

/* How many bytes are copied each iteration of the 4X unrolled loop.  */
#define BIGBLOCKSIZE (sizeof(long) << 2)

/* How many bytes are copied each iteration of the word copy loop.  */
#define LITTLEBLOCKSIZE (sizeof(long))

/* Threshhold for punting to the byte copier.  */
#define TOO_SMALL(LEN) ((LEN) < BIGBLOCKSIZE)

void *memmove(void *dst_void, const void *src_void, size_t length)
{
    char *dst = dst_void;
    const char *src = src_void;
    long *aligned_dst;
    const long *aligned_src;
    size_t len = length;

    if (src < dst && dst < src + len) {
        /* Destructive overlap...have to copy backwards */
        src += len;
        dst += len;
        while (len--) {
            *--dst = *--src;
        }
    } else {
        /* Use optimizing algorithm for a non-destructive copy to closely
           match memcpy. If the size is small or either SRC or DST is unaligned,
           then punt into the byte copy loop.  This should be rare.  */
        if (!TOO_SMALL(len) && !UNALIGNED(src, dst)) {
            aligned_dst = (long *)dst;
            aligned_src = (long *)src;

            /* Copy 4X long words at a time if possible.  */
            while (len >= BIGBLOCKSIZE) {
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                len -= BIGBLOCKSIZE;
            }

            /* Copy one long word at a time if possible.  */
            while (len >= LITTLEBLOCKSIZE) {
                *aligned_dst++ = *aligned_src++;
                len -= LITTLEBLOCKSIZE;
            }

            /* Pick up any residual with a byte copier.  */
            dst = (char *)aligned_dst;
            src = (char *)aligned_src;
        }

        while (len--) {
            *dst++ = *src++;
        }
    }

    return dst_void;
}
