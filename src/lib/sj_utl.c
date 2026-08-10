/* CRI Stream Joint utilities: tagged-chunk search, chunk split */
#include "cri_xpt.h"
#include "sj.h"
#include <string.h>

#pragma explicit_zero_data on

/* zero word before the table (a static the linker left as zero) */
static Sint32 sj_utl_reserved = 0;

Sint32 sj_hexstr_to_val_tbl[112] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static Sint32 sj_HexStrToVal(Char8 *s, Sint32 n)
{
	Sint32 val;
	Sint32 i;

	val = 0;
	for (i = 0; i < n; i++) {
		val = (val << 4) + sj_hexstr_to_val_tbl[s[i]];
	}
	return val;
}

void *SJ_SearchTag(SJCK *inf, const Char8 *tag, const Char8 *name, SJCK *out)
{
	Uint8 *p;
	Uint8 *end;

	out->data = NULL;
	out->len = 0;
	p = inf->data;
	end = inf->data + inf->len;
	while (p < end) {
		if (strncmp((Char8 *)p, tag, 7) == 0) {
			out->data = p + 16;
			out->len = sj_HexStrToVal((Char8 *)p + 8, 7);
			break;
		}
		if (name != NULL && strncmp((Char8 *)p, name, 7) == 0) {
			return NULL;
		}
		p += sj_HexStrToVal((Char8 *)p + 8, 7) + 16;
	}
	if (p < end) {
		return p;
	}
	return NULL;
}

void SJ_SplitChunk(SJCK *src, Sint32 n, SJCK *a, SJCK *b)
{
	*a = *src;
	b->len = a->len;
	if (a->len > n) {
		a->len = n;
	}
	b->len -= a->len;
	if (b->len == 0) {
		b->data = NULL;
	} else {
		b->data = a->data + a->len;
	}
}
