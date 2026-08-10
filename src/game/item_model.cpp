#include "item_model.h"
#include "main_mem.h"

cItmSys* g_pItemModelSys;

// Item model pack: offsets from the pack start to the id table, the model table and the
// texture table.
struct ItemModelPack {
    u32 x0;      // 0x00
    u32 ofsId;   // 0x04
    u32 ofsBin;  // 0x08
    u32 ofsTpl;  // 0x0C
};

struct ItemModelId {
    u16 id;      // 0x00
    u16 pad_2;
    u32 pad_4;
};

struct ItemModelIdTbl {
    u32 num;            // 0x00
    ItemModelId id[1];  // 0x04
};

// model / texture tables: offsets from the table start
struct ItemModelOfsTbl {
    u32 num;     // 0x00
    u32 ofs[1];  // 0x04
};

void ItemModelInit()
{
    ItemModelRoomInit();
}

void ItemModelRoomInit()
{
    cItmSys* sys;

#line 94 "D:/Bio4/Prog/item_model.cpp"
    sys = (cItmSys*) MEM_ALLOC(sizeof(cItmSys), 1, 13);
    g_pItemModelSys = sys;
    sys->Init();
}

int ItemModelDataLoad(void* data)
{
    return g_pItemModelSys->DataLoad((u32) data);
}

int ItemGetBinAddr(u8 no, void** pAddr)
{
    return g_pItemModelSys->GetBinAddr(no, pAddr);
}

int ItemGetTplAddr(u8 no, void** pAddr)
{
    return g_pItemModelSys->GetTplAddr(no, pAddr);
}

int ItemGetBinTplAddr(u8 no, void** pBin, void** pTpl)
{
    if (ItemGetBinAddr(no, pBin) == 0) {
        return 0;
    }
    if (ItemGetTplAddr(no, pTpl) == 0) {
        return 0;
    }
    return 1;
}

void cItmSys::WorkClear()
{
    int i;

    for (i = 0; i < 0x100; i++) {
        work[i].bin = 0;
        work[i].tpl = 0;
    }
}

void cItmSys::Init()
{
    WorkClear();
}

int cItmSys::DataLoad(u32 addr)
{
    ItemModelPack* pack = (ItemModelPack*) addr;
    ItemModelIdTbl* idTbl = (ItemModelIdTbl*) (addr + pack->ofsId);
    ItemModelOfsTbl* binTbl = (ItemModelOfsTbl*) (addr + pack->ofsBin);
    ItemModelOfsTbl* tplTbl = (ItemModelOfsTbl*) (addr + pack->ofsTpl);
    u32 i;

    for (i = 0; i < idTbl->num; i++) {
        u16 id = idTbl->id[i].id;

        g_pItemModelSys->ItmRegist((u8*) binTbl + binTbl->ofs[i], (u8*) tplTbl + tplTbl->ofs[i], id);
    }
    return 1;
}

int cItmSys::ItmRegist(void* bin, void* tpl, u8 no)
{
    work[no].bin = bin;
    work[no].tpl = tpl;
    return 1;
}

int cItmSys::GetBinAddr(u8 no, void** pAddr)
{
    *pAddr = work[no].bin;
    if (*pAddr != 0) {
        return 1;
    }
    return 0;
}

int cItmSys::GetTplAddr(u8 no, void** pAddr)
{
    *pAddr = work[no].tpl;
    if (*pAddr != 0) {
        return 1;
    }
    return 0;
}
