#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_extra.h"
#include "cam_motion.h"
#include "db_log.h"
#include "atari.h"
#include "light.h"
#include "model.h"
#include "player.h"
#include "em.h"
#include "main_mem.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "at_mod.h"
#include "joy.h"

extern "C" {
int strncmp(const char* a, const char* b, unsigned int n);
void* memset(void* dst, int c, unsigned int n);
void OSReport(const char* fmt, ...);
f32 sinf(f32);
f32 cosf(f32);
}

extern f32 ZNEAR;
u32 SubCharGetStatus();
int GetWaterHeight(Vec* pos, f32* height);
void QuakeInit();
void eprintf(int x, int y, int color, int a, const char* fmt, ...);


#define PI 3.1415927f
#define PI2 6.2831855f
#define DEG 0.017453292f

struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

void* g_pToolCamData = NULL;

#define CAMERA_MOTION_BUFFER_SIZE 0x440
static u8 CameraMotionBuffer[CAMERA_MOTION_BUFFER_SIZE];
extern CameraBSpline CamBSpline;

// internal linkage: the table is deferred behind the cManager template strings in .rodata
static const f32 smooth_ratio[12] = {0.0f, 0.9f, 0.85f, 0.92f, 0.8f, 0.92f, 0.9f, 0.9f, 0.9f, 0.9f, 0.0f, 0.0f};

// Byte-wise copy of the float `tmp` into the (unaligned) motion buffer. `&tmp` indexed directly
// (a pointer local is copy-propagated into loops 2/3), `n` is the function-scope counter shared
// by the three copies (one allocno -> r11 in all three, the `&tmp` copies fall to r12/r9/r9).
#define EXPORT_TMP(p)                             \
    {                                             \
        for (n = 0; n < 4; n++) {                 \
            *(p)++ = ((u8*) &tmp)[n];             \
        }                                         \
    }

// Converts a rail cut into the CameraMotion key-frame format (cam_motion): header, 4 channels
// (pos, at, roll, fovy) x 3 components of hermite keys {value, tangent in, tangent out}.
int CameraControl::HermiteExport(CameraCut* cut, u8* p)
{
    u8* buf = p;  // the parameter is the running pointer (r5: `sth 0(r5); stbu 2(r5); addi r5,1`), buf the saved base
    u32* table;
    u16* frames;
    int i;
    int j;
    int k;
    int k0;
    int k1;
    f32 tmp;
    f32 v;
    f32 tan;
    f32 v0;
    f32 v1;
    f32 dt0;
    f32 dt1;
    int n;

    *(u16*) p = (cut->num - 1) * 30;
    p += 2;
    *p++ = 4;
    for (i = 0; i < 4; i++) {
        switch (i) {
        case 0:
        case 1:
            *(u16*) p = 4;
            break;
        case 2:
        case 3:
            *(u16*) p = 2;
            break;
        }
        p += 2;
    }
    for (i = 0; i < 4; i++) {
        *p++ = i;
    }
    *p++ = 0;
    *(u32*) p = 0;
    p += 4;
    table = (u32*) p;
    for (i = 0; i < 4; i++) {
        *(u32*) p = 0;
        p += 4;
    }
    for (i = 0; i < 4; i++) {
        table[i] = p - buf;
        for (j = 0; j < 3; j++) {
            *(u16*) p = cut->num;
            p += 2;
            v = 0.0f;
            v0 = 0.0f;
            v1 = 0.0f;
            frames = (u16*) p;
            for (k = 0; k < cut->num; k++) {
                if (cut->frames == NULL) {
                    *(u16*) p = k * 30;
                } else {
                    *(u16*) p = cut->frames[k];
                }
                p += 2;
            }
            for (k = 0; k < cut->num; k++) {
                k1 = k + 1;
                k0 = k - 1;
                if (k1 > cut->num - 1) {
                    k1 = cut->num - 1;
                }
                if (k0 < 0) {
                    k0 = 0;
                }
                switch (i) {
                case 0:
                    v = (&cut->pos[k].x)[j];
                    v0 = (&cut->pos[k0].x)[j];
                    v1 = (&cut->pos[k1].x)[j];
                    break;
                case 1:
                    v = (&cut->at[k].x)[j];
                    v0 = (&cut->at[k0].x)[j];
                    v1 = (&cut->at[k1].x)[j];
                    break;
                case 2:
                    v = cut->roll[k];
                    v0 = cut->roll[k0];
                    v1 = cut->roll[k1];
                    break;
                case 3:
                    v = cut->fovy[k];
                    v0 = cut->fovy[k0];
                    v1 = cut->fovy[k1];
                    v1 *= DEG;
                    v *= DEG;
                    v0 *= DEG;
                    break;
                }
                tmp = v;
                EXPORT_TMP(p);
                dt0 = (f32) (frames[k] - frames[k0]);
                dt1 = (f32) (frames[k1] - frames[k]);
                if (k == 0) {
                    tan = (v1 - v) / dt1;
                } else if (cut->num - 1 == k) {
                    tan = (v - v0) / dt0;
                } else {
                    tan = (dt1 * ((v - v0) / dt0) + dt0 * ((v1 - v) / dt1)) / (dt0 + dt1);
                }
                tan *= dt0;
                tmp = tan;
                EXPORT_TMP(p);
                EXPORT_TMP(p);
            }
        }
        {
            // `rem` is one multi-set variable (in place in the `p - buf` register) and the pad loop
            // counts on `j` (a GPR elsewhere, so the reversed count stays `addic./bne`, no ctr).
            int rem = p - buf;
            rem %= 4;
            if (rem) {
                rem = 4 - rem;
                for (j = 0; j < rem; j++) {
                    *p++ = 0;
                }
            }
        }
    }
    *(u16*) buf = frames[cut->num - 1];
    return p - buf;
}

int CameraControl::IsChangeCamera()
{
    if (flags_30 & 2) {
        return 1;
    }
    return 0;
}

void CameraControl::Comeback(int)
{
    data = (CameraDataHeader*) pG->pRoomCamData;
    flags_30 &= ~4;
    flags_2C = (flags_2C & ~8) | 0x10;
    if (flags_2C & 0x20) {
        flags_2C &= ~0x20;
    }
    Check();
}

void CameraControl::Disable()
{
    state = 0;
    flags_2C |= 8;
}

void CameraControl::AreaCheckOnOff(int mode)
{
    switch (mode) {
    case 0:
        flags_2C |= 8;
        break;
    case 1:
        flags_2C = (flags_2C & ~8) | 0x10;
        break;
    }
}

u8 CameraControl::AreaNum()
{
    return data->numArea;
}

int CameraControl::CurrentAreaNo()
{
    return area_no;
}

int CameraControl::CurrentCameraNo()
{
    return camera_no;
}

CameraCut* CameraControl::DataSearch(int no)
{
    CameraAreaRec* rec = (CameraAreaRec*) (data + 1);
    CameraAreaInfo* area = (CameraAreaInfo*) (rec + data->numArea);
    CameraCut* cut = (CameraCut*) (area + data->numArea);
    int i = 0;

    while (i < data->numCut && no != cut->camera_no) {
        i++;
        cut++;
    }
    return cut;
}

CameraLerp* CameraControl::LerpDataSearch(int area_from, int cam_from, int area_to, int cam_to)
{
    CameraAreaRec* rec = (CameraAreaRec*) (data + 1);
    CameraAreaInfo* area = (CameraAreaInfo*) (rec + data->numArea);
    CameraCut* cut = (CameraCut*) (area + data->numArea);
    CameraLerp* lerp = (CameraLerp*) (cut + data->numCut);
    int i;

    for (i = 0; i < data->numLerp; i++, lerp++) {
        if (area_from == lerp->area_from && cam_from == lerp->cam_from && area_to == lerp->area_to &&
            cam_to == lerp->cam_to) {
            return lerp;
        }
    }
    return NULL;
}

CameraDataHeader* CameraControl::calcAddr(CameraDataHeader* d)
{
    int ver2;
    int i;
    CameraAreaRec* rec;
    CameraAreaInfo* area;
    CameraCut* cut;

    if (cameraDataVersion((char*) d) <= 1) {
        return d;
    }
    ver2 = 0;  // assigned after the early return: its `li` lands after the strncmp call
    if (strncmp((char*) d, "B402", 4) == 0) {
        ver2 = 1;
        OSReport("CameraControl::calcAddr(): R%1d%02x Ver02", pG->stage_no, pG->room_no);
    }

    rec = (CameraAreaRec*) (d + 1);
    for (i = 0; i < d->numArea; i++, rec++) {
        if ((s32) rec->area < 0) {
            return d;
        }
        rec->area = (CameraAreaInfo*) ((u32) rec->area + (u32) d);
        if (rec->cut) {
            rec->cut = (CameraCut*) ((u32) rec->cut + (u32) d);
        }
    }

    area = (CameraAreaInfo*) rec;
    for (i = 0; i < d->numArea; i++, area++) {
        area->points = (Vec*) ((u32) area->points + (u32) d);
        if (ver2) {
            area->attr = 3;
        }
        if (area->attr & 8) {
            area->attr |= 0x20;
        }
        if (cameraDataVersion((char*) d) <= 3) {
            area->attr2 = 1;
            area->x9 = 0xFF;
            OSReport("CameraControl::calcAddr(): R%1d%02x Ver%02d", pG->stage_no, pG->room_no,
                     cameraDataVersion((char*) d));
        }
    }

    cut = (CameraCut*) area;
    for (i = 0; i < d->numCut; i++, cut++) {
        cut->pos = (Vec*) ((u32) cut->pos + (u32) d);
        cut->at = (Vec*) ((u32) cut->at + (u32) d);
        cut->roll = (f32*) ((u32) cut->roll + (u32) d);
        cut->fovy = (f32*) ((u32) cut->fovy + (u32) d);
        cut->frames = (u16*) ((u32) cut->frames + (u32) d);
    }
    return d;
}

