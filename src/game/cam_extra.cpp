#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_extra.h"
#include "atari.h"
#include "db_log.h"
#include "math_sub.h"
#include "model.h"
#include "motion.h"
#include "player.h"
#include "em.h"
#include "id_sys.h"
#include "main.h"
#include "joy.h"
#include "rnd.h"
#include "mes.h"
#include "cockpit.h"

// Weapon archive (pG->pWepArc): offsets to its sub-files.
#define WEP_ARC_PTR(no) PL_ARC_PTR((PlArc*) pG->pWepArc, no)

extern "C" {
void* memset(void* dst, int c, unsigned int n);
f32 atan2f(f32, f32);
void Filter01SetParam(int mode, int z, u8 type, f32 level);
void IdTexRelease(int id);
int IdTexDataLoad(void* data, int id);
}

extern u8 use_filter0a;
extern u8 filter0a_mask_flag;
extern u8 filter0a_mask_id;
extern u8 filter0a_mask_alpha;

#define PI 3.1415927f
#define DEG 0.017453292f

#define MTX_COPY(src, dst)               \
    {                                    \
        MtxPtr d_ = (dst);               \
        MtxPtr s_ = (src);               \
        int i_ = 3;                      \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        while (i_--) {                   \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        }                                \
    }

static u8 init_focus_frame = 30;
static u8 init_alpha_max = 0xFF;
f32 focus_frame = 5.0f;
u8 alpha_max = 0xDC;

// ---------------------------------------------------------------------------
// CameraAttachedToMotion: follows the AttachCamera channels of a model's motion.
// ---------------------------------------------------------------------------

CameraAttachedToMotion::CameraAttachedToMotion(cModel* m)
{
    model = m;
}

CameraAttachedToMotion::~CameraAttachedToMotion()
{
    memset(this, 9, 0x200);
}

void CameraAttachedToMotion::move()
{
    AttachCamera* ac = MOTION(model)->cam;
    Vec hit;
    Vec nrm;
    Vec pos;
    Vec at;
    Vec d;
    Mtx inv;
    Vec to;
    Vec from;

    if (ac == 0) {
        return;
    }
    if (ac->parts[0] != 0xFF) {
        param.pos = ac->out[0];
        PSMTXMultVec(*ac->pMat, &param.pos, &param.pos);
    }
    if (ac->parts[1] != 0xFF) {
        param.at = ac->out[1];
        PSMTXMultVec(*ac->pMat, &param.at, &param.at);
    }
    if (ac->parts[2] != 0xFF) {
        param.roll = ac->out[2].y;
    }
    if (ac->parts[3] != 0xFF) {
        param.fovy = ac->out[3].y * 180.0f / PI;
    }
    if (!(MOTION(model)->flags & 0x200)) {
        PSMTXInverse(model->mat, inv);
        PSMTXMultVec(inv, &param.pos, &pos);
        PSMTXMultVec(inv, &param.at, &at);
        // Frame order hit, nrm, pos, at, d, inv, to, from; the model-space test is at.z > 0 && pos.z < 0
        // and the hit check runs from the transformed `at` copy to the `pos` copy.
        if (at.z > 0.0f && pos.z < 0.0f) {
            PSVECSubtract(&at, &pos, &d);
            PSVECScale(&d, &d, -pos.z / d.z);
            PSVECAdd(&pos, &d, &to);
            PSMTXMultVec(model->mat, &to, &param.at);
        }
        to = param.at;
        from = param.pos;
        if (cameraHitCheck(&hit, &nrm, &to, &from)) {
            param.pos = hit;
        }
    }
    CameraSetOrientationRoll(this);
}

// ---------------------------------------------------------------------------
// FocusAnimation: filter0a blur fade.
// ---------------------------------------------------------------------------

void FocusAnimation::init(int id)
{
    filter0a_mask_alpha = alpha_max;
    if (id < 0) {
        filter0a_mask_flag = use_filter0a = 0;
    } else {
        filter0a_mask_id = id;
        filter0a_mask_flag = use_filter0a = 1;
    }
    state = 1;
    frame = (f32) init_focus_frame;
    alpha = init_alpha_max;
    count = (int) frame;
}

void FocusAnimation::move(int dir)
{
    static int _filter0a_flag = 0;
    static f32 level_max = 7.0f;

    if (dir != 0) {
        frame = focus_frame;
        alpha = alpha_max;
    }
    switch (state) {
    case 0:
        if (dir == 1) {
            count++;
            if (count >= (int) frame) {
                state = 1;
            }
        } else {
            count = 0;
        }
        break;
    case 1:
        if (dir == 0) {
            count--;
            if (count < 0) {
                state = 0;
                frame = focus_frame;
                alpha = alpha_max;
            }
        } else {
            count = (int) frame;
        }
        break;
    }
    if (_filter0a_flag) {
        int cnt;
        if ((f32) count > frame) {
            cnt = (int) frame;
        } else {
            cnt = count;
        }
        filter0a_mask_alpha = (u8) ((f32) (alpha * cnt) / frame);
    } else {
        int cnt;
        if ((f32) count > frame) {
            cnt = (int) frame;
        } else {
            cnt = count;
        }
        Filter01SetParam(1, 100, 1, level_max * (f32) cnt / frame);
        filter0a_mask_alpha = 0;
    }
}

