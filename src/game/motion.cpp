#include "motion.h"
#include "global.h"
#include "db_log.h"
#include "math_sub.h"
#include "main_mem.h"
#include "player.h"
#include "eprintf.h"

extern const Vec vecZero;
extern "C" void* memset(void* dst, int c, unsigned int n);

// Matrix copy written out as loops (the original never calls PSMTXCopy for these).
// Shape matters (all four sites byte-identical only this way): dst pointer first, the row
// counter `i_ = 2` between the two pointers, `for (; i_ != -1; i_--)` (a `while (i_--)` leaves
// the folded `li 2` behind the source pointer), `d_++` before `s_++` (gcse numbers the
// hoisted `+16` pseudos in that order).
#define MTX_COPY(src, dst)               \
    {                                    \
        MtxPtr d_ = (dst);               \
        int i_ = 2;                      \
        MtxPtr s_ = (src);               \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        for (; i_ != -1; i_--) {         \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        }                                \
    }

// Root key history for the current flip state: hist[flip][rot = 0 / pos = 1]
#define MOT_HIST(w, flip, n) ((u16*) ((u8*) (w) + ((flip) * 12 + 8 + (n) * 6)))

// Fixed 10.6 sequence frame -> float
#define SEQ_FRAME(k) ((f32)((k) >> 6) + (f32)((k) & 0x3F) * 0.015625f)

typedef void (*FccGetData)(u8* data, int i0, int i1, f32* val, f32* tan);

