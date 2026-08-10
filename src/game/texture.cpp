#include "types.h"
#include "vec.h"
#include "gx.h"
#include "tpl.h"
#include "db_log.h"
#include "main_mem.h"
#include "texture.h"

#line 20 "D:/Bio4/Prog/texture.cpp"

int lod_enable = 0;
int tex_dummy = 0;

static inline int IsPow2(u32 n)
{
    return (n & (n - 1)) == 0;
}

void cTexSys::Init(const char* name, u32 num)
{
    u32 i;

    this->name = name;
    nTexObj = num;
#line 53
    if ((pTexObj = (GXTexObj*) MEM_ALLOC(num * sizeof(GXTexObj), 1, 0xD)) == NULL) {
        nTexObj = 0;
        pLog->err(0, 0, "%s::Init(): Memory Allocation Failed.", this->name);
        return;
    }
#line 60
    if ((pFlag = (u8*) MEM_ALLOC(nTexObj / 8 + 1, 1, 0xD)) == NULL) {
        nTexObj = 0;
        pLog->err(0, 0, "%s::Init(): Memory Allocation Failed.", this->name);
        return;
    }
    for (i = 0; i < nTexObj; i++) {
        SetTexObjFlag(i, 0);
    }
    Clear();
}

void cTexSys::Clear()
{
    u32 i;
    TexWk* w = wk;

    for (i = 0; i < 256; i++, w++) {
        w->owner = 0;
    }
    x5408 = 0;
    for (i = 0; i < nTexObj; i++) {
        SetTexObjFlag(i, 0);
    }
}

int cTexSys::GetTexObjFlag(u32 no)
{
    if (no >= nTexObj) {
        pLog->err(0, 0, "%s::GetTexObjFlag : TexNo over [%d/%d]", name, no, nTexObj);
        return 0;
    }
    if ((pFlag[no >> 3] >> (no & 7)) & 1) {
        return 1;
    }
    return 0;
}

void cTexSys::SetTexObjFlag(u32 no, int flag)
{
    u8 bit;

    if (no >= nTexObj) {
        pLog->err(0, 0, "%s::GetTexObjFlag : TexNo over [%d/%d]", name, no, nTexObj);
    }
    bit = 1 << (no & 7);
    if (flag == 1) {
        pFlag[no >> 3] |= bit;
    } else {
        pFlag[no >> 3] &= ~bit;
    }
}

int cTexSys::DataLoad(TexData* data, u32 owner, int clamp)
{
    TexIdTbl* ids;
    TexOfsTbl* tpls;
    TexOfsTbl* anms;
    u32 i;
    // COMPILER-DIFF: candidate (local-alloc qty order). The target names the three offset
    // temporaries r9/r11/r0 (ofsId/ofsTpl/ofsAnm), ours r0/r9/r0: a fake-lifetime tie decided by
    // the sched1 position of the third `lwz` (30 statement/base/order forms tried); the two pins
    // give the target's names with the same schedule.
    register u32 oI asm("r9");
    register u32 oT asm("r11");

    if (data->version != 3) {
        pLog->err(0, 0, "%s::DataLoad() : Data Invalid. [0x%x]", name, data);
        return 0;
    }
    oI = data->ofsId;
    ids = (TexIdTbl*) ((u8*) data + oI);
    oT = data->ofsTpl;
    tpls = (TexOfsTbl*) ((u8*) data + oT);
    anms = (TexOfsTbl*) ((u8*) data + data->ofsAnm);
    for (i = 0; i < ids->num; i++) {
        TEXPalette* tpl = (TEXPalette*) ((u8*) tpls + tpls->ofs[i]);
        TexAnm* anm = (TexAnm*) ((u8*) anms + anms->ofs[i]);
        u16 id = ids->ent[i].id;
        TexRegist(tpl, anm, id, owner, clamp, 1);
    }
    return 1;
}

GXTexObj* cTexSys::PullTexObj(u32 num)
{
    u32 start = 0;
    u32 cnt = 0;
    u32 i;
    GXTexObj* obj;

    while (cnt != num) {
        if (GetTexObjFlag(start + cnt) == 0) {
            cnt++;
        } else {
            start++;
            cnt = 0;
        }
        if (start + cnt >= nTexObj) {
            goto full;
        }
    }
    obj = &pTexObj[start];
    for (i = 0; i < num; i++) {
        SetTexObjFlag(start + i, 1);
    }
    goto done;

full:
    pLog->err(0, 0, "%s::PullTexObj(): TEXOBJ MAX!!", name);
    return NULL;

done:
    return obj;
}

