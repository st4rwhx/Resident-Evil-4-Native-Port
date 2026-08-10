// game/emwindow.cpp: window / fence enemy (cEmWindow): breakable room windows the player and
// the enemies jump through (ExeWindowEvent), with a scenario field (SceAtCreateFieldAt) on
// each side.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "event.h"
#include "emwindow.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "sce.h"
#include "player.h"
#include "pl_wep.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

// One row of WindowData (0x48 bytes), indexed by cModel::type.
struct WindowDataRow {
    int field;        // 0x00  1: scenario field / collision quad on each side
    int hpType;       // 0x04  0: any group-0 weapon breaks it, 1: -500, 2: -250
    int breakEff;     // 0x08  1: effect 8 when the window starts broken
    int breakEff2;    // 0x0C  1: effect 9 when the model changes
    char bin[12];     // 0x10  broken model ("" = hide the model)
    char tpl[12];     // 0x1C
    f32 sizeX;        // 0x28
    f32 sizeY;        // 0x2C
    f32 sizeZ;        // 0x30
    int lowSat;       // 0x34  1: the collision quad sits half way down
    int satType;      // 0x38  1: scenario quad 0x40 / effect quad 0x400000, else the cube form
    int frame;        // 0x3C  1: 200 units of frame around the glass
    int amb;          // 0x40  EtcSetAddAmb argument
    u8 type2;         // 0x44  cModel::x12F
    u8 pad_45[3];
};

// Field info returned by SceAtCheckFieldInfo.
struct SceAtFieldInfo {
    int id;
    cEmWindow* pWindow;
};

extern "C" {
void EtcSetAddAmb(cModel* m, int a);                                                         // EtcModel.cpp
void EffectEspDelete(int a, int b, cModel* m, int c);                                        // est.cpp
void EffectEspgenDelete(int a, int b, cModel* m);
void EffectEfmDelete(int a, int b, cModel* m);
void EmDmBloodSet3(cEm* em, int a, int type, int b, int c, int d);                           // em_sub.cpp
int LadderNearCk(Vec* pos);                                                                  // obj13.cpp
void LadderEventTrans(int on);
SceAtFieldInfo* SceAtCheckFieldInfo(Vec* pos);                                               // sce_at.cpp
int SceAtCreateFieldAt(cModel* m, Vec* pt, int a, int b, int c, f32 r, int d, f32 ang, int e, f32 w, int f, void* out);
int MotionMove(cModel* m, int a);
}
void MotionSetCore(cModel* m, void* w, void* data, int seq, int hokan, int flags, int frame);   // motion.cpp (C++ linkage)

// cUnit::beginEvent takes an int in the original (sscrn BEGIN_EVENT).
class cUnitEvent {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEvent();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};
#define BEGIN_EVENT(p, mode) ((cUnitEvent*) (p))->beginEvent(mode)

