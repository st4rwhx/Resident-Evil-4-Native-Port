#include "light.h"

struct Light10Work {
    u8 wait;  // 0x00 frames before the fade starts
};

// Fade-out light: waits, then decays power by 30% a frame and dies below 0.1.
void Light10_Move(cLight* l)
{
    Light10Work* w = (Light10Work*)l->work;

    if (w->wait != 0) {
        w->wait--;
        return;
    }
    l->power *= 0.3f;
    if (l->power < 0.1f) {
        l->power = 0.0f;
        delete l;
    }
}
