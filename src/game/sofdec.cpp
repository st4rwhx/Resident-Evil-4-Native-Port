// game/sofdec: Sofdec (CRI) movie playback front end (D:/Bio4/Prog/sofdec.cpp).
#include "types.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "joy.h"
#include "fade.h"
#include "eprintf.h"
#include "dvd.h"
#include "scheduler.h"
#include "snd.h"
#include "sofdec.h"

extern "C" {
void DCFlushRangeNoSync(void* addr, u32 nBytes);
void OSReport(const char* fmt, ...);
int sprintf(char* dst, const char* fmt, ...);
char* strcpy(char* dst, const char* src);
void* memset(void* dst, int c, u32 n);
}

// CodeWarrior MSL math.h float constants, defined by the CRI headers for this compiler.
f32 __float_nan = 0.0f / 0.0f;
f32 __float_huge = 1.0f / 0.0f;

cSofdec Sofdec;

static inline void SetU32(u32& d, u32 v)
{
    d = v;
}

// Sofdec frame count (1/100 s units after scaling) -> h:m:s.frac.
void UsrSfcnt2time(int tscale, int count, int* h, int* m, int* s, int* f)
{
    int t = (int) ((f32) count / (f32) tscale * 100.0f);

    *h = t / 360000;
    *m = t / 6000 - *h * 60;
    *s = t / 100 - *h * 3600 - *m * 60;
    *f = t % 100;
}

void disp_info(SofdecApp* app)
{
    MWS_FRM* frm = &app->frm;
    int count, tscale;
    int h, m, s, f;

    mwPlyGetTime(app->hn, &count, &tscale);
    UsrSfcnt2time(tscale, count, &h, &m, &s, &f);
    eprintf(0x20, 0x10, 0, 0, "%s (%3d x %3d)", app->fname, frm->width, frm->height);
    eprintf(0x196, 0x10, 0, 0, "%02d:%02d:%02d.%02d", h, m, s, f);
    eprintf(0x1C6, 0x20, 0, 0, "%5d", frm->fno);
    eprintf(0x20, 0x20, 0, 0, "DECODE SKIP : %d", mwPlyGetNumSkipDec(app->hn));
    eprintf(0x20, 0x30, 0, 0, "DISP SKIP   : %d", mwPlyGetNumSkipDisp(app->hn));
}

void SofdecInit()
{
    MWS_PLY_INIT_SFD prm;

    prm.disp_cycle = 59.94f;
    prm.x04 = 1;
    prm.x08 = 1;
    prm.x0C = 1;
    ADXM_SetCbErr(ap_mwply_err_func, NULL);
    mwPlyInitSfdFx(&prm);
}

