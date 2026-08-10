#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "main_mem.h"
#include "main_sub.h"
#include "file.h"
#include "tpl.h"
#include "t_util.h"
#include "tools.h"

extern "C" int sprintf(char* s, const char* fmt, ...);
extern "C" char* strcpy(char* dst, const char* src);

// TPL viewer debug tool (Tools/t_tplview.cpp): lists the sub-screen viewer TPLs, draws the selected one
// and lets the pad move / resize it.

struct TplViewWork {
    s8 step;       // 0x00  0 init, 1 menu, 2 quit, 3 sub menu
    u8 pad_1[3];
    s8 cursor;     // 0x04  main menu (FiLE / SiZE / QUiT)
    s8 sizeCur;    // 0x05  size menu (WxH / ORG)
    s8 whCur;      // 0x06  WxH menu (FullScrn / Texture)
    u8 pad_7;
    int x;         // 0x08
    int y;         // 0x0C
    int scrW;      // 0x10  screen size (ScreenReSize)
    int scrH;      // 0x14
    int w;         // 0x18  drawn size
    int h;         // 0x1C
    u8 wide;       // 0x20  640-wide screen
    s8 fileNum;    // 0x21  TPL files found
    s8 fileCur;    // 0x22  file list cursor
    u8 debugBak;   // 0x23  pG->debug_mode on entry
    u8 reload;     // 0x24  reload the current file
    u8 pad_25[3];
};

// the first TPL descriptor follows the palette header
struct TplFile {
    TEXPalette pal;
    TEXDescriptor desc;
};

static u32 tplStopFlagBak;
TplViewWork tplWork;
static void* pTplBuf = 0;

void getTplname(char* name, int no);
void TplViewer();


void ToolTplView()
{
    tplStopFlagBak = TOOL_FLAG(OFS_STOP_FLG);
    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000000;
    ToolArrayPush(0);
    for (;;) {
        TplViewer();
        TaskSleep(1);
    }
}

void getTplname(char* name, int no)
{
    sprintf(name, "d:\\bio4/Room/SubScreen/Viewer/file%02ld.tpl", no);
}

// menu tables: emitted here, between getTplname's string and the list printer's "%s" (a namespace
// `static const` is deferred to the end of the file; a public const object is emitted at its
// definition), but the original's relocation fields hold S+A, i.e. the tables were LOCAL symbols. The
// `.L` assembler names keep them out of the symbol table (section-relative relocations) while the
// declarations stay public for the emission order.
struct TplMenu3 {
    const char* s[3];
};
struct TplMenu2 {
    const char* s[2];
};
extern const TplMenu3 tplMainMenu asm(".L_tplMainMenu");
extern const TplMenu2 tplSizeMenu asm(".L_tplSizeMenu");
extern const TplMenu2 tplWhMenu asm(".L_tplWhMenu");
const TplMenu3 tplMainMenu = {{"FiLE", "SiZE", "QUiT"}};
const TplMenu2 tplSizeMenu = {{"WxH", "ORG"}};
const TplMenu2 tplWhMenu = {{"FullScrn", "Texture"}};

// menu list printers; inlined. The sizeMenu call wants the `tbl[i]` giv form (its init then comes
// from loop.c, after the PRE'd "%s" high part) while the main-menu call is only reversed with the
// `*tbl++` biv form (with `tbl[i]` gcse copy-propagates the pointer-flagged `&menu` pseudo into the
// giv and maybe_eliminate_biv replaces the counter): two helpers, one per form (the "%s" literal is
// shared).
static inline void dispList(int x, int y, const char** tbl, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        eprintf(x, y + i * 14, 0, 0, "%s", *tbl++);
    }
}

static inline void dispListI(int x, int y, const char** tbl, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        eprintf(x, y + i * 14, 0, 0, "%s", tbl[i]);
    }
}

