#include "types.h"
#include "vec.h"
#include "atari.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_qfps.h"
#include "joy.h"
#include "eprintf.h"
#include "db_log.h"
#include "math_sub.h"
#include "main_mem.h"
#include "model.h"
#include "em.h"
#include "obj.h"
#include "player.h"
#include "db_cam.h"

extern "C" {
void* memset(void* dst, int c, unsigned int n);
void CameraSetOrientationZeroRoll(Camera* cam);
void CameraCamposDistance(Camera* cam, f32 dist);
void CameraRotAxisPosRad(Camera* cam, Vec* axis, Vec* pos, f32 rad);
void CameraCamposRot(Camera* cam, char axis, f32 rad);
void CameraTargetRot(Camera* cam, char axis, f32 rad);
void CameraDolly(Camera* cam, Vec* mv);
void Draw_line3d(Vec* a, Vec* b, u32 color, int flag);
void MotionMove(cModel* m, int flag);
}

extern int ProjType;
extern f32 ORTHO_T;
extern f32 ORTHO_B;
extern f32 ORTHO_L;
extern f32 ORTHO_R;

#define DEG 0.017453292f

// Orthographic zoom: the top/left extents move together (plain block: a do-while's loop notes
// flip the f0/f13 allocation of the two chains).
#define ORTHO_ZOOM(t)                    \
    {                                    \
        ORTHO_T += (t) * 0.75f;          \
        ORTHO_L -= (t);                  \
    }

// EmMgrWork with the manager through a pointer (one `&EmMgr` instead of three per-field
// `high/lo_sum` pairs): four fewer expand-time insns, so the enemy search loop's `break` sits
// within stmt.c's 30-insn rotation window and the loop rotates at the match test (`bdz` at the
// top, the two tests jumping back to the increment) instead of at the `--i` test (`bdnz`).
// Same final code as em.h's EmMgrWork everywhere else (cse folds the pointer).
static inline cEm* EmMgrWorkP(u32 no)
{
    cEmMgr* m = &EmMgr;
    if (no >= m->nArray) {
        return 0;
    }
    return (cEm*) ((u8*) m->pArray + m->size * no);
}

debugCamera CamDbg;
QfpsOfs g_local_ready[2][3];
QfpsOfs g_local_trans[2][3];
f32 g_local_floor_ratio;
f32 g_local_fovy[2];

const char* key_str[4] = {"DFLT", "SCR", "????", "????"};

// The menu counters are stepped through a reference (the stores then invalidate every cached
// load, as in the original: joy->trg is reloaded after each step).
static inline void Inc(int& v) { v++; }
static inline void Dec(int& v) { v--; }
static inline void Set(int& v, int x) { v = x; }

// adjust_qFPS keeps the edited shoulder offset record as a byte pointer (the original copies it
// with memcpy and steps through it by byte offset).
#define QOFS(p) ((QfpsOfs*) (p))
#define QOFS_CAMPOS2 0xC

// Matrix column -> vector (cam_sys.cpp's helper): through the `Vec*` parameter the stores go via
// the address register for the frame-offset-0 local (`stfs 4(r31)`, its pseudo reused by the
// later PSVECScale(&vx, ..)); for the other locals integrate substitutes the frame address.
static inline void getColumn(Mtx m, int c, Vec* v)
{
    v->x = m[0][c];
    v->y = m[1][c];
    v->z = m[2][c];
}

// Column vectors -> matrix.
#define MTX_SET_COLUMNS(m, c0, c1, c2, c3)                                                    \
    (m)[0][0] = (c0).x; (m)[1][0] = (c0).y; (m)[2][0] = (c0).z;                               \
    (m)[0][1] = (c1).x; (m)[1][1] = (c1).y; (m)[2][1] = (c1).z;                               \
    (m)[0][2] = (c2).x; (m)[1][2] = (c2).y; (m)[2][2] = (c2).z;                               \
    (m)[0][3] = (c3).x; (m)[1][3] = (c3).y; (m)[2][3] = (c3).z

