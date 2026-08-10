// game/foot_shadow.cpp: character foot shadows cast by the type 4 lights.

#include "light.h"
#include "atari.h"
#include "global.h"
#include "model.h"
#include "em.h"
#include "esp.h"
#include "camera.h"
#include "math_sub.h"
#include "foot_shadow.h"

// cLight::work of a type 4 (foot shadow) light.
struct FootLightWork {
    u8 x0;
    u8 x1;
    u8 mode;    // 0x02  5: casts foot shadows
    s8 height;  // 0x03  shadow height offset (* 10 + 50)
    s16 rotX;   // 0x04  direction (degrees), xD 1 / 2
    s16 rotY;   // 0x06
    u8 angle;   // 0x08  spot half angle (degrees), xD 2; 0 = 90
};

// One shadow quad.
struct ShadowInfo {
    Vec pos;
    f32 size;
    f32 alpha;
};

// Attenuation of a shadow at `pos` for a light at `lpos` with squared range `range`.
static inline f32 shadowRate(Vec* pos, Vec* lpos, f32 range)
{
    f32 d = PSVECSquareDistance(pos, lpos) / range;
    return 1.0f - d * d;
}

void DrawFootShadow(cEm* em)
{
    Vec dir;
    Vec lpos;
    Vec pos;
    cLight* l;
    int cnt;

    if (pG->flags_58 & 0x02000000) {
        return;
    }
    if (em->shdCol == 0xFF) {
        return;
    }
    if (pG->flags_500C & 0x1000) {
        pos = em->pParts->worldPos;
        pos.y = SatMgr.getFloor(&pos, 600.0f, 100000.0f, 0, 0);
    } else {
        pos = em->pos;
    }

    l = LightMgr.pAlive;
    cnt = 0;
    while (l) {
        FootLightWork* w;
        f32 rate;
        f32 range;

        if (cnt != 0) {
            l = (cLight*) l->next;
            if (l == 0) {
                break;
            }
        } else {
            cnt++;
        }
        w = (FootLightWork*) l->work;
        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (l->type != 4) {
            continue;
        }
        if (!(l->xF & em->lightInfo.x50)) {
            continue;
        }
        if (pG->flags_500C & 0x80) {
            if (l->kind & 0x80) {
                continue;
            }
        }
        if (w->mode != 5) {
            continue;
        }
        {
            int col = l->color.r;
            rate = (f32) col / 255.0f;
        }
        if (l->x1C == 0.0f) {
            range = 1000000000.0f;
        } else {
            range = l->x1C;
        }
        l->getPos(&lpos);
        range *= range;
        if (PSVECSquareDistance(&pos, &lpos) > range) {
            continue;
        }
        if (l->xD == 1 || l->xD == 2) {
            Vec rot;
            Vec axis = {0.0f, 1.0f, 0.0f};
            Mtx m1;
            Mtx m2;

            dir.x = 0.0f;
            dir.y = -1.0f;
            dir.z = 0.0f;
            rot.x = (f32) w->rotX * 6.2831855f / 360.0f;
            rot.y = (f32) w->rotY * 6.2831855f / 360.0f;
            rot.z = 0.0f;
            PSMTXRotRad(m1, 'x', rot.x);
            PSMTXRotAxisRad(m2, &axis, rot.y);
            PSMTXConcat(m2, m1, m1);
            PSMTXMultVecSR(m1, &dir, &dir);
        } else if (l->xD == 0) {
            PSVECSubtract(&em->pParts->worldPos, &lpos, &dir);
#line 152 "D:/Bio4/Prog/foot_shadow.cpp"
            VECNormalize(&dir, &dir);
        }
        if (l->xD == 2) {
            Vec tmp;
            f32 dot;
            f32 dist;
            f32 ang;
            f32 d;

            PSVECSubtract(&em->pParts->worldPos, &lpos, &tmp);
            dot = PSVECDotProduct(&dir, &tmp);
            PSVECScale(&dir, &tmp, dot);
            PSVECAdd(&lpos, &tmp, &tmp);
            dist = PSVECSquareDistance(&pos, &tmp);
            ang = (f32) w->angle;
            if (ang == 0.0f) {
                ang = 90.0f;
            }
            d = sinf(ang * (PI / 180.0f)) * dot;
            if (dist > d) {
                continue;
            }
            range *= dist / d;
        }
        {
            FootShadowTbl* tbl = (FootShadowTbl*) em->pFootShadowTbl;
            ShadowInfo prev;
            ShadowInfo info;
            GXTexObj* tex;
            u32 i;
            int prevOn;
            u32 prevCnt;

            if (EspChkTexId(0x13) == 0) {
                return;
            }
            tex = EspGetTexObj(0x13, 0);
            rate *= (f32) (255 - em->shdCol) / 255.0f;
            prevOn = 0;
            prevCnt = 0;
            for (i = 0; i < tbl->num; i++) {
                FootShadowDat* dat = &tbl->dat[i];
                cModel* p = em->getPartsPtr(dat->parts);
                ShadowInfo mid;
                Vec ofs;

                {
                    f32 size;

                    info.pos = p->worldPos;
                    PSVECScale(&dir, &ofs, -(info.pos.y - pos.y) * (1.0f / dir.y));
                    PSVECAdd(&info.pos, &ofs, &info.pos);
                    info.pos.y += (f32) w->height * 10.0f + 50.0f;
                    size = dat->size;
                    info.size = size;
                    info.alpha = (f32) dat->alpha;
                    drawShadowParts(tex, &info.pos, size, info.alpha * rate * shadowRate(&info.pos, &lpos, range));
                }
                if (prevOn) {
                    if (prevCnt == 1) {
                        f32 size;

                        PSVECAdd(&prev.pos, &info.pos, &mid.pos);
                        PSVECScale(&mid.pos, &mid.pos, 0.5f);
                        size = (prev.size + info.size) * 0.5f;
                        mid.size = size;
                        mid.alpha = (prev.alpha + info.alpha) * 0.5f;
                        drawShadowParts(tex, &mid.pos, size, mid.alpha * rate * shadowRate(&mid.pos, &lpos, range));
                    } else if (prevCnt != 0) {
                        Vec step;
                        Vec diff;
                        f32 t;
                        f32 dt;
                        f32 s;
                        f32 size;
                        u32 k;

                        dt = 1.0f / (f32) (prevCnt + 1);
                        t = 0.0f;
                        PSVECSubtract(&info.pos, &prev.pos, &diff);
                        for (k = 0; k < prevCnt; k++) {
                            t += dt;
                            PSVECScale(&diff, &step, t);
                            s = 1.0f - t;
                            PSVECAdd(&prev.pos, &step, &mid.pos);
                            size = prev.size * s + info.size * t;
                            mid.size = size;
                            mid.alpha = prev.alpha * s + info.alpha * t;
                            drawShadowParts(tex, &mid.pos, size, mid.alpha * rate * shadowRate(&mid.pos, &lpos, range));
                        }
                    }
                }
                if (dat->flag & 1) {
                    prev = info;
                    prevOn = 1;
                    prevCnt = dat->div;
                } else {
                    prevOn = 0;
                }
            }
        }
    }
}

