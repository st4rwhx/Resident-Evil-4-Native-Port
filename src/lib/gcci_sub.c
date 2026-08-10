#include "cri_xpt.h"

#pragma explicit_zero_data on

Sint32 gcg_ci_rdmode = 0;
Sint32 gcg_ci_sub_work = 0;
Char8 gcg_ci_root_dir[256];

void gcCiSetRdMode(void *obj, Sint32 a, Sint32 b, Sint32 mode)
{
	gcg_ci_rdmode = mode;
}
