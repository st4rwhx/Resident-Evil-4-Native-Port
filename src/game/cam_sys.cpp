#include "types.h"
#include "vec.h"
#include "camera.h"
#include "db_log.h"
#include "math_sub.h"

// Column vectors -> matrix.
#define MTX_SET_COLUMNS(m, c0, c1, c2, c3)                                                    \
    (m)[0][0] = (c0)->x; (m)[1][0] = (c0)->y; (m)[2][0] = (c0)->z;                            \
    (m)[0][1] = (c1)->x; (m)[1][1] = (c1)->y; (m)[2][1] = (c1)->z;                            \
    (m)[0][2] = (c2)->x; (m)[1][2] = (c2)->y; (m)[2][2] = (c2)->z;                            \
    (m)[0][3] = (c3)->x; (m)[1][3] = (c3)->y; (m)[2][3] = (c3)->z

static inline void getColumn(Mtx m, int c, Vec* v)
{
    v->x = m[0][c];
    v->y = m[1][c];
    v->z = m[2][c];
}

static inline void setColumns(Mtx m, Vec* c0, Vec* c1, Vec* c2, Vec* c3)
{
    MTX_SET_COLUMNS(m, c0, c1, c2, c3);
}

void CameraSetOrientationUp(Camera* cam)
{
    PSVECSubtract(&cam->param.pos, &cam->param.at, &cam->dir);
#line 33 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&cam->dir, &cam->dir);
    PSVECCrossProduct(&cam->up, &cam->dir, &cam->right);
#line 37 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&cam->right, &cam->right);
    PSVECCrossProduct(&cam->dir, &cam->right, &cam->up);
    setColumns(cam->mat, &cam->right, &cam->up, &cam->dir, &cam->param.pos);
}

void CameraSetOrientationRoll(Camera* cam)
{
    Vec right;
    Vec up = {0.0f, 1.0f, 0.0f};
    Vec dir;
    Mtx m;

    PSVECSubtract(&cam->param.pos, &cam->param.at, &dir);
    if (dir.x != 0.0f || dir.z != 0.0f) {
        PSVECCrossProduct(&up, &dir, &right);
    } else {
        getColumn(cam->mat, 0, &right);
        if (PSVECMag(&right) == 0.0f) {
            right.x = 1.0f;
            right.y = 0.0f;
            right.z = 0.0f;
        }
    }
    PSVECCrossProduct(&dir, &right, &up);
    PSMTXRotAxisRad(m, &dir, cam->param.roll);
    PSMTXMultVec(m, &right, &right);
    PSMTXMultVec(m, &up, &up);
    if (right.x != 0.0f || right.y != 0.0f || right.z != 0.0f) {
#line 91 "D:/Bio4/Prog/cam_sys.cpp"
        VECNormalize(&right, &right);
    }
    if (up.x != 0.0f || up.y != 0.0f || up.z != 0.0f) {
#line 92 "D:/Bio4/Prog/cam_sys.cpp"
        VECNormalize(&up, &up);
    }
    if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f) {
#line 93 "D:/Bio4/Prog/cam_sys.cpp"
        VECNormalize(&dir, &dir);
    }
    setColumns(cam->mat, &right, &up, &dir, &cam->param.pos);
    cam->up = up;
    cam->dir = dir;
    cam->right = right;
}

void CameraSetOrientationZeroRoll(Camera* cam)
{
    Vec right;
    Vec up = {0.0f, 1.0f, 0.0f};
    Vec dir;

    PSVECSubtract(&cam->param.pos, &cam->param.at, &dir);
    if (dir.x != 0.0f || dir.z != 0.0f) {
        PSVECCrossProduct(&up, &dir, &right);
    } else {
        getColumn(cam->mat, 0, &right);
        if (PSVECMag(&right) == 0.0f) {
            right.x = 1.0f;
            right.y = 0.0f;
            right.z = 0.0f;
        }
    }
    PSVECCrossProduct(&dir, &right, &up);
#line 149 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&right, &right);
#line 150 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&up, &up);
#line 151 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&dir, &dir);
    setColumns(cam->mat, &right, &up, &dir, &cam->param.pos);
    cam->up = up;
    cam->dir = dir;
    cam->right = right;
}

f32 CameraGetRoll(Camera* cam)
{
    Vec v = {1.0f, 0.0f, 0.0f};
    Camera tmp;
    Mtx inv;

    tmp = *cam;
    CameraSetOrientationZeroRoll(&tmp);
    PSMTXInverse(tmp.mat, inv);
    PSMTXMultVecSR(inv, &v, &v);
    PSMTXMultVecSR(cam->mat, &v, &v);
    return atan2f(v.y, v.x);
}

void CameraRotAxisPosRad(Camera* cam, Vec* axis, Vec* pos, f32 rad)
{
    Mtx m;

    MtxRotAxisPosRad(m, axis, pos, rad);
    PSMTXMultVec(m, &cam->param.at, &cam->param.at);
    PSMTXMultVec(m, &cam->param.pos, &cam->param.pos);
    PSMTXMultVecSR(m, &cam->up, &cam->up);
    CameraSetOrientationUp(cam);
    cam->param.roll = CameraGetRoll(cam);
}

void CameraTargetRot(Camera* cam, char axis, f32 rad)
{
    Vec v;

    switch (axis) {
    case 'X':
    case 'x':
        getColumn(cam->mat, 0, &v);
        break;
    case 'Y':
    case 'y':
        getColumn(cam->mat, 1, &v);
        break;
    case 'Z':
    case 'z':
        getColumn(cam->mat, 2, &v);
        break;
    }
    CameraRotAxisPosRad(cam, &v, &cam->param.pos, rad);
}

void CameraCamposRot(Camera* cam, char axis, f32 rad)
{
    Vec v;

    switch (axis) {
    case 'X':
    case 'x':
        getColumn(cam->mat, 0, &v);
        break;
    case 'Y':
    case 'y':
        getColumn(cam->mat, 1, &v);
        break;
    case 'Z':
    case 'z':
        getColumn(cam->mat, 2, &v);
        break;
    }
    CameraRotAxisPosRad(cam, &v, &cam->param.at, rad);
}

void CameraDolly(Camera* cam, Vec* d)
{
    PSVECAdd(&cam->param.pos, d, &cam->param.pos);
    PSVECAdd(&cam->param.at, d, &cam->param.at);
    CameraSetOrientationUp(cam);
}

void CameraTargetDistance(Camera* cam, f32 dist)
{
    Vec v;

    getColumn(cam->mat, 2, &v);
    PSVECScale(&v, &v, dist);
    PSVECSubtract(&cam->param.pos, &v, &cam->param.at);
    cam->dist = dist;
    CameraSetOrientationUp(cam);
}

void CameraCamposDistance(Camera* cam, f32 dist)
{
    Vec v;

    getColumn(cam->mat, 2, &v);
    PSVECScale(&v, &v, dist);
    PSVECAdd(&cam->param.at, &v, &cam->param.pos);
    cam->dist = dist;
    CameraSetOrientationUp(cam);
}

void CameraSetWithRoll(Camera* cam, Vec* pos, Vec* at, f32 roll, f32 fovy)
{
    cam->param.pos = *pos;
    cam->param.at = *at;
    cam->param.roll = roll;
    cam->param.fovy = fovy;
    CameraSetOrientationRoll(cam);
}

// Dead-stripped by the original linker (STRIP_UNUSED); only its DF 0.0 pool remains in .rodata.
static int cam_sys_unused(f64 x)
{
    return x != 0.0;
}
