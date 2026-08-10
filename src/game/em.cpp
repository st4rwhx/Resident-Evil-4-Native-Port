// game/em.cpp: the character work base (cEm) and its manager (cEmMgr): construction of the
// player / enemy / object classes by id, the per-frame emMove loop, the damage info (cDmgInfo).

#include "atari.h"
#include "ctrl.h"
#include "em.h"
#include "player.h"
#include "emobj.h"
#include "emdoor.h"
#include "emwep.h"
#include "embox.h"
#include "emwindow.h"
#include "emtorch.h"
#include "embarrel.h"
#include "emtree.h"
#include "emrock.h"
#include "emswitch.h"
#include "emitem.h"
#include "emhit.h"
#include "emBarred.h"
#include "emmine.h"
#include "emshield.h"
#include "emBar.h"
#include "snd.h"
#include "global.h"
#include "db_log.h"
#include "va_ppc.h"

extern "C" {
void RouteCk();                                     // route_ck.cpp
void* EmReadSearch(u8 id, int a, int b);            // read.cpp: the enemy's read table entry, 0 when not loaded
void ShapeMove(cModelInfo* info);                   // shape.cpp
void EmYarareDisp(cEm* em);                         // em_sub.cpp
void DrawOba(cModel* m);                            // at_mod.cpp
}

// cManager<T>::arrayFree / arrayAlloc: definitions in cManager.h (game.cpp instantiates them too).

const char* cEmMgr::idName[96] = {
    "PLAYER", "", "", "ASHLEY", "LUIS", "", "", "", "", "", "", "", "", "", "JET SKI", "MOTOR BOAT",
    "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO",
    "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO",
    "SPIDER", "DOG", "DOG", "CROW", "SNAKE S", "PARASITE", "COW", "BLACKBASS",
    "CHICKEN", "BAT", "TRAP", "ELGIGANTE", "INSECT BOSS", "INSECT HUMAN", "SPIDER S", "SALAMANDER",
    "SADDLER", "", "U3", "INSECTBOSS EVENT", "MAYOR", "MAYOR AFTER", "REGENERATER", "NO2",
    "NO2 AFTER", "NO3", "NO3 AFTER", "TRUCK", "ARMOR", "HELICOPTER", "", "",
    "OBJ", "DOOR", "WEP", "BOX", "WALL", "RACK", "WINDOW", "TORCH",
    "BARREL", "TREE", "ROCK", "SWITCH", "ITEM", "HIT", "BARRED", "MINE",
    "SHIELD",
};

cPlayer* pPL;
cEm* pSUB;
void (*EmInitFunc)(cEm* em);
void (*PlInitFunc)(cEm* em);

static u32 battleCheckFlag;

cEmMgr::cEmMgr() : cManager<cEm>(sizeof(cEm), 2)
{
    setName("cEmMgr");
    x34 = 0;
}

void cEmMgr::log(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    pLog->vwarn(6, 0, fmt, ap);
}

