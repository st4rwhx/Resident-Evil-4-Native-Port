#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "rnd.h"
#include "db_log.h"

struct Espgen00Work;

extern "C" {
void espgen00_UpdateMatrix(EspgenWork* w);
void espgen00_Update(EspgenWork* w);
void espgen00_Move00(EspgenWork* w);
void espgen00_Move01(EspgenWork* w);
static f32 Calc_D256(Espgen00Work* p, u8 d, f32 rate);
}

// Effect controller 00: emits one esp record repeatedly (num at a time, every wait frames).
struct Espgen00Work {
    EspGenWork* rec;   // 0x14
    cModel* model;     // 0x18
    u32 serial;        // 0x1C model serial the controller was set up with
    u16 cnt;           // 0x20 frame counter
    u16 life;          // 0x24 life time (0 = infinite)
    u8 pad_24;
    u8 wait;           // 0x25 frames between emissions
    u8 waitCnt;        // 0x26 frames left until the next emission
    u8 num;            // 0x27 emissions per frame - 1
    u32 seed;          // 0x28
    u8 flags;          // 0x2C rec->x10B: bit0 spread the angle, bit1 fixed seed
    u8 flags2;         // 0x2D bit0 parts matrix fixed, bit1 head flag, bit2 pass the position on
    u8 parts;          // 0x2E
    u8 waitRnd;        // 0x2F random range added to the wait
    Mtx mtx;           // 0x30
    Vec pos;           // 0x60
    Vec rot;           // 0x6C
    u8 scaleD;         // 0x78 rate curve parameters (Calc_D256)
    u8 spdD;           // 0x79
    u8 colD;           // 0x7A
    u8 waitD;          // 0x7B
    EspSeqOpt opt;     // 0x7C
    EspSeqOpt* pOpt;   // 0x98
};

void espgen00_UpdateMatrix(EspgenWork* w)
{
    Espgen00Work* p = (Espgen00Work*) w->work;
    cModel* model = p->model;

    if ((p->parts >= 0xF8 && p->parts <= 0xFD) || p->parts == 0xFF) {
        pLog->err(0, 0, "ESP_CTRL : NULL_PARTS_NO[%x] invalid.", p->parts);
        PushEspgen(w);
        return;
    }
    if (p->parts == 0xFE) {
        return;
    }
    if (model == NULL) {
        pLog->err(0, 0, "ESP_CTRL : PARTS_NO is set but No Parent.");
        return;
    }
    if (!(p->flags2 & 1)) {
        if (p->parts < model->nParts) {
            cModel* part;
            Vec ofs;
            Vec r;

            part = model->getPartsPtr(p->parts);
            PSMTXIdentity(p->mtx);
            PSVECAdd(&p->rot, &model->rot, &r);
            RotMatrix(p->mtx, &r);
            PSMTXMultVecSR(p->mtx, &p->pos, &ofs);
            p->mtx[0][3] = part->mat[0][3] + ofs.x;
            p->mtx[1][3] = part->mat[1][3] + ofs.y;
            p->mtx[2][3] = part->mat[2][3] + ofs.z;
            if (!(p->flags2 & 2)) {
                p->flags2 |= 1;
            }
        } else {
            pLog->err(0, 0, "ESP_CTRL : PARTS_NO[%d] is invalid(MAX:%d).", p->parts, model->nParts);
            PushEspgen(w);
            return;
        }
    }
}

// Rate curve: d >= 0 fades 1 -> 1 - d/128 over the life, d < 0 grows 1 -> 1 + 10 * -d/128.
static f32 Calc_D256(Espgen00Work* p, u8 d, f32 rate)
{
    s8 v = d;
    f32 t;
    f32 ret;

    // t is assigned in both arms: a global pseudo, so local-alloc does not tie the
    // (f32)d / constant operands to the product (d -> f13, 1/128 -> f12, 1.0 -> f11)
    if (v >= 0) {
        t = (f32) d * 0.0078125f;
        ret = 1.0f - rate * t;
    } else {
        t = (f32) v * -0.0078125f;
        ret = rate * t * 10.0f + 1.0f;
    }
    return ret;
}

