#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_wrap.h"
#include "player.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "rnd.h"

// Room 2-16 (D:/Bio4/Prog/r216.cpp): the armor knight room; three rotating poles with the armors
// on them and the exit door, driven by member-pointer state tables (no virtuals).

// One rotating pole (three models turning together) carrying an armor.
class cR216Pole {
public:
    u8 mode;         // 0x00  state: 0 wait, 1 open, 2 close (index into r216_pole_tbl)
    u8 step;         // 0x01
    u8 no;           // 0x02
    cModel* obj[3];  // 0x04
    cEmWrap* em;     // 0x10  the armor on the pole (turned with it), NULL once in place
    u32 x14;
    u32 x18;
    u32 x1C;
    u32 x20;
    u32 se;          // 0x24  RoomSeCall handle
    int enable;      // 0x28
    u32 x2C;
    int status;      // 0x30  0 closed, 1 opened, 2 moving

    void init(int no);
    void move();
    void wait();
    void open();
    void close();
    void setOpen();
    void setClose();
    void setOpened();
    void setClosed();
    int getStatus();
    void setEm(cEmWrap* em);
};

// The exit door: rises to open, drops and shakes to close.
class cR216Door {
public:
    u8 mode;         // 0x00  0 wait, 1 open, 2 close (index into r216_door_tbl)
    u8 step;         // 0x01
    cModel* obj;     // 0x04
    Vec basePos;     // 0x08
    f32 height;      // 0x14
    f32 spd;         // 0x18
    u32 se;          // 0x1C
    int enable;      // 0x20
    int status;      // 0x24  0 closed, 1 opened, 2 moving
    int shakeCnt;    // 0x28

    void init();
    void move();
    void wait();
    void open();
    void close();
    void setOpen();
    void setClose();
    int getStatus();
    void setOpened();
    void setClosed();
};

struct R216Em {
    cEmWrap em;
    int set;         // 0xC  1 once the armor was set moving
};

struct R216Work {
    R216Em em[9];        // 0x000  the armors (r216_em_idx order)
    R216Em armor[4];     // 0x090  the display armors (0xE5..0xE8)
    cR216Door door;      // 0x0D0
    cR216Pole pole[3];   // 0x0FC
    ScePrim* prim;       // 0x198  r216_ArmorAppearCamera task
};

// One-member struct: every store through the work reloads the pointer.
struct R216WorkPtr {
    R216Work* p;
};

static R216WorkPtr r216_work;

// COMPILER-DIFF: 4 (the int table values are passed to the s16 parameter untruncated).
int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");

static int r216_pole_obj[3][3] = {{0x2F, 0x5B, 0x66}, {0x30, 0x5C, 0x65}, {0x31, 0x5A, 0x67}};
static int r216_em_no[3][3] = {{0xB9, 0xB4, 0xB7}, {0xB8, 0xB2, 0xB6}, {0xBA, 0xB3, 0xB5}};
static int r216_em_idx[3][3] = {{7, 2, 5}, {6, 0, 4}, {8, 1, 3}};
static f32 r216_pole_ang[3] = {3.1415927f, 0.0f, 1.5707964f};
static void (cR216Pole::*r216_pole_tbl[3])() = {&cR216Pole::wait, &cR216Pole::open, &cR216Pole::close};
static void (cR216Door::*r216_door_tbl[3])() = {&cR216Door::wait, &cR216Door::open, &cR216Door::close};

static void r216_BattleStart();
static void r216_BattleStartEndProc();
static void r216_ArmorAppearCamera();
static void r216_2ndArmorAppear();
static void r216_BattleEndCheck();
static void r216_BattleEnd();
static void r216_BattleEndEndProc();
static void r216_ArmorDispOnOff(int on);

