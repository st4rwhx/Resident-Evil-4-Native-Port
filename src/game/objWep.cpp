#include "atari.h"
#include "at_sub2.h"
#include "obj.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "item.h"
#include "pl_wep.h"
#include "player.h"
#include "cam_ctrl.h"
#include "dbmodule.h"
#include "eprintf.h"
#include "gx.h"
#include "camera.h"

// Player weapon object: the model the player holds, its mode dispatch (stay / ready / fire /
// down / reload / drop), the laser sight and the debug collision display.

extern "C" {
int MotionMove(cModel* m, int a);
int GetWepTargetPos(Vec* from, Vec* to, int a, int wepNo, cEm** target, u32* attr);
void Draw_line3d_local_222(Vec* p0, Vec* p1, Mtx mtx, u32 color, int blend);
void Draw_line3d_222(Vec* p0, Vec* p1, u32 color, int blend);
void drawPoint(Vec* p0, Vec* p1);
}

// Display flag helpers: through a reference with the bit as a parameter the mask stays 32-bit
// (`rlwinm`), and separate tests are not folded into one `andi.` of the combined mask.
static inline void DispOn(u8& f, u8 b) { f |= b; }
static inline void DispOff(u8& f, u8 b) { f &= ~b; }
static inline int DispChk(u8 f, u8 b) { return f & b; }
static inline int FlagChk(u32 f, u32 b) { return f & b; }

// Weapon held in the hand: the stance key only counts while the hand weapon is allowed. Never
// constructed in the DOL: the linker dropped its vtable (STRIP_UNUSED).
class cObjHand : public cObjWep {
public:
    virtual int keyKamae();
};

cObjWep::cObjWep()
{
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 500.0f, 0.0f, 0.0f };

    wep.disp = 0;
    sub2B4.atari.throughOn();
    lightInfo.init2(1, 1, &p0, &p1, 1);
    pMotion = 0;
    wep.pMotEmpty = 0;
    wep.pMotNormal = 0;
    wep.parent = 0;
    wep.seHandle = 0;
    wep.disp = 0x1C;
}

void cObjWep::move()
{
    switch (wep.mode) {
    default:
        moveStay();
        break;
    case 1:
        moveReady();
        break;
    case 2:
        moveFire();
        break;
    case 3:
        moveDown();
        break;
    case 4:
        moveReload();
        break;
    case 5:
        moveDrop();
        break;
    }
    moveAll();
    if (DispChk(wep.disp, 4) == 0 || DispChk(wep.disp, 8) == 0 || DispChk(wep.disp, 0x10) == 0 ||
        (wep.parent && (wep.parent->isTrans() == 0 || (pG->flags_58 & 0x40000000)))) {
        be_flag &= ~2;
    } else {
        be_flag |= 2;
    }
    if (wep.parent) {
        alpha = wep.parent->alpha;
        x158 = wep.parent->x158;
    }
    x12F = pPL->x12F;
    if (pMotion) {
        MotionMove(this, 0);
    } else {
        matUpdate();
    }
    if (wep.disp & 1) {
        drawLaserSight(1, 0);
    }
    DispOff(wep.disp, 2);
    if (DispChk(wep.disp, 1)) {
        DispOn(wep.disp, 2);
    }
    DispOff(wep.disp, 1);
}

void cObjWep::setDisp(int type, int on)
{
    if (on == 1) {
        switch (type) {
        case 0:
            DispOn(wep.disp, 4);
            break;
        case 1:
            DispOn(wep.disp, 8);
            break;
        case 2:
            DispOn(wep.disp, 0x10);
            break;
        }
    } else {
        switch (type) {
        case 0:
            DispOff(wep.disp, 4);
            break;
        case 1:
            DispOff(wep.disp, 8);
            break;
        case 2:
            DispOff(wep.disp, 0x10);
            break;
        }
    }
}

void cObjWep::parentSet(cModel* parent, int partsNo, Vec* pos, Vec* rot)
{
    wep.parent = parent;
    pParts->pParent = parent->getPartsPtr(partsNo);
    pParts->pos = *pos;
    pParts->rot = *rot;
}

void cObjWep::parentRelease()
{
    pos = pParts->pParent->worldPos;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    pParts->pParent = this;
    pParts->pos.x = 0.0f;
    pParts->pos.y = 0.0f;
    pParts->pos.z = 0.0f;
    pParts->rot.x = 0.0f;
    pParts->rot.y = 0.0f;
    pParts->rot.z = 0.0f;
    wep.parent = 0;
}

