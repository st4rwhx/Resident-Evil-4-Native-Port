#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "view.h"
#include "db_cam.h"
#include "db_log.h"
#include "joy.h"
#include "quake.h"
#include "main_sub.h"
#include "gx.h"

extern "C" {
void* memset(void* dst, int c, unsigned int n);
f32 sinf(f32);
f32 cosf(f32);
}

extern f32 ORTHO_T;
extern f32 ORTHO_B;
extern f32 ORTHO_L;
extern f32 ORTHO_R;

#define PI 3.1415927f

// Matrix copy written out as loops (same as motion.cpp; the original never calls PSMTXCopy here).
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

int ProjType = 1;

void CameraSetProjection(int type)
{
    ProjType = type;
    switch (type) {
    case 1:
        GXSetProjection(pG->Cam.projMat, 0);
        break;
    case 2:
        GXSetProjection(pG->Cam.projMat, 1);
        break;
    }
}

void CameraCurrentProjection()
{
    CameraSetProjection(ProjType);
}

int CameraGetProjection()
{
    return ProjType;
}

void CameraGameInit()
{
    Vec at = {0.0f, 0.0f, 0.0f};
    Vec pos = {0.0f, 1000.0f, 2000.0f};

    CameraSetWithRoll(&pG->Cam, &pos, &at, 0.0f, 50.0f);
    CameraSetOrientationRoll(&pG->Cam);
    ProjType = 1;
    CameraRoomInit();
}

void CameraRoomInit()
{
    CamDbg.gain = 1.0f;
}

void CameraMove()
{
    Camera* cam = &pG->Cam;

    CamCtrl.Check();
    if (!(pG->flags_170 & 0x40000000)) {
        CamCtrl.Move();
        if ((pG->flags_500C & 0x100) && !(pG->flags_60 & 0x10000000)) {
            pG->Cam = CamCtrl.camera;
            if (CamCtrl.x250 != 0) {
                pG->Cam = *(Camera*) CamCtrl.x250;
            }
        }
        CamCtrl.x250 = 0;
        if (!(pG->flags_170 & 0x10000)) {
            QuakeMove();
        }
    }
    CamDbg.move(cam, &Joy[1], 0);
    if (cam->param.fovy == 0.0f) {
        pLog->err(0, 0, "CameraMove(): Fovy = 0.0f");
        cam->param.fovy = 50.0f;
    }
    switch (ProjType) {
    case 1:
        C_MTXPerspective(cam->projMat, cam->param.fovy, 1.3333334f, ZNEAR, ZFAR);
        break;
    case 2:
        C_MTXOrtho(cam->projMat, ORTHO_T, ORTHO_B, ORTHO_L, ORTHO_R, 0.0f, ZFAR);
        break;
    }
    cam->dist = PSVECDistance(&cam->param.pos, &cam->param.at);
    C_MTXLookAt(cam->viewMat, &cam->param.pos, &cam->up, &cam->param.at);
    View.move();
    CameraDebugInformation();
}

void CamStick2World(Camera* cam, JOY* joy, Vec* out)
{
    static Mtx mat_prev;
    static int carry_on_flag = 0;
    Vec v;

    v.x = (f32) joy->sx;
    v.y = 0.0f;
    v.z = (f32) -joy->sy;
    if (CamCtrl.IsChangeCamera()) {
        if (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f) {
            MTX_COPY(CamCtrl.prev_mat, mat_prev);
            carry_on_flag = 1;
        }
    }
    if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
        carry_on_flag = 0;
    }
    if (carry_on_flag) {
        PSMTXMultVecSR(mat_prev, &v, out);
    } else {
        PSMTXMultVecSR(cam->mat, &v, out);
    }
}

ViewFrustum* CameraViewFrustumPtr(Camera* cam)
{
    return &View.worldFull;
}

void CameraGetUpVec(Camera* cam, Vec* up)
{
    *up = cam->up;
}

void CameraGetLookVec(Camera* cam, Vec* look)
{
    *look = cam->dir;
}

void CameraGetLookVecInverse(Camera* cam, Vec* look)
{
    look->x = -cam->dir.x;
    look->y = -cam->dir.y;
    look->z = -cam->dir.z;
}

// Never called; dead-stripped from the DOL. Its constant pool (0.0f, the int->float magic
// double, -1.0f) is still in .rodata right before CamPos2ScrnVec's.
static f32 ScrnY2Ratio(int y)
{
    f32 r = 0.0f;

    if (y != 0) {
        r = (f32) y + -1.0f;
    }
    return r;
}

void CamPos2ScrnVec(Vec* out, f32 sx, f32 sy)
{
    f32 ang = pG->Cam.param.fovy;
    f32 h = 480.0f;  // first constant of the pool

    out->x = sx - Screen.width * 0.5f;
    out->y = -(sy - Screen.height * 0.5f);
    out->x *= 640.0f / Screen.width;
    ang = ang * 0.5f;
    ang = ang * PI;
    ang = ang / 180.0f;
    out->y *= h / Screen.height;
    FSet(out->z, -(cosf(ang) * 240.0f / sinf(ang)));
    PSMTXMultVecSR(pG->Cam.mat, out, out);
}
