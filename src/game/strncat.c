/* newlib libc/string/strncat.c */
#include "newlib_local.h"

#define ALIGNED(X) (((long)X & (sizeof(long) - 1)) == 0)

char *strncat(char *s1, const char *s2, size_t n)
{
    char *s = s1;

    /* Skip over the data in s1 as quickly as possible.  */
    if (ALIGNED(s1)) {
        unsigned long *aligned_s1 = (unsigned long *)s1;
        while (!DETECTNULL(*aligned_s1))
            aligned_s1++;

        s1 = (char *)aligned_s1;
    }

    while (*s1)
        s1++;

    /* s1 now points to the its trailing null character, now copy
       up to N bytes from S2 into S1 stopping if a NULL is encountered
       in S2.

       It is not safe to use strncpy here since it copies EXACTLY N
       characters, NULL padding if necessary.  */
    while (n-- != 0 && (*s1++ = *s2++)) {
        if (n == 0)
            *s1 = '\0';
    }

    return s;
}
