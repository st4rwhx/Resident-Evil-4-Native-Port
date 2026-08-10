#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "view.h"
#include "geometry.h"
#include "gx.h"
#include "db_log.h"
#include "main_mem.h"
#include "trans_ot.h"

extern "C" {
void* GetPrimBuff(int size);
}

// The original stores g_NowExecOtType through a reference: that keeps the following loads through the
// OtWork pointer below the store (a plain global store lets ProDG hoist them).
static inline void ISet(int& d, int v)
{
    d = v;
}

// Table `type`. As an inline accessor the constant index stays `addi 0x88` after the symbol load
// instead of folding into `g_OtWork+0x88`.
static inline OtWork* otWork(int type)
{
    return &g_OtWork[type];
}

// Depth of `pos` along the camera look vector.
static inline f32 OtDepth(Camera* cam, Vec* pos, Vec* look, Vec* d)
{
    CameraGetLookVecInverse(cam, look);
    d->x = pos->x - cam->param.pos.x;
    d->y = pos->y - cam->param.pos.y;
    d->z = pos->z - cam->param.pos.z;
    return PSVECDotProduct(look, d);
}

static int Ot_max_tbl[OT_MAX] = {
    10, 10, 10, 10, 10, 10, 10, 10, 3, 4, 3, 6, 8, 0x80, 3, 3, 5, 0x400, 10, 10, 3, 3, 1,
};

OtWork g_OtWork[OT_MAX];
OtMirrorWork g_OtMirrirWk[2];
f32 OT_MUL = 0.05f;
asm(".section .sdata; .balign 8");
int g_NowExecOtType;

void InitOt()
{
    OtWork* w = g_OtWork;
    u32 i;

    for (i = 0; i < OT_MAX; i++, w++) {
        w->max = Ot_max_tbl[i];
#line 53 "D:/Bio4/Prog/trans_ot.cpp"
        w->list = (OtData*) MEM_ALLOC(w->max * sizeof(OtData), 1, 13);
        w->prev_kind = 0;
    }
    ClearOt();
}

void ClearOt()
{
    OtWork* w = g_OtWork;
    u32 i;

    for (i = 0; i < OT_MAX; i++, w++) {
        clearOtWork(w);
    }
    CrearOtMirrorWork();
}

void clearOtWork(OtWork* w)
{
    OtData* p;
    OtData* q;

    w->prev_kind = 0;
    ISet(g_NowExecOtType, OT_MAX);
    p = &w->list[w->max - 1];
    do {
        q = p;
        p--;
        q->data = 0;
        q->next = p;
    } while (q > w->list);
    q->next = 0;
}

OtData* MakeOtData(void* data)
{
    OtPrim* p = (OtPrim*) GetPrimBuff(sizeof(OtPrim));

    if ((u32) p < 0x80000000 || (u32) p > 0x82FFFFFF) {
        return 0;
    }
    p->data = data;
    return &p->ot;
}

int AddOtWorldPos(void* data, void (*func)(void*), Vec* pos, u16 kind, f32 zlimit)
{
    Camera* cam = &pG->Cam;
    OtWork* w = otWork(17);
    OtData* p;
    OtData* q;
    Vec look;
    Vec d;
    int idx;
    u32 no;
    f32 z;

    p = MakeOtData(data);
    if (p == 0) {
        pLog->warn(2, 0, "AddOtWorldPos():PrimBuffer OVERFLOW!!");
        return 0xFFFF;
    }
    idx = 0xFFFF;
    if (zlimit == 0.0f) {
        z = 0.0f;
    } else {
        CameraGetLookVecInverse(cam, &look);
        d.x = pos->x - cam->param.pos.x;
        d.y = pos->y - cam->param.pos.y;
        d.z = pos->z - cam->param.pos.z;
        z = PSVECDotProduct(&look, &d);
    }
    if (z >= zlimit) {
        no = (u16) (z * OT_MUL);
        if (no >= w->max) {
            no = w->max - 1;
        }
        q = &w->list[no];
        idx = no;
        p->data = data;
        p->func = func;
        p->next = q->next;
        p->kind = kind;
        q->next = p;
    }
    return idx;
}

int AddOtWorldPosRadius(void* data, void (*func)(void*), Vec* pos, f32 radius, u16 kind, f32 zlimit)
{
    OtWork* w = otWork(17);
    Camera* cam;
    OtData* p;
    OtData* q;
    Vec look;
    Vec d;
    GeoSphere sph;
    GeoHexahedron* h;
    u32 no;
    f32 z;

    p = MakeOtData(data);
    if (p == 0) {
        pLog->warn(2, 0, "AddOtWorldPos():PrimBuffer OVERFLOW!!");
        return 0xFFFF;
    }
    cam = &pG->Cam;
    h = (GeoHexahedron*) CameraViewFrustumPtr(cam);
    sph.pos = *pos;
    sph.r = radius;
    if (!collision_sphere_hexahedron(&sph, h)) {
        return 0xFFFF;
    }
    CameraGetLookVecInverse(cam, &look);
    d.x = pos->x - cam->param.pos.x;
    d.y = pos->y - cam->param.pos.y;
    d.z = pos->z - cam->param.pos.z;
    z = PSVECDotProduct(&look, &d);
    if (z + radius < zlimit) {
        if (zlimit != 0.0f) {
            return 0xFFFF;
        }
        z = zlimit;
    }
    no = (u16) (z * OT_MUL);
    if (no >= w->max) {
        no = w->max - 1;
    }
    q = &w->list[no];
    p->data = data;
    p->func = func;
    p->next = q->next;
    p->kind = kind;
    q->next = p;
    return no;
}