void debugCamera::move(Camera* cam, JOY* joy, int flag)
{
    static void (debugCamera::*camera_type_tbl[4])(Camera*, JOY*) = {
        &debugCamera::camera_type_00,
        &debugCamera::camera_type_01,
        &debugCamera::camera_type_00,
        &debugCamera::camera_type_00,
    };
    static int numEm = 0;
    static int numObj = 0;

    timer--;
    if (timer & 0x80) {
        timer = 0;
        if (joy->trg & JOY_Z) {
            if (mode == 0) {
                timer = 5;
                mode = 1;
                save_mode = pG->debug_mode;
                pGS->debug_mode = 1;
                play = 0;
                adjust_qFPS(NULL, 0, 0, 1, NULL);
            } else {
                mode = 0;
                pG->debug_mode = save_mode;
            }
        }
    }
    switch (mode) {
    case 0:
        break;
    case 1:
        menu(cam, joy);
        return;
    }
    if (joy->on & ~0x1A00) {
        if ((s32) pG->flags_60 < 0) {
            draw_timer = 5;
        } else {
            draw_timer = 30;
        }
    }
    if (draw_timer) {
        draw_timer--;
        CameraDrawTarget(cam, 0);
    }
    if (!(pG->flags_60 & 0x10000000)) {
        if (joy->on & ~0x1A00) {
            pG->flags_60 |= 0x10000000;
        }
    } else {
        if ((s32) pG->flags_60 >= 0 && (pG->flags_51E4 & 0x10)) {
            eprintf(160, 406, 4, 0, "DEBUG CAMERA --- [%s]", key_str[key_type]);
        }
        if (mode == 0 && !(flag & 1) && (joy->on & JOY_B)) {
            pG->flags_60 &= ~0x10000000;
        }
    }
    switch (target_type) {
    case 0: {
        cEm* em;
        if (joy->trg & JOY_A) {
            int i;
            cEm* e;
            int num = EmMgr.nArray;
            i = num;
            numEm++;
            if (num <= numEm) {
                numEm = 0;
            }
            while (--i) {
                e = EmMgrWorkP(numEm);
                if ((e->be_flag & 1) && e->id <= 0x3F) {
                    break;
                }
                numEm++;
                if (num <= numEm) {
                    numEm = 0;
                }
            }
        }
        if (joy->on & JOY_A) {
            if (EmMgrWork(numEm)->be_flag & 1) {
                cModel* parts = EmMgrWork(numEm)->getPartsPtr(0);
                if (parts == NULL) {
                    cam->param.at = EmMgrWork(numEm)->pos;
                } else {
                    cam->param.at = parts->worldPos;
                }
            } else {
                cam->param.at.x = 0.0f;
                cam->param.at.y = 0.0f;
                cam->param.at.z = 0.0f;
            }
            CameraSetOrientationZeroRoll(cam);
        }
        em = EmMgrWork(numEm);
        if (em != NULL) {
            if ((em->be_flag & 1) && em != (cEm*) pPL) {
                int col = 0;
                int dead = (em->flags_324 & 0xFFFF0000) != 0;
                if (dead || em->hp <= 0) {
                    col = 2;
                }
                eprintf2(8, 14, 32, 358, col, 7, "Id=%02x, be=%08x, type=%02x, set=%02x, List=%02d", em->id,
                         em->be_flag, em->type, em->x38D, em->emsetNo);
                eprintf2(8, 14, 32, 372, col, 7, "POS[%.2f, %.2f, %.2f], Dir[%.2f]", em->pos.x, em->pos.y,
                         em->pos.z, em->rot.y);
                eprintf2(8, 14, 32, 386, col, 7, "RNO[%02x][%02x][%02x][%02x], HP[ %d], FRAME[%d/%d] ", em->xFC,
                         em->xFD, em->xFE, em->xFF, em->hp, (u32) em->frame, em->frameMax);
                eprintf2(8, 14, 32, 344, col, 7, "Flag=[%08x], L_pl[%.2f]", em->flags_3C8, SQRTF(em->plDist2));
                if (em->checkStatus(1)) {
                    eprintf2(10, 16, 400, 344, col, 7, "LOCKOFF");
                }
            }
        }
        if (em != NULL && (em->be_flag & 1) && (pG->flags_60 & 0x20000)) {
            em->debugSkeletonDisp();
        }
        break;
    }
    case 1: {
        if (joy->trg & JOY_A) {
            int i;
            int num = ObjMgr.nArray;
            cObj* obj;
            i = num;
            numObj++;
            if (num <= numObj) {
                numObj = 0;
            }
            for (;;) {
                obj = ObjMgrWork(numObj);
                if (!(obj->be_flag & 1)) {
                    if (--i == 0) {
                        break;
                    }
                    numObj++;
                    if (num <= numObj) {
                        numObj = 0;
                    }
                } else {
                    break;
                }
            }
        }
        if (joy->on & JOY_A) {
            if (ObjMgrWork(numObj)->be_flag & 1) {
                cModel* parts = ObjMgrWork(numObj)->getPartsPtr(0);
                if (parts == NULL) {
                    cam->param.at = ObjMgrWork(numObj)->pos;
                } else {
                    cam->param.at = parts->worldPos;
                }
            } else {
                cam->param.at.x = 0.0f;
                cam->param.at.y = 0.0f;
                cam->param.at.z = 0.0f;
            }
            CameraSetOrientationZeroRoll(cam);
        }
        break;
    }
    case 2:
        if (joy->on & JOY_A) {
            if (pPL->be_flag & 1) {
                cModel* parts = pPL->getPartsPtr(0);
                if (parts == NULL) {
                    cam->param.at = pPL->pos;
                } else {
                    cam->param.at = parts->worldPos;
                }
            } else {
                cam->param.at.x = 0.0f;
                cam->param.at.y = 0.0f;
                cam->param.at.z = 0.0f;
            }
            CameraSetOrientationZeroRoll(cam);
        }
        break;
    case 3:
        if (joy->on & JOY_A) {
            cam->param.at.x = 0.0f;
            cam->param.at.y = 0.0f;
            cam->param.at.z = 0.0f;
            CameraSetOrientationZeroRoll(cam);
        }
        break;
    case 4:
        break;
    }
    if (cam_mode == 5) {
        (this->*camera_type_tbl[1])(cam, joy);
    } else {
        cam->dist = PSVECDistance(&cam->param.pos, &cam->param.at);
        (this->*camera_type_tbl[key_type])(cam, joy);
    }
    if ((pG->flags_60 & 0x10000000) && info_disp) {
        Mtx inv;
        Vec pos;
        Vec at;
        PSMTXInverse(pPL->mat, inv);
        PSMTXMultVec(inv, &pG->Cam.param.pos, &pos);
        PSMTXMultVec(inv, &pG->Cam.param.at, &at);
        eprintf(72, 420, 5, 0, "CAMPOS @pPl->mat: (%5.1f, %5.1f, %5.1f)", pos.x, pos.y, pos.z);
        eprintf(72, 434, 5, 0, "TARGET @pPl->mat: (%5.1f, %5.1f, %5.1f)", at.x, at.y, at.z);
    }
}

void debugCamera::camera_type_00(Camera* cam, JOY* joy)
{
    Vec mv = {0.0f, 0.0f, 0.0f};
    f32 spd = 100.0f;
    f32 d = cam->dist / 1000.0f;

    if (joy->on & (JOY_R | JOY_L)) {
        f32 t;
        if (joy->on & JOY_R) {
            f32 r = joy->trigR / 150.0f;
            t = r * -1000.0f * r * r;
        } else {
            f32 r = joy->trigL / 150.0f;
            t = r * 1000.0f * r * r;
        }
        t *= (d / 10.0f + 1.0f) * 0.5f;
        t *= gain;
        switch (ProjType) {
        case 1: {
            f32 dist = cam->dist + t;
            if (dist < 100.0f) {
                mv.z = 0.0f;
            }
            CameraCamposDistance(cam, dist);
            break;
        }
        case 2:
            ORTHO_ZOOM(t);
            ORTHO_B = -ORTHO_T;
            ORTHO_R = -ORTHO_L;
            break;
        }
    }
    if (joy->sx) {
        Vec axis = {0.0f, 1.0f, 0.0f};
        CameraRotAxisPosRad(cam, &axis, &cam->param.at, gain * (f32) joy->sx * 0.05f * DEG);
    }
    if (joy->sy) {
        CameraCamposRot(cam, 'x', gain * (f32) joy->sy * -0.05f * DEG);
    }
    if (joy->on & JOY_LEFT) {
        mv.x = -100.0f;
    }
    if (joy->on & JOY_RIGHT) {
        mv.x = 100.0f;
    }
    if ((joy->on & (JOY_X | JOY_UP)) == (JOY_X | JOY_UP)) {
        mv.y = 100.0f;
    }
    if ((joy->on & (JOY_X | JOY_DOWN)) == (JOY_X | JOY_DOWN)) {
        mv.y = -100.0f;
    }
    if ((joy->on & (JOY_X | JOY_UP)) == JOY_UP) {
        mv.z = -100.0f;
    }
    if ((joy->on & (JOY_X | JOY_DOWN)) == JOY_DOWN) {
        mv.z = 100.0f;
    }
    PSVECScale(&mv, &mv, (d / 10.0f + 1.0f) * 2.0f);
    PSVECScale(&mv, &mv, gain);
    if (mv.x != 0.0f || mv.y != 0.0f || mv.z != 0.0f) {
        Vec axis;
        Mtx m;
        Vec up = {0.0f, 1.0f, 0.0f};
        Vec dir;
        Vec trans;
        if (!along_xyz) {
            dir.x = cam->mat[0][2];
            dir.y = cam->mat[1][2];
            dir.z = cam->mat[2][2];
            if (dir.x == 0.0f && dir.z == 0.0f) {
                dir.x = cam->mat[0][1];
                dir.y = cam->mat[1][1];
                dir.z = cam->mat[2][1];
            }
            dir.y = 0.0f;
#line 452 "D:/Bio4/Prog/db_cam.cpp"
            VECNormalize(&dir, &dir);
            PSVECCrossProduct(&up, &dir, &axis);
            MTX_SET_COLUMNS(m, axis, up, dir, trans);
        } else {
            PSMTXIdentity(m);
        }
        PSMTXMultVecSR(m, &mv, &mv);
        CameraDolly(cam, &mv);
    }
    if (joy->ssx) {
        Vec axis = {0.0f, 1.0f, 0.0f};
        CameraRotAxisPosRad(cam, &axis, &cam->param.pos, gain * (f32) -joy->ssx * 0.05f * DEG);
    }
    if (joy->ssy) {
        CameraTargetRot(cam, 'x', gain * (f32) -joy->ssy * -0.05f * DEG);
    }
}