void FocusAnimation::quit()
{
    use_filter0a = 0;
    filter0a_mask_flag = 0;
    filter0a_mask_id = 0;
}

void FocusAnimation::clear()
{
    state = 0;
    count = 0;
    frame = focus_frame;
    alpha = alpha_max;
}

// ---------------------------------------------------------------------------
// CameraScope: rifle scope view.
// ---------------------------------------------------------------------------

// The wep_type filter is an if/else-if chain on a local (`t == 0`, `== 1`, `== 2`, each storing
// `t`; jump2 cross-jumps the three `stb`s) -- a switch or `||` on one value range-folds; the 9/10/0x28
// arm is one body (its label has a jump use, so cse reloads pG there) and the `type = 0` arm is written
// last so its `stb` is the cross-jump survivor; `&dir` is written per use (a `Vec* dir` local
// merges the arms' PRE copies `mr r29, ..`).
#define SCOPE_WEP_TYPE()                                                                             \
    {                                                                                                \
        int t = pG->wep_type;                                                                        \
        if (t == 0) {                                                                                \
            type = t;                                                                                \
        } else if (t == 1) {                                                                         \
            type = t;                                                                                \
        } else if (t == 2) {                                                                         \
            type = t;                                                                                \
        }                                                                                            \
    }

struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)
CameraScope::CameraScope(Vec* pos, Vec* at)
{
    Mtx inv;
    const f32 len = 300000.0f;

    PSMTXInverse(pPLS->mat, inv);
    if (pos && at) {
        pos_ofs = *pos;
        PSVECSubtract(at, pos, &dir);
    } else {
        cModel* p[2];
        Vec* d;

        p[0] = pPL->getPartsPtr(0x20);
        p[1] = pPL->getPartsPtr(0x21);
        PSVECAdd(&p[0]->worldPos, &p[1]->worldPos, &pos_ofs);
        PSVECScale(&pos_ofs, &pos_ofs, 0.5f);
        d = &dir;
        d->x = pPL->mat[0][2];
        d->y = pPL->mat[1][2];
        d->z = pPL->mat[2][2];
    }
    PSMTXMultVec(inv, &pos_ofs, &pos_ofs);
#line 306 "D:/Bio4/Prog/cam_extra.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, len);
    PSMTXMultVecSR(inv, &dir, &dir);
    switch (pG->wep_no) {
    case 9:
    case 10:
    case 0x28:
        SCOPE_WEP_TYPE();
        break;
    case 13:
    case 14:
    case 0x1D:
        type = 0;
        break;
    }
    f32 zero = 0.0f;
    param.fovy = 45.0f;
    angle_min = -70.0f * 3.1415927f / 180.0f;
    angle_max = 70.0f * 3.1415927f / 180.0f;
    param.roll = zero;
    zoom = zero;
    angle_x = zero;
    yure.x = zero;
    yure.y = zero;
    yure.z = zero;
    id.init(&type);
    focus.init(0x9A);
}

CameraScope::~CameraScope()
{
    id.quit(0);
    focus.quit();
    memset(this, 9, 0x200);
}

void CameraScope::setParam(f32 a, f32 b)
{
    zoom = a;
    angle_x = b;
    focus.clear();
}

void CameraScope::getParam(f32* a, f32* b)
{
    *a = zoom;
    *b = angle_x;
}

// Reading a static through a reference (`FRef`) gives a MEM with neither the struct nor the scalar
// flag: the range loads stay below the reticle stores through the call-result pointers.
static inline f32 FRef(f32& v) { return v; }
// Same for a global pointer: the `lwz pPL` then waits for a preceding member store in sched1.
static inline cPlayer* PlRef(cPlayer*& p) { return p; }

// Matrix column -> vector. The destination is the frame-offset-0 local in both users (`inv` in
// CameraPushObject::move, `dir` in IdBinocular::move), so its address is the bare virtual frame
// register and integrate keeps it as a pointer pseudo (`addi r9,r1,8`, stores/loads through r9);
// the other plmat reads are written directly and go via r1.
static inline void getColumn(Mtx m, int c, Vec* v)
{
    v->x = m[0][c];
    v->y = m[1][c];
    v->z = m[2][c];
}

