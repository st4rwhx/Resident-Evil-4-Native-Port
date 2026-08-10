#include "types.h"
#include "global.h"
#include "gx.h"
#include "os_vi.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "main.h"
#include "main_mem.h"
#include "main_sub.h"
#include "vec.h"
#include "view.h"
#include "scheduler.h"
#include "db_log.h"
#include "eprintf.h"
#include "tpl.h"
#include "tv_mode.h"

typedef s64 OSTime;

struct OSCalendarTime {
    int sec;   // 0x00
    int min;   // 0x04
    int hour;  // 0x08
    int mday;  // 0x0C
    int mon;   // 0x10
    int year;  // 0x14
    int wday;  // 0x18
    int yday;  // 0x1C
    int msec;  // 0x20
    int usec;  // 0x24
};

struct OSStopwatch {
    const char* name;  // 0x00
    u8 pad_4[4];
    OSTime total;      // 0x08
    u32 hits;          // 0x10
    u8 pad_14[4];
    OSTime min;        // 0x18
    OSTime max;        // 0x20
    OSTime last;       // 0x28
};

extern "C" {
void OSReport(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
OSTime OSGetTime();
void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td);
void OSInitStopwatch(OSStopwatch* sw, const char* name);
void OSResetStopwatch(OSStopwatch* sw);
void OSStartStopwatch(OSStopwatch* sw);
void OSStopStopwatch(OSStopwatch* sw);
BOOL OSLink(OSModuleHeader* module, void* bss);
BOOL OSUnlink(OSModuleHeader* module);
void DCFlushRange(void* addr, u32 nBytes);
void* GXInit(void* base, u32 size);
u32 GXSetDispCopyYScale(f32 yscale);
void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht);
void GXSetDispCopyDst(u16 wd, u16 ht);
void GXSetPixelFmt(int pix_fmt, int z_fmt);
void GXSetZCompLoc(u8 before_tex);
void GXCopyDisp(void* dest, u8 clear);
void GXSetDispCopyGamma(int gamma);
void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz);
void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field);
void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht);
void GXInvalidateVtxCache();
void VISetNextFrameBuffer(void* fb);
void VIWaitForRetrace();
u32 VIGetNextField();
void VISetBlack(BOOL black);
void ProcessTickGet(int no, const char* name);
void ExecOt(int no);
}
void SetDrawTmpBufType(int type);

// game/sce_sys.cpp
class cSceSys {
public:
    int wait;  // 0x00
    u8 pad_4[0x138 - 4];
    int checkCTaskRange();
};
extern cSceSys SceSys;
extern "C" void SceSleep(int frames);

// Low memory globals (OSPhysicalToCached(0x00F8) = bus clock); a struct member so the
// address splits into `lis 0x8000` + displacement.
struct OSLowMem {
    u8 pad_0[0xF8];
    u32 busClock;  // 0xF8
};
#define OS_BUS_CLOCK (((OSLowMem*) 0x80000000)->busClock)
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSTicksToSeconds(ticks) ((ticks) / OS_TIMER_CLOCK)
#define OSTicksToMicroseconds(ticks) (((ticks) * 8) / (OS_TIMER_CLOCK / 125000))
#define VIPadFrameBufferWidth(width) ((u16) (((u16) (width) + 15) & ~15))

// A plain block, not do/while(0): the loop notes of a do/while are a scheduling barrier, and the
// original issues `li r4,0` of the preceding pLog->err before the string address (anti-dependence
// on HALT's own `lis r4`), which needs one scheduling region (DLL_Link/DLL_Unlink, read.cpp too).
#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

#line 30 "D:/Bio4/Prog/main_sub.cpp"

// Dead-stripped in the original (its string survives in .rodata).
static void GXVerifyCallback(int level, u32 id, const char* msg)
{
    OSReport("Level %d, Warning %03d\n", level, id);
}

GXRenderModeObj Rmode = {
    0,       // viTVmode
    0x200,   // fbWidth
    0x1C0,   // efbHeight
    0x1C0,   // xfbHeight
    0x28,    // viXOrigin
    0x10,    // viYOrigin
    0x280,   // viWidth
    0x1C0,   // viHeight
    1,       // xFBmode
    0,       // field_rendering
    0,       // aa
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8},
};

void* DefaultFifoObj;
int ScreenShotTriggerType;
void* DefaultFifo;
int flag_render_after = 0;
int lbl_80314BFC = 0;  // ScreenShotCount: unreferenced, Bio4.sym has no name (strip_unused keeps lbl_ names)
int AutoScreenShotExec = 0;
char ScreenShotFilename[11];
OSStopwatch SW;

