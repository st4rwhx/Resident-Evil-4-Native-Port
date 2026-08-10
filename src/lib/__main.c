/* SN ProDG crt: gcc 2.95.3 libgcc2.c L__main without the (unused) __do_global_dtors; built as part
 * of the crt with -G 0, so `initialized' is plain .bss addressed with lis/lwz. */
#include "tconfig.h"
#include "gbl-ctors.h"

/* Run all the global constructors on entry to the program.  */

void __do_global_ctors()
{
    DO_GLOBAL_CTORS_BODY;
}

/* Subroutine called automatically by `main'.
   Compiling a global function named `main'
   produces an automatic call to this function at the beginning.  */

void __main()
{
    /* Support recursive calls to `main': run initializers just once.  */
    static int initialized;
    if (!initialized) {
        initialized = 1;
        __do_global_ctors();
    }
}
