/* newlib libc/ctype/isspace.c */
#include "newlib_local.h"

int isspace(int c)
{
    return ((_ctype_ + 1)[c] & _S);
}
