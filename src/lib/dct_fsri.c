/* CRI Sofdec MPEG video: 8x8 inverse DCT of the six blocks of a macroblock ("fsri": paired-single
 * separable IDCT with the row pass into the work buffer and the column pass written as 16-bit
 * pixels through GQR7). DC-only blocks are filled directly. The index helper interleaves two rows
 * of a block into one 16-entry row (the layout of the paired-single tables). */
#include "cri_xpt.h"
#include "mpv.h"

extern const Char8 *DCT_GetVerStr(void);
extern void DCT_AcInit(void);
extern void DCT_AcIdctDouble(Float64 *in, Float64 *out);
extern void *memset(void *dst, int c, Uint32 n);

/* DCT parameter block (MPV_OBJ + 0x78) */
typedef struct {
	Sint8 cbp[6];                   /* 0x00 block coded flags (0: DC only) */
	Uint8 pad06[0x18 - 0x06];
	Sint32 cnt0;                    /* 0x18 */
	Sint32 cnt1;                    /* 0x1C */
	Uint8 pad20[0x28 - 0x20];
	Sint32 cbp_msk;                 /* 0x28 coded block pattern, MSB first */
	Float32 *blk;                   /* 0x2C six 8x8 coefficient blocks */
	Sint16 **tbl;                   /* 0x30 output block pointers */
	Uint8 pad34[0x48 - 0x34];
	Uint8 *work;                    /* 0x48 row pass work buffer */
	Uint8 pad4C[0x54 - 0x4C];
} DCT_PA;

/* paired-single constants: C4 (x2), C6/C2 ... and the +-0.5 rounding pair for ps_sel */
Float32 B0TableOrg[12] = {
	1.4142135381698608f, 1.4142135381698608f, 2.613126039505005f, 2.613126039505005f,
	1.0823922157287598f, 1.0823922157287598f, 0.7653668522834778f, 0.7653668522834778f,
	0.5f, 0.5f, -0.5f, -0.5f,
};

Float32 PreIDCT[64][64];
Float64 sfsd_scale_tbl[64];
const Char8 *dctfsri_version_dummy;

static const Float64 scale8[8] = {
	0.3535533905932738, 0.4903926402016152, 0.46193976625564337, 0.4157348061512726,
	0.3535533905932738, 0.2777851165098011, 0.1913417161825449, 0.09754516100806414,
};