void debugCamera::camera_type_01(Camera* cam, JOY* joy)
{
    Vec mv = {0.0f, 0.0f, 0.0f};
    f32 dist_min = 1500.0f;
    f32 d = cam->dist / 1000.0f;

    if (joy->on & (JOY_R | JOY_L)) {
        f32 t;
        if (joy->on & JOY_R) {
            f32 r = joy->trigR / 150.0f;
            t = r * -1000.0f * r * r;
        } else {
            f32 r = joy->trigL / 150.0f;
            t = r * 1000.0f * r * r;
        }
        t *= (d / 10.0f + 1.0f) * 0.5f;
        t *= gain;
        switch (ProjType) {
        case 1: {
            f32 dist = cam->dist + t;
            if (dist < 1500.0f) {
                if (!(joy->on & JOY_A)) {
                    mv.z = dist - 1500.0f;
                }
                dist = 1500.0f;
            }
            CameraCamposDistance(cam, dist);
            break;
        }
        case 2:
            ORTHO_ZOOM(t);
            ORTHO_B = -ORTHO_T;
            ORTHO_R = -ORTHO_L;
            break;
        }
    }
    if (joy->ssx) {
        mv.x = (f32) joy->ssx * 2.0f;
    }
    if (joy->ssy) {
        mv.y = (f32) joy->ssy * 2.0f;
    }
    PSVECScale(&mv, &mv, (d / 10.0f + 1.0f) * 2.0f);
    PSVECScale(&mv, &mv, gain);
    if (mv.x != 0.0f || mv.y != 0.0f || mv.z != 0.0f) {
        PSMTXMultVecSR(cam->mat, &mv, &mv);
        CameraDolly(cam, &mv);
    }
    if (joy->sx) {
        Vec axis = {0.0f, 1.0f, 0.0f};
        CameraRotAxisPosRad(cam, &axis, &cam->param.at, (f32) joy->sx * 0.05f * gain * DEG);
    }
    if (joy->sy) {
        CameraCamposRot(cam, 'x', -(f32) joy->sy * 0.05f * gain * DEG);
    }
}

void debugCamera::menu(Camera* cam, JOY* joy)
{
    static int (debugCamera::*sel0_menu_tbl[4])(JOY*) = {
        &debugCamera::menuFlag,
        &debugCamera::menuCamera,
        &debugCamera::menuHitDisp,
        &debugCamera::menuAdjust,
    };
    static int old_cam_mode = 0;
    static CameraParam cameraBak;
    // Word view of campos: the copy below names its y word (see the anchors there).
    static union { Vec v; u32 w[3]; } campos = {{0.0f, 10000.0f, 0.0f}};
    static Vec target = {0.0f, 0.0f, 0.0f};
    static Vec up = {0.0f, 0.0f, -1.0f};
    int ret;

    if (mode == 0) {
        return;
    }
    ret = (this->*sel0_menu_tbl[sel])(joy);
    if (ret == 0) {
        if (joy->trg & JOY_L) {
            sel--;
        }
        if (joy->trg & JOY_R) {
            sel++;
        }
        if (sel < 0) {
            sel = 3;
        } else if (sel > 3) {
            sel = 0;
        }
    }
    if (ret == -1) {
        mode = 0;
        timer = 5;
        pG->debug_mode = save_mode;
    }
    // The split `lbz r0,24(r31); clrlwi r11,r0,24` head and the `beq` landing on the tail's `stw`
    // are gcse PRE of the cam_mode byte into `old_cam_mode = cam_mode` below; it needs the empty
    // join block after the inner if/else, kept alive by the `do {} while (0)` at the end of this body.
    if (old_cam_mode != cam_mode) {
        switch (cam_mode) {
        case 0:
            CamCtrl.flags_2C = (CamCtrl.flags_2C & ~8) | 0x10;
            break;
        case 2:
            CamCtrl.state = 10;
            CamCtrl.flags_2C |= 8;
            break;
        case 3:
            CamCtrl.state = 7;
            CamCtrl.flags_2C |= 8;
            break;
        case 4:
            CamCtrl.state = 8;
            CamCtrl.flags_2C |= 8;
            break;
        }
        CamCtrl.sub_state = 0;
        if (cam_mode != 5) {
            if (old_cam_mode == 5) {
                pG->Cam.param = cameraBak;
                ProjType = 1;
            }
            CameraSetOrientationRoll(&pG->Cam);
        } else {
            cameraBak = pG->Cam.param;
            ProjType = 2;
            u32 t;
            // One destination pointer per copy: the three `addi rD,pG,off` bases are distinct
            // pseudos (r10/r9/r11), so global alloc does not have to give one long-lived `d`
            // the same register three times.
            {
                // The campos copy as three named words: `campos.w[k]` loads are `mem/s` and the
                // `*(u32*)((u32)d0 + k)` stores are flagless MEMs, the same RTL as memcpy's
                // move_by_pieces (a `*(u32*)(d0 + k)` store would be `mem/s`, `((u32*)d0)[k]` too).
                u8* d0 = (u8*) &pG->Cam.param.pos;
                u32 wx = campos.w[0];
                u32 wy = campos.w[1];
                u32 wz = campos.w[2];
                *(u32*) d0 = wx;
                *(u32*) ((u32) d0 + 4) = wy;
                *(u32*) ((u32) d0 + 8) = wz;
                // COMPILER-DIFF: candidate (sched2 tie: the target issues `lwz r0,4(r9)` before
                // `lwz r8,8(r9)`; both loads have priority 29 and 13 dependents at sched2 and the
                // LUID tie goes to z, which sched1 issues first because it kills the campos base).
                // A codeless reader of the y word gives y a 14th sched2 dependent. The `"f"(0.0f)`
                // input (the roll constant's pool load, issued at t13 after the copy stores) makes
                // the anchor ready at t14 so it lands behind the `lfs` at sched1 (any earlier slot
                // lengthens the LC50 high's life and swaps r10/r11); `=&r` keeps local-alloc from
                // tying t to wy; the second anchor after FSet keeps t (and so this insn) alive to
                // sched2 without touching the up copy's pG reload (a slot between the up stores and
                // that reload breaks the fake-death overlap that gives its `addi` r11).
                asm("" : "=&r"(t) : "r"(wy), "f"(0.0f));
            }
            {
                u8* d1 = (u8*) &pG->Cam.param.at;
                memcpy(d1, &target, sizeof(Vec));
            }
            {
                u8* d2 = (u8*) &pG->Cam.up;
                memcpy(d2, &up, sizeof(Vec));
            }
            FSet(pG->Cam.param.roll, 0.0f);
            // COMPILER-DIFF: candidate (sched2 tie, second half; see the campos copy above).
            asm("" : "=m"(ProjType) : "r"(t));
            CameraSetOrientationUp(&pG->Cam);
            pG->flags_60 |= 0x10000000;
        }
        do { } while (0);
    }
    old_cam_mode = cam_mode;
}