// Scope zoom clamp as an inline returning the value: one store after the join, the 0.0 register
// doubling as the result (`fmr f13,f0` / `fmr f13,f12` copies).
static inline f32 scopeClamp01(f32 v)
{
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

void CameraScope::move()
{
    static f32 ZOOM_LIMIT_0 = 9.0f;
    static f32 ZOOM_LIMIT_1 = 3.0f;
    static f32 SCOP_VEL_Y = 0.062831856f;
    static f32 SCOP_VEL_X = 0.062831856f;
    static f32 rnd_gain = 0.001f;   // unreferenced, still in .sdata
    static int rnd_on = 1;          // unreferenced, still in .sdata
    static f32 yure_spd = 0.062831856f;
    static f32 rnd_gain2 = 0.0005f;
    static u8 sct_max = 0x96;       // unreferenced, still in .sdata
    static f32 x_yure_spd;
    static f32 y_yure_spd;
    static f32 xtime;
    static f32 ytime;
    static f32 rdir0;
    static f32 rdir = 0.0f;
    static u8 sct = 0;
    static int pastkey = 0;
    f32 old_zoom = zoom;
    f32 gain;
    f32 limit;
    f32 add;
    f32 ang;
    Mtx m;
    Vec dir;
    Vec ofs;
    Vec yure2;

    param.fovy = 45.0f;
    if (Key.on & 0x10) { // low word bit 4 (the target masks the low half of the u64)
        f32 sy = (f32) Joy[0].ssy;
        if (sy != 0.0f) {
            zoom = old_zoom + sy * 0.001f;
        }
    }
    zoom = scopeClamp01(zoom);
    if (zoom != 0.0f) {
        limit = ZOOM_LIMIT_0;
        switch (type) {
        case 0:
        case 2:
            limit = ZOOM_LIMIT_0;
            break;
        case 1:
            limit = ZOOM_LIMIT_1;
            break;
        }
        param.fovy = zoom * (limit - param.fovy) + param.fovy;
    }
    gain = zoom * -0.9f + 1.0f;
    if (Key.on & 0x10) {
        if (Joy[0].sx != 0 || (Joy[0].on & 3)) {
            add = gain * (f32) Joy[0].sx * -0.05f * DEG;
            if (Joy[0].on & 1) {
                add = gain * SCOP_VEL_Y + add;
            }
            if (Joy[0].on & 2) {
                add = add - gain * SCOP_VEL_Y;
            }
            pPL->rot.y += add;
        }
    }
    if (Key.on & 0x10) {
        if (Joy[0].sy != 0 || (Joy[0].on & 0xC)) {
            add = gain * (f32) Joy[0].sy * -0.05f * DEG;
            if (Joy[0].on & 8) {
                add = add - gain * SCOP_VEL_X;
            }
            if (Joy[0].on & 4) {
                add = gain * SCOP_VEL_X + add;
            }
            if (pSys->flags & 0x80000000) {
                add = -add;
            }
            ang = angle_x;
            if (ang + add < angle_min) {
                add = angle_min - ang;
            } else if (ang + add > angle_max) {
                add = angle_max - ang;
            }
            angle_x += add; // `+=`: the sum is tied to the load register, `ang` copied (`fmr f11,f0`)
        }
    }
    {
        u8 c = sct--; // the old value in a byte local: `clrlwi r0,r9,24` + `addi r9,r9,255`
        if (c == 0) {
        x_yure_spd = yure_spd * (fRand1_1() * 0.5f + 1.0f);
        y_yure_spd = yure_spd * (fRand1_1() * 0.5f + 1.0f);
        rdir0 = fRand0_1() * rnd_gain2;
        sct = 0x96;
    }
    }
    rdir = rdir * 0.95f + rdir0 * 0.05f;
    yure.x = COSF(xtime) * rdir;
    yure.y = COSF(FRef(ytime)) * (rnd_gain2 - rdir); // FRef: the static loads wait for the yure stores
    xtime = LIMIT_ANGLE(FRef(xtime) + FRef(x_yure_spd));
    ytime = LIMIT_ANGLE(ytime + y_yure_spd);
    if (pastkey != 1 || (*(u32*) &Joy[0] & 0xFFFF0000)) { // the first word of Joy[0] (sx/sy bytes), not `on`
        Vec* a = (Vec*) &angle_x;
        PSVECAdd(a, &yure, a);
        yure2 = *a;
    }
    PSMTXRotRad(m, 'x', yure2.x);
    PSMTXMultVecSR(m, &this->dir, &dir);
    PSMTXRotRad(m, 'y', yure2.y);
    PSMTXMultVecSR(m, &dir, &dir);
    PSVECAdd(&pos_ofs, &dir, &ofs);
    {
        cModel* pl = pPL; // held in r30 across the four calls, `&pl->worldMat` in r29
        RotMatrix(pl->worldMat, &pl->rot);
        TransMatrix(pl->worldMat, &pl->pos);
        ScaleMatrix(pl->worldMat, &pl->scale);
        PSMTXCopy(pl->worldMat, pl->mat);
    }
    PSMTXMultVec(pPL->mat, &pos_ofs, &param.pos);
    PSMTXMultVec(pPL->mat, &ofs, &param.at);
    CameraSetOrientationZeroRoll(this);
    id.move(&zoom);
    if (old_zoom != zoom) {
        focus.move(1);
    } else {
        focus.move(0);
    }
}

// ---------------------------------------------------------------------------
// IdScope
// ---------------------------------------------------------------------------

void IdScope::init(void* type)
{
    s8 t = *(u8*) type;

    IdTexDataLoad(WEP_ARC_PTR(4), 0xB);
    switch (t) {
    case 0:
        IdSys.set(WEP_ARC_PTR(5), 0xFF, 0x25, 0x13, 6, 0);
        break;
    case 1:
        IdSys.set(WEP_ARC_PTR(6), 0xFF, 0x25, 0x13, 6, 0);
        break;
    case 2:
        IdSys.set(WEP_ARC_PTR(7), 0xFF, 0x25, 0x13, 6, 0);
        break;
    }
}

void IdScope::move(void* p)
{
    f32* zoom = (f32*) p;
    static f32 minA = -90.0f;
    static f32 maxA = 180.0f;
    static f32 ampA = 0.08f;
    static int spdA = 45;
    static f32 minB = 90.0f;
    static f32 maxB = -90.0f;
    static f32 ampB = 0.05f;
    static int spdB = 60;
    IdUnit* a;
    IdUnit* b;
    f32 ra;
    f32 rb;

    if (pG->wep_no != 0xE) {
        return;
    }
    ra = ampA * SINF((f32) (pG->flags_51E4 % spdA) * 6.2831855f / (f32) spdA) + *zoom;
    rb = ampB * COSF((f32) (pG->flags_51E4 % spdB) * 6.2831855f / (f32) spdB) + *zoom;
    a = IdSys.unitPtr(1, 0x25);
    b = IdSys.unitPtr(2, 0x25);
    a->curve[3] = 0;
    b->curve[3] = 0;
    a->rot.y = 0.0f;
    a->rot.x = 0.0f;
    a->rot.z = (FRef(maxA) - FRef(minA)) * ra + FRef(minA);
    b->rot.y = 0.0f;
    b->rot.x = 0.0f;
    b->rot.z = (FRef(maxB) - FRef(minB)) * rb + FRef(minB);
}

// Never called: its body is stripped at link (STRIP_UNUSED) but its pool words (0.5f, 100000.0f)
// follow IdScope::move's in the original `.rodata`.
static int IdScopeZoomDisp(f32* zoom)
{
    return (int) ((*zoom + 0.5f) * 100000.0f);
}

void IdScope::save(int)
{
    save_a = (s16) IdSys.unitPtr(0, 0x25)->timer[0];
    save_b = (s16) IdSys.unitPtr(0x10, 0x25)->timer[1];
}

void IdScope::load(int)
{
    IdSys.unitPtr(0, 0x25)->timer[0] = save_a;
    IdSys.unitPtr(0x10, 0x25)->timer[1] = save_b;
    IdSys.unitPtr(0x11, 0x25)->timer[1] = save_b;
    IdSys.unitPtr(0x12, 0x25)->timer[1] = save_b;
    IdSys.unitPtr(0x13, 0x25)->timer[1] = save_b;
}

void IdScope::quit(void*)
{
    IdTexRelease(0xB);
    IdSys.kill(0xFF, 0x25);
}

// ---------------------------------------------------------------------------
// CameraBinocular
// ---------------------------------------------------------------------------

// Frame order c 0x8, up 0x18, inv 0x28 (declaration order); the else arm keeps the getPartsPtr
// results in cModel* locals and writes this->up through a `Vec* u`. OPEN (17 words): the else arm's
// gcse copies of `&param.pos`/`&param.at` (`addi r30,r31,164; addi r29,r31,176; mr r26; mr r25`) sit
// before the first getPartsPtr call in the target and after the second one in ours.
CameraBinocular::CameraBinocular(Vec* pos, Vec* at, void* a, void* b)
{
    Vec c;
    Vec up;
    Mtx inv;

    id_a = a;
    id_b = b;
    if (pos && at) {
        mode = 0;
        param.pos = *pos;
        param.at = *at;
        this->up.x = 0.0f;
        this->up.y = 1.0f;
        this->up.z = 0.0f;
    } else {
        mode = 1;
        cModel* p[2];
        p[0] = PlRef(pPL)->getPartsPtr(0x20); // the load waits for the `mode` store: the two
                                              // param addresses go above the call
        p[1] = pPL->getPartsPtr(0x21);
        PSVECAdd(&p[0]->worldPos, &p[1]->worldPos, &c);
        PSVECScale(&c, &c, 0.5f);
        up.x = pPL->mat[0][2];
        up.y = pPL->mat[1][2];
        up.z = pPL->mat[2][2];
        param.pos = c;
        PSVECAdd(&c, &up, &param.at);
        {
            Vec* u = &this->up;
            u->x = pPL->mat[0][1];
            u->y = pPL->mat[1][1];
            u->z = pPL->mat[2][1];
        }
    }
    param.fovy = 45.0f;
    CameraSetOrientationUp(this);
    if (mode != 0) {
        PSMTXInverse(pPL->mat, inv);
        PSMTXMultVec(inv, &param.pos, &pos_local);
        PSMTXMultVec(inv, &param.at, &at_local);
        PSMTXMultVecSR(inv, &this->up, &up_local);
    }
    // Store order pinned by the dying-store rule (the last use of each constant is issued first).
    x124 = 0.0f;
    x100 = 0.0f;
    x104 = 0.0f;
    x10C = -1.0471976f;
    x110 = -1.0471976f;
    x118 = 1.0471976f;
    x11C = 1.0471976f;
    id.init(this, id_a, id_b);
    focus.init(-1);
}

CameraBinocular::~CameraBinocular()
{
    id.quit(this);
    focus.quit();
    memset(this, 9, 0x200);
}

void CameraBinocular::setRange(f32 a, f32 b, f32 c, f32 d)
{
    x10C = a;
    x110 = c;
    x118 = b;
    x11C = d;
}

void CameraBinocular::move()
{
    static f32 zoom_limit = 3.0f;
    static f32 BINO_VEL_Y = 0.062831856f;
    static f32 BINO_VEL_X = 0.062831856f;
    f32 old_zoom = x124;
    f32 gain;
    f32 add;
    f32 ang;

    if (mode != 0) {
        param.pos = pos_local;
        param.at = at_local;
        up = up_local;
        CameraSetOrientationUp(this);
    }
    param.fovy = 45.0f;
    {
        f32 sy = (f32) Joy[0].ssy;
        if (sy != 0.0f) {
            x124 = sy * 0.001f + x124;
        }
    }
    {
        // clamped copy kept in a register (`fmr f12`), stored once, reused by the fovy formula
        f32 zoom = (x124 < 0.0f) ? 0.0f : (x124 > 1.0f) ? 1.0f : x124;
        x124 = zoom;
        if (zoom != 0.0f) {
            param.fovy = zoom * (zoom_limit - param.fovy) + param.fovy;
        }
    }
    gain = x124 * -0.9f + 1.0f;
    if (Joy[0].sx != 0 || (Joy[0].on & 3)) {
        Vec axis = {0.0f, 1.0f, 0.0f}; // initialised inside this block (the stores sit below the stb)
        add = gain * (f32) Joy[0].sx * -0.05f * DEG;
        if (Joy[0].on & 1) {
            add = gain * BINO_VEL_Y + add;
        }
        if (Joy[0].on & 2) {
            add = add - gain * BINO_VEL_Y;
        }
        ang = x104;
        if (ang + add < x110) { // two arms: the `fsubs` tails are cross-jumped, each with its own limit
            add = x110 - ang;
        } else if (ang + add > x11C) {
            add = x11C - ang;
        }
        CameraRotAxisPosRad(this, &axis, &param.pos, add);
        x104 = x104 + add;
    }
    if (Joy[0].sy != 0 || (Joy[0].on & 0xC)) {
        add = gain * (f32) Joy[0].sy * 0.05f * DEG;
        if (Joy[0].on & 8) {
            add = gain * BINO_VEL_X + add;
        }
        if (Joy[0].on & 4) {
            add = add - gain * BINO_VEL_X;
        }
        if (pSys->flags & 0x80000000) {
            add = -add;
        }
        ang = x100;
        if (ang + add < x10C) {
            add = x10C - ang;
        } else if (ang + add > x118) {
            add = x118 - ang;
        }
        CameraTargetRot(this, 'x', add);
        x100 = x100 + add;
    }
    if (mode != 0) {
        pos_local = param.pos;
        at_local = param.at;
        up_local = up;
        PSMTXMultVec(PlRef(pPL)->mat, &pos_local, &param.pos); // `lwz pPL` after the three copies
        PSMTXMultVec(pPL->mat, &at_local, &param.at);
        PSMTXMultVecSR(pPL->mat, &up_local, &up);
    }
    CameraSetOrientationUp(this); // unconditional: mode 0 jumps to it
    id.move(this);
    if (old_zoom != x124) {
        focus.move(1);
    } else {
        focus.move(0);
    }
}

// ---------------------------------------------------------------------------
// IdBinocular
// ---------------------------------------------------------------------------

void IdBinocular::init(Camera* cam, void* a, void* b)
{
    IdUnit* u;

    IdSys.kill(0xFF, 0x21);
    IdSys.kill(0xFF, 0x20);
    IdSys.kill(0xFF, 0x23);
    IdSys.kill(0xFF, 0x30);
    pG->flags_170 |= 0x100;
    IdTexRelease(4);
    IdTexDataLoad(a, 4);
    IdSys.set(b, 0xFF, 0x24, 0x13, 5, 0);
    scr1 = IdSys.unitPtr(1, 0x24)->scr;
    scr3 = IdSys.unitPtr(2, 0x24)->scr;
    scr2 = IdSys.unitPtr(3, 0x24)->scr;
    if (pGS->flags_500C & 0x1000) {
        IdSys.unitPtr(0x30, 0x24)->flags &= ~8;
        IdSys.unitPtr(0x1B, 0x24)->flags &= ~8;
    }
    fovy = cam->param.fovy;
    u = IdSys.unitPtr(0x35, 0x24);
    scr35 = u->scr;
    sizeY = u->sizeY;
    sizeX = u->sizeX;
}

// The skipped ids are a switch (`||`/`&&` range tests fold to `cmplwi 4`). OPEN (3 words): the three
// `sth` come out in source order in the target although `u` dies at the last one.
void IdBinocular::cutin()
{
    int i;

    for (i = 0; i <= 0x40; i++) {
        switch (i) {
        case 0xD:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
            continue;
        }
        IdUnit* u = IdSys.unitPtr(i, 0x24);
        u->timer[0] = 0x96;
        u->timer[1] = 0x96;
        u->timer[2] = 0x96;
        // Codeless keep-alive on an unstored field: the original's u does not die at the last
        // store, so the three sth stay in source order; the non-volatile form (unlike the earlier
        // volatile input-only asm) is no scheduling barrier, so the unitPtr arg moves keep their order.
        asm("" : "=m"(u->timer[3]) : "r"(u)); // COMPILER-DIFF: #13 (keep-alive)
    }
}

void IdBinocular::move(void* p)
{
    Camera* cam = (Camera*) p;
    static f32 ratio = 0.5f;
    static f32 m = 0.5f;
    static f32 n = 1.0f;
    Vec dir;
    u8 digit[4];
    f32 ang;
    f32 lo;
    f32 hi;
    f32 rate;
    f32 y;
    int i;
    int cnt;
    int dist;

    getColumn(cam->mat, 2, &dir);
    ang = (4.712389f - atan2f(-dir.x, -dir.z)) / PI;
    {
        IdUnit* u = IdSys.unitPtr(0, 0x24);
        u->u0 = ang;
        u->u1 = ang + 1.0f;
        lo = ang - 0.5f;
        hi = ang + 0.5f;
    }
    // `cnt` crosses no call: each mark passes `cnt + 1` (a temp the call crosses) and cse turns the
    // `cnt++` after the stores into a copy of that temp, so sched1 keeps the `li cnt,0/1` below the
    // unitPtr calls (REG_N_CALLS_CROSSED == 0 anchor) and global gives cnt the temp's r30.
    cnt = 0;
    if (lo <= 0.0f && hi >= 0.0f) {
        IdUnit* u = IdSys.unitPtr(1, 0x24);
        u->flags |= 8;
        u->no = 3;
        u->flags_7F |= 2;
        u->scr.x = (0.0f - lo) * (scr2.x - scr1.x) + scr1.x;
        cnt = 1;
    }
    if (lo <= 0.5f && hi >= 0.5f) {
        IdUnit* u = IdSys.unitPtr(cnt + 1, 0x24);
        u->flags |= 8;
        u->no = 0;
        u->flags_7F |= 2;
        u->scr.x = (0.5f - lo) * (scr2.x - scr1.x) + scr1.x;
        cnt++;
    }
    if (lo <= 1.0f && hi >= 1.0f) {
        IdUnit* u = IdSys.unitPtr(cnt + 1, 0x24);
        u->flags |= 8;
        u->no = 1;
        u->flags_7F |= 2;
        u->scr.x = (1.0f - lo) * (scr2.x - scr1.x) + scr1.x;
        cnt++;
    }
    if (lo <= 1.5f && hi >= 1.5f) {
        IdUnit* u = IdSys.unitPtr(cnt + 1, 0x24);
        u->flags |= 8;
        u->no = 2;
        u->flags_7F |= 2;
        u->scr.x = (1.5f - lo) * (scr2.x - scr1.x) + scr1.x;
        cnt++;
    }
    if (lo <= 2.0f && hi >= 2.0f) {
        IdUnit* u = IdSys.unitPtr(cnt + 1, 0x24);
        u->flags |= 8;
        u->no = 3;
        u->flags_7F |= 2;
        u->scr.x = (2.0f - lo) * (scr2.x - scr1.x) + scr1.x;
        cnt++;
    }
    // The loop counter is a separate variable copied from cnt (the copy is a no-op after allocation and
    // delays the entry `cmpwi` one slot behind the hoisted `&digit`); it is reused by the digit loop.
    i = cnt;
    while (i <= 2) {
        i++;
        IdSys.unitPtr(i, 0x24)->flags &= ~8;
    }
    if (!(pG->flags_500C & 0x1000)) {
        IdUnit* u = IdSys.unitPtr(0x36, 0x24);
        MessageControl* mc;
        Message* ms;
        s16 x = (s16) ((u->scr.x + 320.0f) * 0.8f);
        s16 y = (s16) ((240.0f - u->scr.y) * 0.8f);
        cMes.setLayout(1, 1);
        mc = &cMes;
        ms = &mc->mes[1];
        mc->MesSet(1, x, (s16) (y - ms->fontH / 2), 0x20081, 1, 0, 4);
        u = IdSys.unitPtr(0x1B, 0x24);
        rate = u->col[3] / 255.0f;
        ms->color = ((u8) ((f32) (ms->color >> 24) * rate) << 24) |
                    ((u8) ((f32) ((ms->color >> 16) & 0xFF) * rate) << 16) |
                    ((u8) ((f32) ((ms->color >> 8) & 0xFF) * rate) << 8) |
                    (u8) ((f32) (ms->color & 0xFF) * rate);
    }
    {
        f32 t0[2] = {1.0f, 16.0f};
        f32 t1[2] = {45.0f, 3.0f};
        int d;
        dist = (int) ((t0[1] - t0[0]) * (cam->param.fovy - t1[0]) / (t1[1] - t1[0]) + t0[0]);
        d = dist * 10;
        for (i = 0; i < 4; i++) {
            digit[i] = d % 10;
            d /= 10;
        }
        for (int k = 0; k <= 3; k++) {
            IdUnit* u = IdSys.unitPtr(0x20 + k, 0x24);
            u->flags_7F |= 2;
            u->no = digit[k];
        }
        ratio = 1.0f - ((f32) dist - t0[0]) / (t0[1] - t0[0]);
    }
    {
        IdUnit* u = IdSys.unitPtr(0x35, 0x24);
        u->v0 = FRef(ratio);
        u->v1 = 1.0f;
        u->scr = scr35;
        u->scr.y = u->scr.y - sizeY * FRef(ratio) * FRef(m);
        u->sizeY = sizeY * (1.0f - FRef(ratio)) * FRef(n);
        y = (sizeY * 0.5f * 0.5f + u->scr.y) * 2.0f;
    }
    for (int k = 0; k <= 3; k++) {
        IdUnit* u = IdSys.unitPtr(5 + k, 0x24);
        if (y < u->pos.y) {
            u->flags &= ~8;
        } else {
            u->flags |= 8;
        }
    }
    IdSys.unitPtr(0x1C, 0x24)->flags &= ~8;
    IdSys.unitPtr(0x1D, 0x24)->flags &= ~8;
    if (cam->param.fovy > fovy) {
        IdSys.unitPtr(0x1D, 0x24)->flags |= 8;
    } else if (cam->param.fovy < fovy) {
        IdSys.unitPtr(0x1C, 0x24)->flags |= 8;
    }
    fovy = cam->param.fovy;
}

void IdBinocular::quit(void*)
{
    IdUnit* u = IdSys.unitPtr(0x35, 0x24);
    int i;

    u->scr = scr35;
    u->sizeY = sizeY;
    u->sizeX = sizeX;
    IdSys.kill(0xFF, 0x24);
    pG->flags_170 &= ~0x100;
    Cckpt.roomInit();
    if (pG->flags_500C & 0x1000) {
        Cckpt.lifeMeterDisp(0);
    }
    {
        MessageControl* mes = &cMes;
        for (i = 0; i <= 0xF; i++) {
            mes->Delete(i);
        }
    }
}

// ---------------------------------------------------------------------------
// CameraPushObject: pushing a heavy object, looks along the push direction.
// ---------------------------------------------------------------------------

CameraPushObject::CameraPushObject()
{
}

CameraPushObject::~CameraPushObject()
{
    memset(this, 9, 0x200);
}

void CameraPushObject::move()
{
    static f32 default_ofs[8] = {0.0f, 2000.0f, -2000.0f, 0.0f, 800.0f, 800.0f, 0.0f, 45.0f};
    Mtx inv;
    Mtx m;
    Mtx rot;
    Vec em_pos;
    Vec near_pos;
    Mtx plmat;
    Vec plpos;
    f32 ofs[8];   // the rotated default_ofs: v[0] at [0], v[1] at [3], roll/fovy at [6]/[7] (one 32-byte slot;
                  // [6]/[7] are stored to the frame and reloaded after the two inv-MultVec calls, not kept in f30/f31)
    cModel* em = 0;
    cModel* e;
    u32 i;

    MTX_COPY(pPL->mat, inv);
    PSMTXInverse(inv, m);
    plmat[0][0] = inv[0][0]; plmat[0][1] = inv[1][0]; plmat[0][2] = inv[2][0];
    plmat[1][0] = inv[0][1]; plmat[1][1] = inv[1][1]; plmat[1][2] = inv[2][1];
    getColumn(inv, 2, (Vec*) plmat[2]);
    plpos.x = inv[0][3]; plpos.y = inv[1][3]; plpos.z = inv[2][3];
    for (i = 0; i < EmMgr.nArray; i++) {
        e = (cModel*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->id == 0x41 || e->id == 0x44 || e->id == 0x46) && (e->be_flag & 0x201) == 1) {
            PSMTXMultVec(m, &e->pos, &em_pos);
            // negated tests: `blt` / `cror so,eq,gt; bso` (a positive `>=`/`<=` gives cror + bns)
            if (!(em_pos.z < 0.0f) && !(PSVECMag(&em_pos) >= 4000.0f)) {
                if (em == 0) {
                    em = e;
                    near_pos = em_pos;
                } else if (PSVECMag(&em_pos) <= PSVECMag(&near_pos)) {
                    em = e;
                    near_pos = em_pos;
                }
            }
        }
    }
    PSMTXIdentity(rot);
    if (em) {
        Vec look;
        Vec axis = {0.0f, 1.0f, 0.0f};
        if (em->id == 0x41) {
            look.x = ((f32*) em)[0x784 / 4];
            look.y = ((f32*) em)[0x794 / 4];
            look.z = ((f32*) em)[0x7A4 / 4];
        } else {
            look.x = em->mat[0][2];
            look.y = em->mat[1][2];
            look.z = em->mat[2][2];
        }
        // one call; `&look` is a plain call argument pseudo that gcse PREs together with `&hit`
        // (same frame slot) into the head register
        f32 ang = VecAngle(&look, (Vec*) &plmat[2]);
        if (ang > 0.7853982f && ang < 2.3561945f) {
            if (near_pos.x > 0.0f) {
                MtxRotAxisPosRad(rot, &axis, (Vec*) &default_ofs[3], 1.5707964f);
            } else {
                MtxRotAxisPosRad(rot, &axis, (Vec*) &default_ofs[3], -1.5707964f);
            }
        }
    }
    PSMTXMultVec(rot, (Vec*) &default_ofs[0], (Vec*) &ofs[0]);
    PSMTXMultVec(rot, (Vec*) &default_ofs[3], (Vec*) &ofs[3]);
    ofs[6] = default_ofs[6];
    ofs[7] = default_ofs[7];
    PSMTXMultVec(inv, (Vec*) &ofs[0], (Vec*) &ofs[0]);
    PSMTXMultVec(inv, (Vec*) &ofs[3], (Vec*) &ofs[3]);
    param.pos = *(Vec*) &ofs[0];
    param.at = *(Vec*) &ofs[3];
    param.roll = ofs[6];
    param.fovy = ofs[7];
    {
        Vec hit;
        Vec nrm;
        Vec from;
        Vec to;
        from = param.at;
        to = param.pos;
        if (cameraHitCheck(&hit, &nrm, &from, &to)) {
            param.pos = hit;
        }
    }
}

