#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "est.h"
#include "math_sub.h"
#include "rnd.h"
#include "player.h"
#include "area.h"
#include "flr_at.h"
#include "at_sub2.h"
#include "snd.h"
#include "db_log.h"

cModel* EspEvModList[0x80];

// Effect set table: starts effect controller 10 on the est data block `head`.
void EstSet(cModel* model, int no, Vec* pos, Vec* rot, EspSeqData* head, int e, int f, u32 g, u32 owner, void* h);

void EstSet(int a, int b, Vec* pos, Vec* rot, int c, int d, int e, int f, u32 g, void* h)
{
    EspSeqData* head = EspGetEstAddr(c, d, 0);

    EstSet((cModel*) a, b, pos, rot, head, e, f, g, c, h);
}

void EstSet(cModel* model, int no, Vec* pos, Vec* rot, EspSeqData* head, int e, int f, u32 g, u32 owner, void* h)
{
    EspgenWork* w;
    Espgen10Work* p;

    if (pG->flags_64 & 0x01000000) {
        return;
    }
    if (head == NULL) {
        return;
    }
    if (head->num == 0) {
        pLog->warn(0, 0, "EstSet():EST is enpty.");
        return;
    }
    if (pG->flags_5014 & 0x00080000) {
        e |= 0x2000;
    }
    if (pG->flags_5014 & 0x02000000) {
        e |= 1;
    }
    if (!PullEspEspgen(&w, e, f, (u8) EspgenGetCallNo(), g, owner, 1)) {
        return;
    }
    EspgenIncCallNo();
    w->id = 0x10;
    p = (Espgen10Work*) w->work;
    p->head = head;
    p->model = model;
    if (model != NULL) {
        p->serial = model->serial;
    } else {
        p->serial = (u32) model;
    }
    p->no = p->cnt = 0;
    if (no == -1) {
        p->parts = head->parts;
    } else {
        p->parts = no;
    }
    if (pos == NULL) {
        p->pos = head->pos;
    } else {
        p->flags |= 2;
        p->pos = *pos;
    }
    if (rot == NULL) {
        p->rot = head->rot;
        PSVECScale(&p->rot, &p->rot, 3.14f / 180.0f);
    } else {
        p->rot = *rot;
    }
    p->seed = Rnd() | (Rnd() << 8) | (Rnd() << 16);
    if (h != NULL) {
        p->p8 = &p->opt;
        p->opt = *(EspSeqOpt*) h;
    } else {
        p->p8 = (EspSeqOpt*) h;
    }
}

// Sets the room effects whose area the player stands in.
void AreaSstSet(int id)
{
    cEspSystem* sys = g_pEspSys;
    SstAreaEnt* ent;
    Vec pos;
    u32 flag;
    u32 i;
    u32 j;

    if (sys->pSstArea == NULL) {
        return;
    }
    pos = pPL->pos;
    pos.y += 100.0f;
    flag = 0;
    ent = sys->pSstArea->ent;
    for (i = 0; i < sys->pSstArea->num; i++, ent++) {
        if (AreaHitCheck(ent->area, &pos) == 1) {
            flag |= 1 << ent->bit;
        }
    }
    for (j = 0; j < 32; j++) {
        if (flag & (1 << j)) {
            SstSet(1, (u16) j, j + 0xC, id, id, 0);
        }
    }
}

int GetSstDispFlag(u32 id)
{
    cEspSystem* sys = g_pEspSys;

    if (id > 0x1F) {
        pLog->err(0, 0, "GetSstDispFlag() : id[%02x] invalid .", id);
        return 0;
    }
    if (sys->sstDispFlag & (1 << id)) {
        return 1;
    }
    return 0;
}

void SetSstDispFlag(u32 id, int on)
{
    cEspSystem* sys = g_pEspSys;

    if (id > 0x1F) {
        pLog->err(0, 0, "GetSstDispFlag() : id[%02x] invalid .", id);
        return;
    }
    if (on == 1) {
        if (GetSstDispFlag(id) == 0) {
            sys->sstDispFlag |= on << id;
            AreaSstSet(id);
        } else {
            sys->sstDispFlag |= on << id;
        }
    } else {
        sys->sstDispFlag &= ~(1 << id);
    }
}

void SetSstAddAreaFlag(u32 flag)
{
    g_pEspSys->sstAddAreaFlag = flag;
}

