#include "cri_xpt.h"
#include <string.h>

void MEM_Copy(void *dst, const void *src, Uint32 nbytes)
{
	memcpy(dst, src, nbytes);
}
