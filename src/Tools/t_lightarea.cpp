#include "types.h"
#include "light.h"
#include "atari.h"
#include "dbg_tool.h"
#include "area.h"
#include "db_cam.h"
#include "block.h"
#include "scheduler.h"
#include "dbmodule.h"
#include "t_util.h"
#include "em.h"
#include "st_mgr_event.h"

// Light area editor (Tools/t_lightarea.cpp): a cDbgToolMain<LIGHT_AREA> over the room's 32 light
// areas (.sar files, game/light_area.cpp's LightAreaData). The works and the file image live on the
// debug heap; every frame the image is rebuilt and handed to LightAreaDataLoad so the game shows it.

extern "C" {
void* memset(void* dst, int c, unsigned int n);
// game/sub2.cpp; really takes Vec*, declared by value here (same ABI) so the caller copies its Vec.
int GetScreenPos(Vec pos, Vec* scr);
int LightAreaDataLoad(void* data);
void LightAreaUpdate();
}

class cPlayer;
extern cPlayer* pPL;
// the player as its cModel base (player.h would bring math_sub.h's pointer GetScreenPos)
static inline cModel* PlModel() { return (cModel*) pPL; }

// The unit's own debug-heap new/delete (the module's copies: every Tools unit's `new` resolves here).
void* __builtin_new(unsigned int size)
{
    return Debug_alloc(size, 1);
}

void* __builtin_vec_new(unsigned int size)
{
    return Debug_alloc(size, 1);
}

void __builtin_delete(void* p)
{
    Debug_free(p);
}

struct LIGHT_AREA {
    u8 no;           // 0x00
    u8 flags;        // 0x01  bit 0: in use
    u8 lightNoPl;    // 0x02  light no for the player (0xFF: none)
    u8 lightNoEm;    // 0x03  light no for the other characters
    AreaData area;   // 0x04
    u8 x34[4];
    s8 power;        // 0x38  scale in percent (0..100)
    u8 lightNoSub;   // 0x39  light no for the sub character
    u8 x3A[0xD8 - 0x3A];
};

#define LIGHT_AREA_MAX 32

