#ifndef T_UTIL_H
#define T_UTIL_H

#include "types.h"
#include "global.h"
#include "joy.h"

// Debug tool helpers (game/t_util.cpp).

// One menu line for ToolMenuDisp_cur (0xC bytes).
struct TOOL_MENU {
    u8 enable;          // 0x00  0 = greyed out (color 0x14) and not selectable
    const char* name;   // 0x04
    void (*func)();     // 0x08  called when selected (may be NULL)
};

// ToolMenuDisp_cur flag bits
#define TOOL_MENU_B_LAST   1  // B button jumps to the last entry
#define TOOL_MENU_START_LAST 2  // cursor starts on the last entry for a new menu

// The tools address pG's flag words through raw u32 pointers instead of struct members. GCC 2.95
// then treats every such store as possibly aliasing pG itself and reloads pG afterwards, and
// keeps the loads in source order; struct-member accesses compile differently.
#define TOOL_FLAG(ofs) (*(u32*) ((u8*) pG + (ofs)))
#define TOOL_FLAG_PTR(ofs) ((u32*) ((u8*) pG + (ofs)))
#define TOOL_HALF(ofs) (*(u16*) ((u8*) pG + (ofs)))
#define TOOL_PTR(ofs) ((u8*) pG + (ofs))
#define OFS_CAMERA 0x74       // Camera Cam
#define OFS_SYSTEM_FLG 0x54
#define OFS_DISP_FLG 0x58
#define OFS_DEBUG_FLG 0x60   // 4 words
#define OFS_STOP_FLG 0x170
#define OFS_STATUS_FLG 0x500C  // 4 words
#define OFS_PL_LIFE 0x4FA4
#define OFS_PL_LIFE_MAX 0x4FA6
#define OFS_SUB_LIFE 0x4FA8
#define OFS_SUB_LIFE_MAX 0x4FAA

void TutilInitDefault();
void TutilQuitDefault();
// Draws `menu` (size in bytes) at x,y and moves the cursor with `joy`. Returns the selected
// entry when A is pressed on an enabled line, else -1. `cursor` may be NULL.
int ToolMenuDisp_cur(int x, int y, int flag, s8* cursor, TOOL_MENU* menu, int size, JOY* joy);
// tools/t_util.cpp only (dead-stripped from the DOL): screen position of a world point.
int TutilGetScreenPos(Vec* pos, f32* scr, int noSetup);
void TutilMoveCursor(Vec* pos, f32 speed, f32 step);
// t_event/t_sce: ToolMenuDisp_cur without a cursor variable
int ToolMenuDisp(int x, int y, int flag, TOOL_MENU* menu, int size, JOY* joy);
// Tools only: XZ position under a screen point (projection search around `center`, side `step`).
int TutilGet3DPosXZ(Vec* target, Vec* center, Vec* out, f32 step);
int TutilGet3DPosXZ_Mov(Vec* target, Vec* center, Vec* out);
int TutilGet3DPosXZ_All(Vec* target, Vec* center, Vec* out);

#endif