void cObjWep::endReload(int noReload)
{
    resetMotion();
    if (noReload == 0) {
        ItemMgr.reload();
    }
}

void cObjWep::resetMotion()
{
    void* mot;

    if (ItemMgr.bulletNum() == 0 && wep.pMotEmpty) {
        mot = wep.pMotEmpty;
    } else if (wep.pMotNormal) {
        mot = wep.pMotNormal;
    } else {
        mot = 0;
    }
    if (mot) {
        motionSet(mot, 0, 0, 1, 0);
        motionMove();
    }
    if (wep.seHandle) {
        SndStop(wep.seHandle, 0);
    }
    motSpeedRate = 1.0f;
    wep.mode = 0;
    wep.step = 0;
}

void cObjWep::trigger()
{
    ItemMgr.trigger();
}

int cObjWep::bulletNum()
{
    if (ItemMgr.bulletNumCurrent()) {
        return 1;
    }
    return 0;
}

int cObjWep::reloadable()
{
    return ItemMgr.reloadable();
}

void cObjWep::drawLaserSight(int draw, int noCalc)
{
    static Vec lpos;
    static Vec lcross;
    static int donfire;

    if (FlagChk(pG->flags_60, 0x08000000) || FlagChk(pG->flags_60, 0x04000000)) {
        satCheck();
        return;
    }
    if (noCalc == 0) {
        Vec dir;
        u32 attr;
        int res;
        f32 dist;

        donfire = 0;
        partsWorldCalc();
        getMarkerPos(&lpos, &lcross);
        res = GetWepTargetPos(&lpos, &lcross, 0, pG->wep_no, &wep.target, &attr);
        if (wep.target && wep.target->checkStatus(10)) {
            donfire = 1;
        }
        dist = (lcross.x - lpos.x) * (lcross.x - lpos.x) + (lcross.y - lpos.y) * (lcross.y - lpos.y) +
               (lcross.z - lpos.z) * (lcross.z - lpos.z);
        switch (res) {
        case 1: {
            f32 dist2;
            AtEffInfo* info;

            dist2 = (lpos.x - lcross.x) * (lpos.x - lcross.x) + (lpos.y - lcross.y) * (lpos.y - lcross.y) +
                    (lpos.z - lcross.z) * (lpos.z - lcross.z);
            info = EatMgr.getEffInfo(EatGetEffectType(attr));
            if (info) {
                int on = 1;
                if ((info->flags & 2) == 0) {
                    on = 0;
                }
                if (on && dist2 < 400000000.0f) {
                    donfire = res;
                }
            }
            break;
        }
        case 2:
            if (draw && dist > 250000.0f) {
                drawPoint(&lpos, &lcross);
            }
            break;
        }
        if (dist < 250000.0f) {
            return;
        }
        PSVECSubtract(&lcross, &lpos, &dir);
#line 404 "D:/Bio4/Prog/objWep.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, 500.0f);
        PSVECAdd(&lpos, &dir, &lpos);
    }
    if (draw) {
        f32 width;

        if ((pG->room_id32 & 0xFFFF0000) == 0x022C0000 || (pG->room_id32 & 0xFFFF0000) == 0x22280000) {
            width = 3.0f;
        } else {
            width = 1.0f;
        }
        if (pG->x4 == 1) {
            Draw_line3d_222(&lpos, &lcross, 0x20400000, 1);
        } else {
            EspDrawLaserLine(lpos, lcross, width);
            if ((pG->room_id32 & 0xFFFF0000) == 0x022C0000 || (pG->room_id32 & 0xFFFF0000) == 0x02280000) {
                EspDrawLaserLine(lpos, lcross, width);
            }
        }
    }
    if (donfire) {
        pG->flags_5014 |= 0x80000000;
    }
    wep.marker = lcross;
}