void DCT_FsriTransCore(DCT_PA *pa, Sint32 cbp)
{
	register Sint8 *flg = pa->cbp;
	register Float32 *blk;
	register Sint16 **tbl;
	register Uint8 *work;
	register Float32 *src;
	register Uint8 *dst;
	register Sint32 cnt;
	register Sint16 *o;
	register Float32 *p;
	/* the kernel's paired-single registers: the constants c1..c6 (loop-invariant) and the
	 * temporaries, all allocated by the compiler (no hard register in the asm: the C paths share
	 * f0/f7 and r5/r7 with them) */
	register __vec2x32float__ x0, c1, c2, c3, c4, c5, c6, x7, x8, x9, x10, x11, x12, x13, x31;
	Sint32 i;
	int k;
	Float32 dc;
	Sint32 n;
	Uint32 *out;

	work = pa->work;
	p = B0TableOrg;
	asm {
		psq_lu c1, 0x0(p), 0, 0
		psq_lu c2, 0x8(p), 0, 0
		psq_lu c3, 0x8(p), 0, 0
		psq_lu c4, 0x8(p), 0, 0
		psq_lu c5, 0x8(p), 0, 0
		psq_lu c6, 0x8(p), 0, 0
	}
	blk = pa->blk;
	tbl = pa->tbl;
	for (i = 0; i < 6; i++) {
		if (cbp < 0) {
			if (flg[0] == 0) {
				dc = blk[0];
				out = (Uint32 *)tbl[0];
				if (dc < 0.0) {
					n = (Sint16)(dc - 0.5f);
				} else {
					n = (Sint16)(0.5f + dc);
				}
				n = (n << 16) | (Uint16)n;
				out += 32;
				for (k = 0; k < 32; k++) {
					*--out = n;
				}
			} else {
				o = tbl[0];
				asm {
		subi src, blk, 0x8
		subi dst, work, 0x8
		li cnt, 0x4
L_80215114:
		psq_lu x0, 0x8(src), 0, 0
		psq_lu x7, 0x8(src), 0, 0
		psq_lu x8, 0x8(src), 0, 0
		psq_lu x9, 0x8(src), 0, 0
		psq_lu x10, 0x8(src), 0, 0
		psq_lu x11, 0x8(src), 0, 0
		psq_lu x12, 0x8(src), 0, 0
		psq_lu x13, 0x8(src), 0, 0
		ps_sub x31, x8, x12
		ps_add x12, x8, x12
		ps_sub x8, x0, x10
		ps_mul x31, x31, c1
		ps_add x10, x0, x10
		ps_sub x31, x31, x12
		ps_sub x0, x10, x12
		ps_add x12, x10, x12
		ps_sub x10, x8, x31
		ps_add x31, x8, x31
		ps_sub x8, x11, x9
		ps_add x9, x11, x9
		ps_sub x11, x7, x13
		ps_add x13, x7, x13
		ps_sub x7, x13, x9
		ps_add x9, x13, x9
		ps_sub x13, x8, x11
		ps_mul x11, x11, c3
		ps_mul x8, x8, c2
		ps_mul x13, x13, c4
		ps_mul x7, x7, c1
		ps_sub x11, x11, x13
		ps_sub x8, x8, x13
		ps_sub x13, x12, x9
		ps_sub x11, x11, x9
		ps_add x9, x12, x9
		ps_sub x7, x7, x11
		ps_sub x12, x31, x11
		ps_add x11, x31, x11
		ps_sub x8, x8, x7
		ps_sub x31, x0, x8
		ps_add x8, x0, x8
		ps_sub x0, x10, x7
		ps_add x7, x10, x7
		ps_merge00 x10, x9, x11
		psq_stu x10, 0x8(dst), 0, 0
		ps_merge00 x10, x7, x8
		psq_stu x10, 0x8(dst), 0, 0
		ps_merge00 x10, x31, x0
		psq_stu x10, 0x8(dst), 0, 0
		ps_merge00 x10, x12, x13
		psq_stu x10, 0x8(dst), 0, 0
		ps_merge11 x10, x9, x11
		psq_stu x10, 0x8(dst), 0, 0
		ps_merge11 x10, x7, x8
		psq_stu x10, 0x8(dst), 0, 0
		ps_merge11 x10, x31, x0
		psq_stu x10, 0x8(dst), 0, 0
		ps_merge11 x10, x12, x13
		subic. cnt, cnt, 0x1
		psq_stu x10, 0x8(dst), 0, 0
		bgt L_80215114
		mr dst, o
		mr src, work
		li cnt, 0x4
L_80215210:
		psq_lu x0, 0x0(src), 0, 0
		psq_lu x11, 0x20(src), 0, 0
		psq_lu x12, 0x20(src), 0, 0
		psq_lu x9, 0x20(src), 0, 0
		psq_lu x31, 0x20(src), 0, 0
		psq_lu x8, 0x20(src), 0, 0
		psq_lu x13, 0x20(src), 0, 0
		psq_lu x7, 0x20(src), 0, 0
		ps_sub x10, x12, x13
		ps_add x13, x12, x13
		ps_sub x12, x0, x31
		ps_mul x10, x10, c1
		ps_add x31, x0, x31
		ps_sub x10, x10, x13
		ps_sub x0, x31, x13
		ps_add x13, x31, x13
		ps_sub x31, x12, x10
		ps_add x10, x12, x10
		ps_sub x12, x8, x9
		ps_add x9, x8, x9
		ps_sub x8, x11, x7
		ps_add x7, x11, x7
		ps_sub x11, x7, x9
		ps_add x9, x7, x9
		ps_sub x7, x12, x8
		ps_mul x8, x8, c3
		ps_mul x12, x12, c2
		ps_mul x7, x7, c4
		ps_mul x11, x11, c1
		ps_sub x8, x8, x7
		ps_sub x12, x12, x7
		ps_sub x7, x13, x9
		ps_sub x8, x8, x9
		ps_add x9, x13, x9
		ps_sub x11, x11, x8
		ps_sub x13, x10, x8
		ps_add x8, x10, x8
		ps_sub x12, x12, x11
		ps_sub x10, x0, x12
		ps_add x12, x0, x12
		ps_sub x0, x31, x11
		ps_add x11, x31, x11
		ps_sel x31, x9, c5, c6
		ps_add x9, x9, x31
		ps_sel x31, x8, c5, c6
		ps_add x8, x8, x31
		ps_sel x31, x11, c5, c6
		ps_add x11, x11, x31
		psq_stu x9, 0x0(dst), 0, 7
		ps_sel x31, x12, c5, c6
		ps_add x12, x12, x31
		psq_stu x8, 0x10(dst), 0, 7
		ps_sel x31, x10, c5, c6
		psq_stu x11, 0x10(dst), 0, 7
		ps_add x10, x10, x31
		psq_stu x12, 0x10(dst), 0, 7
		ps_sel x31, x0, c5, c6
		ps_add x0, x0, x31
		ps_sel x31, x13, c5, c6
		psq_stu x10, 0x10(dst), 0, 7
		ps_add x13, x13, x31
		psq_stu x0, 0x10(dst), 0, 7
		ps_sel x31, x7, c5, c6
		psq_stu x13, 0x10(dst), 0, 7
		ps_add x7, x7, x31
		subic. cnt, cnt, 0x1
		psq_stu x7, 0x10(dst), 0, 7
		ble done
		subi src, src, 0xd8
		subi dst, dst, 0x6c
		b L_80215210
		done:
				}
			}
		}
		cbp <<= 1;
		blk += 64;
		tbl++;
		flg++;
	}
}

