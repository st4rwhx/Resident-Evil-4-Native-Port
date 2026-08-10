#include "atari.h"
#include "light.h"
#include "obj.h"
#include "emhit.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"

extern "C" void* memcpy(void* dst, const void* src, unsigned int n);

// Bell: a pendulum model with a hit-receiving enemy work; a shot swings it, rings it (reported to
// pG for 90 frames) and setBreak() lets it fall.
class cObjBell : public cObj {
public:
    virtual void move();

    void setBreak();
    int ckBreakEnable();
    int ckBreak();
};

extern "C" {
void obj14_R1_Set(cObjBell* obj);
void obj14_R1_Break(cObjBell* obj);
void obj14MatCalc(cObjBell* obj);
void obj14DmCk(cObjBell* obj);
void obj14ClothSet(cObjBell* obj);
void obj14ClothMove(cObjBell* obj);
}

void (*Obj14_R1_move_tbl[2])(cObjBell*) = { obj14_R1_Set, obj14_R1_Break };
u8 obj14ClothP[] = { 1, 2 };
u8 obj14ClothUp[] = { 0xFF, 1 };
u8 obj14ClothDp[] = { 2, 0xFF };
f32 obj14ClothMax[] = { 0.7853982f, 0.43633232f };

cObj* SetObjBell(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    BellWork* w;
    Vec p0;
    Vec p1;

    obj = ObjMgr.create(0x14);
    if (obj == 0) {
        return 0;
    }
    w = &obj->bell;
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    obj->oldPos = obj->pos;
    if (rot) {
        obj->rot = *rot;
    } else {
        obj->rot.x = 0.0f;
        obj->rot.y = 0.0f;
        obj->rot.z = 0.0f;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetObj14() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec l0 = { 0.0f, 0.0f, 0.0f };
    static const Vec l1 = { 1000.0f, 1000.0f, 0.0f };

    obj14ClothSet((cObjBell*) obj);
    obj->sub2B4.atari.throughOn();
    obj->lightInfo.init2(0, 1, &l0, &l1, 0x10);
    w->ringTimer = 0;
    p0.x = 0.0f;
    p0.y = 0.0f;
    p0.z = 0.0f;
    p1.x = 0.0f;
    p1.y = 0.0f;
    p1.z = 0.0f;
    w->pEmHit = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc),
                         (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &p0, &p1, 1);
    if (w->pEmHit) {
        w->pEmHit->setParent(obj, 1, 0);
        YarareInit(w->pEmHit, 0.0f, -650.0f, 0.0f, 300.0f, 50.0f, 1, 1);
    }
    obj->xFD = 0;
    obj->xFC = 1;
    obj->xFE = 0;
    obj->xFF = 0;
    return obj;
}

void cObjBell::move()
{
    obj14DmCk(this);
    Obj14_R1_move_tbl[xFD](this);
    obj14ClothMove(this);
}

void obj14_R1_Set(cObjBell* obj)
{
    BellWork* w = &obj->bell;

    obj14MatCalc(obj);
    if (w->ringTimer) {
        Vec p;

        w->ringTimer--;
        p.x = 0.0f;
        p.y = 0.0f;
        p.z = 250.0f;
        PSMTXMultVec(obj->mat, &p, &p);
        p.y = SatMgr.getFloor(&p, 600.0f, 100000.0f, 0, 0);
        BitOn(pG->flags_5010, 0x20000000);
        // A byte-pointer destination: the copy is then a plain (non-struct) store and the
        // original reloads pG for the following store, as the target shows.
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &p, sizeof(Vec));
        pG->bell_stat = 2;
    }
}

void obj14_R1_Break(cObjBell* obj)
{
    BellWork* w = &obj->bell;

    if (obj->xFE == 0) {
        obj->be_flag &= ~2;
        if (w->pEmHit) {
            w->pEmHit->hp = 0;
        }
        EstSet(0, -1, &obj->pos, &obj->rot, 1, 7, 0, 0, 0, 0);
        obj->xFE++;
    }
    obj14MatCalc(obj);
}

