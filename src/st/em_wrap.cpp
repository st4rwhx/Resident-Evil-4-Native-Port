#include "types.h"
#include "em_wrap.h"
#include "em_set.h"
#include "global.h"
#include "db_log.h"
#include "sce_sys.h"
#include "player.h"
#include "math_sub.h"

extern "C" void* memset(void* dst, int c, unsigned int n);

// int store through a reference (keeps the following loads below it, like global.h BitOn)
static inline void IntSet(int& d, int v) { d = v; }

// Typed view of pG->emlist (the r400 idiom): the original indexes an EmListData array, so the
// element address is `pG + no * 32` (pG first in the add, the table offset in the displacement);
// EM_LIST's `&pG->emlist[no * 0x20]` is a pointer sum whose MULT term expand puts first.
struct EmListView {
    u8 pad[0x52E8];
    EmListData emlist[0x100];
};

// Room-script enemy handle (include/em_wrap.h): the first object of every stage REL (st1_0..st4_0). No
// __FILE__ string: the file name is not in the binary. The original REL link dead-stripped the members no
// room of the module calls (config/G4BE08/modules.py STRIP_UNUSED), leaving their strings and constant
// pools, so every member is written even where no module kept its body. Function order = .text and
// .rodata (string) order. st2_4/st4_0 were built without the cEmControl/cEmPatrol/cEmGuard part
// (src/st/em_wrap_v2.cpp defines EM_WRAP_NO_CONTROL and includes this file).

#ifndef EM_WRAP_NO_CONTROL
int cEmControl::SetControl(s16 no, Vec* tbl, int n, int errOn)
{
    if (em.setPtr(no, -1, errOn) == 0) {
        if (errOn == 1) {
            pLog->err(0, 0, "cEmControl::SetPatrol");
        }
        return 0;
    }
    active = 1;
    SetTargetPos(tbl, n);
    return 1;
}

void cEmControl::SetTargetPos(Vec* tbl, int n)
{
    int i;

    prev = 0;
    cur = 0;
    memset(&point[0], 0, sizeof(EmControlPoint));
    nPoint = n;
    if (n > 15) {
        nPoint = 15;
    }
    for (i = 0; i < nPoint; i++) {
        point[i].pos = tbl[i];
    }
}

void cEmControl::EndControl()
{
    active = 0;
    if (em.isActive() != 0) {
        cEm* p;

        em.setGoto(&pPL->pos, 0);
        em.setCharacter(0);
        p = em.getPtr();
        if (p != 0) {
            p->xFC = 1;
            p->xFD = 0;
            p->xFE = 0;
            p->xFF = 0;
        }
    }
}

int cEmPatrol::SetPatrol(s16 no, Vec* tbl, int n, u8 prio, int errOn)
{
    if (SetControl(no, tbl, n, errOn) == 0) {
        return 0;
    }
    SceExec(0x12, (TaskFunc) TaskMove, (int) this, prio, 2, 0);
    return 1;
}

void cEmPatrol::TaskMove(cEmPatrol* p)
{
    cEmWrap* em = &p->em;

    while (p->active != 0 && em->ckFindPL() != 1 && em->isActive() != 0) {
        if (em->ckGoto() != 6) {
            int cur;
            int next;
            int wrap;

            em->setGoto(&p->point[p->cur].pos, 6);
            cur = p->cur;
            next = cur + 1;
            p->prev = cur;
            p->cur = next;
            if (next < 0) {
                wrap = p->nPoint - 1;
            } else {
                wrap = (next > p->nPoint - 1) ? 0 : next;
            }
            p->cur = wrap;
        }
        SceSleep(1);
    }
}

int cEmGuard::SetGuard(s16 no, Vec* tbl, int n, int (*check)(cEmWrap*), f32 ang, u8 prio, int errOn)
{
    if (SetControl(no, tbl, n, errOn) == 0) {
        return 0;
    }
    SceExec(0x12, (TaskFunc) TaskMove, (int) this, prio, 2, 0);
    this->ang = ang;
    guard_r = em.getGuard_r();
    this->check = check;
    x130 = 0;
    return 1;
}

