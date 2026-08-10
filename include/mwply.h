#ifndef MWPLY_H
#define MWPLY_H

// CRI Sofdec movie player (MWPLY) API as used by game/sofdec.cpp. A handle is a pointer to an
// object whose first word is its interface table; the mwPly* one-liners below are the CRI macros.

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MWPLY_OBJ* MWPLY;

typedef struct {
    void* (*QueryInterface)(MWPLY hn, void* iid);  // 0x00
    u32 (*AddRef)(MWPLY hn);                       // 0x04
    u32 (*Release)(MWPLY hn);                      // 0x08
    void (*x0C)(MWPLY hn);                         // 0x0C
    void (*x10)(MWPLY hn);                         // 0x10
    void (*Destroy)(MWPLY hn);                     // 0x14
    void (*StartFname)(MWPLY hn, const char* fname); // 0x18
    void (*Stop)(MWPLY hn);                        // 0x1C
    int (*GetStat)(MWPLY hn);                      // 0x20
    void (*GetTime)(MWPLY hn, int* ncount, int* tscale); // 0x24
    void (*Pause)(MWPLY hn, int sw);               // 0x28
} MWPLY_IF;

struct MWPLY_OBJ {
    MWPLY_IF* vtbl;
};

#define mwPlyDestroy(hn) (*(hn)->vtbl->Destroy)(hn)
#define mwPlyStartFname(hn, fname) (*(hn)->vtbl->StartFname)(hn, fname)
#define mwPlyGetStat(hn) (*(hn)->vtbl->GetStat)(hn)
#define mwPlyGetTime(hn, ncount, tscale) (*(hn)->vtbl->GetTime)(hn, ncount, tscale)
#define mwPlyPause(hn, sw) (*(hn)->vtbl->Pause)(hn, sw)

#define MWE_PLY_STAT_PLAYEND 3
#define MWE_PLY_STAT_ERROR 4

// mwPlyInitSfdFx parameter
typedef struct {
    f32 disp_cycle;  // 0x00  59.94
    int x04;         // 0x04
    int x08;         // 0x08
    int x0C;         // 0x0C
    u8 pad_10[0x10]; // 0x10
} MWS_PLY_INIT_SFD;

// mwPlyCreateSofdec parameter
typedef struct {
    int ftype;        // 0x00
    int max_bps;      // 0x04
    int max_width;    // 0x08
    int max_height;   // 0x0C
    int nfrm_pool_wk; // 0x10
    int max_stm;      // 0x14
    void* work;       // 0x18
    int wksize;       // 0x1C
    int compo_mode;   // 0x20
} MWS_PLY_CPRM_SFD;

// mwPlyGetHdrInf output
typedef struct {
    u8 pad_0[0xA];
    s16 width;   // 0x0A
    u8 pad_C[2];
    s16 height;  // 0x0E
    u8 pad_10[0x2C - 0x10];
} MWS_SFD_HDRINF;

// mwPlyGetCurFrm output
typedef struct {
    void* bufadr;   // 0x00
    int x04;        // 0x04
    int width;      // 0x08
    int height;     // 0x0C
    u64 x10;        // 0x10 (8-byte member: aligns `Sofdec` to 8)
    u64 x18;        // 0x18
    int fno;        // 0x20 (x22 read as a halfword by cSofdec::draw)
    u8 pad_24[0x88 - 0x24];
} MWS_FRM;

void ADXM_SetCbErr(void (*func)(void* obj, const char* msg), void* obj);
void mwPlyInitSfdFx(MWS_PLY_INIT_SFD* prm);
int mwPlyCalcWorkCprmSfd(MWS_PLY_CPRM_SFD* prm);
MWPLY mwPlyCreateSofdec(MWS_PLY_CPRM_SFD* prm);
void mwPlyGetHdrInf(void* buf, int size, MWS_SFD_HDRINF* info);
void mwPlyGetCurFrm(MWPLY hn, MWS_FRM* frm);
void mwPlyRelCurFrm(MWPLY hn);
int mwPlyGetNumSkipDec(MWPLY hn);
int mwPlyGetNumSkipDisp(MWPLY hn);
void mwPlyFxSetOutBufSize(MWPLY hn, int width, int height);
void mwPlyFxSetOutBufPitchHeight(MWPLY hn, int pitch, int height);
void mwPlyFxCnvFrmY84C44(MWPLY hn, MWS_FRM* frm, void* ybuf, void* uvbuf);
void mwPlyFxCnvFrmARGB8888(MWPLY hn, MWS_FRM* frm, void* buf);

#ifdef __cplusplus
}
#endif

#endif
