#include "light.h"
#include "math_sub.h"

cLight07::cLight07()
{
}

// Rotating directional light: the direction is rebuilt from spinning angles every frame.
void Light07_Move(cLight* l)
{
    Vec* n = &l->normal;
    Vec* ang = (Vec*)l->work;
    Vec* spd = (Vec*)(l->work + 0xC);

    switch (l->x138) {
    case 0:
        l->curColor = l->color;
        l->x138 = 1;
    case 1:
        PSVECAdd(ang, spd, ang);
        ang->x = LIMIT_ANGLE(ang->x);
        ang->y = LIMIT_ANGLE(ang->y);
        ang->z = LIMIT_ANGLE(ang->z);
        n->x = cosf(ang->x) * sinf(ang->y) * 100000.0f;
        n->y = sinf(ang->x) * 100000.0f;
        n->z = cosf(ang->x) * cosf(ang->y) * 100000.0f;
        break;
    }
}
