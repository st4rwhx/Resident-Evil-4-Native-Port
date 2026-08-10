/* Sofdec colour format transforms, PowerPC version (cftyp422_ppc.c): the YCC 4:2:0 planar -> RGBA8
 * 4x4 tile conversion through paired singles, YCC 4:2:0 planar -> Y8 / CbCr 4:4 texture tiles and
 * the ARGB8888 float table generation (luminance alpha and the alpha ramps). (.bss in
 * first-reference order: gqr_save from the dynamic converter, the tables from the static one, the
 * version keep-alive from Init). */
#include "cri_xpt.h"

/* the version string is the first .rodata object; named so cnvStatic's asm can address the pool */
static const Char8 cft_version_str[] = "\nCRI CFT/GC Ver.1.57 Build:Sep 22 2004 10:34:37\n";
const Char8 *volatile CFT_version = cft_version_str;

/* table entry: A, R, G, B contribution of one component value (four Float32) */

/* the two chroma tables at tbl + 0x1000 (Cb) and tbl + 0x2000 (Cr) */
#define CFT_CB_TBL(tbl) ((Float32 *)((Uint8 *)(tbl) + 0x1000))
#define CFT_CR_TBL(tbl) ((Float32 *)((Uint8 *)(tbl) + 0x2000))
#define CFT_MAKE_CHROMA_TBL(tbl)                                                               \
	{                                                                                      \
		Sint32 i;                                                                      \
		for (i = 0; i < 256; i++) {                                                    \
			CFT_CB_TBL(tbl)[i * 4 + 3] = 2.017f * (Float32)(i - 128) + 0.5f;       \
			CFT_CB_TBL(tbl)[i * 4 + 2] = -0.392f * (Float32)(i - 128) + 0.5f;      \
			CFT_CB_TBL(tbl)[i * 4 + 1] = 0.0f;                                     \
			CFT_CB_TBL(tbl)[i * 4 + 0] = 0.0f;                                     \
			CFT_CR_TBL(tbl)[i * 4 + 3] = 0.0f;                                     \
			CFT_CR_TBL(tbl)[i * 4 + 2] = -0.813f * (Float32)(i - 128) + 0.5f;      \
			CFT_CR_TBL(tbl)[i * 4 + 1] = 1.596f * (Float32)(i - 128) + 0.5f;       \
			CFT_CR_TBL(tbl)[i * 4 + 0] = 0.0f;                                     \
		}                                                                              \
	}

/* clamp to the byte range (the arms' literals are pooled before the condition's) */
#define CFT_CLIP255(x) (((x) < 0.0f) ? 0.0f : (((x) > 255.0f) ? 255.0f : (x)))

void CFT_MakeArgb8888Alp3211Tbl(void *tbl, Uint8 a0, Uint8 a1, Uint8 a2)
{
	Float32 *y = (Float32 *)tbl;
	Float32 v;
	Sint32 i;

	CFT_MAKE_CHROMA_TBL(y);
	for (i = 0; i < 48; i++) {
		y[i * 4 + 3] = -16.0f * (255.0f / 219.0f) + 0.5f;
		y[i * 4 + 2] = -16.0f * (255.0f / 219.0f) + 0.5f;
		y[i * 4 + 1] = -16.0f * (255.0f / 219.0f) + 0.5f;
		y[i * 4 + 0] = (Float32)a0;
	}
	for (i = 48; i < 130; i++) {
		v = (255.0f / 55.0f) * CFT_CLIP255((Float32)i - 68.0f) + 0.5f;
		y[i * 4 + 3] = v;
		y[i * 4 + 2] = v;
		y[i * 4 + 1] = v;
		y[i * 4 + 0] = (Float32)a1;
	}
	for (i = 130; i < 256; i++) {
		v = (255.0f / 111.0f) * CFT_CLIP255(247.0f - (Float32)i) + 0.5f;
		y[i * 4 + 3] = v;
		y[i * 4 + 2] = v;
		y[i * 4 + 1] = v;
		y[i * 4 + 0] = (Float32)a2;
	}
}

void CFT_MakeArgb8888Alp3110Tbl(void *tbl, Uint8 a0, Uint8 a1, Uint8 a2)
{
	Float32 *y = (Float32 *)tbl;
	Float32 v;
	Sint32 i;

	CFT_MAKE_CHROMA_TBL(y);
	for (i = 0; i < 9; i++) {
		y[i * 4 + 3] = 0.0f;
		y[i * 4 + 2] = 0.0f;
		y[i * 4 + 1] = 0.0f;
		y[i * 4 + 0] = (Float32)a0;
	}
	for (i = 9; i < 134; i++) {
		v = (255.0f / 110.0f) * CFT_CLIP255((Float32)i - 16.0f) + 0.5f;
		y[i * 4 + 3] = v;
		y[i * 4 + 2] = v;
		y[i * 4 + 1] = v;
		y[i * 4 + 0] = (Float32)a1;
	}
	for (i = 134; i < 256; i++) {
		v = (255.0f / 110.0f) * CFT_CLIP255(251.0f - (Float32)i) + 0.5f;
		y[i * 4 + 3] = v;
		y[i * 4 + 2] = v;
		y[i * 4 + 1] = v;
		y[i * 4 + 0] = (Float32)a2;
	}
}

