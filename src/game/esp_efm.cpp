#include "light.h"
#include "atari.h"
#include "obj.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "scroll.h"
#include "TexRender.h"
#include "db_log.h"

// game/motion.cpp (C++ linkage)
void MotionSetCore(cModel* m, void* work, void* data, int a, int b, int c, int d);

extern "C" {
u32 GetEfmMoveIdMax();
u8 GetEfmMoveId(u32 no);
void EfmDeleteSub(cObj* obj);
void EfmDeleteEventSub(cObj* obj);
cObj* EfmSetObj04(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate, Vec* ofs);
cObj* EfmSetObj05(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate);
cObj* EfmSetObj09(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate);
void setModTexRender(cObj* obj, int no);
cObj* SetEffModel(void* bin, void* tpl, Vec* pos, Vec* rot);   // embox.cpp declares it `void` locally
}

#define DEG2RAD (3.14f / 180.0f)

// Read a tuning static through a reference: the load is a MEM with neither the struct nor the
// scalar flag, so it is not hoisted above the preceding member stores and keeps the store it
// follows (EfmSetObj09: the original reloads moment_mul three times and keeps both mass stores).
static inline f32 FRef(f32& v)
{
    return v;
}

// Read a pointer member through a reference (no struct flag): the store to the stack local
// `model` in between may alias it, so `scr->pInfo` is reloaded for `tpl` (EfmSeqSet).
template <class T> static inline T PRef(T& v)
{
    return v;
}

// Effect model (Efm) set up: the effect sequence record `gen` creates an obj04 / obj05 / obj09
// work in ObjMgr and fills its work from the record.

u8 EfmIdTbl[4] = {4, 5, 9, 4};

// Light parameters shared by the model set ups (defined after EfmSeqSet: external linkage puts
// them into .rodata at the definition, between the EfmSeqSet and EfmSetObj04 strings; a
// `static const` would be deferred to the end of the unit).
extern const Vec efm_light_pos;
extern const Vec efm_light_size;

u16 g_Core_flg;
u8 g_Core_kind;
cModel* g_Core_pEm;

u32 GetEfmMoveIdMax()
{
    return 4;
}

u8 GetEfmMoveId(u32 no)
{
    if (no >= GetEfmMoveIdMax()) {
        no = 0;
    }
    return EfmIdTbl[no];
}

void EfmDelete(int a, int b, int c)
{
    cObjMgr* m = &ObjMgr;
    void (*func)(cObj*) = EfmDeleteSub;
    u32 i;

    g_Core_flg = a;
    g_Core_kind = b;
    g_Core_pEm = (cModel*) c;
    for (i = 0; i < m->nArray; i++) {
        func((cObj*) ((u8*) m->pArray + m->size * i));
    }
}

void EfmDeleteSub(cObj* obj)
{
    if (obj->id == 4) {
        Efm04Work* w = &obj->efm04;
        if ((g_Core_flg == 0 || w->core.flg == g_Core_flg) && (g_Core_kind == 0 || w->core.kind == g_Core_kind) &&
            (g_Core_pEm == 0 || w->core.pEm == g_Core_pEm)) {
            ObjMgr.destroy(obj);
        }
    }
    if (obj->id == 5) {
        Efm05Work* w = &obj->efm05;
        if ((g_Core_flg == 0 || w->core.flg == g_Core_flg) && (g_Core_kind == 0 || w->core.kind == g_Core_kind) &&
            (g_Core_pEm == 0 || w->core.pEm == g_Core_pEm)) {
            ObjMgr.destroy(obj);
        }
    }
    if (obj->id == 9) {
        Efm09Work* w = &obj->efm09;
        if ((g_Core_flg == 0 || w->core.flg == g_Core_flg) && (g_Core_kind == 0 || w->core.kind == g_Core_kind) &&
            (g_Core_pEm == 0 || w->core.pEm == g_Core_pEm)) {
            ObjMgr.destroy(obj);
        }
    }
}

void EfmDeleteEvent()
{
    cObjMgr* m = &ObjMgr;
    void (*func)(cObj*) = EfmDeleteEventSub;
    u32 i;

    for (i = 0; i < m->nArray; i++) {
        func((cObj*) ((u8*) m->pArray + m->size * i));
    }
}