void R216Init()
{
    u32 i;

#line 88 "D:/Bio4/Prog/r216.cpp"
    r216_work.p = (R216Work*) MEM_CALLOC(sizeof(R216Work), 1, 0xd);
    r216_work.p->door.init();
    r216_work.p->pole[0].init(0);
    r216_work.p->pole[1].init(1);
    r216_work.p->pole[2].init(2);
    r216_work.p->door.setOpened();
    if (RsfCheck(G_ROOM_ID, 0)) {
        r216_work.p->pole[0].setOpened();
        r216_work.p->pole[1].setOpened();
        r216_work.p->pole[2].setOpened();
    } else {
        SceAtDataSet_exec(0x80, 0x12, 0, (TaskFunc) r216_BattleStart, 0, 1);
        r216_work.p->armor[0].em.setEm(0xE5, -1, 1, 1, 1);
        r216_work.p->armor[1].em.setEm(0xE6, -1, 1, 1, 1);
        r216_work.p->armor[2].em.setEm(0xE7, -1, 1, 1, 1);
        r216_work.p->armor[3].em.setEm(0xE8, -1, 1, 1, 1);
        SceAtDataSet_exec(5, 0x12, 0, (TaskFunc) r216_ArmorDispOnOff, (void*) 1, 1);
        SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) r216_ArmorDispOnOff, 0, 1);
    }
}

void R216Main()
{
    r216_work.p->door.move();
    r216_work.p->pole[0].move();
    r216_work.p->pole[1].move();
    r216_work.p->pole[2].move();
}

// The knights come alive: the door closes, the poles turn the first armors out.
static void r216_BattleStart()
{
    u32 i;
    R216Em* e;   // shared by both loops: a multi-block pseudo, so the work load is not tied into it

    SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    if (SceMesGetSelection() == 2) {
        SceExit();
    }
    SceAtDataReset(0x80);
    SceAtExecute(0x80);
    while (SceAtItemFlgCk(0x80) == 0) {
        SceSleep(1);
    }
    SceEventStart(0);
    for (i = 0; i < 3; i++) {
        e = &r216_work.p->em[r216_em_idx[i][0]];
        cEmWrapSetEmI(&e->em, r216_em_no[i][0], -1, 1, 1, 1);
        r216_work.p->pole[i].setEm(&e->em);
        e->em.setNoSuspend(1);
    }
    cModel* mdl = NULL;   // the zero of the EstSet stack arguments (r30, set before CutCall)
    CamCtrl.CutCall(1);
    pG->flags_174 &= ~0x20000000;
    SceSetEventCancel(1, (TaskFunc) r216_BattleStartEndProc, 0, 2, 1);
    EstSet(0, -1, NULL, NULL, 1, 6, 1, 5, 0, mdl);
    r216_work.p->door.setClose();
    while (r216_work.p->door.getStatus() != 0) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    pG->flags_174 &= ~0x08000000;
    r216_work.p->prim = SceExec(0x12, (TaskFunc) r216_ArmorAppearCamera, 0, 0, 2, NULL);
    r216_work.p->pole[0].setOpen();
    r216_work.p->pole[1].setOpen();
    r216_work.p->pole[2].setOpen();
    while (r216_work.p->pole[0].getStatus() != 1) {
        SceSleep(1);
    }
    SceSleep(0x5A);
    for (i = 0; i < 3; i++) {
        e = &r216_work.p->em[r216_em_idx[i][0]];
        e->em.setFlag(1);
        e->set = 1;
    }
    while (!(pG->flags_174 & 0x08000000)) {
        SceSleep(1);
    }
    SceSetEventCancel(0, NULL, 0, -1, 1);
    r216_BattleStartEndProc();
}

