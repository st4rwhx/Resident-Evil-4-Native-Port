#include "cMotBase.h"
#include "math_sub.h"

cMotBase::cMotBase()
{
    cnt = 0xFF;
    pModel = 0;
}

void cMotBase::set(cMotModel* m, MotionData* data, Vec* p, Vec* r, u8 c)
{
    pModel = m;
    basePos = *p;
    pos = basePos;
    baseRot = *r;
    rot = baseRot;
    cnt = c;
}

void cMotBase::set(cMotModel* m, Vec* p, Vec* r, u8 c)
{
    set(m, m->mot.data, p, r, c);
    m->mot.flags &= ~1;
}

// add the model's movement since the last move to the followed pose
void cMotBase::adjust()
{
    Vec d;

    if (cnt == 0xFF) {
        return;
    }
    PSVECSubtract(&pModel->pos, &basePos, &d);
    PSVECAdd(&pos, &d, &pos);
    rot.y += pModel->rot.y - baseRot.y;
}

void cMotBase::move()
{
    Vec spd;
    Vec rotSpd;

    if (cnt == 0xFF) {
        return;
    }
    MotionGetSpeed(pModel, &pModel->mot, 0, &spd, &rotSpd);
    PSMTXMultVecSR(pModel->mat, &spd, &spd);
    PSVECAdd(&pos, &spd, &pos);
    PSVECAdd(&rot, &rotSpd, &rot);
    if (cnt != 0) {
        PSVECSubtract(&pos, &pModel->pos, &spd);
        PSVECScale(&spd, &spd, 1.0f / (f32) (int) cnt);
        PSVECAdd(&pModel->pos, &spd, &pModel->pos);
        pModel->rot.y += Muku2(pModel->rot.y, rot.y, PI / (f32) (int) cnt);
        cnt--;
        if (cnt == 0) {
            pModel->mot.flags |= 1;
            cnt = 0xFF;
        }
    } else {
        pModel->pos = pos;
        pModel->rot = rot;
    }
    basePos = pModel->pos;
    baseRot = pModel->rot;
}
