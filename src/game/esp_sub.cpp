#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"
#include "espgen.h"
#include "obj.h"
#include "scroll.h"
#include "rnd.h"
#include "main_sub.h"
#include "db_log.h"
#include "tpl.h"
#include "trans_ot.h"

// game/trans_lit.cpp
extern "C" void commonEspLightSet(cLight** list, int n);

int GetDrawTmpBufType();        // game/TmpBuf.cpp (C++ linkage)
extern GXTexObj g_Get_tex_obj;  // game/trans.cpp

extern "C" {
static void EspCommonTransShimmer(cEsp* esp, int type, u32 blur);
void EspCommonTransNega(cEsp* esp, u32 type);
f32 EspGetCameraPan();   // game/esp.cpp
int EspEstSetSelect(int a, int b, int c, cEsp** out, int d);
void GetPosXY(Vec* p0, Vec* p1, Vec* p2, Vec* p3, f32 u, f32 v, Vec* out);
void Esp1b_SpTrans(cEsp* esp);
}

extern f32 ZNEAR;
extern f32 ZFAR;

#define DEG2RAD (3.14f / 180.0f)

// The pulled effect is kept in a one-member struct: the original reloads the pointer from its
// stack slot after every store through it (a struct-member slot aliases the member stores).
struct EspPtr {
    cEsp* p;
};

#define ESP_PARTS_SCREEN(esp) ((s8) (esp)->partsNo >= -8 && (s8) (esp)->partsNo <= -3)

// Texture coordinate corners for the sprite orientation (flags bit1: flip s, bit2: flip t;
// screen sprites are drawn upside down). One combined condition and corners built from a `zero`
// variable: each leaf is a jump target where cse knows neither operand of `zero + z`, which
// keeps the adds (nested ifs with literals fold 0 + z). The flip-s leaves add first, copy after.
#define ESP_FLIP_T(esp)                                                                           \
    ((ESP_PARTS_SCREEN(esp) && !((esp)->flags & 4)) || (!ESP_PARTS_SCREEN(esp) && ((esp)->flags & 4)))
#define ESP_TEXCOORD_SET()                                                                        \
    if (esp->flags & 2) {                                                                         \
        if (ESP_FLIP_T(esp)) {                                                                    \
            s0 = zero + z;                                                                        \
            s1 = zero;                                                                            \
            t0 = s0;                                                                              \
            t1 = s1;                                                                              \
        } else {                                                                                  \
            s0 = zero + z;                                                                        \
            t0 = zero;                                                                            \
            s1 = zero;                                                                            \
            t1 = s0;                                                                              \
        }                                                                                         \
    } else {                                                                                      \
        if (ESP_FLIP_T(esp)) {                                                                    \
            s0 = zero;                                                                            \
            s1 = s0 + z;                                                                          \
            t1 = s0;                                                                              \
            t0 = s1;                                                                              \
        } else {                                                                                  \
            s0 = zero;                                                                            \
            s1 = s0 + z;                                                                          \
            t0 = s0;                                                                              \
            t1 = s1;                                                                              \
        }                                                                                         \
    }