int cEmMgr::construct(cEm* p, u32 id)
{
    switch (id) {
    case 0:
        switch (pG->x4FB8) {
        case 0:
            p = new (p) cPlLeon;
            break;
        case 1:
            p = new (p) cPlAshley;
            break;
        case 2:
        case 3:
        case 4:
        case 5:
            PlInitFunc(p);
            break;
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case 0xE:
        p->x378 = (u32) EmReadSearch(id, 0, 0);
        if (p->x378 == 0) {
            return 0;
        }
        EmInitFunc(p);
        break;
    case 0x40:
        p = new (p) cEmObj;
        break;
    case 0x41:
        p = new (p) cEmDoor;
        break;
    case 0x42:
        p = new (p) cEmWep;
        break;
    case 0x43:
        p = new (p) cEmBox;
        break;
    case 0x45:
        p = new (p) cEmRack;
        break;
    case 0x46:
        p = new (p) cEmWindow;
        break;
    case 0x47:
        p = new (p) cEmTorch;
        break;
    case 0x48:
        p = new (p) cEmBarrel;
        break;
    case 0x49:
        p = new (p) cEmTree;
        break;
    case 0x4A:
        p = new (p) cEmRock;
        break;
    case 0x4B:
        p = new (p) cEmSwitch;
        break;
    case 0x4C:
        p = new (p) cEmItem;
        break;
    case 0x4D:
        p = new (p) cEmHit;
        break;
    case 0x4E:
        p = new (p) cEmBarred;
        break;
    case 0x4F:
        p = new (p) cEmMine;
        break;
    case 0x50:
        p = new (p) cEmShield;
        break;
    case 0x51:
        p = new (p) cEmBar;
        break;
    case 0xFF:
        p = new (p) cEm;
        break;
    default:
        p->x378 = (u32) EmReadSearch(id, 0, 0);
        if (p->x378 == 0) {
            return 0;
        }
        EmInitFunc(p);
        break;
    }
    switch (p->id) {
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x20:
        p->id = 0x10;
        break;
    }
    p->serial = x34;
    x34++;
    p->emsetNo = 0xFF;
    p->be_flag |= 0x40;
    p->id = id;
    p->be_flag |= 0x02000000;
    p->x37C = p->x378;
    return 1;
}

int cEmMgr::arrayAlloc(u32 n)
{
    cManager<cEm>::arrayAlloc(n);
    pPL = 0;
    pSUB = 0;
    return 1;
}

void cEmMgr::move()
{
    cEm* p;
    void (*func)(cEm*);

    dieCheck();
    RouteCk();
    if (!(pG->flags_170 & 0x20000000)) {
        p = pAlive;
        func = emMove;
        while (p) {
            cEm* cur = p;

            p = (cEm*) p->next;
            func(cur);
        }
    } else if (pSUB && !(pG->flags_170 & 0x1000)) {
        emMove(pSUB);
    }
}

void cEmMgr::destroy(cEm* p)
{
    if ((u32) p < 0x80000000 || (u32) p > 0x82FFFFFF || (p->be_flag & 0x201) != 1) {
        pLog->err(0, 0, "cEmMgr::destroy() WORK IS ALREADY DEAD. %08X", p);
        return;
    }
    p->push();
    cManager<cEm>::destroy(p);
}

int cEmMgr::isBattle()
{
    cEm* p;
    void (*func)(cEm*);

    // reference store: keeps the pAlive load below it (global.h BitSet)
    BitSet(battleCheckFlag, 0);
    func = battleCheck;
    p = pAlive;
    while (p) {
        cEm* cur = p;

        p = (cEm*) p->next;
        func(cur);
    }
    return battleCheckFlag;
}

void battleCheck(cEm* em)
{
    if (em->checkStatus(0)) {
        battleCheckFlag = 1;
    }
}

void killEm(cEm* em)
{
    if (em->id != 0) {
        EmMgr.destroy(em);
    }
}

void cEmMgr::destroyAll()
{
    cEm* p;
    void (*func)(cEm*);

    p = pAlive;
    func = killEm;
    while (p) {
        cEm* cur = p;

        p = (cEm*) p->next;
        func(cur);
    }
}

cEm* cEmMgr::getEmPtr(int id, cEm* start)
{
    cEm* p;

    p = start;
    if (p) {
        p = (cEm*) p->next;
    } else {
        p = pAlive;
    }
    while (p) {
        if (p->id == id) {
            return p;
        }
        p = (cEm*) p->next;
    }
    return 0;
}

cEm::cEm()
{
    new (&dmg) cDmgInfo;
    initWork();
}

void cEm::setStatus(int bit)
{
    status |= 1 << bit;
}

void cEm::clearStatus(int bit)
{
    status &= ~(1 << bit);
}

int cEm::checkStatus(int bit)
{
    if (status & (1 << bit)) {
        return 1;
    }
    return 0;
}

int cEm::checkThrow()
{
    return 0;
}

void cEm::setItem(u16 a, u16 b, u16 c, u16 d, u8 e)
{
    itemNo = a;
    itemNum = b;
    item3DA = c;
    item3DC = d;
    itemFlag = e;
}

void cEm::setNoItem()
{
    itemNo = 0xFFFF;
    itemNum = 0;
    item3DA = 0;
    item3DC = 0;
    itemFlag = 0;
}

void emMove(cEm* em)
{
    f32 dx;
    f32 dz;

    if ((em->be_flag & 0x201) != 1) {
        pLog->err(2, 0, "emMove() DEAD WORK CALLED %08X(ID:%02X)", em, em->id);
        EmMgr.destroy(em);
        return;
    }
    if ((pG->flags_5010 & 0x10000000) && !(em->be_flag & 0x800)) {
        return;
    }
    if (em == pPL) {
        return;
    }
    if (em == pSUB && (pG->flags_170 & 0x1000)) {
        return;
    }
    dz = pPL->pos.z - em->pos.z;
    dx = pPL->pos.x - em->pos.x;
    em->plDist2 = dx * dx + dz * dz;
    em->x374 = 1e16f;
    em->dmg.move();
    em->move();
    if ((em->be_flag & 0x201) != 1) {
        return;
    }
    em->be_flag &= ~0x20000000;
    ShapeMove(em->pInfo);
    if (em->seNo) {
        int no = em->seNo - 1;
        cModel* parts = em->getPartsPtr(0);

        SndCall(8, no, &parts->worldPos, em->id, 0, em);
        em->seNo = 0;
    }
    em->updateOldPos();
    EmYarareDisp(em);
    if (pG->flags_68 & 0x10000000) {
        DrawOba(em);
    }
    if (em->be_flag & 0x80000000) {
        em->drawAllBoundingBox(em->pInfo);
    }
    em->x158 = 1.0f;
}

void cEm::move()
{
}

int cEm::initWork()
{
    be_flag = 0x21;
    x12E = 0;
    return 1;
}

cDmgInfo::cDmgInfo()
{
    clear();
}

void cDmgInfo::set(int a, int b, u8 kind, Vec* p, f32 r, EmHitInfo* prt)
{
    stat = a | 1;
    x1 = b;
    this->kind = kind;
    pos = *p;
    rad = r;
    part = prt;
}

void cDmgInfo::set(int a, int b)
{
    stat = a;
    x1 = b;
}

void cDmgInfo::clear()
{
    stat = 0;
    x1 = 0;
}

void cDmgInfo::move()
{
    if (x1 & 0x80) {
        return;
    }
    if ((x1 & 0x7F) == 0) {
        return;
    }
    x1--;
    if (x1 == 0) {
        stat = 0;
    }
}

cEmMgr EmMgr;