int debugCamera::menuCamera(JOY* joy)
{
    static const char* str[4] = {"Roll", "FOVy", "Gain", "Play"};
    static int pos[2] = {240, 294};
    Camera* cam = &pG->Cam;
    int d;
    int i;

    if (joy->trg & JOY_B) {
        return -1;
    }
    if (joy->rep & 0x80008) {
        cursor--;
    }
    if (joy->rep & 0x40004) {
        cursor++;
    }
    if (cursor < 0) {
        cursor = 3;
    } else if (cursor > 3) {
        cursor = 0;
    }
    d = 0;
    if (joy->rep & 0x1) {
        d = -1;
    }
    if (joy->rep & 0x10000) {
        d = -10;
    }
    if (joy->rep & 0x2) {
        d = 1;
    }
    if (joy->rep & 0x20000) {
        d = 10;
    }
    if (d) {
        switch (cursor) {
        case 0:
            cam->param.roll += (f32) d * DEG;
            if (cam->param.roll < -PI) {
                cam->param.roll = -PI;
            }
            if (cam->param.roll > PI) {
                cam->param.roll = PI;
            }
            CameraSetOrientationRoll(cam);
            break;
        case 1:
            cam->param.fovy += (f32) d * 0.5f;
            if (cam->param.fovy < 1.0f) {
                cam->param.fovy = 1.0f;
            }
            if (cam->param.fovy > 179.0f) {
                cam->param.fovy = 179.0f;
            }
            CamCtrl.camera.param.fovy = cam->param.fovy;
            break;
        case 2:
            gain += (f32) d * 0.1f;
            gain = gain < 0.1f ? 0.1f : (gain > 10.0f ? 10.0f : gain);
            break;
        case 3: {
            int max = -1;
            CameraDataHeader* data = CamCtrl.data;
            CameraAreaRec* rec = (CameraAreaRec*) (data + 1);
            CameraAreaInfo* area = (CameraAreaInfo*) (rec + data->numArea);
            CameraCut* cut = (CameraCut*) (area + data->numArea);
            int n;
            for (n = 0; n < data->numCut; n++, cut++) {
                if (cut->camera_no > max) {
                    max = cut->camera_no;
                }
            }
            cam_no += d;
            cam_no = cam_no < 0 ? max : (cam_no > max ? 0 : cam_no);
            break;
        }
        }
    }
    if (cursor == 3) {
        switch (play) {
        case 0:
            if (joy->trg & JOY_A) {
                if (CamCtrl.DataSearch(cam_no)->type == 6) {
                    CamCtrl.CutCall(cam_no);
                    pG->debug_mode = save_mode;
                    play++;
                }
            }
            break;
        case 1:
            if (CamCtrl.IsMotionEnd()) {
                play++;
            }
            break;
        case 2:
            CamCtrl.Comeback(0);
            pG->debug_mode = 1;
            play = 0;
            break;
        }
    }
    eprintf(240, 280, 5, 0, "----- CAMERA -----");
    for (i = 0; i < 4; i++) {
        int col = (i == cursor) ? 4 : 0;
        eprintf(pos[0], pos[1] + i * 14, col, 0, "%s", str[i]);
        switch (i) {
        case 0:
            eprintf(pos[0] + 40, pos[1], col, 0, "%f", cam->param.roll);
            break;
        case 1:
            eprintf(pos[0] + 40, pos[1] + 14, col, 0, "%f", cam->param.fovy);
            break;
        case 2:
            eprintf(pos[0] + 40, pos[1] + 28, col, 0, "%f", gain);
            break;
        case 3:
            eprintf(pos[0] + 40, pos[1] + 42, col, 0, "%02d", cam_no);
            break;
        }
    }
    return 0;
}

int debugCamera::menuFlag(JOY* joy)
{
    static const char* menu_str[7] = {"DBG_DBG_CAM", "KEY TYPE", "TARGET SEARCH", "INFO_DISP",
                                      "ALONG W_XYZ", "DBG_BACK_CLIP", "CAMERA MODE"};
    static const char* mode_str[6] = {"AREA   ", "BINOCLR", "BEHIND ", "FREE   ", "DEBUG  ", "BIRD   "};
    static const char* target_str[5] = {"EM ", "OBJ", "PL ", "ORG", "OFF"};
    int on = 0;
    int old;
    int d;
    int i;
    int j;
    int x;
    int y;

    if (joy->trg & JOY_B) {
        return -1;
    }
    if (joy->rep & 0x80008) {
        cursor--;
    }
    if (joy->rep & 0x40004) {
        cursor++;
    }
    if (cursor < 0) {
        cursor = 6;
    } else if (cursor > 6) {
        cursor = 0;
    }
    old = lr;
    if (joy->trg & 0x10001) {
        lr--;
    }
    if (joy->trg & 0x20002) {
        lr++;
    }
    d = lr - old;
    if (d != 0) {
        switch (cursor) {
        case 0:
            if (pG->flags_60 & 0x10000000) {
                pG->flags_60 &= ~0x10000000;
            } else {
                pG->flags_60 |= 0x10000000;
            }
            break;
        case 1:
            if (d > 0) {
                key_type++;
            } else {
                key_type--;
            }
            key_type = key_type < 0 ? 1 : (key_type > 1 ? 0 : key_type);
            break;
        case 2:
            if (d > 0) {
                target_type++;
            } else {
                target_type--;
            }
            target_type = target_type < 0 ? 4 : (target_type > 4 ? 0 : target_type);
            break;
        case 3:
            if (d > 0) {
                info_disp = 0;
            } else {
                info_disp = 1;
            }
            break;
        case 4:
            if (d > 0) {
                along_xyz = 0;
            } else {
                along_xyz = 1;
            }
            break;
        case 5:
            if (pG->flags_60 & 0x20000000) {
                pG->flags_60 &= ~0x20000000;
            } else {
                pG->flags_60 |= 0x20000000;
            }
            break;
        case 6:
            cam_mode = lr = lr < 0 ? 5 : (lr > 5 ? 0 : lr);
            break;
        }
    }
    x = 30;
    y = 21;
    eprintf(x * 8, (y - 1) * 14, 5, 0, "------ FLAG ------");
    // `(y + i) * 14` is written out in every row: PRE shares one `y + i` (r25) for the arms, and in
    // the `case 6` arm cse already knows i == 6, so its `y + 6` becomes the hoisted `li r19,27`.
    // The ON/OFF columns are locals (`xon`/`xoff` below): gcse cprop turns `x + 19` into `li 49`
    // and loop.c keeps a 1-insn-lifetime constant in the arm (thr 71 * life 1 < ic 194, then
    // combine folds it into `li r3,392`); a local assigned before the `if (on)` lives long enough
    // to be hoisted (`li r20,49; slwi r3,r20,3`) and takes the extra callee-saved slot (r16..r31).
    // The key/target j-loops compute their colour in a statement (`cj`) before the call: with the
    // ternary inside the argument list the `(y + i) * 14` mult sits before the ternary's join label,
    // so loop.c cannot substitute it into its single use (`no_labels_between_p`) and hoists it
    // instead; as a statement the mult follows the join, is folded into `r4 = P * 14` (a hard-reg
    // dest, never movable) and stays in the loop (`mulli r4,r25,14`). That keeps the shared `y + i`
    // (r25) live across the loop calls, which lets sched1 move the PRE copy `mr r25,r4` up to the
    // head's `addi` (REG_N_CALLS_CROSSED != 0 -> no anti-dependence on the head call), so the head's
    // own `y + i` dies at its mult and gets r4 (`mulli r4,r4,14`).
    for (i = 0; i < 7; i++) {
        int col = (i == cursor) ? 4 : 0;
        u8 c = col;
        eprintf(x * 8, (y + i) * 14, c, 0, "%s", menu_str[i]);
        switch (i) {
        case 0:
        case 3:
        case 4:
        case 5: {
            int c_on;
            int c_off;
            switch (i) {
            case 0:
                on = pG->flags_60 & 0x10000000;
                break;
            case 5:
                on = pG->flags_60 & 0x20000000;
                break;
            case 3:
                on = info_disp;
                break;
            case 4:
                on = along_xyz;
                break;
            }
            if (on) {
                c_on = col;
                c_off = 7;
            } else {
                c_on = 7;
                c_off = col;
            }
            {
                int xon = x + 15;
                int xoff = x + 19;
                eprintf(xon * 8, (y + i) * 14, c_on, 0, "ON ");
                eprintf(xoff * 8, (y + i) * 14, c_off, 0, "OFF");
            }
            break;
        }
        case 1:
            for (j = 0; j < 2; j++) {
                int cj = (j == key_type) ? col : 7;
                eprintf((x + 15 + j * 5) * 8, (y + i) * 14, cj, 0, "%s", key_str[j]);
            }
            break;
        case 2:
            for (j = 0; j < 5; j++) {
                int cj = (j == target_type) ? col : 7;
                eprintf((x + 15 + j * 4) * 8, (y + i) * 14, cj, 0, "%s", target_str[j]);
            }
            break;
        case 6:
            eprintf((x + 15) * 8, (y + i) * 14, c, 0, "%02d: %s", cam_mode, mode_str[cam_mode]);
            break;
        }
    }
    return 0;
}