static int AutoScreenShotFrame;
static char* AutoScreenShotFilename;
static int ScreenShotFrame;
static u8 Line;

void Render_init()
{
    GXRenderModeObj* rm = &Rmode;

    SetTvMode(rm);
    pFrame_buff[0] = (void*) 0x80460000;
    pCurrent_buff = pFrame_buff[1] =
        (void*) (0x80460000 + VIPadFrameBufferWidth(rm->viWidth) * rm->xfbHeight * 2);
    DefaultFifo = (void*) 0x803F0000;
    VIConfigure(rm);
    DefaultFifoObj = GXInit(DefaultFifo, 0x70000);
    ScreenGXSet();
    GXSetDispCopyYScale((f32) rm->xfbHeight / (f32) rm->efbHeight);
    GXSetDispCopySrc(0, 0, rm->fbWidth, rm->xfbHeight);
    GXSetDispCopyDst(rm->fbWidth, rm->xfbHeight);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    if (rm->aa) {
        GXSetPixelFmt(2, 0);
    } else {
        GXSetPixelFmt(0, 0);
    }
    GXSetPixelFmt(1, 0);
    GXSetZCompLoc(0);
    GXCopyDisp(pCurrent_buff, 0);
    GXSetDispCopyGamma(0);
    VISetNextFrameBuffer(pFrame_buff[0]);
    pCurrent_buff = pFrame_buff[1];
    VIFlush();
    VIWaitForRetrace();
    ScreenReSize(512, 448);
}

void Render_before()
{
    if (Rmode.field_rendering) {
        GXSetViewportJitter(0.0f, 0.0f, Screen.width, Screen.height, 0.0f, 1.0f, VIGetNextField());
    } else {
        GXSetViewport(0.0f, 0.0f, Screen.width, Screen.height, 0.0f, 1.0f);
    }
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
    Bg_brightness_set(64.0f);
}

void Render_done()
{
    GXSetZMode(1, 3, 1);
    GXSetColorUpdate(1);
    if (SceSys.checkCTaskRange() == 0) {
        GXDrawDone();
    } else {
        SceSys.wait = 1;
        SceSleep(1);
    }
    after_render_proc();
    Bg_brightness_set((f32) pSys->brightness);
    if (pG->flags_68 & 0x08000000) {
        GXCopyDisp(pCurrent_buff, 0);
    } else {
        GXSetAlphaUpdate(1);
        GXCopyDisp(pCurrent_buff, 1);
        GXSetAlphaUpdate(0);
    }
    if (SceSys.checkCTaskRange() == 0) {
        GXDrawDone();
    } else {
        SceSys.wait = 1;
        SceSleep(1);
    }
}

void Render_swap()
{
    if (!(pG->flags_54 & 0x400)) {
        VISetNextFrameBuffer(pCurrent_buff);
        if (pCurrent_buff == pFrame_buff[0]) {
            pCurrent_buff = pFrame_buff[1];
        } else {
            pCurrent_buff = pFrame_buff[0];
        }
    }
    VIFlush();
}

void UpdateNearClipDist()
{
    GlobalWork* g = pG;
    if (!(g->flags_5010 & 0x1000)) {
        FSet(ZNEAR, 100.0f);
    }
    g->flags_5010 &= ~0x1000;
}

void SetNearClipDist(f32 dist)
{
    FSet(ZNEAR, dist);
    pG->flags_5010 |= 0x1000;
}

int Render_checkBlurPermission()
{
    u8 mode = pG->x20;
    if (mode == 3 || mode == 4 || mode == 6) {
        return 1;
    }
    return 0;
}

void Render_DrawSyncCallback(u16 token)
{
    if (token == 0xADEB) {
        ProcessTickGet(1, "RENDER END");
        pG->flags_54 |= 0x10000000;
    }
}

void systemVISetBlack(int black)
{
    if (black == 1) {
        VISetBlack(1);
        pG->flags_54 |= 0x40000;
    } else {
        VISetBlack(0);
        pG->flags_54 &= ~0x40000;
    }
}

void SetScissorState()
{
    if (pG->flags_5018 & 0x10000000) {
        GXSetScissor(0, 56, (u32) Screen.width, (u32) Screen.height - 111);
    } else {
        SetNoScissor();
    }
}

void SetNoScissor()
{
    GXSetScissor((u32) Screen.x, (u32) Screen.y, (u32) Screen.width, (u32) Screen.height);
}

// Dead-stripped in the original (its SF 0.0 constant survives in .rodata).
static int ScreenIsOrigin()
{
    return Screen.x == 0.0f;
}