// YUV -> RGB TEV setup: stage 0/2 take the UV (IA8) map, stage 1 the Y map.
void setTevPrm(int mapY, int mapUV)
{
    union {
        GXColor c;
        u32 w;
    } kc;

    GXSetNumTexGens(2);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetTexCoordGen(1, 1, 4, 0x3C);
    GXSetNumTevStages(4);

    GXSetTevOrder(0, 0, mapUV, 0xFF);
    GXSetTevColorIn(0, 0xF, 8, 0xE, 2);
    GXSetTevColorOp(0, 0, 0, 0, 0, 0);
    GXSetTevAlphaIn(0, 7, 4, 6, 1);
    GXSetTevAlphaOp(0, 1, 0, 0, 0, 0);
    GXSetTevKColorSel(0, 0xC);
    GXSetTevKAlphaSel(0, 0x1C);
    GXSetTevSwapMode(0, 0, 1);

    GXSetTevOrder(1, 1, mapY, 0xFF);
    GXSetTevColorIn(1, 0xF, 8, 0xE, 0);
    GXSetTevColorOp(1, 0, 0, 1, 0, 0);
    GXSetTevAlphaIn(1, 7, 4, 6, 0);
    GXSetTevAlphaOp(1, 0, 0, 1, 0, 0);
    GXSetTevKColorSel(1, 0xD);
    GXSetTevKAlphaSel(1, 0x1D);
    GXSetTevSwapMode(1, 0, 0);

    GXSetTevOrder(2, 0, mapUV, 0xFF);
    GXSetTevColorIn(2, 0xF, 8, 0xE, 0);
    GXSetTevColorOp(2, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(2, 7, 4, 6, 0);
    GXSetTevAlphaOp(2, 1, 0, 0, 1, 0);
    GXSetTevKColorSel(2, 0xE);
    GXSetTevKAlphaSel(2, 0x1E);
    GXSetTevSwapMode(2, 0, 2);

    GXSetTevOrder(3, 0xFF, 0xFF, 0xFF);
    GXSetTevColorIn(3, 0, 1, 0xE, 0xF);
    GXSetTevColorOp(3, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(3, 7, 7, 7, 7);
    GXSetTevAlphaOp(3, 0, 0, 0, 1, 0);
    GXSetTevSwapMode(3, 0, 0);
    GXSetTevKColorSel(3, 0xF);

    {
        GXColorS10 c = {-111, 0, -138, 68};
        GXSetTevColorS10(1, c);
    }
    kc.w = 0x6600FF32;
    GXSetTevKColor(0, kc.c);
    kc.w = 0x94009494;
    GXSetTevKColor(1, kc.c);
    kc.w = 0xCB0005CF;
    GXSetTevKColor(2, kc.c);
    kc.w = 0x00FF0000;
    GXSetTevKColor(3, kc.c);
    GXSetTevSwapModeTable(0, 0, 1, 2, 3);
    GXSetTevSwapModeTable(1, 0, 3, 3, 3);
    GXSetTevSwapModeTable(2, 0, 0, 3, 0);
    GXSetNumChans(0);
    GXSetNumIndStages(0);
}

void restoreTevPrm()
{
    GXSetZMode(1, 7, 0);
    GXSetBlendMode(0, 1, 0, 0xF);
    GXSetNumTexGens(1);
    GXSetNumChans(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 0xFF);
    GXSetTevOp(0, 3);
    GXSetTevSwapMode(0, 0, 0);
    GXSetTevSwapMode(1, 0, 0);
    GXSetTevSwapMode(2, 0, 0);
    GXSetTevSwapMode(3, 0, 0);
    GXSetTevSwapModeTable(0, 0, 1, 2, 3);
    GXSetTevSwapModeTable(1, 0, 0, 0, 3);
    GXSetTevSwapModeTable(2, 1, 1, 1, 3);
    GXSetTevSwapModeTable(3, 2, 2, 2, 3);
}

void ap_mwply_err_func(void* obj, const char* msg)
{
    OSReport("%s\n", msg);
    for (;;) {
    }
}

void cSofdec::drawTex()
{
    Mtx tm;

    switch (mode) {
    case 0:
        if (drw.tex.yuv.bufY == NULL) {
            return;
        }
        setCamera(&drw);
        GXLoadTexObj(&drw.tex.yuv.texY, 0);
        GXLoadTexObj(&drw.tex.yuv.texUV, 1);
        setTevPrm(0, 1);
        GXSetBlendMode(1, 1, 0, 0);
        PSMTXIdentity(tm);
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXClearVtxDesc();
        GXSetVtxDesc(9, 1);
        GXSetVtxDesc(0xD, 1);
        GXSetVtxAttrFmt(0, 9, 1, 3, 0);
        GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
        drawQuad(&drw);
        break;
    case 1:
        GXLoadTexObj(&drw.tex.argb.tex, 0);
        GXSetNumTevStages(1);
        GXSetBlendMode(1, 4, 5, 0);
        PSMTXIdentity(tm);
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXClearVtxDesc();
        GXSetVtxDesc(9, 1);
        GXSetVtxDesc(0xD, 1);
        GXSetVtxAttrFmt(0, 9, 1, 3, 0);
        GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
        drawPolygon(&drw);
        break;
    }
    restoreTevPrm();
}

void cSofdec::drawQuad(SofdecDraw* d)
{
    Mtx tm, m;
    s16 hw = d->tex.width / 2;
    s16 hh = d->tex.height / 2 + 1;
    s16 nhw = -hw;
    s16 nhh = -hh;

    PSMTXTrans(tm, 0.0f, 0.0f, 0.0f);
    PSMTXConcat(d->mtx, tm, m);
    GXLoadPosMtxImm(m, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3s16(hw, hh, 0);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3s16(hw, nhh, 0);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3s16(nhw, nhh, 0);
    GXTexCoord2f32(0.0f, 1.0f);
    GXPosition3s16(nhw, hh, 0);
    GXTexCoord2f32(0.0f, 0.0f);
}

void cSofdec::drawPolygon(SofdecDraw* d)
{
    Mtx tm, m, sm, rx, ry, rz;

    PSMTXTrans(tm, 0.0f, 0.0f, -800.0f);
    PSMTXConcat(d->mtx, tm, m);
    PSMTXScale(sm, 512.0f, 512.0f, 512.0f);
    PSMTXConcat(m, sm, m);
    PSMTXRotRad(rx, 'X', 0.0f);
    PSMTXRotRad(ry, 'Y', 0.0f);
    PSMTXRotRad(rz, 'Z', 0.0f);
    PSMTXConcat(m, rx, m);
    PSMTXConcat(m, ry, m);
    PSMTXConcat(m, rz, m);
    GXLoadPosMtxImm(m, 0);
    GXDrawTorus(0.4f, 0x10, 0xC);
}

void cSofdec::setCamera(SofdecDraw* d)
{
    Mtx44 proj;
    Vec up = {0.0f, 1.0f, 0.0f};
    Vec pos = {0.0f, 0.0f, 400.0f};
    Vec target = {0.0f, 0.0f, 0.0f};
    f32 hh, hw;

    hh = (f32) (Rmode.xfbHeight / 2);
    hw = (f32) (Rmode.fbWidth / 2);
    C_MTXFrustum(proj, hh, -hh, -hw, hw, 400.0f, 3000.0f);
    GXSetProjection(proj, 0);
    C_MTXLookAt(d->mtx, &pos, &up, &target);
}

void cSofdec::loadMvFrmFx(MWPLY hn, MWS_FRM* frm)
{
    SofdecTex* tex = &drw.tex;

    switch (mode) {
    case 0:
        if (tex->yuv.bufY == NULL) {
            allocTexMem(tex, frm->width, frm->height);
        }
        mwPlyFxSetOutBufSize(hn, drw.tex.width, tex->height);
        mwPlyFxCnvFrmY84C44(hn, frm, tex->yuv.bufY, tex->yuv.bufUV);
        DCFlushRangeNoSync(tex->yuv.bufY, tex->yuv.sizeY);
        DCFlushRangeNoSync(tex->yuv.bufUV, tex->yuv.sizeUV);
        break;
    case 1:
        if (tex->argb.buf == NULL) {
            allocTexMem(tex, frm->width, frm->height);
        }
        mwPlyFxSetOutBufPitchHeight(hn, drw.tex.width * 4, tex->height);
        mwPlyFxCnvFrmARGB8888(hn, frm, tex->argb.buf);
        DCFlushRangeNoSync(tex->argb.buf, tex->argb.size);
        break;
    }
}

void cSofdec::allocTexMem(SofdecTex* tex, int w, int h)
{
    switch (mode) {
    case 0: {
        u16 w2 = (w / 2 + 31) & ~31;
        u16 h2 = h / 2;

        tex->height = (u16) h;
        tex->width = (u16) (w2 * 2);
        tex->yuv.sizeY = GXGetTexBufferSize(tex->width, tex->height, 1, 0, 0);
        tex->yuv.sizeUV = GXGetTexBufferSize(w2, h2, 3, 0, 0);
#line 489 "D:/Bio4/Prog/sofdec.cpp"
        tex->yuv.bufY = MEM_ALLOC(tex->yuv.sizeY, 1, 13);
        tex->yuv.bufUV = MEM_ALLOC(tex->yuv.sizeUV, 1, 13);
        if (tex->yuv.bufY == NULL || tex->yuv.bufUV == NULL) {
            OSReport("can't allocate tex buf.\n");
            break;
        }
        clrTexMem(tex);
        GXInitTexObj(&tex->yuv.texY, tex->yuv.bufY, tex->width, tex->height, 1, 0, 0, 0);
        GXInitTexObj(&tex->yuv.texUV, tex->yuv.bufUV, w2, h2, 3, 0, 0, 0);
        break;
    }
    case 1:
        tex->height = h;
        tex->width = (w + 31) & ~31;
        tex->argb.size = GXGetTexBufferSize(tex->width, tex->height, 6, 0, 0);
#line 518 "D:/Bio4/Prog/sofdec.cpp"
        tex->argb.buf = MEM_ALLOC(tex->argb.size, 1, 13);
        if (tex->argb.buf == NULL) {
            OSReport("can't allocate tex buf.\n");
            break;
        }
        clrTexMem(tex);
        GXInitTexObj(&tex->argb.tex, tex->argb.buf, tex->width, tex->height, 6, 0, 0, 0);
        break;
    }
}

void cSofdec::clrTexMem(SofdecTex* tex)
{
    switch (mode) {
    case 0:
        if (tex->yuv.bufY != NULL) {
            memset_asm(tex->yuv.bufY, 0, tex->yuv.sizeY);
            memset_asm(tex->yuv.bufUV, 0x80, tex->yuv.sizeUV);
        }
        break;
    case 1:
        if (tex->argb.buf != NULL) {
            memset_asm(tex->argb.buf, 0, tex->argb.size);
        }
        break;
    }
}

void cSofdec::initDraw(SofdecDraw* d)
{
    memclr_asm(d, sizeof(SofdecDraw));
    setCamera(d);
}

void cSofdec::initApp(const char* fname)
{
    static MWS_SFD_HDRINF info;
    void* buf;
    int req;

    initDraw(&drw);
    memclr_asm(&app, sizeof(SofdecApp));
    app.disp = 1;
    app.hn = NULL;
    app.xE4 = 0;
    strcpy(app.fname, fname);
#line 615 "D:/Bio4/Prog/sofdec.cpp"
    buf = MEM_ALLOC(0x5000, 1, 13);
    req = DvdReadN(fname, buf, 0, 0, 0x5000, 0x11, __FILE__, __LINE__);
    Dvd.ReadCheck(req, NULL, NULL, NULL);
    mwPlyGetHdrInf(buf, 0x5000, &info);
    Mem_free(buf);
    width = info.width;
    height = info.height;
}

int cSofdec::startApp()
{
    MWS_PLY_CPRM_SFD* cprm = &app.cprm;
    MWPLY hn;

    cprm->compo_mode = 0;
    cprm->ftype = 1;
    cprm->max_bps = 8000000;
    cprm->nfrm_pool_wk = 4;
    cprm->max_width = width;
    cprm->max_height = height;
    cprm->max_stm = 2;
    cprm->wksize = mwPlyCalcWorkCprmSfd(cprm);
#line 649 "D:/Bio4/Prog/sofdec.cpp"
    cprm->work = MEM_ALLOC(cprm->wksize, 1, 13);
    if (cprm->work == NULL) {
        ap_mwply_err_func(NULL, "Can't Malloc.");
        return 0;
    }
    app.work = cprm->work;
    hn = mwPlyCreateSofdec(cprm);
    if (hn == NULL) {
        Mem_free(app.work);
        ap_mwply_err_func(NULL, "Can't Create Handle.");
        return 0;
    }
    app.hn = hn;
    mwPlyStartFname(hn, app.fname);
    return 1;
}

void cSofdec::initSync()
{
    systemVISetBlack(1);
    fadeIn = 1;
    vcnt = GetSystemVcnt();
    SetSystemVcnt(1);
    if (Screen.width != 512.0f) {
        resized = 1;
        ScreenReSize(0x200, 0x1C0);
    }
    clrTexMem(&drw.tex);
    OSReport("Movie Play : %s \n", app.fname);
}

int cSofdec::appMain()
{
    MWS_FRM frm;
    int stat;

    if (Joy[0].trg & 0x1200) {
        flag |= 0x20;
        return 0;
    }
    ADXM_ExecMain();
    mwPlyGetCurFrm(app.hn, &frm);
    if (frm.bufadr != NULL) {
        loadMvFrmFx(app.hn, &frm);
        app.frm = frm;
        mwPlyRelCurFrm(app.hn);
    }
    stat = mwPlyGetStat(app.hn);
    if (stat == MWE_PLY_STAT_PLAYEND || stat == MWE_PLY_STAT_ERROR) {
        return 0;
    }
    app.stat = stat;
    return 1;
}

void cSofdec::draw()
{
    if (app.stat > 1) {
        drawTex();
        if (fadeIn == 1) {
            FadeKill(0);
            systemVISetBlack(0);
            fadeIn = 0;
        }
        fno = ((u16*) &app.frm.fno)[1];
        if (app.disp == 1) {
            disp_info(&app);
        }
    }
}

void cSofdec::finishMovie()
{
    mwPlyDestroy(app.hn);
    app.hn = NULL;
    Mem_free(app.work);
    if (drw.tex.yuv.bufY != NULL) {
        Mem_free(drw.tex.yuv.bufY);
        Mem_free(drw.tex.yuv.bufUV);
        drw.tex.yuv.bufY = NULL;
        drw.tex.yuv.bufUV = NULL;
    }
    systemVISetBlack(1);
    if (resized != 0) {
        ScreenReSize(0x280, 0x1C0);
    }
    SetU32(pG->flags_58, save58);
    SetU32(pG->flags_170, save170);
    SetSystemVcnt(vcnt);
    BitOff(pG->flags_500C, 0x10000000);
    if (!(pG->flags_5014 & 0x8000)) {
        MemDestroyHeap(11);
        Aram.DmaTransReq(1, 0x740000, heapStart, 0x500000, 1);
        MemSignalHeap(heapNo);
        MemSetCurrentHeap(heapNo);
    }
    BitOff(pG->flags_54, 0x00100000);
    if (!chkFlag(0x100)) {
        systemVISetBlack(0);
    }
    flag &= ~1;
}

int cSofdec::initWork(const char* fname)
{
    if (Dvd.FileExistCheck(fname, NULL) == -1) {
        OSReport("File not found : %s\n", fname);
        sprintf(path, "movie/dmy.sfd");
        if (Dvd.FileExistCheck(path, NULL) == -1) {
            return 0;
        }
    }
    SetU32(save170, pG->flags_170);
    SetU32(pG->flags_170, 0xFFFFFFFF);
    SetU32(save58, pG->flags_58);
    SetU32(pG->flags_58, 0xFFFFFFFF);
    if (!(pG->flags_5014 & 0x8000)) {
        heapNo = MemGetCurrentHeap();
        heapStart = MemGetHeapStartAddr(heapNo);
        Aram.DmaTransReq(0, heapStart, 0x740000, 0x500000, 1);
        MemSuspendHeap(heapNo);
        MemCreateHeap(11, heapStart, heapStart + 0x500000);
        MemSetCurrentHeap(11);
    }
    pG->flags_54 |= 0x00100000;
    return 1;
}

int cSofdec::Initialize(const char* fname, u32 flags)
{
    return initSub(fname, flags);
}

int cSofdec::Initialize(cString& fname, u32 flags)
{
    return initSub(fname.c_str(), flags);
}

int cSofdec::initSub(const char* fname, u32 flags)
{
    if (pG->flags_500C & 0x10000000) {
        return 0;
    }
    sprintf(path, "%s", fname);
    if (!(flags & 0x200)) {
        TaskSuspend(0);
        TaskExec(1, (TaskFunc) ThreadMove, (int) this);
    } else {
        if (!initWork(fname)) {
            return 0;
        }
        initApp(path);
        startApp();
        pG->flags_500C |= 0x10000000;
        initSync();
        flag = flags | 1;
    }
    SndAllStop();
    return 1;
}

int cSofdec::Move()
{
    int ret = 0;
    int r = appMain();

    draw();
    if (r == 0) {
        ret = 1;
        finishMovie();
    }
    return ret;
}

void cSofdec::ThreadMove(cSofdec* s)
{
    int r = s->initWork(s->path);

    if (r == 1) {
        s->initApp(s->path);
        s->startApp();
        pG->flags_500C |= 0x10000000;
        s->initSync();
        s->flag = 1;
        while (s->Move() == 0) {
            TaskSleep(1);
        }
    }
    TaskSignal(0);
    TaskExit();
}

void cSofdec::PlayPause(int pause)
{
    if (pause == 1) {
        flag |= 4;
    } else {
        flag &= ~4;
    }
    mwPlyPause(app.hn, pause);
}
