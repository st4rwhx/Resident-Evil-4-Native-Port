/* libgcc: gcc 2.95.3 libgcc2.c section L_eh */
#define L_eh
#include "libgcc2/libgcc2.c"

/* SN ProDG additions to the exception runtime: the heap hooks `_register_malloc` installs for
 * new_eh_context (the DOL's `malloc` is a stub, see game/sn_malloc). The runtime was dead-stripped
 * with them; the linker removes unreferenced statics in 8-byte units, so these three words and
 * `initialized` above leave the DOL's 16 .bss bytes at 0x802821D0. */
static void *(*eh_malloc_hook)(unsigned int);
static void (*eh_free_hook)(void *);
static void *(*eh_realloc_hook)(void *, unsigned int);