void CFT_MakeArgb8888AlpLumiTbl(Sint32 mode, Sint32 lo, Sint32 hi, void *tbl)
{
	Float32 *y = (Float32 *)tbl;
	Float32 *cb = (Float32 *)((Uint8 *)y + 0x1000);
	Float32 *cr = (Float32 *)((Uint8 *)y + 0x2000);
	Float32 f;
	Float32 v;
	Float32 scale;
	Sint32 range;
	Sint32 i;

	for (i = 0; i < 256; i++) {
		v = 1.164f * (Float32)(i - 16) + 0.5f;
		y[i * 4 + 3] = v;
		y[i * 4 + 2] = v;
		y[i * 4 + 1] = v;
		cb[i * 4 + 3] = 2.017f * (Float32)(i - 128) + 0.5f;
		cb[i * 4 + 2] = -0.392f * (Float32)(i - 128) + 0.5f;
		cb[i * 4 + 1] = 0.0f;
		cb[i * 4 + 0] = 0.0f;
		cr[i * 4 + 3] = 0.0f;
		cr[i * 4 + 2] = -0.813f * (Float32)(i - 128) + 0.5f;
		cr[i * 4 + 1] = 1.596f * (Float32)(i - 128) + 0.5f;
		cr[i * 4 + 0] = 0.0f;
	}
	range = hi - lo;
	scale = 255.0f / (Float32)range;
	if (mode == 1) {
		for (i = 0; i < 256; i++) {
			if (i < lo) {
				y[i * 4] = 255.0f;
			} else if (i > hi) {
				y[i * 4] = 0.0f;
			} else {
				y[i * 4] = scale * (Float32)(range - (i - lo));
			}
		}
	} else {
		for (i = 0; i < 256; i++) {
			if (i < lo) {
				y[i * 4] = 0.0f;
			} else if (i > hi) {
				y[i * 4] = 255.0f;
			} else {
				y[i * 4] = scale * (Float32)(i - lo);
			}
		}
	}
}

/* planar source */
typedef struct {
	Uint8 *y;                  /* 0x00 */
	Uint8 *cb;                 /* 0x04 */
	Uint8 *cr;                 /* 0x08 */
	Sint32 ywidth;             /* 0x0C */
	Sint32 cbwidth;            /* 0x10 */
	Sint32 crwidth;            /* 0x14 */
} CFT_YCC420PLN;

/* RGBA8 tile destination */
typedef struct {
	Uint8 *buf;                /* 0x00 */
	Sint32 width;              /* 0x04 */
	Sint32 height;             /* 0x08 */
	Sint32 pitch;              /* 0x0C */
} CFT_ARGBDST;

/* one row of a CbCr 4x4 IA8 tile (Cb, Cr byte pairs, two words) */
#define CFTYP_C44_ROW(c, cb, cr)                                                              \
	crv = *(cr)++;                                                                         \
	cbv = *(cb)++;                                                                         \
	*++(c) = (cbv & 0xFF000000) | ((crv >> 8) & 0x00FF0000) | ((cbv >> 8) & 0xFF00) |      \
		 ((crv >> 16) & 0xFF);                                                         \
	*++(c) = ((cbv << 16) & 0xFF000000) | ((crv << 8) & 0x00FF0000) | ((cbv << 8) & 0xFF00) | \
		 (crv & 0xFF)

/* Y plane as 8x4 I8 tiles (four 8-byte rows per tile), CbCr as 4x4 IA8 tiles. The dcbz/dcbt
 * index is a register variable (ofs = 8 / 4) named as the asm operand: a hard r0 in the asm would
 * keep r0 reserved over the whole function (the original has r0 free between the two loops and the
 * index coloured r0 as a plain high-degree node); the __dcbz intrinsic hoists its literal only one
 * loop level. `const src`: the parameter loads get their own alias class and are scheduled above
 * the frame stores. The four signed divisions of the setup are chained through the carry bit
 * (srawi writes CA, addze reads it: a latency edge in statement order), so their statement order
 * yskip, cnt, hblk, dskip IS the schedule; with hblk not ready at cycle 0 the `li ofs, 8` issues
 * first and ofs (r0) is live across the setup temps. ywidth/yw3 are reused for the chroma loop
 * (cbwidth / 4 and its triple) and declared LAST: one node each, live across both loops, degree
 * >= 29 at the first removal scan, coloured r9/r10 before the unroll remainder copy (r11). */
