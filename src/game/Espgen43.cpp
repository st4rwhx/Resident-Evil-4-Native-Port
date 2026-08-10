#include "light.h"
#include "atari.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "rnd.h"
#include "camera.h"
#include "os_vi.h"
#include "db_log.h"

// Byte-identical. AddSandPower's `stfs f1, Add_power` is an asm with a hard-register anti-dependence
// (see the COMPILER-DIFF note there); everything else is plain C.
// Effect controller 43: sand surface. A (nx+1) x (ny+1) height grid drawn as triangle strips
// through a prebuilt display list; AddSandPower pushes the grid down around a world position
// and GetSandHeight samples it (obj09).
struct Espgen43Work {
    Mtx mat;           // 0x14 grid -> world
    Mtx inv;           // 0x44 world -> grid
    u16 nx;            // 0x74 grid cells along x
    u16 ny;            // 0x76 grid cells along z
    u8 pad_78[4];
    f32 size;          // 0x7C cell size
    Vec* nrm;          // 0x80
    Vec* pos;          // 0x84
    u8* dl;            // 0x88 display list
    u32 dlSize;        // 0x8C
    GXColor color;     // 0x90
    GXColor color2;    // 0x94
    u8 texId;          // 0x98
    u8 texRep;         // 0x99 texture repeats across the grid
};

extern "C" {
// game/trans_lit.cpp
void commonClothLightSet(cLight** list, int n, Vec pos, f32 radius);
// game/espgen.cpp
int EspgenApplyFunc(void (*func)(EspgenWork* w));

void AddSandPowerSub(EspgenWork* w);
void GetSandHeightSub(EspgenWork* w);
void Espgen43_Move00(EspgenWork* w);
void Espgen43_TransSub(EspgenWork* w);
EspgenWork* SetSandWork(EspgenWork* w, Vec* pos, Vec* rot, f32 size, f32 sizeRate, u32 nx, u32 ny);
}

static Vec Chk_pos;
static f32 Height_ret;
static f32 Add_power;
static int Height_find;
static inline void ISet(int& d, int v) { d = v; }
static inline f32 FGet(f32& d) { return d; }

void AddSandPowerSub(EspgenWork* w)
{
    Espgen43Work* p;
    Vec v;
    u32 x;
    u32 z;
    int idx;
    int total;
    int stride;
    int i;
    int j;
    int k;

    if (w->id != 0x43) {
        return;
    }
    p = (Espgen43Work*) w->work;
    v = Chk_pos;
    PSMTXMultVec(p->inv, &v, &v);
    if (v.x < (f32) (-p->nx / 2)) {
        return;
    }
    if (v.z < (f32) (-p->ny / 2)) {
        return;
    }
    if (v.x > (f32) (p->nx / 2)) {
        return;
    }
    if (v.z > (f32) (p->ny / 2)) {
        return;
    }
    z = (u32) (v.z + (f32) (p->ny / 2));
    x = (u32) (v.x + (f32) (p->nx / 2));
    idx = z * (p->nx + 1) + x;
    p->pos[idx].y += FGet(Add_power);
    total = (p->ny + 1) * (p->nx + 1);
    stride = p->nx + 1;
    for (i = -3; i <= 3; i++) {
        for (j = -3; j <= 3; j++) {
            k = idx + i + j * (p->nx + 1);
            if (k <= total && k >= stride) {
                p->pos[k].y -= FGet(Add_power) * 0.02f;
            }
        }
    }
    for (i = -2; i <= 2; i++) {
        for (j = -2; j <= 2; j++) {
            k = idx + i + j * (p->nx + 1);
            if (k <= total && k >= stride) {
                p->pos[k].y -= FGet(Add_power) * 0.1f;
            }
        }
    }
    for (i = -1; i <= 1; i++) {
        for (j = -1; j <= 1; j++) {
            k = idx + i + j * (p->nx + 1);
            if (k <= total && k >= 0) {
                p->pos[k].y += FGet(Add_power) * 0.35f;
            }
        }
    }
    for (i = -3; i <= 3; i++) {
        for (j = -3; j <= 3; j++) {
            k = idx + i + j * (p->nx + 1);
            if (k <= total && k >= stride) {
                p->pos[k].y = p->pos[k].y * 2.5f + p->pos[k + 1].y * 0.5f + p->pos[p->ny + k + 1].y * 0.5f +
                              p->pos[p->ny + k + 2].y * 0.5f;
                p->pos[k].y *= 0.25f;
            }
        }
    }
}

