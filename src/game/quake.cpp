#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "quake.h"

extern "C" void* memset(void* dst, int c, unsigned int n);

QuakeWork Quake;
static Vec QuakeOfsOld[2];

void QuakeMove()
{
    QuakeScheduler();
    if (Quake.active) {
        QuakeMain();
    }
}

void QuakeInit()
{
    int i;
    QuakeEntry* e = Quake.ent;

    for (i = 0; i < 16; i++, e++) {
        e->active = 0;
        e->id = 0;
        e->delay = 0;
        e->time = 0;
        e->power = 0.0f;
        e->axis = 0;
    }
    Quake.rnd_idx = 0;
}

void QuakeExec(u8 id, u16 delay, s16 time, f32 power, u8 axis)
{
    int i;
    QuakeEntry* e = Quake.ent;

    for (i = 0; i < 16; i++, e++) {
        if (!(e->active & 1)) {
            e->active = 1;
            e->id = id;
            e->delay = delay;
            e->time = time;
            e->power = power;
            e->axis = axis;
            break;
        }
    }
}

static void QuakeKill(u8 id)
{
    int i;
    QuakeEntry* e = Quake.ent;

    for (i = 0; i < 16; i++, e++) {
        if ((e->active & 1) && e->id == id) {
            e->active = 0;
            e->id = 0;
            e->delay = 0;
            e->time = 0;
            e->power = 0.0f;
        }
    }
}

void QuakeScheduler()
{
    int i;
    QuakeEntry* e;

    Quake.active = 0;
    Quake.axis = 0;
    Quake.power = 0.0f;
    e = Quake.ent;
    for (i = 0; i < 16; i++, e++) {
        if (e->active & 1) {
            if (e->delay != 0) {
                e->delay--;
            } else if (e->time == 0) {
                e->active = 0;
                e->id = 0;
                e->delay = 0;
                e->time = 0;
                e->power = 0.0f;
                e->axis = 0;
            } else {
                Quake.active = 1;
                if (Quake.power < e->power) {
                    Quake.power = e->power;
                    Quake.axis |= e->axis;
                }
                e->time--;
            }
        }
    }
}

void QuakeMain()
{
    static s8 rnd_tbl[16] = {0, -1, 1, 2, -1, 0, 1, -1, 1, -1, 0, 1, -1, -2, 0, 1};
    GlobalWork* g = pG;
    Camera* cam = &g->Cam;
    Vec ofs = {0.0f, 0.0f, 0.0f};

    if (Quake.axis & 1) {
        Quake.rnd_idx = (Quake.rnd_idx + 1) & 0xF;
        ofs.x = (f32) rnd_tbl[Quake.rnd_idx] * Quake.power;
    }
    if (Quake.axis & 2) {
        Quake.rnd_idx = (Quake.rnd_idx + 1) & 0xF;
        ofs.y = (f32) rnd_tbl[Quake.rnd_idx] * Quake.power;
    }
    if (Quake.axis & 4) {
        Quake.rnd_idx = (Quake.rnd_idx + 1) & 0xF;
        ofs.z = (f32) rnd_tbl[Quake.rnd_idx] * Quake.power;
    }
    PSMTXMultVecSR(cam->mat, &ofs, &ofs);
    PSVECAdd(&g->Cam.param.pos, &ofs, &g->Cam.param.pos);
    PSVECAdd(&g->Cam.param.at, &ofs, &g->Cam.param.at);
    CameraSetOrientationUp(cam);
}