void CameraControl::RoomDataRead(CameraDataHeader* room)
{
    G_ROOM_CAM_DATA = calcAddr(room);
    data = (CameraDataHeader*) pG->pRoomCamData;
}

void CameraControl::CoreDataRead(CameraDataHeader* core)
{
    pG->pCoreCamData = calcAddr(core);
}

int cameraHitCheck(Vec* pos, Vec* nrm, Vec* from, Vec* to)
{
    static f32 R_GAIN = 1.1f;
    static f32 GAIN = 1.33f;
    Vec posA;
    Vec posB;
    Vec posC;
    Vec nrmA;
    Vec nrmB;
    Vec nrmC;
    Vec p;
    Vec hp;
    Vec hn;
    int hitA;
    int hitB;
    int hitC;
    int ret = 0;
    int first;
    f32 dist;
    f32 d;

    hitA = EmHitCheck(&posA, &nrmA, from, to, 1);
    hitB = ObjHitCheck(&posB, &nrmB, from, to, 1);
    hitC = SatMgr.hitCheck(from, to, &posC, &nrmC, 0x8000, 0x1C2810);
    if (hitA | hitB | hitC) {
        // COMPILER-DIFF: codeless anchor. The empty loop leaves NOTE_INSN_LOOP_BEG/END here, which ends
        // the first cse pass's extended basic block at this point (cse1 stops at LOOP_END). Without it
        // cse1 folds the `&posB`/`&nrmB` recomputations into the earlier `&posA` pseudos and the target's
        // `mr r18,r28` (gcse PRE copy) and fresh `addi r6/r7` argument forms are not produced.
        do { } while (0);
        dist = 0.0f;
        first = 1;
        if (hitA) {
            dist = PSVECDistance(from, &posA);
            first = 0;
            *pos = posA;
            *nrm = nrmA;
        }
        if (hitB) {
            d = PSVECDistance(from, &posB);
            if (first || d < dist) {
                dist = d;
                first = 0;
                *pos = posB;
                *nrm = nrmB;
            }
        }
        if (hitC) {
            d = PSVECDistance(from, &posC);
            if (first || d < dist) {
                *pos = posC;
                *nrm = nrmC;
            }
        }
        ret = 1;
    }
    if (pSubEm && pSubEm->id == 3) {
        cAtariInfo atBuf;
        // The target reads/writes the info through a pointer register (lha 0x18(r29), stfs 0x4(r29)) that is
        // a copy of the constructor's `this` register (`mr r29,r30`), and the 76-byte copy below increments
        // that `this` register in place. A plain `cAtariInfo& at = atBuf;` cannot produce this: cse makes the
        // longer-lived reference the canonical register (the copy loop then runs on a copy of it), and gcse
        // copy propagation replaces the reference by the `this` temporary everywhere else.
        // COMPILER-DIFF: register pin (r29) plus launder. The pin keeps `at` out of cse's canonical class
        // (hard regs go last), so the copy loop's address is the `this` temporary; the launder gives `at` a
        // second set so the `at = this` copy is not propagated into the later field accesses.
        register cAtariInfo* at asm("r29") = &atBuf;
        asm("" : "+r"(at));
        cModel* parts;
        Vec w;

        atBuf = pSubEm->atari;
        if (at->partsNo != 0) {
            parts = pSubEm->getPartsPtr(at->partsNo - 1);
        } else {
            parts = pSubEm;
        }
        if (parts) {
            f32 r;
            int hit;

            at->pos.y -= 1000.0f;
            at->h += 1000.0f;
            PSMTXMultVec(parts->mat, &at->pos, &w);
            r = at->rectX * R_GAIN;
            if (ret) {
                p = *pos;
            } else {
                p = *to;
            }
            // `hit = 0` after the `p` copy: the `li` sits in the join block and the w.y/p.y compare
            // registers come out as f12/f13 (declaring it initialised moves both).
            hit = 0;
            if (w.y <= p.y) {
                if (p.y <= w.y + at->h) {
                    Vec a;
                    Vec b;

                    a = w;
                    b = p;
                    a.y = 0.0f;
                    b.y = 0.0f;
                    if (PSVECDistance(&b, &a) <= r) {
                        hit = 1;
                    }
                }
            }
            if (hit == 1) {
                at->rectX *= GAIN;
                if (ObaLineHitChk(pSubEm, at, from, &p, &hp, &hn)) {
                    ret = 1;
                    *pos = hp;
                }
            }
        }
    }
    return ret;
}

void CameraSetCutData(Camera* cam, CameraCut* cut)
{
    cam->param.pos = *cut->pos;
    cam->param.at = *cut->at;
    cam->param.roll = *cut->roll;
    cam->param.fovy = *cut->fovy;
    CameraSetOrientationRoll(cam);
}

void CameraControl::AreaOnOff(int area_no, int camera_no, int on)
{
    CameraDataHeader* d = data;
    CameraAreaRec* rec = (CameraAreaRec*) (d + 1);
    s8 i;

    for (i = 0; i < d->numArea; i++, rec++) {
        if (area_no == rec->area->area_no && camera_no == rec->area->camera_no) {
            rec->area->enable = on;
            break;
        }
    }
}

void CameraControl::SetAreaAttr(int area_no, int camera_no, u8 attr)
{
    CameraDataHeader* d = data;
    CameraAreaRec* rec = (CameraAreaRec*) (d + 1);
    s8 i;

    for (i = 0; i < d->numArea; i++, rec++) {
        if (area_no == rec->area->area_no && camera_no == rec->area->camera_no) {
            rec->area->attr |= attr;
            break;
        }
    }
}

void CameraControl::UnsetAreaAttr(int area_no, int camera_no, u8 attr)
{
    CameraDataHeader* d = data;
    CameraAreaRec* rec = (CameraAreaRec*) (d + 1);
    s8 i;

    for (i = 0; i < d->numArea; i++, rec++) {
        if (area_no == rec->area->area_no && camera_no == rec->area->camera_no) {
            rec->area->attr &= ~attr;
            break;
        }
    }
}

void CameraControl::CutCall(int no)
{
    CameraDataHeader* d = data;
    CameraAreaRec* rec = (CameraAreaRec*) (d + 1);
    int found = 0;
    s8 i;

    for (i = 0; i < d->numArea; i++, rec++) {
        if (no == rec->cut->camera_no) {
            found = 1;
            break;
        }
    }
    if (found) {
        clearAttachCamera();
        interp.frame = 0;
        switchCamera(rec);
        flags_2C |= 8;
        flags_30 |= 4;
    } else {
        pLog->err(0, 0, "CameraControl::CutCall(): Cut %02d doesn't exist.", no);
    }
}

void CameraControl::switchCamera(CameraAreaRec* rec)
{
    CameraAreaInfo* area = rec->area;
    CameraCut* cut = rec->cut;
    CameraLerp* lerp = NULL;
    CameraDataHeader* d;
    register int i asm("r11");  // COMPILER-DIFF: loop counter r11 / pointer r10 (global allocates `r` first in ours: 14 refs/28 insns vs `i` 16/64)
    CameraAreaRec* r;
    int size;

    if (area_no != -1) {
        lerp = LerpDataSearch(area_no, x691, area->area_no, area->camera_no);
        if (lerp && lerp->enable == 1) {
            interp.set(lerp->frame, &cur);
        }
    } else {
        interp.frame = 0;
    }

    if (flags_2C & 2) {
        if (!(rec->area->attr & 8)) {
            d = data;
            for (r = (CameraAreaRec*) (d + 1), i = 0; i < d->numArea; r++, i++) {
                if (r->area->attr & 8) {
                    r->area->enable = 0;
                }
            }
        }
        flags_2C &= ~2;
    }

    if (area_rec != NULL) {
        CameraAreaInfo* a = area_rec->area;
        if (a->attr & 0x10) {
            a->enable = 0;
        } else if (a->attr & 8) {
            d = data;
            for (r = (CameraAreaRec*) (d + 1), i = 0; i < d->numArea; r++, i++) {
                if (r->area->attr & 8) {
                    r->area->enable = 0;
                }
            }
        }
    }

    area_no = area->area_no;
    x691 = area->camera_no;
    camera_no = cut->camera_no;
    area_rec = rec;

    if (area_no != -1) {
        if (flags_2C & 0x10) {
            if (!(flags_2C & 0x40)) {
                LightMgr.update(area_no, -1);
            }
        } else if (!(area->attr & 0x80)) {
            LightMgr.update(area_no, -1);
        }
    }
    flags_30 |= 2;

    switch (cut->type) {
    case 0:
        sub_state = 0;
        state = 1;
        break;
    case 1:
        state = 2;
        sub_state = 0;
        break;
    case 2:
        state = 3;
        sub_state = 0;
        break;
    case 3:
        state = 4;
        sub_state = 0;
        break;
    case 4:
        state = 6;
        sub_state = 0;
        break;
    case 5:
        state = 7;
        sub_state = 0;
        break;
    case 6:
        size = HermiteExport(cut, CameraMotionBuffer);
        if (size > CAMERA_MOTION_BUFFER_SIZE) {
            pLog->err(0, 0, "CameraControl::HermiteExport() = 0x%04x > 0x%04x", size, CAMERA_MOTION_BUFFER_SIZE);
        }
        if (extra) {
            delete extra;
        }
        extra = new (extra_buf) CameraMotion(CameraMotionBuffer, 0, 0, 0.0f);
        ((CameraMotion*) extra)->base_mat = NULL;
        state = 5;
        break;
    case 7:
        size = HermiteExport(cut, CameraMotionBuffer);
        if (size > CAMERA_MOTION_BUFFER_SIZE) {
            pLog->err(0, 0, "CameraControl::HermiteExport() = 0x%04x > 0x%04x", size, CAMERA_MOTION_BUFFER_SIZE);
        }
        if (extra) {
            delete extra;
        }
        extra = new (extra_buf) CameraMotion(CameraMotionBuffer, 0, 0, 0.0f);
        state = 9;
        break;
    case 8: {
        // two pointers to qfps: `q` (blend_src, setAreaData, bindAreaCamera) keeps the addi; `p`
        // (blend_dst, setBlendData) and the direct `qfps.` calls share gcse's copy (mr r29,r30)
        CameraQuasiFPS* q = &qfps;
        CameraQuasiFPS* p = &qfps;
        if (q->blend_src && p->blend_dst) {
            p->setBlendData(q->blend_src, p->blend_dst);
        }
        q->setAreaData(area_rec->cut);
        q->bindAreaCamera(area_rec);
        if (prev_state == 10 && !(flags_2C & 0x10)) {
            qfps.setBlendCount(10);
        } else {
            qfps.init();
            sub_state = 0;
        }
        state = 10;
        break;
    }
    }
    flags_2C &= ~0x10;
}