static void r216_BattleStartEndProc()
{
    u32 i;

    if (pG->flags_174 & 0x20000000) {
        if (r216_work.p->prim) {
            SceKill(r216_work.p->prim);
        }
        for (i = 0; i < 3; i++) {
            R216Em* e;

            r216_work.p->pole[i].setOpened();
            e = &r216_work.p->em[r216_em_idx[i][0]];
            e->em.setFlag(1);
            e->set = 1;
        }
        r216_work.p->door.setClosed();
        EffectEspDelete(1, 5, 0, NULL);
        EffectEspgenDelete(1, 5, 0);
        EffectEfmDelete(1, 5, 0);
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceExec(0x12, (TaskFunc) r216_BattleEndCheck, 0, 0, 2, NULL);
    r216_work.p->em[6].em.setNoSuspend(0);
    r216_work.p->em[7].em.setNoSuspend(0);
    r216_work.p->em[8].em.setNoSuspend(0);
    SceEventEnd(0);
}

// Five camera cuts while the armors appear.
static void r216_ArmorAppearCamera()
{
    int cut[5] = {3, 4, 5, 10, 6};
    int i;

    for (i = 0; i < 5; i++) {
        CamCtrl.CutCall((s8) cut[i]);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    pG->flags_174 |= 0x08000000;
}

// The second set of armors: the poles turn back, take the next armors and turn out again.
static void r216_2ndArmorAppear()
{
    u32 i;

    SceCTask()->task->flag &= ~2;
    r216_work.p->pole[0].setClose();
    r216_work.p->pole[1].setClose();
    r216_work.p->pole[2].setClose();
    while (r216_work.p->pole[0].getStatus() != 0) {
        SceSleep(1);
    }
    SceSleep(0x3C);
    for (i = 0; i < 3; i++) {
        cEmWrapSetEmI(&r216_work.p->em[r216_em_idx[i][1]].em, r216_em_no[i][1], -1, 1, 1, 1);
        r216_work.p->pole[i].setEm(&r216_work.p->em[r216_em_idx[i][1]].em);
    }
    r216_work.p->pole[0].setOpen();
    r216_work.p->pole[1].setOpen();
    r216_work.p->pole[2].setOpen();
    while (r216_work.p->pole[0].getStatus() != 1) {
        SceSleep(1);
    }
    SceSleep(0xF);
    for (i = 0; i < 3; i++) {
        R216Em* e = &r216_work.p->em[r216_em_idx[i][1]];

        e->em.setFlag(1);
        SceSleep(1);
        e->set = 1;
    }
}

// Counts the beaten armors: 3 -> the second set, 6 -> the battle ends.
static void r216_BattleEndCheck()
{
    for (;;) {
        u8 dead = 0;
        u32 i;

        for (i = 0; i < 9; i++) {
            R216Em* e = &r216_work.p->em[i];

            if (e->em.isAlive() == 1 && e->set == 1 && e->em.isActive() == 0) {
                dead++;
            }
        }
        if (dead == 6) {
            SceExec(0x12, (TaskFunc) r216_BattleEnd, 0, 0, 2, NULL);
            SceExit();
        } else if (dead == 3) {
            if (!(pG->flags_174 & 0x10000000)) {
                pG->flags_174 |= 0x10000000;
                SceExec(0x12, (TaskFunc) r216_2ndArmorAppear, 0, 0, 2, NULL);
            }
        }
        SceSleep(1);
    }
}

static void r216_BattleEnd()
{
    SceSleep(0x1E);
    SceEventStart(0);
    CamCtrl.CutCall(1);
    pG->flags_174 &= ~0x20000000;
    SceSetEventCancel(1, (TaskFunc) r216_BattleEndEndProc, 0, -1, 1);
    r216_work.p->door.setOpen();
    while (r216_work.p->door.getStatus() != 1) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, NULL, 0, -1, 1);
    r216_BattleEndEndProc();
}

static void r216_BattleEndEndProc()
{
    if (pG->flags_174 & 0x20000000) {
        r216_work.p->door.setOpened();
    }
    CamCtrl.Comeback(0);
    RsfSet(G_ROOM_ID, 0);
    SceEventEnd(0);
}

static void r216_ArmorDispOnOff(int on)
{
    r216_work.p->armor[0].em.setTrans(on);
    r216_work.p->armor[1].em.setTrans(on);
    r216_work.p->armor[2].em.setTrans(on);
    r216_work.p->armor[3].em.setTrans(on);
}

void cR216Pole::init(int no)
{
    u32 i;

    this->no = no;
    enable = 1;
    for (i = 0; i < 3; i++) {
        obj[i] = SmdGetObjPtr(r216_pole_obj[this->no][i]);
        obj[i]->be_flag |= 0x20;
    }
}

void cR216Pole::move()
{
    if (enable) {
        (this->*r216_pole_tbl[mode])();
    }
}

void cR216Pole::wait()
{
}

// Turns the pole out (to angle 0), the armor with it.
void cR216Pole::open()
{
    f32 spd = 0.03f;

    switch (step) {
    case 0:
        se = RoomSeCall(no * 2 + 1, &obj[0]->pos, 0, 0, obj[0]);
        step++;
    case 1:
        obj[0]->rot.y += spd;
        obj[1]->rot.y += spd;
        obj[2]->rot.y += spd;
        if (em) {
            Vec ang;

            ang.y = em->getAngY() + spd;
            ang.x = 0.0f;
            ang.z = 0.0f;
            em->setAng(&ang);
        }
        if (obj[0]->rot.y > 0.0f) {
            obj[0]->rot.y = 0.0f;
            obj[1]->rot.y = 0.0f;
            obj[2]->rot.y = 0.0f;
            se = RoomSeCall(no * 2 + 2, &obj[0]->pos, 0, 0, obj[0]);
            status = 1;
            mode = 0;
            step = 0;
            em = NULL;
        }
        break;
    }
}

// Turns the pole on (to angle PI == -PI).
void cR216Pole::close()
{
    f32 spd = 0.03f;
    Vec ang;

    switch (step) {
    case 0:
        se = RoomSeCall(no * 2 + 1, &obj[0]->pos, 0, 0, obj[0]);
        step++;
    case 1:
        obj[0]->rot.y += spd;
        obj[1]->rot.y += spd;
        obj[2]->rot.y += spd;
        if (em) {
            ang.y = em->getAngY() + spd;
            cEmWrap* e = em;
            ang.x = 0.0f;
            ang.z = 0.0f;
            e->setAng(&ang);
        }
        if (obj[0]->rot.y > 3.1415927f) {
            obj[0]->rot.y = -3.1415927f;
            obj[1]->rot.y = -3.1415927f;
            obj[2]->rot.y = -3.1415927f;
            se = RoomSeCall(no * 2 + 2, &obj[0]->pos, 0, 0, obj[0]);
            step = mode = status = 0;
            em = NULL;
        }
        break;
    }
}

void cR216Pole::setOpen()
{
    if (!enable) {
        return;
    }
    if (status == 1) {
        return;
    }
    if (mode == 1) {
        return;
    }
    status = 2;
    mode = 1;
    step = 0;
}

void cR216Pole::setClose()
{
    if (!enable) {
        return;
    }
    if (status == 0) {
        return;
    }
    if (mode == 2) {
        return;
    }
    mode = status = 2;
    step = 0;
}

// Snaps the pole to the open angle (room re-entry, event cancel).
void cR216Pole::setOpened()
{
    if (enable) {
        mode = 0;
        step = 0;
        status = 1;
        obj[0]->rot.y = 0.0f;
        obj[1]->rot.y = 0.0f;
        obj[2]->rot.y = 0.0f;
        if (em) {
            Vec ang;

            ang.x = 0.0f;
            ang.y = r216_pole_ang[no] + 3.1415927f;
            ang.z = 0.0f;
            em->setAng(&ang);
        }
        SndStop(se, 0);
    }
}

// Never called: the original linker dropped the body and kept its pool (-PI, 0.0) after setOpened's
// (STRIP_UNUSED); written after cR216Door::setClosed.
void cR216Pole::setClosed()
{
    if (enable) {
        mode = 0;
        step = 0;
        status = 0;
        obj[0]->rot.y = -3.1415927f;
        obj[1]->rot.y = -3.1415927f;
        obj[2]->rot.y = -3.1415927f;
        if (em) {
            Vec ang;

            ang.x = 0.0f;
            ang.y = r216_pole_ang[no];
            ang.z = 0.0f;
            em->setAng(&ang);
        }
        SndStop(se, 0);
    }
}

int cR216Pole::getStatus()
{
    if (enable) {
        return status;
    }
    return -1;
}

// Puts an armor on the pole (at its position, facing the pole's base angle).
// Puts an armor on the pole (at its position, facing the pole's base angle). One Vec serves both
// calls (pos and ang share the frame slot) and the table angle is read before setPos.
void cR216Pole::setEm(cEmWrap* em)
{
    Vec v;
    f32 px, pz, y;

    this->em = em;
    px = obj[0]->pos.x;
    pz = obj[0]->pos.z;
    y = r216_pole_ang[no];
    v.x = px;
    v.y = 91.5f;
    v.z = pz;
    em->setPos(&v);
    v.x = 0.0f;
    v.y = y;
    v.z = 0.0f;
    em->setAng(&v);
}

void cR216Door::init()
{
    enable = 0;
    obj = SmdGetObjPtr(0xA);
    if (obj) {
        obj->be_flag |= 0x20;
        basePos = obj->pos;
        height = 2400.0f;
        mode = 0;
        enable = 1;
    }
}

void cR216Door::move()
{
    if (enable) {
        (this->*r216_door_tbl[mode])();
    }
}

void cR216Door::wait()
{
}

void cR216Door::open()
{
    switch (step) {
    case 0:
        se = RoomSeCall(0x24, &obj->pos, 0, 0, obj);
        SmdGetObjPtr(0xB)->be_flag &= ~2;
        step++;
    case 1:
        obj->pos.y += 22.0f;
        if (obj->pos.y > basePos.y + height) {
            obj->pos.y = basePos.y + height;
            SceAtSetEnable(1, 0);
            se = RoomSeCall(0x25, &obj->pos, 0, 0, obj);
            status = 1;
            mode = 0;
        }
        break;
    }
}

void cR216Door::close()
{
    switch (step) {
    case 0:
        se = RoomSeCall(0x26, &obj->pos, 0, 0, obj);
        SceAtSetEnable(1, 1);
        spd = -20.0f;
        step++;
    case 1:
        obj->pos.y += spd;
        spd -= 5.0f;
        if (obj->pos.y < basePos.y) {
            obj->pos.y = basePos.y;
            shakeCnt = 5;
            step++;
            se = RoomSeCall(0x27, &obj->pos, 0, 0, obj);
            SmdGetObjPtr(0xB)->be_flag |= 2;
        }
        break;
    case 2:
        obj->pos.x = fRand1_1() * 20.0f + basePos.x;
        obj->pos.y = fRand0_1() * 20.0f + basePos.y;
        obj->pos.z = fRand1_1() * 20.0f + basePos.z;
        if (shakeCnt) {
            shakeCnt--;
        } else {
            obj->pos = basePos;
            status = 0;
            mode = 0;
        }
        break;
    }
}

void cR216Door::setOpen()
{
    if (!enable) {
        return;
    }
    if (status == 1) {
        return;
    }
    if (mode == 1) {
        return;
    }
    status = 2;
    mode = 1;
    step = 0;
}

void cR216Door::setClose()
{
    if (!enable) {
        return;
    }
    if (status == 0) {
        return;
    }
    if (mode == 2) {
        return;
    }
    mode = status = 2;
    step = 0;
}

int cR216Door::getStatus()
{
    if (enable) {
        return status;
    }
    return -1;
}

void cR216Door::setOpened()
{
    if (enable) {
        status = 1;
        obj->pos.y = basePos.y + height;
        SceAtSetEnable(1, 0);
        SmdGetObjPtr(0xB)->be_flag &= ~2;
        mode = 0;
        step = 0;
        SndStop(se, 0);
    }
}

void cR216Door::setClosed()
{
    if (enable) {
        status = 0;
        obj->pos.y = basePos.y;
        SceAtSetEnable(1, 1);
        SmdGetObjPtr(0xB)->be_flag |= 2;
        mode = 0;
        step = 0;
        SndStop(se, 0);
    }
}