void TplViewer()
{
    char names[8][256];
    TplMenu3 menu = tplMainMenu;
    char name[256];
    TplMenu2 sizeMenu = tplSizeMenu;
    TplMenu2 whMenu = tplWhMenu;
    JOY* joy = &Joy[0];
    TplViewWork* wk = &tplWork;
    int i;

    if (wk->step > 0) {
        if (joy->trg & 0x10) {
            u8 mode = 0;

            if (pG->debug_mode == 0) {
                mode = wk->debugBak;
            }
            pG->debug_mode = mode;
        }
        if (joy->trg & 0x60) {
            if (joy->trg & 0x40) {
                wk->fileCur--;
            }
            if (joy->trg & 0x20) {
                wk->fileCur++;
            }
            wk->fileCur = wk->fileCur < 0 ? wk->fileNum - 1 : (wk->fileCur > wk->fileNum - 1 ? 0 : wk->fileCur);
            wk->step = 3;
            wk->cursor = 0;
            wk->reload = 1;
        }
        if (joy->trg & 0x1000) {
            wk->wide = (wk->wide == 0);
            switch (wk->wide) {
            case 0:
                wk->scrW = 0x200;
                wk->scrH = 0x1C0;
                ScreenReSize(0x200, 0x1C0);
                break;
            case 1:
                wk->scrW = 0x280;
                wk->scrH = 0x1C0;
                ScreenReSize(0x280, 0x1C0);
                break;
            }
            wk->w = wk->scrW;
            wk->h = wk->scrH;
        }
    }
    switch (wk->step) {
    case 0:
        wk->debugBak = pG->debug_mode;
        for (i = 0; i < 8; i++) {
            getTplname(name, i);
            if (HDReadMemAlloc(name, &pTplBuf)) {
                strcpy(names[wk->fileNum++], name);
            }
            if (pTplBuf) {
                Mem_free(pTplBuf);
                pTplBuf = 0;
            }
        }
        if (wk->fileNum != 0) {
            wk->step++;
        } else {
            wk->step = 2;
        }
        wk->scrW = 0x200;
        wk->scrH = 0x1C0;
        wk->wide = 0;
        break;
    case 1:
        if (joy->trg & 8) {
            wk->cursor--;
        }
        if (joy->trg & 4) {
            wk->cursor++;
        }
        wk->cursor = wk->cursor < 0 ? 0 : (wk->cursor > 2 ? 2 : wk->cursor);
        if (joy->trg & 0x200) {
            wk->cursor = 2;
        }
        if (joy->trg & 0x100) {
            wk->step = 3;
        }
        break;
    case 2:
        ToolWorkPop(0);
        if (pTplBuf) {
            Mem_free(pTplBuf);
            pTplBuf = 0;
        }
        TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x80000000;
        pG->debug_mode = wk->debugBak;
        memclr_asm(wk, sizeof(TplViewWork));
        TaskExit();
        break;
    case 3:
        switch (wk->cursor) {
        case 0:
            if (joy->rep & 8) {
                wk->fileCur--;
            }
            if (joy->rep & 4) {
                wk->fileCur++;
            }
            wk->fileCur = wk->fileCur < 0 ? wk->fileNum - 1 : (wk->fileCur > wk->fileNum - 1 ? 0 : wk->fileCur);
            if (joy->trg & 0x200) {
                wk->step = 1;
            } else {
                if ((joy->trg & 0x100) || wk->reload) {
                    wk->reload = 0;
                    if (pTplBuf) {
                        Mem_free(pTplBuf);
                    }
                    HDReadMemAlloc(names[wk->fileCur], &pTplBuf);
                    wk->y = wk->x = 0;
                    wk->w = wk->scrW;
                    wk->h = wk->scrH;
                    wk->step = 1;
                }
                for (i = 0; i < wk->fileNum; i++) {
                    eprintf(0x60, 0x46 + i * 14, 0, 0, "%s", names[i]);
                }
                eprintf(0x58, (wk->fileCur + 5) * 14, 0, 0, ">");
            }
            break;
        case 1:
            if (pTplBuf == 0) {
                wk->step = 1;
                break;
            }
            if (joy->rep & 8) {
                wk->sizeCur--;
            }
            if (joy->rep & 4) {
                wk->sizeCur++;
            }
            wk->sizeCur = wk->sizeCur < 0 ? 0 : (wk->sizeCur > 2 ? 2 : wk->sizeCur);
            if (joy->trg & 0x200) {
                wk->step = 1;
            } else {
                dispListI(0x60, 0x46, sizeMenu.s, 2);
                eprintf(0x58, (wk->sizeCur + 5) * 14, 0, 0, ">");
                switch (wk->sizeCur) {
                case 0:
                    if (joy->rep & 1) {
                        wk->whCur--;
                    }
                    if (joy->rep & 2) {
                        wk->whCur++;
                    }
                    wk->whCur = wk->whCur < 0 ? 0 : (wk->whCur > 1 ? 1 : wk->whCur);
                    for (i = 0; i < 2; i++) {
                        int col = 7;

                        if (i == wk->whCur) {
                            col = 0;
                        }

                        eprintf(0x90 + i * 0x48, (wk->sizeCur + 5) * 14, col, 0, "%s", whMenu.s[i]);
                    }
                    if (joy->trg & 0x100) {
                        switch (wk->whCur) {
                        case 0:
                            wk->w = wk->scrW;
                            wk->h = wk->scrH;
                            break;
                        case 1:
                            wk->w = ((TplFile*) pTplBuf)->desc.textureHeader->width;
                            wk->h = ((TplFile*) pTplBuf)->desc.textureHeader->height;
                            break;
                        case 2:
                            break;
                        }
                    }
                    break;
                case 1: {
                    int dx = 0;
                    int dy = 0;

                    if (joy->rep & 0x10000) {
                        dx = -1;
                    }
                    if (joy->rep & 0x20000) {
                        dx = 1;
                    }
                    if (joy->rep & 0x80000) {
                        dy = -1;
                    }
                    if (joy->rep & 0x40000) {
                        dy = 1;
                    }
                    if (joy->on & 0x100) {
                        dx *= 10;
                        dy *= 10;
                    }
                    wk->x += dx;
                    wk->y += dy;
                    break;
                }
                }
            }
            break;
        case 2:
            wk->step = 2;
            break;
        }
        break;
    }
    dispList(0x28, 0x46, menu.s, 3);
    eprintf(0x20, (wk->cursor + 5) * 14, 0, 0, ">");
    if (pTplBuf) {
        DrawTpl((TEXPalette*) pTplBuf, wk->x, wk->y, wk->w, wk->h);
        eprintf(0x168, 0x15E, 0, 0, "X: %4d, W: %4d", wk->x, wk->w);
        eprintf(0x168, 0x16C, 0, 0, "Y: %4d, H: %4d", wk->y, wk->h);
    }
}

asm(".section .data; .balign 8");
