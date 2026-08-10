#include "types.h"
#include "vec.h"
#include "camera.h"
#include "cam_extra.h"
#include "cam_motion.h"
#include "main_mem.h"

extern "C" {
void* memset(void* dst, int c, unsigned int n);
}

#define PI 3.1415927f

CameraMotion::CameraMotion(void* data, int hokan, int flags, f32 frame)
{
    CameraMotionWork* w = &info;
    u32* tbl;
    int i;

    memclr_asm(w, sizeof(CameraMotionWork));
    w->data = (MotionData*) data;
    w->maxFrame = (f32) (((MotionData*) data)->maxFrame & 0x3FFF);
    w->maxFrame += 1.0f;
    w->nParts = w->data->nParts;
    w->partsInfo = (u16*) ((u8*) w->data + 3);
    w->partsNo = (u8*) w->data + (w->nParts * 2 + 3);
    tbl = (u32*) ((u32) w->partsNo + w->nParts);
    tbl = (u32*) (((u32) tbl + 3) & ~3);
    tbl++;
    if ((s32) tbl[0] >= 0) {
        for (i = 0; i < w->nParts; i++) {
            tbl[i] += (u32) w->data;
        }
    }
    w->keyTbl = tbl;
    for (i = 0; i < w->nParts; i++) {
        w->hist[i][0] = w->hist[i][1] = w->hist[i][2] = 0;
    }
    w->hokan = hokan;
    w->flags = flags;
    w->frame = frame;
    w->state = 0;
    base_mat = NULL;
    end = 0;
}

CameraMotion::~CameraMotion()
{
    memset(this, 9, 0x200);
}

void CameraMotion::move()
{
    HermitePrm prm;
    Vec pos;
    Vec at;
    Vec roll = {0.0f, 0.0f, 0.0f};
    Vec fov;
    HermitePrm* pp = &prm;
    CameraMotionWork* w = &info;
    int i;

    pp->frame = w->frame;
    pp->maxFrame = w->maxFrame;
    pp->flags = 2;
    for (i = 0; i < w->nParts; i++) {
        pp->type = w->partsInfo[i] >> 12;
        pp->key = (u8*) w->keyTbl[i];
        switch (w->partsNo[i]) {
        case 0:
            HermiteInterpolation(pp, &pos, w->hist[i]);
            break;
        case 1:
            HermiteInterpolation(pp, &at, w->hist[i]);
            break;
        case 2:
            HermiteInterpolation(pp, &roll, w->hist[i]);
            break;
        case 3:
            HermiteInterpolation(pp, &fov, w->hist[i]);
            break;
        }
    }
    param.pos = pos;
    param.at = at;
    param.roll = roll.y;
    param.fovy = fov.y * 180.0f / PI;
    CameraSetOrientationRoll(this);
    if (base_mat) {
        PSMTXMultVec(*base_mat, &param.pos, &param.pos);
        PSMTXMultVec(*base_mat, &param.at, &param.at);
        PSMTXMultVec(*base_mat, &up, &up);
        CameraSetOrientationUp(this);
    }
    end = 0;
    if (CameraSequenceCtrl(&info) == 4) {
        end = 1;
    }
}

static f32 rad2deg(f32 r)
{
    return r * 180.0f / PI;
}

u32 CameraSequenceCtrl(CameraMotionWork* w)
{
    if (!(w->flags & 8)) {
        if (w->frame >= w->maxFrame) {
            if (w->flags & 4) {
                w->state = 1;
                w->frame = 0.0f;
            } else {
                w->state = 4;
            }
        } else {
            w->frame += 1.0f;
        }
    }
    return w->state;
}
