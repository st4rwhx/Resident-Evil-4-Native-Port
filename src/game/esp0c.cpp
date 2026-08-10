#include "atari.h"
#include "light.h"
#include "main_mem.h"
#include "esp.h"

struct Esp0cWork {
    u8 estNo;      // 0x00 est number (gen->xC8)
    u8 estPrm;     // 0x01 (gen->xC9)
    u8 estNo2;     // 0x02 est number used when the sprite hit water (gen->prm byte 0xCF)
    u8 estPrm2;    // 0x03 (gen->prm byte 0xD3)
    u32 onWater;   // 0x04 1: the position was raised to the water surface
};

// Est work passed to EstSet (0x1C bytes).
struct EstSetWork {
    u8 x0;      // 0x00
    u8 flag1;   // 0x01
    u8 flag2;   // 0x02
    u8 x3;      // 0x03
    Vec spd;    // 0x04
    f32 sizeX;  // 0x10
    f32 sizeY;  // 0x14
    u8 col[4];  // 0x18
};

// Est (effect set) trigger: on its first move it starts an est at its position and dies.
class cEsp0c : public cEsp {
public:
    Esp0cWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" int EffAreaCheckInRoom(Vec* pos);

cEsp* Esp0c_Create()
{
    return new cEsp0c;
}

void cEsp0c::move()
{
    Esp0cWork* w = &work;
    EstSetWork est;

    memclr_asm(&est, sizeof(EstSetWork));
    est.flag2 |= 1;
    est.flag1 |= 6;
    est.sizeX = sizeX * 0.005f;
    est.sizeY = sizeY * 0.005f;
    est.col[0] = x80;
    est.col[1] = x81;
    est.col[2] = x82;
    est.col[3] = x83;
    est.spd = spd;
    if (w->onWater == 1) {
        EstSet(0, -1, &pos, &rot, w->estNo2, w->estPrm2, info.x0, info.x2, info.x8, &est);
    } else {
        EstSet(0, -1, &pos, &rot, w->estNo, w->estPrm, info.x0, info.x2, info.x8, &est);
    }
    PushEsp(this);
}

extern "C" void Esp0c_Trans(cEsp* esp)
{
    pLog->err(0, 0, "ESP0C : Invalid Trans.");
}

int cEsp0c::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp0cWork* w = &work;
    u32 attr;
    f32 h;

    w->estNo = gen->xC8;
    w->estPrm = gen->xC9;
    w->estNo2 = gen->prm.b.xCF;
    w->estPrm2 = gen->prm.b.xD3;
    if (parent != pEffParentWorld) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    switch ((s8)gen->xCA) {
    case 0:
        break;
    case 1:
        pos.y = SatMgr.getFloor(&pos, 600.0f, 100000.0f, &attr, 0) + 65.0f + gen->xDC;
        break;
    case 2:
        pos.y = SatMgr.getFloor(&pos, 600.0f, 100000.0f, &attr, 0) + 65.0f;
        if (GetWaterHeight(&pos, &h)) {
            if (pos.y < h + gen->xDC) {
                pos.y = h + gen->xDC;
                w->onWater = 1;
            }
        }
        break;
    default:
        pLog->err(0, 0, "ESP0C : Type[%d] Invalid Trans.", (s8)gen->xCA);
        return 0;
    }
    if ((s8)gen->xCB == 0) {
    } else if ((s8)gen->xCB == 1) {
        if (EffAreaCheckInRoom(&pos) == 1) {
            PushEsp(this);
        }
    } else {
        pLog->err(0, 0, "ESP0C : Type[%d] Invalid Trans.", (s8)gen->xCB);
        return 0;
    }
    return 1;
}