int areaAttr(CameraAreaInfo* area, u8 attr, u8 attr2)
{
    if ((area->enable & 1) && (area->attr & attr) && (area->attr2 & attr2)) {
        return 1;
    }
    return 0;
}

int areaHit(Vec* pos, CameraAreaInfo* area, f32 dir)
{
    int ret;

    if (area->attr & 4) {
        return 0;
    }
    if (area->attr & 0x40) {
        f32 d = area->dir;
        while (dir >= PI) {
            dir -= PI2;
        }
        while (dir < -PI) {
            dir += PI2;
        }
        dir -= d;
        while (dir >= PI) {
            dir -= PI2;
        }
        while (dir < -PI) {
            dir += PI2;
        }
        if (dir > 2.3561945f || dir < -2.3561945f) {
            return 0;
        }
    }
    if (area->num > 4) {
        ret = area_hit_pN(pos, area);
    } else {
        ret = area_hit_p3(pos, area);
    }
    return ret;
}

int area_hit_p3(Vec* pos, CameraAreaInfo* area)
{
    Vec* p[3];  // the three corner pointers live in memory (stw/lwz around the calls)
    Vec v1, v2, v0, c0, c1;
    f32 y = pos->y + 100.0f;
    int i, n, n1, i0;

    if (y < area->base_y) {
        return 0;
    }
    if (y >= area->base_y + area->height) {
        return 0;
    }
    for (i = 0; i <= 1; i++) {
        n = area->num;
        n1 = n - 1;   // its own statement: `(i0 + n - 1)` is reassociated by fold into `(i0 - 1) + n`
        i0 = i + i + 1;
        i0 %= n;      // two sets of i0: loop.c does not strength-reduce the 2i+1 giv
        p[0] = &area->points[i0];
        p[1] = &area->points[(i0 + n1) % n];
        p[2] = &area->points[(i0 + 1) % n];
        PSVECSubtract(pos, p[0], &v0);
        PSVECSubtract(p[1], p[0], &v1);
        PSVECSubtract(p[2], p[0], &v2);
        PSVECCrossProduct(&v1, &v0, &c0);
        PSVECCrossProduct(&v2, &v0, &c1);
        // one `||` return: the shared `li r3,0` block starts with a label, so loop.c's exit-block move
        // leaves it inside the loop and jump2 folds the two entry returns into it
        if (c0.y > 0.0f || c1.y < 0.0f) {
            return 0;
        }
    }
    return 1;
}

int area_hit_pN(Vec* pos, CameraAreaInfo* area)
{
    f32 y = pos->y + 100.0f;
    f32 a0, c, pz;
    f32 xi, zi, a, b, dx, dz, xmin, xmax, zmin, zmax;
    Vec *pi, *pj;
    Vec* pt[2];  // 8-byte pointer pair = one DImode pseudo (r7:r8); its halves are copied out before each
                 // load (`mr r9,r7; lfs 0(r9)`), which combine cannot fold through the subreg
    int i, count, fx, fz;

    if (y < area->base_y) {
        return 0;
    }
    if (y >= area->base_y + area->height) {
        return 0;
    }
    a0 = 1.0f;  // a named 1.0: cse cannot fold it inside the loop (fmsubs/fmadds with f5), pool order 100/1.0/0.0
    pz = pos->z;
    c = pos->x - pz;
    count = 0;
    for (i = 0; i < area->num; i++) {
        pi = &area->points[i];
        pj = &area->points[(i + 1) % area->num];
        dx = pj->x - pi->x;
        dz = pj->z - pi->z;
        pt[0] = pi;
        pt[1] = pj;
        if (dz != 0.0f) {
            a = dx / dz;
            b = pi->x - a * pi->z;
            if (a0 == a) {
                continue;
            }
            xi = (a0 * b - c * a) / (a0 - a);
            zi = (b - c) / (a0 - a);
        } else {
            zi = pi->z;
            xi = a0 * zi + c;
        }
        if (dx > 0.0f) {
            xmin = pt[0]->x;
            xmax = pt[1]->x;
            fx = 0;
        } else {
            xmin = pt[1]->x;
            xmax = pt[0]->x;
            fx = 1;
        }
        if (dz > 0.0f) {
            zmin = pt[0]->z;
            zmax = pt[1]->z;
            fz = 0;
        } else {
            zmin = pt[1]->z;
            zmax = pt[0]->z;
            fz = 1;
        }
        if (!(xi >= pos->x)) {
            continue;
        }
        if (fx == 0) {
            if (!(xi >= xmin && xi < xmax)) {
                continue;
            }
        } else {
            if (!(xi > xmin && xi <= xmax)) {
                continue;
            }
        }
        if (fz == 0) {
            if (!(zi >= zmin && zi < zmax)) {
                continue;
            }
        } else {
            if (!(zi > zmin && zi <= zmax)) {
                continue;
            }
        }
        count++;
    }
    if (count & 1) {
        return 1;
    }
    return 0;
}

void CameraControl::areaHitCheck()
{
    static u8 blink = 0;
    CameraDataHeader* d;
    CameraAreaRec* rec;
    CameraAreaRec* first;
    CameraAreaInfo* area;
    CameraCut* cut;
    u8 attr = 1;
    u8 old_attr;
    int old_area = area_no;
    s8 i;

    if (pG->flags_60 & 0x800) {
        return;
    }
    d = data;
    if (d == NULL) {
        camera_no = -1;
        area_no = -1;
        x691 = -1;
        state = 0xA;  // LAST: its 0xa register stays live across the `flags_2C & 0x10` test (andi. r10, not r9)
        if (old_area != -1 || (flags_2C & 0x10)) {
            qfps.init();
            flags_2C &= ~0x10;
        }
        area_rec = NULL;
        return;
    }
    if (flags_2C & 1) {
        if (flags_2C & 0x10) {
            state = 0xA;
            qfps.init();
            flags_2C &= ~0x10;
        }
        return;
    }
    if (cameraDataVersion((char*) d) <= 1) {
        camera_no = -1;
        area_no = -1;
        x691 = -1;
        state = 0xA;  // LAST: its 0xa register stays live across the `flags_2C & 0x10` test (andi. r10, not r9)
        if (old_area != -1 || (flags_2C & 0x10)) {
            qfps.init();
            flags_2C &= ~0x10;
        }
        area_rec = NULL;
        return;
    }

    if (SubCharGetStatus() & 0x20000000) {
        attr = 2;
    } else {
        switch (pG->x4FB8) {
        case 0:
            break;
        case 1:
            if (pG->x4F93 == 0) {
                attr = 4;
            }
            break;
        case 2:
            attr = 8;
            break;
        case 3:
            attr = 0x20;
            break;
        case 5:
            attr = 0x10;
            break;
        case 4:
            attr = 0x40;
            break;
        }
    }

    first = rec = (CameraAreaRec*) (d + 1);
    for (i = 0; i < d->numArea; i++, rec++) {
        area = rec->area;
        cut = rec->cut;
        if (areaAttr(area, 0x20, attr) && areaHit(&pPL->pos, area, pPL->rot.y)) {
            if ((flags_2C & 0x10) || cut->camera_no != camera_no) {
                switchCamera(rec);
            }
            return;
        }
    }

    old_attr = area_attr;
    if (pG->flags_6C & 0x40000000) {
        area_attr = 2;
        if (blink++ & 0x18) {
            eprintf(27, 18, 22, 0, "[ BATTLE ]");
        }
    } else {
        if (battle_timer > 0) {
            battle_timer--;
        }
        if (battle_timer == 0 && EmMgr.isBattle()) {
            area_attr = 2;
        } else {
            area_attr = 1;
        }
    }
    if (flags_2C & 4) {
        area_attr = 2;
    }
    if (old_attr != area_attr) {
        flags_2C |= 0x10;
    }

    if (area_no != -1 && !(flags_2C & 0x10)) {
        area = area_rec->area;
        if (areaAttr(area, area_attr, attr) && areaHit(&pPL->pos, area, pPL->rot.y)) {
            return;
        }
    }

    rec = first;
    for (i = 0; i < d->numArea; i++, rec++) {
        area = rec->area;
        cut = rec->cut;
        if (areaAttr(area, area_attr, attr) && areaHit(&pPL->pos, area, pPL->rot.y)) {
            if ((flags_2C & 0x10) || cut->camera_no != camera_no) {
                switchCamera(rec);
            }
            return;
        }
    }

    area_no = -1;
    x691 = -1;
    camera_no = -1;
    area_rec = NULL;
    state = 0xA;  // LAST (see the reset arms above); store order found by permutation
    if (flags_2C & 0x10) {
        flags_2C &= ~0x10;
        qfps.bindDefaultCamera();
        qfps.init();
        sub_state = 0;
        LightMgr.update(0, -1);
    } else if (old_area != -1) {
        CameraQuasiFPS* q = &qfps;
        if (prev_state == 0xA) {
            if (q->blend_src && q->blend_dst) {
                q->setBlendData(q->blend_src, q->blend_dst);
            }
            q->setBlendCount(10);
        }
        q->bindDefaultCamera();
        LightMgr.update(0, -1);
    }
}