void espgen00_Update(EspgenWork* w)
{
    Espgen00Work* p = (Espgen00Work*) w->work;
    f32 spdR = 0.0f;
    f32 scaleR = 0.0f;
    f32 colR = 0.0f;
    int bScale = 0;
    int bSpd = 0;
    int bCol = 0;
    int add = 0;
    cModel* model = p->model;

    if (model != NULL) {
        if ((model->be_flag & 0x201) != 1 || model->serial != p->serial) {
            PushEspgen(w);
            return;
        }
    }
    espgen00_UpdateMatrix(w);
    if (p->life != 0) {
        f32 rate = (f32) p->cnt / (f32) (int) p->life;

        if (p->scaleD) {
            scaleR = Calc_D256(p, p->scaleD, rate);
            bScale = 1;
        }
        if (p->spdD) {
            spdR = Calc_D256(p, p->spdD, rate);
            bSpd = 1;
        }
        if (p->colD) {
            colR = Calc_D256(p, p->colD, rate);
            bCol = 1;
        }
        if (p->waitD) {
            add = (int) (rate * (f32) (s8) p->waitD);
        }
    }
    if (p->waitCnt == 0) {
        EspGenWork* rec = p->rec;
        int n;
        int i;

        n = p->wait + add;
        if (n < 0) {
            p->waitCnt = 0;
        } else {
            p->waitCnt = n;
        }
        if (p->waitRnd) {
            int r = Rnd() % (p->waitRnd * 2) - p->waitRnd;

            if (p->waitCnt + r < 0) {
                p->waitCnt = 0;
            } else if (p->waitCnt + r > 255) {
                p->waitCnt = 255;
            } else {
                p->waitCnt = p->waitCnt + r;
            }
        }
        if (g_pEspSys->xC554 - g_pEspSys->xC548 < (u32) (p->num + 1)) {
            return;
        }
        {
            f32 step = 6.28f / (f32) (p->num + 1);
            f32 ang = 0.0f;

            for (i = 0; i < p->num + 1; i++) {
                cEsp* esp;
                Vec* pos = NULL;
                int ret;

                if (p->flags2 & 4) {
                    pos = &p->pos;
                }
                if (p->flags & 1) {
                    ret = EspSeqSet(rec, &w->info, &p->seed, p->model, &p->mtx, 1, ang, &esp, p->pOpt, pos);
                    ang += step;
                } else {
                    ret = EspSeqSet(rec, &w->info, &p->seed, p->model, &p->mtx, 0, 0.0f, &esp, p->pOpt, pos);
                }
                if (ret) {
                    if (bScale) {
                        esp->sizeX *= scaleR;
                        esp->sizeY *= scaleR;
                    }
                    if (bSpd) {
                        PSVECScale(&esp->spd, &esp->spd, spdR);
                    }
                    if (bCol) {
                        f32 a = (f32) (int) esp->x83 * colR;

                        if (a > 255.0f) {
                            a = 255.0f;
                        }
                        esp->x83 = (u8) a;
                        esp->colA *= colR;
                    }
                }
            }
        }
    } else {
        p->waitCnt--;
    }
    p->cnt++;
    if (p->life != 0 && p->life <= p->cnt) {
        PushEspgen(w);
    }
}

void espgen00_Move00(EspgenWork* w)
{
    espgen00_Update(w);
    w->step = 1;
}

void espgen00_Move01(EspgenWork* w)
{
    espgen00_Update(w);
}

void Espgen00_Move(EspgenWork* w)
{
    static void (*Espgen00MoveTbl[])(EspgenWork*) = {espgen00_Move00, espgen00_Move01};
    cModel* model = ((Espgen00Work*) w->work)->model;

    if (model != NULL && (pG->flags_5010 & 0x10000000)) {
        int susp = !(model->be_flag & 0x800);
        if (susp) {
            return;
        }
    }
    Espgen00MoveTbl[w->step](w);
}

int Espgen00_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8, int flag)
{
    Espgen00Work* p = (Espgen00Work*) w->work;

    p->rec = rec;
    p->model = model;
    if (model != NULL) {
        p->serial = model->serial;
    } else {
        p->serial = (u32) model;
    }
    p->waitCnt = p->cnt = 0;
    p->life = rec->x110;
    p->wait = rec->x10C;
    p->num = rec->x10D;
    p->scaleD = rec->x124;
    p->spdD = rec->x125;
    p->colD = rec->x126;
    p->waitD = rec->x127;
    p->flags = rec->x10B;
    p->waitRnd = rec->x128;
    if (p->waitRnd) {
        p->wait += (u32) Rnd() % p->waitRnd;
    }
    if (head->flags & 1) {
        p->flags2 |= 2;
    }
    if (flag == 1) {
        p->flags2 |= 4;
    }
    p->parts = parts;
    p->pos = *pos;
    p->rot = *rot;
    if (p->flags & 2) {
        p->seed = 0x12345678 + rec->x10E;
    } else {
        p->seed = Rnd() | (Rnd() << 8) | (Rnd() << 16);
    }
    PSMTXCopy(*mtx, p->mtx);
    if (p8 != NULL) {
        p->pOpt = &p->opt;
        p->opt = *p8;
    } else {
        p->pOpt = p8;
    }
    return 1;
}
