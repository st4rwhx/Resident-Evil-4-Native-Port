#include "light.h"
#include "math_sub.h"

struct Light02Work {
    f32 base;   // 0x00 base brightness rate
    f32 amp;    // 0x04 sine amplitude
    f32 freq;   // 0x08 cycles per second
    f32 phase;  // 0x0C
};

// Pulsing light: brightness rate = base + amp * sin(phase), clamped to [0, 255] per channel.
void Light02_Move(cLight* l)
{
    Light02Work* w = (Light02Work*)l->work;
    f32 rate;
    f32 r;
    f32 g;
    f32 b;

    if (w->freq <= 0.0f) {
        w->freq = 0.00001f;
    }
    w->phase += w->freq * 6.2831855f / 30.0f;
    w->phase = LIMIT_ANGLE(w->phase);
    rate = w->base + w->amp * sinf(w->phase);

    r = rate * l->color.r;
    if (r < 0.0f) {
        r = 0.0f;
    } else if (r > 255.0f) {
        r = 255.0f;
    }
    l->curColor.r = (u8)r;
    g = rate * l->color.g;
    if (g < 0.0f) {
        g = 0.0f;
    } else if (g > 255.0f) {
        g = 255.0f;
    }
    l->curColor.g = (u8)g;
    b = rate * l->color.b;
    if (b < 0.0f) {
        b = 0.0f;
    } else if (b > 255.0f) {
        b = 255.0f;
    }
    l->curColor.b = (u8)b;
    l->curColor.a = l->color.a;
}