void CameraControl::roomInit()
{
    s8 ver;

    BitOn(pG->flags_500C, 0x100);
    if (data == NULL) {
        flags_28 = 0;
    } else {
        flags_2C = 0x10;
        ver = cameraDataVersion((char*) data);
        switch (ver) {
        case 2:
            OSReport("CameraControl::roomInit(): Ver.02");
            break;
        case 1:
            pLog->warn(0, 0, "CameraControl::roomInit(): Ver.01");
            break;
        case 0:
            pLog->warn(0, 0, "CameraControl::roomInit(): Ver.00");
            break;
        case -1:
            pLog->warn(0, 0, "CameraControl::roomInit(): Empty!");
            break;
        }
        if (ver >= -1) {
            if (ver > 1) {
                if (ver > 4) {
                    goto clear;
                }
                flags_28 |= 1;
            } else {
                flags_28 |= 1;
                flags_2C |= 1;
            }
        } else {
        clear:
            flags_28 = 0;
        }
    }
    area_rec = NULL;
    state = 0xA;
    flags_28 &= ~4;
    flags_30 &= ~4;
    qfps.offsetCorrection();
    qfps.bindDefaultCamera();
    qfps.setFloorRatio(0.33333334f);
    qfps.init();
    area_no = -1;
    x691 = -1;
    camera_no = -1;
    x6CC = 60.0f;
    x6D0 = 600.0f;
    x6D4 = 400.0f;
    x6D8 = 0.7853982f;
    x6DC = 0.3926991f;
    x6E0 = 0xF;
    x6E4 = 0.001f;
    x6E8 = 0.75f;
    clearAttachCamera();
    BitOn(flags_2C, 2);
    if (pG->flags_54 & 0x200000) {
        state = 0;
        BitOn(flags_2C, 8);
    }
    x250 = 0;
    extra = NULL;
    interp.frame = 0;
    Check();
    Move();
    CameraMove();
    QuakeInit();
    memset(extra_buf, 9, sizeof(extra_buf));
    g_pToolCamData = NULL;
}

void CameraControl::Check()
{
    Vec d;

    if (pG->flags_500C & 0x40000) {
        return;
    }
    if (!(flags_28 & 1)) {
        return;
    }
    if (flags_28 & 4) {
        return;
    }
    flags_30 &= ~2;
    prev_state = state;
    if (!(flags_2C & 8)) {
        areaHitCheck();
    }
    checkAttachCamera();
    if (pG->flags_64 & 0x800000) {
        return;
    }
    if (pG->flags_500C & 0x1000) {
        return;
    }
    if (x250 != 0) {
        return;
    }
    if (flags_30 & 4) {
        return;
    }
    if (pPL->p2A4 && ((EmWork2A4*) pPL->p2A4)->x5) {
        return;
    }
    PSVECSubtract(&pPL->getPartsPtr(1)->worldPos, &pPL->pos, &d);
    if (state != 0xB) {
        if (d.y <= 500.0f) {
            interp.set(3, &camera.param);
            state = 0xB;
            if (extra) {
                delete extra;
            }
            extra = new (extra_buf) CameraLookAt(&camera);
        }
    } else {
        if (d.y > 500.0f) {
            interp.set(30, &camera.param);
            flags_2C = 0x10;
        }
    }
}

void CameraControl::Move()
{
    static f32 gain = 2.0f;
    f32 water_y;
    f32 t;
    f32 lim;

    if (pG->flags_500C & 0x40000) {
        return;
    }
    if (!(flags_28 & 1)) {
        return;
    }
    flags_30 &= ~1;
    if (area_rec) {
        CalcAim(area_rec->cut);
    }
    switch (state) {
    case 0:
        r0_Wait();
        break;
    case 1:
        r0_Fix();
        break;
    case 2:
        r0_Pan();
        break;
    case 3:
        r0_Track();
        break;
    case 4:
        r0_RailPan();
        break;
    case 5:
        CamSmth.ratio = 0.0f;
        extra->move();
        cur = extra->param;
        if (((CameraMotion*) extra)->end == 1) {
            if (extra) {
                delete extra;
            }
            state = 0;
        }
        break;
    case 6:
        r0_RailBehind();
        break;
    case 7:
        r0_Free();
        break;
    case 8:
        r0_Debug();
        break;
    case 9:
        r0_UpCut();
        break;
    case 0xA:
        qfps.move();
        cur = qfps.cam.param;
        break;
    case 0xC:
        CamSmth.ratio = 0.0f;
        extra->move();
        cur = extra->param;
        break;
    case 0x10:
    case 0x11:
        CamSmth.ratio = 0.0f;
        extra->move();
        cur = extra->param;
        break;
    case 0xB:
    case 0xF:
        extra->move();
        cur = extra->param;
        break;
    case 0xD:
        CamSmth.ratio = 0.0f;
        extra->move();
        cur = extra->param;
        break;
    default:
        r0_Debug();
        break;
    }

    if (!(pG->flags_500C & 0x1000)) {
        if (GetWaterHeight(&cur.pos, &water_y)) {
            t = sinf(cur.fovy * PI / 360.0f) / cosf(cur.fovy * PI / 360.0f);
            lim = gain * (ZNEAR * t * 1.3333334f) + water_y;
            if (cur.pos.y < lim) {
                cur.pos.y = lim;
            }
        }
    }
    interp.move(&cur);
    if (interp.frame != 0) {
        CamSmth.flags &= ~1;
    }
    CamSmth.move(&interp.param);
    camera.param = *CamSmth.getParam();
    CameraSetOrientationRoll(&camera);
    if (!(pG->flags_60 & 0x10000000) && (flags_30 & 4)) {
        pG->Cam = CamCtrl.camera;
    }
}

void CameraControl::CalcAim(CameraCut* cut)
{
    static Vec offset0 = {0.0f, 1000.0f, 0.0f};

    switch (state) {
    case 0:
    case 1:
    case 5:
    case 9:
        break;
    default:
        if (cut->flags & 1) {
            PSVECAdd(&pPL->pos, &cut->aim_ofs, &aim);
        } else {
            PSVECAdd(&pPL->pos, &offset0, &aim);
        }
        break;
    }
}

f32 CameraControl::getCameraPitch()
{
    return 0.0f;
}

void CameraInterpolation::set(int f, CameraParam* p)
{
    frame = f;
    param = *p;
}

void CameraInterpolation::move(CameraParam* p)
{
    CameraParam tmp;
    f32 r, s;

    if (frame != 0) {
        r = 1.0f / (f32) frame;
        s = 1.0f - r;
        tmp = *p;
        PSVECScale(&param.pos, &param.pos, s);
        PSVECScale(&param.at, &param.at, s);
        param.roll *= s;
        param.fovy *= s;
        PSVECScale(&p->pos, &p->pos, r);
        PSVECScale(&p->at, &p->at, r);
        p->roll *= r;
        p->fovy *= r;
        PSVECAdd(&p->pos, &param.pos, &param.pos);
        PSVECAdd(&p->at, &param.at, &param.at);
        param.roll += p->roll;
        param.fovy += p->fovy;
        frame--;
    } else {
        frame = 0;
        param = *p;
    }
}

void CameraSmooth::init(CameraParam* p)
{
    param = *p;
}

void CameraSmooth::move(CameraParam* p)
{
    Vec tmp;

    if (flags & 1) {
        flags &= ~1;
        init(p);
        return;
    }
    PSVECAdd(&param.pos, &pG->quake_ofs, &param.pos);
    PSVECAdd(&param.at, &pG->quake_ofs, &param.at);
    PSVECScale(&param.pos, &param.pos, ratio);
    PSVECScale(&p->pos, &tmp, 1.0f - ratio);
    PSVECAdd(&param.pos, &tmp, &param.pos);
    PSVECScale(&param.at, &param.at, ratio);
    PSVECScale(&p->at, &tmp, 1.0f - ratio);
    PSVECAdd(&param.at, &tmp, &param.at);
    param.roll *= ratio;
    param.roll = p->roll * (1.0f - ratio) + param.roll;
    param.fovy *= ratio;
    param.fovy = p->fovy * (1.0f - ratio) + param.fovy;
}