int debugCamera::menuHitDisp(JOY* joy)
{
    static int view_mode = 0;
    static int old_view_mode = -1;
    static int shadow_flag = 0;
    static int pos[2] = {240, 294};
    const char* str[9] = {"Game screen", "Scroll atari + BG", "Scroll atari", "Sprite atari + BG", "Sprite atari",
                          "Shadow + BG", "Shadow", "Mirror + BG", "Mirror"};

    if (joy->trg & JOY_B) {
        return -1;
    }
    if (joy->trg & 0x10001) {
        Dec(view_mode);
    }
    if (joy->trg & 0x20002) {
        Inc(view_mode);
    }
    if (view_mode < 0) {
        view_mode = 8;
    } else if (view_mode > 8) {
        view_mode = 0;
    }
    if (old_view_mode != view_mode) {
        old_view_mode = view_mode;
        switch (view_mode) {
        case 0:
            if (!shadow_flag) {
                BitOff(pG->flags_58, 0x2000000);
            }
            BitOff(pG->flags_60, 0x800000);
            BitOff(pG->flags_500C, 0x80000000);
            BitOff(pG->flags_60, 0x80000);
            break;
        case 1:
            BitOn(pG->flags_60, 0x40000000);
            break;
        case 3:
            if (!shadow_flag) {
                BitOff(pG->flags_58, 0x2000000);
            }
            BitOff(pG->flags_500C, 0x80000000);
            BitOff(pG->flags_60, 0x40000000);
            BitOn(pG->flags_60, 0x200000);
            break;
        case 5:
            if (!shadow_flag) {
                BitOff(pG->flags_58, 0x2000000);
            }
            BitOff(pG->flags_500C, 0x80000000);
            BitOff(pG->flags_60, 0x200000);
            BitOn(pG->flags_60, 0x800000);
            break;
        case 2:
        case 4:
        case 6:
            shadow_flag = pG->flags_58 & 0x2000000;
            BitOn(pG->flags_500C, 0x80000000);
            BitOn(pG->flags_58, 0x2000000);
            break;
        case 7:
            BitOff(pG->flags_500C, 0x80000000);
            BitOn(pG->flags_60, 0x80000);
            BitOff(pG->flags_60, 0x800000);
            break;
        case 8:
            BitOn(pG->flags_500C, 0x80000000);
            BitOn(pG->flags_60, 0x80000);
            break;
        }
    }
    eprintf(240, 280, 5, 0, "------ DISP ------");
    eprintf(pos[0], pos[1], 0, 0, "%s", str[view_mode]);
    return 0;
}

int debugCamera::menuAdjust(JOY* joy)
{
    static void (debugCamera::*camera_type_tbl[4])(Camera*, JOY*) = {
        &debugCamera::camera_type_00,
        &debugCamera::camera_type_01,
        &debugCamera::camera_type_00,
        &debugCamera::camera_type_00,
    };
    static Vec target_bak;
    static int old_ret = 0;
    Camera* cam = &CamCtrl.camera;
    int ret;

    ret = adjust_qFPS(joy, 240, 294, 0, NULL);
    if (ret == 6) {
        if (old_ret != 6) {
            CamCtrl.sub_state = 0;
        }
        CamCtrl.state = 10;
        CamCtrl.Move();
        pG->Cam = CamCtrl.camera;
    }
    old_ret = ret;
    cam->dist = PSVECDistance(&cam->param.pos, &cam->param.at);
    switch (ret) {
    case -1:
        return -1;
    case 1:
        (this->*camera_type_tbl[key_type])(cam, &Joy[1]);
        target_bak = cam->param.at;
        break;
    case 2:
        (this->*camera_type_tbl[key_type])(cam, &Joy[1]);
        cam->param.at = target_bak;
        break;
    }
    eprintf(240, 280, 5, 0, "----- ADJUST -----");
    return ret;
}