// Starts every effect of owner `owner` whose room key lies in [lo, hi] and whose type is `type`.
void SstSet(u32 owner, int type, int no, int lo, int hi, int move)
{
    cEspSystem* sys = g_pEspSys;
    SstTbl* tbl;
    SstList* list;
    u32* ofs;
    u32 i;

    if (owner > 0xD2) {
        pLog->err(0, 0, "GetSstAddr():Invalid OWNER_ID[%x].", owner);
        return;
    }
    tbl = &sys->sstTbl[owner];
    if (tbl->owner == 0xD2) {
        return;
    }
    list = tbl->list;
    for (i = 0; i < list->num; i++) {
        if (list->ent[i].no < (u16) lo || list->ent[i].no > (u16) hi) {
            continue;
        }
        if (list->ent[i].type != type) {
            continue;
        }
        if (!GetSstDispFlag(list->ent[i].b.id)) {
            continue;
        }
        ofs = tbl->data->ofs;
        ofs += i;
        EstSet(NULL, -1, NULL, NULL, (EspSeqData*) ((u8*) tbl->data + *ofs), 0x4001, (u8) no, 0, 0xD0, NULL);
    }
    if (move) {
        EspGenSetMoveLoop(200);
        EspGenLoopMove();
    }
}

void EffectEspDelete(int a, int b, u32 c, cModel* model)
{
    EspDelete(a, b, c, model);
}

void EffectEspgenDelete(int a, int b, int c)
{
    EspgenDelete(a, b, c);
}

void EffectEfmDelete(int a, int b, int c)
{
    EfmDelete(a, b, c);
}

void EffectDeleteAll()
{
    pG->flags_5010 &= ~0x20;
    EspArrayClear();
    EspgenArrayClear();
    EfmArrayClear();
}

void EffectEventDelete()
{
    EspDeleteEvent();
    EspgenDeleteEvent();
    EfmDeleteEvent();
}

void EspDelete(int a, int b, u32 c, cModel* model)
{
    cEspSystem* sys = g_pEspSys;
    u32 i;

    for (i = 0; i < sys->xC554; i++) {
        cEsp* esp = (cEsp*) (sys->pEspBuf + i * 0x150);

        if ((esp->flag & 1) == 0) {
            continue;
        }
        if (a != 0 && esp->info.x0 != a) {
            continue;
        }
        if (b != 0 && esp->info.x2 != b) {
            continue;
        }
        if (c != 0 && esp->info.x8 != c) {
            continue;
        }
        if (model != NULL) {
            if (esp->pModel != model) {
                continue;
            }
            if (esp->x20 != model->serial) {
                continue;
            }
        }
        PushEsp(esp);
    }
}

void EspDeleteEvent()
{
    cEspSystem* sys = g_pEspSys;
    u32 i;

    for (i = 0; i < sys->xC554; i++) {
        cEsp* esp = (cEsp*) (sys->pEspBuf + i * 0x150);

        if (esp->flag & 1) {
            int ev = !(esp->info.x0 & 1);

            if (ev && !(esp->info.x0 & 0x800)) {
                PushEsp(esp);
            }
        }
    }
}

void EspSetWaterBomb(Vec* pos)
{
    if (pG->room_id == 0x10A || pG->room_id == 0x10B || pG->room_id == 0x11A || pG->room_id == 0x11B) {
        EstSet(0, -1, pos, NULL, 1, 0x2F, 0, 0, 0, NULL);
    } else {
        EstSet(0, -1, pos, NULL, 0, 0x15, 0, 0, 0, NULL);
    }
}

void EspSetWaterHitmark(Vec* pos)
{
    if ((pG->room_id32 & 0xFFFF0000) == 0x03110000 || (pG->room_id32 & 0xFFFF0000) == 0x02240000) {
        return;
    }
    if (pG->room_id == 0x10A || pG->room_id == 0x10B || pG->room_id == 0x11A || pG->room_id == 0x11B) {
        EstSet(0, -1, pos, NULL, 1, 0x20, 0, 0, 0, NULL);
    } else {
        EstSet(0, -1, pos, NULL, 0, 0x14, 0, 0, 0, NULL);
    }
}

// Never called: the eat effect messages by type.
static inline void EspEatEffectMessage(int type)
{
    switch (type) {
    case 0:
        pLog->err(0, 0, "ESP: EAT no set(type=0)");
        break;
    case 1:
        pLog->err(0, 0, "ESP: EAT no set(type=1)");
        break;
    case 2:
        pLog->err(0, 0, "ESP: EAT no set(type=2)");
        break;
    case 3:
        pLog->err(0, 0, "ESP: EAT no set(type=3)");
        break;
    }
}

