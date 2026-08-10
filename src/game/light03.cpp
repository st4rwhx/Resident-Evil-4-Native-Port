#include "light.h"
#include "math_sub.h"

// Directional light whose direction is rotated by the work angles every frame.
void Light03_Move(cLight* l)
{
    Vec* n = &l->normal;
    Vec* rot = (Vec*)l->work;
    Mtx m;

    if (PSVECMag(n) < 0.9f) {
        n->x = 0.0f;
        n->y = 0.0f;
        n->z = 1.0f;
    }
    RotMatrix(m, rot);
    PSMTXMultVec(m, n, n);
#line 48 "D:/Bio4/Prog/light03.cpp"
    VECNormalize(n, n);
    l->curColor = l->color;
}
