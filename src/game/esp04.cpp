#include "atari.h"
#include "light.h"
#include "gx.h"
#include "rnd.h"
#include "esp.h"

struct Esp04Work {
    Vec pos0;      // 0x00 initial position
    u8 tileFlag;   // 0x0C bit0: repeat horizontally, bit1: repeat vertically (gen->xC8)
    s8 randX;      // 0x0D random jitter in x (gen->xC9)
    s8 randY;      // 0x0E random jitter in y (gen->xCA)
    s8 alphaSpd;   // 0x0F alpha change per frame (gen->prm byte 0xCF)
    u8 alphaWait;  // 0x10 frames before the alpha starts changing (gen->xCB)
};

// Screen-space tiled texture (rain / dust overlay): a 2D quad grid drawn in an orthographic
// projection; scrolls with pos and wraps around the 512x448 screen.
class cEsp04 : public cEsp {
public:
    Esp04Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
void move00(cEsp04* esp);
void move10(cEsp04* esp);
}

static void (*func_tbl[])(cEsp04*) = { move00, move10 };

cEsp* Esp04_Create()
{
    return new cEsp04;
}

void cEsp04::move()
{
    if (ColorUpdate()) {
        if (life != 0 && life <= cnt) {
            PushEsp(this);
        } else {
            cnt++;
            func_tbl[x10](this);
        }
    }
}

void move00(cEsp04* esp)
{
    esp->work.pos0 = esp->pos;
    esp->x10 = 1;
}

void move10(cEsp04* esp)
{
    Esp04Work* w = &esp->work;
    f32 v;
    f32 x;
    f32 y;
    f32 lim;

    // Wrap the position back into the screen. The original keeps the loaded coordinate (x/y)
    // and the loop variable (v) in separate registers, which GCC's cse only does when the
    // loaded variable's last mention is later than v's (see the trailing reloads below).
    x = esp->pos.x;
    v = x;
    lim = 512.0f;
    if (x >= lim) {
        while (v >= 0.0f) {
            esp->pos.x = v - esp->sizeX;
            v = esp->pos.x;
        }
    } else if (x < -esp->sizeX) {
        if (x < lim - esp->sizeX) {
            v = x;
            while (v < lim - esp->sizeX) {
                v += esp->sizeX;
            }
            esp->pos.x = v;
        }
    }
    y = esp->pos.y;
    v = y;
    lim = 448.0f;
    if (y >= lim) {
        while (v >= 0.0f) {
            esp->pos.y = v - esp->sizeY;
            v = esp->pos.y;
        }
    } else if (y < -esp->sizeY) {
        if (y < lim - esp->sizeX) {
            // The original's loop step lives in f0 and its `v = y` copy is issued after the
            // hoisted step/bound copies (y stays live past it, so the loop bound cannot take
            // y's f12): value pin + keep-alive.
            register f32 s asm("fr0");  // COMPILER-DIFF: #17 (FPR value pin)
            s = esp->sizeY;
            v = y;
            while (v < lim - esp->sizeX) {
                v += s;
            }
            esp->pos.y = v;
            asm("" : "=m"(esp->pos.y) : "f"(y));  // COMPILER-DIFF: #13 (keep-alive)
        }
    }
    x = esp->pos.x;  // dead: keeps x/y alive past v for cse (see above)
    y = esp->pos.y;

    if (w->randX != 0) {
        esp->pos.x = w->pos0.x + (Rnd() % (w->randX * 2)) - (f32)w->randX;
        if (esp->pos.x < -esp->sizeX) {
            esp->pos.x += esp->sizeX + 512.0f;
        }
        if (esp->pos.x > esp->sizeX + 512.0f) {
            esp->pos.x -= esp->sizeX + 512.0f;
        }
    }
    if (w->randY != 0) {
        esp->pos.y = w->pos0.y + (Rnd() % (w->randY * 2)) - (f32)w->randY;
        if (esp->pos.y < -esp->sizeY) {
            esp->pos.y += esp->sizeY + 448.0f;
        }
        if (esp->pos.y > esp->sizeY + 448.0f) {
            esp->pos.y -= esp->sizeY + 448.0f;
        }
    }

    if (w->alphaWait != 0) {
        w->alphaWait--;
    } else {
        if (w->alphaSpd > 0) {
            if ((int)esp->colA + w->alphaSpd > 255) {
                esp->colA = 255.0f;
            } else {
                esp->colA += (f32)w->alphaSpd;
            }
        } else if (w->alphaSpd < 0) {
            if ((int)esp->colA + w->alphaSpd < 0) {
                esp->colA = 0.0f;
            } else {
                esp->colA += (f32)w->alphaSpd;
            }
        }
    }
    if (!esp->AnmMove()) {
        PushEsp(esp);
    }
}

extern "C" void Esp04_Trans(cEsp04* esp)
{
    Esp04Work* w = &esp->work;
    EspAnmData* anm;
    Mtx44 proj;
    int sx;
    int sy;
    int nx;
    int ny;
    int i;
    int j;
    s16 x;
    s16 y;
    s16 xx;
    f32 u;
    f32 uw;
    s16 z;

    if (!EspGetAnmAddr(esp->anmNo, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->anmNo);
        return;
    }
    z = 0;
    PSMTXIdentity(esp->mat);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    GXLoadPosMtxImm(esp->mat, 0);
    GXSetCurrentMtx(0);
    EspTexSet(esp->anmNo, esp->anmPtn);
    esp->ChannelSet();
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    esp->CommonStateSet();
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);

    sx = (int)esp->sizeX;
    sy = (int)esp->sizeY;
    u = 0.0f;
    uw = 1.0f;
    if (w->tileFlag & 1) {
        nx = 542 / sx + 2;
        x = (s16)esp->pos.x;
        if (sx != 0) {
            while (x > 0) {
                x -= sx;
            }
        }
    } else {
        nx = 1;
        x = (s16)esp->pos.x;
    }
    if (w->tileFlag & 2) {
        ny = 448 / sy + 2;
        y = (s16)esp->pos.y;
        if (sy != 0) {
            while (y > 0) {
                y -= sy;
            }
        }
    } else {
        ny = 1;
        y = (s16)esp->pos.y;
    }
    for (j = 0; j < ny; j++) {
        xx = x;
        for (i = 0; i < nx; i++) {
            GXBegin(0x80, 0, 4);
            GXPosition3s16(xx, y, z);
            GXTexCoord2f32(u, u);
            GXPosition3s16(xx + sx, y, z);
            GXTexCoord2f32(u + uw, u);
            GXPosition3s16(xx + sx, y + sy, z);
            GXTexCoord2f32(u + uw, u + uw);
            GXPosition3s16(xx, y + sy, z);
            GXTexCoord2f32(u, u + uw);
            xx += sx;
        }
        y += sy;
    }
}

int cEsp04::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp04Work* w = &work;

    w->tileFlag = gen->xC8;
    w->randX = gen->xC9;
    w->randY = gen->xCA;
    w->alphaWait = gen->xCB;
    w->alphaSpd = gen->prm.b.xCF;
    if (sizeX < 0.1f) {
        sizeX = 0.1f;
    }
    if (sizeY < 0.1f) {
        sizeY = 0.1f;
    }
    if (!((s8)partsNo >= -8 && (s8)partsNo <= -3)) {
        pLog->warn(0, 0, "ESP04: Parent is no SCREEN.");
    }
    return 1;
}
