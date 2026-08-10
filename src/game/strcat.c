/* newlib libc/string/strcat.c */
#include "newlib_local.h"

#define ALIGNED(X) (((long)X & (sizeof(long) - 1)) == 0)

char *strcat(char *s1, const char *s2)
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

    /* s1 now points to the its trailing null character, we can
       just use strcpy to do the work for us now.  */
    strcpy(s1, s2);

    return s;
}