void ScreenGXSet()
{
    GXSetViewport(Screen.x, Screen.y, Screen.width, Screen.height, 0.0f, 1.0f);
    SetScissorState();
}

void ScreenReSize(u16 w, u16 h)
{
    Mtx44 mtx;
    GXRenderModeObj* rm = &Rmode;

    Screen.width = (f32) w;
    Screen.height = (f32) h;
    rm->fbWidth = w;
    rm->efbHeight = h;
    DCFlushRange(pFrame_buff[0], 0x8C000);
    DCFlushRange(pFrame_buff[1], 0x8C000);
    VIConfigure(rm);
    VIFlush();
    ScreenGXSet();
    GXSetDispCopyYScale((f32) rm->xfbHeight / (f32) rm->efbHeight);
    GXSetDispCopySrc(0, 0, rm->fbWidth, rm->xfbHeight);
    GXSetDispCopyDst(rm->fbWidth, rm->xfbHeight);
    C_MTXOrtho(mtx, 0.0f, 448.0f, 0.0f, (f32) rm->fbWidth, 0.0f, -10000.0f);
    GXSetProjection(mtx, 1);
}

void EFBReSize(int w, int h)
{
    Screen.width = (f32) w;
    Screen.height = (f32) h;
    GXSetViewport(Screen.x, Screen.y, Screen.width, Screen.height, 0.0f, 1.0f);
    GXSetScissor((u32) Screen.x, (u32) Screen.y, (u32) Screen.width, (u32) Screen.height);
}

void SecToTime(u32 sec, u32* h, u32* m, u32* s)
{
    u32 hour = sec / 3600;
    if (h) {
        *h = hour;
    }
    if (m) {
        *m = (sec - hour * 3600) / 60;
    }
    if (s) {
        *s = sec % 60;
    }
}

void InitGameTime()
{
    OSTime t = OSGetTime();
    pG->time_base = OSTicksToSeconds(t);
}

u32 GetGameTime(u32* h, u32* m, u32* s)
{
    u32 sec;
    OSTime t = OSGetTime();
    sec = OSTicksToSeconds(t) - pG->time_base + pG->play_time;
    SecToTime(sec, h, m, s);
    return sec;
}

void SetGameTime()
{
    OSTime t = OSGetTime();
    pG->play_time += OSTicksToSeconds(t) - pG->time_base;
    t = OSGetTime();
    pG->time_base = OSTicksToSeconds(t);
}

void ScreenShotStart(char* name, int frame, int flag)
{
    static int AutoScreenShotExecFlag = 1;
    AutoScreenShotExecFlag = flag;
    AutoScreenShotExec = 1;
    AutoScreenShotFrame = frame;
    AutoScreenShotFilename = name;
}

void ScreenShotEnd()
{
    AutoScreenShotExec = 0;
}

int ScreenShotExec = 0;
int lbl_80314C0C = 0;  // ScreenShotWait: unreferenced, Bio4.sym has no name

void SelfScreenShotInit()
{
    OSCalendarTime ct;
    OSTicksToCalendarTime(OSGetTime(), &ct);
    sprintf(ScreenShotFilename, "_%02d%02d%02d%02d_", ct.mon + 1, ct.mday, ct.hour, ct.min);
    ScreenShotFrame = 0;
    ScreenShotTriggerType = 0;
    ScreenShotExec = 0;
    AutoScreenShotExec = 0;
}

// Dead-stripped in the original (strings and constant pool survive in .rodata).
static void ScreenShotMain(int frame)
{
    char buf[64];
    char name[64];
    sprintf(name, "%s_%05d.bmp", ScreenShotFilename, frame);
    sprintf(buf, "d:\\bio4/Room/Sc_shot/r%03x%s%06d.bmp", G_ROOM_ID, ScreenShotFilename, frame);
    if (Screen.x != 1.0f) {
        Screen.y = (f32) frame;
    }
}

void StopwatchInit()
{
    OSInitStopwatch(&SW, "");
    Line = 0;
}

void StopwatchStart()
{
    OSResetStopwatch(&SW);
    OSStartStopwatch(&SW);
}

u32 StopwatchStop(const char* name)
{
    OSTime us;
    OSStopStopwatch(&SW);
    us = OSTicksToMicroseconds(SW.total);
    if (name) {
        eprintf2(10, 16, 50, Line + 50, 0, 1, "%s %d", (u32) us, name);
        Line += 16;
    }
    return us;
}

void after_render_proc()
{
    flag_render_after = 1;
    ExecOt(0x16);
    SetDrawTmpBufType(0);
    flag_render_after = 0;
}