void CFT_Ycc420plnToY84C44(const CFT_YCC420PLN *src, void *ybuf, void *cbuf, Sint32 width, Sint32 height)
{
	register Sint32 ofs;
	Sint32 n;
	Float64 *y3;
	Sint32 yskip;
	Sint32 dskip;
	Float64 *y2;
	Float64 *y1;
	register Float64 *d;
	register Float64 *y0;
	Sint32 yw2;
	Sint32 hblk;
	Sint32 cnt;
	Sint32 i;
	register Float64 w0;
	register Float64 w1;
	register Float64 w2;
	register Float64 w3;
	Uint32 *cbp3;
	Uint32 *cbp2;
	Uint32 *cbp1;
	Uint32 *crp3;
	Uint32 *crp2;
	Uint32 *crp1;
	Sint32 o3;
	Sint32 o2;
	Sint32 o1;
	Sint32 cskip;
	Uint32 *cbp0;
	Uint32 *crp0;
	Uint32 crv;
	Uint32 cbv;
	register Uint32 *c;
	Sint32 ywidth;
	Sint32 yw3;

	ywidth = src->ywidth;
	yw2 = ywidth * 2;
	y0 = (Float64 *)src->y;
	d = (Float64 *)ybuf - 1;
	yw3 = ywidth * 3;
	yskip = yw3 / 8 * 8;
	cnt = ywidth / 8;
	hblk = height / 4;
	dskip = (width - ywidth) / 8 * 32;
	ofs = 8;
	for (i = 0; i < hblk; i++) {
		y1 = (Float64 *)(ywidth + (Uint32)y0);
		y2 = (Float64 *)(yw2 + (Uint32)y0);
		y3 = (Float64 *)(yw3 + (Uint32)y0);
		n = cnt;
		while (n-- > 0) {
			asm { dcbz d, ofs }
			w0 = *y0;
			w1 = *y1;
			w2 = *y2;
			w3 = *y3;
			asm { dcbt y0, ofs }
			asm { stfdu w0, 8(d) }
			asm { stfdu w1, 8(d) }
			asm { stfdu w2, 8(d) }
			asm { stfdu w3, 8(d) }
			y0++;
			y3++;
			y2++;
			y1++;
		}
		d = (Float64 *)((Uint8 *)d + dskip);
		y0 = (Float64 *)((Uint8 *)y0 + yskip);
	}

	cnt = src->ywidth / 2 / 4;
	cskip = (src->cbwidth - src->ywidth / 2) / 4;
	hblk = height / 2 / 4;
	ywidth = src->cbwidth / 4;
	yw3 = ywidth * 3;
	o1 = ywidth * 4;
	o2 = ywidth * 8;
	o3 = yw3 * 4;
	c = (Uint32 *)cbuf - 1;
	cbp0 = (Uint32 *)src->cb;
	crp0 = (Uint32 *)src->cr;
	ofs = 4;
	for (i = 0; i < hblk; i++) {
		crp1 = (Uint32 *)(o1 + (Uint32)crp0);
		crp2 = (Uint32 *)(o2 + (Uint32)crp0);
		crp3 = (Uint32 *)(o3 + (Uint32)crp0);
		cbp1 = (Uint32 *)(o1 + (Uint32)cbp0);
		cbp2 = (Uint32 *)(o2 + (Uint32)cbp0);
		cbp3 = (Uint32 *)(o3 + (Uint32)cbp0);
		for (n = 0; n < cnt; n++) {
			asm { dcbz c, ofs }
			CFTYP_C44_ROW(c, cbp0, crp0);
			CFTYP_C44_ROW(c, cbp1, crp1);
			CFTYP_C44_ROW(c, cbp2, crp2);
			CFTYP_C44_ROW(c, cbp3, crp3);
		}
		c += cskip * 8;
		cbp0 += yw3 + cskip;
		crp0 += yw3 + cskip;
	}
}

/* paired-single conversion state: GQR4 = u8 loads/stores, the static tables (the original's .bss is
 * in first-reference order: gqr_save from the dynamic converter, then cr_r, cr_g, cb_b, cb_g, y from
 * the static one and CFT_dummy from Init; with the converter bodies as asm ours places CFT_dummy first) */
Uint32 gqr_save;
Float32 cr_r[256];
Float32 cr_g[256];
Float32 cb_b[256];
static Float32 cb_g[256];
Float32 y[256] __attribute__((aligned(32)));
const Char8 *CFT_dummy;

#pragma scheduling off
#pragma peephole off

/* The two tile converters are compiler-generated paired-single code in the original (ps_merge/ps_add
 * of the table entries, psq_st through GQR4 as bytes: rows of AR then GB pairs of a 4x4 RGBA8 tile);
 * CodeWarrior 2.4.7 has no paired-single intrinsics, so the bodies are kept as asm. */