void EfmDeleteEventSub(cObj* obj)
{
    if (obj->id == 4) {
        Efm04Work* w = &obj->efm04;
        if (!(w->core.flg & 1) && !(w->core.flg & 0x800)) {
            ObjMgr.destroy(obj);
        }
    }
    if (obj->id == 5) {
        Efm05Work* w = &obj->efm05;
        if (!(w->core.flg & 1) && !(w->core.flg & 0x800)) {
            ObjMgr.destroy(obj);
        }
    }
    if (obj->id == 9) {
        Efm09Work* w = &obj->efm09;
        if (!(w->core.flg & 1) && !(w->core.flg & 0x800)) {
            ObjMgr.destroy(obj);
        }
    }
}

void EfmArrayClear()
{
    void (*func)(cObj*);
    cObj* p;
    cObj* n;

    g_Core_flg = 0;
    g_Core_kind = 0;
    g_Core_pEm = 0;
    func = EfmDeleteSub;
    p = ObjMgr.pAlive;
    while (p) {
        n = p;
        p = (cObj*) p->next;
        func(n);
    }
}

cObj* EfmSeqSet(EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate, Vec* ofs)
{
    cObj* obj = 0;
    Vec size;
    Vec center;
    void* model;
    void* tpl;
    u32 moveId;

    if (!(info->flg & 0x1000) && gen->x6 != 0) {
        parent = SmdGetObjPtr(gen->x6 - 1);
        if (parent == 0) {
            pLog->err(0, 0, "ESP_EFM : PARENT_NO[%d] Invalid.", gen->x6);
            return 0;
        }
    }
    switch (gen->x1) {
    case 0xFF:
        moveId = 0;
        break;
    case 0xFE:
        moveId = 1;
        break;
    case 0xFD:
        moveId = 2;
        break;
    case 0xFC:
        moveId = 3;
        break;
    default:
        pLog->err(0, 0, "ESP_EFM : EFM_MOVE_ID[%x] is invalid.", gen->x1);
        return 0;
    }
    if (moveId >= GetEfmMoveIdMax()) {
        pLog->err(0, 0, "ESP_EFM : EFM_MOVE_ID[%x] is invalid.", moveId);
        return 0;
    }
    if (moveId == 3) {
        cObj* scr = SmdGetGroupObjPtr(gen->x2);
        if (scr == 0) {
            pLog->err(0, 0, "ESP_EFM : SCR_MODEL_NO[%x] is invalid.", gen->x2);
            return 0;
        }
        model = PRef(scr->pInfo)->pData;
        tpl = PRef(scr->pInfo)->pTpl;
    } else {
        if (EspGetEfmAddr(gen->x2, &model, &tpl) == 0) {
            pLog->err(0, 0, "ESP_EFM : EFM_ID[%x] is invalid.", gen->x2);
            return 0;
        }
    }
    switch (moveId) {
    case 0:
    case 3: {
        int light;
        obj = ObjMgr.createBack(4);
        if (obj == 0) {
            break;
        }
        if (obj->modelInit(model, tpl) == 0) {
            ObjMgr.destroy(obj);
            pLog->err(0, 0, "ESP_EFM : ModelInit() failed.");
            return 0;
        }
        obj->sub2B4.clrFlags(0xFCFF);
        light = 0x10;
        if (gen->flags & 0x80) {
            light = 4;
        }
        if (gen->flags & 0x20000) {
            light = 8;
        }
        if (moveId == 3) {
            ModelBound* bound = &obj->pInfo->bound;
            size.x = bound->size.x;
            size.y = bound->size.y;
            size.z = bound->size.z;
            PSVECSubtract(&bound->center, &obj->pParts->pos, &center);
            obj->lightInfo.init2(2, 1, &center, &size, light);
            obj->x103 = 0x80;
        } else {
            obj->lightInfo.init2(0, 1, &efm_light_pos, &efm_light_size, light);
        }
        obj->id = GetEfmMoveId(moveId);
        obj = EfmSetObj04(obj, gen, info, seed, parent, m, x, rate, ofs);
        if (obj && (info->flg & 1)) {
            obj->setNoSuspend(1);
        }
        break;
    }
    case 1: {
        int light;
        obj = ObjMgr.createBack(5);
        if (obj == 0) {
            break;
        }
        if (obj->modelInit(model, tpl) == 0) {
            ObjMgr.destroy(obj);
            pLog->err(0, 0, "ESP_EFM : ModelInit() failed.");
            return 0;
        }
        obj->sub2B4.clrFlags(0xFCFF);
        light = 0x10;
        if (gen->flags & 0x80) {
            light = 4;
        }
        if (gen->flags & 0x20000) {
            light = 8;
        }
        obj->lightInfo.init2(0, 1, &efm_light_pos, &efm_light_size, light);
        obj->id = GetEfmMoveId(1);
        obj = EfmSetObj05(obj, gen, info, seed, parent, m, x, rate);
        if (obj && (info->flg & 1)) {
            obj->setNoSuspend(1);
        }
        break;
    }
    case 2:
        obj = ObjMgr.createBack(9);
        if (obj == 0) {
            break;
        }
        if (obj->modelInit(model, tpl) == 0) {
            ObjMgr.destroy(obj);
            pLog->err(0, 0, "ESP_EFM : ModelInit() failed.");
            return 0;
        }
        obj->sub2B4.clrFlags(0xFCFF);
        obj->lightInfo.init2(0, 1, &efm_light_pos, &efm_light_size, 0x10);
        obj->id = GetEfmMoveId(2);
        obj = EfmSetObj09(obj, gen, info, seed, parent, m, x, rate);
        if (obj && (info->flg & 1)) {
            obj->setNoSuspend(1);
        }
        break;
    }
    if (info->pEm != 0 && obj != 0 && (info->pEm->be_flag & 9) == 9) {
        u8 r = info->pEm->x139;
        u8 g = info->pEm->x13A;
        u8 b = info->pEm->x13B;
        if (r == 0 && ((g == 0) & (b == 0))) {
            obj->be_flag &= ~8;
        } else {
            obj->be_flag |= 8;
        }
        obj->x139 = r;
        obj->x13A = g;
        obj->x13B = b;
    }
    return obj;
}

