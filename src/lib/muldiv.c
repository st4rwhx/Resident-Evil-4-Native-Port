#include "cri_xpt.h"

Sint32 UTY_MulDiv(Sint32 a, Sint32 b, Sint32 c)
{
	if (c == 0) {
		if ((a ^ b) >= 0) {
			return 0x7FFFFFFF;
		}
		return (Sint32)0x80000000;
	}
	return (Sint32)(((Sint64)a * (Sint64)b) / c);
}