void cEmGuard::TaskMove(cEmGuard* g)
{
    Vec v;   // frame offset 0: its address is recomputed per call
    cEmWrap* em = &g->em;

    em->setGoto(&pPL->pos, 0xB);
    while (g->active != 0 && em->isActive() != 0) {
        if (g->check != 0 && g->check(em) != 0) {
            if (g->alerted != 0) {
                g->alerted = 0;
                g->step = 0;
            }
            switch (g->step) {
            case 0:
                em->setGoto(&g->point[g->cur].pos, 1);
                g->step++;
                break;
            case 1:
                if (em->ckGoto() != 1) {
                    Vec rot;
                    Mtx m;

                    rot.x = 0.0f;
                    rot.y = g->ang;
                    rot.z = 0.0f;
                    RotMatrix(m, &rot);
                    v.x = 0.0f;
                    v.y = 0.0f;
                    v.z = 1000.0f;
                    PSMTXMultVec(m, &v, &v);
                    PSVECAdd(&g->point[g->cur].pos, &v, &v);
                    em->setGoto(&v, 1);
                    em->setCharacter(1);
                    em->setGuard_r(100.0f);
                    g->step++;
                }
                break;
            case 2:
                if (em->ckGoto() != 1) {
                    if (em->ckRno01(1, 1) == 0) {
                        g->step = 0;
                    }
                }
                break;
            }
        } else if (g->alerted != 1) {
            IntSet(g->alerted, 1);
            IntSet(g->step, 0);
            em->setGoto(&pPL->pos, 0);
            em->setCharacter(0);
            em->setGuard_r(g->guard_r);
        }
        SceSleep(1);
    }
}
#endif

cEmWrap::cEmWrap()
{
    initWork();
}

void cEmWrap::initWork()
{
    pEm = 0;
    no = 0;
    list = 0;
    alive = 0;
}

void cEmWrap::err(const char* msg, int no)
{
    if (errOn == 1 && !(pG->flags_64 & 0x20000)) {
        pLog->err(0, 0, msg, no);
    }
}

int cEmWrap::setEm(s16 no, s8 list, int errOn, int chkDead, int setAlive)
{
    this->errOn = errOn;
    this->no = no;
    this->list = list;
    if (list >= 0 && pG->emlist_no != list) {
        pEm = 0;
        err("EM_SET_NO(%d) cEmWrap::setEm list No. difference", this->no);
        return 0;
    }
    if (((EmListView*) pG)->emlist[no].flags & 2) {
        pEm = GetEmPtrFromList(no);
    } else {
        pEm = EmSetFromList2(no, chkDead == 1);
    }
    if (pEm == 0) {
        err("EM_SET_NO(%d) cEmWrap::setEm failed", this->no);
        return 0;
    }
    alive = 1;
    if (setAlive == 1) {
        EmListSetAlive(no, 1);
    }
    return 1;
}

cEm* setEm(s16 no, s8 list, int errOn, int chkDead, int setAlive)
{
    cEmWrap em;

    if (em.setEm(no, list, errOn, chkDead, setAlive) != 1) {
        return 0;
    }
    return em.getPtr();
}

int cEmWrap::setPtr(s16 no, s8 list, int errOn)
{
    cEm* p;

    if (list >= 0 && pG->emlist_no != list) {
        pEm = 0;
        err("EM_SET_NO(%d) cEmWrap::setEm list No. difference", no);
        return 0;
    }
    p = GetEmPtrFromList(no);
    if (p != 0) {
        return setPtr(p, errOn);
    }
    return setEm(no, list, errOn, 1, 1);
}

int cEmWrap::setPtr(cEm* em, int errOn)
{
    this->errOn = errOn;
    if (em != 0) {
        int a = (em->be_flag & 0x201) == 1;

        if (a == 1) {
            alive = a;
            no = -1;
            pEm = em;
            list = -1;
            return 1;
        }
    }
    err("EM_SET_NO(%d) cEmWrap::setPtr failed", no);
    return 0;
}

cEm* cEmWrap::getPtr()
{
    if (isAlive() == 1) {
        return pEm;
    }
    err("EM_SET_NO(%d) cEmWrap::getPtr error", no);
    return 0;
}

int cEmWrap::isAlive()
{
    if (alive != 1) {
        return 0;
    }
    if (pEm == 0) {
        return 0;
    }
    return (pEm->be_flag & 0x201) == 1;
}

int cEmWrap::isActive()
{
    if (isAlive() == 1 && pEm->checkStatus(5) == 1) {
        return 1;
    }
    return 0;
}

void cEmWrap::destroy()
{
    if (isAlive() == 1) {
        EmListData* d = GetListPtrFromEm(pEm);

        if (d != 0) {
            d->flags &= ~1;
        }
        EmMgr.destroy(pEm);
    } else {
        err("EM_SET_NO(%d) cEmWrap::destroy error", no);
    }
    initWork();
}