void drawShadowParts(GXTexObj* tex, Vec* pos, f32 size, f32 alpha)
{
    GXColor col;
    u8 a;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    a = (u8) alpha;
    GXSetNumChans(1);
    GXSetChanCtrl(4, 1, 0, 0, 0, 0, 2);
    {
        s8 c = a;
        col.a = a;
        col.r = col.g = col.b = c;
    }
    GXSetChanAmbColor(4, col);
    GXSetChanMatColor(4, col);
    Mtx m;
    PSMTXIdentity(m);
    GXLoadTexObj(tex, 0);
    GXLoadTexMtxImm(m, 0x1E, 1);
    GXSetTexCoordGen(0, 1, 4, 0x1E);
    GXSetNumTexGens(1);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 4, 5, 7);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    PSMTXInverse(pG->Cam.viewMat, m);
    PSMTXTranspose(m, m);
    GXLoadNrmMtxImm(m, 0);
    GXLoadPosMtxImm(pG->Cam.viewMat, 0);
    GXSetCurrentMtx(0);
    CameraCurrentProjection();
    GXSetBlendMode(1, 4, 5, 0);
    GXSetCullMode(0);
    GXSetZMode(1, 3, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(pos->x - size, pos->y, pos->z - size);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(pos->x + size, pos->y, pos->z - size);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(pos->x + size, pos->y, pos->z + size);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(pos->x - size, pos->y, pos->z + size);
    GXTexCoord2f32(0.0f, 1.0f);
}