void CameraControl::r0_Wait()
{
}

void CameraControl::r0_Debug()
{
    Vec a;
    Vec b;
    Vec c;
    Vec unused[2];  // 0x18-byte frame slot between c and m in the original
    Mtx m;
    CameraParam p;
    Vec hit;
    const Vec campos_ofs = {0.0f, 1900.0f, -2000.0f};
    const Vec target_ofs = {0.0f, 1000.0f, 0.0f};
    Camera* cam = &camera;
    f32 rate;

    switch (sub_state) {
    case 0:
        dbg_pos = campos_ofs;
        dbg_at = target_ofs;
        PSMTXMultVec(pPLS->mat, &dbg_pos, &p.pos);
        PSVECAdd(&pPL->pos, &dbg_at, &p.at);
        p.roll = 0.0f;
        p.fovy = 55.0f;
        cur = p;
        CamSmth.flags |= 1;
        sub_state++;
        break;
    case 1: {
        JOY* joy = &Joy[0];
        Vec* dp = &dbg_pos;
        Vec* da = &dbg_at;

        if (joy->ssx != 0) {
            PSMTXRotRad(m, 'y', (f32) joy->ssx * 0.05f * DEG);
            PSMTXMultVec(m, dp, dp);
            PSMTXMultVec(m, da, da);
        }
        if (joy->ssy != 0) {
            Vec up = {0.0f, 1.0f, 0.0f};

            PSVECCrossProduct(dp, &up, &up);
            PSMTXRotAxisRad(m, &up, (f32) joy->ssy * 0.05f * DEG);
            PSMTXMultVec(m, dp, dp);
            PSMTXMultVec(m, da, da);
        }
        rate = 0.8f;
        if (joy->on == 0) {
            if (counter_58++ > 30) {
                rate = 0.8f * 1.2f;  // 0x3F75C290 (0.96f is 0x3F75C28F)
            }
        } else {
            counter_58 = 0;
        }
        PSVECAdd(&pPL->pos, dp, &a);
        PSVECAdd(&pPL->pos, da, &b);
        PSVECScale(&cam->param.pos, &cam->param.pos, rate);
        PSVECScale(&a, &c, 1.0f - rate);
        PSVECAdd(&cam->param.pos, &c, &cam->param.pos);
        PSVECScale(&cam->param.at, &cam->param.at, rate);
        PSVECScale(&b, &c, 1.0f - rate);
        PSVECAdd(&cam->param.at, &c, &cam->param.at);
        if (SatMgr.hitCheck(&cam->param.at, &cam->param.pos, &hit, NULL, 0x8000, 0)) {
            cam->param.pos = hit;
        }
        cur = cam->param;
        break;
    }
    }
}

void CameraControl::r0_Fix()
{
    Camera cam;

    CameraSetCutData(&cam, area_rec->cut);
    cur = cam.param;
    CamSmth.flags |= 1;
    state = 0;
}

// Start smoothing with `ratio`: `stw flags` is issued before `stfs ratio` only when the flags store
// is the LAST user of the CamSmth address in RTL order (it then carries the base register's
// REG_DEAD, weight -2 in sched1's tie-break), while the ratio store is a scalar reference so the
// ratio load stays below the flags load.
static inline void smoothStart(f32 ratio)
{
    u32 f = CamSmth.flags;
    FSet(CamSmth.ratio, ratio);
    CamSmth.flags = f | 1;
}

void CameraControl::r0_Pan()
{
    CameraParam p;
    CameraCut* cut = area_rec->cut;

    switch (sub_state) {
    case 0:
        p.pos = *cut->pos;
        p.roll = *cut->roll;
        p.fovy = *cut->fovy;
        p.at = aim;
        cur = p;
        smoothStart(smooth_ratio[1]);
        sub_state++;
    case 1:
        p.pos = camera.param.pos;
        p.roll = camera.param.roll;
        p.fovy = camera.param.fovy;
        p.at = aim;
        cur = p;
        break;
    }
}

void CameraControl::r0_Track()
{
    Camera cam;
    CameraBSpline* bs = &CamBSpline;
    CameraCut* cut = area_rec->cut;

    switch (sub_state) {
    case 0:
        Parametrize(cut, bs);
        searchRail(bs, cut, &aim, 0);
        BSpline(bs, &cam, 0);
        cur = cam.param;
        smoothStart(smooth_ratio[2]);
        sub_state++;
        break;
    case 1:
        searchRail(bs, cut, &aim, 0);
        BSpline(bs, &cam, 0);
        cur = cam.param;
        if (pGS->debug_mode == 0xF) {  // struct view: the pG load stays below the copy's stores
            debugDrawRail(cut);
        }
        break;
    }
}

void CameraControl::r0_RailPan()
{
    Camera cam;
    CameraBSpline* bs = &CamBSpline;
    CameraCut* cut = area_rec->cut;

    switch (sub_state) {
    case 0:
        Parametrize(cut, bs);
        searchRail(bs, cut, &aim, 0);
        BSpline(bs, &cam, 0);
        cam.param.at = aim;
        cur = cam.param;
        smoothStart(smooth_ratio[2]);
        sub_state++;
        break;
    case 1:
        searchRail(bs, cut, &aim, 0);
        BSpline(bs, &cam, 0);
        cam.param.at = aim;
        cur = cam.param;
        if (pGS->debug_mode == 0xF) {  // struct view: the pG load stays below the copy's stores
            debugDrawRail(cut);
        }
        break;
    }
}

void CameraControl::r0_UpCut()
{
}

// r0_RailBehind: argument addresses substituted into the hard-register sets (see COMPILER-DIFF #3 there).
static inline void VecLinComb(Vec* a, Vec* b, f32 s, f32 t, Vec* out)
{
    VecLinearCombination(a, b, s, t, out);
}

