#include "types.h"
#include "vec.h"
#include "atari.h"
#include "light.h"
#include "global.h"
#include "db_log.h"
#include "main_mem.h"
#include "model.h"
#include "obj.h"
#include "scroll.h"

extern "C" void* memcpy(void* dst, const void* src, unsigned int n);
int MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);
void slideModelAddr(u32 addr, int ofs);
void slideTplAddr(void* tpl, int ofs);

// Scroll object id -> name table (unused in this build; keeps the strings and the table).
struct ScrIdRef {
    u8 type;
    const char* name;
};

static u32 DmyZeroTpl[3] = {0x0020AF30, 0, 0x0000000C};
static ScrIdRef ScrIdRefTbl[16] = {
    {2, "NORMAL"}, {3, "ROTATE"}, {6, "SWING ROT"}, {2, "----"}, {2, "----"}, {2, "----"},
    {2, "----"},   {2, "----"},   {2, "----"},      {2, "----"}, {2, "----"}, {2, "----"},
    {2, "----"},   {2, "----"},   {2, "----"},      {2, "MIRROR"},
};

cSmd* pSmd;
cSmd* pSmdComn;
static cSmx* pSmx;
static cObj** scrObjTbl;   // 250 entries, indexed by scroll object id
static cObj** scrTbl;      // one entry per SMD work
int nScrWork;
static const u8 ScrObjIdNum = 16;

// Never called in this build; keeps ScrIdRefTbl alive (GCC 2.95 emits statics an inline body
// mentions).
static inline const char* scrIdName(u32 no)
{
    if (no < ScrObjIdNum) {
        return ScrIdRefTbl[no].name;
    }
    return NULL;
}

// Not in the DOL: the original linker dead-stripped it (tools/strip_unused.py does the same to
// every function sym_map.tsv does not list). Taking the address is what makes GCC emit the
// otherwise folded `static const` ScrObjIdNum into .sdata2, where the original object has it.
const u8* SmdGetIdNumPtr()
{
    return &ScrObjIdNum;
}

int SmdInit(cSmd* smd, cSmx* smx, cSmd* comn)
{
    if (smd == NULL) {
        pLog->err(0, 0, "ERROR: SMdInit() COMN DATA was NULL");
        pSmd = smd;
        return 0;
    }
    pSmx = smx;
    pSmdComn = comn;
    pSmd = smd;
#line 102 "D:/Bio4/Prog/scroll.cpp"
    scrObjTbl = (cObj**) MEM_ALLOC(250 * sizeof(cObj*), 1, 13);
    nScrWork = pSmd->getWorkNum();
    scrTbl = (cObj**) MEM_ALLOC(nScrWork * sizeof(cObj*), 1, 13);
    SmdClear(0);
    return nScrWork;
}

void SmdClear(int mode)
{
    int i;

    switch (mode) {
    case 0:
        memclr_asm(scrObjTbl, 250 * sizeof(cObj*));
        memclr_asm(scrTbl, nScrWork * sizeof(cObj*));
        break;
    case 1:
        for (i = 0; i < 250; i++) {
            // index-first integer address: the store through the plain pointer may alias the
            // scalar `scrObjTbl`, which is reloaded every iteration in the target.
            cObj** p = (cObj**) (i * sizeof(cObj*) + (u32) scrObjTbl);
            if (*p != NULL && (*p)->blk != -1) {
                *p = NULL;
            }
        }
        break;
    }
}

void workInit(cObj* obj)
{
    obj->setNoSuspend(1);
    obj->x12E = 2;
    obj->type = 0;
    obj->x12F = 3;
    obj->blk = -2;
}

void SmdSetup(int blk)
{
    if (pSmd == NULL) {
        return;
    }
    if (pSmd->version == 0) {
        pLog->err(0, 0, "ERROR! SmdSet() OLD version %02x.", pSmd->version);
    }
    setObj(blk);
}

int setObj(int blk)
{
    SmdWork* w = pSmd->getWorkPtr(0);
    cObj* obj;
    int i;

    for (i = 0; i < pSmd->nWork; i++, w++) {
        if (w->id == 0xFF) {
            continue;
        }
        obj = ObjMgr.createBack(2);
        if (obj == NULL) {
            pLog->err(0, 0, "SmdInit() setObj() CAN'T ALLOC cObj WORK %d", i);
            continue;
        }
        workInit(obj);
        if (scrObjTbl[w->id] == NULL && w->id != 0xFE) {
            scrObjTbl[w->id] = obj;
        }
        scrTbl[i] = obj;
        if (obj->blk != -2 && obj->blk != blk) {
            pLog->err(0, 0, "Smd::setObj() REDECLARATION WORK %d. BLK %d and %d", w->id, obj->blk, blk);
            continue;
        }
        obj->blk = blk;
        if (SmdSetParam(obj, w) == 0) {
            return -1;
        }
        if (pSmx != NULL && w->id != 0xFE) {
            smxInit(obj, w->id);
        }
        if (obj->type == 0) {
            obj->be_flag &= ~0x20;
        }
        obj->matUpdate();
    }
    return 0;
}