/* interleaved position of coefficient i: two 8-entry rows become one 16-entry row */
static inline int dctfsri_Idx(int i)
{
	int q = i / 8;
	int r = i % 8;
	int n;

	if (q % 2 == 0) {
		r *= 2;
	} else {
		q--;
		r = r * 2 + 1;
	}
	n = r + q * 8;
	if (n < 0 || n >= 256) {
		for (;;) {
		}
	}
	return n;
}

static inline void dctfsri_SetPreIdct(int n, int k, const Float64 *v)
{
	PreIDCT[dctfsri_Idx(n)][k] = (Float32)*v;
}

void DCT_FsriInitScanTbl(const Sint8 *seq, Sint8 *scan)
{
	int i;

	for (i = 0; i < 64; i++) {
		scan[i] = (Sint8)dctfsri_Idx(seq[i]);
	}
}

void DCT_FsriTransCbp(DCT_PA *pa)
{
	DCT_FsriTransCore(pa, pa->cbp_msk);
}

void DCT_FsriTrans6Blk(DCT_PA *pa)
{
	DCT_FsriTransCore(pa, -1);
}

void DCT_FsriSetGqr(void)
{
	asm {
		li r0, 0x7
		oris r0, r0, 0x7
		mtspr GQR7, r0
	}
}

/* the scan row is recomputed for every coefficient store */
#pragma opt_loop_invariants off
void initSparseTbl(void)
{
	Float64 in[64];
	Float64 out[64];
	int n;

	memset(PreIDCT, 0, sizeof(PreIDCT));
	DCT_AcInit();
	for (n = 0; n < 64; n++) {
		int k;
		for (k = 0; k < 64; k++) {
			if (k == n) {
				in[k] = 1.0 / sfsd_scale_tbl[k];
			} else {
				in[k] = 0.0;
			}
		}
		DCT_AcIdctDouble(in, out);
		for (k = 0; k < 64; k++) {
			dctfsri_SetPreIdct(n, k, &out[k]);
		}
	}
}
#pragma opt_loop_invariants on

void DCT_FsriInitPa(DCT_PA *pa)
{
	memset(pa, 0, sizeof(DCT_PA));
}

void DCT_FsriInitScaleTbl(Float32 *tbl)
{
	int i;

	for (i = 0; i < 64; i++) {
		tbl[dctfsri_Idx(i)] = (Float32)sfsd_scale_tbl[i];
	}
}

void DCT_FsriInit(void)
{
	Sint32 i;
	Sint32 j;

	dctfsri_version_dummy = DCT_GetVerStr();
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			sfsd_scale_tbl[i * 8 + j] = scale8[i] * scale8[j];
		}
	}
	initSparseTbl();
}