void cnvDynamicYcc420plnToArgb8888(CFT_YCC420PLN *src, CFT_ARGBDST *dst, Float32 *tbl)
{
	Uint32 slot[8];

	asm {
		addi r12, r5, 0x1000
		addi r31, r5, 0x2000
		mfspr r0, GQR4
		li r7, 0x4
		lis r6, gqr_save@ha
		oris r7, r7, 0x4
		stw r0, gqr_save@l(r6)
		mtspr GQR4, r7
		lwz r16, 0xc(r3)
		lwz r7, 0x8(r4)
		li r0, 0x0
		srawi r8, r16, 2
		lwz r6, 0xc(r4)
		lwz r17, 0x0(r4)
		addze r4, r8
		stw r4, slot[1]
		srawi r4, r7, 2
		addze r4, r4
		lwz r15, 0x10(r3)
		stw r4, slot[0]
		subf r4, r16, r6
		srawi r4, r4, 2
		li r7, 0x0
		addze r9, r4
		lwz r10, 0x4(r3)
		lwz r11, 0x8(r3)
		srwi r8, r16, 2
		lwz r6, 0x0(r3)
		mr r3, r17
		stw r7, slot[2]
		srwi r15, r15, 1
		addi r4, r17, 0x20
		slwi r9, r9, 4
		mulli r7, r8, 0xc
		slwi r26, r8, 2
		slwi r27, r8, 3
		stw r7, slot[4]
		slwi r7, r15, 1
		stw r7, slot[3]
		slwi r7, r15, 2
		stw r7, slot[5]
		slwi r7, r9, 2
		stw r7, slot[6]
		b L_801FC090
L_801FBC88:
		lwz r9, slot[3]
		mr r7, r10
		mr r8, r11
		add r29, r9, r11
		add r30, r9, r10
		lwz r9, slot[1]
		mtctr r9
		cmpwi r9, 0x0
		ble L_801FC064
L_801FBCAC:
		lwz r16, 0x0(r6)
		lhz r9, 0x0(r7)
		lhz r22, 0x0(r8)
		dcbz r3, r0
		dcbz r4, r0
		rlwinm r19, r16, 20, 20, 27
		clrlslwi r25, r16, 24, 4
		rlwinm r17, r22, 28, 20, 27
		add r17, r31, r17
		rlwinm r15, r16, 12, 20, 27
		rlwinm r21, r16, 28, 20, 27
		add r16, r5, r15
		add r20, r5, r19
		clrlslwi r22, r22, 24, 4
		add r23, r31, r22
		rlwinm r18, r9, 28, 20, 27
		add r18, r12, r18
		clrlslwi r22, r9, 24, 4
		add r24, r12, r22
		add r28, r5, r25
		add r22, r5, r21
		lfs f6, 0x4(r16)
		lfs f7, 0x4(r20)
		lfs f12, 0x4(r17)
		ps_merge11 f1, f6, f7
		lfs f10, 0xc(r18)
		lfs f31, 0x8(r17)
		lfs f30, 0x8(r18)
		lfs f8, 0x4(r22)
		ps_add f27, f1, f12
		lfs f9, 0x4(r28)
		ps_add f31, f31, f30
		lfsx f0, r5, r15
		ps_add f23, f1, f10
		lfs f29, 0x8(r23)
		lfs f28, 0x8(r24)
		ps_add f25, f1, f31
		ps_merge11 f2, f8, f9
		lfs f13, 0x4(r23)
		ps_add f29, f29, f28
		lfs f11, 0xc(r24)
		ps_merge00 f19, f25, f23
		lfsx f3, r5, r19
		ps_merge10 f21, f0, f27
		lfsx f4, r5, r21
		ps_add f26, f2, f13
		lfsx f5, r5, r25
		ps_add f22, f2, f11
		ps_add f24, f2, f29
		ps_merge10 f20, f4, f26
		ps_merge11 f27, f3, f27
		ps_merge00 f18, f24, f22
		ps_merge11 f26, f5, f26
		ps_merge11 f23, f25, f23
		ps_merge11 f22, f24, f22
		psq_st f21, 0x0(r3), 0, 4
		psq_st f27, 0x2(r3), 0, 4
		psq_st f20, 0x4(r3), 0, 4
		psq_st f26, 0x6(r3), 0, 4
		psq_st f19, 0x0(r4), 0, 4
		psq_st f23, 0x2(r4), 0, 4
		psq_st f18, 0x4(r4), 0, 4
		psq_st f22, 0x6(r4), 0, 4
		extrwi r15, r9, 8, 16
		lfs f31, 0x8(r17)
		lwzx r16, r6, r26
		rlwinm r17, r16, 12, 20, 27
		add r9, r5, r17
		lfsx f0, r5, r17
		lfs f6, 0x4(r9)
		extrwi r9, r16, 8, 8
		slwi r18, r9, 4
		slwi r19, r15, 4
		add r9, r5, r18
		rlwinm r17, r16, 28, 20, 27
		lfs f7, 0x4(r9)
		clrlslwi r15, r16, 24, 4
		add r19, r12, r19
		add r16, r5, r17
		ps_merge11 f1, f6, f7
		add r9, r5, r15
		lfs f30, 0x8(r19)
		lfs f29, 0x8(r23)
		ps_add f31, f31, f30
		lfs f8, 0x4(r16)
		lfs f9, 0x4(r9)
		ps_add f27, f1, f12
		ps_add f29, f29, f28
		lfsx f3, r5, r18
		ps_merge11 f2, f8, f9
		lfsx f4, r5, r17
		ps_merge10 f21, f0, f27
		lfsx f5, r5, r15
		ps_add f25, f1, f31
		ps_add f26, f2, f13
		ps_add f23, f1, f10
		ps_add f24, f2, f29
		ps_add f22, f2, f11
		ps_merge10 f20, f4, f26
		ps_merge00 f19, f25, f23
		ps_merge00 f18, f24, f22
		ps_merge11 f27, f3, f27
		ps_merge11 f26, f5, f26
		ps_merge11 f23, f25, f23
		ps_merge11 f22, f24, f22
		psq_st f21, 0x8(r3), 0, 4
		psq_st f27, 0xa(r3), 0, 4
		psq_st f20, 0xc(r3), 0, 4
		psq_st f26, 0xe(r3), 0, 4
		psq_st f19, 0x8(r4), 0, 4
		psq_st f23, 0xa(r4), 0, 4
		psq_st f18, 0xc(r4), 0, 4
		psq_st f22, 0xe(r4), 0, 4
		add r6, r6, r27
		lwz r16, 0x0(r6)
		lhz r9, 0x0(r30)
		rlwinm r24, r16, 12, 20, 27
		rlwinm r20, r16, 20, 20, 27
		lhz r22, 0x0(r29)
		rlwinm r18, r16, 28, 20, 27
		rlwinm r15, r9, 28, 20, 27
		add r23, r5, r24
		add r21, r12, r15
		rlwinm r17, r22, 28, 20, 27
		add r28, r31, r17
		add r19, r5, r20
		add r17, r5, r18
		lfs f6, 0x4(r23)
		lfsx f0, r5, r24
		clrlwi r15, r9, 24
		lfs f12, 0x4(r28)
		clrlwi r23, r22, 24
		lfs f10, 0xc(r21)
		lfs f31, 0x8(r28)
		lfs f30, 0x8(r21)
		lfs f7, 0x4(r19)
		lfsx f3, r5, r20
		lfs f8, 0x4(r17)
		lfsx f4, r5, r18
		slwi r17, r23, 4
		ps_merge11 f1, f6, f7
		add r18, r31, r17
		clrlslwi r16, r16, 24, 4
		slwi r17, r15, 4
		ps_add f31, f31, f30
		add r17, r12, r17
		add r15, r5, r16
		lfs f29, 0x8(r18)
		lfs f9, 0x4(r15)
		ps_add f27, f1, f12
		lfs f28, 0x8(r17)
		ps_add f25, f1, f31
		ps_merge11 f2, f8, f9
		lfs f13, 0x4(r18)
		ps_add f29, f29, f28
		lfs f11, 0xc(r17)
		ps_merge10 f21, f0, f27
		ps_add f26, f2, f13
		ps_add f23, f1, f10
		lfsx f5, r5, r16
		ps_add f24, f2, f29
		ps_add f22, f2, f11
		ps_merge10 f20, f4, f26
		ps_merge00 f19, f25, f23
		ps_merge00 f18, f24, f22
		ps_merge11 f27, f3, f27
		ps_merge11 f26, f5, f26
		ps_merge11 f23, f25, f23
		ps_merge11 f22, f24, f22
		psq_st f21, 0x10(r3), 0, 4
		psq_st f27, 0x12(r3), 0, 4
		psq_st f20, 0x14(r3), 0, 4
		psq_st f26, 0x16(r3), 0, 4
		psq_st f19, 0x10(r4), 0, 4
		psq_st f23, 0x12(r4), 0, 4
		psq_st f18, 0x14(r4), 0, 4
		psq_st f22, 0x16(r4), 0, 4
		clrlslwi r15, r22, 24, 4
		add r18, r31, r15
		clrlslwi r9, r9, 24, 4
		lwzx r16, r6, r26
		add r17, r12, r9
		lfs f31, 0x8(r28)
		rlwinm r23, r16, 12, 20, 27
		rlwinm r21, r16, 20, 20, 27
		clrlslwi r15, r16, 24, 4
		rlwinm r19, r16, 28, 20, 27
		add r22, r5, r23
		add r20, r5, r21
		add r16, r5, r19
		add r9, r5, r15
		lfs f6, 0x4(r22)
		ps_add f31, f31, f30
		lfs f7, 0x4(r20)
		lfs f8, 0x4(r16)
		ps_merge11 f1, f6, f7
		lfs f9, 0x4(r9)
		lfs f29, 0x8(r18)
		lfs f28, 0x8(r17)
		ps_merge11 f2, f8, f9
		lfs f13, 0x4(r18)
		ps_add f29, f29, f28
		lfs f11, 0xc(r17)
		ps_add f27, f1, f12
		lfsx f0, r5, r23
		lfsx f3, r5, r21
		ps_add f26, f2, f13
		ps_merge10 f21, f0, f27
		lfsx f4, r5, r19
		ps_add f25, f1, f31
		lfsx f5, r5, r15
		ps_add f23, f1, f10
		ps_merge10 f20, f4, f26
		ps_add f24, f2, f29
		ps_add f22, f2, f11
		ps_merge00 f19, f25, f23
		ps_merge11 f27, f3, f27
		ps_merge00 f18, f24, f22
		ps_merge11 f26, f5, f26
		ps_merge11 f23, f25, f23
		ps_merge11 f22, f24, f22
		psq_st f21, 0x18(r3), 0, 4
		psq_st f27, 0x1a(r3), 0, 4
		psq_st f20, 0x1c(r3), 0, 4
		psq_st f26, 0x1e(r3), 0, 4
		psq_st f19, 0x18(r4), 0, 4
		psq_st f23, 0x1a(r4), 0, 4
		psq_st f18, 0x1c(r4), 0, 4
		psq_st f22, 0x1e(r4), 0, 4
		addi r6, r6, 0x4
		addi r4, r4, 0x40
		subf r6, r27, r6
		addi r7, r7, 0x2
		addi r30, r30, 0x2
		addi r8, r8, 0x2
		addi r29, r29, 0x2
		addi r3, r3, 0x40
		bdnz L_801FBCAC
L_801FC064:
		lwz r7, slot[4]
		add r6, r6, r7
		lwz r7, slot[5]
		add r10, r10, r7
		add r11, r11, r7
		lwz r7, slot[6]
		add r3, r3, r7
		add r4, r4, r7
		lwz r7, slot[2]
		addi r7, r7, 0x1
		stw r7, slot[2]
L_801FC090:
		lwz r8, slot[2]
		lwz r7, slot[0]
		cmpw r8, r7
		blt L_801FBC88
		lis r3, gqr_save@ha
		lwz r0, gqr_save@l(r3)
		mtspr GQR4, r0
	}
}