WindowDataRow WindowData[29] = {
    { 1, 1, 0, 0, "et0001.bin", "et0000.tpl", 1300.0f, 1400.0f, 1.0f, 0, 1, 1, 6, 1 },
    { 1, 3, 0, 0, "et0701.bin", "et0700.tpl", 1300.0f, 1400.0f, 1.0f, 0, 1, 1, 6, 0 },
    { 0, 1, 0, 0, "et1d01.bin", "et1d00.tpl", 2200.0f, 3800.0f, 1.0f, 0, 1, 1, 6, 1 },
    { 0, 0, 1, 0, "", "", 800.0f, 1010.0f, 1.0f, 1, 1, 1, 6, 1 },
    { 1, 2, 1, 0, "et2901.bin", "et2900.tpl", 2800.0f, 1400.0f, 1.0f, 1, 1, 1, 6, 1 },
    { 0, 1, 0, 0, "et2c01.bin", "et2c00.tpl", 1000.0f, 650.0f, 1.0f, 0, 1, 1, 6, 1 },
    { 0, 0, 1, 0, "et3501.bin", "et3500.tpl", 2400.0f, 1500.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 0, 1, 0, "et3601.bin", "et3600.tpl", 1400.0f, 1500.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 0, 0, 0, "et4401.bin", "et4401.tpl", 2600.0f, 200.0f, 1.0f, 1, 1, 0, 6, 0 },
    { 0, 0, 1, 0, "", "", 1500.0f, 700.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 1, 2, 1, 0, "et4a01.bin", "et4a00.tpl", 2800.0f, 1400.0f, 1.0f, 1, 1, 1, 6, 1 },
    { 0, 1, 1, 0, "et5001.bin", "et5000.tpl", 1100.0f, 1100.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 1, 1, 0, "et5101.bin", "et5101.tpl", 800.0f, 900.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 0, 1, 0, "et5201.bin", "et5201.tpl", 1000.0f, 1400.0f, 1000.0f, 1, 0, 0, 6, 1 },
    { 0, 0, 1, 0, "et5301.bin", "et5301.tpl", 400.0f, 2000.0f, 400.0f, 1, 0, 0, 6, 1 },
    { 0, 0, 1, 1, "", "", 570.0f, 480.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 1, 0, 0, "et5501.bin", "et5501.tpl", 3600.0f, 2000.0f, 1.0f, 1, 1, 0, 7, 1 },
    { 0, 1, 0, 0, "et5601.bin", "et5601.tpl", 1500.0f, 1500.0f, 1.0f, 1, 1, 0, 7, 1 },
    { 0, 1, 0, 0, "et5701.bin", "et5701.tpl", 1500.0f, 1500.0f, 1.0f, 1, 1, 0, 7, 1 },
    { 0, 1, 0, 0, "et5801.bin", "et5801.tpl", 1200.0f, 1400.0f, 1.0f, 1, 1, 0, 14, 0 },
    { 0, 1, 0, 0, "et5a01.bin", "et5a01.tpl", 1200.0f, 1400.0f, 1.0f, 1, 1, 0, 15, 0 },
    { 0, 1, 0, 0, "et5b01.bin", "et5b01.tpl", 650.0f, 1100.0f, 1.0f, 1, 1, 0, 15, 1 },
    { 0, 1, 0, 0, "et5c01.bin", "et5c01.tpl", 650.0f, 1100.0f, 1.0f, 1, 1, 0, 15, 1 },
    { 0, 1, 0, 0, "et5d01.bin", "et5d01.tpl", 1700.0f, 1000.0f, 1.0f, 1, 1, 0, 16, 0 },
    { 0, 1, 0, 0, "et5e01.bin", "et5e01.tpl", 1700.0f, 1000.0f, 1.0f, 1, 1, 0, 16, 0 },
    { 0, 1, 0, 0, "et5f01.bin", "et5f01.tpl", 2800.0f, 1600.0f, 1.0f, 1, 1, 0, 15, 0 },
    { 0, 1, 0, 0, "et6001.bin", "et6001.tpl", 2800.0f, 1600.0f, 1.0f, 1, 1, 0, 15, 0 },
    { 0, 0, 1, 1, "", "", 570.0f, 480.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 0, 1, 1, "", "", 400.0f, 250.0f, 1.0f, 1, 1, 0, 6, 1 },
};

// Not broken yet (etc flag bit0 clear).
static inline int WindowAlive(cEmWindow* em)
{
    return !(em->ChkStatus() & 1);
}

// Bit `no` of a u32 bit table (bit 0 = the top bit of the first word).
static inline void TblBitOn(u32* tbl, u32 no)
{
    tbl[no >> 5] |= 0x80000000 >> (no & 0x1F);
}

static inline void TblBitOff(u32* tbl, u32 no)
{
    tbl[no >> 5] &= ~(0x80000000 >> (no & 0x1F));
}

cEmWindow* SetWindow(void* bin, void* tpl, Vec* pos, Vec* rot, int type, u8 etcNo, void* arc)
{
    cEmWindow* em;

    em = (cEmWindow*) EmMgr.create(0x46);
    if (em == 0) {
        pLog->err(0, 0, "SetWindow():create failed");
        return 0;
    }
    if (em->init(bin, tpl, pos, rot, type, etcNo, arc) == 0) {
        return 0;
    }
    return em;
}

