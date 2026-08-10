#ifndef MAIN_SUB_H
#define MAIN_SUB_H

#include "types.h"
#include "gx.h"

// Render target description (game/main.cpp `Screen`, 0x18 bytes; layout partially known).
struct ScreenInfo {
    f32 x;       // 0x00
    f32 y;       // 0x04
    f32 width;   // 0x08 (512.0f)
    f32 height;  // 0x0C (448.0f)
    u8 pad_10[0x18 - 0x10];
};

extern ScreenInfo Screen;
extern GXRenderModeObj Rmode;  // game/main_sub.cpp

// game/main.cpp frame buffers
extern void* pFrame_buff[2];
extern void* pCurrent_buff;

// Dolphin OSSectionInfo / OSModuleHeader (REL header); only the fields the game uses are named.
struct OSSectionInfo {
    u32 offset;        // 0x00  bit 0: executable (exception.cpp takes the .text address from section 1)
    u32 size;          // 0x04
};

struct OSModuleHeader {
    u32 id;            // 0x00
    u8 pad_4[8];       // 0x04  link
    u32 numSections;   // 0x0C
    OSSectionInfo* sectionInfo;  // 0x10  (an offset before OSLink)
    u8 pad_14[0x20 - 0x14];
    u32 bssSize;       // 0x20  (read.cpp: must fit the 0x80-byte area in front of a ReadModule)
    u8 pad_24[0x34 - 0x24];
    void (*prolog)();  // 0x34  (read.cpp calls it right after DLL_Link)
    void (*epilog)();  // 0x38
    u8 pad_3C[4];
};

// game/main_sub.cpp
int Render_checkBlurPermission();
extern "C" {
void Render_init();
void Render_before();
void Render_done();
void Render_swap();
void UpdateNearClipDist();
void SetNearClipDist(f32 dist);
void Render_DrawSyncCallback(u16 token);
void systemVISetBlack(int black);
void SetScissorState();
void SetNoScissor();
void ScreenGXSet();
void ScreenReSize(u16 w, u16 h);
void EFBReSize(int w, int h);
void SecToTime(u32 sec, u32* h, u32* m, u32* s);
void InitGameTime();
u32 GetGameTime(u32* h, u32* m, u32* s);
void SetGameTime();
void ScreenShotStart(char* name, int frame, int flag);
void ScreenShotEnd();
void SelfScreenShotInit();
void StopwatchInit();
void StopwatchStart();
u32 StopwatchStop(const char* name);
void after_render_proc();
void Bg_brightness_set(f32 brightness);
void DrawTpl(struct TEXPalette* tpl, int x, int y, int w, int h);
void DrawTexture(GXTexObj* obj, s16 x, s16 y, s16 z, s16 w, s16 h);
void DLL_Unlink(OSModuleHeader* module);
void DLL_Link(OSModuleHeader* module, void* bss);
}
// main_sub.cpp also owns flag_render_after, AutoScreenShotExec, ScreenShotExec,
// ScreenShotTriggerType, ScreenShotFilename[11]; declare them extern locally where needed
// (a header extern would change main_sub's .sbss order).
// game/TmpBuf.cpp
void* GetDrawTmpBufAddr(int type);

#endif