void Bg_brightness_set(f32 brightness)
{
    u8 vf[7] = {8, 8, 10, 12, 10, 8, 8};
    u8 order[7] = {5, 1, 3, 0, 4, 2, 6};
    GXRenderModeObj* rm = &Rmode;
    f32 rate = brightness * (1.0f / 64.0f);
    int i;
    int j;

    for (i = 0; i < 7; i++) {
        rm->vfilter[i] = (u8) ((f32) (int) vf[i] * rate);
        brightness -= (f32) (int) rm->vfilter[i];
    }
    i = 0;
    while (brightness >= 1.0f) {
        for (j = 0; j < 7; j++) {
            if (order[j] == i) {
                break;
            }
        }
        rm->vfilter[j]++;
        brightness -= 1.0f;
        if (i++ > 6) {
            i = 0;
        }
    }
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
}

void DrawTpl(TEXPalette* tpl, int x, int y, int w, int h)
{
    GXTexObj texObj;
    GXTlutObj tlutObj;
    TEXDescriptor* desc;
    TEXHeader* hdr;
    CLUTHeader* clut;
    u32 addr = (u32) tpl;

    if (addr < 0x80000000 || addr > 0x82FFFFFF) {
        return;
    }
    desc = (TEXDescriptor*) (tpl + 1);
    if (!VALID_PTR(desc)) {
        return;
    }
    if ((s32) desc->textureHeader >= 0) {
        desc->textureHeader = (TEXHeader*) ((u8*) tpl + (u32) desc->textureHeader);
        if (!VALID_PTR(desc->textureHeader)) {
            return;
        }
        desc->CLUTHeader = (CLUTHeader*) ((u8*) tpl + (u32) desc->CLUTHeader);
        desc->textureHeader->data = (u8*) tpl + (u32) desc->textureHeader->data;
        desc->CLUTHeader->data = (u8*) tpl + (u32) desc->CLUTHeader->data;
        if ((s32) desc->textureHeader >= 0) {
            return;
        }
    }
    hdr = desc->textureHeader;
    if ((u32) hdr > 0x82FFFFFF) {
        return;
    }
    if (hdr->format - 8 <= 1) {
        GXInitTexObjCI(&texObj, hdr->data, hdr->width, hdr->height, hdr->format, 0, 0, 0, 0);
        clut = desc->CLUTHeader;
        if (!VALID_PTR(clut)) {
            return;
        }
        GXInitTlutObj(&tlutObj, clut->data, clut->format, clut->numEntries);
        GXLoadTlut(&tlutObj, 0);
    } else {
        GXInitTexObj(&texObj, hdr->data, hdr->width, hdr->height, hdr->format, 0, 0, 0);
    }
    DrawTexture(&texObj, x, y, 1, w, h);
}

void DrawTexture(GXTexObj* obj, s16 x, s16 y, s16 z, s16 w, s16 h)
{
    GXColor color;
    s16 x2;
    s16 y2;

    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    color.r = color.g = color.b = color.a = 255;
    GXSetChanAmbColor(0, color);
    GXSetChanMatColor(0, color);
    Mtx mtx;
    Mtx44 proj;
    PSMTXIdentity(mtx);
    GXLoadTexObj(obj, 0);
    GXLoadTexMtxImm(mtx, 30, 1);
    GXSetTexCoordGen(0, 1, 4, 30);
    GXSetNumTexGens(1);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevOp(0, 3);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, (f32) Rmode.fbWidth, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(0, 1, 0, 0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    x2 = x + w;
    y2 = y + h;
    GXPosition3s16(x, y, z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3s16(x2, y, z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3s16(x2, y2, z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3s16(x, y2, z);
    GXTexCoord2f32(0.0f, 1.0f);
}

void DLL_Unlink(OSModuleHeader* module)
{
    if (module->epilog) {
        module->epilog();
    }
    if (OSUnlink(module) != 1) {
        pLog->err(0, 0, "OSUnlink failed : 0x%08x", module);
        TaskSleep(60);
#line 1424 "D:/Bio4/Prog/main_sub.cpp"
        HALT();
    }
    OSReport("The unlink of DLL was completed.\n");
}

void DLL_Link(OSModuleHeader* module, void* bss)
{
    if (OSLink(module, bss) != 1) {
        pLog->err(0, 0, "OSLink failed : 0x%08x", module);
        TaskSleep(60);
#line 1440 "D:/Bio4/Prog/main_sub.cpp"
        HALT();
    }
    OSReport("The link of DLL was completed.\n");
}