extern "C" {
void Fcc_get_data_000(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_001(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_002(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_010(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_011(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_012(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_020(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_021(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_022(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_033(u8* d, int i0, int i1, f32* v, f32* t);
void dummy(u8* d, int i0, int i1, f32* v, f32* t);
}

void PartsWorldPosCalc(cModel* m)
{
    cModel* p;
    Vec d;

    p = m->pParts;
    PSVECSubtract(&m->pos, &MOTION(m)->basePos, &d);
    m->mat[0][3] += d.x;
    m->mat[1][3] += d.y;
    m->mat[2][3] += d.z;
    if (PSVECMag(&d) != 0.0f) {
        for (; p != 0; p = p->pParts) {
            PSVECAdd(&p->worldPos, &d, &p->worldPos);
            p->mat[0][3] += d.x;
            p->mat[1][3] += d.y;
            p->mat[2][3] += d.z;
        }
        MOTION(m)->basePos = m->pos;
    }
}

void MotionBlendOff(cModel* m)
{
    MOTION(m)->blend = 0;
}

void MotionPause(cModel* m)
{
    MOTION(m)->flags |= 8;
}

void MotionClear(cModel* m, int flag)
{
    MotionWork* w = MOTION(m);
    cModel* p;
    Mtx tmp;
    Mtx inv;
    int i;
    int j;

    for (p = m->pParts; p != 0; p = p->pParts) {
        if (m != p->pParent) {
            PSMTXInverse(PARTS_BIND_MAT(p), tmp);
            PSMTXConcat(PARTS_BIND_MAT(p->pParent), tmp, inv);
        } else {
            PSMTXInverse(PARTS_BIND_MAT(p), inv);
        }
        p->pos.x = inv[0][3];
        p->pos.y = inv[1][3];
        p->pos.z = inv[2][3];
        p->rot.x = 0.0f;
        p->rot.y = 0.0f;
        p->rot.z = 0.0f;
        if (!(flag & 1)) {
            p->scale.x = 1.0f;
            p->scale.y = 1.0f;
            p->scale.z = 1.0f;
        }
    }
    if (w->cam != 0) {
        for (i = 0; i < 5; i++) {
            w->cam->parts[i] = 0xFF;
        }
        w->cam->type = 0;
        for (j = 0; j < 5; j++) {
            memclr_asm(&w->cam->out[j], sizeof(Vec));
        }
    }
    MOTION(m)->data = 0;
}

void MotionSetCore(cModel* m, void* w_, void* data_, int seq_, int hokan, int flags, int frame)
{
    MotionWork* w = (MotionWork*) w_;
    MotionData* data = (MotionData*) data_;
    u16* seq = (u16*) seq_;
    HermitePrm prm;
    HermitePrm* pp = &prm;
    u16 hist0[3] = { 0, 0, 0 };
    u16 hist1[3] = { 0, 0, 0 };
    Vec v0;
    Vec v1;
    Vec v2;
    u32* tbl;
    cModel* p;
    AttachCamera* cam;
    int f;
    int i;

    if (!(w->flags2 & 0x20000000)) {
        MOTION(m)->blend = 0;
    }
    w->posPrev = vecZero;
    w->pos = w->posPrev;
    w->posDelta = vecZero;
    w->rotPrev = vecZero;
    w->rot = w->rotPrev;
    w->rotDelta = vecZero;
    w->data = data;
    w->flags = flags;
    w->state = 0;
    w->flags2 = (w->flags2 & 0x7FFFFFFF) | 0x04000000;
    if (data == 0) {
        if ((s32) pG->flags_60 >= 0) {
#line 273
            pLog->err(0, 0, "MotionSetCore():%d pMot == NULL", __LINE__);
        }
        return;
    }
    if (seq == 0) {
        w->seq = 0;
        w->seqMax = data->maxFrame & 0x3FFF;
        if (!(flags & 4)) {
            w->seqMax++;
        }
        if ((u32) frame >= w->seqMax) {
            frame = (u16) (w->seqMax - 1);
        }
        w->key0.x2 = 0;
        w->key0.x3 = 0;
        w->seqFrame = (f32) (u16) frame;
        w->key1.x2 = 0;
        w->key1.x3 = 0;
        w->key2.x2 = 0;
        w->key2.x3 = 0;
    } else {
        w->seq = (MotionSeqKey*) (seq + 2);
        w->seqMax = seq[0];
        if ((u32) frame >= w->seqMax) {
            frame = (u16) (w->seqMax - 1);
        }
        w->seqFrame = (f32) (u16) frame;
        w->flags = (((u8*) seq)[2] & 1) ? (flags | 0x1000) : (flags & 0xEFFF);
    }
    if (w->seq == 0) {
        w->key0.frame = (u16) (w->seqFrame * 64.0f);
        w->key1.frame = (u16) (w->seqFrame * 64.0f);
    } else {
        MotionSeqKey k = w->seq[(u16) w->seqFrame];

        w->key0 = k;
        w->key1 = k;
        w->key2 = k;
        w->key2.x2 = 0;
        w->key2.x3 = 0;
    }
    w->maxFrame = (f32) w->data->maxFrame;
    w->nParts = w->data->nParts;
    w->partsInfo = (u16*) ((u8*) w->data + 3);
    w->partsNo = (u8*) w->data + (w->nParts * 2 + 3);
    if (!(w->flags2 & 0x10000000)) {
        IKInit(m, w);
    }
    // Two statements: the end pointer lives in `tbl` (r10) before the align (one expression ties the
    // partsNo reload to the sum and allocates it).
    tbl = (u32*) ((u32) w->partsNo + w->nParts);
    tbl = (u32*) (((u32) tbl + 3) & ~3);
    tbl++;
    if ((s32) tbl[0] >= 0) {
        for (i = 0; i < w->nParts; i++) {
            tbl[i] += (u32) w->data;
        }
    }
    w->keyTbl = tbl;
    w->rootRotIdx = 0xFFFF;
    w->rootPosIdx = 0xFFFF;
    if (w->cam != 0) {
        for (i = 0; i < 5; i++) {
            w->cam->parts[i] = 0xFF;
        }
        w->cam->type = 0;
        for (i = 0; i < 5; i++) {
            memclr_asm(&w->cam->out[i], sizeof(Vec));
        }
    }
    for (i = 0; i < w->nParts; i++) {
        u16 info = w->partsInfo[i];
        int kind = info & 0xFF;
        int ch = (info >> 8) & 0xF;
        u8 pno;

        if (kind == 1) {
            w->rootPosIdx = i;
            continue;
        }
        if (kind == 0x40) {
            w->rootRotIdx = i;
            continue;
        }
        if (w->cam == 0) {
            continue;
        }
        if (ch != 6 && ch != 7) {
            continue;
        }
        if (w->flags & 0x100) {
            continue;
        }
        if (ch == 6) {
            w->cam->type = 1;
        } else if (ch == 7) {
            w->cam->type = 2;
        }
        pno = w->partsNo[i];
        if (kind == 4) {
            if (pno == 0) {
                w->cam->parts[0] = i;
            } else if (pno == 1) {
                w->cam->parts[1] = i;
            } else if (pno == 4) {
                w->cam->parts[4] = i;
            }
        } else if (kind == 2) {
            if (pno == 2) {
                w->cam->parts[2] = i;
            } else if (pno == 3) {
                w->cam->parts[3] = i;
            }
        }
    }
    for (p = m->pParts; p != 0; p = p->pParts) {
        if (MOTION_PARTS(p)->flags & 0x04000000) {
            continue;
        }
        memclr_asm(MOTION_PARTS(p)->hist[0], 6);
        memclr_asm(MOTION_PARTS(p)->hist[1], 6);
        memclr_asm(MOTION_PARTS(p)->hist[2], 6);
        memclr_asm(MOTION_PARTS(p)->hist[3], 6);
        memclr_asm(MOTION_PARTS(p)->hist[4], 6);
        memclr_asm(MOTION_PARTS(p)->hist[5], 6);
    }
    w->hist[0][0][0] = w->hist[0][0][1] = w->hist[0][0][2] = 0;
    w->hist[0][1][0] = w->hist[0][1][1] = w->hist[0][1][2] = 0;
    w->hist[1][0][0] = w->hist[1][0][1] = w->hist[1][0][2] = 0;
    w->hist[1][1][0] = w->hist[1][1][1] = w->hist[1][1][2] = 0;
    if (w->cam != 0) {
        memclr_asm(w->cam->hist[0], 6);
        memclr_asm(w->cam->hist[1], 6);
        memclr_asm(w->cam->hist[2], 6);
        memclr_asm(w->cam->hist[3], 6);
        memclr_asm(w->cam->hist[4], 6);
    }
    if (hokan != 0) {
        w->hokanMax = hokan;
        w->hokanCnt = hokan;
        for (p = m->pParts; p != 0; p = p->pParts) {
            PSMTXCopy(p->worldMat, p->prevMat);
        }
    } else {
        w->hokanMax = 0;
        w->hokanCnt = 0;
    }
    w->state = 0;
    if (w->flags & 2) {
        f = frame + 1;
        if (f >= w->seqMax) {
            f = w->seqMax - 1;
        }
    } else {
        f = frame - 1;
        if (f < 0) {
            f = 0;
        }
    }
    u32 zero = 0;
    pp->flags = zero;
    pp->maxFrame = w->maxFrame;
    if (w->flags & 2) {
        if (!(w->flags & 0x1000)) {
            asm("" : : "r"(zero));  // COMPILER-DIFF: dead use makes the zero global (r11), w->flags takes r0
            pp->flags = 2;
        }
    } else {
        if (w->flags & 0x1000) {
            pp->flags = 2;
        } else {
            pp->flags = 0;
        }
    }
    if (w->seq == 0) {
        w->frame = (f32) f;
    } else {
        u16 k = w->seq[f].frame;
        w->frame = (f32) (k >> 6) + (f32) (k & 0x3F) * 0.0015625f;
    }
    w->prevFrame = w->frame;
    w->prevFrame2 = w->frame;
    pp->frame = w->frame;
    if (w->rootPosIdx != 0xFFFF) {
        pp->type = w->partsInfo[w->rootPosIdx] >> 12;
        pp->key = (u8*) w->keyTbl[w->rootPosIdx];
        HermiteInterpolation(pp, &w->pos, hist0);
        w->posPrev = w->pos;
    }
    if (w->rootRotIdx != 0xFFFF) {
        pp->type = w->partsInfo[w->rootRotIdx] >> 12;
        pp->key = (u8*) w->keyTbl[w->rootRotIdx];
        HermiteInterpolation(pp, &w->rot, hist1);
        w->rotPrev = w->rot;
    }
    if (w->rootPosIdx != 0xFFFF) {
        pp->frame = 0.0f;
        pp->type = w->partsInfo[w->rootPosIdx] >> 12;
        pp->key = (u8*) w->keyTbl[w->rootPosIdx];
        HermiteInterpolation(pp, &v0, hist0);
        pp->frame = w->maxFrame;
        pp->flags |= 2;
        HermiteInterpolation(pp, &v1, hist0);
        PSVECSubtract(&v1, &v0, &w->posDelta);
    }
    if (w->rootRotIdx != 0xFFFF) {
        pp->frame = 0.0f;
        pp->type = w->partsInfo[w->rootRotIdx] >> 12;
        pp->key = (u8*) w->keyTbl[w->rootRotIdx];
        HermiteInterpolation(pp, &v0, hist1);
        pp->frame = w->maxFrame;
        pp->flags |= 2;
        HermiteInterpolation(pp, &v1, hist1);
        PSVECSubtract(&v1, &v0, &w->rotDelta);
        VecRadLimit(&w->rotDelta);
    }
    cam = w->cam;
    if (cam != 0) {
        if (cam->type != 0) {
            if (cam->parts[4] != 0xFF) {
                pp->frame = 0.0f;
                pp->type = w->partsInfo[cam->parts[4]] >> 12;
                pp->key = (u8*) w->keyTbl[cam->parts[4]];
                HermiteInterpolation(pp, &v2, hist0);
            }
            cam->frame = 0;
            asm volatile("" : : : "memory");  // COMPILER-DIFF: anchor, the type load stays after the frame store
            if (cam->type == 1) {
                cam->pMat = &m->mat;
            } else if (cam->type == 2) {
                MTX_COPY(m->mat, cam->mat);
                cam->pMat = &cam->mat;
            }
        }
        if (w->cam != 0) {
            if (w->cam->type != 0) {
                CamCtrl.registAttachCamera(w->cam, m);
            } else {
                CamCtrl.deleteAttachCamera(w->cam, m);
            }
        }
    }
}

u16 MotionMove(cModel* m)
{
    static int new_add = 1;
    cModel* p;
    Vec spd;
    Vec rot;
    Vec spd2;
    Vec rot2;
    f32 rate;
    f32 inv;

    if (MOTION(m)->blend != 0) {
        MOTION(m)->blend->speedRate = MOTION(m)->speedRate;
    }
    if (MOTION(m)->flags & 1) {
        MotionGetSpeed(m, MOTION(m), 0, &spd, &rot);
        if (MOTION(m)->blend != 0) {
            MOTION(m)->blend->flags2 |= 0x08000000;
            rate = MOTION(m)->blend->blendRate;
            if (!(MOTION(m)->blend->flags2 & 0x80000000)) {
                if (rate != 0.0f) {
                    MOTION(m)->blend->hokanCnt = 0;
                    MotionGetSpeed(m, MOTION(m)->blend, 0, &spd2, &rot2);
                    inv = 1.0f - rate;
                    VecLinearCombination(&spd2, &spd, rate, inv, &spd);
                    VecLinearCombination(&rot2, &rot, rate, inv, &rot);
                }
            } else {
                if (rate != 0.0f) {
                    MOTION(m)->blend->hokanCnt = 0;
                    MotionGetSpeed(m, MOTION(m)->blend, 0, &spd2, &rot2);
                    VecLinearCombination(&spd2, &spd, rate, 1.0f, &spd);
                    VecLinearCombination(&rot2, &rot, rate, 1.0f, &rot);
                }
            }
        }
        MotionAddSpeed(m, MOTION(m), &spd, &rot);
    }
    MotionMoveCore(m, MOTION(m), 0);
    MotionSequenceCtrl(MOTION(m));
    m->partsMatCalc();
    MOTION(m)->flags &= ~0x2000;
    if (MOTION(m)->blend != 0) {
        MOTION(m)->blend->flags2 |= 0x08000000;
        rate = MOTION(m)->blend->blendRate;
        if ((s32) MOTION(m)->blend->flags2 >= 0) {
            if (rate != 0.0f) {
                MotionMoveCore(m, MOTION(m)->blend, 0);
                MotionSequenceCtrl(MOTION(m)->blend);
                cModel_matBlend(m, MOTION(m)->blend->blendRate);
            } else {
                MotionSequenceCtrl(MOTION(m)->blend);
            }
        } else {
            MOTION(m)->flags |= 0x2000;
            for (p = m->pParts; p != 0; p = p->pParts) {
                if (MOTION_PARTS(p)->flags & 0x03000000) {
                    continue;
                }
                MOTION_PARTS(p)->pos = p->pos;
                MOTION_PARTS(p)->rot = p->rot;
                MOTION_PARTS(p)->scale = p->scale;
                memclr_asm(&p->rot, sizeof(Vec));
                memclr_asm(&p->pos, sizeof(Vec));
                if (new_add) {
                    memclr_asm(&p->scale, sizeof(Vec));
                }
            }
            MotionMoveCore(m, MOTION(m)->blend, 0);
            MotionSequenceCtrl(MOTION(m)->blend);
            for (p = m->pParts; p != 0; p = p->pParts) {
                if (MOTION_PARTS(p)->flags & 0x03000000) {
                    continue;
                }
                if (new_add) {
                    PSVECAdd(&MOTION_PARTS(p)->pos, &p->pos, &p->pos);
                    PSVECAdd(&MOTION_PARTS(p)->rot, &p->rot, &p->rot);
                    PSVECAdd(&MOTION_PARTS(p)->scale, &p->scale, &p->scale);
                } else {
                    Vec rotAdd;
                    Vec rotScl;
                    Vec posAdd;
                    Vec posScl;

                    PSVECScale(&p->rot, &rotScl, rate);
                    PSVECScale(&p->pos, &posScl, rate);
                    PSVECAdd(&MOTION_PARTS(p)->pos, &posScl, &posAdd);
                    PSVECAdd(&MOTION_PARTS(p)->rot, &rotScl, &rotAdd);
                    p->pos = posAdd;
                    p->rot = rotAdd;
                    RotMatrix(p->worldMat, &p->rot);
                    TransMatrix(p->worldMat, &p->pos);
                    ScaleMatrix(p->worldMat, &p->scale);
                    PSMTXCopy(p->worldMat, p->mat);
                }
            }
            if (new_add) {
                cModel_matBlend(m, MOTION(m)->blend->blendRate);
            }
        }
    }
    {
        Mtx tmp;
        Mtx save;

        MTX_COPY(m->mat, save);
        if (m->scale.x != 0.0f || m->scale.y != 0.0f || m->scale.z != 0.0f) {
            PSMTXScale(tmp, 1.0f / m->scale.x, 1.0f / m->scale.y, 1.0f / m->scale.z);
        } else {
            PSMTXIdentity(tmp);
        }
        PSMTXConcat(m->mat, tmp, tmp);
        MTX_COPY(tmp, m->mat);
        m->partsWorldCalc();
        InverseKinematics(m, 1);
        MTX_COPY(save, m->mat);
        m->partsWorldCalc();
    }
    if (MOTION(m)->hokanCnt != 0) {
        MotionHokan(m, MOTION(m));
        m->partsWorldCalc();
    }
    if (MOTION(m)->blendTbl != 0) {
        int n = *(s32*) MOTION(m)->blendTbl;
        u16* tbl = MOTION(m)->blendTbl + 2;
        int i;

        {
            Mtx m0;
            Mtx m1;
            Mtx mq;
            Quaternion q0;
            Quaternion q1;
            Quaternion q2;
            Mtx inv;
            cModel* dst;
            cModel* a;
            cModel* c;
            int per;
            f32 r;

            for (i = 0; i < n; i++) {
                dst = m->getPartsPtr(*tbl++);
                a = m->getPartsPtr(*tbl++);
                c = m->getPartsPtr(*tbl++);
                per = *tbl++;
                r = (f32) per / 100.0f;
                if (MOTION_PARTS(dst)->flags & 0x10000) {
                    MOTION_PARTS(dst)->flags &= ~0x10000;
                    continue;
                }
                MOTION_PARTS(dst)->flags &= ~0x10000000;
                PSMTXIdentity(m0);
                m0[0][0] = a->mat[0][0];
                m0[0][1] = a->mat[0][1];
                m0[0][2] = a->mat[0][2];
                m0[1][0] = a->mat[1][0];
                m0[1][1] = a->mat[1][1];
                m0[1][2] = a->mat[1][2];
                m0[2][0] = a->mat[2][0];
                m0[2][1] = a->mat[2][1];
                m0[2][2] = a->mat[2][2];
                C_QUATMtx(&q0, m0);
                PSMTXIdentity(m1);
                m1[0][0] = c->mat[0][0];
                m1[0][1] = c->mat[0][1];
                m1[0][2] = c->mat[0][2];
                m1[1][0] = c->mat[1][0];
                m1[1][1] = c->mat[1][1];
                m1[1][2] = c->mat[1][2];
                m1[2][0] = c->mat[2][0];
                m1[2][1] = c->mat[2][1];
                m1[2][2] = c->mat[2][2];
                C_QUATMtx(&q1, m1);
                C_QUATSlerp(&q1, &q0, &q2, r);
                PSMTXQuat(mq, &q2);
                dst->mat[0][0] = mq[0][0];
                dst->mat[0][1] = mq[0][1];
                dst->mat[0][2] = mq[0][2];
                dst->mat[1][0] = mq[1][0];
                dst->mat[1][1] = mq[1][1];
                dst->mat[1][2] = mq[1][2];
                dst->mat[2][0] = mq[2][0];
                dst->mat[2][1] = mq[2][1];
                dst->mat[2][2] = mq[2][2];
                dst->prevScale.x = c->prevScale.x * (1.0f - r) + a->prevScale.x * r;
                dst->prevScale.y = c->prevScale.y * (1.0f - r) + a->prevScale.y * r;
                dst->prevScale.z = c->prevScale.z * (1.0f - r) + a->prevScale.z * r;
                ScaleMatrix(dst->mat, &dst->prevScale);
                PSMTXInverse(dst->pParent->mat, inv);
                PSMTXConcat(inv, dst->mat, dst->worldMat);
            }
        }
    }
    m->rot.y = LIMIT_ANGLE(m->rot.y);
    return MOTION(m)->state;
}

u16 MotionMoveSub(cModel* m, MotionWork* w)
{
    MotionMoveCore(m, w, 0);
    MotionSequenceCtrl(w);
    if (w->hokanCnt != 0) {
        MotionHokan(m, w);
    }
    return w->state;
}

void MotionMoveCore(cModel* m, MotionWork* w, int flag)
{
    HermitePrm prm;
    HermitePrm* pp = &prm;
    AttachCamera* cam;
    cModel* p;
    u16* flipTbl = MOTION(m)->flip;
    int n = w->nParts;
    int i = 0;
    int flip;

    if (w->data == 0) {
        return;
    }
    if (!(w->flags & 0x8000)) {
        w->frame = SEQ_FRAME(w->key0.frame);
    } else {
        w->frame = w->seqFrame;
    }
    w->prevFrame2 = w->prevFrame;
    w->prevFrame = w->frame;
    pp->frame = w->frame;
    pp->maxFrame = w->maxFrame;
    pp->flags = 0;
    if (w->flags & 4) {
        if (w->seq == 0) {
            pp->flags = 4;
        }
    }
    if (w->flags & 2) {
        if (!(w->flags & 0x1000)) {
            pp->flags |= 2;
        } else {
            pp->flags &= ~2;
        }
    } else {
        if (w->flags & 0x1000) {
            pp->flags |= 2;
        } else {
            pp->flags &= ~2;
        }
    }
    if (!(w->flags2 & 0x40000000)) {
        RotMatrix(m->mat, &m->rot);
        TransMatrix(m->mat, &m->pos);
        ScaleMatrix(m->mat, &m->scale);
    }
    flip = 0;
    if (w->flags2 & 0x08000000) {
        flip = 1;
    }
    w->flags2 &= ~0x04000000;
    cam = 0;
    if (w->cam != 0 && w->cam->type != 0) {
        cam = w->cam;
    }
    if ((w->flags & 0x40) && flipTbl == 0) {
        w->flags &= ~0x40;
#line 1067
        pLog->err(0, 0, "MotionMoveCore():%d Flip Info Error!", __LINE__);
    }
    do {
        int kind = w->partsInfo[i] & 0xFF;
        u16 info = w->partsInfo[i];
        int ch = (info >> 8) & 0xF;
        int pno = w->partsNo[i];

        if (ch == 6 || ch == 7) {
            if (cam == 0) {
                continue;
            }
            pp->type = info >> 12;
            pp->key = (u8*) w->keyTbl[i];
            if (i == cam->parts[0]) {
                HermiteInterpolation(pp, &cam->out[0], cam->hist[0]);
                if (w->flags & 0x40) {
                    cam->out[0].x = -cam->out[0].x;
                }
            } else if (i == cam->parts[1]) {
                HermiteInterpolation(pp, &cam->out[1], cam->hist[1]);
                if (w->flags & 0x40) {
                    cam->out[1].x = -cam->out[1].x;
                }
            } else if (i == cam->parts[2]) {
                HermiteInterpolation(pp, &cam->out[2], cam->hist[2]);
                if (w->flags & 0x40) {
                    cam->out[2].y = -cam->out[2].y;
                }
            } else if (i == cam->parts[3]) {
                HermiteInterpolation(pp, &cam->out[3], cam->hist[3]);
            } else if (i == cam->parts[4]) {
                HermiteInterpolation(pp, &cam->out[4], cam->hist[4]);
                cam->frame = (u8) (cam->out[4].y / 100.0f);
            }
            continue;
        }
        if (w->flags & 0x40) {
            u16 fp = flipTbl[pno];
            if (fp != 0xFFFF) {
                pno = (s16) fp;
            }
        }
        if (pno >= m->nParts) {
            static const char* kind_str[] = { "Em", "Obj", "Scr", "Shadow", "Mirror" };

            if (pPL == m) {
                pLog->err(0, 0, "MotionMoveCore(): Pl, Invalid parts %d.", pno);
            } else {
                pLog->err(0, 0, "MotionMoveCore(): %s[%0xh], Invalid parts %d. [0x%x]", kind_str[m->x12E], m->id, pno, m);
            }
        }
        p = m->getPartsPtr(pno);
        if (p == 0) {
            continue;
        }
        if (MOTION_PARTS(p)->flags & 0x20000000) {
            continue;
        }
        MOTION_PARTS(p)->flags |= 0x10010000;
        if ((s32) w->flags2 < 0) {
            MOTION_PARTS(p)->flags |= 0x80000000;
        }
        pp->type = w->partsInfo[i] >> 12;
        pp->key = (u8*) w->keyTbl[i];
        if (MOTION_PARTS(p)->flags & 0x04000000) {
            pp->flags |= 8;
        } else {
            pp->flags &= ~8;
        }
        if (kind & 2) {
            HermiteInterpolation(pp, &p->rot, flip ? MOTION_PARTS(p)->hist[3] : MOTION_PARTS(p)->hist[0]);
            VecRadLimit(&p->rot);
            if (w->flags & 0x40) {
                p->rot.y = -p->rot.y;
                p->rot.z = -p->rot.z;
            }
        } else if (kind & 4) {
            HermiteInterpolation(pp, &p->pos, flip ? MOTION_PARTS(p)->hist[4] : MOTION_PARTS(p)->hist[1]);
            if (w->flags & 0x40) {
                p->pos.x = -p->pos.x;
            }
        } else if (kind & 8) {
            HermiteInterpolation(pp, &p->scale, flip ? MOTION_PARTS(p)->hist[5] : MOTION_PARTS(p)->hist[2]);
        } else if (kind & 0x30) {
            HermiteInterpolation(pp, &p->rot, flip ? MOTION_PARTS(p)->hist[3] : MOTION_PARTS(p)->hist[0]);
            VecRadLimit(&p->rot);
            if (w->flags & 0x40) {
                p->rot.y = -p->rot.y;
                p->rot.z = -p->rot.z;
            }
        }
    } while (++i < n);
}

// The caller forms `1.0f - v`: with the subtraction inside the inline body the argument copy
// (a load) precedes the constant load in RTL and sched1 keeps that order (equal prio/weight).
static inline int nearZero(f32 d, f32 eps)
{
    if (fabsf(d) < eps) {
        return 1;
    }
    return 0;
}

void MotionHokan(cModel* m, MotionWork* w)
{
    static int g_scale_cancel = 1;
    static f32 epsilon = 0.00002f;
    static f32 EPS = 0.1f;
    cModel* p;
    Vec pos;
    Quaternion q0;
    Quaternion q1;
    Quaternion q2;
    Mtx m0;
    Mtx m1;
    Vec c0;
    Vec c1;
    Vec c2;
    f32 t;
    f32 u;
    f32 s0, s1, s2;
    f32 n0, n1, n2;
    f32 sx, sz, sy;

    w->hokanCnt--;
    t = (f32) (w->hokanMax - w->hokanCnt);
    t /= (f32) w->hokanMax;
    u = 1.0f - t;
    for (p = m->pParts; p != 0; p = p->pParts) {
        if (!(MOTION(m)->flags & 0x2000)) {
            if (!(MOTION_PARTS(p)->flags & 0x10000000)) {
                continue;
            }
            MOTION_PARTS(p)->flags &= ~0x10000000;
        } else {
            if ((s32) MOTION_PARTS(p)->flags >= 0) {
                continue;
            }
        }
        pos.x = p->prevMat[0][3] * u + p->worldMat[0][3] * t;
        pos.y = p->prevMat[1][3] * u + p->worldMat[1][3] * t;
        pos.z = p->prevMat[2][3] * u + p->worldMat[2][3] * t;
        c0.x = p->prevMat[0][0];
        c0.y = p->prevMat[1][0];
        c0.z = p->prevMat[2][0];
        c1.x = p->prevMat[0][1];
        c1.y = p->prevMat[1][1];
        c1.z = p->prevMat[2][1];
        c2.x = p->prevMat[0][2];
        c2.y = p->prevMat[1][2];
        c2.z = p->prevMat[2][2];
        s0 = PSVECMag(&c0);
        s1 = PSVECMag(&c1);
        s2 = PSVECMag(&c2);
        if (s0 != 0.0f) {
            PSVECScale(&c0, &c0, 1.0f / s0);
        }
        if (s1 != 0.0f) {
            PSVECScale(&c1, &c1, 1.0f / s1);
        }
        if (s2 != 0.0f) {
            PSVECScale(&c2, &c2, 1.0f / s2);
        }
        m0[0][0] = c0.x;
        m0[1][0] = c0.y;
        m0[2][0] = c0.z;
        m0[0][1] = c1.x;
        m0[1][1] = c1.y;
        m0[2][1] = c1.z;
        m0[0][2] = c2.x;
        m0[1][2] = c2.y;
        m0[2][2] = c2.z;
        c0.x = p->worldMat[0][0];
        c0.y = p->worldMat[1][0];
        c0.z = p->worldMat[2][0];
        c1.x = p->worldMat[0][1];
        c1.y = p->worldMat[1][1];
        c1.z = p->worldMat[2][1];
        c2.x = p->worldMat[0][2];
        c2.y = p->worldMat[1][2];
        c2.z = p->worldMat[2][2];
        n0 = PSVECMag(&c0);
        n1 = PSVECMag(&c1);
        n2 = PSVECMag(&c2);
        if (n0 != 0.0f) {
            PSVECScale(&c0, &c0, 1.0f / n0);
        }
        if (n1 != 0.0f) {
            PSVECScale(&c1, &c1, 1.0f / n1);
        }
        if (n2 != 0.0f) {
            PSVECScale(&c2, &c2, 1.0f / n2);
        }
        m1[0][0] = c0.x;
        m1[1][0] = c0.y;
        m1[2][0] = c0.z;
        m1[0][1] = c1.x;
        m1[1][1] = c1.y;
        m1[2][1] = c1.z;
        m1[0][2] = c2.x;
        m1[1][2] = c2.y;
        m1[2][2] = c2.z;
        C_QUATMtx(&q0, m0);
        C_QUATMtx(&q1, m1);
        C_QUATSlerp(&q0, &q1, &q2, t);
        PSMTXQuat(p->worldMat, &q2);
        if (g_scale_cancel) {
            // `one` is loaded before the first scale load (the target's constant load comes first).
            f32 one = 1.0f;
            if (!(nearZero(one - p->scale.x, epsilon) && nearZero(one - p->scale.y, epsilon) && nearZero(one - p->scale.z, epsilon))) {
                if (nearZero(1.0f - p->pParent->prevScale.x * p->scale.x, EPS) && nearZero(1.0f - p->pParent->prevScale.y * p->scale.y, EPS) &&
                    nearZero(1.0f - p->pParent->prevScale.z * p->scale.z, EPS)) {
                    MOTION_PARTS(p)->flags |= 0x20000;
                }
            }
        } else {
            MOTION_PARTS(p)->flags &= ~0x20000;
        }
        if (MOTION_PARTS(p)->flags & 0x20000) {
            // Source order x, y, z: sched1 issues first the load where the pParent pointer dies (the last one).
            sx = 1.0f / p->pParent->scale.x;
            sy = 1.0f / p->pParent->scale.y;
            sz = 1.0f / p->pParent->scale.z;
        } else {
            sx = s0 * u + n0 * t;
            sy = s1 * u + n1 * t;
            sz = s2 * u + n2 * t;
        }
        // Before the products: the store is ready early and wins the LSU slot on LUID (equal priority).
        MOTION_PARTS(p)->flags &= ~0x20000;
        p->worldMat[0][0] *= sx;
        p->worldMat[1][0] *= sx;
        p->worldMat[2][0] *= sx;
        p->worldMat[0][1] *= sy;
        p->worldMat[1][1] *= sy;
        p->worldMat[2][1] *= sy;
        p->worldMat[0][2] *= sz;
        p->worldMat[1][2] *= sz;
        p->worldMat[2][2] *= sz;
        p->prevScale = p->scale;
        p->scale.x = sx;
        p->scale.y = sy;
        p->scale.z = sz;
        TransMatrix(p->worldMat, &pos);
        {
            MtxPtr pm;
            asm("" : "=r"(pm) : "0"(p->prevMat));  // COMPILER-DIFF: launder, the extra insn on the r4 path ranks both addi r4 above mr r3
            PSMTXCopy(p->worldMat, pm);
        }
    }
}

void MotionGetSpeed(cModel* m, MotionWork* w, int flag, Vec* pos, Vec* rot)
{
    HermitePrm prm;
    HermitePrm* pp = &prm;
    Vec a = { 0.0f, 0.0f, 0.0f };
    Vec b = { 0.0f, 0.0f, 0.0f };
    Mtx rm;
    int flip;

    w->frame = SEQ_FRAME(w->key0.frame);
    pp->flags = 0;
    w->rotPrev = w->rot;
    w->posPrev = w->pos;
    if (w->flags & 2) {
        if (w->flags & 0x1000) {
            pp->flags = 0;
        } else {
            pp->flags = 2;
        }
    } else {
        if (w->flags & 0x1000) {
            pp->flags = 2;
        } else {
            pp->flags = 0;
        }
    }
    flip = 0;
    if (w->flags2 & 0x08000000) {
        flip = 1;
    }
    if (w->rootPosIdx != 0xFFFF) {
        pp->frame = w->frame;
        pp->maxFrame = w->maxFrame;
        pp->key = (u8*) w->keyTbl[w->rootPosIdx];
        pp->type = w->partsInfo[w->rootPosIdx] >> 12;
        HermiteInterpolation(pp, &a, MOT_HIST(w, flip, 1));
    }
    if (w->rootRotIdx != 0xFFFF) {
        pp->frame = w->frame;
        pp->maxFrame = w->maxFrame;
        pp->key = (u8*) w->keyTbl[w->rootRotIdx];
        pp->type = w->partsInfo[w->rootRotIdx] >> 12;
        HermiteInterpolation(pp, &b, MOT_HIST(w, flip, 0));
    }
    PSVECSubtract(&b, &w->rotPrev, rot);
    PSVECSubtract(&a, &w->posPrev, pos);
    if (w->state & 1) {
        PSVECAdd(pos, &w->posDelta, pos);
        PSVECAdd(rot, &w->rotDelta, rot);
    } else if (w->state & 2) {
        PSVECSubtract(pos, &w->posDelta, pos);
        PSVECSubtract(rot, &w->rotDelta, rot);
    }
    PSMTXRotRad(rm, 'y', w->rotPrev.y);
    rm[0][2] = -rm[0][2];
    rm[2][0] = -rm[2][0];
    PSMTXMultVecSR(rm, pos, pos);
    if (w->flags & 0x40) {
        if (MOTION(m)->flip == 0) {
            w->flags &= ~0x40;
#line 1642
            pLog->err(0, 0, "MotionMoveCore():%d Flip Info Error!", __LINE__);
        } else {
            rot->y = -rot->y;
            rot->z = -rot->z;
            pos->x = -pos->x;
        }
    }
    if (w->flags & 0x400) {
        int cnt = w->hokanCnt;

        if (cnt - 1 > 0) {
            int mx = w->hokanMax + 1;
            f32 t = (f32) (mx - cnt) / (f32) w->hokanMax;
            f32 u = 1.0f - t;
            pos->x = w->speed.x * u + pos->x * t;
            pos->z = w->speed.z * u + pos->z * t;
        }
    }
    if (!(flag & 8)) {
        w->speed = *pos;
        w->rot = b;
        w->pos = a;
    }
}

void MotionAddSpeed(cModel* m, MotionWork* w, Vec* pos, Vec* rot)
{
    Vec t;

    PSMTXMultVecSR(m->mat, pos, &t);
    PSVECAdd(&m->pos, &t, &m->pos);
    PSVECAdd(&m->rot, rot, &m->rot);
}

void MotionGetPosition(cModel* m, Vec* pos, Vec* rot)
{
    MotionWork* w = MOTION(m);
    HermitePrm prm;
    HermitePrm* pp;
    int flip;

    pos->x = pos->y = pos->z = 0.0f;
    rot->x = rot->y = rot->z = 0.0f;
    w->frame = SEQ_FRAME(w->key1.frame);
    pp = &prm;
    pp->flags = 0;
    asm("" : : "r"(pp));  // COMPILER-DIFF: pp must outrank w for r31
    if (w->flags & 2) {
        if (!(w->flags & 0x1000)) {
            pp->flags = 2;
        }
    } else {
        if (w->flags & 0x1000) {
            pp->flags = 2;
        } else {
            pp->flags = 0;
        }
    }
    flip = 0;
    if (w->flags2 & 0x08000000) {
        flip = 1;
    }
    if (w->rootPosIdx != 0xFFFF) {
        pp->frame = w->frame;
        pp->maxFrame = w->maxFrame;
        pp->key = (u8*) w->keyTbl[w->rootPosIdx];
        pp->type = w->partsInfo[w->rootPosIdx] >> 12;
        HermiteInterpolation(pp, pos, MOT_HIST(w, flip, 1));
    }
    if (w->rootRotIdx != 0xFFFF) {
        pp->frame = w->frame;
        pp->maxFrame = w->maxFrame;
        pp->key = (u8*) w->keyTbl[w->rootRotIdx];
        pp->type = w->partsInfo[w->rootRotIdx] >> 12;
        HermiteInterpolation(pp, rot, MOT_HIST(w, flip, 0));
    }
}

u16 MotionSequenceCtrl(MotionWork* w)
{
    f32 f;

    w->key2 = w->key1;
    if (!(w->flags & 8)) {
        w->state = 0;
        if (w->flags & 2) {
            if (w->flags & 4) {
                if (w->seqFrame <= 0.0f) {
                    w->state = 2;
                    f = w->seqFrame + (f32) (int) w->seqMax;
                } else {
                    f = w->seqFrame - w->speedRate * pG->mot_speed;
                }
                w->seqFrame = f;
            } else {
                if (w->seqFrame <= 0.0f) {
                    w->state = 8;
                    w->seqFrame = 0.0f;
                } else {
                    f = w->seqFrame - w->speedRate * pG->mot_speed;
                    w->seqFrame = f;
                }
            }
        } else {
            w->seqFrame = w->seqFrame + w->speedRate * pG->mot_speed;
            if (w->flags & 4) {
                u16 max = w->seqMax;

                if (w->seqFrame >= (f32) max) {
                    w->state = 1;
                    w->seqFrame = w->seqFrame - (f32) (int) max;
                }
            } else {
                u16 max = w->seqMax;

                if (w->seqFrame >= (f32) max) {
                    w->state = 4;
                    w->seqFrame = (f32) (max - 1);
                }
            }
        }
        w->key1 = w->key0;
        if (w->seq == 0) {
            w->key0.frame = (u16) (w->seqFrame * 64.0f);
        } else {
            // One `sf` variable carries seqFrame, the fraction (`sf -= (f32)(int)fi`) and the
            // else-arm product (`sf *=`): 9 refs rank it above mf and the 0x43300000 double.
            f32 sf = w->seqFrame;
            u16 fi = (u16) sf;

            w->key0 = w->seq[fi];
            if ((f32) fi != sf) {
                u16 nx = (u16) (sf + 1.0f);

                sf -= (f32) (int) fi;
                if (nx >= w->seqMax) {
                    f32 mf = w->maxFrame * 64.0f;

                    if (mf == (f32) (int) w->seq[fi].frame) {
                        w->key0.frame = (u16) (sf * 64.0f);
                    } else if (w->seq[0].frame == 0) {
                        w->key0.frame = w->seq[fi].frame + (u16) (sf * (mf - (f32) (int) w->seq[fi].frame));
                    }
                } else {
                    sf *= (f32) (w->seq[nx].frame - w->seq[fi].frame);
                    w->key0.frame = w->seq[fi].frame + (u16) sf;
                }
            }
            if ((f32) (int) w->key0.frame > w->maxFrame * 64.0f) {
                pLog->err(0, 0, "MotSeqCtrl(@0x%08x): %.2f Invalid Seq. Frame", w, (f32) (int) w->key0.frame * 0.015625f);
            }
        }
    } else {
        w->key1 = w->key0;
        w->key0.x2 = 0;
        w->state &= 0xFFF0;
    }
    return w->state;
}

u16 FcvGetMaxFrame(u16* data)
{
    return data[0];
}

f32 MotionGetMaxFrame(MotionWork* w)
{
    if (w->data == 0) {
        return -1.0f;
    }
    return w->maxFrame;
}

f32 MotionGetCurrentFrame(MotionWork* w)
{
    if (w->data == 0) {
        return -1.0f;
    }
    return w->frame;
}

int MotionCheckCrossFrame(MotionWork* w, f32 frame)
{
    f32 cur;
    f32 prev;

    if (w->data == 0) {
        return 0;
    }
    if (w->flags2 & 0x04000000) {
        return 0;
    }
    cur = w->frame;
    prev = w->prevFrame2;
    if (frame == 0.0f && cur == 0.0f && prev == 0.0f) {
        return 1;
    }
    if (cur >= prev) {
        if (frame <= cur && frame > prev) {
            return 1;
        }
        return 0;
    }
    if (frame > prev || frame <= cur) {
        return 1;
    }
    return 0;
}

int MotionGetState(cModel* m)
{
    MotionWork* w = MOTION(m);

    if (w->data == 0) {
        return -1;
    }
    return w->state;
}

int HermiteInterpolation(HermitePrm* prm, Vec* out, u16* hist)
{
    static FccGetData Fcc_get_data_tbl[16] = {
        Fcc_get_data_000, Fcc_get_data_001, Fcc_get_data_002, dummy,
        Fcc_get_data_010, Fcc_get_data_011, Fcc_get_data_012, dummy,
        Fcc_get_data_020, Fcc_get_data_021, Fcc_get_data_022, dummy,
        dummy,            dummy,            dummy,            Fcc_get_data_033,
    };
    f32 r = 0.0f;
    f32 frame = prm->frame;
    u8* p = prm->key;
    f32* o = (f32*) out;
    u16* hp = hist - 1;
    f32 f0 = r;
    f32 f1 = r;
    int ret = 0;
    int axis = 0;
    u16* frames;
    u8* data;
    f32 val[2];
    f32 tan[2];
    int n;
    int last = 0;
    int idx;
    int cnt;
    int found;

    for (; axis <= 2; axis++) {
        n = *(u16*) p;
        frames = (u16*) (p + 2);
        data = p + n * 2 + 2;
        hp++;
        p = data + Fcc_next_axis_addr(prm->type, n);
        cnt = n;
        found = 0;
        if (prm->maxFrame <= frame) {
            if ((prm->flags & 6) == 4) {
                frame -= prm->maxFrame;
                if (!(prm->flags & 8)) {
                    *hp = 0;
                }
            } else {
                Fcc_get_data_tbl[prm->type](data, n - 1, 0, val, tan);
                cnt = 0;
                found = 1;
                r = val[0];
            }
        }
        if (!(prm->flags & 8)) {
            idx = *hp;
        } else {
            idx = 0;
        }
        {
            // The target keeps `n - 1` in a local (r0) and copies it to `last` BEFORE the compare
            // (gcse's pre_insert_copies shape); a plain copy is coalesced by regmove. The r0 pin plus
            // the codeless use in the error arm keep m live past the compare, so combine cannot fold
            // the copy and regmove's forward scan stops at the branch (COMPILER-DIFF: gcse copy kept).
            register int m asm("r0") = n - 1;
            last = m;
            asm("" : : "r"(last));                    // COMPILER-DIFF: last must outrank n for r30
            if (idx > m) {
                asm("" : : "r"(m));                   // COMPILER-DIFF: keep-alive for the r0 temp
                pLog->err(0, 0, "H.I.(): axis=%d, hist=%d nFrm=%d, Invalid key history.", axis, idx, n);
                idx = 0;
                ret = 1;
            }
        }
        if (cnt != 0) {
            asm("" : "+r"(idx));  // COMPILER-DIFF: the table lis is issued before the fp init
            u16* fp = (u16*) (idx * 2 + (u32) frames);

            do {
                f0 = (f32) *fp;
                if (f0 == frame) {
                    Fcc_get_data_tbl[prm->type](data, idx, 0, val, tan);
                    r = val[0];
                    if (!(prm->flags & 8)) {
                        *hp = idx;
                    }
                    found = 1;
                    break;
                }
                if (f0 < frame) {
                    int nx = idx + 1;

                    if (nx > last) {
                        nx = 0;
                    }
                    f1 = (f32) frames[nx];
                    if (frame < f1) {
                        Fcc_get_data_tbl[prm->type](data, idx, nx, val, tan);
                        if (!(prm->flags & 8)) {
                            *hp = idx;
                        }
                        break;
                    }
                }
                if ((prm->flags & 1) || f0 > frame) {
                    fp--;
                    idx--;
                    if (idx < 0) {
                        fp = (u16*) (last * 2 + (u32) frames);
                        idx = last;
                    }
                } else {
                    idx++;
                    fp++;
                    if (idx > last) {
                        fp = frames;
                        idx = 0;
                    }
                }
            } while (--cnt);
        }
        if (!found) {
            r = hermite(val, tan, (frame - f0) / (f1 - f0));
        }
        o[axis] = r;
    }
    return ret;
}

// Byte-wise big-endian reads of the key data (the streams are unaligned).
typedef union {
    f32 f;
    struct {
        u8 b0;
        u8 b1;
        u8 b2;
        u8 b3;
    } b;
} FccF32;

typedef union {
    s16 s;
    struct {
        u8 hi;
        u8 lo;
    } b;
} FccS16;

#define FCC_F32(dst, i)      \
    cf.b.b0 = d[(i)];        \
    cf.b.b1 = d[(i) + 1];    \
    cf.b.b2 = d[(i) + 2];    \
    cf.b.b3 = d[(i) + 3];    \
    (dst) = cf.f;
#define FCC_S16(dst, i)      \
    cs.b.hi = d[(i)];        \
    cs.b.lo = d[(i) + 1];    \
    (dst) = (f32) cs.s * 0.0001f;
#define FCC_S8(dst, i) (dst) = (f32) (s8) d[(i)] * 0.0001f;

void Fcc_get_data_000(u8* d, int i0, int i1, f32* v, f32* t)
{
    FccF32 cf;

    FCC_F32(v[0], i0 * 12);
    FCC_F32(v[1], i1 * 12);
    FCC_F32(t[0], i0 * 12 + 8);
    FCC_F32(t[1], i1 * 12 + 4);
}

void Fcc_get_data_001(u8* d, int i0, int i1, f32* v, f32* t)
{
    FccF32 cf;
    FccS16 cs;

    FCC_F32(v[0], i0 * 8);
    FCC_F32(v[1], i1 * 8);
    FCC_S16(t[0], i0 * 8 + 6);
    FCC_S16(t[1], i1 * 8 + 4);
}

void Fcc_get_data_002(u8* d, int i0, int i1, f32* v, f32* t)
{
    FccF32 cf;

    FCC_F32(v[0], i0 * 6);
    FCC_F32(v[1], i1 * 6);
    FCC_S8(t[0], i0 * 6 + 5);
    FCC_S8(t[1], i1 * 6 + 4);
}

void Fcc_get_data_010(u8* d, int i0, int i1, f32* v, f32* t)
{
    FccF32 cf;
    FccS16 cs;

    FCC_S16(v[0], i0 * 10);
    FCC_S16(v[1], i1 * 10);
    FCC_F32(t[0], i0 * 10 + 6);
    FCC_F32(t[1], i1 * 10 + 2);
}

void Fcc_get_data_011(u8* d, int i0, int i1, f32* v, f32* t)
{
    static int flag = 0;
    FccS16 cs;

    if (flag) {
        v[0] = (f32) *(s16*) &d[i0 * 6] * 0.0001f;
        v[1] = (f32) *(s16*) &d[i1 * 6] * 0.0001f;
        t[0] = (f32) *(s16*) &d[i0 * 6 + 4] * 0.0001f;
        t[1] = (f32) *(s16*) &d[i1 * 6 + 2] * 0.0001f;
    } else {
        FCC_S16(v[0], i0 * 6);
        FCC_S16(v[1], i1 * 6);
        FCC_S16(t[0], i0 * 6 + 4);
        FCC_S16(t[1], i1 * 6 + 2);
    }
}

void Fcc_get_data_012(u8* d, int i0, int i1, f32* v, f32* t)
{
    FccS16 cs;

    FCC_S16(v[0], i0 * 4);
    FCC_S16(v[1], i1 * 4);
    FCC_S8(t[0], i0 * 4 + 3);
    FCC_S8(t[1], i1 * 4 + 2);
}

void Fcc_get_data_020(u8* d, int i0, int i1, f32* v, f32* t)
{
    FccF32 cf;

    FCC_S8(v[0], i0 * 9);
    FCC_S8(v[1], i1 * 9);
    FCC_F32(t[0], i0 * 9 + 5);
    FCC_F32(t[1], i1 * 9 + 1);
}

void Fcc_get_data_021(u8* d, int i0, int i1, f32* v, f32* t)
{
    FccS16 cs;

    FCC_S8(v[0], i0 * 5);
    FCC_S8(v[1], i1 * 5);
    FCC_S16(t[0], i0 * 5 + 3);
    FCC_S16(t[1], i1 * 5 + 1);
}

void Fcc_get_data_022(u8* d, int i0, int i1, f32* v, f32* t)
{
    FCC_S8(v[0], i0 * 3);
    FCC_S8(v[1], i1 * 3);
    FCC_S8(t[0], i0 * 3 + 2);
    FCC_S8(t[1], i1 * 3 + 1);
}

void Fcc_get_data_033(u8* d, int i0, int i1, f32* v, f32* t)
{
    FccF32 cf;

    FCC_F32(v[0], i0 * 4);
    FCC_F32(v[1], i1 * 4);
}

void dummy(u8* d, int i0, int i1, f32* v, f32* t)
{
}

int Fcc_next_axis_addr(int type, int n)
{
    switch (type) {
    case 0:
        return n * 12;
    case 1:
        return n * 8;
    case 2:
        return n * 6;
    case 3:
        return -1;
    case 4:
        return n * 10;
    case 5:
        return n * 6;
    case 6:
        return n * 4;
    case 7:
        return -1;
    case 8:
        return n * 9;
    case 9:
        return n * 5;
    case 10:
        return n * 3;
    case 11:
        return -1;
    case 12:
        return -1;
    case 13:
        return -1;
    case 14:
        return -1;
    case 15:
        return n * 4;
    }
    return -1;
}

// Debug speed display: dead-stripped by the linker, only the strings, constant pools and statics
// remain. The three zeroed 4-byte statics survive as one unnamed 12-byte .sdata object; this stand-in
// keeps the split label as its name (and the section forced) so strip_unused leaves it in place.
static Vec lbl_80314C44 __attribute__((section(".sdata"))) = { 0.0f, 0.0f, 0.0f };

// The two label pointers at .rodata+0x218 (relocated words) are a function-local static table
// declared AFTER the first eprintf: its strings and the table are assembled when the declaration
// is reached, i.e. after "MOTION SPEED ---" and before the "%s" of the following call.
void MotionSpeedDispHeader(int x, int y, int who)
{
    eprintf(x, y, 0, 0, "MOTION SPEED ---");
    {
        static const char* const who_str[2] = { "GLOBAL: ", "PLAYER: " };
        eprintf(x, y + 10, 0, 0, "%s", who_str[who]);
    }
}

void MotionSpeedDisp(cModel* m, int x, int y)
{
    MotionWork* w = MOTION(m);

    eprintf(x, y, 0, 0, "%s", (char*) w->data);
    lbl_80314C44.x = lbl_80314C44.x * 0.01f + w->speed.x * 10.0f * 0.5f;
    eprintf(x, y + 10, 0, 0, "%.2f", (f32) x * lbl_80314C44.x);
    if (lbl_80314C44.y == 0.0f) {
        lbl_80314C44.z = 2.0f;
    }
}
