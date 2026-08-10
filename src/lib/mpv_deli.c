/* MPEG video start-code (delimiter) search */
#include "cri_xpt.h"
#include "mpv.h"

Sint32 MPV_CheckDelim(void *ptr);

Sint8 *MPV_SearchDelim(Sint8 *p, Sint32 n, Sint32 mask)
{
	Sint8 *end;
	Sint8 c;
	Sint32 st;

	end = p + n;
	st = 0;
	while (p < end) {
		c = *p++;
		switch (st) {
		case 0:
			if (c == 0) {
				st = 1;
			}
			break;
		case 1:
			if (c == 0) {
				st = 2;
			} else {
				st = 0;
			}
			break;
		case 2:
			if (c == 1) {
				st = 3;
			} else if (c != 0) {
				st = 0;
			}
			break;
		case 3:
			if (MPV_CheckDelim(p - 4) & mask) {
				return p - 4;
			}
			st = 0;
			break;
		}
	}
	return NULL;
}

Sint8 *MPV_BsearchDelim(Sint8 *p, Sint32 n, Sint32 mask)
{
	Sint8 *end;
	Sint8 c;
	Sint32 st;

	end = p - n;
	st = 0;
	while (end < p) {
		c = *--p;
		switch (st) {
		case 0:
			st = 1;
			break;
		case 1:
			if (c == 1) {
				st = 2;
			}
			break;
		case 2:
			if (c == 0) {
				st = 3;
			} else if (c != 1) {
				st = 1;
			}
			break;
		case 3:
			if (c == 0) {
				if (MPV_CheckDelim(p) & mask) {
					return p;
				}
				st = 0;
			} else if (c != 1) {
				st = 1;
			} else {
				st = 2;
			}
			break;
		}
	}
	return NULL;
}

Sint32 MPV_CheckDelim(void *ptr)
{
	Uint8 *p = ptr;
	Sint32 code;
	Sint32 ret;

	code = (p[0] << 8) | p[1];
	code <<= 8;
	code |= p[2];
	code <<= 8;
	code |= p[3];
	if (code == 0x100) {
		ret = 0x04;
	} else if (code == 0x101) {
		ret = 0x03;
	} else if (code > 0x101 && code <= 0x1AF) {
		ret = 0x01;
	} else if (code == 0x1B2) {
		ret = 0x20;
	} else if (code == 0x1B3) {
		ret = 0x40;
	} else if (code == 0x1B5) {
		ret = 0x10;
	} else if (code == 0x1B7) {
		ret = 0x80;
	} else if (code == 0x1B8) {
		ret = 0x08;
	} else {
		ret = 0;
	}
	return ret;
}
