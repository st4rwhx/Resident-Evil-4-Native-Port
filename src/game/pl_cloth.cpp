// game/pl_cloth.cpp: player cloth simulation setup: hair / jacket / holster (Leon), hair / skirt /
// sweater / ribbon (Ashley), hair (Luis), dress / hair / ribbon (Ada). The chain tables give each
// link its model part and neighbours; PenCloth* (pendulum.cpp) does the simulation.

#include "event.h"
#include "atari.h"
#include "pl_cloth.h"
#include "pendulum.h"
#include "model.h"
#include "global.h"
#include "math_sub.h"

extern "C" {
void PenClothMove(cModel* m, PenCloth* c);   // game/pendulum.cpp
f32 sqrtf(f32 x);
f32 asinf(f32 x);
}

PlCloth leonHair;
PlCloth leonJacket;
PlCloth leonHolster;
PlCloth girlHair;
PlCloth girlSkirt;
PlCloth girlSweater;
PlCloth luisHair;
PlCloth adaDress;
PlCloth adaHair;
PlCloth adaRibbon;

void testHairSetLeon(cModel* pl, PlCloth* c);
void testHairMoveLeon(cModel* pl, PlCloth* c);
void testJacketSetLeon(cModel* pl, PlCloth* c);
void testJacketMoveLeon(cModel* pl, PlCloth* c);
void testHolsterSetLeon(cModel* pl, PlCloth* c);
void testHolsterMoveLeon(cModel* pl, PlCloth* c);
void testHairSetGirl(cModel* pl, PlCloth* c, int evt);
void testHairMoveGirl(cModel* pl, PlCloth* c);
void testSkirtSetGirl(cModel* pl, PlCloth* c, int evt);
void testSkirtMoveGirl(cModel* pl, PlCloth* c);
void testSweaterSetGirl(cModel* pl, PlCloth* c);
void testSweaterMoveGirl(cModel* pl, PlCloth* c);
void testRibbonSetGirl(cModel* pl, PlCloth* c);
void testRibbonMoveGirl(cModel* pl, PlCloth* c);
void girlLapelMove(cModel* pl);
void testHairSetLuis(cModel* pl, PlCloth* c);
void testHairMoveLuis(cModel* pl, PlCloth* c);
void testDressSetAda(cModel* pl, PlCloth* c, int evt);
void testDressMoveAda(cModel* pl, PlCloth* c);
void testHairSetAda(cModel* pl, PlCloth* c);
void testHairMoveAda(cModel* pl, PlCloth* c);

