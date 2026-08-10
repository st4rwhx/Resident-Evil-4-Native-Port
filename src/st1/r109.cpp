#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "scroll.h"
#include "obj.h"
#include "emtorch.h"
#include "etc_model.h"

// Room 1-09 (D:/Bio4/Prog/r109.cpp): the hut ("koya") interior; extra collision pieces and the
// hut's scroll-object flags.

struct R109Work {
    cSat* sat[3];   // 0x00
    cSat* eat[3];   // 0x0C
};

static R109Work* r109_work;

void koya_init();

static Vec r109_pos0 = {112971.0f, 2262.0f, 16941.0f};
static Vec r109_pos1 = {117073.0f, 2262.0f, 17411.0f};
static Vec r109_pos2 = {121549.0f, 2262.0f, 15823.0f};
static Vec r109_rot0 = {0.0f, -3.1642818f, 0.0f};
static Vec r109_rot1 = {0.0f, -3.1642818f, 0.0f};
static Vec r109_rot2 = {0.0f, -4.00204f, 0.0f};

static inline void PSet(cSat*& d, cSat* v) { d = v; }

void R109Init()
{
    cObj* obj;
    cEm* torch;

#line 53 "D:/Bio4/Prog/r109.cpp"
    r109_work = (R109Work*) MEM_CALLOC(sizeof(R109Work), 1, 0xd);

    if ((obj = SmdGetObjPtr(0x21)) != 0) {
        Vec ang = {-0.2159845f, -1.4628042f, -2.1205752f};
        obj->setAng(&ang);
    }
    if ((obj = SmdGetObjPtr(0x22)) != 0) {
        Vec ang = {-1.259219f, 1.5707964f, 1.259219f};
        obj->setAng(&ang);
    }
    PSet(r109_work->sat[0], SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), 0, &r109_pos0, &r109_rot0, 0));
    PSet(r109_work->sat[1], SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), 0, &r109_pos1, &r109_rot1, 0));
    PSet(r109_work->sat[2], SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), 0, &r109_pos2, &r109_rot2, 0));
    PSet(r109_work->eat[0], EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x20), 0, &r109_pos0, &r109_rot0, 0));
    PSet(r109_work->eat[1], EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x20), 0, &r109_pos1, &r109_rot1, 0));
    PSet(r109_work->eat[2], EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x20), 0, &r109_pos2, &r109_rot2, 0));

    if (getRoomEtcTorch(0, &torch, 1)) {
        ((cEmTorch*) torch)->setDelete();
    }
    if (getRoomEtcTorch(1, &torch, 1)) {
        ((cEmTorch*) torch)->setDelete();
    }
    if (getRoomEtcTorch(2, &torch, 1)) {
        ((cEmTorch*) torch)->setDelete();
    }
    koya_init();
    SmdSetTrans(0x2C, 0);
}

void R109Main()
{
}

// Hut scroll objects: show the intact set (bit 1) and hide the broken one.
void koya_init()
{
    SmdGetObjPtr(1)->be_flag |= 2;
    SmdGetObjPtr(4)->be_flag |= 2;
    SmdGetObjPtr(5)->be_flag |= 2;
    SmdGetObjPtr(0x27)->be_flag &= ~2;
    SmdGetObjPtr(0x26)->be_flag &= ~2;
    SmdGetObjPtr(2)->be_flag |= 2;
    SmdGetObjPtr(6)->be_flag |= 2;
    SmdGetObjPtr(7)->be_flag |= 2;
    SmdGetObjPtr(0x29)->be_flag &= ~2;
    SmdGetObjPtr(0x28)->be_flag &= ~2;
    SmdGetObjPtr(3)->be_flag |= 2;
    SmdGetObjPtr(8)->be_flag |= 2;
    SmdGetObjPtr(9)->be_flag |= 2;
    SmdGetObjPtr(0x2B)->be_flag &= ~2;
    SmdGetObjPtr(0x2A)->be_flag &= ~2;
}
