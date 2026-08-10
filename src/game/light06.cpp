#include "light.h"

struct Light06Work {
    f32 start;  // 0x00 initial rate
    f32 speed;  // 0x04 rate change per frame (sign gives direction)
    f32 rate;   // 0x08 current brightness rate 0..1
};

// Fade light: brightness rate moves from `start` by `speed` and is clamped to [0, 1].
void Light06_Move(cLight* l)
{
    Light06Work* w = (Light06Work*)l->work;

    switch (l->x138) {
    case 0:
        w->rate = w->start;
        l->x138 = 1;
    case 1:
        w->rate += w->speed;
        if (w->speed > 0.0f) {
            if (w->rate > 1.0f) {
                w->rate = 1.0f;
            }
        } else {
            if (w->rate < 0.0f) {
                w->rate = 0.0f;
            }
        }
        l->curColor.r = (u8)(w->rate * l->color.r);
        l->curColor.g = (u8)(w->rate * l->color.g);
        l->curColor.b = (u8)(w->rate * l->color.b);
        break;
    }
}
