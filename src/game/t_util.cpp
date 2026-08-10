#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "main_sub.h"
#include "t_prim.h"
#include "t_util.h"

void GameStopModeEnd();

// Copies of the pG flag words the tools modify, restored by TutilQuitDefault.
Camera globalCamera;
u32 debug_flg_bak[4];   // pG->flags_60 .. flags_6C
u32 status_flg_bak[4];  // pG->flags_500C .. 0x5018
u32 stop_flg_bak;       // pG->flags_170
u32 disp_flg_bak;       // pG->flags_58
static u32 system_flg_bak;  // pG->flags_54

void TutilInitDefault()
{
    TprimView view;

    GameStopModeEnd();
    view.rect.x = Screen.x;
    view.rect.y = Screen.y;
    view.rect.w = Screen.width;
    view.rect.h = Screen.height;
    view.nearz = 0.0f;
    view.farz = 1.0f;
    TprimInitEnv2D3D(&view, pG->Cam.projMat, pG->Cam.viewMat);
    globalCamera = pG->Cam;
    system_flg_bak = TOOL_FLAG(OFS_SYSTEM_FLG);
    stop_flg_bak = TOOL_FLAG(OFS_STOP_FLG);
    disp_flg_bak = TOOL_FLAG(OFS_DISP_FLG);
    memcpy(debug_flg_bak, TOOL_FLAG_PTR(OFS_DEBUG_FLG), sizeof(debug_flg_bak));
    memcpy(status_flg_bak, TOOL_FLAG_PTR(OFS_STATUS_FLG), sizeof(status_flg_bak));
    TOOL_FLAG(OFS_STOP_FLG) |= 0x200;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x80;
    TOOL_FLAG(OFS_DEBUG_FLG) |= 0x8000;
    TOOL_FLAG(OFS_DEBUG_FLG + 8) |= 0x800000;
    TOOL_FLAG(OFS_DEBUG_FLG + 12) &= ~0x2000;
}

void TutilQuitDefault()
{
    memcpy(TOOL_PTR(OFS_CAMERA), &globalCamera, sizeof(Camera));
    TOOL_FLAG(OFS_SYSTEM_FLG) = system_flg_bak;
    TOOL_FLAG(OFS_STOP_FLG) = stop_flg_bak;
    TOOL_FLAG(OFS_DISP_FLG) = disp_flg_bak;
    if (TOOL_FLAG(OFS_DEBUG_FLG) & 0x100) {
        debug_flg_bak[0] |= 0x100;
    }
    memcpy(TOOL_FLAG_PTR(OFS_DEBUG_FLG), debug_flg_bak, sizeof(debug_flg_bak));
    memcpy(TOOL_FLAG_PTR(OFS_STATUS_FLG), status_flg_bak, sizeof(status_flg_bak));
    TOOL_FLAG(OFS_DEBUG_FLG) &= 0x7FFFFFFF;
    TOOL_FLAG(OFS_DEBUG_FLG + 12) |= 0x2000;
}

// Never called in this build. GCC 2.95 emits the initializer templates of local aggregates in
// inline functions at parse time; the original t_util.o has these 7 words between the
// TutilInitDefault constants and the ToolMenuDisp_cur strings. Values are the original's, the
// grouping and body are a guess that reproduces them.
static inline void tutil_2d_env(f32* scale, Vec* size)
{
    f32 sc[4] = {0.0078125f, 0.0f, 0.5f, 0.0f};
    Vec sz = {1024.0f, 2048.0f, 0.0f};

    scale[0] = sc[0];
    scale[1] = sc[2];
    *size = sz;
}

TOOL_MENU* old_menu = NULL;

int ToolMenuDisp_cur(int x, int y, int flag, s8* cursor, TOOL_MENU* menu, int size, JOY* joy)
{
    static s8 cursor_s = 0;
    static u8 flicker = 4;
    TOOL_MENU* p = menu;
    int num;
    int i;
    int ret;
    int color;

    if (cursor != NULL) {
        cursor_s = *cursor;
    }
    num = size / sizeof(TOOL_MENU);
    if (old_menu != p) {
        if (cursor == NULL) {
            if (flag & TOOL_MENU_START_LAST) {
                cursor_s = num - 1;
            } else {
                cursor_s = 0;
            }
        }
        old_menu = p;
    }
    if (joy->rep & JOY_DOWN) {
        cursor_s++;
    }
    if (joy->rep & JOY_UP) {
        cursor_s--;
    }
    cursor_s = cursor_s < 0 ? num - 1 : (cursor_s > num - 1 ? 0 : cursor_s);
    if (joy->rep & (JOY_DOWN | JOY_UP)) {
        flicker = 8;
    }
    if ((joy->trg & JOY_B) && (flag & TOOL_MENU_B_LAST)) {
        cursor_s = num - 1;
        flicker = 8;
    }
    for (i = 0; i < num; i++) {
        color = 0x14;
        if (p->enable) {
            color = 0;
        }
        eprintf(x, y + i * 16, color, 0, "%s", p->name);
        p++;
    }
    if (flicker & 0x18) {
        eprintf(x - 8, y + cursor_s * 16, 0, 0, ">");
    }
    flicker++;
    if (cursor != NULL) {
        *cursor = cursor_s;
    }
    p = &menu[cursor_s];
    if ((joy->trg & JOY_A) && p->enable) {
        if (p->func != NULL) {
            p->func();
        }
        ret = cursor_s;
        cursor_s = 0;
        return ret;
    }
    return -1;
}

// The next unit's .sdata (TexRender: 32-byte aligned vfilter tables) starts 32-byte aligned in
// the original link, which leaves 0x1A zero bytes after `flicker`. The split object of TexRender
// does not carry that alignment yet, so pad this unit's .sdata to the same boundary here.
asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
