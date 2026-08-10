#include "types.h"
#include "cManager.h"
#include "map_obj.h"
#include "eprintf.h"

cMapMgr MapMgr;

cMapMgr::cMapMgr() : cManager<cMap>(sizeof(cMap), 0)
{
    setName("cMapMgr");
}

int cMapMgr::construct(cMap* p, u32 id)
{
    u8 n = 0;
    u32 i;

    i = 0;
    if (i < nArray) {
        do {
            cMap* q = getWork(i);
            if ((q->be_flag & 0x201) == 1 && (int)id == q->id) {
                n++;
            }
        } while (++i < nArray);
    }
    p = new (p) cMap;
    p->id = id;
    p->index = n;
    return 1;
}

cMap* cMapMgr::room(int id, int no)
{
    u32 i;

    i = 0;
    if (i < nArray) {
        do {
            cMap* p = getWork(i);
            if ((p->be_flag & 0x201) == 1 && id == p->id && no == p->index) {
                return p;
            }
        } while (++i < nArray);
    }
    return 0;
}

void cMapMgr::move()
{
    u32 i;

    i = 0;
    if (i < nArray) {
        do {
            cMap* p = getWork(i);
            if ((p->be_flag & 0x201) == 1) {
                if (p->be_flag & 0x20) {
                    dieCheck();
                    p->move();
                    p->updateOldPos();
                }
            }
        } while (++i < nArray);
    }
    dispInfo();
}

int cMapMgr::dispInfo()
{
    u32 i;
    u32 n;

    if (pArray == 0) {
        return 0;
    }
    n = 0;
    for (i = 0; i < nArray; i++) {
        cMap* p = (cMap*)((u8*)pArray + size * i);
        if ((p->be_flag & 0x201) == 1) {
            n++;
        }
    }
    eprintf(0x1D8, 0x2A, 0, 1, "%3d", nArray - n);
    return 1;
}

cMap::cMap()
{
    x12E = 2;
    be_flag |= 0x1023;
}

void cMap::move()
{
    static int timer = 0;

    if (xFC != 0) {
        return;
    }
    timer = 0;
    xFC++;
}

// unreferenced (the second .sdata word of the unit; the map lost its name)
static int MapObjTimer = 0;