const Vec efm_light_pos = {0.0f, 0.0f, 0.0f};
const Vec efm_light_size = {1000.0f, 1000.0f, 0.0f};

cObj* EfmSetObj04(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate, Vec* ofs)
{
    Efm04Work* w = &obj->efm04;
    Vec v;
    Mtx mtx;
    void* mot;
    f32 rnd;
    cModel* parts;

    obj->be_flag |= 0x4000;
    w->core = *info;
    w->x79 = gen->x7;
    w->flags = gen->flags;
    obj->pos = gen->pos;
    obj->pos.x += gen->x18 * fRandSeed1_1(seed);
    obj->pos.y += gen->x1C * fRandSeed1_1(seed);
    obj->pos.z += gen->x20 * fRandSeed1_1(seed);
    obj->speed = gen->x24;
    obj->speed.x += gen->x34.x * fRandSeed1_1(seed);
    obj->speed.y += gen->x34.y * fRandSeed1_1(seed);
    obj->speed.z += gen->x34.z * fRandSeed1_1(seed);
    w->spdDamp = gen->x30;
    w->acc = gen->x40;
    w->acc.x += gen->x4C.x * fRandSeed1_1(seed);
    w->acc.y += gen->x4C.y * fRandSeed1_1(seed);
    w->acc.z += gen->x4C.z * fRandSeed1_1(seed);
    obj->rot = gen->x58;
    obj->rot.x += gen->x64.x * fRandSeed1_1(seed);
    obj->rot.y += gen->x64.y * fRandSeed1_1(seed);
    obj->rot.z += gen->x64.z * fRandSeed1_1(seed);
    PSVECScale(&obj->rot, &obj->rot, DEG2RAD);
    w->rotSpd = gen->x70;
    w->rotSpd.x += gen->x7C.x * fRandSeed1_1(seed);
    w->rotSpd.y += gen->x7C.y * fRandSeed1_1(seed);
    w->rotSpd.z += gen->x7C.z * fRandSeed1_1(seed);
    PSVECScale(&w->rotSpd, &w->rotSpd, DEG2RAD);
    w->scaleXZ = gen->x88 * 0.005f;
    w->scaleY = gen->x8C * 0.005f;
    w->scale = 1.0f;
    rnd = gen->x90 * fRandSeed1_1(seed) * 0.005f;
    w->scaleXZ += rnd;
    w->scaleY += rnd;
    w->scaleSpd = gen->x94;
    w->scaleDamp = gen->x98;
    w->r0 = gen->x9C;
    w->g0 = gen->x9D;
    w->b0 = gen->x9E;
    w->a0 = gen->x9F;
    w->r = (f32) gen->x9C;
    w->g = (f32) gen->x9D;
    w->b = (f32) gen->x9E;
    w->a = (f32) gen->x9F;
    w->rMul = gen->xA0;
    w->gMul = gen->xA4;
    w->bMul = gen->xA8;
    w->aMul = gen->xAC;
    if (gen->flags & 0x400000) {
        obj->x12F = 2;
    } else if (w->a < 250.0f) {
        obj->x12F = 1;
    } else {
        obj->x12F = 0;
    }
    w->fadeStart = gen->xB0;
    w->fadeLen = gen->xB2;
    w->moveStart = gen->xB4;
    w->scaleStart = gen->xB6;
    w->life = gen->xB8;
    w->frame = gen->xBA;
    w->rotFrame = gen->xC0;
    obj->pInfo->color[0] = (u8) w->r;
    obj->pInfo->color[1] = (u8) w->g;
    obj->pInfo->color[2] = (u8) w->b;
    obj->pInfo->color[3] = 0xFF;
    if (w->fadeStart == 0) {
        obj->alpha = w->a * (1.0f / 255.0f);
    } else {
        obj->alpha = 0.0f;
    }
    obj->pInfo->xD6 = gen->xC2;
    obj->scale.y = w->scaleY * w->scale;
    obj->scale.z = obj->scale.x = w->scaleXZ * w->scale;
    w->groundOfs = (f32) (int) gen->xD4;
    if (gen->prm.w.xCC != 0) {
        u8 c = gen->prm.b.xCF;
        if (c == 0) {
            obj->be_flag &= ~8;
        } else {
            obj->be_flag |= 8;
        }
        obj->x139 = c;
        obj->x13A = c;
        obj->x13B = c;
    }
    if (gen->prm.w.xD0 != 0) {
        setModTexRender(obj, gen->prm.w.xD0 - 1);
    }
    w->bounce = gen->vE4;
    PSVECScale(&w->bounce, &w->bounce, 0.1f);
    if (!(w->flags & 0x40)) {
        obj->lightInfo.x54 = 0;
        obj->be_flag |= 0x20000;
    }
    if (w->flags & 0x200000) {
        obj->x12C = 1;
    }
    switch (w->x79) {
    case 0xFF:
        w->parentWorld = pEffParentWorld;
        Efm04RotMatrix(obj, m);
        break;
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
        w->parentWorld = pEffParentWorld;
        break;
    case 0xFE:
        w->parentWorld = pEffParentWorld;
        if (gen->xC0 != 0) {
            pLog->warn(0, 0, "ESP_EFM : ReleaseTime not 0 but no parent.");
        }
        break;
    default:
        if (parent == 0) {
            pLog->err(0, 0, "ESP_EFM : PARTS_NO[%d] but Not on parts.", w->x79);
            ObjMgr.destroy(obj);
            return 0;
        }
        if (w->x79 < parent->nParts) {
            if (w->flags & 0x20) {
                parts = parent->getPartsPtr(w->x79);
                PSMTXIdentity(mtx);
                RotMatrix(mtx, &parent->rot);
                PSMTXMultVecSR(mtx, &obj->pos, &v);
                mtx[0][3] = parts->mat[0][3] + v.x;
                mtx[1][3] = parts->mat[1][3] + v.y;
                mtx[2][3] = parts->mat[2][3] + v.z;
                w->parentWorld = pEffParentWorld;
                Efm04RotMatrix(obj, mtx);
            } else {
                // parent/parentSerial stored through a word pointer: the store `(mem link)` has a
                // register address, so cse1 (following the `beq` into this arm) treats it as
                // aliasing `w->x79` and the getPartsPtr argument is reloaded (`lbz r4,0x79(w)`);
                // combine folds the address back into `stw 0x6C(w)`. A plain member store never
                // conflicts (same base, disjoint offsets) and the arm reuses the switch register.
                u32* link = (u32*) &w->parent;
                link[0] = (u32) parent;
                link[1] = parent->serial;
                w->parentWorld = parent->getPartsPtr(w->x79);
                if (ofs) {
                    PSVECAdd(&obj->pos, ofs, &obj->pos);
                }
            }
        } else {
            pLog->err(0, 0, "ESP_EFM : PARTS_NO[%d] is invalid(MAX:%d).", w->x79, parent->nParts);
            ObjMgr.destroy(obj);
            return 0;
        }
        break;
    }
    if (w->flags & 4) {
        EstSet((int) obj, -1, 0, 0, gen->xFC, gen->xFD, 0, 0, (u32) obj, 0);
    }
    if (w->flags & 8) {
        w->x7B = gen->xFE;
        if (EspGetEfmMotAddr(gen->x2, w->x7B, &mot)) {
            switch (gen->xFF) {
            case 0:
                MotionSetCore(obj, &obj->pMotion, mot, 0, 0, 0, 0);
                break;
            case 1:
                MotionSetCore(obj, &obj->pMotion, mot, 0, 0, 4, 0);
                break;
            default:
                pLog->err(0, 0, "ESP_EFM04 : MotionType[%d] is invalid.", gen->xFF);
                break;
            }
        } else {
            pLog->err(0, 0, "ESP_EFM04 : MotionNo[%d] is invalid.", w->x7B);
            w->flags |= 8;
        }
    }
    obj->move();
    return obj;
}