int ChkWindow(cModel* m, Vec* a, Vec* b, int id, u16* status, Vec* dir, Vec* pos, cEmWindow** out)
{
    SceAtFieldInfo* info;
    cEmWindow* win;

    if (out) {
        *out = 0;
    }
    if (m == 0) {
        pLog->err(0, 0, "SceAtCheck : param error");
        return 0;
    }
    info = SceAtCheckFieldInfo(b);
    if (info == 0) {
        return 0;
    }
    if (info->id != id) {
        return 0;
    }
    win = info->pWindow;
    if (win == 0) {
        pLog->err(0, 0, "SceAtCheck : failed");
        return 0;
    }
    if (win->id != 0x46) {
        return 0;
    }
    if (m->id <= 0xF) {
        if (win->ChkEnableFence(1) == 0) {
            return 0;
        }
    } else {
        if (win->ChkEnableFence(2) == 0) {
            return 0;
        }
    }
    if (m->id == 0) {
        if (WindowAlive(win) && win->type == 1) {
            return 0;
        }
    }
    if (m->id != 3 && m->id <= 0xF) {
        if (LadderNearCk(&win->pos) == 0) {
            return 0;
        }
    }
    if (Front_check(win, a, PI / 2) == 0 && Front_check(win, b, PI / 2) == 1) {
        dir->x = 0.0f;
        dir->y = 0.0f;
        dir->z = -1.0f;
        RotVector(dir, &win->rot, dir);
        pos->x = win->pos.x;
        pos->y = win->pos.y;
        pos->z = win->pos.z;
        if (out) {
            *out = win;
        }
        *status = win->ChkStatus();
        return 1;
    }
    if (Front_check(win, a, PI / 2) == 1 && Front_check(win, b, PI / 2) == 0) {
        dir->x = 0.0f;
        dir->y = 0.0f;
        dir->z = 1.0f;
        RotVector(dir, &win->rot, dir);
        pos->x = win->pos.x;
        pos->y = win->pos.y;
        pos->z = win->pos.z;
        if (out) {
            *out = win;
        }
        *status = win->ChkStatus();
        return 1;
    }
    return 0;
}

int cEmWindow::init(void* bin, void* tpl, Vec* pos_, Vec* rot_, int type_, u8 etcNo, void* arc)
{
    EmWindowWork* w = EMWINDOW_WK(this);
    Vec pt[4];
    Vec size;
    Vec satPos;
    void* out;
    f32 frame;
    int cube;
    int no;

    if (modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetWindow():modelInit failed");
        EmMgr.destroy(this);
        return 0;
    }
    EmObjInit();
    setEtc(etcNo);
    w->arc = arc;
    if (pos_) {
        pos = *pos_;
    }
    if (rot_) {
        rot = *rot_;
    }
    RotMatrix(worldMat, &rot);
    TransMatrix(worldMat, &pos);
    ScaleMatrix(worldMat, &scale);
    PSMTXCopy(worldMat, mat);
    type = type_;
    if (WindowData[type_].field == 0) {
        SetEnableFence(0, 0);
    }
    EtcSetAddAmb(this, WindowData[type].amb);
    switch (type) {
    case 0xD:
    case 0xE:
        pInfo->xD6 = 2;
        break;
    }
    {
        static const Vec zero = { 0.0f, 0.0f, 0.0f };
        static const Vec lsize = { 2000.0f, 2000.0f, 2000.0f };

        lightInfo.init2(0, 1, &zero, &lsize, 0x10);
    }
    if (WindowData[type].frame == 1) {
        frame = 200.0f;
    } else {
        frame = 0.0f;
    }
    size.x = (WindowData[type].sizeX + frame * 2.0f) * 0.5f;
    size.y = WindowData[type].sizeY + frame * 2.0f;
    size.z = WindowData[type].sizeZ * 0.5f;
    satPos.x = 0.0f;
    satPos.z = 0.0f;
    if (WindowData[type].lowSat == 1) {
        satPos.y = -(WindowData[type].sizeY * 0.5f) - frame;
    } else {
        satPos.y = -frame;
    }
    if (WindowData[type].satType == 1) {
        cube = 0;
        if (WindowData[type].field == 1) {
            setSat(&satPos, 0x40, 0, 0, size.x, size.y, size.z);
        }
        setEat(&satPos, 0x400000, 0, 0, size.x, size.y, size.z);
    } else {
        cube = 1;
        if (WindowData[type].field == 1) {
            setSat(&satPos, 0x40, 0, 1, size.x, size.y, size.z);
        }
        setEat(&satPos, 0, 0, 1, size.x * 0.7f, size.y, size.z * 0.7f);
    }
    size.z = (WindowData[type].sizeZ + 50.0f) * 0.5f;
    setYarare(0, &satPos, 0x21, cube, size.x, size.y, size.z);
    be_flag &= ~0x01000000;
    hpMax = hp = 1000;
    setStatus(1);
    setStatus(0xB);
    atari.setPriority(3);
    atari.throughOn();
    lockParts = 0;
    lockOfs.x = 0.0f;
    lockOfs.y = 0.0f;
    lockOfs.z = 0.0f;
    be_flag &= ~0x10;
    x12F = WindowData[type].type2;
    setNoSuspend(1);
    if (WindowData[type].field == 1) {
        CalFloor();
        pt[0].x = 500.0f;
        pt[0].y = -2000.0f;
        pt[0].z = -1000.0f;
        pt[1].x = 500.0f;
        pt[1].y = -2000.0f;
        pt[1].z = 1000.0f;
        pt[2].x = -500.0f;
        pt[2].y = -2000.0f;
        pt[2].z = 1000.0f;
        pt[3].x = -500.0f;
        pt[3].y = -2000.0f;
        pt[3].z = -1000.0f;
        no = SceAtCreateFieldAt(this, pt, 3, 0, 0, 3000.0f, 5, 0.0f, 0, 1.3962635f, 1, &out);
        if (no == -1) {
            pLog->err(0, 0, "move : SceAt no create");
        }
        w->fieldAt[0] = no;
        pt[0].x = 500.0f;
        pt[0].y = -2000.0f;
        pt[0].z = -1000.0f;
        pt[1].x = 500.0f;
        pt[1].y = -2000.0f;
        pt[1].z = 1000.0f;
        pt[2].x = -500.0f;
        pt[2].y = -2000.0f;
        pt[2].z = 1000.0f;
        pt[3].x = -500.0f;
        pt[3].y = -2000.0f;
        pt[3].z = -1000.0f;
        no = SceAtCreateFieldAt(this, pt, 3, 0, 0, 3000.0f, 5, -PI, 0, 1.3962635f, 1, &out);
        if (no == -1) {
            pLog->err(0, 0, "move : SceAt no create");
        }
        w->fieldAt[1] = no;
    }
    return 1;
}

