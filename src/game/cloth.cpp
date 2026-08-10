// game/cloth.cpp: grid cloth simulation (esp4e sheets): a nx*ny grid of points joined by springs,
// pulled down by gravity, drawn as triangle strips with the lights of the room.

#include "light.h"
#include "global.h"
#include "model.h"
#include "camera.h"
#include "gx.h"
#include "tpl.h"
#include "cloth.h"
#include "math_sub.h"
#include "main_mem.h"
#include "trans_ot.h"

extern "C" {
// game/trans_lit.cpp
void commonClothLightSet(cLight** list, int n, Vec pos, f32 radius);
void OSReport(const char* fmt, ...);
}

Cloth ClothWk[8];

// clothTrans builds a temporary cModel for the light setup. The real cModel is 0x320 bytes
// (see the KNOWN DEBT note in docs/matching.md: motion/atari fields still live in cEm/cObj), so an
// unused pad local follows it to keep the original frame layout until cModel carries them itself.

f32 K_PARAM = 7.5f;   // spring constant
f32 G_PARAM = 0.5f;   // gravity
f32 T_PARAM = 0.06f;  // time step
static f32 D_PARAM = 0.006f;  // unused (the 0.006 word after T_PARAM in .sdata; name unknown)

void ClothInit()
{
    int i;

    for (i = 0; i < 8; i++) {
        ClothWk[i].flag = 0;
    }
}

void ClothRoomInit()
{
    ClothInit();
}

// Dead-stripped in the DOL (only its strings and its 0.0f pool survive, ahead of Cloth::Set's
// .rodata): a helper that allocates a Vec and normalises into it. Body unknown; this one
// reproduces the string order (__FILE__ before "VECNormalize:[%s/%d]") and the pool.
static Vec* clothAllocNormal(Vec* dir)
{
#line 120 "D:/Bio4/Prog/cloth.cpp"
    Vec* v = (Vec*) MEM_ALLOC(sizeof(Vec), 1, 13);
    VECNormalize(dir, v);
    return v;
}

void Cloth::Set(Vec ang, Vec pos_, u8 nx_, u8 ny_, f32 w, GXTexObj* tex_, f32 h, void* p_, f32 d, GXTlutObj* tlut_,
                int flag_)
{
    Vec zero = {0.0f, 0.0f, 0.0f};
    Vec n = {0.0f, 0.0f, 1.0f};
    Vec* pp;
    Vec* pn;
    Vec* ps;
    u32 rowSize;
    int i;
    int j;

    x4 = w;
    x8 = h;
    xC = d;
    nx = nx_;
    ny = ny_;
    x70 = flag_;
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos_);
    rowSize = ny_ * sizeof(Vec);
#line 212 "D:/Bio4/Prog/cloth.cpp"
    mem = MEM_ALLOC(nx_ * rowSize * 3, 1, 13);
    if (mem == 0) {
        OSReport("ClothSet(): Can't allocate memory.\n");
        flag = 0;
    }
    pp = (Vec*) mem;
    pos = pp;
    pn = pp + nx_ * ny_;
    nrm = pn;
    ps = pp + nx_ * 2 * ny_;
    spd = ps;
    for (j = 0; j < ny_; j++) {
        for (i = 0; i < nx_; i++) {
            pp->x = i * x4;
            pp->y = -j * x8;
            if (flag_ != 0 && (j >= 0 && j <= 3)) {
                int m = i % 6;
                pp->z = sinf(m * 6.2831855f / 6.0f) * -2.0f;
            } else {
                pp->z = 0.0f;
            }
            *ps = zero;
            *pn = n;
            pp++;
            ps++;
            pn++;
        }
    }
    for (i = 0; i < 120; i++) {
        calcSpeed(0.9f);
        move();
    }
    x4C.x = x4 * nx * 0.5f * xC;
    x4C.y = 0.0f;
    x4C.z = 0.0f;
    PSMTXMultVec(mat, &x4C, &x4C);
    x58 = x4 * nx * 0.5f * xC;
    tex = tex_;
    tlut = tlut_;
    x64 = p_;
    flag = 0x31;
    colR = 0xFF;
    colG = 0xFF;
    colB = 0xFF;
    colA = 0xFF;
    x74 = 0;
}

void Cloth::SetPosAng(Vec ang, Vec pos_)
{
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos_);
}

void Cloth::Destroy()
{
    Mem_free(mem);
    flag = 0;
}