void CameraDrawTarget(Camera* cam, int flag)
{
    Vec v[2];
#define a v[0]
#define b v[1]

    if (pG->debug_mode == 0) {
        return;
    }
    if ((s32) pG->flags_60 < 0) {
        if (flag & 1) {
            flag |= 1;
        } else {
            flag &= ~1;
        }
    }
    a = cam->param.at;
    b = cam->param.at;
    a.x += 300.0f;
    b.x -= 300.0f;
    Draw_line3d(&a, &b, 0xFFFF0000, 0);
    b = a;
    b.x -= 60.0f;
    b.z += 60.0f;
    Draw_line3d(&a, &b, 0xFFFF0000, 0);
    b = a;
    b.x -= 60.0f;
    b.z -= 60.0f;
    Draw_line3d(&a, &b, 0xFFFF0000, 0);

    a = cam->param.at;
    b = cam->param.at;
    a.y += 300.0f;
    b.y -= 300.0f;
    Draw_line3d(&a, &b, 0xFF00FF00, 0);
    b = a;
    b.y -= 60.0f;
    b.x += 60.0f;
    Draw_line3d(&a, &b, 0xFF00FF00, 0);
    b = a;
    b.y -= 60.0f;
    b.x -= 60.0f;
    Draw_line3d(&a, &b, 0xFF00FF00, 0);

    a = cam->param.at;
    b = cam->param.at;
    a.z += 300.0f;
    b.z -= 300.0f;
    Draw_line3d(&a, &b, 0xFF2020FF, 0);
    b = a;
    b.z -= 60.0f;
    b.x += 60.0f;
    Draw_line3d(&a, &b, 0xFF2020FF, 0);
    b = a;
    b.z -= 60.0f;
    b.x -= 60.0f;
    Draw_line3d(&a, &b, 0xFF2020FF, 0);

    if (flag & 1) {
        a = cam->param.at;
        b = cam->param.at;
        a.y = 50.0f;
        b.y -= 300.0f;
        Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
        a = cam->param.at;
        b = cam->param.at;
        b.y = 50.0f;
        a.y = 50.0f;
        a.x += 300.0f;
        b.x -= 300.0f;
        Draw_line3d(&a, &b, 0xFFFF8080, 0);
        a = cam->param.at;
        b = cam->param.at;
        b.y = 50.0f;
        a.y = 50.0f;
        a.z += 300.0f;
        b.z -= 300.0f;
        Draw_line3d(&a, &b, 0xFF8080FF, 0);
    }
#undef a
#undef b
}

void CameraDebugInformation()
{
    CameraControl* cc = &CamCtrl;
    Camera* cam = &cc->camera;
    eprintf(56, 266, 0, 15, "----- GAME CAMERA -----");
    eprintf(56, 280, 0, 15, "Cpos : (%.2f, %.2f, %.2f)", cam->param.pos.x, cam->param.pos.y, cam->param.pos.z);
    eprintf(56, 294, 0, 15, "Trgt : (%.2f, %.2f, %.2f)", cam->param.at.x, cam->param.at.y, cam->param.at.z);
    eprintf(56, 308, 0, 15, "Roll : %.2f", cam->param.roll);
    eprintf(176, 308, 0, 15, "FOVy : %.2f", cam->param.fovy);
    cam = &pG->Cam;
    eprintf(56, 336, 0, 15, "----- DEBUG CAMERA ----");
    eprintf(56, 350, 0, 15, "Cpos : (%.2f, %.2f, %.2f)", cam->param.pos.x, cam->param.pos.y, cam->param.pos.z);
    eprintf(56, 364, 0, 15, "Trgt : (%.2f, %.2f, %.2f)", cam->param.at.x, cam->param.at.y, cam->param.at.z);
    eprintf(56, 378, 0, 15, "Roll : %.2f", cam->param.roll);
    eprintf(176, 378, 0, 15, "FOVy : %.2f", cam->param.fovy);
}

void moveOnPlaneXZ(Vec* in, Vec* out)
{
    Camera* cam = &pG->Cam;
    Vec vx;
    Vec vy;
    Vec vz;

    if (in->x == 0.0f && in->y == 0.0f && in->z == 0.0f) {
        memclr_asm(out, sizeof(Vec));
        return;
    }
    getColumn(cam->mat, 0, &vx);
    getColumn(cam->mat, 1, &vy);
    getColumn(cam->mat, 2, &vz);
    if (vz.y != 0.0f) {
        Vec dx;
        Vec dz;
        Vec zero0 = {0.0f, 0.0f, 0.0f};
        Vec zero1 = {0.0f, 0.0f, 0.0f};
        Vec plane_p = {0.0f, 0.0f, 0.0f};
        Vec plane_n = {0.0f, 1.0f, 0.0f};
        Vec campos = cam->param.pos;
        Vec q;
        Vec s;
        Vec r;
        Mtx m;
        OrthographicProjection(&campos, &s, &vz, &plane_p, &plane_n);
        PSVECScale(&vx, &vx, 1000.0f);
        PSVECAdd(&campos, &vx, &q);
        OrthographicProjection(&q, &r, &vz, &plane_p, &plane_n);
        PSVECSubtract(&r, &s, &dx);
#line 1427 "D:/Bio4/Prog/db_cam.cpp"
        VECNormalize(&dx, &dx);
        PSVECScale(&vy, &vy, 1000.0f);
        PSVECAdd(&campos, &vy, &q);
        OrthographicProjection(&q, &r, &vz, &plane_p, &plane_n);
        PSVECSubtract(&r, &s, &dz);
#line 1434 "D:/Bio4/Prog/db_cam.cpp"
        VECNormalize(&dz, &dz);
        MTX_SET_COLUMNS(m, dx, dz, zero0, zero1);
        PSMTXMultVecSR(m, in, out);
    } else {
        Vec v;
        v.x = in->x;
        v.y = 0.0f;
        v.z = in->y;
        PSMTXMultVecSR(cam->mat, &v, out);
    }
}

void drawGround(int big)
{
    const f32 unit = 1000.0f;
    int n = big ? 60 : 30;
    Vec a;
    Vec b;
    int i;
    f32 neg;
    f32 pos;

    b.y = 0.0f;
    a.y = 0.0f;
    a.x = (f32) -n * unit;
    b.x = (f32) n * unit;
    for (i = -n; i <= n; i++) {
        if (i != 0) {
            b.z = (f32) i * unit;
            a.z = (f32) i * unit;
            Draw_line3d(&a, &b, 0xFF404040, 0);
        }
    }
    b.y = 0.0f;
    a.y = 0.0f;
    a.z = (f32) -n * unit;
    b.z = (f32) n * unit;
    for (i = -n; i <= n; i++) {
        if (i != 0) {
            b.x = (f32) i * unit;
            a.x = (f32) i * unit;
            Draw_line3d(&a, &b, 0xFF404040, 0);
        }
    }
    neg = (f32) -n * unit;
    pos = (f32) n * unit;
    a.x = neg * 1.1f;
    a.z = b.z = a.y = b.y = 0.0f;
    b.x = pos * 1.1f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.x = pos;
    a.z = pos * 0.05f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.z = neg * 0.05f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.x = b.x = a.y = b.y = 0.0f;
    a.z = neg * 1.1f;
    b.z = pos * 1.1f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.z = pos;
    a.x = pos * 0.05f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.x = neg * 0.05f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
}

