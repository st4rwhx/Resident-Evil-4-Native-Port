// game/mirror.cpp: draws a model as a mirror surface, projecting the render-to-texture
// result (TexRenderMng 0) onto it (TexRenderModAddOtMirror queues MirrorDraw2).

#include "atari.h"
#include "light.h"
#include "global.h"
#include "model.h"
#include "gx.h"
#include "TexRender.h"

// game/trans.cpp
int commonScreenMat(cModel* m);

static void mirrorModelTrans2(cModel* m, cModelInfo* info, Mtx viewMat);

void MirrorDraw2(cModel* m)
{
    if (m == 0) {
        return;
    }
    commonScreenMat(m);
    mirrorModelTrans2(m, m->pInfo, pG->Cam.viewMat);
}

static void mirrorModelTrans2(cModel* m, cModelInfo* info, Mtx viewMat)
{
    GxStageWork* st = &pG->gxStage;
    GXColor white;
    GXColor col;
    u32 i;
    u16 nParts;
    ModelPart* part;
    ModelData* d;

    st->tevStage = 0;
    st->texMap = 0;
    st->texCoord = 0;
    GXSetAlphaCompare(4, 0, 1, 4, 0xFF);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetZMode(0, 7, 1);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
    white.r = white.g = white.b = white.a = 0xFF;
    GXSetChanAmbColor(0, white);
    GXSetChanMatColor(0, white);
    Mtx mv;
    Mtx inv;
    Mtx nrm;
    Mtx tex;
    Mtx proj;
    PSMTXConcat(viewMat, m->pParts->mat, mv);
    PSMTXInverse(mv, inv);
    PSMTXTranspose(inv, nrm);
    GXLoadPosMtxImm(mv, 0);
    GXLoadNrmMtxImm(nrm, 0);
    GXSetCurrentMtx(0);

    for (; info != 0; info = info->pNext) {
        d = info->pData;
        void* texArr = d->pTex;
        GXClearVtxDesc();
        GXSetVtxDesc(9, 3);
        GXSetVtxDesc(10, 3);
        GXSetVtxDesc(13, 3);
        GXSetVtxAttrFmt(0, 10, 0, 3, 14);
        if ((s32) d->flags < 0) {
            void* clrArr = d->pClr;
            GXSetVtxAttrFmt(0, 13, 1, 3, 8);
            GXSetVtxDesc(11, 3);
            GXSetVtxAttrFmt(0, 11, 1, 5, 0);
            GXSetArray(11, clrArr, 4);
        } else {
            GXSetVtxAttrFmt(0, 13, 1, 2, 15);
        }
        GXSetArray(9, info->pPosBuf[pG->vtx_buf_no], 6);
        GXSetArray(10, info->pNrmBuf[pG->vtx_buf_no], 6);
        GXSetArray(13, texArr, 4);
        GXSetVtxAttrFmt(0, 9, 1, 3, d->shift);
        if (d->x18 == 1 && d->x2A <= 0xFF && !(info->be_flag & 2) && d->x19 == 1) {
            GXSetArray(9, d->vtxOrig, 8);
            GXSetArray(10, d->nrmOrig, 8);
        }
        GXSetCullMode(1);
        GXLoadTexObj(&GetTexRenderMgrAddr(0)->texObj, st->texMap);
        C_MTXLightPerspective(proj, pG->Cam.param.fovy, 1.3333334f, 0.5f, -0.5f, 0.5f, 0.5f);
        PSMTXConcat(proj, mv, tex);
        GXLoadTexMtxImm(tex, 0x1E, 0);
        GXSetTexCoordGen2(st->texCoord, 0, 0, 0x1E, 0, 0x7D);
        GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
        GXSetTevColorIn(st->tevStage, 0xF, 8, 0xC, 0xF);
        GXSetTevColorOp(st->tevStage, 0, 0, 1, 1, 0);
        GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
        GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
        st->tevStage++;
        st->texMap++;
        st->texCoord++;
        GXSetNumTevStages(st->tevStage);
        GXSetNumTexGens(st->texCoord);
        nParts = d->nParts;
        part = d->pParts;
        for (i = 0; i < nParts; i++) {
            if (m->alpha < 1.0f) {
                col = *(GXColor*) info->color;
                col.a = col.a * m->alpha;
                GXSetChanMatColor(4, col);
            }
            {
                u8* p = (u8*) part + 0x20;
                GXCallDisplayList(p, part->size);
                part = (ModelPart*) (p + part->size);
            }
        }
    }
}
