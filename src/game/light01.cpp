#include "light.h"
#include "rnd.h"

struct Light01Work {
    u8 pad_0[4];
    s8 range;  // 0x04 random brightness range (+-)
};

cLight01::cLight01()
{
}

// Flicker light: adds a random offset in [-range, range) to every color channel.
void Light01_Move(cLight* l)
{
    Light01Work* w = (Light01Work*)l->work;
    int r;
    int c;

    if (w->range != 0) {
        r = Rnd() % (w->range + w->range) - w->range;
    } else {
        r = 0;
    }
    c = l->color.r + r;
    if (c < 0) {
        c = 0;
    } else if (c > 255) {
        c = 255;
    }
    l->curColor.r = c;
    c = l->color.g + r;
    if (c < 0) {
        c = 0;
    } else if (c > 255) {
        c = 255;
    }
    l->curColor.g = c;
    c = l->color.b + r;
    if (c < 0) {
        c = 0;
    } else if (c > 255) {
        c = 255;
    }
    l->curColor.b = c;
    l->curColor.a = l->color.a;
}