int SmdSetParam(cObj* obj, SmdWork* w)
{
    void* bin;
    void* tpl;
    void* mot;
    ModelBound* b;
    Vec size;

    obj->be_flag |= 4;
    obj->be_flag &= ~0x20;
    obj->x3D0 = w->b.x47;
    if (pSmd->version <= 0x1F && w->motNo == 0) {
        w->motNo = 0xFF;
    }
    if (w->binNo == 0xFF) {
        w->binNo = 0;
        pLog->err(0, 0, "SmdInit() NULL BIN USED");
    }
    if (w->tplNo == 0xFF) {
        w->tplNo = 0;
        pLog->err(0, 0, "SmdInit() NULL TPL USED");
    }
    if (w->flags & 0x10) {
        bin = pSmdComn->getBinPtr(w->binNo);
        obj->be_flag |= 0x80000;
    } else {
        bin = pSmd->getBinPtr(w->binNo);
    }
    if (w->flags & 0x10) {
        tpl = DmyZeroTpl;
    } else {
        tpl = pSmd->getTplPtr(w->tplNo);
    }
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "setObj() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    if (pSmdComn != NULL) {
        u8* tbl = (u8*) pSmdComn + pSmdComn->ofsTpl;
        obj->pInfo->addTplAddr(tbl + *(u32*) tbl);
    }
    if (w->motNo != 0xFF) {
        if (w->flags & 0x40) {
            mot = pSmdComn->getMotPtr(w->motNo);
        } else {
            mot = pSmd->getMotPtr(w->motNo);
        }
        if (mot != NULL) {
            MotionSetCore(obj, &obj->pMotion, mot, 0, 0, 5, 0);
        }
    }
    obj->pos = w->pos;
    obj->rot = w->rot;
    obj->scale = w->scale;
    if (obj->scale.x == 0.0f || obj->scale.y == 0.0f || obj->scale.z == 0.0f) {
        pLog->warn(0, 0, "SmdInit() cObj SCALE SET 0.0");
    }
    b = &obj->pInfo->bound;
    size.x = b->size.x;
    size.y = b->size.y;
    size.z = b->size.z;
    obj->lightInfo.init2(2, 1, &obj->pInfo->bound.center, &size, 0x10);
    obj->matUpdate();
    obj->lightInfo.updateMatrix(obj);
    return 1;
}

void SmxSetFlag(cObj* obj, u32 flags)
{
    if (flags & 1) {
        obj->be_flag |= 0x10;
    }
    if (flags & 4) {
        obj->be_flag |= 0x2000000;
    } else {
        obj->be_flag &= ~0x2000000;
    }
    if (flags & 8) {
        obj->x103 = 0x80;
    }
    if (flags & 0x10) {
        obj->be_flag |= 0x8000;
    }
    if (flags & 0x20) {
        obj->x3D0 |= 1;
    }
}

int SmxGetFlag(cObj* obj)
{
    u32 be = obj->be_flag;
    int flags = 0;

    if (be & 0x10) {
        flags = 1;
    }
    if (obj->pInfo->pData->flags & 0x40000000) {
        flags |= 2;
    }
    if (be & 0x2000000) {
        flags |= 4;
    }
    if (obj->x103 != 0xFF) {
        flags |= 8;
    }
    if (be & 0x8000) {
        flags |= 0x10;
    }
    if (obj->x3D0 & 1) {
        flags |= 0x20;
    }
    return flags;
}

// `pSmx` read directly in the loop test: gcse PRE re-loads it for the loop block and cse2 turns
// that into the `mr r10,r9` copy the loop uses; a `cSmx* smx = pSmx` local merges both reads.
void smxInit(cObj* obj, u8 id)
{
    SmxWork* w = pSmx->work;
    int i;

    for (i = 0; i < pSmx->nWork; i++, w++) {
        if (w->id == id) {
            smxInit(obj, w);
            return;
        }
    }
}