/* COMPILER-DIFF: .bss first-reference order. The original's cnvStatic is C and addresses the five
 * tables through its gqr_save-based .bss pool (immediates, no relocations), which puts them in .bss
 * before Init's CFT_dummy; our asm transcription carries the immediates, so this never-called
 * function (dropped by strip_unused.py) supplies the references in the target's order. */
void cftyp_bss_order(Float32 *p)
{
	cr_r[0] = p[0];
	cr_g[0] = p[1];
	cb_b[0] = p[2];
	cb_g[0] = p[3];
	y[0] = p[4];
}

/* COMPILER-DIFF: the original (compiler-generated paired-single C) loads the unit's pooled 255.0f
 * literal, `lis r5, @494@ha; lfs f21, @494@l(r5)` = .rodata+0x50 (the table makers' clip constant).
 * Inline asm cannot name a compiler literal, so the asm addresses it relative to the version string,
 * the named object at .rodata+0 (`cft_version_str + 0x50`); the relocation resolves to the same
 * address and no extra .rodata object is emitted. */

void cnvStaticYcc420plnToArgb8888(CFT_YCC420PLN *src, CFT_ARGBDST *dst)
{
	Uint32 slot[8];

	asm {
		lis r6, gqr_save@ha
		lis r5, cft_version_str + 0x50@ha
		addi r0, r6, gqr_save@l
		lfs f21, cft_version_str + 0x50@l(r5)
		stw r0, slot[2]
		mfspr r0, GQR4
		li r6, 0x4
		lwz r5, slot[2]
		oris r6, r6, 0x4
		stw r0, 0x0(r5)
		mtspr GQR4, r6
		lwz r7, 0xc(r3)
		li r29, 0x0
		lwz r6, 0x8(r4)
		li r22, 0x0
		srawi r0, r7, 2
		lwz r5, 0xc(r4)
		addze r0, r0
		lwz r9, 0x0(r4)
		stw r0, slot[1]
		srawi r0, r6, 2
		addze r0, r0
		subf r4, r7, r5
		stw r0, slot[0]
		srawi r0, r4, 2
		lwz r4, 0x10(r3)
		addze r0, r0
		lwz r21, 0x4(r3)
		mr r31, r9
		lwz r20, 0x8(r3)
		srwi r8, r7, 2
		lwz r28, 0x0(r3)
		srwi r10, r4, 1
		addi r30, r9, 0x20
		slwi r12, r0, 4
		lwz r3, slot[2]
		mulli r0, r8, 0xc
		slwi r4, r8, 2
		addi r11, r3, 0x1020
		addi r9, r3, 0x4
		addi r7, r3, 0x804
		stw r0, slot[3]
		addi r6, r3, 0x404
		addi r5, r3, 0xc04
		slwi r3, r8, 3
		slwi r8, r10, 2
		slwi r0, r10, 1
		stw r8, slot[4]
		slwi r8, r12, 2
		stw r8, slot[5]
		b L_801FC5B4
L_801FC254:
		lwz r8, slot[1]
		mr r27, r21
		mr r26, r20
		add r18, r0, r20
		add r19, r0, r21
		mtctr r8
		cmpwi r8, 0x0
		ble L_801FC590
L_801FC274:
		lwz r25, 0x0(r28)
		lhz r24, 0x0(r27)
		lhz r23, 0x0(r26)
		dcbz r31, r29
		dcbz r30, r29
		rlwinm r12, r25, 10, 22, 29
		rlwinm r10, r23, 26, 22, 29
		rlwinm r8, r24, 26, 22, 29
		clrlslwi r23, r23, 24, 2
		clrlslwi r17, r24, 24, 2
		rlwinm r15, r25, 18, 22, 29
		rlwinm r16, r25, 26, 22, 29
		clrlslwi r25, r25, 24, 2
		lfsx f0, r11, r12
		lfsx f3, r11, r15
		lfsx f8, r9, r10
		ps_merge11 f1, f0, f3
		lfsx f6, r7, r8
		lfsx f10, r6, r10
		lfsx f11, r5, r8
		ps_add f31, f1, f8
		lfsx f4, r11, r16
		lfsx f5, r11, r25
		ps_add f10, f10, f11
		lfsx f12, r6, r23
		ps_add f27, f1, f6
		lfsx f13, r5, r17
		ps_add f29, f1, f10
		ps_merge10 f25, f21, f31
		lfsx f9, r9, r23
		ps_merge11 f2, f4, f5
		lfsx f7, r7, r17
		ps_add f12, f12, f13
		ps_merge00 f23, f29, f27
		ps_add f30, f2, f9
		ps_add f28, f2, f12
		ps_add f26, f2, f7
		ps_merge10 f24, f21, f30
		ps_merge11 f31, f21, f31
		ps_merge00 f22, f28, f26
		ps_merge11 f30, f21, f30
		ps_merge11 f27, f29, f27
		ps_merge11 f26, f28, f26
		psq_st f25, 0x0(r31), 0, 4
		psq_st f31, 0x2(r31), 0, 4
		psq_st f24, 0x4(r31), 0, 4
		psq_st f30, 0x6(r31), 0, 4
		psq_st f23, 0x0(r30), 0, 4
		psq_st f27, 0x2(r30), 0, 4
		psq_st f22, 0x4(r30), 0, 4
		psq_st f26, 0x6(r30), 0, 4
		clrlwi r16, r24, 24
		lwzx r25, r28, r4
		lfsx f8, r9, r10
		rlwinm r15, r25, 10, 22, 29
		rlwinm r12, r25, 18, 22, 29
		lfsx f10, r6, r10
		extrwi r10, r25, 8, 16
		lfsx f0, r11, r15
		lfsx f6, r7, r8
		lfsx f11, r5, r8
		lfsx f3, r11, r12
		ps_merge11 f1, f0, f3
		slwi r12, r10, 2
		slwi r10, r16, 2
		clrlslwi r8, r25, 24, 2
		lfsx f4, r11, r12
		ps_add f10, f10, f11
		lfsx f5, r11, r8
		ps_add f31, f1, f8
		lfsx f12, r6, r23
		ps_add f29, f1, f10
		lfsx f13, r5, r10
		ps_add f27, f1, f6
		ps_merge10 f25, f21, f31
		ps_merge11 f2, f4, f5
		lfsx f9, r9, r23
		ps_add f12, f12, f13
		lfsx f7, r7, r10
		ps_merge00 f23, f29, f27
		ps_add f30, f2, f9
		ps_add f28, f2, f12
		ps_add f26, f2, f7
		ps_merge10 f24, f21, f30
		ps_merge11 f31, f21, f31
		ps_merge00 f22, f28, f26
		ps_merge11 f30, f21, f30
		ps_merge11 f27, f29, f27
		ps_merge11 f26, f28, f26
		psq_st f25, 0x8(r31), 0, 4
		psq_st f31, 0xa(r31), 0, 4
		psq_st f24, 0xc(r31), 0, 4
		psq_st f30, 0xe(r31), 0, 4
		psq_st f23, 0x8(r30), 0, 4
		psq_st f27, 0xa(r30), 0, 4
		psq_st f22, 0xc(r30), 0, 4
		psq_st f26, 0xe(r30), 0, 4
		add r28, r28, r3
		lwz r25, 0x0(r28)
		lhz r24, 0x0(r19)
		lhz r23, 0x0(r18)
		rlwinm r17, r25, 10, 22, 29
		rlwinm r12, r24, 26, 22, 29
		clrlslwi r8, r24, 24, 2
		rlwinm r24, r23, 26, 22, 29
		clrlslwi r10, r23, 24, 2
		rlwinm r23, r25, 18, 22, 29
		rlwinm r16, r25, 26, 22, 29
		clrlslwi r15, r25, 24, 2
		lfsx f0, r11, r17
		lfsx f3, r11, r23
		lfsx f8, r9, r24
		ps_merge11 f1, f0, f3
		lfsx f6, r7, r12
		lfsx f10, r6, r24
		lfsx f11, r5, r12
		ps_add f31, f1, f8
		lfsx f4, r11, r16
		lfsx f5, r11, r15
		ps_add f10, f10, f11
		lfsx f12, r6, r10
		ps_add f27, f1, f6
		lfsx f13, r5, r8
		ps_add f29, f1, f10
		ps_merge10 f25, f21, f31
		lfsx f9, r9, r10
		ps_merge11 f2, f4, f5
		lfsx f7, r7, r8
		ps_add f12, f12, f13
		ps_merge00 f23, f29, f27
		ps_add f30, f2, f9
		ps_add f28, f2, f12
		ps_add f26, f2, f7
		ps_merge10 f24, f21, f30
		ps_merge11 f31, f21, f31
		ps_merge00 f22, f28, f26
		ps_merge11 f30, f21, f30
		ps_merge11 f27, f29, f27
		ps_merge11 f26, f28, f26
		psq_st f25, 0x10(r31), 0, 4
		psq_st f31, 0x12(r31), 0, 4
		psq_st f24, 0x14(r31), 0, 4
		psq_st f30, 0x16(r31), 0, 4
		psq_st f23, 0x10(r30), 0, 4
		psq_st f27, 0x12(r30), 0, 4
		psq_st f22, 0x14(r30), 0, 4
		psq_st f26, 0x16(r30), 0, 4
		lwzx r25, r28, r4
		lfsx f8, r9, r24
		rlwinm r16, r25, 10, 22, 29
		rlwinm r15, r25, 18, 22, 29
		lfsx f0, r11, r16
		rlwinm r16, r25, 26, 22, 29
		lfsx f3, r11, r15
		clrlslwi r15, r25, 24, 2
		lfsx f10, r6, r24
		ps_merge11 f1, f0, f3
		lfsx f11, r5, r12
		lfsx f4, r11, r16
		lfsx f5, r11, r15
		ps_add f10, f10, f11
		ps_add f31, f1, f8
		lfsx f6, r7, r12
		ps_merge11 f2, f4, f5
		lfsx f9, r9, r10
		ps_add f29, f1, f10
		lfsx f7, r7, r8
		ps_add f27, f1, f6
		lfsx f12, r6, r10
		lfsx f13, r5, r8
		ps_add f30, f2, f9
		ps_merge10 f25, f21, f31
		ps_add f12, f12, f13
		ps_merge10 f24, f21, f30
		ps_merge00 f23, f29, f27
		ps_add f28, f2, f12
		ps_add f26, f2, f7
		ps_merge11 f31, f21, f31
		ps_merge11 f30, f21, f30
		ps_merge00 f22, f28, f26
		ps_merge11 f27, f29, f27
		ps_merge11 f26, f28, f26
		psq_st f25, 0x18(r31), 0, 4
		psq_st f31, 0x1a(r31), 0, 4
		psq_st f24, 0x1c(r31), 0, 4
		psq_st f30, 0x1e(r31), 0, 4
		psq_st f23, 0x18(r30), 0, 4
		psq_st f27, 0x1a(r30), 0, 4
		psq_st f22, 0x1c(r30), 0, 4
		psq_st f26, 0x1e(r30), 0, 4
		addi r28, r28, 0x4
		subf r28, r3, r28
		addi r31, r31, 0x40
		addi r30, r30, 0x40
		addi r27, r27, 0x2
		addi r19, r19, 0x2
		addi r26, r26, 0x2
		addi r18, r18, 0x2
		bdnz L_801FC274
L_801FC590:
		lwz r8, slot[3]
		addi r22, r22, 0x1
		add r28, r28, r8
		lwz r8, slot[4]
		add r21, r21, r8
		add r20, r20, r8
		lwz r8, slot[5]
		add r31, r31, r8
		add r30, r30, r8
L_801FC5B4:
		lwz r8, slot[0]
		cmpw r22, r8
		blt L_801FC254
		lwz r3, slot[2]
		lwz r0, 0x0(r3)
		mtspr GQR4, r0
	}
}