void cEmWindow::move()
{
    EmWindowWork* w = EMWINDOW_WK(this);
    u8 eff;

    eff = getEff();
    DmCk();
    EmObjMove();
    switch (xFC) {
    case 0:
        w->rotBase.x = rot.x;
        w->rotBase.y = rot.y;
        w->rotBase.z = rot.z;
        xFC = 1;
        if (WindowAlive(this)) {
            if (WindowData[type].breakEff == 1) {
                EstSet(0, -1, &pos, &rot, eff, 8, 0x801, 0x31, (u32) this, 0);
            }
        }
        break;
    case 1:
        if (w->shake != 0) {
            rot.y = w->rotBase.y + (f32) (s8) ((s8) Rnd() % 3) * PI / 180.0f;
            w->shake--;
            if (w->shake <= 0) {
                rot.x = w->rotBase.x;
                rot.y = w->rotBase.y;
                rot.z = w->rotBase.z;
            }
        }
        break;
    case 2:
    case 3:
        break;
    case 4:
        MotionMove(this, 0);
        break;
    }
}

void cEmWindow::DmCk()
{
    int a = 0;
    int hit = 0;
    u8 eff;
    int dmg;
    u8 wep;

    eff = getEff();
    dmg = 0;
    if (hp > 0) {
        dmg = DmgMgr.hitCheck(&pos, 0);
        switch (dmg) {
        case 1:
        case 4:
        case 5:
        case 7:
            hit = 1;
            break;
        }
    }
    if (dmHit == 0 && hit == 0) {
        return;
    }
    wep = dmWep;
    dmHit = 0;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0x2A) {
        return;
    }
    if (wep == 0xE) {
        return;
    }
    switch (wep) {
    case 0xB:
    case 0xC:
    case 0x11:
        dmType = 0;
        break;
    }
    if (ChkEnableDamage() == 0) {
        return;
    }
    switch (dmg) {
    case 1:
    case 4:
    case 5:
    case 7:
        hp = 0;
        a = 0;
        break;
    }
    switch (GetWepSizeGroup(dmWep)) {
    case 0:
        if (WindowData[type].hpType == 0) {
            hp = 0;
        }
        if (WindowData[type].hpType == 1) {
            hp -= 500;
        }
        if (WindowData[type].hpType == 2) {
            hp -= 250;
        }
        a = 0;
        break;
    case 1:
        hp = 0;
        a = 1;
        break;
    case 2:
        hp = 0;
        a = 0;
        break;
    default:
        if (hit == 0) {
            return;
        }
        break;
    }
    if (hp <= 0) {
        SetBreakAll(&x328, a, 0);
    } else if (type == 1) {
        if (eff != 0xFF) {
            EmDmBloodSet2(this, eff, 5, 0, 0, 0);
        }
    } else {
        SndCall(6, 0x2E, &pos, 0, 0, this);
        if (eff != 0xFF) {
            EmDmBloodSet3(this, eff, 5, 0, 0x801, 0x31);
        }
    }
}