void cEmWrap::setTrans(int on)
{
    if (isAlive() == 1) {
        cEm* p = pEm;

        if (on == 1) {
            p->be_flag |= 2;
        } else {
            p->be_flag &= ~2;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setTrans error", no);
    }
}

int cEmWrap::isTrans()
{
    if (isAlive() == 1) {
        return (pEm->be_flag & 2) != 0;
    }
    err("EM_SET_NO(%d) cEmWrap::isTrans error", no);
    return 0;
}

void cEmWrap::setMove(int on)
{
    if (isAlive() == 1) {
        cEm* p = pEm;

        if (on == 1) {
            p->be_flag |= 4;
        } else {
            p->be_flag &= ~4;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setMove error", no);
    }
}

int cEmWrap::isMove()
{
    if (isAlive() == 1) {
        return (pEm->be_flag & 4) != 0;
    }
    err("EM_SET_NO(%d) cEmWrap::isMove error", no);
    return 0;
}

void cEmWrap::setBeFlag(u32 bit, int on)
{
    if (isAlive() == 1) {
        if (on == 1) {
            pEm->be_flag |= bit;
        } else {
            pEm->be_flag &= ~bit;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setBeFlag error", no);
    }
}

int cEmWrap::isBeFlag(u32 bit)
{
    if (isAlive() == 1) {
        return (pEm->be_flag & bit) != 0;
    }
    err("EM_SET_NO(%d) cEmWrap::isBeFlag error", no);
    return 0;
}

int cEmWrap::isDamage()
{
    if (isAlive() == 1) {
        return pEm->checkStatus(1);
    }
    err("EM_SET_NO(%d) cEmWrap::isDamage error", no);
    return 0;
}

void cEmWrap::setNoSuspend(int on)
{
    if (isAlive() == 1) {
        pEm->setNoSuspend(on);
    } else {
        err("EM_SET_NO(%d) cEmWrap::setNoSuspend error", no);
    }
}

int cEmWrap::isNoSuspend()
{
    if (isAlive() == 1) {
        return (pEm->be_flag & 0x400) != 0;
    }
    err("EM_SET_NO(%d) cEmWrap::isNoSuspend error", no);
    return 0;
}

void cEmWrap::setRno(u8 r0, u8 r1)
{
    if (isAlive() == 1) {
        pEm->xFC = r0;
        pEm->xFD = r1;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setRno error", no);
    }
}

int cEmWrap::ckRno01(int r0, int r1)
{
    if (isAlive() == 1) {
        cEm* p = pEm;

        if (p->xFC == r0 && p->xFD == r1) {
            return 1;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckRno01 error", no);
    }
    return 0;
}

void cEmWrap::setHp(s16 hp)
{
    if (isAlive() == 1) {
        pEm->hp = hp;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setHp error", no);
    }
}

s16 cEmWrap::getHp()
{
    if (isAlive() == 1) {
        return pEm->hp;
    }
    err("EM_SET_NO(%d) cEmWrap::getHp error", no);
    return 0;
}

s16 cEmWrap::getHpMax()
{
    if (isAlive() == 1) {
        return pEm->hpMax;
    }
    err("EM_SET_NO(%d) cEmWrap::getHpMax error", no);
    return 0;
}

u8 cEmWrap::Character()
{
    if (isAlive() == 1) {
        return pEm->x3D0;
    }
    err("EM_SET_NO(%d) cEmWrap::Character error", no);
    return 0;
}

void cEmWrap::setCharacter(u8 c)
{
    if (isAlive() == 1) {
        pEm->x3D0 = c;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setCharacter error", no);
    }
}

f32 cEmWrap::getGuard_r()
{
    if (isAlive() == 1) {
        return pEm->x3CC;
    }
    err("EM_SET_NO(%d) cEmWrap::Guard_r error", no);
    return 0.0f;
}

void cEmWrap::setGuard_r(f32 r)
{
    if (isAlive() == 1) {
        pEm->x3CC = r;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setGuard_r error", no);
    }
}

int cEmWrap::checkStatus(int stat)
{
    if (isAlive() == 1) {
        return pEm->checkStatus(stat) != 0 ? 1 : 0;
    }
    err("EM_SET_NO(%d) cEmWrap::checkStatus error", no);
    return 0;
}

void cEmWrap::setPos(Vec* pos)
{
    if (isAlive() == 1) {
        pEm->setPos(pos);
    } else {
        err("EM_SET_NO(%d) cEmWrap::setPos error", no);
    }
}

void cEmWrap::setAng(Vec* ang)
{
    if (isAlive() == 1) {
        pEm->setAng(ang);
    } else {
        err("EM_SET_NO(%d) cEmWrap::setAng error", no);
    }
}

void cEmWrap::setSca(Vec* sca)
{
    if (isAlive() == 1) {
        pEm->scale = *sca;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setSca error", no);
    }
}

void cEmWrap::setFlag(u32 bit)
{
    if (isAlive() == 1) {
        pEm->flags_3C8 |= bit;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setFlag error", no);
    }
}

int cEmWrap::ckFlag(u32 bit)
{
    if (isAlive() != 1) {
        err("EM_SET_NO(%d) cEmWrap::setFlag error", no);
    } else if (pEm->flags_3C8 & bit) {
        return 1;
    }
    return 0;
}

void cEmWrap::getPos(Vec* pos)
{
    if (isAlive() == 1) {
        *pos = pEm->pos;
    } else {
        err("EM_SET_NO(%d) cEmWrap::getPos error", no);
        pos->x = 0.0f;
        pos->y = 0.0f;
        pos->z = 0.0f;
    }
}

f32 cEmWrap::getPosX()
{
    if (isAlive() == 1) {
        return pEm->pos.x;
    }
    err("EM_SET_NO(%d) cEmWrap::getPosX error", no);
    return 0.0f;
}

f32 cEmWrap::getPosY()
{
    if (isAlive() == 1) {
        return pEm->pos.y;
    }
    err("EM_SET_NO(%d) cEmWrap::getPosY error", no);
    return 0.0f;
}

f32 cEmWrap::getPosZ()
{
    if (isAlive() == 1) {
        return pEm->pos.z;
    }
    err("EM_SET_NO(%d) cEmWrap::getPosZ error", no);
    return 0.0f;
}

void cEmWrap::getAng(Vec* ang)
{
    if (isAlive() == 1) {
        *ang = pEm->rot;
    } else {
        err("EM_SET_NO(%d) cEmWrap::getAng error", no);
        ang->x = 0.0f;
        ang->y = 0.0f;
        ang->z = 0.0f;
    }
}

f32 cEmWrap::getAngX()
{
    if (isAlive() == 1) {
        return pEm->rot.x;
    }
    err("EM_SET_NO(%d) cEmWrap::getAngX error", no);
    return 0.0f;
}

f32 cEmWrap::getAngY()
{
    if (isAlive() == 1) {
        return pEm->rot.y;
    }
    err("EM_SET_NO(%d) cEmWrap::getAngY error", no);
    return 0.0f;
}

f32 cEmWrap::getAngZ()
{
    if (isAlive() == 1) {
        return pEm->rot.z;
    }
    err("EM_SET_NO(%d) cEmWrap::getAngZ error", no);
    return 0.0f;
}

void cEmWrap::motionSet(void* data, int a, int b, int c, int d)
{
    if (isAlive() == 1) {
        pEm->motionSet(data, a, b, c, d);
    } else {
        err("EM_SET_NO(%d) cEmWrap::motionSet error", no);
    }
}

void cEmWrap::motionMove()
{
    if (isAlive() == 1) {
        pEm->motionMove();
    } else {
        err("EM_SET_NO(%d) cEmWrap::motionMove error", no);
    }
}

void cEmWrap::motionPause(int on)
{
    if (isAlive() == 1) {
        cEm* p = pEm;

        if (on == 1) {
            p->be_flag |= 0x40;
        } else {
            p->be_flag &= ~0x40;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::motionPause error", no);
    }
}

void cEmWrap::addModel(cModelInfo* info)
{
    if (isAlive() == 1) {
        pEm->addModel(info);
    } else {
        err("EM_SET_NO(%d) cEmWrap::addModel error", no);
    }
}

void cEmWrap::setParent(cModel* parent)
{
    if (isAlive() == 1) {
        pEm->pParent = parent;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setParent error", no);
    }
}

void cEmWrap::beginEvent()
{
    if (isAlive() == 1) {
        pEm->beginEvent();
    } else {
        err("EM_SET_NO(%d) cEmWrap::beginEvent error", no);
    }
}

void cEmWrap::endEvent()
{
    if (isAlive() == 1) {
        pEm->endEvent();
    } else {
        err("EM_SET_NO(%d) cEmWrap::endEvent error", no);
    }
}

void cEmWrap::matCalc()
{
    if (isAlive() == 1) {
        pEm->matUpdate();
    } else {
        err("EM_SET_NO(%d) cEmWrap::matCalc error", no);
    }
}

int cEmWrap::isNormalGanade()
{
    if (isAlive() == 1) {
        u8 id = pEm->id;

        if (id > 0xF) {
            if (id <= 0x20) {
                if (id == 0x18) {
                    return 0;
                }
                return 1;
            }
        }
    }
    return 0;
}

void cEmWrap::setGotoSwitch(Vec* pos, int mode, void* sw)
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->setGotoSwitch(pos, mode, sw);
        } else {
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[setGotoSwitch]", pEm->id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setGotoSwitch() error", no);
    }
}

void cEmWrap::setGoto(Vec* pos, int mode)
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->setGoto(pos, mode);
        } else {
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[setGoto]", pEm->id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setGoto() error", no);
    }
}