void drawPoint(Vec* p0, Vec* p1)
{
    static f32 laset_max_dist = 8000.0f;
    static f32 laset_max_size = 3.0f;
    static f32 laset_base_size = 0.8f;
    Vec d;
    cEsp* esp;
    f32 size;

    if (pG->flags_6C & 0x40) {
        return;
    }
    if (EspEstSetSelect(0, 0x50, 0, &esp, 1) != 1) {
        return;
    }
    PSVECSubtract(&pG->Cam.param.pos, p1, &d);
    size = PSVECMag(&d);
    if ((pG->flags_5018 & 0x02000000) || (pG->room_id32 & 0xFFFF0000) == 0x022C0000 ||
        (pG->room_id32 & 0xFFFF0000) == 0x02280000) {
        size = size * 0.00033333333f + 1.0f;
        if (size > 6.0f) {
            size = 6.0f;
        }
    } else {
        size = size * (1.0f / laset_max_dist) + laset_base_size;
        if (size > laset_max_size) {
            size = laset_max_size;
        }
    }
    esp->pos = *p1;
    FSet(esp->sizeX, esp->sizeX * size);
    FSet(esp->sizeY, esp->sizeY * size);
    if (pG->flags_5010 & 1) {
        // COMPILER-DIFF: #17. `esp` is address-taken, so each store reloads it; the original's first
        // reload sits in r11 (r9 was still held by the previous reload at its sched1 position), ours
        // in r9. Pinned, no code emitted.
        register cEsp* e asm("r11");
        e = esp;
        e->xA4 = 1;
        esp->xA5 = 4;
        esp->xA6 = 5;
        esp->xA7 = 0;
    }
}

void cObjWep::getMarkerPos(Vec* pos, Vec* at)
{
    static const Vec ofs[46] = {
        { 500.0f, 0.0f, 0.0f },      { 262.0f, -24.0f, 32.0f },   { 228.0f, -24.0f, 29.0f },
        { 156.0f, -6.0f, 90.0f },    { 231.0f, -24.0f, 30.5f },   { 378.5f, -1.7f, 61.7f },
        { 192.0f, -24.0f, 134.0f },  { -116.0f, -21.0f, 32.0f },  { -129.0f, -23.0f, 62.0f },
        { 228.0f, -24.0f, 29.0f },   { 228.0f, -24.0f, 29.0f },   { 206.0f, -24.0f, 33.4f },
        { 4.0f, -24.0f, 85.0f },     { 265.7f, 130.0f, 73.3f },   { -97.0f, 7.0f, -18.0f },
        { 87.70001f, -23.1f, 72.6f },    { 500.0f, 0.0f, 0.0f },      { 225.6f, -24.0f, 40.1f },
        { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },
        { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },
        { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },
        { 500.0f, 0.0f, 0.0f },      { -400.0f, -6.2f, 163.7f },  { 500.0f, 0.0f, 0.0f },
        { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },
        { -300.0f, -21.0f, 32.0f },  { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
    };
    Vec v;
    cModel* parts;
    f32 len;

    switch (pG->wep_no) {
    default:
        parts = getPartsPtr(pG->wep_no == 0xE);
        PSMTXMultVec(parts->mat, &ofs[pG->wep_no], pos);
        if (pG->wep_no == 0x1C) {
            len = 50000.0f;
        } else {
            len = -50000.0f;
        }
        v.x = len;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVecSR(parts->mat, &v, &v);
        PSVECAdd(&v, pos, at);
        break;
    case 9:
    case 10:
        CamCtrl.getTrajectory(pos, at);
        break;
    }
}

void cObjWep::interrupt()
{
    setDisp(1, 1);
    setDisp(2, 1);
    be_flag |= 2;
    if (wep.mode == 4) {
        endReload(0);
    }
    sub2B4.atari.clrFlag200();
    resetMotion();
}

int cObjHand::keyKamae()
{
    if (pG->flags_5018 & 0x00800000) {
        return cObjWep::keyKamae();
    } else {
        return 0;
    }
}

#define NOHIT_COL(bit) col = 0; if ((attr & (bit)) == 0) col = 20;

void cObjWep::satCheck()
{
    static const char* strAt[2] = { "SCROLL ATARI INFO", "EFFECT ATARI INFO" };
    static const u32 lcol[2] = { 0xFF000088, 0xFF008800 };
    static const char* strEffType[8] = { "EAT_ET_NORMAL", "EAT_ET_BULLET", "EAT_ET_WATER", "EAT_ET_PAD",
                                         "EAT_ET_ROOM0", "EAT_ET_ROOM1", "EAT_ET_ROOM2", "EAT_ET_ROOM3" };
    Vec p0;
    Vec p1;
    Vec hit;
    Vec nrm;
    u32 attr;
    int col;
    u32 t = pG->flags_60 & 0x08000000;
    int eat = t == 0;

    getMarkerPos(&p0, &p1);
    if (eat == 0) {
        attr = SatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0x8000, 0);
        attr |= SatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0, 0);
    } else {
        attr = EatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0, 0);
    }
    Draw_line3d_222(&p0, &p1, lcol[attr != 0], 1);
    if (attr & 0x01000000) {
        PSVECScale(&nrm, &p1, 1000.0f);
        PSVECAdd(&p1, &hit, &p1);
        Draw_line3d(&hit, &p1, 0xFFFF5533, 0);
        Draw_pos(&hit, 300);
    }
    eprintf(24, 48, 0, 0, strAt[eat]);
    eprintf(24, 64, 0, 0, "ATTR:%08X", attr);
    if (eat == 0) {
        NOHIT_COL(0x00800000) eprintf(24, 0x50, col, 0, "SEE_NOHIT");
        NOHIT_COL(0x8000) eprintf(24, 0x60, col, 0, "SMALL_NOHIT");
        NOHIT_COL(0x80) eprintf(24, 0x70, col, 0, "STEPS");
        NOHIT_COL(0x00400000) eprintf(24, 0x80, col, 0, "PL_NOHIT");
        NOHIT_COL(0x4000) eprintf(24, 0x90, col, 0, "EM_NOHIT");
        NOHIT_COL(0x40) eprintf(24, 0xA0, col, 0, "ROUTE_NOHIT");
        NOHIT_COL(0x00200000) eprintf(24, 0xB0, col, 0, "UP");
        NOHIT_COL(0x2000) eprintf(24, 0xC0, col, 0, "DOWN");
        NOHIT_COL(0x20) eprintf(24, 0xD0, col, 0, "FANCE");
        NOHIT_COL(0x00100000) eprintf(24, 0xE0, col, 0, "FALL");
        NOHIT_COL(0x1000) eprintf(24, 0xF0, col, 0, "UP2");
        NOHIT_COL(0x10) eprintf(24, 0x100, col, 0, "DOWN2");
        NOHIT_COL(0x00080000) eprintf(24, 0x110, col, 0, "JUMPOVER");
        NOHIT_COL(0x800) eprintf(24, 0x120, col, 0, "CLIFF");
        NOHIT_COL(0x8) eprintf(24, 0x130, col, 0, "HIDE");
        NOHIT_COL(0x00040000) eprintf(24, 0x140, col, 0, "FALL FANCE");
        NOHIT_COL(0x400) eprintf(24, 0x150, col, 0, "ONLY CAM HIT");
        NOHIT_COL(0x4) eprintf(24, 0x160, col, 0, "NO EFF SET");
    } else {
        col = 0;
        eprintf(24, 0x50, col, 0, "EFF TYPE: %s", strEffType[EatGetEffectType(attr)]);
        NOHIT_COL(0x00400000) eprintf(24, 0x60, col, 0, "SMALL_NOHIT");
        NOHIT_COL(0x4000) eprintf(24, 0x70, col, 0, "MIDDLE_NOHIT");
        NOHIT_COL(0x40) eprintf(24, 0x80, col, 0, "NO EFF SET");
    }
}