int adjust_qFPS(JOY* joy, int x, int y, int flag, int* out)
{
    static const char* menu_str[5] = {"Select Site", "Symmetry", "Follow Grnd", "Fovy", "Reset"};
    static u8* p_offset;   // byte pointers: the original copies the records with memcpy
    static u8* p_counter;
    static int menu_no = 0;
    static int menu_level = 0;
    static int symmetry_flag = 1;
    static int site_col = 0;
    static int site_row = 0;
    static int site_LR = 0;
    static int site_NF = 0;
    static int site_UMD = 0;
    static int yes_no = 0;
    static int near_far = 0;
    GlobalWork* g = pG;
    Camera* cam = &g->Cam;
    CameraQuasiFPS* q = &CamCtrl.qfps;
    Mtx inv;
    Vec target;
    Vec campos;
    Vec close;
    int cx = 0;
    int cy = 0;
    int ret = 0;
    int i;
    int j;

    PSMTXInverse(pPL->mat, inv);
    if (flag & 1) {
        if (flag & 2) {
            menu_no = -1;
        } else {
            menu_no = 0;
        }
        site_col = 1;
        site_row = 4;
        menu_level = 0;
        near_far = 0;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 3; j++) {
                g_local_ready[i][j] = g_readyOfs[0][i][j];
                g_local_trans[i][j] = g_transOfs[0][i][j];
            }
        }
        q->setAreaData(g_local_ready, g_local_trans);
        g_local_floor_ratio = q->getFloorRatio();
        return 0;
    }
    switch (menu_level) {
    case 0:
        if (joy->trg & JOY_B) {
            menu_no = 0;
            menu_level = 0;
            ret = -1;
            break;
        }
        if (joy->rep & JOY_UP) {
            Dec(menu_no);
        }
        if (joy->rep & JOY_DOWN) {
            Inc(menu_no);
        }
        if (flag & 2) {
            menu_no = menu_no < -1 ? -1 : (menu_no > 6 ? 6 : menu_no);
            *out = menu_no;
        } else {
            menu_no = menu_no < 0 ? 0 : (menu_no > 4 ? 4 : menu_no);
        }
        switch (menu_no) {
        case 0:
            if (joy->trg & JOY_A) {
                menu_level = 1;
                BitOn(pG->flags_64, 0x20000000);
                if ((s32) pG->flags_60 >= 0) {
                    CamDbg.cam_mode = 2;
                }
                BitOn(pG->flags_170, 0x10000000);
            }
            break;
        case 1:
            if (joy->rep & (JOY_LEFT | JOY_RIGHT)) {
                symmetry_flag = !symmetry_flag;
            }
            break;
        case 2: {
            f32 step = (joy->on & JOY_X) ? 0.01f : 0.1f;
            if (joy->rep & JOY_LEFT) {
                FSet(g_local_floor_ratio, g_local_floor_ratio - step);
            }
            if (joy->rep & JOY_RIGHT) {
                FSet(g_local_floor_ratio, g_local_floor_ratio + step);
            }
            FSet(g_local_floor_ratio,
                 g_local_floor_ratio < 0.0f ? 0.0f : (g_local_floor_ratio > 2.0f ? 2.0f : g_local_floor_ratio));
            if (joy->trg & JOY_A) {
                ret = 5;
                q->setFloorRatio(g_local_floor_ratio);
            }
            break;
        }
        case 3:
            if (joy->trg & JOY_A) {
                q->getAreaData(g_local_ready, g_local_trans);
                g_local_fovy[0] = g_local_ready[0][1].fovy;
                g_local_fovy[1] = g_local_trans[0][1].fovy;
                menu_level = 5;
            }
            break;
        case 4:
            if (joy->trg & JOY_A) {
                menu_level = 4;
                Set(yes_no, 0);
            }
            break;
        }
        break;
    case 1:
        if (joy->trg & JOY_B) {
            BitOff(pG->flags_64, 0x20000000);
            BitOff(pG->flags_170, 0x10000000);
            if (CamDbg.cam_mode != 2) {
                CamDbg.cam_mode = 0;
            }
            menu_level = 0;
        } else if (joy->trg & JOY_A) {
            q->getAreaData(g_local_ready, g_local_trans);
            BitOn(pG->flags_170, 0x40000000);
            menu_level = 2;
        } else {
            int old_umd = site_UMD;
            int old_nf = site_NF;
            if (joy->rep & JOY_LEFT) {
                Dec(site_col);
            }
            if (joy->rep & JOY_RIGHT) {
                Inc(site_col);
            }
            site_col = site_col < 0 ? 0 : (site_col > 1 ? 1 : site_col);
            if (symmetry_flag) {
                site_col = 1;
            }
            if (joy->rep & JOY_UP) {
                Dec(site_row);
            }
            if (joy->rep & JOY_DOWN) {
                Inc(site_row);
            }
            site_row = site_row < 0 ? 0 : (site_row > 5 ? 5 : site_row);
            if (site_col == 0) {
                site_LR = 0;
            } else {
                site_LR = site_col;
            }
            if (site_row <= 2) {
                Set(site_UMD, site_row);
                Set(site_NF, 0);
            } else {
                Set(site_NF, 1);
                Set(site_UMD, site_row - 3);
            }
            if (site_NF) {
                if (site_LR) {
                    q->site = 2;
                } else {
                    q->site = 3;
                }
            } else {
                if (site_LR) {
                    q->site = 0;
                } else {
                    q->site = 1;
                }
            }
            switch (q->site) {
            case 0:
                p_offset = (u8*) &g_local_ready[0][site_UMD];
                p_counter = (u8*) &g_local_ready[1][site_UMD];
                break;
            case 1:
                p_offset = (u8*) &g_local_ready[1][site_UMD];
                p_counter = (u8*) &g_local_ready[0][site_UMD];
                break;
            case 2:
                p_offset = (u8*) &g_local_trans[0][site_UMD];
                p_counter = (u8*) &g_local_trans[1][site_UMD];
                break;
            case 3:
                p_offset = (u8*) &g_local_trans[1][site_UMD];
                p_counter = (u8*) &g_local_trans[0][site_UMD];
                break;
            }
            switch (site_UMD) {
            case 0:
                q->angle_y = 1.0f;
                break;
            case 1:
                q->angle_y = 0.0f;
                break;
            case 2:
                q->angle_y = -1.0f;
                break;
            }
            if (site_NF == 0) {
                if (old_umd != site_UMD) {
                    switch (site_UMD) {
                    case 0:
                        PlWepMotSet(1);
                        break;
                    case 1:
                        PlWepMotSet(0);
                        break;
                    case 2:
                        PlWepMotSet(2);
                        break;
                    }
                }
            } else {
                if (old_nf == 0) {
                    PlWepMotSet(3);
                }
            }
            MotionMove(pPL, 0);
            ret = 6;
        }
        break;
    case 2:
        MotionMove(pPL, 0);
        if (joy->trg & JOY_B) {
            BitOff(pG->flags_170, 0x40000000);
            menu_level = 1;
        } else if (joy->trg & JOY_A) {
            PSMTXMultVec(inv, &g->Cam.param.pos, &QOFS(p_offset)->campos);
            PSMTXMultVec(inv, &g->Cam.param.at, &QOFS(p_offset)->target);
            if (symmetry_flag) {
                memcpy(p_counter, p_offset, sizeof(QfpsOfs));
                FSet(QOFS(p_counter)->campos.x, -QOFS(p_counter)->campos.x);
                FSet(QOFS(p_counter)->target.x, -QOFS(p_counter)->target.x);
            }
            q->setAreaData(g_local_ready, g_local_trans);
            PSMTXMultVec(pPL->mat, &QOFS(p_offset)->campos2, &CamCtrl.camera.param.pos);
            CameraSetOrientationRoll(&CamCtrl.camera);
            menu_level = 3;
            ret = 3;
        } else {
            ret = 1;
        }
        break;
    case 3:
        MotionMove(pPL, 0);
        if (joy->trg & JOY_B) {
            PSMTXMultVec(pPL->mat, &QOFS(p_offset)->campos, &CamCtrl.camera.param.pos);
            CameraSetOrientationRoll(&CamCtrl.camera);
            menu_level = 2;
        } else if (joy->trg & JOY_A) {
            PSMTXMultVec(inv, &g->Cam.param.pos, &QOFS(p_offset)->campos2);
            if (symmetry_flag) {
                memcpy(p_counter + QOFS_CAMPOS2, p_offset + QOFS_CAMPOS2, sizeof(Vec));
                FSet(QOFS(p_counter)->campos2.x, -QOFS(p_counter)->campos2.x);
            }
            q->setAreaData(g_local_ready, g_local_trans);
            ret = 3;
            BitOff(pG->flags_170, 0x40000000);
            menu_level = 1;
        } else {
            ret = 2;
        }
        break;
    case 4:
        if (joy->trg & JOY_B) {
            menu_level = 0;
            break;
        }
        if (joy->rep & JOY_LEFT) {
            Set(yes_no, 1);
        }
        if (joy->rep & JOY_RIGHT) {
            Set(yes_no, 0);
        }
        if (joy->trg & JOY_A) {
            if (yes_no) {
                for (i = 0; i < 2; i++) {
                    for (j = 0; j < 3; j++) {
                        g_local_ready[i][j] = g_readyOfs[0][i][j];
                        g_local_trans[i][j] = g_transOfs[0][i][j];
                    }
                }
                ret = 4;
                q->setAreaData(g_local_ready, g_local_trans);
            }
            menu_level = 0;
        }
        break;
    case 5:
        if (joy->trg & JOY_B) {
            menu_level = 0;
            break;
        }
        if (joy->trg & JOY_UP) {
            Set(near_far, 0);
        }
        if (joy->trg & JOY_DOWN) {
            Set(near_far, 1);
        }
        {
            f32 step = 1.0f;
            if (joy->on & JOY_X) {
                step = 10.0f;
            }
            if (joy->rep & JOY_LEFT) {
                FSet(g_local_fovy[near_far], g_local_fovy[near_far] - step);
            }
            if (joy->rep & JOY_RIGHT) {
                FSet(g_local_fovy[near_far], g_local_fovy[near_far] + step);
            }
            g_local_fovy[near_far] = g_local_fovy[near_far] < 1.0f
                                         ? 1.0f
                                         : (g_local_fovy[near_far] > 90.0f ? 90.0f : g_local_fovy[near_far]);
        }
        if (joy->trg & JOY_A) {
            ret = 3;
            menu_level = 0;
        }
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 3; j++) {
                g_local_ready[i][j].fovy = g_local_fovy[0];
                g_local_trans[i][j].fovy = g_local_fovy[1];
            }
        }
        q->setAreaData(g_local_ready, g_local_trans);
        break;
    }
    for (i = 0; i < 5; i++) {
        int col = (menu_no == i) ? 4 : 0;
        eprintf(x + cx * 8, y + (cy + i) * 14, col, 0, "%s", menu_str[i]);
        switch (i) {
        case 0:
            break;
        case 1:
            if (symmetry_flag) {
                eprintf(x + (cx + 12) * 8, y + (cy + 1) * 14, col, 0, "ON-/---");
            } else {
                eprintf(x + (cx + 12) * 8, y + (cy + 1) * 14, col, 0, "---/OFF");
            }
            break;
        case 2:
            eprintf(x + (cx + 12) * 8, y + (cy + 2) * 14, col, 0, "%4.2f", g_local_floor_ratio);
            break;
        case 3:
            if (menu_level == 5) {
                static const char* fovy_str[2] = {"READY", "TRANS"};
                for (j = 0; j < 2; j++) {
                    eprintf(x + (cx + 12) * 8, y + (cy + 3 + j) * 14, (near_far == j) ? 4 : 0, 0, "%s", fovy_str[j]);
                    eprintf(x + (cx + 17) * 8, y + (cy + 3 + j) * 14, 0, 0, "%3.1f", g_local_fovy[j]);
                }
            }
            break;
        case 4:
            if (menu_level == 4) {
                if (yes_no) {
                    eprintf(x + (cx + 12) * 8, y + (cy + 4) * 14, col, 0, "YES/---");
                } else {
                    eprintf(x + (cx + 12) * 8, y + (cy + 4) * 14, col, 0, "---/NO-");
                }
            }
            break;
        }
    }
    if (menu_level >= 1 && menu_level <= 3) {
        static const char* umd_str[3] = {"Up", "Mid", "Dwn"};
        eprintf(x + 168, y - 14, 5, 0, "--- SITE ---");
        eprintf(x + 168, y, 5, 0, "LFT RGT");
        eprintf(x + 232, y + 28, 5, 0, "NEAR");
        eprintf(x + 232, y + 70, 5, 0, "FAR");
        for (i = 0; i < 2; i++) {
            // `line` lives in the outer body: the inner loop's giv init stays `line * 14 + y`
            // (mulli hoisted, folded to `li 14` in the outer preheader), not `addi y, 14`.
            int line = cy + 1;
            for (j = 0; j < 6; j++) {
                int col = 0;
                if (i == site_col) {
                    col = (j == site_row) ? 4 : 0;
                }
                eprintf(x + (21 + i * 4) * 8, line * 14 + y + j * 14, col, 0, "%s", umd_str[j % 3]);
            }
        }
    }
    if (menu_level == 2) {
        PSMTXMultVec(inv, &cam->param.at, &target);
        PSMTXMultVec(inv, &cam->param.pos, &campos);
        if (pG->flags_51E4 & 0x18) {
            eprintf(120, 14, 5, 0, "--- CAMERA OFFSET ---");
        }
        eprintf(120, 28, 4, 0, "Target: (%f, %f, %f)", target.x, target.y, target.z);
        eprintf(120, 42, 4, 0, "Campos: (%f, %f, %f)", campos.x, campos.y, campos.z);
    }
    if (menu_level == 3) {
        PSMTXMultVec(inv, &cam->param.at, &target);
        PSMTXMultVec(inv, &cam->param.pos, &close);
        if (pG->flags_51E4 & 0x18) {
            eprintf(120, 14, 5, 0, "---- CLOSE POINT ----");
        }
        eprintf(120, 28, 0, 0, "Target: (%f, %f, %f)", target.x, target.y, target.z);
        eprintf(120, 42, 4, 0, "Campos: (%f, %f, %f)", close.x, close.y, close.z);
    }
    return ret;
}

asm(".section .sdata; .balign 8");
