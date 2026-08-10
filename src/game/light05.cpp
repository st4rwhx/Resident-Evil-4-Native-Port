#include "light.h"

struct Light05Work {
    u8 pad_0[0xC];
    u8 pathNo;   // 0x0C light path data number
    u8 pathIdx;  // 0x0D path inside the data
};

// Path light: follows a light path; the path returns the brightness (0..200) or 0xFF at the end.
void Light05_Move(cLight* l)
{
    cLightPath* path = (cLightPath*)l->work;
    Light05Work* w = (Light05Work*)l->work;
    int v;
    f32 rate;

    switch (l->x138) {
    case 0:
        path->setPath(LightMgr.getPathPtr(w->pathNo), w->pathIdx);
        l->x138 = 1;
    case 1:
        v = path->movePath();
        if (v == 0xff) {
            LightMgr.destroy(l);
            return;
        }
        rate = (f32)v * 0.005f;
        l->curColor.r = (u8)(rate * l->color.r);
        l->curColor.g = (u8)(rate * l->color.g);
        l->curColor.b = (u8)(rate * l->color.b);
        l->curColor.a = (u8)(rate * l->color.a);
        break;
    }
}