int EspChkInPuddle(Vec* pos, Vec* nrm)
{
    if (nrm->y > 0.9f) {
        FlrAt* at = FlrAtCheck(0, pos, 1);

        if (at != NULL && at->x45 == 1) {
            return 1;
        }
    }
    return 0;
}

// Hit effect for the eat (effect collision) attribute type.
void EspSetEatEffect(Vec* pos, Vec* nrm, int type, int wep)
{
    AtEffInfo* info = EatMgr.getEffInfo(type);
    Vec rot;
    u32 eff1;
    u32 eff2;
    f32 len;

    if (info != NULL && (info->flags & 1)) {
        rot.x = atan2f(SQRTF(nrm->x * nrm->x + nrm->z * nrm->z), nrm->y);
        rot.y = atan2f(nrm->x, nrm->z);
        rot.z = 0.0f;
    } else {
        len = SQRTF(nrm->x * nrm->x + nrm->z * nrm->z);
        rot.x = -atan2f(nrm->y, len);
        rot.y = atan2f(nrm->x, nrm->z);
        rot.z = 0.0f;
    }
    switch (type) {
    case 0:
        if (EspChkInPuddle(pos, nrm) == 1) {
            EstSet(0, -1, pos, NULL, 0, 0x11, 0, 0, type, (void*) type);
            SndCall(2, 0xC, pos, 0, 0, NULL);
        } else {
            EstSet(0, -1, pos, &rot, 0, 0x1F, 0, 0, type, (void*) type);
            if (pG->flags_6C & 0x4000) {
                EstSet(0, -1, pos, &rot, 0, 0x87, 0, 0, type, (void*) type);
            }
        }
        break;
    case 1:
        EstSet(0, -1, pos, &rot, 0, 0x1F, 0, 0, 0, NULL);
        EstSet(0, -1, pos, &rot, 0, 0x20, 0, 0, 0, NULL);
        break;
    case 2:
        if (info == NULL || eff1 == 0xD2) {
            pLog->err(0, 0, "NOT REGIST EAT EFF INFO %d", type);
            break;
        }
        info->getWepEff(wep, &eff1, &eff2);
        if (eff1 != 0xD2 && eff2 != 1) {
            EstSet(0, -1, pos, &rot, eff1, (u8) eff2, 0, 0, 0, NULL);
        }
        SndCall(2, 0xB, pos, 0, 0, NULL);
        break;
    case 3:
        pLog->err(0, 0, "ESP: EAT no set(type=EAT_ET_PAD)");
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        if (info == NULL || eff1 == 0xD2) {
            pLog->err(0, 0, "NOT REGIST EAT EFF INFO %d", type);
            break;
        }
        info->getWepEff(wep, &eff1, &eff2);
        if (eff1 != 0xD2 && eff2 != 1) {
            EstSet(0, -1, pos, &rot, eff1, (u8) eff2, 0, 0, 0, NULL);
        }
        if (info != NULL && (info->flags & 1)) {
            SndCall(2, 0xB, pos, 0, 0, NULL);
        }
        break;
    }
}

void EventCutEstSet(int owner, u32 no)
{
    u8 id = (no / 10) * 16 + no % 10;

    if (EspGetEstAddr(owner, id, 1) != NULL) {
        EstSet(0, -1, NULL, NULL, owner, id, 0x1001, 0, 0, NULL);
    }
}

void EventCutEffDelete()
{
    EffectEspDelete(0x3001, 0, 0, NULL);
    EffectEspgenDelete(0x3001, 0, 0);
    EffectEfmDelete(0x3001, 0, 0);
}

void EventAllEffDelete()
{
    EventCutEffDelete();
    EffectEspDelete(0x2001, 0, 0, NULL);
    EffectEspgenDelete(0x2001, 0, 0);
    EffectEfmDelete(0x2001, 0, 0);
}

int ChkWaterEffectEnable(Vec* pos)
{
    if (pG->flags_5010 & 0x400) {
        if (EffAreaCheckInRoom(pos) == 0) {
            return 1;
        }
    }
    return 0;
}

void EstSetEm10WaterFall(Vec* pos)
{
    EspSeqData* head = EspGetEstAddr(1, 0x32, 1);

    if (head != NULL) {
        EstSet((int) pos, -1, NULL, NULL, 1, 0x32, 0, 0, (u32) pos, NULL);
    } else {
        EstSet((int) pos, -1, NULL, NULL, 0x10, 0x8D, 0, 0, (u32) pos, NULL);
    }
}

asm(".section .rodata; .balign 8");