void AddSandPower(Vec* pos, f32 power)
{
    if (pG->flags_500C & 2) {
        // COMPILER-DIFF: word copy with the .z word pinned to r11 (the original issues `stfs Add_power`
        // in the first cycle in both schedulers). Scalar `u32` loads are not MEM_IN_STRUCT_P, so the
        // plain `Add_power` store gates them (priority 7) and takes the first cycle in sched1 too;
        // that moves the Chk_pos high's birth one slot later, which would rank it above the .z word
        // (2 refs) in local-alloc and give it r11. Pinning the .z word removes that qty: high takes
        // r10 (r11 busy), addi r8, W0 r0, W4 r9 as in the original. Order 0,4,8 keeps the addi's
        // death on the .z store so sched1 issues S0, S8, S4.
        register u32 c asm("r11");
        u32* s = (u32*) pos;
        u32* d = (u32*) &Chk_pos;
        u32 a, b;
        Add_power = power;
        ISet(Height_find, 0);
        a = s[0];
        b = s[1];
        c = s[2];
        d[0] = a;
        d[1] = b;
        d[2] = c;
        EspgenApplyFunc(AddSandPowerSub);
    }
}

void GetSandHeightSub(EspgenWork* w)
{
    Espgen43Work* p;
    Vec v;

    if (w->id != 0x43) {
        return;
    }
    p = (Espgen43Work*) w->work;
    v = Chk_pos;
    PSMTXMultVec(p->inv, &v, &v);
    if (v.x < (f32) (-p->nx / 2)) {
        return;
    }
    if (v.z < (f32) (-p->ny / 2)) {
        return;
    }
    if (v.x > (f32) (p->nx / 2)) {
        return;
    }
    if (v.z > (f32) (p->ny / 2)) {
        return;
    }
    v.y = 0.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (v.y > Height_ret) {
        Height_ret = v.y;
    }
    Height_find = 1;
}

int GetSandHeight(Vec* pos, f32* height)
{
    if (!(pG->flags_500C & 2)) {
        return 0;
    }
    ISet(Height_find, 0);
    FSet(Height_ret, -100000000.0f);
    Chk_pos = *pos;
    EspgenApplyFunc(GetSandHeightSub);
    *height = Height_ret;
    return Height_find;
}

#line 246 "D:/Bio4/Prog/Espgen43.cpp"
void Espgen43_Move00(EspgenWork* w)
{
    Espgen43Work* p = (Espgen43Work*) w->work;
    Vec v;
    int i;
    int j;
    int k;
    u32 n;

    pG->flags_500C |= 2;
    for (i = 1; i < p->ny; i++) {
        k = i * (p->nx + 1);
        for (j = 1; j < p->nx; j++) {
            Vec* n = &p->nrm[k];
            Vec* q = &p->pos[k];
            v.x = q[-1].y - q[1].y;
            v.y = 2.0f;
            v.z = p->pos[k - p->nx].y - p->pos[k + p->nx].y;
            VECNormalize(&v, n);
            k++;
        }
    }
    n = sizeof(Vec) * (p->nx + 1) * (p->ny + 1);
    DCStoreRange(p->pos, n);
    DCStoreRange(p->nrm, n);
}

void Espgen43_Move(EspgenWork* w)
{
    static void (*Espgen43MoveTbl[])(EspgenWork*) = {Espgen43_Move00};

    Espgen43MoveTbl[w->step](w);
}

void Espgen43_Trans(EspgenWork* w)
{
    if ((w->flag & 1) && !(w->flag & 2)) {
        AddOtDirect(0x10, w, (void (*)()) Espgen43_TransSub, 1, 0x80, NULL, 0.0f);
    }
}