void ClothCalcTplAddr(void* tpl)
{
    TEXPalette* pal = (TEXPalette*) tpl;
    TEXDescriptor* d;
    u32 i;

    if (pal == 0) {
        return;
    }
    if ((s32) pal->descriptorArray < 0) {
        return;
    }
    pal->descriptorArray = (TEXDescriptor*) ((u32) pal->descriptorArray + (u32) pal);
    for (i = 0; i < pal->numDescriptors; i++) {
        d = &pal->descriptorArray[i];
        d->textureHeader = (TEXHeader*) ((u32) pal + (u32) d->textureHeader);
        d->textureHeader->data = (void*) ((u32) pal + (u32) d->textureHeader->data);
        if (d->CLUTHeader != 0) {
            d->CLUTHeader = (CLUTHeader*) ((u32) pal + (u32) d->CLUTHeader);
            d->CLUTHeader->data = (void*) ((u32) pal + (u32) d->CLUTHeader->data);
        }
    }
}

int ClothTexSetUp(void* tpl, GXTexObj* tex, int no, GXTlutObj* tlut)
{
    TEXDescriptor* d;
    TEXHeader* t;
    CLUTHeader* c;
    int ret = 0;

    ClothCalcTplAddr(tpl);
    d = TEXGet((TEXPalette*) tpl, 0);
    t = d->textureHeader;
    if (t->format >= 8 && t->format <= 9) {
        GXInitTexObjCI(tex, t->data, t->width, t->height, t->format, 0, 0, 0, 0);
        ret = 1;
        c = d->CLUTHeader;
        GXInitTlutObj(tlut, c->data, c->format, c->numEntries);
        GXLoadTlut(tlut, 0);
    } else {
        GXInitTexObj(tex, t->data, t->width, t->height, t->format, 0, 0, 0);
    }
    return ret;
}

void Cloth::calcSpeed(f32 damping)
{
    Vec v;
    Vec* p = pos;
    Vec* s = spd;
    int i;
    int j;
    int k;
    int w;
    f32 len;

    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            if (i == 0 && j != 0 && j != nx - 1) {
                continue;
            }
            w = nx;
            k = j + i * w;
            if (j != 0) {
                PSVECSubtract(&p[k - 1], &p[k], &v);
                len = PSVECMag(&v);
                if (len > x4) {
                    PSVECScale(&v, &v, K_PARAM * (len - x4) / len);
                    PSVECAdd(&v, &s[k], &s[k]);
                }
            }
            if (j != nx - 1) {
                PSVECSubtract(&p[k + 1], &p[k], &v);
                len = PSVECMag(&v);
                if (len > x4) {
                    PSVECScale(&v, &v, K_PARAM * (len - x4) / len);
                    PSVECAdd(&v, &s[k], &s[k]);
                }
            }
            if (i != 0) {
                PSVECSubtract(&p[k - w], &p[k], &v);
                len = PSVECMag(&v);
                if (len > x8) {
                    PSVECScale(&v, &v, K_PARAM * (len - x8) / len);
                    PSVECAdd(&v, &s[k], &s[k]);
                }
            }
            if (i != ny - 1) {
                PSVECSubtract(&p[k + w], &p[k], &v);
                len = PSVECMag(&v);
                if (len > x8) {
                    PSVECScale(&v, &v, K_PARAM * (len - x8) / len);
                    PSVECAdd(&v, &s[k], &s[k]);
                }
            }
            s[k].y -= G_PARAM;
            PSVECScale(&s[k], &s[k], damping);
        }
    }
}

int PullCloth(Cloth** out)
{
    Cloth* c = ClothWk;
    int i;

    *out = 0;
    for (i = 0; i < 8; i++) {
        if ((c->flag & 1) == 0) {
            *out = c;
            return 1;
        }
        c++;
    }
    return 0;
}

void Cloth::move()
{
    Vec v;
    int i;
    int j;

    for (i = 0; i < ny; i++) {
        if (x70 == 0) {
            if (i == 0) {
                continue;
            }
        } else if (i >= 0 && i <= 3) {
            continue;
        }
        for (j = 0; j < nx; j++) {
            int k = j + nx * i;
            PSVECScale(&spd[k], &v, T_PARAM);
            PSVECAdd(&pos[k], &v, &pos[k]);
        }
    }
}

void Cloth::calcNormal()
{
    Vec a;
    Vec b;
    Vec c;
    int i;
    int j;
    int k;

    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            u8 w = nx;
            k = j + i * w;
            if (i == ny - 1) {
                nrm[k] = nrm[k - w];
            } else if (j == w - 1) {
                nrm[k] = nrm[k - 1];
            } else {
                PSVECSubtract(&pos[k + 1], &pos[k], &a);
                PSVECSubtract(&pos[k + w], &pos[k], &b);
                PSVECCrossProduct(&b, &a, &c);
#line 622 "D:/Bio4/Prog/cloth.cpp"
                VECNormalize(&c, &nrm[k]);
            }
        }
    }
}