void obj14MatCalc(cObjBell* obj)
{
    RotMatrix(obj->worldMat, &obj->rot);
    TransMatrix(obj->worldMat, &obj->pos);
    ScaleMatrix(obj->worldMat, &obj->scale);
    PSMTXCopy(obj->worldMat, obj->mat);
    if (obj->pMotion == 0) {
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
}

void obj14DmCk(cObjBell* obj)
{
    BellWork* w = &obj->bell;
    Vec dm;
    Vec dm2;
    Vec dir;
    u32 wep;
    f32 rate;
    cModel* parts;

    if (w->pEmHit == 0) {
        return;
    }
    wep = w->pEmHit->ckDmgWeapon();
    if (wep == 0) {
        return;
    }
    switch (wep) {
    default:
        if (pG->room_id != 4) {
            EmDmBloodSet2(w->pEmHit, 1, 5, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (pG->room_id != 4) {
            EmDmBloodSet2(w->pEmHit, 1, 6, 0, 0, 0);
        }
        break;
    }
    SndCall(6, 0xE, &obj->pos, 0, 0, 0);
    w->ringTimer = 90;
    switch (wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x11:
    case 0x26:
    case 0x2B:
        rate = 50.0f;
        break;
    case 0xB:
    case 0xC:
    case 0x19:
    case 0x1B:
    case 0x1D:
    case 0x1F:
    case 0x20:
    case 0x27:
        rate = 30.0f;
        break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x10:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x21:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2C:
    case 0x2D:
    default:
        rate = 100.0f;
        break;
    }
    if (EmGetDmPos(w->pEmHit, &dm, &dm2) == 0) {
        dm = w->pEmHit->x328;
    }
    PSVECSubtract(&obj->pos, &dm, &dir);
    dir.y = 0.0f;
    if (dir.x == 0.0f && dir.z == 0.0f) {
        dir.z = 1.0f;
    }
#line 359 "D:/Bio4/Prog/obj14.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, rate);
    parts = obj->getPartsPtr(1);
    PSVECAdd((Vec*) &parts->x150, &dir, (Vec*) &parts->x150);
    parts = obj->getPartsPtr(2);
    PSVECScale(&dir, &dir, 0.8f);
    PSVECAdd((Vec*) &parts->x150, &dir, (Vec*) &parts->x150);
}

void cObjBell::setBreak()
{
    xFC = 1;
    xFE = 0;
    xFD = 1;
    xFF = 0;
}

int cObjBell::ckBreakEnable()
{
    return (stat & 0xFFFF0000) == 0x01000000;
}

int cObjBell::ckBreak()
{
    return (stat & 0xFFFF0000) == 0x01010000;
}

void obj14ClothSet(cObjBell* obj)
{
    BellWork* w = &obj->bell;

    w->cloth.num = 2;
    w->cloth.pParts = obj14ClothP;
    w->cloth.pUp = obj14ClothUp;
    w->cloth.pDown = obj14ClothDp;
    w->cloth.pMax = obj14ClothMax;
    w->cloth.x3C = 15.0f;
    w->cloth.x40 = 1.0f;
    w->cloth.x08 = 0;
    w->cloth.x0C = 0;
    w->cloth.x10 = 0;
    w->cloth.x14 = 0;
    w->cloth.x2C = 0;
    w->cloth.x30 = 0;
    w->cloth.x20 = 0;
    w->cloth.x24 = 0;
    w->cloth.x34 = 0;
    w->cloth.x38 = 0;
    w->cloth.x58 = 0;
    w->cloth.x44 = 0;
    w->cloth.x48 = 0.0f;
    w->cloth.x4C = 0.0f;
    w->cloth.x50 = 0.0f;
    w->cloth.flags = 0x100;
    w->cloth.x54 = 0;
    PenClothSet(obj, &w->cloth, 100.0f);
}

void obj14ClothMove(cObjBell* obj)
{
    PenClothMove3(obj, &obj->bell.cloth);
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