void Espgen43_TransSub(EspgenWork* w)
{
    GxStageWork* st;
    Espgen43Work* p;
    GXTexObj* tex;
    GXTlutObj* tlut;
    f32 r;

    if (!(w->flag & 1)) {
        return;
    }
    if (w->flag & 2) {
        return;
    }
    st = &pG->gxStage;
    p = (Espgen43Work*) w->work;
    st->tevStage = 0;
    st->texMap = 0;
    st->texCoord = 0;
    CameraCurrentProjection();
    GXSetCullMode(0);
    GXSetZMode(1, 3, 1);
    cModel model;
    u8 modelPad[0x320 - sizeof(cModel)];
    PSMTXIdentity(model.mat);
    {
        static const Vec p0 = {0.0f, 0.0f, 0.0f};
        static const Vec p1 = {10000.0f, 10000.0f, 10000.0f};
        model.lightInfo.init2(1, 0, &p0, &p1, 0x10);
    }
    model.pos.x = p->mat[0][3];
    model.pos.y = p->mat[1][3];
    model.pos.z = p->mat[2][3];
    LightMgr.setClothN(&model, 5);
    if (model.lightInfo.size.x > model.lightInfo.size.y) {
        r = model.lightInfo.size.x;
    } else {
        r = model.lightInfo.size.y;
    }
    commonClothLightSet(model.lightInfo.pLight, 5, model.pos, r);
    GXSetChanMatColor(4, p->color);
    {
        Mtx nrm;
        Mtx mv;
        PSMTXConcat(pG->Cam.viewMat, p->mat, mv);
        PSMTXInverse(mv, nrm);
        PSMTXTranspose(nrm, nrm);
        GXLoadNrmMtxImm(nrm, 0);
        GXLoadPosMtxImm(mv, 0);
    }
    GXSetBlendMode(1, 4, 5, 0);
    tex = EspGetTexObj(p->texId, 0);
    if (tex == NULL) {
        tex = &Specular;
    }
    GXLoadTexObj(tex, st->texMap);
    GXSetTexCoordGen2(st->texCoord, 1, 4, 0x3C, 0, 0x7D);
    tlut = EspGetTlutObj(p->texId);
    if (tlut != NULL) {
        GXLoadTlut(tlut, 0);
    }
    GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
    GXSetTevColorIn(st->tevStage, 0xF, 0xA, 8, 0xF);
    GXSetTevColorOp(st->tevStage, 0, 0, 2, 1, 0);
    GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
    GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
    st->tevStage++;
    st->texMap++;
    st->texCoord++;
    GXSetNumTevStages(st->tevStage);
    GXSetNumTexGens(st->texCoord);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 3);
    GXSetVtxDesc(10, 3);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 10, 0, 4, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXSetArray(9, p->pos, sizeof(Vec));
    GXSetArray(10, p->nrm, sizeof(Vec));
    GXCallDisplayList(p->dl, p->dlSize);
}

// Dead-stripped from the DOL (pool and string kept): pulls a generator and sets the grid up.
static int SetSand(Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny)
{
    EspgenWork* w;

    if (PullEspgen(&w) == 0) {
        pLog->err(0, 0, "Espgen43 : work pull failed");
        return 0;
    }
    return (int) SetSandWork(w, pos, rot, size, 1.0f, nx, ny);
}

// Texture coordinate wrap: keeps the repeat in 0..1 by mirroring at 1.
#define TEX_WRAP(v)                                                                                 \
    while ((v) > 2.0f) {                                                                            \
        (v) -= 2.0f;                                                                                \
    }                                                                                               \
    while ((v) > 1.0f) {                                                                            \
        (v) = 2.0f - (v);                                                                           \
    }

EspgenWork* SetSandWork(EspgenWork* w, Vec* pos, Vec* rot, f32 size, f32 sizeRate, u32 nx, u32 ny)
{
    Espgen43Work* p = (Espgen43Work*) w->work;
    Mtx m;
    u32 n;
    u8* d;
    int i;
    int j;
    int k;
    f32 rep;
    f32 fx;
    f32 fy;

    w->id = 0x43;
    p->nx = nx;
    p->ny = ny;
    p->size = size;
    RotMatrix(p->mat, rot);
    PSMTXScale(m, p->size, p->size * sizeRate, p->size);
    PSMTXConcat(p->mat, m, p->mat);
    PSMTXTransApply(p->mat, p->mat, pos->x, pos->y, pos->z);
    PSMTXInverse(p->mat, p->inv);
    n = sizeof(Vec) * (p->nx + 1) * (p->ny + 1);
#line 445 "D:/Bio4/Prog/Espgen43.cpp"
    p->pos = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->pos == NULL) {
        pLog->err(0, 0, "Espgen43 : not enough memory");
        PushEspgen(w);
        return 0;
    }
    memclr_asm(p->pos, n);
#line 452 "D:/Bio4/Prog/Espgen43.cpp"
    p->nrm = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->nrm == NULL) {
        pLog->err(0, 0, "Espgen43 : not enough memory");
        PushEspgen(w);
        return 0;
    }
    memclr_asm(p->nrm, n);
    p->dlSize = ((p->nx + 1) * (p->ny + p->ny) * 12 + 0x61) & ~0x1F;