// leonHair
u8 leonHairP[21] = {65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85};
u8 leonHairUp[21] = {0xFF, 65, 66, 0xFF, 68, 0xFF, 70, 0xFF, 72, 0xFF, 74, 0xFF, 76, 0xFF, 78, 0xFF, 80, 0xFF, 82, 0xFF, 84};
u8 leonHairDp[21] = {66, 67, 0xFF, 69, 0xFF, 71, 0xFF, 73, 0xFF, 75, 0xFF, 77, 0xFF, 79, 0xFF, 81, 0xFF, 83, 0xFF, 85, 0xFF};
f32 leonHairMax[21] = {0.4f, 0.5f, 0.6f, 0.4f, 0.5f, 0.4f, 0.5f, 0.3f, 0.3f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.3f, 0.3f, 0.4f, 0.5f, 0.4f, 0.5f};
f32 leonHairWindS[21] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.5f, 1.5f, 2.0f, 2.0f, 2.5f, 2.5f, 3.0f, 3.0f, -2.5f, -2.5f, -2.0f, -2.0f, -1.5f, -1.5f, 0.0f, 0.0f};
f32 leonHairWindR[21] = {0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
PlClothAt leonHairAt[4] = {
    {0x0000, 0x04, 0x04, 1.0f, 90.0f, {0.0f, 90.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 85.0f, {0.0f, 90.0f, 50.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 90.0f, {0.0f, 100.0f, 25.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 75.0f, {0.0f, 65.0f, 50.0f}, {0.0f, 0.0f, 0.0f}},
};

// leonJacket
u8 leonJacketP[24] = {86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109};
u8 leonJacketLp[24] = {0xFF, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 108, 109, 0xFF, 0xFF, 0xFF};
u8 leonJacketRp[24] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
u8 leonJacketUp[24] = {0xFF, 86, 87, 0xFF, 89, 0xFF, 91, 0xFF, 93, 0xFF, 95, 0xFF, 97, 0xFF, 99, 0xFF, 101, 0xFF, 103, 0xFF, 105, 0xFF, 107, 108};
u8 leonJacketDp[24] = {87, 88, 0xFF, 90, 0xFF, 92, 0xFF, 94, 0xFF, 96, 0xFF, 98, 0xFF, 100, 0xFF, 102, 0xFF, 104, 0xFF, 106, 0xFF, 108, 109, 0xFF};
f32 leonJacketMax[24] = {0.1f, 0.2f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.2f, 0.3f};
f32 leonJacketWindS[24] = {0.0f, 0.0f, 0.0f, 0.4f, 0.4f, 0.9f, 0.9f, 1.2f, 1.2f, 1.5f, 1.5f, 1.7f, 1.7f, 1.9f, 1.9f, 2.1f, 2.1f, 2.4f, 2.4f, 2.8f, 2.8f, 3.1f, 3.1f, 3.1f};
f32 leonJacketWindR[24] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
PlClothAt leonJacketAt[6] = {
    {0x0000, 0x02, 0x02, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 130.0f, {0.0f, -50.0f, 20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x11, 0.5f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 140.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.4f, 125.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x16, 0.4f, 125.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};

// leonHolster
u8 leonHolsterP[2] = {29, 30};
u8 leonHolsterLp[2] = {0xFF, 0xFF};
u8 leonHolsterRp[2] = {0xFF, 0xFF};
u8 leonHolsterUp[2] = {0xFF, 0xFF};
u8 leonHolsterDp[2] = {0xFF, 0xFF};
f32 leonHolsterMax[2] = {0.1f, 0.1f};
f32 leonHolsterWindS[2] = {0.0f, 1.5f};
f32 leonHolsterWindR[2] = {0.1f, 0.1f};
PlClothAt leonHolsterAt[1] = {
    {0x0000, 0x02, 0x02, 1.0f, 90.0f, {0.0f, 50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};

// girlHair
u8 girlHairP[21] = {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84};
u8 girlHairUp[21] = {0xFF, 64, 0xFF, 66, 0xFF, 68, 0xFF, 70, 71, 0xFF, 73, 74, 0xFF, 76, 77, 0xFF, 79, 80, 0xFF, 82, 83};
u8 girlHairDp[21] = {65, 0xFF, 67, 0xFF, 69, 0xFF, 71, 72, 0xFF, 74, 75, 0xFF, 77, 78, 0xFF, 80, 81, 0xFF, 83, 84, 0xFF};
f32 girlHairMax[21] = {0.4f, 0.5f, 0.4f, 0.5f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f};
f32 girlHairMaxEvt[21] = {0.4f, 0.5f, 0.1f, 0.3f, 0.4f, 0.5f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
f32 girlHairWindS[21] = {0.0f, 0.2f, 1.0f, 1.2f, 0.0f, 0.2f, 1.0f, 1.2f, 1.4f, 0.0f, 0.2f, 0.4f, 1.0f, 1.2f, 1.4f, 0.0f, 0.2f, 0.4f, 1.0f, 1.2f, 1.4f};
f32 girlHairWindR[21] = {0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f};
PlClothAt girlHairAt[5] = {
    {0x0000, 0x04, 0x04, 1.0f, 72.0f, {0.0f, 50.0f, -10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x04, 0.5f, 75.0f, {0.0f, 60.0f, 0.0f}, {0.0f, 60.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 100.0f, {0.0f, -20.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {-3.0f, 100.0f, 42.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 68.0f, {-3.0f, 60.0f, 45.0f}, {0.0f, 0.0f, 0.0f}},
};
PlClothAt girlHairAtEvt[7] = {
    {0x0000, 0x04, 0x04, 1.0f, 65.0f, {0.0f, 50.0f, -10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x04, 0.5f, 80.0f, {0.0f, 50.0f, 0.0f}, {0.0f, 50.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 90.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 65.0f, {0.0f, 100.0f, 35.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 60.0f, {0.0f, 70.0f, 50.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.3f, 100.0f, {-50.0f, 0.0f, -10.0f}, {-50.0f, 0.0f, -10.0f}},
    {0x0000, 0x02, 0x03, 0.3f, 100.0f, {50.0f, 0.0f, -10.0f}, {50.0f, 0.0f, -10.0f}},
};

// girlSkirt
u8 girlSkirtP[48] = {93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140};
u8 girlSkirtLp[48] = {96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 93, 94, 95};
u8 girlSkirtUp[48] = {0xFF, 93, 94, 0xFF, 96, 97, 0xFF, 99, 100, 0xFF, 102, 103, 0xFF, 105, 106, 0xFF, 108, 109, 0xFF, 111, 112, 0xFF, 114, 115, 0xFF, 117, 118, 0xFF, 120, 121, 0xFF, 123, 124, 0xFF, 126, 127, 0xFF, 129, 130, 0xFF, 132, 133, 0xFF, 135, 136, 0xFF, 138, 139};
u8 girlSkirtDp[48] = {94, 95, 0xFF, 97, 98, 0xFF, 100, 101, 0xFF, 103, 104, 0xFF, 106, 107, 0xFF, 109, 110, 0xFF, 112, 113, 0xFF, 115, 116, 0xFF, 118, 119, 0xFF, 121, 122, 0xFF, 124, 125, 0xFF, 127, 128, 0xFF, 130, 131, 0xFF, 133, 134, 0xFF, 136, 137, 0xFF, 139, 140, 0xFF};
f32 girlSkirtMax[48] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
f32 girlSkirtMaxEvt[48] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
f32 girlSkirtWindS[48] = {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.2f, 1.2f, 1.2f, 1.5f, 1.5f, 1.5f, 1.8f, 1.8f, 1.8f, 2.0f, 2.0f, 2.0f, 2.2f, 2.2f, 2.2f, 2.5f, 2.5f, 2.5f, 2.7f, 2.7f, 2.7f, 3.0f, 3.0f, 3.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 1.5f, 2.0f, 2.0f, 2.0f};
f32 girlSkirtWindR[48] = {0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f};
PlClothAt girlSkirtAt[11] = {
    {0x0000, 0x12, 0x12, 1.0f, 80.0f, {10.0f, -50.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 80.0f, {10.0f, -100.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 80.0f, {10.0f, -150.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 80.0f, {10.0f, -200.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 80.0f, {-10.0f, -50.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 80.0f, {-10.0f, -100.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 80.0f, {-10.0f, -150.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 80.0f, {-10.0f, -200.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 80.0f, {0.0f, -50.0f, -20.0f}, {0.0f, -50.0f, -20.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 80.0f, {0.0f, -100.0f, -20.0f}, {0.0f, -100.0f, -20.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 85.0f, {0.0f, -150.0f, -20.0f}, {0.0f, -150.0f, -20.0f}},
};
PlClothAt girlSkirtAtEvt[11] = {
    {0x0000, 0x12, 0x12, 1.0f, 90.0f, {10.0f, -50.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 90.0f, {10.0f, -100.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 90.0f, {10.0f, -150.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 90.0f, {10.0f, -200.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 90.0f, {-10.0f, -50.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 90.0f, {-10.0f, -100.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 90.0f, {-10.0f, -150.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 90.0f, {-10.0f, -200.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 80.0f, {0.0f, -50.0f, -20.0f}, {0.0f, -50.0f, -20.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 80.0f, {0.0f, -100.0f, -20.0f}, {0.0f, -100.0f, -20.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 85.0f, {0.0f, -150.0f, -20.0f}, {0.0f, -150.0f, -20.0f}},
};

// girlSweater
u8 girlSweaterP[8] = {85, 86, 87, 88, 89, 90, 91, 92};
u8 girlSweaterLp[8] = {87, 88, 89, 90, 91, 92, 0xFF, 0xFF};
u8 girlSweaterUp[8] = {0xFF, 85, 0xFF, 87, 0xFF, 89, 0xFF, 91};
u8 girlSweaterDp[8] = {86, 0xFF, 88, 0xFF, 90, 0xFF, 92, 0xFF};
f32 girlSweaterMax[8] = {0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f};
f32 girlSweaterWindS[8] = {0.0f, 0.0f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f};
f32 girlSweaterWindR[8] = {0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
PlClothAt girlSweaterAt[6] = {
    {0x0000, 0x02, 0x02, 1.0f, 100.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 100.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.75f, 100.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.75f, 100.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.5f, 100.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.5f, 100.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
};

// girlRibbon
u8 girlRibbonP[12] = {144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155};
u8 girlRibbonUp[12] = {0xFF, 144, 0xFF, 146, 0xFF, 148, 0xFF, 150, 0xFF, 152, 0xFF, 154};
u8 girlRibbonDp[12] = {145, 0xFF, 147, 0xFF, 149, 0xFF, 151, 0xFF, 153, 0xFF, 155, 0xFF};
f32 girlRibbonMax[12] = {0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f};
f32 girlRibbonWindS[12] = {0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f, 1.5f, 1.5f, 2.0f, 2.0f, 2.5f, 2.5f};
f32 girlRibbonWindR[12] = {0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
PlClothAt girlRibbonAt[8] = {
    {0x0000, 0x02, 0x02, 1.0f, 110.0f, {-30.0f, 0.0f, 0.0f}, {-30.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 110.0f, {30.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.75f, 110.0f, {-30.0f, 0.0f, 0.0f}, {-30.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.75f, 110.0f, {30.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.5f, 110.0f, {-30.0f, 0.0f, 0.0f}, {-30.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.5f, 110.0f, {30.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.2f, 140.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x16, 0.2f, 140.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
};

// luisHair
u8 luisHairP[26] = {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89};
u8 luisHairUp[26] = {0xFF, 64, 0xFF, 66, 67, 0xFF, 69, 70, 0xFF, 72, 0xFF, 74, 0xFF, 76, 0xFF, 78, 0xFF, 80, 0xFF, 82, 83, 0xFF, 85, 86, 0xFF, 88};
u8 luisHairDp[26] = {65, 0xFF, 67, 68, 0xFF, 70, 71, 0xFF, 73, 0xFF, 75, 0xFF, 77, 0xFF, 79, 0xFF, 81, 0xFF, 83, 84, 0xFF, 86, 87, 0xFF, 89, 0xFF};
f32 luisHairMax[26] = {0.2f, 0.3f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f};
f32 luisHairWindS[26] = {0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 1.5f, 2.0f, 2.0f, 2.5f, 2.5f, 3.0f, 3.0f, -2.5f, -2.5f, -2.0f, -2.0f, -1.5f, -1.5f, -1.5f, -1.0f, -1.0f, -1.0f, -0.5f, -0.5f};
f32 luisHairWindR[26] = {0.5f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f};
PlClothAt luisHairAt[7] = {
    {0x0000, 0x04, 0x04, 1.0f, 75.0f, {0.0f, 50.0f, 8.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 90.0f, 75.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 75.0f, {0.0f, 70.0f, 35.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 65.0f, 80.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 35.0f, 80.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 20.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 90.0f, {0.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
};

// adaDress
u8 adaDressP[146] = {73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218};
u8 adaDressUp[146] = {0xFF, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 0xFF, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 0xFF, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 0xFF, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 0xFF, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 0xFF, 128, 129, 130, 131, 132, 133, 134, 135, 136, 0xFF, 138, 139, 140, 141, 142, 143, 144, 145, 146, 0xFF, 148, 149, 150, 151, 152, 153, 154, 155, 156, 0xFF, 158, 159, 160, 161, 162, 163, 164, 165, 166, 0xFF, 168, 169, 170, 171, 172, 173, 174, 175, 176, 0xFF, 178, 179, 180, 181, 182, 183, 184, 185, 186, 0xFF, 188, 189, 190, 191, 192, 193, 194, 195, 196, 0xFF, 198, 199, 200, 201, 202, 203, 204, 205, 206, 0xFF, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217};
u8 adaDressDp[146] = {74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 0xFF, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 0xFF, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 0xFF, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 0xFF, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0xFF, 129, 130, 131, 132, 133, 134, 135, 136, 137, 0xFF, 139, 140, 141, 142, 143, 144, 145, 146, 147, 0xFF, 149, 150, 151, 152, 153, 154, 155, 156, 157, 0xFF, 159, 160, 161, 162, 163, 164, 165, 166, 167, 0xFF, 169, 170, 171, 172, 173, 174, 175, 176, 177, 0xFF, 179, 180, 181, 182, 183, 184, 185, 186, 187, 0xFF, 189, 190, 191, 192, 193, 194, 195, 196, 197, 0xFF, 199, 200, 201, 202, 203, 204, 205, 206, 207, 0xFF, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 0xFF};
u8 adaDressLp[146] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207};
u8 adaDressULp[146] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 0xFF, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 0xFF, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 0xFF, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 0xFF, 118, 119, 120, 121, 122, 123, 124, 125, 126, 0xFF, 128, 129, 130, 131, 132, 133, 134, 135, 136, 0xFF, 138, 139, 140, 141, 142, 143, 144, 145, 146, 0xFF, 148, 149, 150, 151, 152, 153, 154, 155, 156, 0xFF, 158, 159, 160, 161, 162, 163, 164, 165, 166, 0xFF, 168, 169, 170, 171, 172, 173, 174, 175, 176, 0xFF, 178, 179, 180, 181, 182, 183, 184, 185, 186, 0xFF, 188, 189, 190, 191, 192, 193, 194, 195, 196, 0xFF, 198, 198, 199, 200, 201, 202, 203, 204, 205, 206};
f32 adaDressMax[146] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 adaDressWindS[146] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
f32 adaDressWindR[146] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
PlClothAt adaDressAt[35] = {
    {0x0000, 0x12, 0x12, 1.0f, 85.0f, {20.0f, 100.0f, 0.0f}, {20.0f, 100.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 85.0f, {20.0f, 50.0f, 0.0f}, {20.0f, 50.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 85.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.8f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.6f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.4f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.2f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x13, 1.0f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.8f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.6f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.4f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.2f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x14, 1.0f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 85.0f, {-20.0f, 50.0f, 0.0f}, {-20.0f, 50.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 85.0f, {-20.0f, 100.0f, 0.0f}, {-20.0f, 100.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 85.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.8f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.6f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.4f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.2f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x17, 1.0f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.8f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.6f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.4f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.2f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x18, 0x18, 1.0f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -50.0f, 0.0f}, {0.0f, -50.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -100.0f, 0.0f}, {0.0f, -100.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -150.0f, 0.0f}, {0.0f, -150.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -200.0f, 0.0f}, {0.0f, -200.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -250.0f, 0.0f}, {0.0f, -250.0f, 0.0f}},
    {0x0000, 0x13, 0x17, 0.5f, 95.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x17, 0.5f, 95.0f, {0.0f, -50.0f, 0.0f}, {0.0f, -50.0f, 0.0f}},
    {0x0000, 0x13, 0x17, 0.5f, 95.0f, {0.0f, -100.0f, 0.0f}, {0.0f, -100.0f, 0.0f}},
};

// adaHair
u8 adaHairP[14] = {65, 66, 67, 68, 69, 70, 71, 72, 219, 220, 221, 222, 223, 224};
u8 adaHairUp[14] = {0xFF, 65, 0xFF, 67, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 219, 0xFF, 221, 0xFF, 223};
u8 adaHairDp[14] = {66, 0xFF, 68, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 220, 0xFF, 222, 0xFF, 224, 0xFF};
f32 adaHairMax[14] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.1f, 0.1f, 0.1f, 0.1f};
f32 adaHairWindS[14] = {0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.0f, -2.5f, -2.5f, -2.0f, -2.0f};
f32 adaHairWindR[14] = {0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
PlClothAt adaHairAt[6] = {
    {0x0000, 0x04, 0x04, 1.0f, 65.0f, {0.0f, 50.0f, -10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x04, 0.5f, 70.0f, {0.0f, 50.0f, 0.0f}, {0.0f, 50.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 60.0f, {0.0f, 30.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {-5.0f, 100.0f, 30.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {-3.0f, 70.0f, 30.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 50.0f, 30.0f}, {0.0f, 0.0f, 0.0f}},
};

// adaRibbon
u8 adaRibbonP[22] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
u8 adaRibbonUp[22] = {0xFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0xFF, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
u8 adaRibbonDp[22] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0xFF, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 0xFF};
f32 adaRibbonMax[22] = {0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 adaRibbonWindS[22] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 adaRibbonWindR[22] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
PlClothAt adaRibbonAt[10] = {
    {0x0000, 0x04, 0x04, 1.0f, 90.0f, {0.0f, 0.0f, 20.0f}, {0.0f, 0.0f, 20.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 70.0f, {0.0f, 0.0f, 20.0f}, {0.0f, 0.0f, 20.0f}},
    {0x0000, 0x03, 0x02, 0.666f, 130.0f, {0.0f, 0.0f, 30.0f}, {0.0f, 0.0f, 30.0f}},
    {0x0000, 0x03, 0x02, 0.333f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x01, 0.666f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x01, 0.333f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x01, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x06, 0x06, 1.0f, 130.0f, {0.0f, 0.0f, 30.0f}, {0.0f, 0.0f, 30.0f}},
    {0x0000, 0x0C, 0x0C, 1.0f, 130.0f, {0.0f, 0.0f, 30.0f}, {0.0f, 0.0f, 30.0f}},
};

// The callers pass (&leonHair, &leonJacket, &leonHolster); the works are used as jacket, holster, hair.
void PlClothSetLeon(cModel* pl, PlCloth* jacket, PlCloth* holster, PlCloth* hair)
{
    testJacketSetLeon(pl, jacket);
    testHolsterSetLeon(pl, holster);
    testHairSetLeon(pl, hair);
}

void PlClothMoveLeon(cModel* pl, PlCloth* jacket, PlCloth* holster, PlCloth* hair)
{
    testJacketMoveLeon(pl, jacket);
    testHolsterMoveLeon(pl, holster);
    testHairMoveLeon(pl, hair);
    pl->be_flag &= ~0x00E00000;
}

// Likewise (&girlHair, &girlSkirt, &girlSweater) are used as skirt, hair, sweater (or ribbon).
void PlClothSetGirl(cModel* pl, PlCloth* skirt, PlCloth* hair, PlCloth* sweater, int evt)
{
    testHairSetGirl(pl, hair, evt);
    if (pG->costume2 == 1) {
        testRibbonSetGirl(pl, sweater);
    } else {
        testSkirtSetGirl(pl, skirt, evt);
        testSweaterSetGirl(pl, sweater);
    }
}

void PlClothMoveGirl(cModel* pl, PlCloth* skirt, PlCloth* hair, PlCloth* sweater)
{
    testHairMoveGirl(pl, hair);
    if (pG->costume2 == 1) {
        testRibbonMoveGirl(pl, sweater);
        girlLapelMove(pl);
    } else {
        testSkirtMoveGirl(pl, skirt);
        testSweaterMoveGirl(pl, sweater);
    }
    pl->be_flag &= ~0x00E00000;
}

void PlClothSetLuis(cModel* pl, PlCloth* hair)
{
    testHairSetLuis(pl, hair);
}

void PlClothMoveLuis(cModel* pl, PlCloth* hair)
{
    testHairMoveLuis(pl, hair);
    pl->be_flag &= ~0x00E00000;
}

void PlClothSetAda(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair, int evt)
{
    testDressSetAda(pl, dress, evt);
    testHairSetAda(pl, hair);
}

void PlClothMoveAda(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair)
{
    testDressMoveAda(pl, dress);
    testHairMoveAda(pl, hair);
    pl->be_flag &= ~0x00E00000;
}

void testHairSetLeon(cModel* pl, PlCloth* c)
{
    c->num = 21;
    c->pParts = leonHairP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = leonHairUp;
    c->pDown = leonHairDp;
    c->x20 = 0;
    c->pRate = 0;
    c->pMax = leonHairMax;
    c->pWindS = leonHairWindS;
    c->pWindR = leonHairWindR;
    c->pAt = leonHairAt;
    c->nAt = 4;
    c->x3C = 15.0f;
    c->x40 = 0.75f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.5f;
    c->pModel = 0;
    c->flags = 0x302;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testHairMoveLeon(cModel* pl, PlCloth* c)
{
    PenClothMove(pl, (PenCloth*) c);
}

void testJacketSetLeon(cModel* pl, PlCloth* c)
{
    if (pG->costume != 0) {
        return;
    }
    c->num = 24;
    c->pParts = leonJacketP;
    c->pLeft = leonJacketLp;
    c->pRight = leonJacketRp;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = leonJacketUp;
    c->pDown = leonJacketDp;
    c->x20 = 0;
    c->pRate = 0;
    c->pMax = leonJacketMax;
    c->pWindS = leonJacketWindS;
    c->pWindR = leonJacketWindR;
    c->pAt = leonJacketAt;
    c->nAt = 6;
    c->x3C = 25.0f;
    c->pModel = 0;
    c->x40 = 0.5f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 0.1f;
    c->x50 = 0.5f;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testJacketMoveLeon(cModel* pl, PlCloth* c)
{
    if (pG->costume == 0) {
        PenClothMove3(pl, (PenCloth*) c);
    }
}

void testHolsterSetLeon(cModel* pl, PlCloth* c)
{
    if (pG->costume != 1) {
        return;
    }
    c->num = 2;
    c->pParts = leonHolsterP;
    c->pLeft = leonHolsterLp;
    c->pRight = leonHolsterRp;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = leonHolsterUp;
    c->pDown = leonHolsterDp;
    c->x20 = 0;
    c->pRate = 0;
    c->pMax = leonHolsterMax;
    c->pWindS = leonHolsterWindS;
    c->pWindR = leonHolsterWindR;
    c->pAt = leonHolsterAt;
    c->nAt = 1;
    c->x3C = 25.0f;
    c->x40 = 0.7f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.5f;
    c->pModel = 0;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testHolsterMoveLeon(cModel* pl, PlCloth* c)
{
    if (pG->costume == 1) {
        PenClothMove3(pl, (PenCloth*) c);
    }
}

void testHairSetGirl(cModel* pl, PlCloth* c, int evt)
{
    c->num = 21;
    c->pParts = girlHairP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = girlHairUp;
    c->pDown = girlHairDp;
    c->pWindS = girlHairWindS;
    c->pWindR = girlHairWindR;
    if (evt) {
        c->pMax = girlHairMaxEvt;
        c->pAt = girlHairAtEvt;
        c->nAt = 7;
    } else {
        c->pMax = girlHairMax;
        c->pAt = girlHairAt;
        c->nAt = 5;
    }
    c->x20 = 0;
    c->pRate = 0;
    c->x3C = 15.0f;
    c->x40 = 0.75f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.5f;
    c->pModel = 0;
    c->flags = 0x302;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testHairMoveGirl(cModel* pl, PlCloth* c)
{
    PenClothMove(pl, (PenCloth*) c);
}

void testSkirtSetGirl(cModel* pl, PlCloth* c, int evt)
{
    c->num = 48;
    c->pParts = girlSkirtP;
    c->pLeft = girlSkirtLp;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = girlSkirtUp;
    c->pDown = girlSkirtDp;
    if (evt) {
        c->pMax = girlSkirtMaxEvt;
        c->pAt = girlSkirtAtEvt;
        c->nAt = 11;
    } else {
        c->pMax = girlSkirtMax;
        c->pAt = girlSkirtAt;
        c->nAt = 11;
    }
    c->pWindS = girlSkirtWindS;
    c->pWindR = girlSkirtWindR;
    c->x20 = 0;
    c->pRate = 0;
    c->x3C = 10.0f;
    c->x40 = 0.9f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.5f;
    c->pModel = 0;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testSkirtMoveGirl(cModel* pl, PlCloth* c)
{
    if (pG->flags_5010 & 0x200000) {
        c->x50 = 0.9f;
    } else {
        c->x50 = 0.5f;
    }
    PenClothMove3(pl, (PenCloth*) c);
}

void testSweaterSetGirl(cModel* pl, PlCloth* c)
{
    c->num = 8;
    c->pParts = girlSweaterP;
    c->pLeft = girlSweaterLp;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = girlSweaterUp;
    c->pDown = girlSweaterDp;
    c->x20 = 0;
    c->pRate = 0;
    c->pMax = girlSweaterMax;
    c->pWindS = girlSweaterWindS;
    c->pWindR = girlSweaterWindR;
    c->pAt = girlSweaterAt;
    c->nAt = 6;
    c->x3C = 10.0f;
    c->x40 = 0.7f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 0.1f;
    c->x50 = 0.5f;
    c->pModel = 0;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testSweaterMoveGirl(cModel* pl, PlCloth* c)
{
    PenClothMove(pl, (PenCloth*) c);
}

void testRibbonSetGirl(cModel* pl, PlCloth* c)
{
    c->num = 12;
    c->pParts = girlRibbonP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = girlRibbonUp;
    c->pDown = girlRibbonDp;
    c->x20 = 0;
    c->pRate = 0;
    c->pMax = girlRibbonMax;
    c->pWindS = girlRibbonWindS;
    c->pWindR = girlRibbonWindR;
    c->pAt = girlRibbonAt;
    c->nAt = 8;
    c->x3C = 10.0f;
    c->x40 = 0.7f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 0.1f;
    c->x50 = 0.5f;
    c->pModel = 0;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testRibbonMoveGirl(cModel* pl, PlCloth* c)
{
    PenClothMove(pl, (PenCloth*) c);
}

// One lapel of Ashley's alternate costume: the part is rotated away from the body by the angle the
// chest (two parts, weighted) rises above it, then bent around the local Y and Z axes. The two
// lapels are written out (the second copy's pointer locals become gcse copies of the first's).
// A plain block, not do/while(0): a loop note makes haifa treat the next insn as a full barrier,
// which would pin the PRE copies behind the second lapel's first call. The inner block re-derives
// `pm1`, so the first lapel's later uses go through the PRE copy and `pm1` itself dies in bb 0.
#define LAPEL_MOVE(no, pa, pb, lift, sy, sz)                                        \
    {                                                                               \
        f32 ang;                                                                    \
        p = pl->getPartsPtr(no);                                                    \
        PSMTXIdentity(p->worldMat);                                                 \
        TransMatrix(p->worldMat, &p->pos);                                          \
        PSMTXConcat(p->pParent->mat, p->worldMat, p->mat);                          \
        p->worldPos.x = p->mat[0][3];                                               \
        p->worldPos.y = p->mat[1][3];                                               \
        p->worldPos.z = p->mat[2][3];                                               \
        PSMTXConcat(p->mat, rotm, pm1);                                             \
        PSMTXInverse(pm1, pinv);                                                    \
        a = pl->getPartsPtr(pa);                                                    \
        b = pl->getPartsPtr(pb);                                                    \
        PosToPos(&a->worldPos, &b->worldPos, pv, lapel_weight);                     \
        PSMTXMultVec(pinv, pv, pv);                                                 \
        v.y += lift;                                                                \
        if (v.y > 0.0f) {                                                           \
            ang = asinf(v.y / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z));            \
        } else {                                                                    \
            ang = 0.0f;                                                             \
        }                                                                           \
        {                                                                           \
            f32 (*pm1)[4] = m1;                                                     \
            f32 (*pm3)[4] = m3;                                                      \
            Vec* pax = &axis;                                                       \
            rot.x = -ang * lapel_rate_x;                                            \
            rot.y = sy ang * lapel_rate_y;                                          \
            rot.z = sz ang * lapel_rate_z;                                          \
            PSMTXRotRad(p->worldMat, 'x', rot.x);                                   \
            TransMatrix(p->worldMat, &p->pos);                                      \
            PSMTXConcat(p->pParent->mat, p->worldMat, p->mat);                      \
            PSMTXConcat(p->mat, rotm, pm1);                                         \
            axis.x = 0.0f;                                                          \
            axis.y = 1.0f;                                                          \
            axis.z = 0.0f;                                                          \
            PSMTXMultVecSR(pm1, pax, pax);                                          \
            PSMTXRotAxisRad(pm3, pax, rot.y);                                       \
            PSMTXConcat(pm3, p->mat, p->mat);                                       \
            TransMatrix(p->mat, &p->worldPos);                                      \
            PSMTXConcat(p->mat, rotm, pm1);                                         \
            axis.x = 0.0f;                                                          \
            axis.y = 0.0f;                                                          \
            axis.z = 1.0f;                                                          \
            PSMTXMultVecSR(pm1, pax, pax);                                          \
            PSMTXRotAxisRad(pm3, pax, rot.z);                                       \
            PSMTXConcat(pm3, p->mat, p->mat);                                       \
            TransMatrix(p->mat, &p->worldPos);                                      \
        }                                                                           \
    }

void girlLapelMove(cModel* pl)
{
    static f32 lapel_rr = 50.0f;
    static f32 lapel_rl = 50.0f;
    static char lapel_name[3] = "CH";
    static f32 lapel_rate_x = 1.3f;
    static f32 lapel_rate_y = 0.3f;
    static f32 lapel_rate_z = 0.6f;
    static f32 lapel_weight = 0.32f;
    Mtx rotm;
    Mtx m1;
    Mtx inv;
    Mtx m3;
    Vec v;
    Vec axis;
    Vec rot;
    cModel* p;
    cModel* a;
    cModel* b;
    f32 (*pm1)[4] = m1;
    f32 (*pinv)[4] = inv;
    Vec* pv = &v;

    PSMTXRotRad(rotm, 'x', 0.75049156f);
    pl->getPartsPtr(4);
    LAPEL_MOVE(0x8D, 7, 8, lapel_rr, -, -);
    {
        f32 (*pm1)[4] = m1;
        f32 (*pinv)[4] = inv;
        Vec* pv = &v;

        LAPEL_MOVE(0x8E, 0xD, 0xE, lapel_rl, +, +);
    }
}

void testHairSetLuis(cModel* pl, PlCloth* c)
{
    c->num = 26;
    c->pParts = luisHairP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = luisHairUp;
    c->pDown = luisHairDp;
    c->x20 = 0;
    c->pRate = 0;
    c->pMax = luisHairMax;
    c->pWindS = luisHairWindS;
    c->pWindR = luisHairWindR;
    c->pAt = luisHairAt;
    c->nAt = 7;
    c->x3C = 15.0f;
    c->x40 = 0.75f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.5f;
    c->pModel = 0;
    c->flags = 0x302;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testHairMoveLuis(cModel* pl, PlCloth* c)
{
    PenClothMove(pl, (PenCloth*) c);
}

// The zero stores come out in source order because none of them is the zero's last use: `x54 = 0`
// is the last one (issued first). x44 is stored directly in both arms (jump2 folds the two `li`s
// into `li 10; beq; li 2` after reload, so nothing is hoisted above the zero stores).
void testDressSetAda(cModel* pl, PlCloth* c, int evt)
{
    f32 rate;

    c->num = 146;
    c->pParts = adaDressP;
    c->pLeft = adaDressLp;
    c->pUpLeft = adaDressULp;
    c->pUp = adaDressUp;
    c->pDown = adaDressDp;
    c->pMax = adaDressMax;
    c->pWindS = adaDressWindS;
    c->pWindR = adaDressWindR;
    c->pAt = adaDressAt;
    c->nAt = 35;
    c->x3C = 20.0f;
    rate = 0.7f;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->pRight = 0;
    c->x14 = 0;
    c->x20 = 0;
    c->pRate = 0;
    c->pModel = 0;
    c->x40 = rate;
    c->flags = 0;
    c->x54 = 0;
    if (evt) {
        c->x50 = rate;
        c->x44 = 2;
    } else {
        c->x50 = rate;
        c->x44 = 10;
    }
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testDressMoveAda(cModel* pl, PlCloth* c)
{
    PenClothMove3(pl, (PenCloth*) c);
}

void testHairSetAda(cModel* pl, PlCloth* c)
{
    c->num = 14;
    c->pParts = adaHairP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = adaHairUp;
    c->pDown = adaHairDp;
    c->x20 = 0;
    c->pRate = 0;
    c->pWindS = adaHairWindS;
    c->pWindR = adaHairWindR;
    c->pMax = adaHairMax;
    c->pAt = adaHairAt;
    c->nAt = 6;
    c->x3C = 15.0f;
    c->x40 = 0.75f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.5f;
    c->pModel = 0;
    c->flags = 0x302;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testHairMoveAda(cModel* pl, PlCloth* c)
{
    PenClothMove(pl, (PenCloth*) c);
}

cObjChain* AdaRibbonSet(cModel* pl, PlCloth* c, void* bin, void* tpl)
{
    cObjChain* chain;
    Vec pos;
    Vec rot;
    f32 zero;

    if (bin == 0 || tpl == 0) {
        return 0;
    }
    c->num = 22;
    c->pParts = adaRibbonP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = adaRibbonUp;
    c->pDown = adaRibbonDp;
    c->x20 = 0;
    c->pRate = 0;
    c->pMax = adaRibbonMax;
    c->x3C = 10.0f;
    c->x40 = 0.8f;
    c->x44 = 0;
    zero = 0.0f;
    c->x50 = 0.5f;
    c->x4C = 1.0f;
    c->x54 = 0;
    c->pWindS = adaRibbonWindS;
    c->pWindR = adaRibbonWindR;
    c->pAt = adaRibbonAt;
    c->nAt = 10;
    c->flags = 0x200;
    c->pModel = pl;
    c->x48 = zero;
    pos.x = zero;
    pos.y = zero;
    pos.z = zero;
    rot.x = zero;
    rot.y = zero;
    rot.z = zero;
    chain = SetChain(bin, tpl, &pos, &rot);
    chain->setChain((PenCloth*) c);
    pos.x = zero;
    pos.y = zero;
    pos.z = zero;
    chain->setParent(pl, 0x40, &pos, 0);
    return chain;
}
