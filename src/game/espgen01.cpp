#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "main_sub.h"
#include "view.h"
#include "cam_ctrl.h"
#include "db_log.h"

// Effect controller 01: lens flare. Projects the light position to the screen and lays the
// est table sprites along the line to the screen centre; HideCheck samples the Z buffer.
struct Espgen01Work {
    Vec pos;           // 0x14 light offset (from the parts)
    Vec wpos;          // 0x20 world position
    Vec dir;           // 0x2C light direction
    f32 ang;           // 0x38 half of the visible cone angle
    f32 sizeRate;      // 0x3C
    f32 scaleRate;     // 0x40
    f32 dist;          // 0x44 fade distance
    u8 pad_48[4];
    u16 parts;         // 0x4C
    u8 pad_4E[2];
    cModel* model;     // 0x50
    u8 partsNo;        // 0x54
    u8 pad_55;
    u16 flags;         // 0x56 bit0: directional, bit1: hide check
    f32 sx;            // 0x58 screen position
    f32 sy;            // 0x5C
    f32 hideAlpha;     // 0x60
    f32 hideRadius;    // 0x64
    u8 owner;          // 0x68
    u8 estId;          // 0x69
    u16 camCnt;        // 0x6A frames to hide after a camera change
    u32 seed;          // 0x6C
};

extern "C" {
void espgen01_Move00(EspgenWork* w);
void espgen01_Move01(EspgenWork* w);
void SetEsp(EspgenWork* w);
u32 GetEstTblnum(EspSeqData* head);
cEsp* SetEstTbl(EspgenWork* w, EspSeqData* head, int no);
f32 GetDistAlpha(EspgenWork* w);
f32 GetDirAlpha(EspgenWork* w, Vec* dir);
void HideCheck(cEsp* esp);
}

void espgen01_Move00(EspgenWork* w)
{
    SetEsp(w);
    w->step = 1;
}

void espgen01_Move01(EspgenWork* w)
{
    SetEsp(w);
}

void Espgen01_Move(EspgenWork* w)
{
    static void (*Espgen01MoveTbl[])(EspgenWork*) = {espgen01_Move00, espgen01_Move01};

    Espgen01MoveTbl[w->step](w);
}

void Espgen01_Trans(EspgenWork* w)
{
    if ((w->flag & 1) && !(w->flag & 2)) {
        EspAddOtAfterRender((cEsp*) w, HideCheck);
    }
}