#line 466 "D:/Bio4/Prog/Espgen43.cpp"
    p->dl = (u8*) MEM_ALLOC(p->dlSize, 1, 13);
    if (p->dl == NULL) {
        pLog->err(0, 0, "Espgen43 : not enough memory");
        PushEspgen(w);
        return 0;
    }
    memclr_asm(p->dl, p->dlSize);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 3);
    GXSetVtxDesc(10, 3);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 10, 0, 4, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    d = p->dl;
    *d = 0;
    d++;
    *d = 0x98;
    d++;
    *(u16*) d = (p->nx + 1) * (p->ny + p->ny);
    d++;
    d++;
    rep = p->texRep;
    for (i = 0; i < p->ny; i++) {
        k = i * (p->nx + 1);
        for (j = 0; j < p->nx + 1; j++) {
            *(u16*) d = k;
            d++;
            d++;
            *(u16*) d = k;
            d++;
            d++;
            k++;
            *(f32*) d = (f32) j / p->nx * rep;
            TEX_WRAP(*(f32*) d);
            d++;
            d++;
            d++;
            d++;
            *(f32*) d = (f32) i / p->ny * rep;
            TEX_WRAP(*(f32*) d);
            d++;
            d++;
            d++;
            d++;
            *(u16*) d = p->nx + k;
            d++;
            d++;
            *(u16*) d = p->nx + k;
            d++;
            d++;
            *(f32*) d = (f32) j / p->nx * rep;
            TEX_WRAP(*(f32*) d);
            d++;
            d++;
            d++;
            d++;
            *(f32*) d = (f32) (i + 1) / p->ny * rep;
            TEX_WRAP(*(f32*) d);
            d++;
            d++;
            d++;
            d++;
        }
        i++;
        if (i < p->ny) {
            for (j = p->nx; j >= 0; j--) {
                k = i * (p->nx + 1) + j;
                *(u16*) d = k;
                d++;
            d++;
                *(u16*) d = k;
                d++;
            d++;
                *(f32*) d = (f32) j / p->nx * rep;
                TEX_WRAP(*(f32*) d);
                d++;
            d++;
            d++;
            d++;
                k++;
                *(f32*) d = (f32) i / p->ny * rep;
                TEX_WRAP(*(f32*) d);
                d++;
            d++;
            d++;
            d++;
                *(u16*) d = p->nx + k;
                d++;
            d++;
                *(u16*) d = p->nx + k;
                d++;
            d++;
                *(f32*) d = (f32) j / p->nx * rep;
                TEX_WRAP(*(f32*) d);
                d++;
            d++;
            d++;
            d++;
                *(f32*) d = (f32) (i + 1) / p->ny * rep;
                TEX_WRAP(*(f32*) d);
                d++;
                d++;
                d++;
                d++;
            }
        }
    }
    fy = 0.0f;
    {
        int x;
        int y;
        int idx;
        for (y = 0; y < p->ny + 1; y++) {
            fx = 0.0f;
            idx = y * (p->nx + 1);
            for (x = 0; x < p->nx + 1; x++) {
                p->pos[idx].x = fx - (f32) (p->nx / 2);
                p->pos[idx].y = fRand1_1() * 0.15f;
                p->pos[idx].z = fy - (f32) (p->ny / 2);
                p->nrm[idx].x = 0.0f;
                p->nrm[idx].y = 1.0f;
                p->nrm[idx].z = 0.0f;
                fx += 1.0f;
                idx++;
            }
            fy += 1.0f;
        }
    }
    {
        u32 n2 = sizeof(Vec) * (p->nx + 1) * (p->ny + 1);
        DCStoreRange(p->pos, n2);
        DCStoreRange(p->nrm, n2);
    }
    DCStoreRange(p->dl, p->dlSize);
    return w;
}

void Espgen43_Destruct(EspgenWork* w)
{
    Espgen43Work* p = (Espgen43Work*) w->work;

    if (p->pos != NULL) {
        Mem_free(p->pos);
        p->pos = NULL;
    }
    if (p->nrm != NULL) {
        Mem_free(p->nrm);
        p->nrm = NULL;
    }
    if (p->dl != NULL) {
        Mem_free(p->dl);
        p->dl = NULL;
    }
}

int Espgen43_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8)
{
    Espgen43Work* p = (Espgen43Work*) w->work;
    Vec r;
    u32 nx = 0x40;
    u32 ny = 0x40;

    if (rec->prm.w.xCC != 0) {
        nx = rec->prm.w.xCC;
        if (nx > 0x100) {
            nx = 0x100;
        }
    }
    if (rec->prm.w.xD0 != 0) {
        ny = rec->prm.w.xD0;
        if (ny > 0x100) {
            ny = 0x100;
        }
    }
    p->color.r = rec->x9C;
    p->color.g = rec->x9D;
    p->color.b = rec->x9E;
    p->color.a = rec->x9F;
    p->color2.r = rec->xA0 * 255.0f;
    p->color2.g = rec->xA4 * 255.0f;
    p->color2.b = rec->xA8 * 255.0f;
    p->color2.a = rec->xAC * 255.0f;
    p->texId = rec->x2;
    p->texRep = 1 << (s8) rec->xC8;
    PSVECScale(&rec->x58, &r, 6.28f / 360.0f);
    if (SetSandWork(w, (Vec*) &rec->x0C, &r, rec->x88, rec->x94 + 1.0f, nx, ny) == NULL) {
        return 0;
    }
    Espgen43_Move(w);
    return 1;
}

asm(".section .sdata; .balign 8");