void CameraControl::r0_RailBehind()
{
    static Camera camera_old;
    static f32 move_z;
    static Vec campos_ofs0 = {0.0f, 1800.0f, -1200.0f};
    static Vec target_ofs0 = {0.0f, 1550.0f, 0.0f};
    static Vec pos_old;
    static int init_flg;
    static int edge_camera;
    static int c_rno;
    static int key_flg;
    static Vec ang;
    static int nI = 1;
    static int mI = 2;
    static f32 rate = 0.95f;
    Camera cam;
    Mtx m;
    Mtx inv;
    Camera* c = &camera;
    CameraCut* cut = area_rec->cut;
    Vec xaxis = {1.0f, 0.0f, 0.0f};
    Vec yaxis = {0.0f, 1.0f, 0.0f};
    Vec zaxis = {0.0f, 0.0f, 1.0f};
    Vec dir;
    Vec v;
    Vec d;
    Vec p0;
    Vec p1;
    Vec p2;
    Vec q;
    Vec q2;
    Vec hit;
    Vec floor;
    Vec a;
    int reset = 0;
    CameraBSpline* bs = &CamBSpline;
    JOY* joy = &Joy[0];
    int moved;
    int edge;
    f32 t;
    f32 k;
    f32 n;
    f32 mm;

    switch (sub_state) {
    case 0:
        Parametrize(cut, bs);
        if (cut->flags & 1) {
            dbg_pos = cut->aim_ofs;
            dbg_at = *(Vec*) &cut->floor_ratio;
        } else {
            dbg_pos = campos_ofs0;
            dbg_at = target_ofs0;
        }
        pos_old = pPL->pos;
        memclr_asm(&camera_old, sizeof(Camera));
        x36 = 0;
        sub_state++;
        edge_camera = 0;
        init_flg = 1;
        ang.x = ang.y = ang.z = 0.0f;
        key_flg = 0xFF;
        c_rno = 0;
        reset = 1;
    case 1:
        if (c_rno == 0) {
            if (joy->trg & 0xF00000) {
                if (joy->trg & 0x800000) {
                    if (key_flg == 2) {
                        key_flg = 0;
                    } else {
                        key_flg = 1;
                    }
                }
                if (joy->trg & 0x400000) {
                    if (key_flg == 1) {
                        key_flg = 0;
                    } else {
                        key_flg = 2;
                    }
                }
                if (joy->trg & 0x100000) {
                    if (key_flg == 4) {
                        key_flg = 0;
                    } else {
                        key_flg = 3;
                    }
                }
                if (joy->trg & 0x200000) {
                    if (key_flg == 3) {
                        key_flg = 0;
                    } else {
                        key_flg = 4;
                    }
                }
                c_rno++;
            }
            if (joy->trg & 0x200) {
                ang.x = ang.y = ang.z = 0.0f;
                key_flg = 0;
            }
        } else {
            if (joy->on & 0xF00000) {
                ang.y -= (f32) joy->ssx * x6E4;
                ang.x -= (f32) joy->ssy * x6E4;
                ang.y = ang.y < -x6D8 ? -x6D8 : (ang.y > x6D8 ? x6D8 : ang.y);
                ang.x = ang.x < -x6DC ? -x6DC : (ang.x > x6DC ? x6DC : ang.x);
                c_rno++;
            } else {
                if (c_rno < x6E0) {
                    ang.x = 0.0f;
                    ang.y = 0.0f;
                    switch (key_flg) {
                    case 0:
                        break;
                    case 1:
                        ang.x = -x6DC;
                        break;
                    case 2:
                        ang.x = x6DC;
                        break;
                    case 3:
                        ang.y = x6D8;
                        break;
                    case 4:
                        ang.y = -x6D8;
                        break;
                    }
                }
                c_rno = 0;
            }
        }
        moved = 0;
        if (PSVECDistance(&pos_old, &pPL->pos) > 50.0f) {
            moved = 1;
        }
        searchRail(bs, cut, &aim, 0);
        edge = 0;
        if (cut->flags & 4) {
            if (bs->t == 0.0f || (f32) (cut->num - 1) == bs->t) {
                cam = camera_old;
                edge = 1;
            }
        }
        if (edge_camera != 0) {
            if (edge == 0) {
                edge_camera = 0;
            }
        } else {
            if (edge == 1) {
                edge_camera = 1;
            }
            if ((cut->flags & 8) && init_flg == 1) {
                edge_camera = 0;
                if (edge == 0) {
                    init_flg = 0;
                }
            }
        }
        t = bs->t;
        BSpline(bs, &cam, 0);
        p0 = cam.param.at;
        bs->t = t - 0.1f;
        if (bs->t < 0.0f) {
            bs->t = 0.0f;
        }
        BSpline(bs, &cam, 0);
        p1 = cam.param.at;
        bs->t = t + 0.1f;
        if (bs->t > (f32) (cut->num - 1)) {
            bs->t = (f32) (cut->num - 1);
        }
        BSpline(bs, &cam, 0);
        p2 = cam.param.at;
        PSVECSubtract(&p1, &p2, &dir);
        dir.y = 0.0f;
        switch (x36) {
        case 0:
            if (init_flg == 1 && edge_camera == 1) {
                PSVECSubtract(&pPL->pos, &p0, &v);
                if (PSVECDotProduct(&v, &dir) < 0.0f) {
                    PSVECScale(&dir, &dir, -1.0f);
                }
            } else {
                PSMTXRotRad(m, 'y', pPL->rot.y);
                PSMTXMultVecSR(m, &zaxis, &v);
                if (PSVECDotProduct(&v, &dir) < 0.0f) {
                    PSVECScale(&dir, &dir, -1.0f);
                }
                reset = 1;
            }
            x36++;
            break;
        case 1:
            PSVECSubtract(&pPL->pos, &c->param.pos, &v);
            if (PSVECDotProduct(&v, &dir) < 0.0f) {
                PSVECScale(&dir, &dir, -1.0f);
            }
            break;
        }
#line 2628 "D:/Bio4/Prog/cam_ctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECCrossProduct(&yaxis, &dir, &xaxis);
        m[0][0] = xaxis.x;
        m[1][0] = xaxis.y;
        m[2][0] = xaxis.z;
        m[0][1] = yaxis.x;
        m[1][1] = yaxis.y;
        m[2][1] = yaxis.z;
        m[0][2] = dir.x;
        m[1][2] = dir.y;
        m[2][2] = dir.z;
        m[0][3] = p0.x;
        m[1][3] = p0.y;
        m[2][3] = p0.z;
        if (edge_camera) {
            floor = p0;
            floor.y = EatMgr.getFloor(&floor, 600.0f, 100000.0f, NULL, 0);
        } else {
            floor = pPL->pos;
        }
        PSMTXMultVecSR(m, &dbg_pos, &cam.param.pos);
        PSVECAdd(&cam.param.pos, &floor, &cam.param.pos);
        PSMTXMultVecSR(m, &dbg_at, &cam.param.at);
        PSVECAdd(&cam.param.at, &floor, &cam.param.at);
        if (!(cut->flags & 1)) {
            cam.param.fovy = x6CC;
            cam.param.roll = 0.0f;
        } else {
            cam.param.roll = 0.0f;
        }
        PSMTXInverse(m, inv);
        PSMTXMultVec(inv, &cam.param.pos, &q);
        if (moved) {
            PSMTXMultVec(inv, &cam.param.at, &q2);
            q2.x = q.x;
            PSMTXMultVec(m, &q2, &cam.param.at);
        }
        if ((s32) pSys->flags < 0) {
            PSVECScale(&ang, &a, -1.0f);
        } else {
            a = ang;
        }
        n = (f32) nI;
        mm = (f32) mI;
        k = 1.0f / (mm + n);
        // COMPILER-DIFF: #3 fresh-addi arguments. The target recomputes the three pointer arguments of
        // this one call (`addi r3,r1,0xb8; addi r4,r1,0xac; addi r5,r1,0x220`) while every surrounding
        // call copies them from the live address registers (`mr r3,r28` ...). Inside this `if` the
        // `&x` arguments of a plain call are precomputed into pseudos and gcse PRE turns them into
        // copies of the reaching registers; through the inline wrapper integrate substitutes the
        // addresses straight into the hard-register argument sets, which PRE never touches.
        VecLinComb(&cam.param.at, &cam.param.pos, mm * k, n * k, &floor);
        PSVECSubtract(&cam.param.at, &cam.param.pos, &dir);
        dir.y = 0.0f;
        PSVECCrossProduct(&yaxis, &dir, &xaxis);
        MtxRotAxisPosRad(m, &xaxis, &floor, a.x);
        PSMTXMultVec(m, &cam.param.at, &cam.param.at);
        PSMTXMultVec(m, &cam.param.pos, &cam.param.pos);
        MtxRotAxisPosRad(m, &yaxis, &floor, a.y);
        PSMTXMultVec(m, &cam.param.at, &cam.param.at);
        PSMTXMultVec(m, &cam.param.pos, &cam.param.pos);
        PSMTXRotRad(m, 'y', pPL->rot.y);
        PSMTXMultVecSR(m, &zaxis, &v);
        if (PSVECDotProduct(&v, &dir) < 0.0f) {
            PSVECSubtract(&pos_old, &pPL->pos, &d);
            pos_old = pPL->pos;
            PSMTXMultVecSR(inv, &d, &d);
            FSet(move_z, move_z + d.z);
            if (move_z > x6D4 || move_z < -x6D4) {
                if (edge_camera == 0) {
                    x36 = 0;
                }
            }
        } else {
            move_z = 0.0f;
        }
        if (SatMgr.hitCheck(&cam.param.at, &cam.param.pos, &hit, NULL, 0x8000, 0)) {
            cam.param.pos = hit;
        }
        if (edge_camera) {
            CamSmth.ratio = rate;
            cam.param.at = pPL->pos;
            cam.param.at.y += 1550.0f;
        } else {
            CamSmth.ratio = x6E8;
        }
        cur = cam.param;
        CamSmth.flags |= 1;
        if (reset == 1) {
            CamSmth.ratio = x6E8;
        }
        break;
    }
    pos_old = pPL->pos;
}

// Separate `on & bit` tests: fold merges `(on & a) || (on & b)` on one lvalue into one mask.
static inline u32 JoyOn(JOY* j, u32 bit)
{
    return j->on & bit;
}

static inline u32 JoyTrg(JOY* j, u32 bit)
{
    return j->trg & bit;
}