cObj* EfmSetObj05(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate)
{
    Efm05Work* w = &obj->efm05;
    Vec v;
    Mtx mtx;
    f32 rnd;
    cModel* parts;
    cModel* p;
    u32 i;

    w->core = *info;
    w->flags = gen->flags;
    w->seed = *seed;
    obj->pos = gen->pos;
    obj->pos.x += gen->x18 * fRandSeed1_1(seed);
    obj->pos.y += gen->x1C * fRandSeed1_1(seed);
    obj->pos.z += gen->x20 * fRandSeed1_1(seed);
    obj->rot = gen->x58;
    obj->rot.x += gen->x64.x * fRandSeed1_1(seed);
    obj->rot.y += gen->x64.y * fRandSeed1_1(seed);
    obj->rot.z += gen->x64.z * fRandSeed1_1(seed);
    PSVECScale(&obj->rot, &obj->rot, DEG2RAD);
    if ((w->flags & 0x10) && parent) {
        Matrix2AxisAngle(parent->pParts->mat, &v);
        PSVECAdd(&obj->rot, &v, &obj->rot);
    }
    if (w->flags & 0x200000) {
        obj->x12C = 1;
    }
    w->rotSpd = gen->x70;
    w->rotSpd.x += gen->x7C.x * fRandSeed1_1(seed);
    w->rotSpd.y += gen->x7C.y * fRandSeed1_1(seed);
    w->rotSpd.z += gen->x7C.z * fRandSeed1_1(seed);
    PSVECScale(&w->rotSpd, &w->rotSpd, DEG2RAD);
    w->scaleXZ = gen->x88 * 0.005f;
    w->scaleY = gen->x8C * 0.005f;
    w->scale = 1.0f;
    rnd = gen->x90 * fRandSeed1_1(seed);
    w->scaleXZ += rnd;
    w->scaleY += rnd;
    w->scaleSpd = gen->x94;
    w->scaleDamp = gen->x98;
    w->r0 = gen->x9C;
    w->g0 = gen->x9D;
    w->b0 = gen->x9E;
    w->a0 = gen->x9F;
    w->r = (f32) gen->x9C;
    w->g = (f32) gen->x9D;
    w->b = (f32) gen->x9E;
    w->a = (f32) gen->x9F;
    w->rMul = gen->xA0;
    w->gMul = gen->xA4;
    w->bMul = gen->xA8;
    w->aMul = gen->xAC;
    if (gen->flags & 0x400000) {
        obj->x12F = 2;
    } else if (w->a < 250.0f) {
        obj->x12F = 1;
    } else {
        obj->x12F = 0;
    }
    w->fadeStart = gen->xB0;
    w->fadeLen = gen->xB2;
    w->x54 = gen->xB4;
    w->scaleStart = gen->xB6;
    w->life = gen->xB8;
    w->frame = gen->xBA;
    obj->pInfo->color[0] = (u8) w->r;
    obj->pInfo->color[1] = (u8) w->g;
    obj->pInfo->color[2] = (u8) w->b;
    obj->pInfo->color[3] = 0xFF;
    if (w->fadeStart == 0) {
        obj->alpha = w->a * (1.0f / 255.0f);
    } else {
        obj->alpha = 0.0f;
    }
    obj->pInfo->xD6 = gen->xC2;
    if (!(w->flags & 0x40)) {
        obj->lightInfo.x54 = 0;
        obj->be_flag |= 0x20000;
    }
    w->center = gen->vD8;
    w->center.x += gen->vF0.x * fRandSeed1_1(seed);
    w->center.y += gen->vF0.y * fRandSeed1_1(seed);
    w->center.z += gen->vF0.z * fRandSeed1_1(seed);
    w->bounce = gen->vE4;
    PSVECScale(&w->bounce, &w->bounce, 0.1f);
    w->pow = gen->xC8;
    w->rangeStep = gen->xC9;
    w->rnd = gen->xCA;
    w->rotAmp = gen->xCB;
    w->grav = (f32) (int) gen->prm.w.xCC * -0.1f;
    w->spdDamp = 1.0f - (f32) (int) gen->prm.w.xD0 * 0.001f;
    w->groundOfs = gen->xD4;
    switch (gen->x7) {
    case 0xFF:
        Efm05RotMatrix(obj, m);
        break;
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
        Efm05RotMatrix(obj, m);
        break;
    case 0xFE:
        if (gen->xC0 != 0) {
            pLog->warn(0, 0, "ESP_EFM : ReleaseTime not 0 but no parent.");
        }
        break;
    default:
        if (parent == 0) {
            pLog->err(0, 0, "ESP_EFM : PARTS_NO[%d] but Not on parts.", gen->x7);
            ObjMgr.destroy(obj);
            return 0;
        }
        if (gen->x7 < parent->nParts) {
            if (w->flags & 0x20) {
                parts = parent->getPartsPtr(gen->x7);
                PSMTXIdentity(mtx);
                RotMatrix(mtx, &parent->rot);
                PSMTXMultVecSR(mtx, &obj->pos, &v);
                mtx[0][3] = parts->mat[0][3];
                mtx[1][3] = parts->mat[1][3];
                mtx[2][3] = parts->mat[2][3];
                Efm05RotMatrix(obj, mtx);
            } else {
                parts = parent->getPartsPtr(gen->x7);
                Efm05RotMatrix(obj, parts->mat);
            }
        } else {
            pLog->err(0, 0, "ESP_EFM : PARTS_NO[%d] is invalid(MAX:%d).", gen->x7, parent->nParts);
            ObjMgr.destroy(obj);
            return 0;
        }
        break;
    }
    for (p = obj->pParts, i = 0; i < obj->nParts; i++, p = p->pParts) {
        p->efmStat = 0;
    }
    obj->matUpdate();
    if (w->flags & 4) {
        EstSet((int) obj, -1, 0, 0, gen->xFC, gen->xFD, 0, 0, (u32) obj, 0);
    }
    if (gen->xFE != 0) {
        setModTexRender(obj, gen->xFE - 1);
    }
    obj->be_flag |= 0x1000;
    obj->move();
    return obj;
}