// ---------------------------------------------------------------------------
// CameraLookAt: falling camera, looks at a random hand of the player.
// ---------------------------------------------------------------------------

CameraLookAt::CameraLookAt(Camera* cam)
{
    Vec hit;
    Vec nrm;
    Vec from;
    Vec to;

    if (Rnd() & 1) {
        parts = pPL->getPartsPtr(1);
    } else {
        parts = pPL->getPartsPtr(2);
    }
    param.pos = cam->param.pos;
    param.at = cam->param.at;
    param.roll = cam->param.roll;
    param.fovy = cam->param.fovy;
    from = param.at;
    to = param.pos;
    if (cameraHitCheck(&hit, &nrm, &from, &to)) {
        param.pos = hit;
    }
}

CameraLookAt::~CameraLookAt()
{
    memset(this, 9, 0x200);
}

void CameraLookAt::move()
{
    Vec hit;
    Vec nrm;
    Vec from;
    Vec to;

    param.at = parts->worldPos;
    from = param.at;
    to = param.pos;
    if (cameraHitCheck(&hit, &nrm, &from, &to)) {
        param.pos = hit;
    }
}

// ---------------------------------------------------------------------------
// CameraLookDownEm
// ---------------------------------------------------------------------------

CameraLookDownEm::CameraLookDownEm(void* e, Vec* pos)
{
    parts = ((cModel*) e)->getPartsPtr(2);
    param.pos = *pos;
    param.at = parts->worldPos;
    param.roll = 0.0f;
    param.fovy = 45.0f;
}

CameraLookDownEm::~CameraLookDownEm()
{
    memset(this, 9, 0x200);
}

void CameraLookDownEm::move()
{
}

// The split object pads .sdata to 8 bytes (cam_qfps follows 8-aligned).
asm(".section .sdata; .balign 8");