void CameraControl::r0_Free()
{
    static Vec campos_ofs0 = {0.0f, 1800.0f, -1200.0f};
    static Vec target_ofs0 = {0.0f, 1550.0f, 0.0f};
    static Vec ang;
    static Mtx cam_mat;
    Camera cam;
    Mtx m;
    Vec hit;
    Vec nrm;
    Vec tmp;
    Vec unused[2];
    JOY* joy = &Joy[0];
    JOY* joy2 = joy;
    f32 rate;

    switch (sub_state) {
    case 0: {
        PSMTXIdentity(cam_mat);
        cam_mat[0][3] = pPL->mat[0][3];
        cam_mat[1][3] = pPL->mat[1][3];
        cam_mat[2][3] = pPL->mat[2][3];
        ang.x = 0.0f;
        ang.y = pPL->rot.y;
        ang.z = 0.0f;
        Vec xaxis = {1.0f, 0.0f, 0.0f};
        Vec yaxis = {0.0f, 1.0f, 0.0f};
        Vec tofs;
        Vec a;
        if ((s32) pSys->flags < 0) {
            PSVECScale(&ang, &a, -1.0f);
        } else {
            a = ang;
        }
        tofs = target_ofs0;
        MtxRotAxisPosRad(m, &xaxis, &tofs, a.x);
        PSMTXMultVec(m, &campos_ofs0, &dbg_pos);
        PSMTXMultVec(m, &target_ofs0, &dbg_at);
        MtxRotAxisPosRad(m, &yaxis, &tofs, a.y);
        PSMTXMultVec(m, &dbg_pos, &dbg_pos);
        PSMTXMultVec(m, &dbg_at, &dbg_at);
        PSMTXMultVec(cam_mat, &dbg_pos, &cam.param.pos);
        PSMTXMultVec(cam_mat, &dbg_at, &cam.param.at);
        cam.param.roll = 0.0f;
        cam.param.fovy = x6CC;
        cur = cam.param;
        CamSmth.flags |= 1;
        x36 = 0;
        sub_state++;
    }
    case 1: {
        ang.y -= (f32) joy->ssx * 0.00125f;
        ang.x -= (f32) joy->ssy * 0.00125f;
        ang.x = ang.x < -0.7853982f ? -0.7853982f : (ang.x > PI * 0.35f ? PI * 0.35f : ang.x);
        ang.y = ang.y < -PI ? PI : (ang.y > PI ? -PI : ang.y);
        {
            cam_mat[0][3] = pPLS->mat[0][3];
            cam_mat[1][3] = pPLS->mat[1][3];
            cam_mat[2][3] = pPLS->mat[2][3];
            Vec xaxis = {1.0f, 0.0f, 0.0f};
            Vec yaxis = {0.0f, 1.0f, 0.0f};
            Vec tofs;
            Vec a;
            if ((s32) pSys->flags < 0) {
                PSVECScale(&ang, &a, -1.0f);
            } else {
                a = ang;
            }
            tofs = target_ofs0;
            MtxRotAxisPosRad(m, &xaxis, &tofs, a.x);
            PSMTXMultVec(m, &campos_ofs0, &dbg_pos);
            PSMTXMultVec(m, &target_ofs0, &dbg_at);
            MtxRotAxisPosRad(m, &yaxis, &tofs, a.y);
            PSMTXMultVec(m, &dbg_pos, &dbg_pos);
            PSMTXMultVec(m, &dbg_at, &dbg_at);
        }
        {
            int st = x36;

            asm("" : "+r"(st));  // COMPILER-DIFF 2: the original zero-extends the loaded byte again
            switch ((u8) st) {
            case 0:
                if (JoyTrg(joy, 0x200) || JoyOn(joy, 0x200) || JoyOn(joy, 0x20)) {
                    x36 = st + 1;
                }
                break;
            case 1: {
                Vec d;

                d.x = 0.0f - ang.x;
                d.y = pPL->rot.y - ang.y;
                d.z = 0.0f;
                VecRadLimit(&d);
                ang.y += d.y * 0.1f;
                ang.x += d.x * 0.1f;
                if (!JoyOn(joy, 0x200) && !JoyOn(joy, 0x20)) {
                    // the pairs fold to one halfword test each; the second pointer keeps fold
                    // from merging all four bytes into one word compare
                    if (PSVECMag(&d) < 0.05f || (joy->ssx != 0 || joy->ssy != 0) || (joy2->sx != 0 || joy2->sy != 0)) {
                        x36--;
                    }
                }
                break;
            }
            }
        }
        PSMTXMultVec(cam_mat, &dbg_pos, &cam.param.pos);
        PSMTXMultVec(cam_mat, &dbg_at, &cam.param.at);
        cam.param.roll = 0.0f;
        cam.param.fovy = x6CC;
        rate = 0.8f;
        PSVECScale(&cam.param.pos, &cam.param.pos, rate);
        PSVECScale(&cur.pos, &tmp, 1.0f - rate);
        PSVECAdd(&cam.param.pos, &tmp, &cam.param.pos);
        PSVECScale(&cam.param.at, &cam.param.at, rate);
        PSVECScale(&cur.at, &tmp, 1.0f - rate);
        PSVECAdd(&cam.param.at, &tmp, &cam.param.at);
        {
            Vec from = cam.param.at;
            Vec to = cam.param.pos;

            if (cameraHitCheck(&hit, &nrm, &from, &to)) {
                cam.param.pos = hit;
            }
        }
        cur = cam.param;
        break;
    }
    }
}

void CamCtrlShoulderSetSearchFrame(s16 frame)
{
    CamCtrl.qfps.search_frame = frame;
    CamCtrl.qfps.search_count = 0;
}

void CamCtrlShoulderSetAim(Vec* aim)
{
    CamCtrl.qfps.shoulder_aim = *aim;
}

void CameraControl::resetCameraAngle()
{
    CameraQuasiFPS* q = &CamCtrl.qfps;

    q->angle_y = 0.0f;
    q->angle_x = 0.0f;
}

f32 CameraControl::getCameraDirection()
{
    f32 dir = CamCtrl.qfps.angle_x;
    CamCtrl.qfps.angle_x = 0.0f;
    return dir;
}

void Parametrize(CameraCut* cut, CameraBSpline* bs)
{
    int i;
    f32* B;
    f32* Binv;
    f32* px;
    f32* py;
    f32* pz;
    f32* ax;
    f32* ay;
    f32* az;
    f32* roll;
    f32* fovy;

    bs->num = cut->num;
    if (bs->num > 1) {
#line 3058 "D:/Bio4/Prog/cam_ctrl.cpp"
        B = (f32*) MEM_ALLOC(sizeof(f32) * bs->num * bs->num, 1, 0xd);
        Binv = (f32*) MEM_ALLOC(sizeof(f32) * bs->num * bs->num, 1, 0xd);
        px = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        py = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        pz = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        ax = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        ay = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        az = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        roll = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        fovy = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        bs->k = 2;
        if (bs->k > bs->num - 1) {
            bs->k = bs->num - 1;
        }
        for (i = 0; i < bs->num; i++) {
            px[i] = cut->pos[i].x;
            py[i] = cut->pos[i].y;
            pz[i] = cut->pos[i].z;
            ax[i] = cut->at[i].x;
            ay[i] = cut->at[i].y;
            az[i] = cut->at[i].z;
            roll[i] = cut->roll[i];
            fovy[i] = cut->fovy[i];
        }
        for (i = 0; i < bs->num; i++) {
            de_Boor_Cox(bs->num, NULL, bs->k, (f32) i, &B[bs->num * i]);
        }
        MtxNNInverse(bs->num, B, Binv);
        MtxNNMultVecSR(bs->num, bs->num, Binv, px, bs->px);
        MtxNNMultVecSR(bs->num, bs->num, Binv, py, bs->py);
        MtxNNMultVecSR(bs->num, bs->num, Binv, pz, bs->pz);
        MtxNNMultVecSR(bs->num, bs->num, Binv, ax, bs->ax);
        MtxNNMultVecSR(bs->num, bs->num, Binv, ay, bs->ay);
        MtxNNMultVecSR(bs->num, bs->num, Binv, az, bs->az);
        MtxNNMultVecSR(bs->num, bs->num, Binv, roll, bs->roll);
        MtxNNMultVecSR(bs->num, bs->num, Binv, fovy, bs->fovy);
        Mem_free(B);
        Mem_free(Binv);
        Mem_free(px);
        Mem_free(py);
        Mem_free(pz);
        Mem_free(ax);
        Mem_free(ay);
        Mem_free(az);
        Mem_free(roll);
        Mem_free(fovy);
    }
}

void BSpline(CameraBSpline* bs, Camera* cam, int)
{
    int i;

    memclr_asm(cam, sizeof(Camera));
    de_Boor_CoxF(bs->num, NULL, bs->t, bs->k, bs->basis);  // COMPILER-DIFF 1 (floats-first alias)
    for (i = 0; i < bs->num; i++) {
        cam->param.at.x += bs->basis[i] * bs->ax[i];
        cam->param.at.y += bs->basis[i] * bs->ay[i];
        cam->param.at.z += bs->basis[i] * bs->az[i];
        cam->param.pos.x += bs->basis[i] * bs->px[i];
        cam->param.pos.y += bs->basis[i] * bs->py[i];
        cam->param.pos.z += bs->basis[i] * bs->pz[i];
        cam->param.roll += bs->basis[i] * bs->roll[i];
        cam->param.fovy += bs->basis[i] * bs->fovy[i];
    }
}

void searchRail(CameraBSpline* bs, CameraCut* cut, Vec* aim, int)
{
    Vec d;
    Vec v;
    f32 min = 10000000000.0f;
    int found = 0;
    int i;
    f32 dot;
    f32 s;
    f32 dist;

    for (i = 0; i < cut->num - 1; i++) {
        // One variable per value (each block-local with a single death): `dot0` for the first
        // product, `dot` for the second, `prod` tied to `dot` (`fmuls f31, f30, f31`).
        f32 dot0;
        f32 prod;

        PSVECSubtract(&cut->at[i + 1], &cut->at[i], &d);
        d.y = 0.0f;
        PSVECSubtract(aim, &cut->at[i], &v);
        v.y = 0.0f;
        dot0 = PSVECDotProduct(&d, &v);
        s = dot0 / PSVECMag(&d);
        PSVECSubtract(aim, &cut->at[i + 1], &v);
        v.y = 0.0f;
        dot = PSVECDotProduct(&d, &v);
        dot = dot / PSVECMag(&d);
        prod = s * dot;
        if (prod < 0.0f) {
            d.y = cut->at[i + 1].y - cut->at[i].y;
            PSVECScale(&d, &v, s / PSVECMag(&d));
            PSVECAdd(&v, &cut->at[i], &v);
            dist = PSVECDistance(aim, &v);
            if (dist < min) {
                min = dist;
                bs->t = (f32) i + s / PSVECMag(&d);
                bs->seg = i;
                found = 1;
            }
        }
    }
    if (found) {
        f32 min2 = 10000000000.0f;
        int seg = 0;

        for (i = 0; i < cut->num; i++) {
            PSVECSubtract(aim, &cut->at[i], &d);
            dist = PSVECMag(&d);
            if (dist < min2) {
                min2 = dist;
                seg = i;
            }
        }
        if (min > min2) {
            bs->seg = seg;
            bs->t = (f32) seg;
        }
    } else {
        f32 min2 = 10000000000.0f;

        for (i = 0; i < cut->num; i++) {
            PSVECSubtract(aim, &cut->at[i], &d);
            dist = PSVECMag(&d);
            if (dist < min2) {
                min2 = dist;
                bs->seg = i;
                bs->t = (f32) i;
            }
        }
    }
}