namespace t_lightarea_namespace {

static LIGHT_AREA* light_area_work;      // LIGHT_AREA_MAX works
static DbgToolFileHeader* light_area_buf;  // the file image fed to the game

int IsWorkAlive(LIGHT_AREA* w)
{
    if (w->flags & 1) {
        return 1;
    }
    return 0;
}

void SetWorkAlive(LIGHT_AREA* w, int alive)
{
    if (alive == 1) {
        w->flags |= 1;
    } else {
        w->flags &= ~1;
    }
}

int GetWorkNo(LIGHT_AREA* w)
{
    return w->no;
}

void SetWorkNo(LIGHT_AREA* w, int no)
{
    w->no = no;
}

void InitWork(LIGHT_AREA* w, int no)
{
    memclr_asm(w, sizeof(LIGHT_AREA));
    w->no = no;
    AreaDataInit(&w->area, &PlModel()->pos, 1, 7000.0f, 5000.0f);
}

int PosExec_callback(int no, LIGHT_AREA* w, cDbgButtonTemplate<LIGHT_AREA>* b)
{
    AreaData* a = &w->area;

    AreaDataEdit(a, 0xA0FF8080, 1, 0, 2.3f);
    AreaDataInfoDisp(a, 0x28, 0x18);
    AreaDataHelpDisp(a, 0x108, 8);
    {
        u32 t = Joy[0].trg & 0x200;
        return t == 0;
    }
}

void PosUpdate_callback(int no, LIGHT_AREA* w, cDbgButtonTemplate<LIGHT_AREA>* b)
{
    char buf[64];
    Vec pos = {0.0f, 0.0f, 0.0f};
    f32 h = 0.0f;

    if (IsWorkAlive(w)) {
        AreaGetCenterPos(&pos, &w->area);
        PSVECScale(&pos, &pos, 0.001f);
        h = w->area.u.xz4.h / 1000.0f;
    }
    sprintf(buf, "%6.1f %6.1f %6.1f %6.1f", pos.x, pos.y, pos.z, h);
    DbgButtonSetName(b, buf);
}

int AreaNoExec_callback(int no, LIGHT_AREA* w, cDbgButtonTemplate<LIGHT_AREA>* b)
{
    static int cursor = 0;
    int step = 0;
    u32 rep;
    JOY* joy;

    // 0xFF: no light
    eprintf(0xAA, 0xA0, 4, 0, "PL NO : ");
    if (w->lightNoPl != 0xFF) {
        eprintf(0xAA, 0xA0, 0, 0, "       %3d", w->lightNoPl);
    } else {
        eprintf(0xAA, 0xA0, 0, 0, "       OFF");
    }
    eprintf(0xAA, 0xB0, 4, 0, "EM NO : ");
    if (w->lightNoEm != 0xFF) {
        eprintf(0xAA, 0xB0, 0, 0, "       %3d", w->lightNoEm);
    } else {
        eprintf(0xAA, 0xB0, 0, 0, "       OFF");
    }
    eprintf(0xAA, 0xC0, 4, 0, "SUB NO: ");
    if (w->lightNoSub != 0xFF) {
        eprintf(0xAA, 0xC0, 0, 0, "       %3d", w->lightNoSub);
    } else {
        eprintf(0xAA, 0xC0, 0, 0, "       OFF");
    }
    eprintf(0xAA, 0xD0, 4, 0, "POWER : ");
    eprintf(0xAA, 0xD0, 0, 0, "       %3d %", w->power);
    if (pG->flags_51E4 & 7) {
        eprintf(0x9A, (cursor + 10) * 16, 0, 0, cDbgStr::cursor());
    }
    // one pad pointer for the four cases (the last one is past cse's jump-following path length), taken
    // here so that it does not live across the eprintf calls
    joy = &Joy[0];
    rep = joy->rep;
    if (rep & 0x80008) {
        cursor--;
    }
    if (rep & 0x40004) {
        cursor++;
    }
    if (cursor < 0) {
        cursor = 3;
    }
    if (cursor > 3) {
        cursor = 0;
    }
    switch (cursor) {
    case 0:
        if (rep & 0x10001) {
            step = -1;
        }
        if (rep & 0x20002) {
            step = 1;
        }
        if (joy->on & 0x100) {
            step *= 10;
        }
        w->lightNoPl += step;
        if ((joy->on & 0x800) && (joy->trg & 0x100)) {
            w->lightNoPl = 0;
        }
        break;
    case 1:
        if (rep & 0x10001) {
            step = -1;
        }
        if (rep & 0x20002) {
            step = 1;
        }
        if (joy->on & 0x100) {
            step *= 10;
        }
        w->lightNoEm += step;
        if ((joy->on & 0x800) && (joy->trg & 0x100)) {
            w->lightNoEm = 0;
        }
        break;
    case 2:
        if (rep & 0x10001) {
            step = -1;
        }
        if (rep & 0x20002) {
            step = 1;
        }
        if (joy->on & 0x100) {
            step *= 10;
        }
        w->lightNoSub += step;
        if ((joy->on & 0x800) && (joy->trg & 0x100)) {
            w->lightNoSub = 0;
        }
        break;
    case 3:
        if (rep & 0x10001) {
            step = -1;
        }
        if (rep & 0x20002) {
            step = 1;
        }
        if (joy->on & 0x100) {
            step *= 10;
        }
        w->power += step;
        if (w->power < 0) {
            w->power = 0;
        }
        if (w->power > 100) {
            w->power = 100;
        }
        if ((joy->on & 0x800) && (joy->trg & 0x100)) {
            w->power = 0;
        }
        break;
    }
    {
        u32 t = Joy[0].trg & 0x200;
        return t == 0;
    }
}

// "hPP/EE/SS/WW": the four numbers of the work in two digits each (the hundreds digit of the first
// goes in front, the others' land on the separators, as in the original)
void AreaNoUpdate_callback(int no, LIGHT_AREA* w, cDbgButtonTemplate<LIGHT_AREA>* b)
{
    static char digits[] = "0123456789";
    u32 n;
    char buf[13];

    n = w->lightNoPl;
    if (n == 0xFF) {
        buf[0] = ' ';
        buf[1] = 'x';
        buf[2] = 'x';
        buf[3] = '/';
    } else {
        if (n <= 99) {
            buf[0] = ' ';
        } else {
            buf[0] = digits[n / 100];
            n %= 100;
        }
        buf[3] = '/';
        buf[1] = digits[n / 10];
        buf[2] = digits[n % 10];
    }
    n = w->lightNoEm;
    if (n == 0xFF) {
        buf[4] = 'x';
        buf[5] = 'x';
        buf[6] = '/';
    } else {
        if (n > 99) {
            buf[3] = digits[n / 100];
            n %= 100;
        }
        buf[6] = '/';
        buf[4] = digits[n / 10];
        buf[5] = digits[n % 10];
    }
    n = w->lightNoSub;
    if (n == 0xFF) {
        buf[7] = 'x';
        buf[8] = 'x';
        buf[9] = '/';
    } else {
        if (n > 99) {
            buf[3] = digits[n / 100];
            n %= 100;
        }
        buf[9] = '/';
        buf[7] = digits[n / 10];
        buf[8] = digits[n % 10];
    }
    n = w->power;
    if (n > 99) {
        buf[6] = digits[n / 100];
        n %= 100;
    }
    buf[12] = 0;
    buf[10] = digits[n / 10];
    buf[11] = digits[n % 10];
    DbgButtonSetName(b, buf);
}

void OptionExec()
{
    static int cursor = 0;
    u32 rep;

    eprintf(0xAA, 0xA0, 4, 0, "FOG : ");
    if (pG->flags_58 & 0x4000) {
        eprintf(0xAA, 0xA0, 0, 0, "       ON");
    } else {
        eprintf(0xAA, 0xA0, 0, 0, "       OFF");
    }
    if (pG->flags_51E4 & 7) {
        eprintf(0x9A, (cursor + 10) * 16, 0, 0, cDbgStr::cursor());
    }
    rep = Joy[0].rep;
    if (rep & 0x80008) {
        cursor--;
    }
    if (rep & 0x40004) {
        cursor++;
    }
    if (cursor < 0) {
        cursor = 1;
    }
    if (cursor > 0) {
        cursor = 0;
    }
    switch (cursor) {
    case 0:
        if ((rep & 0x30003) || (Joy[0].trg & 0x100)) {
            if (pG->flags_58 & 0x4000) {
                pG->flags_58 &= ~0x4000;
            } else {
                pG->flags_58 |= 0x4000;
            }
        }
        break;
    }
}

void tLightAreaInit();
void tLightAreaExit();

void ToolLightAreaMain()
{
    // declaration order matters: the first zero-initialised local is the zero register of the tool's
    // constructor stores (preview here, `wait` in t_esp_area), the later ones spill in this order
    int preview = 0;
    cDbgToolMain<LIGHT_AREA> tool;
    char path1[64];
    char path2[32];
    char buf[128];
    u8 cnt = 0;
    int cam = 0;
    u8 wait = 0;
    int plNoHit = 0;
    u32 i;

    if (pG->flags_54 & 0x800) {
        plNoHit = 1;
    }
    TutilInitDefault();
    tLightAreaInit();
    light_area_work = (LIGHT_AREA*) Debug_alloc(sizeof(LIGHT_AREA) * LIGHT_AREA_MAX, 1);
    light_area_buf = (DbgToolFileHeader*) Debug_alloc(sizeof(LIGHT_AREA) * LIGHT_AREA_MAX + sizeof(DbgToolFileHeader), 0);
    tool.CreateMenuWindow();
    sprintf(path1, "X:\\Soft\\Room\\st%d\\r%03x\\", pG->stage_no, pG->room_id);
    sprintf(path2, "r%03x", pG->room_id);
    tool.CreateFileWindows(0x16, 0xA, path1, path2, ".sar");
    tool.CreateEditWindow(4, 0x19, light_area_work, "No ========Position=========== =Data========", 5,
                          LIGHT_AREA_MAX);
    tool.AddEditColumn(3, "                          ", 1, PosExec_callback, PosUpdate_callback);
    tool.AddEditColumn(0x20, "             ", 2, AreaNoExec_callback, AreaNoUpdate_callback);
    tool.SetIsWorkAliveFunc(IsWorkAlive);
    tool.SetSetWorkAliveFunc(SetWorkAlive);
    tool.SetGetWorkNoFunc(GetWorkNo);
    tool.SetSetWorkNoFunc(SetWorkNo);
    tool.SetInitWorkFunc(InitWork);
    tool.InitAllWork();

    // the room's default file
    sprintf(buf, "%s%s00.sar", path1, path2);
    tool.LoadData(buf, light_area_work, LIGHT_AREA_MAX);

    for (;;) {
        LIGHT_AREA* w;
        Vec pos;
        Vec scr;

        if (Joy[0].trg & 0x1000) {
            cam ^= 1;
        }
        if (preview) {
            int c = cnt;

            cnt = (u8) (c + 1);
            if (c & 8) {
                eprintf2(0xE, 0x12, 0xAA, 0x18, 6, 0, "PREVIEW MODE");
            } else if (pG->flags_68 & 8) {
                eprintf(0xF0, 0x30, 2, 0, "PL NOHIT");
            }
            if (Joy[0].trg & 0x400) {
                if (pG->flags_68 & 8) {
                    BitOff(pG->flags_68, 8);
                } else {
                    BitOn(pG->flags_68, 8);
                }
            }
            if (!(pG->flags_170 & 0x10000000) && (Joy[0].trg & 0x10)) {
                // back to the editor: the game's own light areas again
                BitOn(pG->flags_60, 0x10000000);
                BitOn(pG->flags_170, 0x10000000);
                BitOn(pG->flags_170, 0x20000000);
                TaskSleep(10);
                if (plNoHit == 0) {
                    BitOff(pG->flags_54, 0x800);
                }
                preview ^= 1;
            } else if (!(Joy[0].on & 0x10)) {
                BitOff(pG->flags_170, 0x10000000);
                BitOff(pG->flags_170, 0x20000000);
            }
        } else {
            BitOn(pG->flags_170, 0x10000000);
            BitOn(pG->flags_170, 0x20000000);
            BitOn(pG->flags_68, 0x00800000);
            w = light_area_work;
            for (i = 0; i < LIGHT_AREA_MAX; i++, w++) {
                    if (IsWorkAlive(w)) {
                    AreaGetCenterPos(&pos, &w->area);
                    pos.y = (pos.y + w->area.u.xz4.h) * 0.5f;
                    if (GetScreenPos(pos, &scr) == 1) {
                        if (i == tool.GetEdit()->GetCurrentNo()) {
                            AreaDataDisp(&w->area, 0xA0FF8080, 0, 0);
                            eprintf2(8, 12, (int) scr.x + 8, (int) scr.y + 0x10, 6, 0, "%d", w->lightNoPl);
                        } else {
                            AreaDataDisp(&w->area, 0x60808080, 1, 0);
                            eprintf2(8, 12, (int) scr.x + 8, (int) scr.y + 0x10, 0, 0, "%d", w->lightNoPl);
                        }
                    }
                }
            }
            if (cam) {
                int c = cnt;

                CamDbg.move(&pG->Cam, &Joy[0], 1);
                cnt = (u8) (c + 1);
                if (c & 8) {
                    eprintf2(0xE, 0x12, 0xAA, 0x18, 6, 0, "CAMERA MODE");
                }
            } else {
                if (tool.Update() == 0) {
                    break;
                }
                if (tool.GetMode() == 4) {
                    if (wait == 0) {
                        OptionExec();
                    } else {
                        wait--;
                    }
                } else if (tool.GetMode() == 2) {
                    // separate compares: `!= 2 && != 3` would fold into a subi/cmplwi range test
                } else if (tool.GetMode() == 3) {
                } else {
                    wait = 1;
                }
                tool.Disp();
                // the edited areas as the game sees them
                tool.MakeSaveData(light_area_buf, light_area_work, LIGHT_AREA_MAX);
                LightAreaDataLoad(light_area_buf);
                LightAreaUpdate();
                if (Joy[0].trg & 0x10) {
                    // preview: play with the edited areas
                    preview ^= 1;
                    ((cUnitEventView*) pPL)->endEvent(0);
                    BitOff(pG->flags_60, 0x10000000);
                    BitOn(pG->flags_54, 0x800);
                }
            }
        }
        TaskSleep(1);
    }
    BitOff(pG->flags_60, 0x10000000);
    tLightAreaExit();
    TutilQuitDefault();
    TaskExit();
}

void tLightAreaInit()
{
    BitOn(pG->flags_170, 0x20000000);
    BitOn(pG->flags_170, 0x10000000);
    BitOn(pG->flags_170, 0x08000000);
    BitOn(pG->flags_170, 0x00800000);
    BitOn(pG->flags_170, 0x00400000);
    BitOn(pG->flags_170, 0x00010000);
    BitOn(pG->flags_170, 0x00002000);
    BitOn(pG->flags_60, 0x10000000);
    CamDbg.target_type = 4;
    Block.dispAllBlock(1);
}

void tLightAreaExit()
{
    BitOff(pG->flags_170, 0x20000000);
    BitOff(pG->flags_170, 0x10000000);
    BitOff(pG->flags_170, 0x08000000);
    BitOff(pG->flags_170, 0x00800000);
    BitOff(pG->flags_170, 0x00400000);
    BitOff(pG->flags_170, 0x00010000);
    BitOff(pG->flags_170, 0x00002000);
    BitOff(pG->flags_60, 0x10000000);
    {
        // through a volatile pointer: the store keeps `&CamDbg` in a register (`stb 0xf(rX)`)
        volatile debugCamera* c = &CamDbg;
        c->target_type = 0;
    }
    Block.dispAllBlock(0);
}

} // namespace t_lightarea_namespace

void ToolLightArea()
{
    t_lightarea_namespace::ToolLightAreaMain();
}