int cEmWrap::ckGoto()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckGoto();
        }
        pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[ckGoto]", pEm->id);
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckGoto() error", no);
    }
    return 0;
}

int cEmWrap::ckResetEnable()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckResetEnable();
        } else {
            u8 id = pEm->id;

            if (id == 0x2D) {
                return ((cEmDog*) pEm)->ckResetEnable();
            }
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[ckResetEnable]", id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckResetEnable() error", no);
    }
    return 0;
}

void cEmWrap::setReset()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->setReset();
        } else {
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[setReset]", pEm->id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setReset() error", no);
    }
}

void cEmWrap::setReset(int a, int b)
{
    if (isAlive() == 1) {
        u8 id = pEm->id;

        if (id == 0x2D) {
            ((cEmDog*) pEm)->setReset(a, b);
        } else {
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[setReset]", id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setReset() error", no);
    }
}

int cEmWrap::ckFindPL()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckFindPL();
        } else {
            u8 id = pEm->id;

            if (id == 0x2D) {
                return ((cEmDog*) pEm)->ckFindPL();
            }
            if (id == 0x36) {
                return ((cEm36*) pEm)->ckFindPL();
            }
            err("EM_SET_NO(%d) id[%2x] invalid.[ckFindPL]", id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckFindPL() error", no);
    }
    return 0;
}

