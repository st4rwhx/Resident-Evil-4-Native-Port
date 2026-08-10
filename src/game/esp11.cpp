#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"

struct Esp11Work {
    u8 type;         // 0x00 light type (gen->xC9)
    u8 no;           // 0x01 light number (gen->xCA)
    u8 kind;         // 0x02 0/1: create a light, 2: fixed type 8, 3: no light (gen->xC8)
    u8 mode;         // 0x03 1: light follows the sprite (gen->xCB)
    cLight* pLight;  // 0x04
    GXColor col;     // 0x08 base color of the light
    u8 toolState;    // 0x0C (gen->xFC)
};

// Light source effect: creates a cLight and (mode 1) drives its position and color from the
// sprite.
class cEsp11 : public cEsp {
public:
    Esp11Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
    virtual void Destruct();
};

extern "C" {
void EffSetToolState(int state);
void Esp11_SetParam(cEsp11* esp);
}

cEsp* Esp11_Create()
{
    return new cEsp11;
}

void cEsp11::move()
{
    Esp11Work* w = &work;

    if (w->toolState != 0) {
        EffSetToolState(w->toolState);
    }
    if (w->pLight != NULL && (w->pLight->be_flag & 0x201) != 1) {
        w->pLight = NULL;
        PushEsp(this);
    } else if (w->mode == 1) {
        if (CommonMove()) {
            Esp11_SetParam(this);
        }
    } else {
        if (life != 0 && life <= cnt) {
            PushEsp(this);
        } else {
            cnt++;
        }
    }
}

void Esp11_Trans(cEsp* esp)
{
}

void cEsp11::Destruct()
{
    cLight* l = work.pLight;

    if (l != NULL && (l->be_flag & 0x201) == 1) {
        LightMgr.destroy(l);
    }
}

void Esp11_SetParam(cEsp11* esp)
{
    Esp11Work* w = &esp->work;
    f32 alpha;

    if (w->pLight == NULL) {
        return;
    }
    w->pLight->pos = esp->pos;
    if (esp->parent != pEffParentWorldS) {
        PSMTXMultVec(esp->parent->mat, &w->pLight->pos, &w->pLight->pos);
    }
    w->pLight->x1C = esp->sizeX * esp->scale * 10.0f;
    alpha = esp->colA / 255.0f;
    w->pLight->color.r = (u8)(w->col.r * esp->colR / 255.0f);
    w->pLight->color.g = (u8)(w->col.g * esp->colG / 255.0f);
    w->pLight->color.b = (u8)(w->col.b * esp->colB / 255.0f);
    w->pLight->color.a = (u8)(w->col.a * alpha * 0.5f);
}

int cEsp11::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp11Work* w = &work;

    w->kind = gen->xC8;
    w->type = gen->xC9;
    w->no = gen->xCA;
    w->mode = gen->xCB;
    w->toolState = gen->xFC;
    if (w->kind == 2) {
        w->kind = 0;
        w->type = 8;
        w->no = 0;
        w->mode = 1;
    }
    if (w->kind <= 1) {
        w->pLight = LightMgr.create(w->kind != 0, w->type, w->no, 0);
        if (w->pLight == NULL) {
            pLog->err(0, 0, "ESP11 : LightId[Kind:%d cut:%d no:%d] no data", w->kind, w->type, w->no);
            return 0;
        }
        w->pLight->be_flag |= 0x40;
    } else if (w->kind == 3) {
        w->pLight = NULL;
        return 1;
    } else {
        pLog->err(0, 0, "ESP11 : LightKind[%d] invalid", w->kind);
        return 0;
    }
    w->col = w->pLight->color;
    if (w->mode == 0) {
    } else if (w->mode == 1) {
        Esp11_SetParam(this);
    } else {
        pLog->err(0, 0, "ESP11 : LightType[%d] invalid", w->mode);
        return 0;
    }
    if (pG->flags_64 & 0x00800000) {
        pG->flags_170 &= ~0x01000000;
    }
    return 1;
}