void SetEsp(EspgenWork* w)
{
    Espgen01Work* p = (Espgen01Work*) w->work;
    Vec v;
    Vec scr;
    Vec d;
    Vec dir;
    Mtx m;
    EspSeqData* head;
    u32 num;
    u32 i;
    cEsp* esp;
    f32 alpha;
    f32 x0;
    f32 s;

    head = EspGetEstAddr(p->owner, p->estId, 0);
    if (head == NULL) {
        pLog->err(0, 0, "ESP_FLARE : OWNER[%d] EST_ID[%d] invalid", p->owner, p->estId);
        PushEspgen(w);
        return;
    }
    num = GetEstTblnum(head);
    if (p->model == NULL || p->partsNo == 0xFE) {
        p->wpos = p->pos;
        dir = p->dir;
    } else {
        cModel* part;

        if (p->partsNo >= p->model->nParts) {
            pLog->err(0, 0, "ESP_FLARE :PARTS_NO[%d] is invalid(MAX:%d).", p->partsNo, p->model->nParts);
            PushEspgen(w);
            return;
        }
        part = p->model->getPartsPtr(p->partsNo);
        PSMTXMultVec(part->mat, &p->pos, &p->wpos);
        PSMTXCopy(part->mat, m);
        m[0][3] = 0.0f;
        m[1][3] = 0.0f;
        m[2][3] = 0.0f;
        PSMTXMultVec(m, &p->dir, &dir);
    }
    PSMTXMultVec(pG->Cam.viewMat, &p->wpos, &v);
    PSMTX44MultVec(pG->Cam.projMat, &v, &scr);
    scr.x = (scr.x * 0.5f + 0.5f) * Screen.width;
    scr.y = (-scr.y * 0.5f + 0.5f) * Screen.height;
    scr.z = 0.0f;
    p->sx = scr.x;
    p->sy = scr.y;
    if (v.z < 0.0f) {
        v.x = Screen.width * 0.5f;
        v.y = Screen.height * 0.5f;
        v.z = 0.0f;
        PSVECSubtract(&v, &scr, &d);
        alpha = PSVECMag(&d) / (Screen.height * (p->sizeRate * 0.7f));
        alpha *= alpha;
        alpha = 1.0f - alpha;
        if (p->flags & 1) {
            alpha *= GetDirAlpha(w, &dir);
        }
        alpha *= GetDistAlpha(w);
        if (p->flags & 2) {
            alpha *= p->hideAlpha;
        }
        if (alpha > 0.01f) {
            esp = SetEstTbl(w, head, 0);
            if (esp == EspGetDmyPtr()) {
                PushEspgen(w);
                return;
            }
            esp->partsNo = 0xF8;
            esp->life = 1;
            x0 = esp->pos.x;
            esp->pos.x = p->sx;
            esp->pos.y = p->sy;
            esp->colA *= alpha;
            if (p->scaleRate != 0.0f) {
                s = alpha * p->scaleRate + (1.0f - p->scaleRate);
                if (s < 0.0f) {
                    s = 0.0f;
                }
                esp->sizeX *= s;
                esp->sizeY *= s;
            }
            for (i = 1; i < num; i++) {
                esp = SetEstTbl(w, head, i);
                if (esp == EspGetDmyPtr()) {
                    PushEspgen(w);
                    return;
                }
                PSVECScale(&d, &v, (esp->pos.x - x0) / (Screen.width * 0.5f - x0));
                esp->partsNo = 0xF8;
                esp->life = 1;
                esp->pos.x = p->sx;
                esp->pos.y = p->sy;
                esp->colA *= alpha;
                if (p->scaleRate != 0.0f) {
                    s = alpha * p->scaleRate + (1.0f - p->scaleRate);
                    if (s < 0.0f) {
                        s = 0.0f;
                    }
                    esp->sizeX *= s;
                    esp->sizeY *= s;
                }
                PSVECAdd(&v, &esp->pos, &esp->pos);
            }
        }
    }
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
}

u32 GetEstTblnum(EspSeqData* head)
{
    return head->num;
}

cEsp* SetEstTbl(EspgenWork* w, EspSeqData* head, int no)
{
    Espgen01Work* p = (Espgen01Work*) w->work;
    EspGenWork* rec = head->rec;
    Mtx m;
    cEsp* esp;

    rec = &rec[no];
    PSMTXIdentity(m);
    EspSeqSet(rec, &w->info, &p->seed, p->model, &m, 0, 0.0f, &esp, NULL, NULL);
    return esp;
}

f32 GetDistAlpha(EspgenWork* w)
{
    Espgen01Work* p = (Espgen01Work*) w->work;
    Camera* cam;
    Vec d;
    f32 a;

    if (p->dist != 0.0f) {
        cam = &pG->Cam;
        d.x = p->wpos.x - cam->param.pos.x;
        d.y = p->wpos.y - cam->param.pos.y;
        d.z = p->wpos.z - cam->param.pos.z;
        a = PSVECMag(&d) / p->dist;
        if (a > 1.0f) {
            a = 1.0f;
        }
        if (a < 0.0f) {
            a = 0.0f;
        }
        return 1.0f - a;
    }
    return 1.0f;
}

f32 GetDirAlpha(EspgenWork* w, Vec* dir)
{
    Espgen01Work* p = (Espgen01Work*) w->work;
    Camera* cam;
    Vec d;
    f32 ang;
    f32 a;
    f32 c;

    ang = LIMIT_ANGLE(p->ang);
    cam = &pG->Cam;
    d.x = p->wpos.x - cam->param.pos.x;
    d.y = p->wpos.y - cam->param.pos.y;
    d.z = p->wpos.z - cam->param.pos.z;
#line 339 "D:/Bio4/Prog/espgen01.cpp"
    VECNormalize(&d, &d);
    a = -PSVECDotProduct(&d, dir);
    c = cosf(ang);
    a = a - c;
    if (a <= 0.0f) {
        a = 0.0f;
    } else {
        a = a / (1.0f - c);
    }
    return a;
}