void cEmWrap::setFindPL()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->setFindPL();
        } else {
            u8 id = pEm->id;

            if (id == 0x36) {
                ((cEm36*) pEm)->setFindPL();
            } else {
                err("EM_SET_NO(%d) id[%2x] invalid.[setFindPL]", id);
            }
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setFindPL() error", no);
    }
}

f32 cEmWrap::get_l_pl()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return pEm->x3CC;   // dead in every module: the field is a guess (only the strings and the -1.0 pool survive)
        }
        err("EM_SET_NO(%d) id[%2x] invalid.[get_l_pl]", pEm->id);
    } else {
        err("EM_SET_NO(%d) cEmWrap::get_l_pl() error", no);
    }
    return -1.0f;
}

void cEmWrap::clearFindPL()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->clearFindPL();
        } else {
            err("EM_SET_NO(%d) id[%2x] invalid.[clearFindPL]", pEm->id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::clearFindPL() error", no);
    }
}

int cEmWrap::ckBowgunFire()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckBowgunFire();
        }
        pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[ckBowgunFire]", pEm->id);
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckBowgunFire() error", no);
    }
    return 0;
}

int cEmWrap::ckParasite()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckParasite();
        }
        pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[ckParasite]", pEm->id);
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckParasite() error", no);
    }
    return 0;
}

int SceCkFindPL(f32* dist)
{
    f32 min = 100000000000000.0f;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* p = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        cEmWrap em;
        Vec pos;

        em.setPtr(p, 0);
        if (em.ckFindPL() == 1) {
            f32 d;

            if (dist == 0) {
                return 1;
            }
            em.getPos(&pos);
            d = PSVECSquareDistance(&pos, &pPL->pos);
            if (min > d) {
                min = d;
            }
        }
    }
    if (dist != 0) {
        if (min < 100000000000000.0f) {
            *dist = SQRTF(min);
            return 1;
        }
        *dist = 0.0f;
        return 0;
    }
    return 0;
}