// Shared sprite draw: the sprite quad (g_EspCommonDisplayList) with the effect's texture, an
// optional mask texture in TEV stage 1, screen-space or camera-relative placement.
void EspCommonTrans(cEsp* esp)
{
    static int s_proj_type;
    static int s_tex_no;
    static EspAnmData* s_pAnm;
    static int s_ptn_no;
    Mtx inv;
    f32 sx;
    f32 sy;
    f32 ox;
    f32 oy;

    if (esp->xEC != 0) {
        EspCommonTransShimmer(esp, esp->xED, esp->xEC);
        return;
    }
    if (esp->flags & 0x2000) {
        EspCommonTransNega(esp, 2);
        return;
    }
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    if (OtGetPrevKind() != 8) {
        if (ESP_PARTS_SCREEN(esp)) {
            Mtx44 proj;
            s_proj_type = 0;
            C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
            GXSetProjection(proj, 1);
        } else {
            s_proj_type = 1;
            CameraCurrentProjection();
        }
        if (!EspGetAnmAddr(esp->anmNo, &s_pAnm)) {
            pLog->err(0, 0, "ESP : TexId[%x] no data", esp->anmNo);
            s_tex_no = -1;
            return;
        }
        EspTexSet(esp->anmNo, esp->anmPtn);
        s_tex_no = esp->anmNo;
        s_ptn_no = esp->anmPtn;
        esp->CommonStateSet();
        GXClearVtxDesc();
        GXSetVtxDesc(9, 1);
        GXSetVtxDesc(0xA, 1);
        GXSetVtxDesc(0xD, 1);
        GXSetVtxAttrFmt(0, 9, 1, 1, 0);
        GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
        GXSetVtxAttrFmt(0, 0xD, 1, 1, 0);
    } else {
        if (ESP_PARTS_SCREEN(esp)) {
            if (s_proj_type != 0) {
                Mtx44 proj;
                s_proj_type = 0;
                C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
                GXSetProjection(proj, 1);
            }
        } else {
            if (s_proj_type != 1) {
                s_proj_type = 1;
                CameraCurrentProjection();
            }
        }
        if (s_tex_no != esp->anmNo) {
            if (!EspGetAnmAddr(esp->anmNo, &s_pAnm)) {
                pLog->err(0, 0, "ESP : TexId[%x] no data", esp->anmNo);
                s_tex_no = -1;
                return;
            }
        }
        if (s_tex_no != esp->anmNo || s_ptn_no != esp->anmPtn) {
            EspTexSet(esp->anmNo, esp->anmPtn);
            s_ptn_no = esp->anmPtn;
        }
        s_tex_no = esp->anmNo;
    }
    if (!esp->ChannelSet()) {
        return;
    }
    sx = esp->sizeX * esp->scale;
    sy = esp->sizeY * esp->scale;
    if ((f32) s_pAnm->x4 == 0.0f) {
        ox = -0.5f;
    } else {
        ox = (f32) -s_pAnm->x4 / s_pAnm->x0;
    }
    if ((f32) s_pAnm->x6 == 0.0f) {
        oy = -0.5f;
    } else {
        oy = (f32) s_pAnm->x6 / s_pAnm->x2 + -1.0f;
    }
    if (ESP_PARTS_SCREEN(esp)) {
        Mtx m;

        esp->mat[2][2] = 1.0f;
        esp->mat[0][0] = sx;
        esp->mat[0][1] = 0.0f;
        esp->mat[0][2] = 0.0f;
        esp->mat[0][3] = ox * sx;
        esp->mat[1][0] = 0.0f;
        esp->mat[1][1] = sy;
        esp->mat[1][2] = 0.0f;
        esp->mat[1][3] = oy * sy;
        esp->mat[2][0] = 0.0f;
        esp->mat[2][1] = 0.0f;
        esp->mat[2][3] = 0.0f;
        if (esp->flags & 2) {
            esp->mat[0][0] = -sx;
            esp->mat[0][3] = -(ox * sx);
            if (!(esp->flags & 4)) {
                esp->mat[1][1] = -sy;
                esp->mat[1][3] = -(oy * sy);
            }
        } else if (!(esp->flags & 4)) {
            esp->mat[1][1] = -sy;
            esp->mat[1][3] = -(oy * sy);
        }
        low_RotMatrix(m, &esp->rot);
        PSMTXConcat(m, esp->mat, esp->mat);
        esp->mat[0][3] += esp->pos.x;
        esp->mat[1][3] += esp->pos.y;
        esp->mat[2][3] += esp->pos.z;
    } else if (!(esp->flags & 0x80001)) {
        Mtx m;
        Vec p;
        Mtx m2;

        esp->mat[2][2] = 1.0f;
        esp->mat[0][0] = sx;
        esp->mat[0][1] = 0.0f;
        esp->mat[0][2] = 0.0f;
        esp->mat[0][3] = ox * sx;
        esp->mat[1][0] = 0.0f;
        esp->mat[1][1] = sy;
        esp->mat[1][2] = 0.0f;
        esp->mat[1][3] = oy * sy;
        esp->mat[2][0] = 0.0f;
        esp->mat[2][1] = 0.0f;
        esp->mat[2][3] = 0.0f;
        if (esp->flags & 2) {
            esp->mat[0][0] = -sx;
            esp->mat[0][3] = -(ox * sx);
            if (esp->flags & 4) {
                esp->mat[1][1] = -sy;
                esp->mat[1][3] = -(oy * sy);
            }
        } else if (esp->flags & 4) {
            esp->mat[1][1] = -sy;
            esp->mat[1][3] = -(oy * sy);
        }
        PSMTXRotRad(m, 'z', esp->rot.z);
        PSMTXConcat(m, esp->mat, esp->mat);
        PSMTXConcat(pG->Cam.viewMat, esp->parent->mat, m2);
        PSMTXMultVec(m2, &esp->pos, &p);
        esp->mat[0][3] += p.x;
        esp->mat[1][3] += p.y;
        esp->mat[2][3] += p.z;
    } else {
        Mtx m;

        esp->mat[0][0] = sx;
        esp->mat[0][1] = 0.0f;
        esp->mat[2][2] = 1.0f;
        esp->mat[0][2] = 0.0f;
        esp->mat[0][3] = ox * sx;
        esp->mat[1][0] = 0.0f;
        esp->mat[1][1] = sy;
        esp->mat[1][2] = 0.0f;
        esp->mat[1][3] = oy * sy;
        esp->mat[2][0] = 0.0f;
        esp->mat[2][1] = 0.0f;
        esp->mat[2][3] = 0.0f;
        if (esp->flags & 2) {
            esp->mat[0][0] = -sx;
            esp->mat[0][3] = -(ox * sx);
            if (esp->flags & 4) {
                esp->mat[1][1] = -sy;
                esp->mat[1][3] = -(oy * sy);
            }
        } else if (esp->flags & 4) {
            esp->mat[1][1] = -sy;
            esp->mat[1][3] = -(oy * sy);
        }
        low_RotMatrix(m, &esp->rot);
        PSMTXConcat(m, esp->mat, esp->mat);
        if (esp->flags & 0x80000) {
            Mtx m3;
            PSMTXRotRad(m3, 'y', EspGetCameraPan() * (3.1415927f / 180.0f));
            PSMTXConcat(m3, esp->mat, esp->mat);
        }
        esp->mat[0][3] += esp->pos.x;
        esp->mat[1][3] += esp->pos.y;
        esp->mat[2][3] += esp->pos.z;
        PSMTXConcat(esp->parent->mat, esp->mat, esp->mat);
        PSMTXConcat(pG->Cam.viewMat, esp->mat, esp->mat);
    }
    PSMTXInverse(esp->mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->mat, 0);
    GXSetCurrentMtx(0);
    if (esp->flags & 0x4000) {
        int no = esp->anmNo2;
        EspTexWk* tw = EspGetTexWk(no, 1);
        if (tw->owner == 0xD2) {
            pLog->err(0, 0, "ESP : Mask_TexId[%x] no data", no);
        } else {
            GXTexObj tex;
            GXTlutObj tlut;
            GXTexObj* pTex = &tex;
            GXTlutObj* pTlut = &tlut;
            TEXDescriptor* td = TEXGet(tw->pTpl, esp->anmPtn2);
            TEXHeader* th = td->textureHeader;

            if (th->format == 8 || th->format == 9) {
                GXInitTexObjCI(pTex, th->data, th->width, th->height, th->format, 0, 0, 0, 1);
                GXInitTlutObj(pTlut, td->CLUTHeader->data, td->CLUTHeader->format, td->CLUTHeader->numEntries);
                GXLoadTlut(pTlut, 1);
            } else {
                GXInitTexObj(pTex, th->data, th->width, th->height, th->format, 0, 0, 0);
            }
            GXLoadTexObj(pTex, 1);
            GXLoadTexMtxImm(tw->mtx, 0x21, 1);
            GXSetTexCoordGen(1, 1, 4, 0x21);
            GXSetNumTevStages(2);
            GXSetNumTexGens(2);
            GXSetTevOrder(1, 1, 1, 4);
            GXSetTevColorIn(1, 0xF, 0xF, 0xF, 0);
            GXSetTevColorOp(1, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(1, 7, 4, 5, 7);
            if (esp->flags & 0x20000) {
                GXSetTevAlphaOp(1, 0, 0, 2, 1, 0);
            } else {
                GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
            }
        }
    }
    {
        // The flag word is read into a local for the first test only: with two plain reads the
        // pre-cse jump threading merges the compares; the original kept one compare in cr7.
        u32 sysFlags = pG->flags_5010;
        if ((!(sysFlags & 0x80) && (esp->flags & 0x8000)) || ((pG->flags_5010 & 0x80) && (esp->flags & 0x800000))) {
            GXSetAlphaUpdate(1);
        }
    }
    if (esp->flags & 0x200000) {
        GXSetZMode(1, 3, 1);
        GXSetDstAlpha(1, 0);
    }
    if (esp->flags & 0x100000) {
        GXSetAlphaCompare(4, 0x80, 1, 4, 0x80);
    }
    if (esp->dispFlag & 0x10) {
        Esp1b_SpTrans(esp);
    } else {
        GXCallDisplayList(g_EspCommonDisplayList, 0x60);
    }
    if (esp->flags & 0x4000) {
        GXSetNumTevStages(1);
        GXSetNumTexGens(1);
    }
    if (esp->flags & 0x200000) {
        GXSetZMode(1, 3, 0);
        GXSetDstAlpha(0, 0);
    }
    if (esp->flags & 0x100000) {
        GXSetAlphaCompare(4, 1, 1, 4, 1);
    }
    if (esp->flags & 0x808000) {
        GXSetAlphaUpdate(0);
    }
}

// Heat shimmer sprite: the frame is copied into a texture and drawn back through an indirect
// texture (blur 1..3 select the warp mode; type scales the distortion).
static void EspCommonTransShimmer(cEsp* esp, int type, u32 blur)
{
    static Mtx Matrix1 = {
        {0.001953125f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f / 448.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
    static Mtx Matrix2 = {
        {0.001953125f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0029762f, -0.167f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
    Mtx44 proj;
    Mtx inv;
    EspAnmData* anm;
    GXColor fog;
    f32 sx;
    f32 sy;
    f32 ox;
    f32 oy;
    f32 x0;
    f32 y0;
    f32 z;
    f32 zero;
    f32 s0;
    f32 s1;
    f32 t0;
    f32 t1;
    f32 ofs;
    f32 scale;
    u8 stages;
    int texGens;
    int copyOk;
    void* buf;

    scale = (f32) type * (1.0f / 32.0f) + 1.0f;
    if (!esp->ChannelSet()) {
        return;
    }
    if (!EspGetAnmAddr(esp->anmNo, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->anmNo);
        return;
    }
    GXSetCullMode(0);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    GXSetZMode(1, 3, 0);
    CameraCurrentProjection();
    if (ESP_PARTS_SCREEN(esp)) {
        PSMTXIdentity(esp->mat);
        low_RotMatrix(esp->mat, &esp->rot);
        TransMatrix(esp->mat, &esp->pos);
        C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
        GXSetProjection(proj, 1);
    } else if (!(esp->flags & 1)) {
        Vec p;
        Mtx m;

        PSMTXIdentity(esp->mat);
        PSMTXRotRad(esp->mat, 'z', esp->rot.z);
        PSMTXConcat(pG->Cam.viewMat, esp->parent->mat, m);
        PSMTXMultVec(m, &esp->pos, &p);
        esp->mat[0][3] = p.x;
        esp->mat[1][3] = p.y;
        esp->mat[2][3] = p.z;
    } else {
        Mtx m;

        PSMTXIdentity(esp->mat);
        low_RotMatrix(esp->mat, &esp->rot);
        TransMatrix(esp->mat, &esp->pos);
        PSMTXConcat(pG->Cam.viewMat, esp->parent->mat, m);
        PSMTXConcat(m, esp->mat, esp->mat);
    }
    PSMTXInverse(esp->mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->mat, 0);
    GXSetCurrentMtx(0);
    EspTexSet(esp->anmNo, esp->anmPtn);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    sx = esp->sizeX * esp->scale;
    sy = esp->sizeY * esp->scale;
    ox = -anm->x4;
    oy = (f32) anm->x6;
    z = 1.0f;
    zero = 0.0f;
    if (ox == zero) {
        ox = -anm->x0 * 0.5f;
    }
    if (oy == zero) {
        oy = anm->x2 * 0.5f;
    }
    x0 = ox * sx / anm->x0;
    y0 = oy * sy / anm->x2;
    ESP_TEXCOORD_SET()
    GXTexObj tex;
    f32 indMtx[2][3];
    f32 dot;
    texGens = 0;
    copyOk = 1;
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    if (esp->flags & 0x1000) {
        if (GetDrawTmpBufType() == 2) {
            copyOk = 0;
        }
        buf = GetDrawTmpBufAddr(2);
    } else {
        buf = GetDrawTmpBufAddr(1);
    }
    ofs = 56.0f;
    if (pG->flags_5010 & 0x08000000) {
        ofs = 0.0f;
    }
    if (copyOk) {
        if (buf == NULL) {
            pLog->warn(0, 0, "Esp0d() : not enough memory");
            return;
        }
        GXSetTexCopySrc(0, (u32) ofs, (u32) Screen.width, (u32) (Screen.height - ofs));
        GXSetTexCopyDst((u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 1);
        GXCopyTex(buf, 0);
        GXPixModeSync();
        GXInvalidateTexAll();
    }
    GXInitTexObj(&tex, buf, (u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 0, 0, 0);
    GXLoadTexObj(&tex, 1);
    g_Get_tex_obj = tex;
    Mtx tm;
    Mtx pm;
    if (ESP_PARTS_SCREEN(esp)) {
        if (pG->flags_5010 & 0x08000000) {
            PSMTXConcat(Matrix1, esp->mat, tm);
        } else {
            PSMTXConcat(Matrix2, esp->mat, tm);
        }
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXSetTexCoordGen(texGens, 1, 0, 0x1E);
    } else {
        C_MTXLightPerspective(pm, pG->Cam.param.fovy, 1.3333334f, 0.5f, -0.6666667f, 0.5f, 0.5f);
        PSMTXConcat(pm, esp->mat, tm);
        GXLoadTexMtxImm(tm, 0x1E, 0);
        GXSetTexCoordGen(texGens, 0, 0, 0x1E);
    }
    texGens++;
    GXSetNumIndStages(1);
    GXSetTexCoordGen(texGens, 1, 4, 0x3C);
    texGens++;
    GXSetIndTexOrder(0, 1, 0);
    GXSetIndTexCoordScale(0, 0, 0);
    if (ESP_PARTS_SCREEN(esp)) {
        dot = 2500.0f;
    } else {
        Vec dir;
        Vec d;
        Vec p;
        Camera* cam;

        if (esp->parent != pEffParentWorld) {
            PSMTXMultVec(esp->parent->mat, &esp->pos, &p);
        } else {
            p = esp->pos;
        }
        cam = &pG->Cam;
        dir.x = cam->param.at.x - cam->param.pos.x;
        dir.y = cam->param.at.y - cam->param.pos.y;
        dir.z = cam->param.at.z - cam->param.pos.z;
#line 865 "D:/Bio4/Prog/esp_sub.cpp"
        VECNormalize(&dir, &dir);
        d.x = p.x - cam->param.pos.x;
        d.y = p.y - cam->param.pos.y;
        d.z = p.z - cam->param.pos.z;
        dot = PSVECDotProduct(&dir, &d);
    }
    if (dot < 1500.0f) {
        dot = 1500.0f;
    }
    if (blur != 3) {
        indMtx[1][1] = indMtx[0][0] = esp->colA * (1.0f / 255.0f) * 0.04f * 1000.0f / dot * scale;
        indMtx[0][1] = 0.0f;
        indMtx[0][2] = 0.0f;
        indMtx[1][0] = 0.0f;
        indMtx[1][2] = 0.0f;
    } else {
        static f32 prm1 = 0.5f;
        static f32 prm2 = 0.0f;
        static f32 prm3 = 0.0f;
        static f32 prm4 = Screen.height * 0.5f / Screen.width;

        indMtx[0][0] = prm1;
        indMtx[0][1] = prm2;
        indMtx[0][2] = 0.0f;
        indMtx[1][0] = prm3;
        indMtx[1][1] = prm4;
        indMtx[1][2] = 0.0f;
    }
    GXSetIndTexMtx(1, indMtx, 1);
    {
        u8 signedOfs;
        u8 replace;

        switch (blur) {
        case 1:
            signedOfs = 0;
            replace = 0;
            break;
        case 2:
            signedOfs = 1;
            replace = 0;
            break;
        case 3:
            signedOfs = 0;
            replace = 1;
            break;
        default:
            pLog->err(0, 0, "ESP_SHIMMER : BLUR_TYPE[%x] invalid", blur);
            signedOfs = 0;
            replace = 1;
            break;
        }
        stages = 1;
        GXSetTevIndWarp(0, 0, signedOfs, replace, 1);
    }
    GXSetTevOrder(0, 0, 1, 4);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    if (esp->flags & 0x4000) {
        int no = esp->anmNo2;
        EspTexWk* tw = EspGetTexWk(no, 1);
        if (tw->owner == 0xD2) {
            pLog->err(0, 0, "ESP : Mask_TexId[%x] no data", no);
        } else {
            GXTexObj tex2;
            GXTexObj* pTex = &tex2;
            GXTlutObj* pTlut = (GXTlutObj*) indMtx;
            TEXDescriptor* td = TEXGet(tw->pTpl, esp->anmPtn2);
            TEXHeader* th = td->textureHeader;

            if (th->format == 8 || th->format == 9) {
                GXInitTexObjCI(pTex, th->data, th->width, th->height, th->format, 0, 0, 0, 1);
                GXInitTlutObj(pTlut, td->CLUTHeader->data, td->CLUTHeader->format, td->CLUTHeader->numEntries);
                GXLoadTlut(pTlut, 1);
            } else {
                GXInitTexObj(pTex, th->data, th->width, th->height, th->format, 0, 0, 0);
            }
            GXLoadTexObj(pTex, 2);
            GXLoadTexMtxImm(tw->mtx, 0x21, 1);
            GXSetTexCoordGen(texGens, 1, 4, 0x21);
            GXSetTevOrder(1, texGens, 2, 4);
            GXSetTevColorIn(1, 0xF, 0xF, 0xF, 0);
            GXSetTevColorOp(1, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(1, 7, 4, 5, 7);
            if (esp->flags & 0x20000) {
                GXSetTevAlphaOp(1, 0, 0, 2, 1, 0);
            } else {
                GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
            }
            stages = 2;
            texGens++;
        }
    }
    GXSetNumTevStages(stages);
    GXSetNumTexGens(texGens);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x0, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(x0 + sx, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t0);
    GXPosition3f32(x0 + sx, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(x0, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t1);
    GXSetNumTevStages(1);
    GXSetNumTexGens(0);
    GXSetNumIndStages(0);
    GXSetTevDirect(0);
    GXSetTevDirect(1);
    LightMgr.setFog();
}

// Frame-buffer sprite: the frame is copied into a texture and drawn on the sprite quad,
// modulated by the sprite's own texture (type: TEV colour scale 0..2).
void EspCommonTransNega(cEsp* esp, u32 type)
{
    static Mtx Matrix = {
        {0.001953125f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0029762f, -0.167f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
    Mtx44 proj;
    Mtx inv;
    EspAnmData* anm;
    GXColor fog;
    void* buf;
    f32 sx;
    f32 sy;
    f32 ox;
    f32 oy;
    f32 x0;
    f32 y0;
    f32 z;
    f32 zero;
    f32 s0;
    f32 s1;
    f32 t0;
    f32 t1;

    if (!esp->ChannelSet()) {
        return;
    }
    if (!EspGetAnmAddr(esp->anmNo, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->anmNo);
        return;
    }
    CameraCurrentProjection();
    if (ESP_PARTS_SCREEN(esp)) {
        PSMTXIdentity(esp->mat);
        low_RotMatrix(esp->mat, &esp->rot);
        TransMatrix(esp->mat, &esp->pos);
        C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
        GXSetProjection(proj, 1);
    } else if (!(esp->flags & 1)) {
        Vec p;
        Mtx m;

        PSMTXIdentity(esp->mat);
        PSMTXRotRad(esp->mat, 'z', esp->rot.z);
        PSMTXConcat(pG->Cam.viewMat, esp->parent->mat, m);
        PSMTXMultVec(m, &esp->pos, &p);
        esp->mat[0][3] = p.x;
        esp->mat[1][3] = p.y;
        esp->mat[2][3] = p.z;
    } else {
        Mtx m;

        PSMTXIdentity(esp->mat);
        low_RotMatrix(esp->mat, &esp->rot);
        TransMatrix(esp->mat, &esp->pos);
        PSMTXConcat(pG->Cam.viewMat, esp->parent->mat, m);
        PSMTXConcat(m, esp->mat, esp->mat);
    }
    GXTexObj tex;
    PSMTXInverse(esp->mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->mat, 0);
    GXSetCurrentMtx(0);
    EspTexSet(esp->anmNo, esp->anmPtn);
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    esp->CommonStateSet();
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    sx = esp->sizeX * esp->scale;
    sy = esp->sizeY * esp->scale;
    ox = -anm->x4;
    oy = (f32) anm->x6;
    z = 1.0f;
    zero = 0.0f;
    if (ox == zero) {
        ox = -anm->x0 * 0.5f;
    }
    if (oy == zero) {
        oy = anm->x2 * 0.5f;
    }
    x0 = ox * sx / anm->x0;
    y0 = oy * sy / anm->x2;
    ESP_TEXCOORD_SET()
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    buf = GetDrawTmpBufAddr(3);
    if (buf == NULL) {
        pLog->warn(0, 0, "Esp0d() : not enough memory");
        return;
    }
    f32 ofs = 56.0f;
    GXSetTexCopySrc(0, (u32) ofs, (u32) Screen.width, (u32) (Screen.height - ofs));
    GXSetTexCopyDst((u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 1);
    GXCopyTex(buf, 0);
    GXPixModeSync();
    GXInvalidateTexAll();
    GXInitTexObj(&tex, buf, (u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 0, 0, 0);
    GXLoadTexObj(&tex, 1);
    if (ESP_PARTS_SCREEN(esp)) {
        Mtx tm;

        PSMTXConcat(Matrix, esp->mat, tm);
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXSetTexCoordGen(0, 1, 0, 0x1E);
    } else {
        Mtx tm;
        Mtx pm;

        C_MTXLightPerspective(pm, pG->Cam.param.fovy, 1.3333334f, 0.5f, -0.6666667f, 0.5f, 0.5f);
        PSMTXConcat(pm, esp->mat, tm);
        GXLoadTexMtxImm(tm, 0x1E, 0);
        GXSetTexCoordGen(0, 0, 0, 0x1E);
    }
    GXSetTevOrder(0, 0, 1, 4);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 8);
    switch (type) {
    case 0:
        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
        break;
    case 1:
        GXSetTevColorOp(0, 0, 0, 1, 1, 0);
        break;
    case 2:
        GXSetTevColorOp(0, 0, 0, 2, 1, 0);
        break;
    }
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXSetTevOrder(1, 0xFF, 0xFF, 4);
    GXSetTevColorIn(1, 0xA, 0xF, 0, 0xF);
    GXSetTevColorOp(1, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(1, 7, 7, 7, 5);
    GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
    GXSetNumTexGens(2);
    GXSetTexCoordGen(1, 1, 4, 0x3C);
    GXSetNumTevStages(2);
    GXSetTevOrder(2, 1, 0, 4);
    GXSetTevColorIn(2, 0xF, 0xF, 0xF, 0);
    GXSetTevColorOp(2, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(2, 7, 0, 4, 7);
    GXSetTevAlphaOp(2, 0, 0, 0, 1, 0);
    GXSetNumTevStages(3);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x0, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(x0 + sx, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t0);
    GXPosition3f32(x0 + sx, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(x0, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t1);
    GXSetNumTevStages(1);
    GXSetNumTexGens(0);
    GXSetNumIndStages(0);
    GXSetTevDirect(0);
    GXSetTevDirect(1);
    LightMgr.setFog();
}

void cEsp::move()
{
    pLog->err(0, 0, "ESP : ESP_ID[%x] move() invalid", id);
}

int cEsp::CommonMove()
{
    if (parent != pEffParentWorld && parentCnt != 0xFF && parentCnt <= cnt) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    if (spdCnt == 0 || spdCnt <= cnt) {
        PSVECAdd(&pos, &spd, &pos);
        PSVECAdd(&spd, &acc, &spd);
        PSVECScale(&spd, &spd, spdScale);
    }
    if (scaleCnt == 0 || scaleCnt <= cnt) {
        scale += scaleSpd;
        scaleSpd *= scaleScale;
        if (scale <= 0.0f) {
            PushEsp(this);
            return 0;
        }
    }
    PSVECAdd(&rot, &rotSpd, &rot);
    if (!ColorUpdate()) {
        return 0;
    }
    if (life != 0 && life <= cnt) {
        PushEsp(this);
        return 0;
    }
    cnt++;
    xB8 = SQRTF(sizeX * sizeX + sizeY * sizeY) * scale;
    return 1;
}

int cEsp::ColorUpdate()
{
    if (xA8 < cnt) {
        if (xA8 + xAA <= cnt) {
            colR *= colRSpd;
            colG *= colGSpd;
            colB *= colBSpd;
            colA *= colASpd;
            if (colR > 255.0f) {
                colR = 255.0f;
            }
            if (colG > 255.0f) {
                colG = 255.0f;
            }
            if (colB > 255.0f) {
                colB = 255.0f;
            }
            if (colA > 255.0f) {
                colA = 255.0f;
            }
            if (colA < 4.0f) {
                PushEsp(this);
                return 0;
            }
        }
    } else if (xA8 != 0) {
        f32 rate = (f32) cnt / (f32) xA8;
        if (blendType == 3) {
            colR = (f32) x80 * rate;
            colG = (f32) x81 * rate;
            colB = (f32) x82 * rate;
        }
        colA = (f32) x83 * rate;
    }
    return 1;
}

int cEsp::SetFreeWork(EspGenWork* gen, u32* seed)
{
    return 1;
}

int cEsp::AnmMove()
{
    EspAnmData* anm;
    u32 time;

    if (!EspGetAnmAddr(anmNo, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", anmNo);
        return 0;
    }
    if (anm->xC == 0) {
        time = 1;
    } else {
        time = anm->ptnTime[anm->nPtn + anmPtn];
    }
    anmCnt += anmSpd;
    while ((anmCnt >> 5) > (u16) time) {
        anmPtn++;
        anmCnt -= time << 5;
        if (anmPtn >= anm->nPtn) {
            switch (anm->xB & 3) {
            case 0:
                return 0;
            case 1:
                anmPtn = 0;
                break;
            case 2:
                anmPtn = anm->nPtn - 1;
                break;
            }
        }
    }
    if (flags & 0x4000) {
        if (!EspGetAnmAddr(anmNo2, &anm)) {
            pLog->err(0, 0, "ESP : MaskTexId[%x] no data", anmNo);
            return 0;
        }
        if (anm->xC == 0) {
            time = 1;
        } else {
            time = anm->ptnTime[anm->nPtn + anmPtn2];
        }
        anmCnt2 += anmSpd;
        while ((anmCnt2 >> 5) > (u16) time) {
            anmPtn2++;
            anmCnt2 -= time << 5;
            if (anmPtn2 >= anm->nPtn) {
                switch (anm->xB & 3) {
                case 0:
                    return 0;
                case 1:
                    anmPtn2 = 0;
                    break;
                case 2:
                    anmPtn2 = anm->nPtn - 1;
                    break;
                }
            }
        }
    }
    return 1;
}

#line 1730 "D:/Bio4/Prog/esp_sub.cpp"
int cEsp::ChannelSet()
{
    GXColor col;
    GXColor fin;
    Vec p;
    Vec dir;
    Vec d;
    cEspSystem* sys = g_pEspSys;

    if (flags & 0x40) {
        GXSetTevOp(0, 0);
        GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
        GXSetTevColorOp(0, 0, 0, 2, 1, 0);
        commonEspLightSet(sys->lightList.p, sys->lightList.num);
    } else {
        GXSetTevOp(0, 0);
        if (flags & 0x80) {
            GXSetTevColorOp(0, 0, 0, 2, 1, 0);
        }
        if (flags & 0x20000) {
            GXSetTevAlphaOp(0, 0, 0, 2, 1, 0);
        }
        GXSetNumChans(1);
        GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
    }
    if (dispFlag & 1) {
        col.r = (u8) (colR * colA * (1.0f / 255.0f));
        col.g = (u8) (colG * colA * (1.0f / 255.0f));
        col.b = (u8) (colB * colA * (1.0f / 255.0f));
        col.a = 0xFF;
    } else {
        col.r = (u8) colR;
        col.g = (u8) colG;
        col.b = (u8) colB;
        col.a = (u8) colA;
    }
    if (!(dispFlag & 4) && EffIsSetFinalCol()) {
        EffGetFinalCol(&fin);
        col.r = (u32) col.r * fin.r >> 8;
        col.g = (u32) col.g * fin.g >> 8;
        col.b = (u32) col.b * fin.b >> 8;
        col.a = (u32) col.a * fin.a >> 8;
    }
    if (x16 != 0) {
        Camera* cam;
        f32 dot;

        if (parent != pEffParentWorld) {
            PSMTXMultVec(parent->mat, &pos, &p);
        } else {
            p = pos;
        }
        cam = &pG->Cam;
        dir.x = cam->param.at.x - cam->param.pos.x;
        dir.y = cam->param.at.y - cam->param.pos.y;
        dir.z = cam->param.at.z - cam->param.pos.z;
#line 1798 "D:/Bio4/Prog/esp_sub.cpp"
        VECNormalize(&dir, &dir);
        d.x = p.x - cam->param.pos.x;
        d.y = p.y - cam->param.pos.y;
        d.z = p.z - cam->param.pos.z;
        dot = PSVECDotProduct(&dir, &d);
        if (dot < x16 * 10.0f) {
            f32 rate = 1.0f - (x16 * 10.0f - dot) / ((x16 - x14) * 10.0f);
            if (dispFlag & 1) {
                col.r = (u8) (col.r * rate);
                col.g = (u8) (col.g * rate);
                col.b = (u8) (col.b * rate);
            } else {
                col.a = (u8) (col.a * rate);
            }
        }
    }
    GXSetChanMatColor(4, col);
    {
        int ret = 0;
        if (col.a != 0) {
            ret = 1;
        }
        return ret;
    }
}

void cEsp::ApplyMatrix(Mtx m)
{
    Mtx r;

    PSMTXMultVec(m, &pos, &pos);
    PSMTXMultVecSR(m, &spd, &spd);
    PSMTXMultVecSR(m, &acc, &acc);
    if (flags & 1) {
        low_RotMatrix(r, &rot);
        PSMTXConcat(m, r, r);
        Matrix2AxisAngle(r, &rot);
    }
}

void cEsp::CommonStateSet()
{
    GXSetCullMode(0);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    GXSetZMode(1, 3, 0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x1E);
}

cEsp::cEsp()
{
}

cEsp::~cEsp()
{
}

void cEsp::Destruct()
{
}

int EspEstSetSelect(int a, int b, int c, cEsp** out, int d)
{
    EspSeqData* head;
    EspGenWork* rec;
    EspInfo info;
    Mtx m;
    u8 type;

    head = EspGetEstAddr(a, b, 0);
    if (head == 0) {
        pLog->err(0, 0, "EspEstSetSelect:[%x/0x%02x] OWNER Invalid", a, b);
        return 0;
    }
    if ((u32) c >= head->num) {
        pLog->err(0, 0, "EspEstSetSelect() : invalid no[%d] MAX=%d", c, head->num);
        return 0;
    }
    u32 seed = 0x12345678;
    rec = &head->rec[c];
    type = rec->type;
    if (type != 0) {
        if (type == 1) {
            pLog->err(0, 0, "EspEstSetSelect : can't call Espgen.");
            return 0;
        }
        pLog->err(0, 0, "EspEstSetSelect : KIND[%d] is invalid.", type);
        return 0;
    }
    PSMTXIdentity(m);
    memclr_asm(&info, sizeof(info));
    if (d == 1) {
        info.x0 |= 1;
    }
    return EspSeqSet(rec, &info, &seed, 0, &m, 0, 0.0f, out, 0, 0) == 1;
}

int EspSeqSet(EspGenWork* rec, EspInfo* info, u32* seed, cModel* model, Mtx* mtx, int a, f32 f, cEsp** out,
              EspSeqOpt* p8, Vec* pos)
{
    static int bl[6][4] = {
        {1, 4, 5, 0}, {1, 4, 1, 0}, {1, 1, 1, 0}, {1, 2, 1, 0}, {1, 2, 0, 0}, {1, 4, 3, 0},
    };
    Vec v;
    Mtx m;
    Mtx m2;
    EspPtr e;
    f32 rnd;
    int ret;
    cModel* parts;

    if ((u8) (rec->x1 + 4) <= 3) {
        EfmSeqSet(rec, (EfmCore*) info, seed, model, *mtx, 0, 0.0f, pos);
        *out = EspGetDmyPtr();
        return 1;
    }
    if (rec->x1 == 0x3F) {
        pLog->err(0, 0, "ESP : ESP_ID[%x] is invalid.", rec->x1);
        *out = EspGetDmyPtr();
        return 0;
    }
    if (rec->x1 != 3 && rec->x1 != 9 && rec->x1 != 0xC && !EspChkTexId(rec->x2)) {
        pLog->err(0, 0, "ESP : TEX_ID[%x] not initialized.", rec->x2);
        *out = EspGetDmyPtr();
        return 0;
    }
    if (PullEsp(&e.p, rec->x1) != 0) {
        e.p->info = *info;
        e.p->id = rec->x1;
        e.p->anmNo = rec->x2;
        e.p->xF = rec->x3;
        e.p->partsNo = rec->x7;
        if (!(info->x0 & 0x1000) && rec->x6 != 0) {
            model = SmdGetObjPtr(rec->x6 - 1);
            if (model == 0) {
                pLog->err(0, 0, "ESP : PARENT_NO[%d] Invalid.", rec->x6);
                PushEsp(e.p);
                *out = EspGetDmyPtr();
                return 0;
            }
        }
        e.p->flags = rec->flags;
        if (e.p->flags & 8) {
            if (fRandSeed1_1(seed) > 0.0f) {
                e.p->flags |= 2;
            } else {
                e.p->flags &= ~2;
            }
        }
        if (e.p->flags & 0x10) {
            if (fRandSeed1_1(seed) > 0.0f) {
                e.p->flags |= 4;
            } else {
                e.p->flags &= ~4;
            }
        }
        e.p->pos = rec->pos;
        e.p->pos.x += rec->x18 * fRandSeed1_1(seed);
        e.p->pos.y += rec->x1C * fRandSeed1_1(seed);
        e.p->pos.z += rec->x20 * fRandSeed1_1(seed);
        e.p->spd = rec->x24;
        e.p->spd.x += rec->x34.x * fRandSeed1_1(seed);
        e.p->spd.y += rec->x34.y * fRandSeed1_1(seed);
        e.p->spd.z += rec->x34.z * fRandSeed1_1(seed);
        if (a) {
            v.x = 0.0f;
            v.y = f;
            v.z = 0.0f;
            RotMatrixZXY(m, &v);
            PSMTXMultVecSR(m, &e.p->spd, &e.p->spd);
        }
        e.p->spdScale = rec->x30;
        e.p->acc = rec->x40;
        e.p->acc.x += rec->x4C.x * fRandSeed1_1(seed);
        e.p->acc.y += rec->x4C.y * fRandSeed1_1(seed);
        e.p->acc.z += rec->x4C.z * fRandSeed1_1(seed);
        e.p->rot = rec->x58;
        e.p->rot.x += rec->x64.x * fRandSeed1_1(seed);
        e.p->rot.y += rec->x64.y * fRandSeed1_1(seed);
        e.p->rot.z += rec->x64.z * fRandSeed1_1(seed);
        PSVECScale(&e.p->rot, &e.p->rot, DEG2RAD);
        e.p->rotSpd = rec->x70;
        e.p->rotSpd.x += rec->x7C.x * fRandSeed1_1(seed);
        e.p->rotSpd.y += rec->x7C.y * fRandSeed1_1(seed);
        e.p->rotSpd.z += rec->x7C.z * fRandSeed1_1(seed);
        PSVECScale(&e.p->rotSpd, &e.p->rotSpd, DEG2RAD);
        e.p->sizeX = rec->x88;
        e.p->sizeY = rec->x8C;
        e.p->scale = 1.0f;
        rnd = rec->x90 * fRandSeed1_1(seed);
        e.p->sizeX += rnd;
        e.p->sizeY += rnd;
        e.p->scaleSpd = rec->x94;
        e.p->scaleScale = rec->x98;
        e.p->x80 = rec->x9C;
        e.p->x81 = rec->x9D;
        e.p->x82 = rec->x9E;
        e.p->x83 = rec->x9F;
        e.p->colR = (f32) rec->x9C;
        e.p->colG = (f32) rec->x9D;
        e.p->colB = (f32) rec->x9E;
        e.p->colA = (f32) rec->x9F;
        e.p->colRSpd = rec->xA0;
        e.p->colGSpd = rec->xA4;
        e.p->colBSpd = rec->xA8;
        e.p->colASpd = rec->xAC;
        if (rec->xC2 > 5) {
            pLog->err(0, 0, "ESP : BLEND_TYPE[%d] Invalid.", rec->xC2);
            PushEsp(e.p);
            *out = EspGetDmyPtr();
            return 0;
        }
        e.p->blendType = rec->xC2;
        e.p->xA4 = bl[rec->xC2][0];
        e.p->xA5 = bl[rec->xC2][1];
        e.p->xA6 = bl[rec->xC2][2];
        e.p->xA7 = bl[rec->xC2][3];
        if (rec->xC2 == 4) {
            e.p->dispFlag |= 1;
        }
        e.p->xA8 = rec->xB0;
        e.p->xAA = rec->xB2;
        e.p->spdCnt = rec->xB4;
        e.p->scaleCnt = rec->xB6;
        e.p->life = rec->xB8;
        e.p->cnt = rec->xBA;
        e.p->anmPtn = rec->xBC;
        e.p->anmSpd = rec->xBD + 0x20;
        e.p->anmCnt = rec->xBE;
        e.p->parentCnt = rec->xC0;
        e.p->xEC = rec->xC3;
        e.p->xED = rec->xC4;
        e.p->anmNo2 = rec->xC5;
        e.p->x16 = rec->xC6 * 10;
        e.p->x14 = rec->xC7 * 10;
        if (e.p->xEC != 0) {
            e.p->dispFlag |= 4;
        }
        switch (e.p->partsNo) {
        case 0xFF:
            e.p->parent = pEffParentWorld;
            e.p->ApplyMatrix(*mtx);
            break;
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFD:
            e.p->parent = pEffParentWorld;
            e.p->pos.x += (*mtx)[0][3];
            e.p->pos.y += (*mtx)[1][3];
            e.p->pos.z += (*mtx)[2][3];
            break;
        case 0xFE:
            e.p->parent = pEffParentWorld;
            if (rec->xC0 != 0) {
                pLog->warn(0, 0, "ESP:ReleaseTime not 0 but no parent.");
            }
            break;
        default:
            if (model == 0) {
                pLog->err(0, 0, "ESP : PARTS_NO[%d] but Not on parts.", e.p->partsNo);
                PushEsp(e.p);
                *out = EspGetDmyPtr();
                return 0;
            }
            if (e.p->partsNo < model->nParts) {
                if (e.p->flags & 0x20) {
                    parts = model->getPartsPtr(e.p->partsNo);
                    PSMTXIdentity(m2);
                    low_RotMatrix(m2, &model->rot);
                    PSMTXMultVecSR(m2, &rec->pos, &v);
                    m2[0][3] = parts->mat[0][3] + v.x;
                    m2[1][3] = parts->mat[1][3] + v.y;
                    m2[2][3] = parts->mat[2][3] + v.z;
                    e.p->parent = pEffParentWorld;
                    e.p->pos.x = 0.0f;
                    e.p->pos.y = 0.0f;
                    e.p->pos.z = 0.0f;
                    e.p->ApplyMatrix(m2);
                    e.p->pos.x += rec->x18 * fRandSeed1_1(seed);
                    e.p->pos.y += rec->x1C * fRandSeed1_1(seed);
                    e.p->pos.z += rec->x20 * fRandSeed1_1(seed);
                } else {
                    e.p->pModel = model;
                    e.p->x20 = model->serial;
                    e.p->parent = model->getPartsPtr(e.p->partsNo);
                    if (pos) {
                        PSVECAdd(&e.p->pos, pos, &e.p->pos);
                    }
                }
            } else {
                pLog->err(0, 0, "ESP : PARTS_NO[%d] is invalid(MAX:%d).", e.p->partsNo, model->nParts);
                PushEsp(e.p);
                *out = EspGetDmyPtr();
                return 0;
            }
            break;
        }
        ret = e.p->SetFreeWork(rec, seed);
        if (p8) {
            if (p8->set & 1) {
                e.p->spd = p8->spd;
                e.p->spd.x += rec->x34.x * fRandSeed1_1(seed);
                e.p->spd.y += rec->x34.y * fRandSeed1_1(seed);
                e.p->spd.z += rec->x34.z * fRandSeed1_1(seed);
            }
            if (p8->set & 2) {
                e.p->sizeX = p8->sizeX;
                e.p->sizeY = p8->sizeY;
            }
            if (p8->set & 4) {
                e.p->x80 = p8->r;
                e.p->x81 = p8->g;
                e.p->x82 = p8->b;
                e.p->x83 = p8->a;
                e.p->colR = (f32) p8->r;
                e.p->colG = (f32) p8->g;
                e.p->colB = (f32) p8->b;
                e.p->colA = (f32) p8->a;
            }
            if (p8->mul & 1) {
                e.p->spd.x *= p8->spd.x;
                e.p->spd.y *= p8->spd.y;
                e.p->spd.z *= p8->spd.z;
            }
            if (p8->mul & 2) {
                e.p->sizeX *= p8->sizeX;
                e.p->sizeY *= p8->sizeY;
            }
            if (p8->mul & 4) {
                f32 c;
                c = (f32) e.p->x80 * (f32) (int) p8->r * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->x80 = (u8) c;
                c = (f32) e.p->x81 * (f32) (int) p8->g * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->x81 = (u8) c;
                c = (f32) e.p->x82 * (f32) (int) p8->b * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->x82 = (u8) c;
                c = (f32) e.p->x83 * (f32) (int) p8->a * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->x83 = (u8) c;
                e.p->colR = (f32) e.p->x80;
                e.p->colG = (f32) e.p->x81;
                e.p->colB = (f32) e.p->x82;
                e.p->colA = (f32) e.p->x83;
            }
            if (p8->add & 1) {
                e.p->spd.x += p8->spd.x;
                e.p->spd.y += p8->spd.y;
                e.p->spd.z += p8->spd.z;
            }
            if (p8->add & 2) {
                e.p->sizeX += p8->sizeX;
                e.p->sizeY += p8->sizeY;
            }
            if (p8->add & 4) {
                f32 c;
                c = (f32) e.p->x80 + (f32) (int) p8->r * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->x80 = (u8) c;
                c = (f32) e.p->x81 + (f32) (int) p8->g * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->x81 = (u8) c;
                c = (f32) e.p->x82 + (f32) (int) p8->b * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->x82 = (u8) c;
                c = (f32) e.p->x83 + (f32) (int) p8->a * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->x83 = (u8) c;
                e.p->colR = (f32) e.p->x80;
                e.p->colG = (f32) e.p->x81;
                e.p->colB = (f32) e.p->x82;
                e.p->colA = (f32) e.p->x83;
            }
        }
    } else {
        ret = 0;
    }
    if (ret == 0) {
        *out = EspGetDmyPtr();
        if (e.p->flag & 1) {
            PushEsp(e.p);
        }
        return 0;
    }
    *out = e.p;
    return ret;
}

// Bilinear point of the quad p0 p1 p2 p3 at (u, v).
void GetPosXY(Vec* p0, Vec* p1, Vec* p2, Vec* p3, f32 u, f32 v, Vec* out)
{
    static Vec v0;
    static Vec v1;
    static Vec v2;

    PSVECSubtract(p1, p0, &v0);
    PSVECSubtract(p2, p3, &v1);
    PSVECScale(&v0, &v0, u);
    PSVECScale(&v1, &v1, u);
    PSVECAdd(&v0, p0, &v0);
    PSVECAdd(&v1, p3, &v1);
    PSVECSubtract(&v1, &v0, &v2);
    PSVECScale(&v2, &v2, v);
    PSVECAdd(&v2, &v0, out);
}

// esp1b work at cEsp+0xF8: the quad is subdivided div x div times.
struct Esp1bSpWork {
    u32 div;   // 0x00
    Vec p1;    // 0x04 corner offsets from the unit quad (0,1,1) (1,1,1) (1,0,1) (0,0,1)
    Vec p2;    // 0x10
    Vec p3;    // 0x1C
};

void Esp1b_SpTrans(cEsp* esp)
{
    static Vec p0;
    static Vec p1;
    static Vec p2;
    static Vec p3;
    static Vec ret;
    Esp1bSpWork* w = (Esp1bSpWork*) &esp->pad_EC[0xC];
    u32 div;
    u32 i;
    u32 j;
    f32 step;
    f32 u;
    f32 u1;
    f32 v;
    f32 v1;
    f32 tu;
    f32 tu1;
    f32 tv;
    f32 tv1;

    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    div = w->div;
    p0.x = 0.0f;
    p0.y = 1.0f;
    p0.z = 1.0f;
    p1.x = w->p1.x + 1.0f;
    p1.y = w->p1.y + 1.0f;
    p1.z = w->p1.z + 1.0f;
    p2.x = w->p2.x + 1.0f;
    p2.y = w->p2.y + 0.0f;
    p2.z = w->p2.z + 1.0f;
    p3.x = w->p3.x + 0.0f;
    p3.y = w->p3.y + 0.0f;
    p3.z = w->p3.z + 1.0f;
    GXBegin(0x80, 0, div * 4 * div);
    step = 1.0f / (f32) div;
    v = 0.0f;
    tv = 0.0f;
    for (i = 0; i < div; i++) {
        v1 = v + step;
        tv1 = tv + step;
        u = 0.0f;
        tu = 0.0f;
        for (j = 0; j < div; j++) {
            u1 = u + step;
            tu1 = tu + step;
            GetPosXY(&p0, &p1, &p2, &p3, u, v, &ret);
            GXPosition3f32(ret.x, ret.y, ret.z);
            GXNormal3s8(0, 1, 0);
            GXTexCoord2f32(tu, tv);
            GetPosXY(&p0, &p1, &p2, &p3, u1, v, &ret);
            GXPosition3f32(ret.x, ret.y, ret.z);
            GXNormal3s8(0, 1, 0);
            GXTexCoord2f32(tu1, tv);
            GetPosXY(&p0, &p1, &p2, &p3, u1, v1, &ret);
            GXPosition3f32(ret.x, ret.y, ret.z);
            GXNormal3s8(0, 1, 0);
            GXTexCoord2f32(tu1, tv1);
            GetPosXY(&p0, &p1, &p2, &p3, u, v1, &ret);
            GXPosition3f32(ret.x, ret.y, ret.z);
            GXNormal3s8(0, 1, 0);
            GXTexCoord2f32(tu, tv1);
            u = u1;
            tu = tu1;
        }
        v += step;
        tv += step;
    }
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 1, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 1, 0);
}

// The split object's .sdata is padded to 8 bytes.
asm(".section .sdata; .balign 8");