int AddOtModelPosRadius(void* data, void (*func)(void*), Vec* pos, f32 radius, u16 kind, f32 zlimit)
{
    OtWork* w = otWork(13);
    Camera* cam;
    OtData* p;
    OtData* q;
    Vec look;
    Vec d;
    GeoSphere sph;
    GeoHexahedron* h;
    u32 no;
    f32 z;

    p = MakeOtData(data);
    if (p == 0) {
        pLog->warn(2, 0, "AddOtWorldPos():PrimBuffer OVERFLOW!!");
        return 0xFFFF;
    }
    cam = &pG->Cam;
    h = (GeoHexahedron*) CameraViewFrustumPtr(cam);
    sph.pos = *pos;
    sph.r = radius;
    if (!collision_sphere_hexahedron(&sph, h)) {
        return 0xFFFF;
    }
    CameraGetLookVecInverse(cam, &look);
    d.x = pos->x - cam->param.pos.x;
    d.y = pos->y - cam->param.pos.y;
    d.z = pos->z - cam->param.pos.z;
    z = PSVECDotProduct(&look, &d);
    if (z + radius < zlimit) {
        if (zlimit != 0.0f) {
            return 0xFFFF;
        }
        z = zlimit;
    }
    no = (u16) (z * 0.01f);
    if (no >= w->max) {
        no = w->max - 1;
    }
    q = &w->list[no];
    p->data = data;
    p->func = func;
    p->next = q->next;
    p->kind = kind;
    q->next = p;
    return no;
}

extern "C" int AddOtDirect(int ot, void* data, void (*func)(), u32 no, u16 flag, Vec* pos, f32 radius)
{
    OtWork* w;
    OtData* p;
    OtData* q;
    GeoSphere sph;
    GeoHexahedron* h;

    if (radius != 0.0f && pos != 0) {
        h = (GeoHexahedron*) CameraViewFrustumPtr(&pG->Cam);
        sph.pos = *pos;
        sph.r = radius;
        if (!collision_sphere_hexahedron(&sph, h)) {
            return 0xFFFF;
        }
    }
    w = otWork(ot);
    p = MakeOtData(data);
    if (p == 0) {
        pLog->warn(2, 0, "AddOtDirect():PrimBuffer OVERFLOW!!");
        return 0xFFFF;
    }
    if (no >= w->max) {
        no = (u16) (w->max - 1);
    }
    q = &w->list[no];
    p->data = data;
    p->func = (void (*)(void*)) func;
    p->next = q->next;
    p->kind = flag;
    q->next = p;
    return no;
}

int ExecOt(int type)
{
    OtWork* w = &g_OtWork[type];
    OtData* p;
    int count = 0;

    g_NowExecOtType = type;
    p = &w->list[w->max - 1];
    w->prev_kind = 0;
    if (p) {
        for (; p; p = p->next) {
            if (p->data) {
                count++;
                p->func(p->data);
                w->prev_kind = p->kind;
            }
        }
    }
    GXSetAlphaCompare(7, 0, 1, 7, 0);
    g_NowExecOtType = OT_MAX;
    return count;
}

u16 OtGetPrevKind()
{
    if (g_NowExecOtType == OT_MAX) {
        return 0;
    }
    return g_OtWork[g_NowExecOtType].prev_kind;
}

void CrearOtMirrorWork()
{
    int i;

    for (i = 0; i < 2; i++) {
        memclr_asm(&g_OtMirrirWk[i], sizeof(OtMirrorWork));
    }
}

// Dead-stripped by the original linker (STRIP_UNUSED); its message string remains.
static void SetOtMirrorWork(u32 no)
{
    if (no >= 2) {
        pLog->err(0, 0, "SetOtMirrorWork() : invalid no[%d] MAX=%d", no, 2);
    }
}

void DeleteOtData(u32 type, u32 no)
{
    OtWork* w;
    OtData* q;

    if (type >= OT_MAX) {
        pLog->err(0, 0, "DeleteOtData() : invalid ot_type[%d] MAX=%d", type, OT_MAX);
        return;
    }
    w = &g_OtWork[type];
    if (no >= w->max) {
        pLog->err(0, 0, "DeleteOtData() : invalid no[%d] MAX=%d", no, w->max);
        return;
    }
    q = &w->list[no];
    q->next = q->next->next;
}
