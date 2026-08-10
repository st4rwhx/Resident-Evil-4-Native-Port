#include "types.h"
#include "atari.h"
#include "event.h"
#include "light.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "math_sub.h"
#include "em.h"
#include "obj.h"
#include "scroll.h"
#include "db_work.h"

extern "C" {
void Draw_pos(Vec* pos, int size);
void Draw_sphere(Vec pos, f32 r, int color, int zcmp, int zupd);
}

// obj18 work (cObj::work) as far as the viewer reads it
struct DbObj18Work {
    u8 pad_0[0x68];
    int type;        // 0x68
    u8 pad_6C[0xC];
    char name[0x30]; // 0x78
};

cDbWork::cDbWork()
{
    no = mode = 0;
}

void cDbWork::move()
{
    if (pG->debug_mode != 11) {
        return;
    }
    eprintf(16, 14, 0, 0, "MODEL WORK VIEWER");
    if (Joy[0].rep & JOY_UP) {
        switch (mode) {
        case 0:
            mode = 2;
            break;
        case 1:
            mode = 0;
            break;
        case 2:
            mode = 1;
            break;
        }
    }
    if (Joy[0].rep & JOY_DOWN) {
        switch (mode) {
        case 0:
            mode = 1;
            break;
        case 1:
            mode = 2;
            break;
        case 2:
            mode = 0;
            break;
        }
    }
    switch (mode) {
    case 0:
        dispEm();
        break;
    case 1:
        dispObj();
        break;
    case 2:
        dispLit();
        break;
    }
}

void cDbWork::dispEm()
{
    cEm* em;

    eprintf(32, 28, 4, 0, "ENEMY %d", no);
    em = EmMgrWork(no);
    if (Joy[0].rep & JOY_RIGHT) {
        no++;
    }
    if (Joy[0].rep & JOY_LEFT) {
        no--;
    }
    no = (no + EmMgr.nArray) % EmMgr.nArray;
    if ((em->be_flag & 0x201) == 1) {
        dispModel(em, 4, 3);
        eprintf(32, 280, 0, 0, "HP       %d", em->hp);
        eprintf(32, 294, 0, 0, "HP MAX   %d", em->hpMax);
        eprintf(32, 308, 0, 0, "L PL     %f", SQRTF(em->plDist2));
        eprintf(32, 322, 0, 0, "EMSET NO %d", em->emsetNo);
        Draw_pos(&em->pos, 500);
    }
}

void cDbWork::dispObj()
{
    cObj* obj;
    int x;
    int y;

    obj = ObjMgrWork(no);
    eprintf(32, 28, 4, 0, "OBJ %d  [0x%08X]", no, obj);
    if (Joy[0].rep & JOY_RIGHT) {
        no++;
    }
    if (Joy[0].rep & JOY_LEFT) {
        no--;
    }
    no = (no + ObjMgr.nArray) % ObjMgr.nArray;
    if ((obj->be_flag & 0x201) == 1) {
        dispModel(obj, 4, 3);
        x = 4;
        y = 20;
        if (obj->id == 2) {
            int id;
            eprintf(32, 280, 0, 0, "ATTR     %02X", obj->x3D0);
            y++;
            id = SmdGetWorkId(obj);
            if (id >= 0) {
                eprintf(32, 294, 0, 0, "SCR-ID  %3d", id);
            } else {
                eprintf(32, 294, 0, 0, "SCR-ID  ---");
            }
        }
        if (obj->id == 0x18) {
            DbObj18Work* w = (DbObj18Work*) obj->work;
            eprintf(x * 8, y * 14, 0, 0, "NAME     %s", w->name);
            y++;
            eprintf(x * 8, y * 14, 0, 0, "TYPE     %2d", w->type);
        }
        Draw_pos(&obj->pos, 1000);
    }
}

