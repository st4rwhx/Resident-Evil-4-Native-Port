// pl11 module (pl11.rel = pl15.rel): Ashley in the knight armour, a partner (cSubChar) with its own
// model set, face shapes and hand models built from the partner archive.

#include "atari.h"
#include "light.h"
#include "pl_mod.h"
#include "db_log.h"
#include "esp.h"

#define ARC(no) SUB_ARC(this, no)

static void Pl11Init(cEm* em)
{
    cSubAshley* sub = new (em) cSubAshley();

    sub->modelSet();
    sub->init();
    pG->flags_5010 &= ~0x00020000;
}

cSubAshley::cSubAshley()
{
    hp = pGS->sub_life;
    litArea.on(1);
    pFootShadowTbl = pl_fs_tbl;
    EspDataLoad((u32) ARC(0x11), 4, 0);
    pSUB = this;
}

void cSubAshley::modelSet()
{
    cModelInfo* info;

    if (modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "cSubChar::modelSet() failed.");
    }
    info = ModInfoMgr.create(ARC(7), ARC(0xB));
    subShape = info;
    if (info) {
        addModel(info);
    }
    info = ModInfoMgr.create(ARC(6), ARC(8));
    if (info) {
        addModel(info);
    }
    info = ModInfoMgr.create(ARC(9), ARC(5));
    if (info) {
        addModel(info);
    }
    info = ModInfoMgr.create(ARC(0xA), ARC(5));
    if (info) {
        addModel(info);
    }
    setHand(0);
}

void cSubAshley::setFace(int no)
{
    void* data = 0;
    int type = 0;

    if (subShape == 0) {
        return;
    }
    switch ((u32) no) {
    case 0:
        break;
    case 1:
        data = ARC(0x6C);
        type = 2;
        break;
    case 2:
        data = ARC(0x6D);
        type = 2;
        break;
    case 3:
        data = ARC(0x6C);
        type = 6;
        break;
    case 4:
        data = ARC(0x6D);
        type = 6;
        break;
    }
    if (no != 0) {
        ShapeSet(subShape, 0, data, type);
    } else {
        ShapeEnd(subShape);
    }
}

void cSubAshley::setHand(int no)
{
    cModelInfo* info;
    void* data;

    if (subHand[0]) {
        deleteModelInfo(subHand[0]);
        subHand[0] = 0;
    }
    switch ((u32) no) {
    case 0:
    default:
        data = ARC(0xC);
        break;
    case 1:
        data = ARC(0xD);
        break;
    case 3:
        data = ARC(0xE);
        break;
    }
    subHand[0] = ModInfoMgr.create(data, ARC(5));
    if (subHand[0]) {
        addModel(subHand[0]);
    }
    if (subHand[1]) {
        deleteModelInfo(subHand[1]);
        subHand[1] = 0;
    }
    switch ((u32) no) {
    case 0:
    default:
        data = ARC(0xF);
        break;
    case 1:
        data = ARC(0x10);
        break;
    case 3:
        data = ARC(0xF);
        break;
    }
    info = ModInfoMgr.create(data, ARC(5));
    subHand[1] = info;
    if (info) {
        addModel(info);
    }
}

extern "C" void _prolog()
{
    EmInitFunc = Pl11Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}
