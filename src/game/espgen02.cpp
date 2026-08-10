#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "rnd.h"
#include "db_log.h"

struct Espgen02Work;

extern "C" {
void espgen02_UpdateMatrix(EspgenWork* w);
void espgen02_Update(EspgenWork* w);
static void espgen02_Move00(EspgenWork* w);
void espgen02_Move01(EspgenWork* w);
static f32 Calc_D256(Espgen02Work* p, u8 d, f32 rate);
}

// Effect controller 02: like controller 00 but places every emitted esp on a path (path.cpp),
// optionally oriented along it.
struct Espgen02Work {
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
    u8 flags2;         // 0x2D bit0 parts matrix fixed, bit1 head flag, bit2 scale, bit3 pass the position on
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
    u8 pathId;         // 0x9C
    u8 pathNo;         // 0x9D
    u8 pathOfs;        // 0x9E position along the path in 1/100
    u8 pathRnd;        // 0x9F random range added to it
    u16 seg;           // 0xA0 path segment cache
    u8 rotX;           // 0xA2 rotation in 1/256 turns
    u8 rotY;           // 0xA3
    Vec scale;         // 0xA4
    u8 mode;           // 0xB0 bit0: orient along the path, bit1: orient along the path (type 2)
};

void espgen02_UpdateMatrix(EspgenWork* w)
{
    Espgen02Work* p = (Espgen02Work*) w->work;
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
static f32 Calc_D256(Espgen02Work* p, u8 d, f32 rate)
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

// The shared 0.0f is loaded into spdR and copied to scaleR and colR. The FPR order scaleR f25 /
// colR f24 / spdR f23 is a global-alloc live-length knife edge (all three have 6 weighted refs):
// with the copies between the `bScale` and `bSpd` zero stores (the original's sched order) colR
// loses to spdR (f23/f24 swapped); colR is therefore pinned (see the tag below).
void espgen02_Update(EspgenWork* w)
{
    Espgen02Work* p = (Espgen02Work*) w->work;
    f32 scaleR;
    f32 spdR = 0.0f;
    // COMPILER-DIFF: #17. colR pinned to f24 (global-alloc order of the three 0.0f copies); no
    // code is emitted.
    register f32 colR asm("fr24");
    int bScale = 0;
    scaleR = spdR;
    colR = spdR;
    int bSpd = 0;
    int bCol = 0;
    int add = 0;
    cModel* model = p->model;
    Mtx sm;
    Mtx rm;

    if (model != NULL) {
        if ((model->be_flag & 0x201) != 1 || model->serial != p->serial) {
            PushEspgen(w);
            return;
        }
    }
    espgen02_UpdateMatrix(w);
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
        Mtx mtx;
        EspGenWork* rec;
        int n;
        int i;

        PSMTXIdentity(mtx);
        rec = p->rec;
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
            pLog->warn(0, 0, "ESP : num max. retry.[left:%d/need:%d]", g_pEspSys->xC554 - g_pEspSys->xC548,
                       p->num + 1);
            return;
        }
        {
            f32 step = 6.28f / (f32) (p->num + 1);
            f32 ang = 0.0f;

            for (i = 0; i < p->num + 1; i++) {
                Vec pos;
                Vec rot2;
                Mtx m3;
                Mtx m2;
                Vec pos2;
                Vec right;
                Vec up;
                Vec dir;
                Vec dir2;
                cEsp* esp;
                Vec* pp;
                void* path;
                f32 len;
                f32 t;
                f32 d;
                int ret;

                path = EspGetPathAddr(p->pathId, p->pathNo);
                if (path == NULL) {
                    return;
                }
                len = PathGetLength(path);
                t = ((f32) p->pathOfs + fRandSeed0_1(&p->seed) * (f32) (int) p->pathRnd) * 0.01f;
                while (t > 1.0f) {
                    t -= 1.0f;
                }
                while (t < 0.0f) {
                    t += 1.0f;
                }
                d = len * t;
                if (d < 0.0f) {
                    d = 0.0f;
                }
                if (d > len - 1.01f) {
                    d = len - 1.01f;
                }
                if (PathHasWeight(path)) {
                    if (p->model != NULL) {
                        ret = PathGetPosEm(path, d, p->model, &p->seg, &pos);
                    } else {
                        ret = PathGetPos(path, d, &p->seg, &pos);
                    }
                } else {
                    ret = PathGetPos(path, d, &p->seg, &pos);
                }
                if (ret == 0) {
                    pLog->err(0, 0, "ESP_CTRL02 : OUT OF RANGE.");
                }
                PSMTXIdentity(sm);
                if (p->flags2 & 4) {
                    PSMTXScale(sm, p->scale.x, p->scale.y, p->scale.z);
                }
                rot2.x = (f32) p->rotX * 3.1415927f * 2.0f * 0.00390625f;
                rot2.y = (f32) p->rotY * 3.1415927f * 2.0f * 0.00390625f;
                rot2.z = 0.0f;
                RotMatrix(rm, &rot2);
                if (p->mode & 1) {
                    d += 1.0f;
                    if (PathHasWeight(path) && p->model != NULL) {
                        PathGetPosEm(path, d, p->model, &p->seg, &pos2);
                    } else {
                        PathGetPos(path, d, &p->seg, &pos2);
                    }
                    PSVECSubtract(&pos2, &pos, &dir);
                    up.x = 0.0f;
                    up.y = 1.0f;
                    up.z = 0.0f;
#line 284 "D:/Bio4/Prog/espgen02.cpp"
                    VECNormalize(&dir, &dir);
                    PSVECCrossProduct(&dir, &up, &right);
                    PSVECCrossProduct(&right, &dir, &up);
                    m2[0][0] = right.x;
                    m2[0][1] = right.y;
                    m2[0][2] = right.z;
                    m2[0][3] = 0.0f;
                    m2[1][0] = up.x;
                    m2[1][1] = up.y;
                    m2[1][2] = up.z;
                    m2[1][3] = 0.0f;
                    m2[2][0] = dir.x;
                    m2[2][1] = dir.y;
                    m2[2][2] = dir.z;
                    m2[2][3] = 0.0f;
                    PSMTXMultVec(sm, &pos, &pos);
                    PSMTXMultVec(rm, &pos, &pos);
                    PSMTXCopy(p->mtx, m3);
                    m3[0][3] -= rec->x0C + p->mtx[0][3];
                    m3[1][3] -= rec->x10 + p->mtx[1][3];
                    m3[2][3] -= rec->x14 + p->mtx[2][3];
                    PSMTXConcat(m2, m3, m3);
                    PSMTXConcat(rm, m3, m3);
                    m3[0][3] += rec->x0C + p->mtx[0][3];
                    m3[1][3] += rec->x10 + p->mtx[1][3];
                    m3[2][3] += rec->x14 + p->mtx[2][3];
                    m3[0][3] += pos.x;
                    m3[1][3] += pos.y;
                    m3[2][3] += pos.z;
                } else if (p->mode & 2) {
                    if (pG->flags_6C & 0x100) {
                        pLog->warn(0, 0, "ESP : USE CTRL_PATH TYPE=2");
                    }
                    d += 1.0f;
                    if (PathHasWeight(path) && p->model != NULL) {
                        PathGetPosEm(path, d, p->model, &p->seg, &pos2);
                    } else {
                        PathGetPos(path, d, &p->seg, &pos2);
                    }
                    PSVECSubtract(&pos2, &pos, &dir2);
                    up.x = 0.0f;
                    up.y = -1.0f;
                    up.z = 0.0f;
#line 359 "D:/Bio4/Prog/espgen02.cpp"
                    VECNormalize(&dir2, &dir2);
                    PSVECCrossProduct(&up, &dir2, &right);
                    PSVECCrossProduct(&dir2, &right, &up);
                    m2[0][0] = right.x;
                    m2[0][1] = right.y;
                    m2[0][2] = right.z;
                    m2[0][3] = 0.0f;
                    m2[1][0] = up.x;
                    m2[1][1] = up.y;
                    m2[1][2] = up.z;
                    m2[1][3] = 0.0f;
                    m2[2][0] = dir2.x;
                    m2[2][1] = dir2.y;
                    m2[2][2] = dir2.z;
                    m2[2][3] = 0.0f;
                    PSMTXMultVec(sm, &pos, &pos);
                    PSMTXMultVec(rm, &pos, &pos);
                    PSMTXCopy(p->mtx, m3);
                    m3[0][3] -= rec->x0C + p->mtx[0][3];
                    m3[1][3] -= rec->x10 + p->mtx[1][3];
                    m3[2][3] -= rec->x14 + p->mtx[2][3];
                    PSMTXConcat(m2, m3, m3);
                    PSMTXConcat(rm, m3, m3);
                    m3[0][3] += rec->x0C + p->mtx[0][3];
                    m3[1][3] += rec->x10 + p->mtx[1][3];
                    m3[2][3] += rec->x14 + p->mtx[2][3];
                    m3[0][3] += pos.x;
                    m3[1][3] += pos.y;
                    m3[2][3] += pos.z;
                } else {
                    PSMTXMultVec(sm, &pos, &pos);
                    PSMTXMultVec(rm, &pos, &pos);
                    PSMTXCopy(p->mtx, m3);
                    m3[0][3] += pos.x;
                    m3[1][3] += pos.y;
                    m3[2][3] += pos.z;
                }
                pp = NULL;
                if (p->flags2 & 8) {
                    pp = &p->pos;
                }
                if (p->flags & 1) {
                    ret = EspSeqSet(rec, &w->info, &p->seed, p->model, &mtx, 1, ang, &esp, p->pOpt, pp);
                    ang += step;
                } else {
                    ret = EspSeqSet(rec, &w->info, &p->seed, p->model, &mtx, 0, 0.0f, &esp, p->pOpt, pp);
                }
                if (ret) {
                    esp->ApplyMatrix(m3);
                    if (bScale) {
                        esp->sizeX *= scaleR;
                        esp->sizeY *= scaleR;
                    }
                    if (bSpd) {
                        PSVECScale(&esp->spd, &esp->spd, spdR);
                    }
                    if (bCol) {
                        esp->x83 = (u8) ((f32) (int) esp->x83 * colR);
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

static void espgen02_Move00(EspgenWork* w)
{
    espgen02_Update(w);
    w->step = 1;
}

void espgen02_Move01(EspgenWork* w)
{
    espgen02_Update(w);
}

void Espgen02_Move(EspgenWork* w)
{
    static void (*Espgen02MoveTbl[])(EspgenWork*) = {espgen02_Move00, espgen02_Move01};

    Espgen02MoveTbl[w->step](w);
}

int Espgen02_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8, int flag)
{
    Espgen02Work* p = (Espgen02Work*) w->work;

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
        p->flags2 |= 8;
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
    p->pathId = rec->x104;
    p->pathNo = rec->x105;
    p->pathOfs = rec->x106;
    p->pathRnd = rec->x107;
    p->rotX = rec->x129;
    p->rotY = rec->x12A;
    p->mode = rec->x12B;
    if (rec->x118.x != 0.0f || rec->x118.y != 0.0f || rec->x118.z != 0.0f) {
        p->flags2 |= 4;
        p->scale = rec->x118;
        PSVECScale(&p->scale, &p->scale, 0.1f);
        p->scale.x += 1.0f;
        p->scale.y += 1.0f;
        p->scale.z += 1.0f;
    }
    return 1;
}
