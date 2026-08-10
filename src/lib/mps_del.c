#include "cri_xpt.h"

Uint32 MPS_CheckDelim(Uint8 *p)
{
	if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
		switch (p[3]) {
		case 0xB9:
			return 0x80000;
		case 0xBA:
			return 0x10000;
		case 0xBB:
			return 0x20000;
		default:
			if (p[3] >= 0xBC) {
				return 0x40000;
			}
		}
	}
	return 0;
}