void smxInit(cObj* obj, SmxWork* w)
{
    cModelInfo* mi;
    u32 col;

    if (w->id > 0xF9) {
        pLog->err(0, 0, "SmdInit() SMX WORK NUM ERR %d", w->id);
        return;
    }
    if ((u32) obj < 0x80000000 || (u32) obj > 0x82FFFFFF || (obj->be_flag & 0x201) != 1) {
        pLog->err(0, 0, "SmdInit() SMX UNUSED cObj SELECT %d", w->id);
        return;
    }
    obj->type = w->type;
    obj->lightInfo.x54 = w->x4;
    obj->x12F = w->type2;
    SmxSetFlag(obj, w->flags);
    obj->x135 = w->x3;
    mi = obj->pInfo;
    if (mi != NULL) {
        col = w->color;
        *(u32*) mi->color = col;
        if ((col & ~0xFF) == 0) {
            mi->color[0] = 0xFF;
            mi->color[1] = 0xFF;
            mi->color[2] = 0xFF;
        }
        col = w->color2;
        *(u32*) mi->color2 = col;
        if ((col & ~0xFF) == 0) {
            mi->color2[3] = 0;
        } else {
            mi->color2[3] = 0xFF;
        }
        mi->xD6 = mi->color[3];
        mi->color[3] = 0xFF;
        mi->uvScrollU = w->uvScrollU;
        mi->uvScrollV = w->uvScrollV;
        if (w->uvScrollU != 0.0f || w->uvScrollV != 0.0f) {
            mi->flagsDC |= 1;
        }
    }
    memcpy(obj->work, w->work, 0x78);
    if (obj->type == 0xF) {
        pLog->err(0, 0, "smxInit() : mirror model used.");
    }
    if (obj->type != 0) {
        obj->be_flag |= 0x20;
    }
}

void* SmdGetTplPtr(int no)
{
    u8* tbl = (u8*) pSmd + pSmd->ofsTpl;
    return tbl + ((u32*) tbl)[no];
}

cObj* SmdGetObjPtr(u32 id)
{
    cObj* obj;

    if (id > 0xF9) {
        if (pG->flags_60 & 0x80000000) {
            if (!(pG->flags_60 & 0x2000000)) {
                return NULL;
            }
        }
        {
            // The format string in a local: the string address is expanded before the pLog load,
            // so jump.c does not hoist the early `return NULL`s above the flag tests (the three
            // `li r3, 0` tails are cross-jumped instead) and each error block keeps its schedule.
            const char* s = "SmdGetObjPtr() invalid ID [%d] used";
            pLog->err(0, 0, s, id);
        }
        id = 0;
    }
    obj = scrObjTbl[id];
    if ((u32) obj < 0x80000000 || (u32) obj > 0x82FFFFFF) {
        if (pG->flags_60 & 0x80000000) {
            if (!(pG->flags_60 & 0x2000000)) {
                return NULL;
            }
        }
        {
            const char* s = "SmdGetObjPtr(%d) invalid work";
            pLog->err(0, 0, s, id);
        }
        return NULL;
    }
    if (obj->x3D0 & 4) {
        pLog->err(0, 0, "SmdGetObjPtr(%d) GROUP -> SmdGetGroupObjPtr()", id);
    }
    return scrObjTbl[id];
}

int SmdGetObjNum()
{
    return nScrWork;
}

int SmdGetWorkId(cObj* obj)
{
    cObj* p;
    int i;

    for (i = 0; i < 250; i++) {
        for (p = scrObjTbl[i]; p != NULL; p = SmdGetGroupNext(p)) {
            if (p == obj) {
                return i;
            }
        }
    }
    return -1;
}

void BlockCreate(int blk, cSmd* smd)
{
    pSmd = smd;
    SmdSetup(blk);
}

void BlockDestroy(int blk)
{
    cObj* p = ObjMgr.pAlive;
    cObj* cur;
    cObj* next;

    // Entry test on the head, bottom test on `next` (pl_sub PlDataRelease shape).
    if (p != NULL) {
        do {
            cur = p;
            next = (cObj*) cur->next;
            p = next;
            if (cur->x12E == 2 && cur->blk == blk) {
                ObjMgr.destroy(cur);
            }
        } while (next != NULL);
    }
}

void cSmd::slide(int ofs)
{
    SmdWork* w = getWorkPtr(0);
    int nBin = 0;
    int nTpl;
    u32* tbl;
    u32 addr;
    int i;

    if ((u32) w < 0x80000000 || (u32) w > 0x82FFFFFF) {
        pLog->err(0, 0, "cSmd::slide(%d) PTR ERROR", ofs);
        return;
    }
    for (i = 0; i < nWork; i++, w++) {
        if (w->id != 0xFF && !(w->flags & 0x10) && w->binNo + 1 > nBin) {
            nBin = w->binNo + 1;
        }
    }
    {
        // An integer base (not a pointer) ranks below the hoisted 0x02FFFFFF constant in the
        // callee-saved allocation (base r28, constant r29).
        u32 base = (u32) this + ofsBin;
        for (i = 0; i < nBin; i++) {
            addr = base + ((u32*) base)[i];
            if (addr < 0x80000000 || addr > 0x82FFFFFF) {
                pLog->err(0, 0, "cSmd::slide() PTR ERR %08X", addr);
                return;
            }
            slideModelAddr(addr, ofs);
        }
    }
    nTpl = 0;   // set before the call: the pseudo crosses it and takes a callee-saved register
    w = getWorkPtr(0);
    for (i = 0; i < nWork; i++, w++) {
        if (w->id != 0xFF && !(w->flags & 0x10) && w->tplNo + 1 > nTpl) {
            nTpl = w->tplNo + 1;
        }
    }
    tbl = (u32*) ((u8*) this + ofsTpl);
    for (i = 0; i < nTpl; i++) {
        slideTplAddr((u8*) tbl + tbl[i], ofs);
    }
}

