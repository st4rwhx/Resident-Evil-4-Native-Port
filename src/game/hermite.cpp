#include "types.h"
#include "hermite.h"
#include "main_mem.h"

void Hermite_1Clear(Hermite1* h)
{
    int i;

    for (i = 0; i < h->num; i++) {
        memclr_asm(&h->key[i], sizeof(HermiteKey));
    }
    h->num = 0;
}

int Hermite_1CurveRight(Hermite1* h, f32 t)
{
    if (h == NULL || h->num <= 0) {
        return 0;
    }
    if (t < h->key[0].t) {
        return 0;
    }
    if (t > h->key[h->num - 1].t) {
        return 0;
    }
    return 1;
}

int Hermite_1CurveCalc(Hermite1* h, f32 t, f32* out)
{
    if (out == NULL) {
        return 0;
    }
    if (!Hermite_1CurveRight(h, t)) {
        return 0;
    }
    *out = Hermite_1CurveCalc(h, t);
    return 1;
}

f32 Hermite_1CurveCalc(Hermite1* h, f32 t)
{
    int num = h->num;
    HermiteKey* k0 = NULL;
    HermiteKey* k1 = NULL;
    int found = 0;
    int i;
    f32 result;

    for (i = 0; i < num - 1; i++) {
        k0 = &h->key[i];
        k1 = &h->key[i + 1];
        if (t >= k0->t && t <= k1->t) {
            found = 1;
            break;
        }
    }
    if (found) {
        Hermite_1(k0, k1, t, &result);
    } else {
        result = 0.0f;
    }
    return result;
}

void Hermite_1Scale(Hermite1* h, f32 sx, f32 sy)
{
    int i;
    f32 base;

    base = h->key[0].t;
    for (i = 0; i < h->num; i++) {
        h->key[i].t = (h->key[i].t - base) * sx + base;
        h->key[i].out /= sx;
        h->key[i].in /= sx;
    }
    base = h->key[0].v;
    for (i = 0; i < h->num; i++) {
        h->key[i].v = (h->key[i].v - base) * sy + base;
        h->key[i].out *= sy;
        h->key[i].in *= sy;
    }
}

void Hermite_1Trans(Hermite1* h, f32 tx, f32 ty)
{
    int i;

    tx -= h->key[0].t;
    for (i = 0; i < h->num; i++) {
        h->key[i].t += tx;
    }
    ty -= h->key[0].v;
    for (i = 0; i < h->num; i++) {
        h->key[i].v += ty;
    }
}

void Hermite_1Reverse(Hermite1* h)
{
    int num = h->num;
    HermiteKey* tmp = (HermiteKey*) Debug_alloc(num * sizeof(HermiteKey), 1);
    f32 t0, t1;
    int i;

    for (i = 0; i < num; i++) {
        tmp[i] = h->key[i];
    }
    t0 = h->key[0].t;
    t1 = h->key[num - 1].t;
    for (i = 0; i < num; i++) {
        h->key[i].t = t1 - tmp[num - 1 - i].t + t0;
        h->key[i].v = tmp[num - 1 - i].v;
        h->key[i].out = -tmp[num - 1 - i].in;
        h->key[i].in = -tmp[num - 1 - i].out;
    }
    Debug_free(tmp);
}

void Hermite_1(HermiteKey* a, HermiteKey* b, f32 t, f32* out)
{
    f32 dt = b->t - a->t;
    f32 s = (t - a->t) / dt;
    f32 s2 = s * s;
    f32 s3 = s * s2;
    f32 h01 = -(s3 + s3) + 3.0f * s2;
    f32 h11 = s3 - s2;
    f32 h10 = h11 - s2 + s;
    f32 h00 = -h01 + 1.0f;

    *out = h00 * a->v + h01 * b->v + dt * (h10 * a->out + h11 * b->in);
}

void Hermite_1_dt(HermiteKey* a, HermiteKey* b, f32 t, f32* out)
{
    f32 dt = b->t - a->t;
    f32 s = (t - a->t) / dt;
    f32 s2 = s * s;
    f32 dh11 = 3.0f * s2 - 2.0f * s;
    f32 dh10 = 3.0f * s2 - 2.0f * s - 2.0f * s + 1.0f;
    f32 dh00 = dh10 + dh11 - 1.0f;
    f32 dh01 = -dh00;

    *out = dh00 * a->v + dh01 * b->v + dt * (dh10 * a->out + dh11 * b->in);
}
