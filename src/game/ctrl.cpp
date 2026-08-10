#include "types.h"
#include "cManager.h"
#include "ctrl.h"
#include "light.h"

cCtrlMgr::cCtrlMgr() : cManager<cCtrl>(sizeof(cCtrl), 2)
{
    setName("cCtrlMgr");
}

int cCtrlMgr::construct(cCtrl* p, u32 id)
{
    p->id = id;
    switch (id) {
    case 0:
        new (p) cCtrl00;
        p->be_flag = 1;
        break;
    case 1:
        new (p) cCtrl01;
        p->be_flag = 1;
        break;
    case 0x10:
        new (p) cCtrl10;
        p->be_flag = 1;
        break;
    case 0x11:
        new (p) cCtrl11;
        p->be_flag = 1;
        break;
    case 0x12:
        new (p) cCtrl12;
        p->be_flag = 1;
        break;
    case 0x14:
        new (p) cCtrl14;
        p->be_flag = 1;
        break;
    default:
        new (p) cCtrl;
        p->be_flag = 1;
        break;
    }
    return 1;
}

void cCtrlMgr::move()
{
    u32 i;

    dieCheck();
    for (i = 0; i < nArray; i++) {
        cCtrl* p = (cCtrl*) ((u8*) pArray + size * i);
        if ((p->be_flag & 0x201) == 1) {
            p->move();
        }
    }
}

int cCtrlMgr::trans()
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        cCtrl* p = (cCtrl*) ((u8*) pArray + size * i);
        if ((p->be_flag & 0x201) == 1) {
            p->trans();
        }
    }
    return 1;
}

void cCtrl::move()
{
}

void cCtrl::trans()
{
}

cCtrlMgr CtrlMgr;