void HideCheck(cEsp* esp)
{
    static f32 Zscale = 1.0f;
    static f32 Zoffset = 1.0f;
    static int Zs_bias = -5000;
    static f32 hide_x_tbl[12] = {0.0f, 0.5f, 0.86f, 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f};
    static f32 hide_y_tbl[12] = {1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f, 0.0f, 0.5f, 0.86f};
    EspgenWork* w = (EspgenWork*) esp;
    Espgen01Work* p = (Espgen01Work*) w->work;
    Vec v;
    Vec s;
    u32 z;
    int zval;
    u32 cnt;
    u32 i;
    f32 border;
    f32 a;
    f32 tmp;
    f32 m22;
    f32 m23;
    f32 iw;

    if (!(p->flags & 2)) {
        return;
    }
    PSMTXMultVec(pG->Cam.viewMat, &p->wpos, &v);
    v.z += 150.0f;
    tmp = 1.0f / (ZFAR - ZNEAR);
    m22 = -(ZNEAR) * tmp;
    m23 = -(ZFAR * ZNEAR) * tmp;
    iw = 1.0f / -v.z;
    m22 = m22 * v.z;
    zval = (u32) ((iw * ((m22 + m23) * Zscale) + Zoffset) * 16777215.0f);
    if (pG->flags_54 & 0x800) {
        border = 56.0f;
    } else {
        border = 0.0f;
    }
    GXPixModeSync();
    GXDrawDone();
    cnt = 0;
    for (i = 0; i < 12; i++) {
        s.x = hide_x_tbl[i] * p->hideRadius + p->sx;
        s.y = hide_y_tbl[i] * p->hideRadius + p->sy;
        if (s.x < 0.0f || s.x >= Screen.width || s.y < border + 0.0f || s.y >= Screen.height - border) {
            cnt++;
        } else {
            GXPeekZ((u16) s.x, (u16) s.y, &z);
            if (zval > (int) (z - Zs_bias)) {
                cnt++;
            }
        }
    }
    if (cnt == 12) {
        p->hideAlpha = 0.0f;
    } else {
        a = 1.0f - (f32) cnt * 0.1f;
        if (a < 0.0f) {
            a = 0.0f;
        }
        if (a > 1.0f) {
            a = 1.0f;
        }
        p->hideAlpha = (a - p->hideAlpha) * 0.6f + p->hideAlpha;
    }
    if (CamCtrl.IsChangeCamera()) {
        p->camCnt = 2;
    }
    if (p->camCnt != 0) {
        p->camCnt--;
        p->hideAlpha = 0.0f;
    }
}

int Espgen01_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8, int flag)
{
    Espgen01Work* p = (Espgen01Work*) w->work;
    Mtx m1;
    Mtx m2;
    f32 rx;
    f32 ry;
    f32 fov;

    p->pos = *(Vec*) &rec->x0C;
    p->owner = rec->xC8;
    p->estId = rec->xC9;
    p->flags = 0;
    p->parts = parts;
    p->model = model;
    p->partsNo = rec->x7;
    fov = rec->xF8;
    if (fov != 0.0f) {
        p->ang = fov * 6.2831855f / 360.0f * 0.5f;
        p->dir.x = 0.0f;
        p->dir.z = 1.0f;
        p->dir.y = 0.0f;
        rx = rec->xF0 * 6.2831855f / 360.0f;
        ry = rec->xF4 * 6.2831855f / 360.0f;
        rx = LIMIT_ANGLE(rx);
        ry = LIMIT_ANGLE(ry);
        PSMTXRotRad(m1, 'Y', ry);
        PSMTXRotRad(m2, 'X', rx);
        PSMTXConcat(m1, m2, m1);
        PSMTXMultVec(m1, &p->dir, &p->dir);
#line 482 "D:/Bio4/Prog/espgen01.cpp"
        VECNormalize(&p->dir, &p->dir);
        p->flags |= 1;
    }
    p->sizeRate = 1.0f - rec->xD8 * 0.01f;
    if (p->sizeRate > 1.0f) {
        p->sizeRate = 1.0f;
    }
    p->scaleRate = rec->xDC * 0.01f;
    p->dist = rec->xE0;
    if (rec->xE4 != 0.0f) {
        p->hideRadius = rec->xE4;
        p->flags |= 2;
    }
    return 1;
}

asm(".section .sdata; .balign 8");