void Cloth::disturbance(f32 power, u32 x, u32 y)
{
    u32 idx = x + nx * y;

    spd[idx].z += power;
    spd[idx].y += power * 0.5f;
}

void ClothDraw()
{
    Cloth* c = ClothWk;
    int i;

    for (i = 0; i < 8; i++, c++) {
        if (c->flag & 0x20) {
            AddOtDirect(0xD, c, (void (*)()) clothTrans, 0, 0x1000, 0, 0.0f);
        }
    }
}

void clothTrans(Cloth* c)
{
    Mtx texMtx;
    GXTexObj* tex = c->tex;
    void* tex2 = c->x64;
    GXTlutObj* tlut = c->tlut;
    int nStages;
    int i;
    int j;
    int k;
    int t;
    f32 r;

    CameraSetProjection(1);
    PSMTXIdentity(texMtx);
    GXSetCullMode(0);
    GXSetZMode(1, 3, 1);
    GXSetNumChans(1);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 2, 1, 0);
    GXSetTevAlphaIn(0, 7, 4, 5, 7);
    GXSetTevAlphaOp(0, 0, 0, 2, 1, 0);
    GXSetTevOrder(0, 0, 0, 4);
    cModel model;
    u8 modelPad[0x320 - sizeof(cModel)];  // see the cModel size note above
    PSMTXIdentity(model.mat);
    {
        static const Vec p0 = {0.0f, 0.0f, 0.0f};
        static const Vec p1 = {10000.0f, 10000.0f, 10000.0f};
        model.lightInfo.init2(1, 0, &p0, &p1, 0x10);
    }
    model.pos = c->x4C;
    LightMgr.setClothN(&model, 8);
    if (model.lightInfo.size.x > model.lightInfo.size.y) {
        r = model.lightInfo.size.x;
    } else {
        r = model.lightInfo.size.y;
    }
    commonClothLightSet(model.lightInfo.pLight, 8, model.pos, r);
    GXSetChanMatColor(4, c->color);
    GXSetNumTexGens(1);
    GXLoadTexObj(tex, 0);
    if (tlut != 0) {
        GXLoadTlut(tlut, 0);
    }
    GXLoadTexMtxImm(texMtx, 0x1E, 1);
    GXSetTexCoordGen2(0, 1, 4, 0x1E, 0, 0x7D);
    if (c->x74 == 0) {
        GXSetBlendMode(1, 4, 5, 0);
    } else {
        GXSetBlendMode(1, 1, 1, 0);
        GXSetZMode(1, 3, 0);
    }
    GXSetAlphaCompare(4, 4, 1, 4, 0xFF);
    nStages = 1;
    if (tex2 != 0) {
        nStages = 2;
        GXSetZCompLoc(0);
        GXSetBlendMode(1, 4, 5, 0);
        GXLoadTexObj((GXTexObj*) tex2, 1);
        GXSetTevOrder(1, 0, 1, 4);
        GXSetTevOp(1, 0);
    }
    GXSetNumTevStages(nStages);
    {
        Mtx scale;
        Mtx tmp;
        PSMTXScale(scale, c->xC, c->xC, c->xC);
        PSMTXConcat(c->mat, scale, tmp);
        PSMTXConcat(pG->Cam.viewMat, tmp, scale);
        GXLoadPosMtxImm(scale, 0);
        PSMTXInverse(scale, tmp);
        PSMTXTranspose(tmp, scale);
        GXLoadNrmMtxImm(scale, 0);
    }
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(10, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 10, 0, 4, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    for (i = 0; i < c->ny - 1; i++) {
        GXBegin(0x98, 0, c->nx * 2);
        for (j = 0; j < c->nx * 2; j++) {
            Vec* pp;
            Vec* pn;
            if (j & 1) {
                k = c->nx * (i + 1) + j / 2;
                t = 1;
            } else {
                k = i * c->nx + j / 2;
                t = 0;
            }
            pp = &c->pos[k];
            pn = &c->nrm[k];
            GXPosition3f32(pp->x, pp->y, pp->z);
            GXNormal3f32(pn->x, pn->y, pn->z);
            GXTexCoord2f32((f32) (j / 2) / (f32) (c->nx - 1), (f32) (i + t) / (f32) (c->ny - 1));
        }
    }
}