#pragma peephole on
#pragma scheduling on

void CFT_Ycc420plnToArgb8888(CFT_YCC420PLN *src, CFT_ARGBDST *dst, Float32 *tbl)
{
	if (tbl == NULL) {
		cnvStaticYcc420plnToArgb8888(src, dst);
	} else {
		cnvDynamicYcc420plnToArgb8888(src, dst, tbl);
	}
}

void CFT_Ycc420plnToArgb8888Init(void)
{
	Sint32 i;
	Float32 *py = y;
	Float32 *pcb_g = cb_g;
	Float32 *pcb_b = cb_b;
	Float32 *pcr_r = cr_r;
	Float32 *pcr_g = cr_g;
	Float32 v;
	Sint32 k;

	CFT_dummy = CFT_version;
	for (i = 0; i != 256; i++) {
		/* the y product is a variable node (coloured after the hoisted literal loads): the
		 * redefinition of k between v's definition and its store blocks the frontend's
		 * single-use substitution */
		k = i - 16;
		v = 1.164f * (Float32)k;
		k = i - 128;
		*py++ = v;
		*pcb_g++ = -0.392f * (Float32)k;
		*pcb_b++ = 2.017f * (Float32)k;
		*pcr_r++ = 1.596f * (Float32)k;
		*pcr_g++ = -0.813f * (Float32)k;
	}
}

