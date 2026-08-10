#ifndef HERMITE_H
#define HERMITE_H

#include "types.h"

// game/hermite.cpp: 1-D cubic Hermite curve (C linkage).
struct HermiteKey {
    f32 t;    // 0x00  key time
    f32 v;    // 0x04  value
    f32 out;  // 0x08  tangent leaving this key
    f32 in;   // 0x0C  tangent arriving at this key
};

struct Hermite1 {
    s32 num;             // 0x00
    HermiteKey key[1];   // 0x04  num entries
};

extern "C" {
void Hermite_1Clear(Hermite1* h);
int Hermite_1CurveRight(Hermite1* h, f32 t);
int Hermite_1CurveCalc(Hermite1* h, f32 t, f32* out);
void Hermite_1Scale(Hermite1* h, f32 sx, f32 sy);
void Hermite_1Trans(Hermite1* h, f32 tx, f32 ty);
void Hermite_1Reverse(Hermite1* h);
void Hermite_1(HermiteKey* a, HermiteKey* b, f32 t, f32* out);
void Hermite_1_dt(HermiteKey* a, HermiteKey* b, f32 t, f32* out);
}

// C++ overload (Hermite_1CurveCalc__FP8Hermite1f): evaluate the curve, 0.0f when t is outside.
f32 Hermite_1CurveCalc(Hermite1* h, f32 t);

#endif