void cDbWork::dispModel(cModel* m, int x, int y)
{
    int color;

    x *= 8;
    eprintf(x, y * 14, 0, 0, "BE FLAG  %08X", m->be_flag);
    y++;
    eprintf(x, y * 14, 0, 0, "POSITION %7.0f %7.0f %7.0f", m->pos.x, m->pos.y, m->pos.z);
    y++;
    eprintf(x, y * 14, 0, 0, "ANGLE    %4.2f %4.2f %4.2f", m->rot.x, m->rot.y, m->rot.z);
    y++;
    eprintf(x, y * 14, 0, 0, "SCALE    %4.2f %4.2f %4.2f", m->scale.x, m->scale.y, m->scale.z);
    y++;
    eprintf(x, y * 14, 0, 0, "RTN NO   %02X %02X %02X %02X", m->xFC, m->xFD, m->xFE, m->xFF);
    y++;
    eprintf(x, y * 14, 0, 0, "ID       %02X", m->id);
    y++;
    eprintf(x, y * 14, 0, 0, "TYPE     %02X", m->type);
    y++;
    eprintf(x, y * 14, 0, 0, "nParts   %02X", m->nParts);
    y++;
    eprintf(x, y * 14, 0, 0, "SPEED    %7.0f %7.0f %7.0f", m->speed.x, m->speed.y, m->speed.z);
    y++;
    eprintf(x, y * 14, 0, 0, "pCldShMd %08X", m->pCldShMd);
    y++;
    eprintf(x, y * 14, 0, 0, "SHD COL  %02X", m->shdCol);
    y++;
    eprintf(x, y * 14, 0, 0, "CullMode %d", m->x135);
    y++;
    eprintf(x, y * 14, 0, 0, "pModInfo %08X", m->pInfo);
    y++;
    eprintf(x, y * 14, 0, 0, "pShMdIfo %08X", m->pShMdInfo);
    y++;
    color = 0;
    if (m->lightInfo.getLightNum() > 5) {
        color = 0x16;
    }
    eprintf(x, y * 14, color, 0, "nLight   %d", m->lightInfo.getLightNum());
    if (Joy[0].on & JOY_A) {
        if (m->pInfo != NULL) {
            m->pInfo->color[0] = ~m->pInfo->color[0];
            m->pInfo->color[1] = ~m->pInfo->color[1];
            m->pInfo->color[2] = ~m->pInfo->color[2];
        }
    } else {
        if (m->pInfo != NULL) {
            m->pInfo->color[0] = 0xFF;
            m->pInfo->color[1] = 0xFF;
            m->pInfo->color[2] = 0xFF;
        }
    }
    if (Joy[0].on & JOY_X) {
        if (m->scale.y == 0.2f) {
            m->scale.y = 1.0f;
        } else {
            m->scale.y = 0.2f;
        }
    }
    if (Joy[0].trg & 0x800000) {
        m->pos.y += 1000.0f;
        m->matUpdate();
    }
    if (Joy[0].trg & 0x400000) {
        m->pos.y -= 1000.0f;
        m->matUpdate();
    }
    m->drawAllBoundingBox(m->pInfo);
}

void cDbWork::dispLit()
{
    cLight* l;

    eprintf(32, 28, 4, 0, "LIGHT %d", no);
    l = LightMgr.getWorkPtr(no);
    if (Joy[0].rep & JOY_RIGHT) {
        no++;
    }
    if (Joy[0].rep & JOY_LEFT) {
        no--;
    }
    no = (no + LightMgr.nArray) % LightMgr.nArray;
    if ((l->be_flag & 0x201) == 1) {
        eprintf(32, 280, 0, 0, "BE FLAG  %08X", l->be_flag);
        eprintf(32, 294, 0, 0, "POSITION %7.0f %7.0f %7.0f", l->pos.x, l->pos.y, l->pos.z);
        eprintf(32, 308, 0, 0, "ATTR     %02x", l->attr);
        Draw_sphere(l->curPos, l->x1C, -1, 1, 1);
    }
}
