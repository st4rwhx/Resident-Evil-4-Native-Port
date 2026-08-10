/* newlib libc/ctype/isdigit.c */
#include "newlib_local.h"

int isdigit(int c)
{
    return ((_ctype_ + 1)[c] & _N);
}