cObj* EfmSetObj09(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate)
{
    Efm09Work* w = &obj->efm09;
    static f32 mass_mul = 1.0f;
    static f32 moment_mul = 2.0f;

    BitOn(obj->be_flag, 0x10);
    if (pG->flags_64 & 0x00800000) {
        pG->flags_58 &= ~0x02000000;
    }
    w->core = *info;
    w->basePos = gen->pos;
    w->basePos.x += gen->x18 * fRandSeed1_1(seed);
    w->basePos.y += gen->x1C * fRandSeed1_1(seed);
    w->basePos.z += gen->x20 * fRandSeed1_1(seed);
    w->basePos.y += 0.0001f;
    w->pos = w->basePos;
    w->spd = gen->x24;
    w->spd.x += gen->x34.x * fRandSeed1_1(seed);
    w->spd.y += gen->x34.y * fRandSeed1_1(seed);
    w->spd.z += gen->x34.z * fRandSeed1_1(seed);
    PSMTXIdentity(w->mat);
    w->x74.x = 0.0f;
    w->x74.y = 0.0f;
    w->x74.z = 0.0f;
    w->rotSpd = gen->x70;
    w->rotSpd.x += gen->x7C.x * fRandSeed1_1(seed);
    w->rotSpd.y += gen->x7C.y * fRandSeed1_1(seed);
    w->rotSpd.z += gen->x7C.z * fRandSeed1_1(seed);
    w->size.x = gen->xD8 * 100.0f + 250.0f;
    w->size.y = gen->xDC * 100.0f + 250.0f;
    w->size.z = gen->xE0 * 100.0f + 250.0f;
    w->mass = w->size.x * w->size.y * w->size.z / 1000000000.0f;
    w->mass *= FRef(mass_mul);
    PSVECScale(&w->spd, &w->spd, w->mass * 100.0f);
    w->momentX = FRef(moment_mul) * w->mass * (w->size.y * w->size.y + w->size.z * w->size.z) / 12.0f;
    w->momentY = FRef(moment_mul) * w->mass * (w->size.x * w->size.x + w->size.z * w->size.z) / 12.0f;
    w->momentZ = FRef(moment_mul) * w->mass * (w->size.x * w->size.x + w->size.y * w->size.y) / 12.0f;
    obj->scale = w->size;
    PSVECScale(&obj->scale, &obj->scale, 0.01f);
    if (gen->x2 == 0x7C) {
        obj->scale.x *= 0.05f;
        obj->scale.y *= 0.05f;
        obj->scale.z *= 0.05f;
    }
    if (gen->x2 == 0x21) {
        obj->scale.x *= 0.5f;
        obj->scale.y *= 0.5f;
        obj->scale.z *= 0.5f;
    }
    obj->move();
    return obj;
}