void CameraControl::debugDrawRail(CameraCut* cut)
{
    static Vec Fc_old;
    static Vec Ft_old;
    CameraBSpline* bs = &CamBSpline;
    Vec fc;
    Vec ft;
    int i;
    int j;

    for (i = 0; i < 100; i++) {
        de_Boor_Cox(cut->num, NULL, bs->k, (f32) ((cut->num - 1) * i) / 100.0f + 0.0f, bs->basis);
        fc.x = 0.0f;
        fc.y = 0.0f;
        fc.z = 0.0f;
        ft.x = 0.0f;
        ft.y = 0.0f;
        ft.z = 0.0f;
        for (j = 0; j < cut->num; j++) {
            fc.x += bs->basis[j] * bs->px[j];
            fc.y += bs->basis[j] * bs->py[j];
            fc.z += bs->basis[j] * bs->pz[j];
            ft.x += bs->basis[j] * bs->ax[j];
            ft.y += bs->basis[j] * bs->ay[j];
            ft.z += bs->basis[j] * bs->az[j];
        }
        if (i > 0) {
            Draw_line3d(&Fc_old, &fc, 0xFF2020FF, 0);
            Draw_line3d(&Ft_old, &ft, 0xFF20FF20, 0);
        }
        Fc_old = fc;
        Ft_old = ft;
    }
}

CameraControl CamCtrl;
CameraBSpline CamBSpline;
CameraSmooth CamSmth;

void CameraControl::UpCutCall(int no, Vec* pos, Vec* at, Vec* up, int sel)
{
    switch (sel) {
    case 0:
        data = (CameraDataHeader*) pG->pCoreCamData;
        break;
    case 1:
        data = (CameraDataHeader*) pG->pRoomCamData;
        break;
    }
    if (pos) {
        up_pos = *pos;
    }
    if (at) {
        up_at = *at;
    }
    if (up) {
        up_vec = *up;
    }
    CutCall(no);
}

void CameraControl::startPushObject()
{
    extra = new (extra_buf) CameraPushObject();
    state = 0xF;
    AreaCheckOnOff(0);
}

void CameraControl::endPushObject()
{
    if (extra) {
        delete extra;
    }
    Comeback(0);
}

void CameraControl::StartLookDownEm(void* em)
{
    Vec c;
    cModel* p[2];

    p[0] = pPL->getPartsPtr(0x20);
    p[1] = pPL->getPartsPtr(0x21);
    PSVECAdd(&p[0]->worldPos, &p[1]->worldPos, &c);
    PSVECScale(&c, &c, 0.5f);
    extra = new (extra_buf) CameraLookDownEm(em, &c);
    state = 0xD;
    AreaCheckOnOff(0);
    BitOff(pG->flags_500C, 0x2000000);
}

void CameraControl::EndLookDownEm()
{
    if (extra) {
        delete extra;
    }
    Comeback(0);
    BitOn(pG->flags_500C, 0x2000000);
}

void CameraControl::startScope(Vec* pos, Vec* at)
{
    if (!(pG->flags_500C & 0x40)) {
        BitOn(pG->flags_500C, 0x40);
        BitOn(pG->flags_500C, 0x8000);
        extra = new (extra_buf) CameraScope(pos, at);
        state = 0x10;
        BitOn(pG->flags_58, 0x40000000);
        AreaCheckOnOff(0);
    }
}

void CameraControl::endScope()
{
    if (pG->flags_500C & 0x40) {
        BitOff(pG->flags_500C, 0x40);
        BitOff(pG->flags_500C, 0x8000);
        BitOff(pG->flags_58, 0x40000000);
        if (extra) {
            delete extra;
        }
        Comeback(0);
    }
}

void CameraControl::getTrajectory(Vec* pos, Vec* at)
{
    if (pG->flags_500C & 0x40) {
        cCamera* c = extra;
        *pos = c->param.pos;
        *at = c->param.at;
    }
}

void CameraControl::saveScopeParam()
{
    ((CameraScope*) extra)->getParam(&scope_param0, &scope_param1);
    ((CameraScope*) extra)->id.save(0);
}

void CameraControl::loadScopeParam()
{
    ((CameraScope*) extra)->setParam(scope_param0, scope_param1);
    ((CameraScope*) extra)->id.load(0);
}

void CameraControl::SetBinocularRange(f32 a, f32 b, f32 c, f32 d)
{
    ((CameraBinocular*) extra)->setRange(a, b, c, d);
}

void CameraControl::HoldBinocular(void* id_a, void* id_b, Vec* pos, Vec* at)
{
    BitOn(pG->flags_500C, 0x400);
    BitOn(pG->flags_500C, 0x8000);
    extra = new (extra_buf) CameraBinocular(pos, at, id_a, id_b);
    state = 0xC;
    BitOn(pG->flags_58, 0x40000000);
    AreaCheckOnOff(0);
}

void CameraControl::LowerBinocular()
{
    BitOff(pG->flags_500C, 0x400);
    BitOff(pG->flags_500C, 0x8000);
    BitOff(pG->flags_58, 0x40000000);
    if (extra) {
        delete extra;
    }
    Comeback(0);
}

void CameraControl::GetBinocularIDAddr(void** a, void** b)
{
    *a = ((CameraBinocular*) extra)->id_a;
    *b = ((CameraBinocular*) extra)->id_b;
}

void CameraControl::MotionSet(void* motion, int frame, f32 speed)
{
    BitOn(flags_2C, 0x28);
    BitOn(pG->flags_5014, 0x10000000);
    extra = new (extra_buf) CameraMotion(motion, 0, 0, speed);
    ((CameraMotion*) extra)->base_mat = NULL;
    state = 5;
    interp.set(frame, &pG->Cam.param);
}

int CameraControl::IsMotionSet()
{
    if (flags_2C & 0x20) {
        return 1;
    }
    return 0;
}

int CameraControl::IsMotionEnd()
{
    if (state != 5) {
        return 1;
    }
    return ((CameraMotion*) extra)->end == 1;
}

void CameraControl::setMotionBaseMatPtr(Mtx* mat)
{
    ((CameraMotion*) extra)->base_mat = mat;
}

void* CameraControl::getMotionInfoPtr()
{
    return &((CameraMotion*) extra)->info;
}

void CameraControl::clearAttachCamera()
{
    int i;

    attach_num = 0;
    attach_cur = NULL;
    for (i = 0; i < 3; i++) {
        attach_model[i] = NULL;
        attach_cam[i] = NULL;
    }
}

void CameraControl::registAttachCamera(AttachCamera* cam, cModel* model)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (model == attach_model[i]) {
            attach_cam[i] = cam;
            return;
        }
    }
    for (i = 0; i < 3; i++) {
        if (attach_model[i] == NULL) {
            attach_num++;
            attach_model[i] = model;
            attach_cam[i] = cam;
            return;
        }
    }
    pLog->err(0, 0, "registAttachCamera(): lack of ptr table.");
}

void CameraControl::deleteAttachCamera(AttachCamera* cam, cModel* model)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (model == attach_model[i] && cam == attach_cam[i]) {
            attach_num--;
            attach_model[i] = NULL;
            attach_cam[i] = NULL;
            return;
        }
    }
}

cModel* CameraControl::getAttachModel(cModel* model)
{
    int i;

    if (model == NULL) {
        for (i = 0; i < 3; i++) {
            if (attach_model[i] != NULL) {
                return attach_model[i];
            }
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (model == attach_model[i]) {
                return model;
            }
        }
    }
    return NULL;
}

AttachCamera* CameraControl::getAttachCamera(cModel* model)
{
    int i;

    if (model == NULL) {
        for (i = 0; i < 3; i++) {
            if (attach_model[i] != NULL) {
                return attach_cam[i];
            }
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (model == attach_model[i]) {
                return attach_cam[i];
            }
        }
    }
    return NULL;
}

// struct-member view of pG with a direct symbol address (no `li rX, pG@sda21`): the original
// reloads pG and `extra` after every one of the four param copies below
extern GlobalWorkPtr pGW asm("pG");

void CameraControl::checkAttachCamera()
{
    static int inter_frame;
    cModel* em[3] = {NULL, NULL, NULL};
    cModel* model = NULL;
    AttachCamera* ac;
    int i;

    if (pG->flags_500C & 0x40) {
        return;
    }
    if (flags_30 & 4) {
        return;
    }
    switch (attach_num) {
    case 0:
        break;
    case 1:
        model = getAttachModel(NULL);
        break;
    default:
        for (i = 0; i < 2; i++) {
            model = getAttachModel(em[i]);
            if (model) {
                break;
            }
        }
        break;
    }
    if (model) {
        ac = getAttachCamera(model);
        if (attach_cur != model) {
            BitOn(flags_2C, 8);
            interp.set(ac->frame, &pG->Cam.param);
            state = 0x11;
            if (extra) {
                delete extra;
            }
            extra = new (extra_buf) CameraAttachedToMotion(model);
            extra->param.pos = pGW.p->Cam.param.pos;
            extra->param.at = pGW.p->Cam.param.at;
            extra->param.roll = pGW.p->Cam.param.roll;
            extra->param.fovy = pGW.p->Cam.param.fovy;
        }
        inter_frame = ac->frame;
    } else if (attach_cur) {
        BitSet(flags_2C, 0x10);
        interp.set(inter_frame, &pG->Cam.param);
    }
    attach_cur = model;
}

int cameraDataVersion(char* data)
{
    if (strncmp(data, "B404", 4) == 0) {
        return 4;
    }
    if (strncmp(data, "B403", 4) == 0) {
        return 3;
    }
    if (strncmp(data, "B402", 4) == 0) {
        return 2;
    }
    if (strncmp(data, "B401", 4) == 0) {
        return 1;
    }
    if (strncmp(data, "B400", 4) == 0) {
        return 0;
    }
    strncmp(data, "EMPT", 4);
    return -1;
}
