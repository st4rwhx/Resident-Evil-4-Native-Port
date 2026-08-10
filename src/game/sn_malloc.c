/* SN ProDG libc stubs: the heap functions only report the caller and were dead-stripped;
 * their messages and two static words survived. */
#include "newlib_stdio.h"

static int sn_malloc_called = 0;
static int sn_free_called = 0;

void *malloc(size_t n)
{
    sn_malloc_called++;
    printf("\n*** Library error ***\nAn external call has been made to 'malloc'\nCalling function address: 0x%X\nPlease See the ProDG manual\n",
           (unsigned)__builtin_return_address(0));
    return NULL;
}

void free(void *p)
{
    sn_free_called++;
    printf("\n*** Library error ***\nAn external call has been made to 'free'\nCalling function address: 0x%X\nPlease See the ProDG manual\n",
           (unsigned)__builtin_return_address(0));
}

void *realloc(void *p, size_t n)
{
    printf("\n*** Library error ***\nAn external call has been made to 'realloc'\nCalling function address: 0x%X\nPlease See the ProDG manual\n",
           (unsigned)__builtin_return_address(0));
    return NULL;
}

void *calloc(size_t n, size_t m)
{
    printf("\n*** Library error ***\nAn external call has been made to 'calloc'\nCalling function address: 0x%X\nPlease See the ProDG manual\n",
           (unsigned)__builtin_return_address(0));
    return NULL;
}