int cEmWindow::ExeWindowEvent()
{
    EmWindowWork* w;
    Vec plPos;
    void* fcv[3];
    void* mot;
    int i;

    if (this == 0) {
        pLog->err(0, 0, "WindowEvent : ptr faild!");
        return 0;
    }
    w = EMWINDOW_WK(this);
    fcv[0] = GetEtcAddr(w->arc, "pl00537.fcv");
    fcv[1] = GetEtcAddr(w->arc, "pl00538.fcv");
    fcv[2] = GetEtcAddr(w->arc, "pl00536.fcv");
    mot = fcv[0];
    if (pG->stage_no == 4 && pG->x4FB8 == 2) {
        EvtMgr.GetEmWindowFcv(&fcv[0], &fcv[1], &fcv[2]);
    }
    SceEventStart(0);
    SetStatus(2);
    LadderEventTrans(0);
    BEGIN_EVENT(pPL, 0);
    pPL->setNoSuspend(1);
    plPos.x = pPL->pos.x;
    plPos.y = pPL->pos.y;
    plPos.z = pPL->pos.z;
    pPL->setFace(2);
    w->breakDir = ChkBreakDir(&pPL->pos);
    switch (w->breakDir) {
    case 0:
        FSet(pPL->pos.x, 90.0f);
        FSet(pPL->pos.y, -1000.0f);
        FSet(pPL->pos.z, -1610.0f);
        FSet(pPL->rot.x, 0.0f);
        FSet(pPL->rot.y, 0.0f);
        FSet(pPL->rot.z, 0.0f);
        mot = fcv[0];
        break;
    case 1:
        FSet(pPL->pos.x, -210.0f);
        FSet(pPL->pos.y, -1000.0f);
        FSet(pPL->pos.z, 1990.0f);
        FSet(pPL->rot.x, 0.0f);
        FSet(pPL->rot.y, PI);
        FSet(pPL->rot.z, 0.0f);
        mot = fcv[1];
        break;
    case 2:
        FSet(pPL->pos.x, 0.0f);
        FSet(pPL->pos.y, -1000.0f);
        FSet(pPL->pos.z, -980.0f);
        FSet(pPL->rot.x, 0.0f);
        FSet(pPL->rot.y, 0.0f);
        FSet(pPL->rot.z, 0.0f);
        mot = fcv[2];
        break;
    }
    PSMTXMultVec(mat, &pPL->pos, &pPL->pos);
    FSet(pPL->rot.x, pPL->rot.x + rot.x);
    FSet(pPL->rot.y, pPL->rot.y + rot.y);
    FSet(pPL->rot.z, pPL->rot.z + rot.z);
    if (mot) {
        MotionSetCore(pPL, &pPL->pMotion, mot, 0, 0, 0x201, 0);
    }
    for (i = 0; (pPL->motState & 4) == 0; i++) {
        switch (w->breakDir) {
        case 0:
            if (WindowAlive(this) && i == 0xF) {
                SetBreakAll(&plPos, 1, 1);
            }
            if (i == 0x19 && pG->room_id == 0x11F) {
                EstSet(0, -1, 0, 0, 1, 6, 1, 0, 0, 0);
            }
            if (i == 0) {
                EstSet((int) pPL, -1, 0, 0, w->eff, 8, 1, 0, 0, 0);
            }
            if (i == 3) {
                SndCall(1, 0x29, &pPL->pos, 0, 0, pPL);
            }
            if (i == 4) {
                SndCall(1, 0x34, &pPL->pos, 0, 0, pPL);
            }
            if (i == 5) {
                SndCall(1, 0x43, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x23) {
                SndCall(5, 5, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x29) {
                SndCall(5, 0x12, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x2C) {
                SndCall(5, 0x13, &pPL->pos, 0, 0, pPL);
            }
            break;
        case 1:
            if (WindowAlive(this) && i == 0x10) {
                SetBreakAll(&plPos, 1, 1);
            }
            if (i == 0) {
                EstSet((int) pPL, -1, 0, 0, w->eff, 2, 1, 0, 0, 0);
            }
            if (i == 3) {
                SndCall(1, 0x29, &pPL->pos, 0, 0, pPL);
            }
            if (i == 4) {
                SndCall(1, 0x34, &pPL->pos, 0, 0, pPL);
            }
            if (i == 5) {
                SndCall(1, 0x43, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x2D) {
                SndCall(5, 5, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x33) {
                SndCall(5, 0x12, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x37) {
                SndCall(5, 0x13, &pPL->pos, 0, 0, pPL);
            }
            break;
        case 2:
            if (WindowAlive(this) && i == 0xD) {
                SetBreakAll(&plPos, 1, 1);
            }
            if (i == 0) {
                EstSet((int) pPL, -1, 0, 0, w->eff, 4, 1, 0, 0, 0);
            }
            if (i == 3) {
                SndCall(1, 0x29, &pPL->pos, 0, 0, pPL);
            }
            if (i == 4) {
                SndCall(1, 0x34, &pPL->pos, 0, 0, pPL);
            }
            if (i == 5) {
                SndCall(1, 0x43, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x28) {
                SndCall(5, 5, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x37) {
                SndCall(5, 0x12, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x39) {
                SndCall(5, 0x13, &pPL->pos, 0, 0, pPL);
            }
            break;
        }
        SceSleep(1);
    }
    pPL->setFace(0);
    LadderEventTrans(1);
    SceEventEnd(0);
    return 1;
}

void cEmWindow::CalFloor()
{
    EmWindowWork* w = EMWINDOW_WK(this);
    Vec v;
    Vec bottom;
    Vec hit;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 2000.0f;
    RotVector(&v, &rot, &v);
    PSVECAdd(&v, &pos, &v);
    bottom.x = v.x;
    bottom.y = v.y - 320000.0f;
    bottom.z = v.z;
    SatMgr.hitCheck(&v, &bottom, &hit, 0, 0, 0);
    if (hit.y < pos.y - 3000.0f) {
        w->floor = 1;
    } else {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -2000.0f;
        RotVector(&v, &rot, &v);
        PSVECAdd(&v, &pos, &v);
        bottom.x = v.x;
        bottom.y = v.y - 320000.0f;
        bottom.z = v.z;
        SatMgr.hitCheck(&v, &bottom, &hit, 0, 0, 0);
        if (hit.y < pos.y - 3000.0f) {
            w->floor = 1;
        } else {
            w->floor = 0;
        }
    }
}

u8 cEmWindow::GetFloor()
{
    return EMWINDOW_WK(this)->floor;
}

int cEmWindow::ChkBreakDir(Vec* p)
{
    if (Front_check(this, p, PI / 2) != 0) {
        return 1;
    }
    return GetFloor() == 1 ? 2 : 0;
}

int cEmWindow::ChkStatus()
{
    u16* flg;

    flg = GetEtcFlgPtr(getEtc(), pG->room_id);
    if (flg) {
        return *flg;
    }
    return 0;
}

void cEmWindow::SetStatus(u16 f)
{
    u16* flg;

    flg = GetEtcFlgPtr(getEtc(), pG->room_id);
    if (flg) {
        *flg |= f;
    }
}

int cEmWindow::SetShake()
{
    EmWindowWork* w = EMWINDOW_WK(this);

    if (type == 1) {
        SndCall(6, 0x38, &pos, 0, 0, this);
    } else {
        SndCall(6, 0x3A, &pos, 0, 0, this);
    }
    w->shake = 10;
    hp -= 50;
    if (hp <= 0) {
        hp = 1;
    }
    return 1;
}

int cEmWindow::SetBreakAll(Vec* p, int a, int b)
{
    if (ChkEtcFlag(3) == 0) {
        switch (type) {
        case 8:
            SndCall(6, 0x38, &pos, 0, 0, this);
            break;
        case 1:
            SndCall(6, 0x39, &pos, 0, 0, this);
            break;
        default:
            SndCall(6, 0x3B, &pos, 0, 0, this);
            break;
        }
        SetBreakEsp(ChkBreakDir(p), a, b);
        SetBreakModel();
    }
    return 1;
}

int cEmWindow::SetBreakModel()
{
    EmWindowWork* w = EMWINDOW_WK(this);
    void* bin;

    if (strcmp(WindowData[type].bin, "") != 0) {
        bin = GetEtcAddr(w->arc, WindowData[type].bin);
        SetChangeModel(bin, GetEtcAddr(w->arc, WindowData[type].tpl));
    } else {
        be_flag &= ~2;
    }
    if (WindowData[type].breakEff2 == 1) {
        EstSet(0, -1, &pos, &rot, w->eff, 9, 1, 0, 0, 0);
    }
    SetAtariOff();
    SetStatus(1);
    EffectEspDelete(0, 0x31, this, 0);
    EffectEspgenDelete(0, 0x31, this);
    EffectEfmDelete(0, 0x31, this);
    return 1;
}

int cEmWindow::SetChangeModel(void* bin, void* tpl)
{
    if (modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetEmObj : modelInit failed");
        return 0;
    }
    return 1;
}

int cEmWindow::SetAtariOff()
{
    atari.flags &= ~0x300;
    clrSat();
    clrEat();
    hp = 0;
    SetEnableDamage(1);
    return 1;
}

int cEmWindow::SetBreakEsp(int dir, int kind, int flag)
{
    Vec p;
    Vec r;
    int tblA[4][2] = { { 6, 0 }, { 7, 1 }, { 6, 0 }, { 7, 1 } };
    int tblB[4][2] = { { 6, 0 }, { 7, 1 }, { 3, 3 }, { 7, 1 } };
    u8 eff;
    int id;

    eff = getEff();
    if (kind > 1) {
        kind = 1;
        pLog->err(0, 0, "EmWindow : break_size limit over");
    }
    if (flag == 0) {
        if (dir > 3) {
            dir = 0;
            pLog->err(0, 0, "EmWindow : espid faild!");
        }
        id = tblA[dir][kind];
    } else {
        if (dir > 2) {
            dir = 0;
            pLog->err(0, 0, "EmWindow : espid faild!");
        }
        id = tblB[dir][kind];
    }
    p.x = 0.0f;
    p.y = 0.0f;
    p.z = 0.0f;
    r.x = 0.0f;
    r.y = 0.0f;
    r.z = 0.0f;
    PSMTXMultVec(mat, &p, &p);
    r.x += rot.x;
    r.y += rot.y;
    r.z += rot.z;
    EstSet(0, -1, &p, &r, eff, (u8) id, 0x801, 0, 0, 0);
}

void cEmWindow::SetEnableDamage(int on)
{
    int v = 1;

    if (on == 1) {
        v = 0;
    }
    SetEtcFlag(0, v);
}

int cEmWindow::ChkEnableDamage()
{
    if (ChkEtcFlag(0) == 1) {
        return 0;
    }
    return 1;
}

void cEmWindow::SetEtcFlag(u32 no, int on)
{
    if (on == 1) {
        TblBitOn(EMWINDOW_WK(this)->etcFlag, no);
    } else {
        TblBitOff(EMWINDOW_WK(this)->etcFlag, no);
    }
}

int cEmWindow::ChkEtcFlag(u32 no)
{
    u32* flg = EMWINDOW_WK(this)->etcFlag;

    if (flg[no >> 5] & (0x80000000 >> (no & 0x1F))) {
        return 1;
    }
    return 0;
}

int cEmWindow::SetEnableFence(int on, int kind)
{
    EmWindowWork* w = EMWINDOW_WK(this);

    if (kind == 0 || kind == 1) {
        if (on == 1) {
            EMWINDOW_WK(this)->etcFlag[0] &= ~0x40000000;
        } else {
            EMWINDOW_WK(this)->etcFlag[0] |= 0x40000000;
        }
    }
    if (kind == 0 || kind == 2) {
        if (on == 1) {
            w->etcFlag[0] &= ~0x20000000;
        } else {
            w->etcFlag[0] |= 0x20000000;
        }
    }
    return 1;
}

int cEmWindow::ChkEnableFence(int kind)
{
    u32 t;

    if (kind == 1) {
        t = EMWINDOW_WK(this)->etcFlag[0] & 0x40000000;
        return t == 0;
    }
    if (kind == 2) {
        t = EMWINDOW_WK(this)->etcFlag[0] & 0x20000000;
        return t == 0;
    }
    return 0;
}