void Draw_line3d_local_222(Vec* p0, Vec* p1, Mtx mtx, u32 color, int blend)
{
    static f32 max_laser_dist = 8000.0f;
    Vec end;
    Vec dir;
    f32 rate = 1.0f;
    f32 d;
    u8 r, g, b, a;

    if (blend == 0) {
        GXSetBlendMode(1, 1, 0, 0);
    } else {
        GXSetBlendMode(1, 1, 1, 0);
    }
    CameraCurrentProjection();
    GXSetCullMode(0);
    if ((color >> 24) == 0xFE) {
        GXSetZMode(0, 3, 1);
    } else {
        GXSetZMode(1, 3, 1);
    }
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = color & 0xFF;
    a = 0xFF;

    end = *p1;
    PSVECSubtract(p1, p0, &dir);
    d = PSVECMag(&dir);
    if (d > max_laser_dist) {
#line 800 "D:/Bio4/Prog/objWep.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, max_laser_dist);
        PSVECAdd(p0, &dir, &end);
        d = max_laser_dist;
    }
    rate = 1.0f - d / max_laser_dist;

    GXBegin(0xB0, 0, 2);
    GXPosition3f32(p0->x, p0->y, p0->z);
    GXColor4u8(r, g, b, a);
    GXPosition3f32(end.x, end.y, end.z);
    GXColor4u8((u8)(r * rate), (u8)(g * rate), (u8)(b * rate), (u8)(a * rate));
}

void Draw_line3d_222(Vec* p0, Vec* p1, u32 color, int blend)
{
    Draw_line3d_local_222(p0, p1, pG->Cam.viewMat, color, blend);
}