SmdWork* cSmd::getWorkPtr(int no)
{
    return (flags & 1) ? (SmdWork*) ((u8*) this + grp.nGroup * 4 + 0x14) : &work[no];
}

void* cSmd::getBinPtr(int no)
{
    u8* tbl = (u8*) this + ofsBin;
    return tbl + ((u32*) tbl)[no];
}

void* cSmd::getTplPtr(int no)
{
    u8* tbl = (u8*) this + ofsTpl;
    return tbl + ((u32*) tbl)[no];
}

void* cSmd::getMotPtr(int no)
{
    u8* tbl = (u8*) this + ofsMot;
    return tbl + ((u32*) tbl)[no];
}

int cSmd::getWorkNum()
{
    int n = nWork;
    u32 i;

    if (flags & 1) {
        // guarded do-while + indexing: the loop test's second `grp.nGroup` read becomes the
        // `mr r10,r0` PRE copy, and `grp.num[i]` gives the `addi r3,r3,0x14` after the compare
        i = 0;
        if (i < grp.nGroup) {
            do {
                n += grp.num[i];
                i++;
            } while (i < grp.nGroup);
        }
    }
    return n;
}

SmdWork* SmdGetWorkPtr(int id)
{
    SmdWork* w;
    u32 i;

    for (i = 0; i < pSmd->getWorkNum(); i++) {
        w = pSmd->getWorkPtr(i);
        if (w->id == id) {
            return w;
        }
    }
    return NULL;
}

cObj* SmdGetGroupObjPtr(u32 id)
{
    cObj* obj;

    if (id > 0xF9) {
        if (pG->flags_60 & 0x80000000) {
            if (!(pG->flags_60 & 0x2000000)) {
                goto ng;
            }
        }
        {
            const char* s = "SmdGetObjPtr() invalid ID [%d] used";
            pLog->err(0, 0, s, id);
        }
        id = 0;
    }
    obj = scrObjTbl[id];
    if ((u32) obj < 0x80000000 || (u32) obj > 0x82FFFFFF) {
        if (pG->flags_60 & 0x80000000) {
            if (!(pG->flags_60 & 0x2000000)) {
                goto ng;
            }
        }
        {
            const char* s = "SmdGetObjPtr(%d) invalid work";
            pLog->err(0, 0, s, id);
        }
        // The shared `return NULL` block sits after the second error call and the call jumps over
        // it to the normal return (the layout of the original).
        goto ok;
    ng:
        return NULL;
    }
ok:
    return scrObjTbl[id];
}

cObj* SmdGetGroupObjPtr2(u32 id)
{
    if (id > 0xF9) {
        return NULL;
    }
    return scrObjTbl[id];
}

cObj* SmdGetGroupNext(cObj* obj)
{
    if (!(obj->x3D0 & 4)) {
        return NULL;
    }
    return ObjMgr.getPrevWork(obj);
}

void SmdSetTrans(u32 id, int on)
{
    cObj* obj = SmdGetGroupObjPtr(id);

    if ((u32) obj < 0x80000000 || (u32) obj > 0x82FFFFFF) {
        pLog->err(0, 0, "SmdSetTrans() INVALID INDEX %d", id);
        return;
    }
    do {
        if (on == 1) {
            obj->be_flag |= 2;
        } else {
            obj->be_flag &= ~2;
        }
        obj = SmdGetGroupNext(obj);
    } while (obj != NULL);
}

cObj* SetObjSmd(void* bin, void* tpl, Vec* pos, Vec* rot, int lightFlag, int front)
{
    cObj* obj;
    cModelInfo* mi;
    ModelBound* b;
    Vec size;
    Vec d;

    if (front == 1) {
        obj = ObjMgr.create(2);
    } else {
        obj = ObjMgr.createBack(2);
    }
    if (obj == NULL) {
        return NULL;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(obj);
        return NULL;
    }
    obj->pos = *pos;
    obj->rot = *rot;
    obj->setNoSuspend(1);
    obj->be_flag |= 0x20;
    obj->blk = -1;
    mi = obj->pInfo;
    b = &mi->bound;
    size.x = b->size.x;
    size.y = b->size.y;
    size.z = b->size.z;
    PSVECSubtract(&mi->bound.center, &obj->pParts->pos, &d);
    obj->lightInfo.init2(2, 1, &d, &size, lightFlag);
    return obj;
}