cObj* SetEffModel(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    Efm04Work* w;

    obj = ObjMgr.createBack(4);
    if (obj) {
        if (obj->modelInit(bin, tpl) == 0) {
            ObjMgr.destroy(obj);
            pLog->err(0, 0, "ESP_EFM : ModelInit() failed.");
            return 0;
        }
        obj->sub2B4.clrFlags(0xFCFF);
        obj->lightInfo.init2(0, 1, &efm_light_pos, &efm_light_size, 0x10);
        obj->id = 4;
        obj->setNoSuspend(1);
        w = &obj->efm04;
        w->core.flg = 1;
        obj->be_flag |= 0x4000;
        w->x79 = 0;
        w->flags = 0;
        obj->pos = *pos;
        w->spdDamp = 0.0f;
        obj->rot = *rot;
        w->scaleXZ = 1.0f;
        w->scaleY = 1.0f;
        w->scale = 1.0f;
        w->rMul = 1.0f;
        w->gMul = 1.0f;
        w->r0 = 0xFF;
        w->g0 = 0xFF;
        w->b0 = 0xFF;
        w->a0 = 0xFF;
        w->r = (f32) w->r0;
        w->g = (f32) w->g0;
        w->b = (f32) w->b0;
        w->a = (f32) w->a0;
        w->bMul = 1.0f;
        w->aMul = 1.0f;
        obj->x12F = 1;
        obj->pInfo->color[0] = (u8) w->r;
        obj->pInfo->color[1] = (u8) w->g;
        obj->pInfo->color[2] = (u8) w->b;
        obj->pInfo->color[3] = 0xFF;
        if (w->fadeStart == 0) {
            obj->alpha = w->a * (1.0f / 255.0f);
        } else {
            obj->alpha = 0.0f;
        }
        obj->scale.y = w->scaleY * w->scale;
        obj->scale.z = obj->scale.x = w->scaleXZ * w->scale;
        w->parentWorld = pEffParentWorldS;
        obj->move();
    }
    return obj;
}

void setModTexRender(cObj* obj, int no)
{
    static u8 buf[0x20];
    u8* tbl = buf;
    TexRenderMng* mgr = GetTexRenderMgrAddr(no);

    if (mgr->used == 0) {
        return;
    }
    tbl[0] = 1;
    tbl[1] = 0;
    tbl[4] = 0xF7;
    tbl[5] = mgr->texId;
    obj->pInfo->setTexBlendTbl(tbl);
    obj->pInfo->setBlendRatio(0xFF);
    obj->x136 = 1;
    obj->x137 = 0xF;
    obj->x138 = 0xB4;
}

// .sdata alignment padding of the split object
asm(".section .sdata; .balign 8");
