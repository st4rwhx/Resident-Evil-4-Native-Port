// game/pl_event.cpp: player routine 0 (event control): idle, walk to a point, smooth motion change.

#include "player.h"
#include "atari.h"
#include "math_sub.h"

void Pl_R0_Event(cPlayer* pl)
{
    static void (*funcTbl[])(cPlayer*) = {
        pl_R1_Event_Normal,
        pl_R1_Event_ToWalk,
        pl_R1_Event_Smooth,
    };

    funcTbl[pl->xFD](pl);
}

void pl_R1_Event_Normal(cPlayer* pl)
{
    if (pl->xFE == 0) {
        pl->xFE = 1;
    }
    if (pl->motionMove()) {
        if (pl->flags_41C & 0x100) {
            pl->flags_41C &= ~0x100;
            pl->xFC = 0;
            pl->xFD = 0;
            pl->xFE = 0;
            pl->xFF = 0;
        }
    }
}

void pl_R1_Event_ToWalk(cPlayer* pl)
{
    f32 ang;

    switch (pl->xFE) {
    case 0:
        ang = Muku(&pl->pos, &pl->evTarget, pl->rot.y, PI * 2.0f);
        if (fabsf(ang) > PI / 3.0f) {
            pl->motionSet(pl->pMotTbl[2], 5, 0, 4, 0);
            pl->xFE = 1;
            break;
        }
        goto set_walk;
    case 1:
        ang = Muku(&pl->pos, &pl->evTarget, pl->rot.y, pl->x400);
        pl->rot.y += ang;
        if (fabsf(ang) < pl->x400 * 0.5f) {
        set_walk:
            pl->motionSet(pl->pMotTbl[2], 5, 0, 5, 0);
            pl->xFE = 2;
        }
        break;
    case 2:
        ang = Muku(&pl->pos, &pl->evTarget, pl->rot.y, pl->x400);
        pl->rot.y += ang;
        if (GetDistance(&pl->pos, &pl->evTarget) < 10000.0f) {
            pl->motionSet(pl->pMotTbl[0], 5, 0, 1, 0);
            pl->x3E0 = 1;
            pl->xFE = 3;
        }
        break;
    }
    pl->motionMove();
}

void pl_R1_Event_Smooth(cPlayer* pl)
{
    switch (pl->xFE) {
    case 0:
        if (pl->motionMove()) {
            pl->xFE = 2;
        }
        break;
    case 2:
        pl->setFootwork();
        pl->motionMove();
        pl->xFE = 3;
    case 3:
        pl->motionMove();
        break;
    }
}
