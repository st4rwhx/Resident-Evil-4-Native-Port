// game/em_cloth.cpp: cloth / hair chain tables of the enemy costume models (obj18 type 34, 18,
// 37, 33, 30) and the em2b short rope; PenCloth* (pendulum.cpp) does the simulation.

#include "pl_cloth.h"
#include "pendulum.h"
#include "model.h"
#include "math_sub.h"
#include "em_cloth.h"

u8 em34ClothP[2] = {124, 125};
u8 em34ClothUp[2] = {0xFF, 124};
u8 em34ClothDp[2] = {125, 0xFF};
f32 em34ClothMax[2] = {0.3f, 0.4f};
static f32 em34ClothRate[2] = {0.8f, 0.8f};
u8 em34ClothP2[91] = {29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119};
u8 em34ClothUp2[91] = {0xFF, 29, 30, 31, 32, 33, 34, 0xFF, 36, 37, 38, 39, 40, 41, 0xFF, 43, 44, 45, 46, 47, 48, 0xFF, 50, 51, 52, 53, 54, 55, 0xFF, 57, 58, 59, 60, 61, 62, 0xFF, 64, 65, 66, 67, 68, 69, 0xFF, 71, 72, 73, 74, 75, 76, 0xFF, 78, 79, 80, 81, 82, 83, 0xFF, 85, 86, 87, 88, 89, 90, 0xFF, 92, 93, 94, 95, 96, 97, 0xFF, 99, 100, 101, 102, 103, 104, 0xFF, 106, 107, 108, 109, 110, 111, 0xFF, 113, 114, 115, 116, 117, 118};
u8 em34ClothDp2[91] = {30, 31, 32, 33, 34, 35, 0xFF, 37, 38, 39, 40, 41, 42, 0xFF, 44, 45, 46, 47, 48, 49, 0xFF, 51, 52, 53, 54, 55, 56, 0xFF, 58, 59, 60, 61, 62, 63, 0xFF, 65, 66, 67, 68, 69, 70, 0xFF, 72, 73, 74, 75, 76, 77, 0xFF, 79, 80, 81, 82, 83, 84, 0xFF, 86, 87, 88, 89, 90, 91, 0xFF, 93, 94, 95, 96, 97, 98, 0xFF, 100, 101, 102, 103, 104, 105, 0xFF, 107, 108, 109, 110, 111, 112, 0xFF, 114, 115, 116, 117, 118, 119, 0xFF};
u8 em34ClothLp2[91] = {36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
f32 em34ClothMax2[91] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
PlClothAt em34ClothAt2_LongStride[16] = {
    {0x0000, 0x12, 0x12, 1.0f, 150.0f, {30.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.75f, 160.0f, {40.0f, 0.0f, 0.0f}, {40.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.5f, 160.0f, {60.0f, 0.0f, 0.0f}, {60.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.25f, 160.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x13, 1.0f, 160.0f, {80.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.75f, 160.0f, {80.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.5f, 160.0f, {80.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.25f, 160.0f, {80.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 150.0f, {-30.0f, 0.0f, 0.0f}, {-30.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.75f, 160.0f, {-40.0f, 0.0f, 0.0f}, {-40.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.5f, 160.0f, {-60.0f, 0.0f, 0.0f}, {-60.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.25f, 160.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x17, 1.0f, 160.0f, {-80.0f, 0.0f, 0.0f}, {-80.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.75f, 160.0f, {-80.0f, 0.0f, 0.0f}, {-80.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.5f, 160.0f, {-80.0f, 0.0f, 0.0f}, {-80.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.25f, 160.0f, {-80.0f, 0.0f, 0.0f}, {-80.0f, 0.0f, 0.0f}},
};
static PlClothAt em34ClothAt[5] = {
    {0x0000, 0x04, 0x04, 1.0f, 120.0f, {0.0f, -100.0f, -30.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.3f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x05, 0x05, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0B, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.7f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
u8 em18ClothP[27] = {56, 57, 58, 59, 60, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55};
u8 em18ClothUp[27] = {0xFF, 56, 57, 58, 59, 0xFF, 34, 35, 36, 37, 38, 39, 40, 0xFF, 42, 43, 44, 45, 46, 47, 48, 0xFF, 50, 51, 52, 53, 54};
u8 em18ClothDp[27] = {57, 58, 59, 60, 0xFF, 35, 36, 37, 38, 39, 40, 41, 0xFF, 43, 44, 45, 46, 47, 48, 49, 0xFF, 51, 52, 53, 54, 55, 0xFF};
u8 em18ClothLp[27] = {37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
f32 em18ClothMax[27] = {0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f};
f32 em18ClothRate[27] = {0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
PlClothAt em18ClothAt[15] = {
    {0x0000, 0x02, 0x02, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 150.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, -150.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.5f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x01, 1.0f, 170.0f, {0.0f, 100.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x11, 0.5f, 180.0f, {0.0f, 0.0f, 25.0f}, {0.0f, 0.0f, 25.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 180.0f, {0.0f, -150.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 180.0f, {40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.33f, 180.0f, {40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.66f, 180.0f, {40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x13, 1.0f, 180.0f, {40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 180.0f, {-40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.33f, 180.0f, {-40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.66f, 180.0f, {-40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x17, 1.0f, 180.0f, {-40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
};
u8 em37HairP[8] = {64, 65, 66, 67, 68, 69, 70, 71};
u8 em37HairUp[8] = {0xFF, 64, 65, 66, 0xFF, 68, 0xFF, 70};
static u8 em37HairDp[8] = {65, 66, 67, 0xFF, 69, 0xFF, 71, 0xFF};
static f32 em37HairMax[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 em37HairRate[8] = {0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
PlClothAt em37HairAt[8] = {
    {0x0000, 0x03, 0x04, 0.1f, 65.0f, {0.0f, 90.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x04, 0.6f, 75.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 90.0f, {0.0f, -30.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.5f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x05, 0x03, 1.0f, 90.0f, {-50.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x05, 0x03, 1.0f, 100.0f, {-50.0f, -50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x03, 1.0f, 90.0f, {50.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x03, 1.0f, 100.0f, {50.0f, -50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
u8 em37CoatP[39] = {72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110};
static u8 em37CoatUp[39] = {0xFF, 72, 73, 0xFF, 75, 76, 0xFF, 78, 79, 0xFF, 81, 82, 0xFF, 84, 85, 0xFF, 87, 88, 0xFF, 90, 91, 0xFF, 93, 94, 0xFF, 96, 97, 0xFF, 99, 100, 0xFF, 102, 103, 0xFF, 105, 106, 0xFF, 108, 109};
u8 em37CoatDp[39] = {73, 74, 0xFF, 76, 77, 0xFF, 79, 80, 0xFF, 82, 83, 0xFF, 85, 86, 0xFF, 88, 89, 0xFF, 91, 92, 0xFF, 94, 95, 0xFF, 97, 98, 0xFF, 100, 101, 0xFF, 103, 104, 0xFF, 106, 107, 0xFF, 109, 110, 0xFF};
static u8 em37CoatLp[39] = {75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 0xFF, 0xFF, 0xFF};
f32 em37CoatMax[39] = {0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f};
static f32 em37CoatRate[39] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
static PlClothAt em37CoatAt[12] = {
    {0x0000, 0x11, 0x12, 0.3f, 110.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.8f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.6f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.4f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.2f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x16, 0.3f, 110.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.8f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.6f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.4f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.2f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static u8 em33HairP[60] = {91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96};
static u8 em33HairUp[60] = {0xFF, 91, 92, 93, 94, 0xFF, 96, 97, 98, 99, 0xFF, 101, 102, 103, 104, 0xFF, 106, 107, 108, 109, 0xFF, 111, 112, 113, 114, 0xFF, 116, 117, 118, 119, 0xFF, 121, 122, 123, 124, 0xFF, 126, 127, 0x80, 0x81, 0xFF, 0x83, 0x84, 0x85, 0x86, 0xFF, 0x88, 0x89, 0x8A, 0x8B, 0xFF, 0x8D, 0x8E, 0x8F, 0x90, 0xFF, 0x92, 0x93, 0x94, 0x95};
static u8 em33HairDp[60] = {92, 93, 94, 95, 0xFF, 97, 98, 99, 100, 0xFF, 102, 103, 104, 105, 0xFF, 107, 108, 109, 110, 0xFF, 112, 113, 114, 115, 0xFF, 117, 118, 119, 120, 0xFF, 122, 123, 124, 125, 0xFF, 127, 0x80, 0x81, 0x82, 0xFF, 0x84, 0x85, 0x86, 0x87, 0xFF, 0x89, 0x8A, 0x8B, 0x8C, 0xFF, 0x8E, 0x8F, 0x90, 0x91, 0xFF, 0x93, 0x94, 0x95, 0x96, 0xFF};
static u8 em33ClothLp[60] = {96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static f32 em33HairMax[60] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
static PlClothAt em33HairAt[17] = {
    {0x0000, 0x10, 0x10, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.2f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.4f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.6f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.8f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.5f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x14, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.5f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.2f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.4f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.6f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.8f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x15, 0x15, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x15, 0x16, 0.5f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static PlClothAt em33HairAtSmall[17] = {
    {0x0000, 0x10, 0x10, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.2f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.4f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.6f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.8f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.5f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x14, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.5f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.2f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.4f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.6f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.8f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x15, 0x15, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x15, 0x16, 0.5f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static u8 em33HairP2[46] = {0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6};
u8 em33HairUp2[46] = {0xFF, 0x97, 0x98, 0xFF, 0x9A, 0x9B, 0xFF, 0x9D, 0x9E, 0xFF, 0xA0, 0xA1, 0xFF, 0xA3, 0xA4, 0xFF, 0xA6, 0xA7, 0xFF, 0xAA, 0xFF, 0xAC, 0xFF, 0xAE, 0xFF, 0xB0, 0xFF, 0xB2, 0xFF, 0xB4, 0xFF, 0xB6, 0xFF, 0xB9, 0xFF, 0xBB, 0xFF, 0xBD, 0xFF, 0xBF, 0xFF, 0xC1, 0xFF, 0xC3, 0xFF, 0xC5};
static u8 em33HairDp2[46] = {0x98, 0x99, 0xFF, 0x9B, 0x9C, 0xFF, 0x9E, 0x9F, 0xFF, 0xA1, 0xA2, 0xFF, 0xA4, 0xA5, 0xFF, 0xA7, 0xA8, 0xFF, 0xAB, 0xFF, 0xAD, 0xFF, 0xAF, 0xFF, 0xB1, 0xFF, 0xB3, 0xFF, 0xB5, 0xFF, 0xB7, 0xFF, 0xBA, 0xFF, 0xBC, 0xFF, 0xBE, 0xFF, 0xC0, 0xFF, 0xC2, 0xFF, 0xC4, 0xFF, 0xC6, 0xFF};
static u8 em33ClothLp2[46] = {0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xFF, 0xFF, 0xFF, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xFF, 0xFF, 0xFF, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xFF, 0xFF, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xFF, 0xFF};
f32 em33HairMax2[46] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
static PlClothAt em33HairAt2[11] = {
    {0x0000, 0x02, 0x02, 1.0f, 220.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 220.0f, {120.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 220.0f, {-120.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x07, 1.0f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.8f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.6f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.4f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0B, 1.0f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.8f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.6f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.4f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static PlClothAt em33HairAt2Small[11] = {
    {0x0000, 0x02, 0x02, 1.0f, 187.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 187.0f, {102.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 187.0f, {-120.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x07, 1.0f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.8f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.6f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.4f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0B, 1.0f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.8f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.6f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.4f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
u8 em2bShortRopeP[5] = {1, 2, 3, 4, 5};
u8 em2bShortRopeUp[5] = {0xFF, 1, 2, 3, 4};
u8 em2bShortRopeDp[5] = {2, 3, 4, 5, 0xFF};
static PlClothAt em2bRopeAt[5] = {
    {0x0000, 0x03, 0x03, 1.0f, 650.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.5f, 750.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 900.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x05, 0x05, 1.0f, 700.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0B, 1.0f, 700.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
};
static u8 em30ClothP[49] = {94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E};
static u8 em30ClothUp[49] = {0xFF, 94, 95, 96, 97, 98, 99, 0xFF, 101, 102, 103, 104, 105, 106, 0xFF, 108, 109, 110, 111, 112, 113, 0xFF, 115, 116, 117, 118, 119, 120, 0xFF, 122, 123, 124, 125, 126, 127, 0xFF, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0xFF, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D};
static u8 em30ClothDp[49] = {95, 96, 97, 98, 99, 100, 0xFF, 102, 103, 104, 105, 106, 107, 0xFF, 109, 110, 111, 112, 113, 114, 0xFF, 116, 117, 118, 119, 120, 121, 0xFF, 123, 124, 125, 126, 127, 0x80, 0xFF, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0xFF, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0xFF};
u8 em30ClothLp[49] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87};
static f32 em30ClothMax[49] = {0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 em30ClothWindS[49] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f};
static f32 em30ClothWindR[49] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f};
PlClothAt em30ClothAt[2] = {
    {0x0000, 0x01, 0x01, 1.0f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static u8 em30ClothP2[30] = {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93};
u8 em30ClothUp2[30] = {0xFF, 64, 0xFF, 66, 0xFF, 68, 69, 0xFF, 71, 72, 73, 74, 0xFF, 76, 77, 78, 79, 0xFF, 81, 82, 83, 84, 0xFF, 86, 87, 88, 89, 0xFF, 91, 92};
u8 em30ClothDp2[30] = {65, 0xFF, 67, 0xFF, 69, 70, 0xFF, 72, 73, 74, 75, 0xFF, 77, 78, 79, 80, 0xFF, 82, 83, 84, 85, 0xFF, 87, 88, 89, 90, 0xFF, 92, 93, 0xFF};
static u8 em30ClothLp2[30] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 88, 89, 90};
f32 em30ClothMax2[30] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

void Em34ClothSet1(cModel* m, PlCloth* c)
{
    {
        // COMPILER-DIFF: 13 (local-alloc qty order): a codeless prio-4 filler issued before the
        // pUp2 `lis` in sched1 equalises the pUp2/pDown2/pMax2 qty lives (r10/r8/r7 in qty order).
        register u32 k asm("r12");
        asm("" : "=r"(k));
        asm("" : "=m"(em34ClothRate[1]) : "r"(k));
    }
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pWindS = 0;
    c->pWindR = 0;
    c->x20 = 0;
    c->pRate = 0;
    c->pModel = m;
    c->x48 = 0.0f;
    c->x50 = 0.0f;
    c->num = 91;
    c->pParts = em34ClothP2;
    c->pLeft = em34ClothLp2;
    c->pUp = em34ClothUp2;
    c->pDown = em34ClothDp2;
    c->pMax = em34ClothMax2;
    c->flags = 0x100;
    c->x54 = 0;
    c->pAt = em34ClothAt2_LongStride;
    c->nAt = 16;
    c->x3C = 20.0f;
    c->x40 = 0.7f;
    c->x44 = 14;
    c->x4C = 1.0f;
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em34ClothMove1(cModel* m, PlCloth* c)
{
    PenClothMove3(m, (PenCloth*) c);
}

void Em34ClothReset(cModel* m)
{
    m->be_flag &= ~0x00E00000;
}

void Em34ClothSet2(cModel* m, PlCloth* c)
{
    c->num = 2;
    c->pParts = em34ClothP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em34ClothUp;
    c->pDown = em34ClothDp;
    c->pMax = em34ClothMax;
    c->pWindS = 0;
    c->pWindR = 0;
    c->x20 = 0;
    c->pAt = em34ClothAt;
    c->pRate = em34ClothRate;
    c->nAt = 5;
    c->x3C = 15.0f;
    c->x40 = 0.8f;
    c->x44 = 4;
    c->pModel = m;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.0f;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em34ClothMove2(cModel* m, PlCloth* c)
{
    PenClothMove3(m, (PenCloth*) c);
}

void Em18ClothSet(cModel* m, PlCloth* c, int a)
{
    c->num = 27;
    c->pParts = em18ClothP;
    c->pLeft = em18ClothLp;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em18ClothUp;
    c->pDown = em18ClothDp;
    c->pMax = em18ClothMax;
    c->pWindS = 0;
    c->pWindR = 0;
    c->x20 = 0;
    c->pAt = em18ClothAt;
    c->pRate = em18ClothRate;
    c->nAt = 15;
    c->x3C = 20.0f;
    c->pModel = m;
    c->x40 = 0.1f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 0.05f;
    c->x50 = 0.0f;
    c->flags = 0x100;
    c->x54 = 0;
    if (a) {
        // The x40/x44 stores repeat the defaults: they are real uses for flow/sched1/regalloc
        // (0.1 and 4 live across the branch: f11, callee-saved r28) and reload_cse_regs deletes
        // them as no-op stores before sched2 (no code).
        c->pRate = 0;
        c->x40 = 0.1f;
        c->x44 = 4;
        c->x4C = 1.0f;
    }
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em18ClothMove(cModel* m, PlCloth* c)
{
    PenClothMove3(m, (PenCloth*) c);
    m->be_flag &= ~0x00E00000;
}

void Em37HairSet(cModel* m, PlCloth* c)
{
    c->num = 8;
    c->pParts = em37HairP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em37HairUp;
    c->pDown = em37HairDp;
    c->pMax = em37HairMax;
    c->pWindS = 0;
    c->pWindR = 0;
    c->x20 = 0;
    c->pAt = em37HairAt;
    c->pRate = em37HairRate;
    c->nAt = 8;
    c->x3C = 10.0f;
    c->x40 = 0.8f;
    c->x44 = 4;
    c->pModel = m;
    c->x48 = 0.0f;
    c->x4C = 0.1f;
    c->x50 = 0.0f;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em37HairMove(cModel* m, PlCloth* c)
{
    PenClothMove(m, (PenCloth*) c);
}

void Em37ClothReset(cModel* m)
{
    m->be_flag &= ~0x00E00000;
}

void Em37CoatSet(cModel* m, PlCloth* c)
{
    c->num = 39;
    c->pParts = em37CoatP;
    c->pLeft = em37CoatLp;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em37CoatUp;
    c->pDown = em37CoatDp;
    c->pMax = em37CoatMax;
    c->pWindS = 0;
    c->pWindR = 0;
    c->x20 = 0;
    c->pAt = em37CoatAt;
    c->pRate = em37CoatRate;
    c->nAt = 12;
    c->x3C = 15.0f;
    c->x40 = 0.9f;
    c->x44 = 4;
    c->pModel = m;
    c->x48 = 0.0f;
    c->x4C = 0.1f;
    c->x50 = 0.0f;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em37CoatMove(cModel* m, PlCloth* c)
{
    PenClothMove(m, (PenCloth*) c);
}

void Em33ClothSet(cModel* m, PlCloth* c, int small)
{
    c->num = 60;
    c->pParts = em33HairP;
    c->pLeft = em33ClothLp;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em33HairUp;
    c->pDown = em33HairDp;
    c->pMax = em33HairMax;
    c->pWindS = 0;
    c->pWindR = 0;
    if (small == 0) {
        c->pAt = em33HairAt;
    } else {
        c->pAt = em33HairAtSmall;
    }
    c->nAt = 17;
    c->x20 = 0;
    c->pRate = 0;
    c->x3C = 10.0f;
    c->x40 = 0.8f;
    c->x44 = 4;
    c->pModel = m;
    c->x48 = 0.0f;
    c->x4C = 0.1f;
    c->x50 = 0.0f;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em33ClothMove(cModel* m, PlCloth* c)
{
    PenClothMove3(m, (PenCloth*) c);
}

void Em33ClothSet2(cModel* m, PlCloth* c, int small)
{
    c->num = 46;
    c->pParts = em33HairP2;
    c->pLeft = em33ClothLp2;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em33HairUp2;
    c->pDown = em33HairDp2;
    c->pMax = em33HairMax2;
    c->pWindS = 0;
    c->pWindR = 0;
    if (small == 0) {
        c->pAt = em33HairAt2;
    } else {
        c->pAt = em33HairAt2Small;
    }
    c->nAt = 11;
    c->x20 = 0;
    c->pRate = 0;
    c->x3C = 10.0f;
    c->x40 = 0.8f;
    c->x44 = 4;
    c->pModel = m;
    c->x48 = 0.0f;
    c->x4C = 0.1f;
    c->x50 = 0.0f;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em33ClothMove2(cModel* m, PlCloth* c)
{
    cModel* p;

    p = m->getPartsPtr(0xA9);
    p->rot.z = m->getPartsPtr(7)->rot.z;
    RotMatrix(p->worldMat, &p->rot);
    TransMatrix(p->worldMat, &p->pos);
    ScaleMatrix(p->worldMat, &p->scale);
    PSMTXConcat(p->pParent->mat, p->worldMat, p->mat);
    p->worldPos.x = p->mat[0][3];
    p->worldPos.y = p->mat[1][3];
    p->worldPos.z = p->mat[2][3];
    p = m->getPartsPtr(0xB8);
    p->rot.z = m->getPartsPtr(0xB)->rot.z;
    RotMatrix(p->worldMat, &p->rot);
    TransMatrix(p->worldMat, &p->pos);
    ScaleMatrix(p->worldMat, &p->scale);
    PSMTXConcat(p->pParent->mat, p->worldMat, p->mat);
    p->worldPos.x = p->mat[0][3];
    p->worldPos.y = p->mat[1][3];
    p->worldPos.z = p->mat[2][3];
    PenClothMove3(m, (PenCloth*) c);
}

void Em33ClothReset(cModel* m)
{
    m->be_flag &= ~0x00E00000;
}

cObjChain* Em2bShortRopeSet(cModel* m, PlCloth* c, void* bin, void* tpl)
{
    cObjChain* chain;
    Vec pos;
    Vec ofs;
    Vec rot;

    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    chain = SetChain(bin, tpl, &pos, &rot);
    if (chain == 0) {
        return 0;
    }
    c->num = 5;
    c->pParts = em2bShortRopeP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em2bShortRopeUp;
    c->pDown = em2bShortRopeDp;
    c->pMax = 0;
    c->pWindS = 0;
    c->pWindR = 0;
    c->x20 = 0;
    c->pRate = 0;
    c->pAt = em2bRopeAt;
    c->nAt = 5;
    c->x3C = 20.0f;
    c->x40 = 0.8f;
    c->x44 = 100;
    c->pModel = m;
    c->x48 = 0.0f;
    c->x4C = 0.1f;
    c->x50 = 0.0f;
    c->flags = 0;
    c->x54 = 0;
    chain->setChain((PenCloth*) c);
    pos.x = -290.0f;
    pos.y = -162.95f;
    pos.z = 655.0f;
    ofs.x = -290.0f;
    ofs.y = -412.32f;
    ofs.z = 39.15f;
    chain->setParent2(m, 3, &pos, 4, &ofs, 0);
    return chain;
}

void Em30ClothSet1(cModel* m, PlCloth* c)
{
    c->num = 49;
    c->pParts = em30ClothP;
    c->pLeft = em30ClothLp;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em30ClothUp;
    c->pDown = em30ClothDp;
    c->pMax = em30ClothMax;
    c->pWindS = em30ClothWindS;
    c->pWindR = em30ClothWindR;
    c->x20 = 0;
    c->pAt = em30ClothAt;
    c->pRate = 0;
    c->nAt = 2;
    c->x3C = 30.0f;
    c->x40 = 0.9f;
    c->x44 = 20;
    c->pModel = m;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.0f;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em30ClothMove1(cModel* m, PlCloth* c)
{
    PenClothMove3(m, (PenCloth*) c);
}

void Em30ClothSet2(cModel* m, PlCloth* c)
{
    c->num = 30;
    c->pParts = em30ClothP2;
    c->pLeft = em30ClothLp2;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em30ClothUp2;
    c->pDown = em30ClothDp2;
    c->pWindS = 0;
    c->pWindR = 0;
    c->pMax = em30ClothMax2;
    c->pAt = 0;
    c->x20 = 0;
    c->pRate = 0;
    c->nAt = 0;
    c->x3C = 15.0f;
    c->x40 = 0.5f;
    c->pModel = m;
    c->x44 = 0;
    c->x48 = 0.0f;
    c->x4C = 0.05f;
    c->x50 = 0.0f;
    c->flags = 0;
    c->x54 = 0;
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em30ClothMove2(cModel* m, PlCloth* c)
{
    PenClothMove3(m, (PenCloth*) c);
}

void Em30ClothReset(cModel* m)
{
    m->be_flag &= ~0x00E00000;
}

// Never called (dead-stripped by the original linker; only its 0.0f pool entry survives).
static void Em30ClothStop(cModel* m, PlCloth* c)
{
    c->x48 = 0.0f;
}

asm(".section .sdata; .balign 8");
