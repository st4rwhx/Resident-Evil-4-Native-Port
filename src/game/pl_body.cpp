// game/pl_body.cpp: player body helper (waist twist, SPAE records, weapon hand).

#include "pl_body.h"
#include "atari.h"

cPlBody::cPlBody(cModel* model)
{
    pModel = model;
    pHeadData = 0;
    pRightData = 0;
    pLeftData = 0;
    pWepHand = 0;
    pShape = 0;
    x14 = 0;
    x18 = 0;
    pRight = 0;
    pLeft = 0;
    waist = 0.0f;
}

void cPlBody::move()
{
    waistMove();
}

void cPlBody::waistSet(f32 angle)
{
    waist = angle;
}

void cPlBody::waistMove()
{
    PlBodyParts* p;
    f32 half = waist * 0.5f;

    p = (PlBodyParts*) pModel->getPartsPtr(1);
    p->flags |= 0x40000000;
    p->rot.y = half;
    p->rot.x = 0.0f;
    p->rot.z = 0.0f;

    p = (PlBodyParts*) pModel->getPartsPtr(2);
    p->flags |= 0x40000000;
    p->rot.y = half;
    p->rot.x = 0.0f;
    p->rot.z = 0.0f;

    p = (PlBodyParts*) pModel->getPartsPtr(3);
    p->flags |= 0x40000000;
    p->rot.x = 0.0f;
    p->rot.y = -waist;
    p->rot.z = 0.0f;
}

void cPlBody::makeSpaeData()
{
    SpaeData* d = spae;
    u32 i;

    for (i = 0; i < 2; i++) {
        d->id = 0x101;
        d->type = 2;
        d->x08 = 0x18;
        d->x0C = 0;
        d->x0E = 2;
        d->x10 = 0x38;
        d->x14 = 1;
        d->x16 = 2;
        d->x18 = 0;
        d->scale.x = 1.0f;
        d->scale.y = 0.0f;
        d->scale.z = 0.0f;
        d->x28 = 0x100;
        d->x2C.x = 0.0f;
        d->x2C.y = 0.0f;
        d->x2C.z = 0.0f;
        d->x38 = 0;
        d->x3C.x = 0.0f;
        d->x3C.y = 0.0f;
        d->x3C.z = 0.0f;
        d->x48 = 0x100;
        d->x4C.x = 1.0f;
        d->x4C.y = 0.0f;
        d->x4C.z = 0.0f;
        d++;
    }
}

void cPlBody::initWepHand(u32 hand)
{
    // Unused; local static consts are still emitted (trailing 0, PI/2, 256 in .rodata).
    static const f32 hand_tbl[3] = {0.0f, PI * 0.5f, 256.0f};
    pWepHand = (void*) hand;
}
