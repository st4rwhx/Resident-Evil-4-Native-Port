#ifndef CLOTH_H
#define CLOTH_H

#include "types.h"
#include "vec.h"
#include "gx.h"

// Cloth simulation (game/cloth.cpp). ClothWk[8], stride 0x78. Layout from Cloth::Set / esp4e.
class Cloth {
public:
    u8 flag;           // 0x00 bit0: in use
    u8 x1;             // 0x01
    u8 nx;             // 0x02 grid columns
    u8 ny;             // 0x03 grid rows
    f32 x4;            // 0x04 cell width
    f32 x8;            // 0x08 cell height
    f32 xC;            // 0x0C
    Vec* pos;          // 0x10 grid positions (nx*ny)
    Vec* nrm;          // 0x14 grid normals (calcNormal; {0,0,1} initially)
    Vec* spd;          // 0x18
    Mtx mat;           // 0x1C
    Vec x4C;           // 0x4C
    f32 x58;           // 0x58
    GXTexObj* tex;     // 0x5C
    GXTlutObj* tlut;   // 0x60
    void* x64;         // 0x64
    union {
        GXColor color; // 0x68 material colour (clothTrans passes it by value)
        struct {
            u8 colR;   // 0x68
            u8 colG;   // 0x69
            u8 colB;   // 0x6A
            u8 colA;   // 0x6B
        };
    };
    void* mem;         // 0x6C
    int x70;           // 0x70
    int x74;           // 0x74

    void Set(Vec ang, Vec pos, u8 nx, u8 ny, f32 w, GXTexObj* tex, f32 h, void* p, f32 d, GXTlutObj* tlut,
             int flag);
    void SetPosAng(Vec ang, Vec pos);
    void Destroy();
    void calcSpeed(f32 damping);
    void move();
    void calcNormal();
    void disturbance(f32 power, u32 x, u32 y);
};

extern "C" {
void ClothInit();
void ClothRoomInit();
void ClothCalcTplAddr(void* tpl);
int ClothTexSetUp(void* tpl, GXTexObj* tex, int no, GXTlutObj* tlut);
int PullCloth(Cloth** out);
void ClothDraw();
void clothTrans(Cloth* c);
}

#endif