void cTexSys::CalcTplAddr(TEXPalette* tpl)
{
    u32 i;
    TEXDescriptor* desc;

    if (tpl == NULL) {
        return;
    }
    if ((s32) tpl->descriptorArray < 0) {
        return;
    }
    tpl->descriptorArray = (TEXDescriptor*) ((u32) tpl->descriptorArray + (u32) tpl);
    for (i = 0; i < tpl->numDescriptors; i++) {
        desc = &tpl->descriptorArray[i];
        desc->textureHeader = (TEXHeader*) ((u8*) tpl + (u32) desc->textureHeader);
        desc->textureHeader->data = (u8*) tpl + (u32) desc->textureHeader->data;
        if (desc->CLUTHeader != NULL) {
            desc->CLUTHeader = (CLUTHeader*) ((u8*) tpl + (u32) desc->CLUTHeader);
            desc->CLUTHeader->data = (u8*) tpl + (u32) desc->CLUTHeader->data;
        }
    }
}

int cTexSys::TexRegist(TEXPalette* tpl, TexAnm* anm, u8 id, u32 owner, int clamp, int check)
{
    TexWk* w = &wk[id];
    TEXDescriptor* desc;
    TEXHeader* hdr;
    GXTexObj* obj;
    int i;

    if (w->owner != 0) {
        if (check != 0) {
            pLog->err(0, 0, "%s::TexRegist():TexId[%x] id already used.", name, id);
        }
        return 0;
    }
    CalcTplAddr(tpl);
    desc = TEXGet(tpl, 0);
    w->nTex = anm->numTex;
    w->pTexObj = PullTexObj(w->nTex);
    if (w->pTexObj == NULL) {
        pLog->err(0, 0, "%s : ID[%02x] PullTexObj() work full!!", name, id);
        return 0;
    }
    w->texHdr = desc->textureHeader;
    w->pAnm = anm;
    w->owner = owner;
    w->pTpl = tpl;
    for (i = 0; i < w->nTex; i++) {
        obj = &w->pTexObj[i];
        desc = TEXGet(tpl, i);
        hdr = desc->textureHeader;
        if (hdr->format - 8 <= 1) {
            if (clamp == 0 && IsPow2(hdr->width) && IsPow2(hdr->height)) {
                GXInitTexObjCI(obj, hdr->data, hdr->width, hdr->height, hdr->format, 1, 1, 0, 0);
            } else {
                GXInitTexObjCI(obj, desc->textureHeader->data, desc->textureHeader->width, desc->textureHeader->height,
                               desc->textureHeader->format, 0, 0, 0, 0);
            }
            GXInitTlutObj(&w->tlut, desc->CLUTHeader->data, desc->CLUTHeader->format, desc->CLUTHeader->numEntries);
            GXLoadTlut(&w->tlut, 0);
        } else {
            if (clamp == 0 && IsPow2(hdr->width) && IsPow2(hdr->height)) {
                GXInitTexObj(obj, hdr->data, hdr->width, hdr->height, hdr->format, 1, 1, 0);
            } else {
                GXInitTexObj(obj, desc->textureHeader->data, desc->textureHeader->width, desc->textureHeader->height,
                             desc->textureHeader->format, 0, 0, 0);
            }
        }
        if (lod_enable) {
            TEXHeader* h = desc->textureHeader;
            GXInitTexObjLOD(obj, 0, 0, (f32) h->minLOD, (f32) h->maxLOD, h->LODBias, 0, h->edgeLODEnable, 0);
        }
    }
    PSMTXIdentity(w->mtx);
    return 1;
}

int cTexSys::GetTplAddr(u32 id, TEXPalette** out)
{
    TexWk* w = &wk[id];

    if (w->owner == 0) {
        return 0;
    }
    *out = w->pTpl;
    return 1;
}

int cTexSys::GetTexObj(u32 id, u32 no, GXTexObj** out)
{
    TexWk* w = &wk[id];

    if (w->owner == 0) {
        return 0;
    }
    *out = &w->pTexObj[no];
    return 1;
}

int cTexSys::GetAnmAddr(u32 id, TexAnm** out)
{
    TexWk* w = &wk[id];

    if (w->owner == 0) {
        return 0;
    }
    *out = w->pAnm;
    return 1;
}

int cTexSys::GetTlutObj(u32 id, GXTlutObj** out)
{
    TexWk* w = &wk[id];

    if (w->owner == 0) {
        return 0;
    }
    if (w->texHdr->format - 8 <= 1) {
        *out = &w->tlut;
        return 1;
    }
    *out = NULL;
    return 0;
}

TexWk* cTexSys::GetTexWk(u32 id, int quiet)
{
    TexWk* w = &wk[id];

    if (w->owner != 0) {
        return w;
    }
    if (quiet == 0) {
        pLog->err(0, 0, "GetTexWk(): TexId[%x] No such texture", id);
    }
    return NULL;
}

int cTexSys::TexRelease(u32 owner)
{
    TexWk* w;
    u32 i;
    u32 j;
    u32 base;

    for (w = wk, i = 0; i < 256; w++, i++) {
        if (w->owner == owner) {
            w->owner = 0;
            base = w->pTexObj - pTexObj;
            for (j = base; j < base + w->nTex; j++) {
                SetTexObjFlag(j, 0);
            }
        }
    }
    return 1;
}
