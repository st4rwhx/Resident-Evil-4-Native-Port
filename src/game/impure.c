/* newlib 1.8.2 libc/reent/impure.c */
#include "newlib_stdio.h"

struct _reent impure_data = _REENT_INIT(impure_data);
struct _reent *_impure_ptr = &impure_data;
