#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "est.h"
#include "tpl.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"

// Matching. EspDataLoad takes the data address as a `u32` (not a pointer): with a pointer-flagged
// base the table offsets are index registers (GENERAL_REGS, r0 first); with an integer base
// regclass counts base/index equally and the offsets prefer BASE_REGS (r9/r11).
// Effect system core: the cEspSystem work (g_pEspSys), per-owner registration of the effect
// data files (textures, effect set tables, room effect tables, paths, effect models) and the
// small state accessors the game code uses.

#define EFF_OWNER_MAX 0xD3   // owner ids 0..0xD2; 0xD2 = "NONE" marks a free table entry
#define EFF_TEXOBJ_MAX 0x1F4

// Effect data file (EspDataLoad, version 0xB): byte offsets from the file start.
struct EffIdTbl {
    u32 num;           // 0x00
    struct {
        u16 id;        // 0x00
        u16 x2;
        u32 x4;
    } ent[1];          // 0x04
};
struct EffOfsTbl {
    u32 num;           // 0x00
    u32 ofs[1];        // 0x04 relative to the table
};
struct EffEfmEnt {
    u32 x0;            // 0x00
    u32 ofsModel;      // 0x04 relative to the entry
    u32 ofsTpl;        // 0x08
    u32 ofsMot;        // 0x0C 0 = none
    u32 ofsX;          // 0x10 0 = none
};
struct EffData {
    u32 version;       // 0x00 == 0xB
    u32 ofsTexId;      // 0x04 EffIdTbl of texture ids
    u32 ofsEstList;    // 0x08
    u32 ofsSstList;    // 0x0C
    u32 ofsPathList;   // 0x10
    u32 ofsEfmId;      // 0x14 EffIdTbl of effect model ids
    u32 ofsTpl;        // 0x18 EffOfsTbl of TPLs
    u32 ofsAnm;        // 0x1C EffOfsTbl of texture animations
    u32 ofsEstData;    // 0x20
    u32 ofsSstData;    // 0x24
    u32 ofsPathData;   // 0x28
    u32 ofsEfm;        // 0x2C EffOfsTbl of EffEfmEnt
};

extern "C" {
// game/esp.cpp
void EspFuncTblInit();
int EspMove();
// game/espgen.cpp
int EspgenMove();

void EspInit();
void EspRoomInit();
GXTexObj* EspPullTexObj(u32 num);
int EspDataLoad(u32 addr, u32 owner, int flag);
int EffAreaDataLoad(SstArea* area);
int EspDataRelease(u32 owner, int flag, int warn);
EspTexWk* EspGetTexWk(int id, int quiet);
int EspGetTexOwner(int id, u32* out);
int espTexRegist(TEXPalette* tpl, EspAnmData* anm, u8 id, u32 owner);
int estRegist(void* data, void* list, u32 owner);
int sstRegist(void* data, void* list, u32 owner);
int pathRegist(void* data, void* list, u32 owner);
int efmRegist(void* model, void* tpl, void* mot, void* x, u8 id, u32 owner);
int espTexRelease(u32 owner);
int estRelease(u32 owner);
int sstRelease(u32 owner);
int pathRelease(u32 owner);
int efmRelease(u32 owner);
int EspGetEfmAddr(int id, void** model, void** tpl);
int EspGetEfmMotAddr(int id, u32 no, void** out);
u8 EspPullCoreKind();
int EffIsSetFinalCol();
void EffGetFinalCol(GXColor* col);
void EffSetFinalCol(u8 r, u8 g, u8 b, u8 a);
int GetAreaState(int no);
void SetAreaState(int no, int on);
int EffGetAreaState(int no);
void EffSetToolState(int state);
u8 EffGetToolState();
void EffClearToolState();
void EffSetToolStateCallBack(int no, void (*on)(), void (*off)());
void EffCallToolStateCallBack();
}
void RoomEfmRegist(cModel* m, u8 id);
void RoomEfmRegist(void* model, void* tpl, u8 id);

#define OWNER_ERR(owner, fmt_s, fmt_x)                                                              \
    if ((owner) <= 0xD0) {                                                                          \
        pLog->err(0, 0, fmt_s, owner_name_tbl[owner]);                                              \
    } else {                                                                                        \
        pLog->err(0, 0, fmt_x, owner);                                                              \
    }
#define OWNER_ERR2(owner, fmt_s, fmt_x, arg)                                                        \
    if ((owner) <= 0xD0) {                                                                          \
        pLog->err(0, 0, fmt_s, owner_name_tbl[owner], arg);                                         \
    } else {                                                                                        \
        pLog->err(0, 0, fmt_x, owner, arg);                                                         \
    }
#define OWNER_WARN2(owner, fmt_s, fmt_x, arg)                                                       \
    if ((owner) <= 0xD0) {                                                                          \
        pLog->warn(0, 0, fmt_s, owner_name_tbl[owner], arg);                                        \
    } else {                                                                                        \
        pLog->warn(0, 0, fmt_x, owner, arg);                                                        \
    }

char* owner_name_tbl[0xD1] = {
    "CORE",  "ROOM",  "EM3A",  "PL00",  "PL01",  "PL02",  "PL03",  "PL04",  "PL05",  "PL06",  "PL07",  "PL0A",
    "PL0B",  "PL0D",  "PL0E",  "PL0F",  "EM10",  "EM12",  "EM15",  "EM16",  "EM17",  "EM18",  "EM19",  "EM1A",
    "EM1B",  "EM20",  "EM22",  "EM23",  "EM24",  "EM25",  "EM26",  "EM27",  "EM28",  "EM29",  "EM2A",  "EM2B",
    "EM2C",  "EM2D",  "EM2E",  "EM2F",  "EM30",  "EM31",  "EM32",  "EM34",  "EM35",  "EM36",  "EM38",  "EM39",
    "EM3B",  "EM3C",  "EM3D",  "EM3E",  "WEP00", "WEP01", "WEP02", "WEP03", "WEP04", "WEP05", "WEP06", "WEP07",
    "WEP08", "WEP09", "WEP0A", "WEP0B", "WEP0C", "WEP0D", "WEP0E", "WEP0F", "WEP10", "WEP11", "WEP12", "WEP13",
    "WEP14", "WEP15", "WEP16", "WEP17", "WEP18", "WEP19", "WEP26", "WEP27", "WEP28", "WEP29", "WEP30", "WEP33",
    "ET00",  "ET01",  "ET02",  "ET03",  "ET04",  "ET05",  "ET06",  "ET07",  "ET08",  "ET09",  "ET0A",  "ET0B",
    "ET0C",  "ET0D",  "ET0E",  "ET0F",  "ET10",  "ET11",  "ET12",  "ET13",  "ET14",  "ET15",  "ET16",  "ET17",
    "ET18",  "ET19",  "ET1A",  "ET1B",  "ET1C",  "ET1D",  "ET1E",  "ET1F",  "ET20",  "ET21",  "ET22",  "ET23",
    "ET24",  "ET25",  "ET26",  "ET27",  "ET28",  "ET29",  "ET2A",  "ET2B",  "ET2C",  "ET2D",  "ET2E",  "ET2F",
    "ET30",  "ET31",  "ET32",  "ET33",  "ET34",  "ET35",  "ET36",  "ET37",  "ET38",  "ET39",  "ET3A",  "ET3B",
    "ET3C",  "ET3D",  "ET3E",  "ET3F",  "ET40",  "ET41",  "ET42",  "ET43",  "ET44",  "ET45",  "ET46",  "ET47",
    "ET48",  "ET49",  "ET4A",  "ET4B",  "ET4C",  "ET4D",  "ET4E",  "ET4F",  "ET50",  "ET51",  "ET52",  "ET53",
    "ET54",  "ET55",  "ET56",  "ET57",  "ET58",  "ET59",  "ET5A",  "ET5B",  "ET5C",  "ET5D",  "ET5E",  "ET5F",
    "ET60",  "ET61",  "ET62",  "ET63",  "ET64",  "ET65",  "ET66",  "ET67",  "ET68",  "ET69",  "ET6A",  "ET6B",
    "ET6C",  "ET6D",  "ET6E",  "ET6F",  "EV00",  "EV01",  "EV02",  "EV03",  "OBM1F", "OBM2B", "OBM34", "OBM4C",
    "OBM66", "OBM83", "SUBSCR", "DEBUG", "NONE",
};

cEsp g_DmyEsp;
static cCoord g_EffParentWorld;
u8 g_EspCommonDisplayList[0x60] __attribute__((aligned(32)));
u32 g_nLoop = 0;
cCoord* pEffParentWorld;
cEspSystem* g_pEspSys;
static void* g_EspToolSeqHedAddr;

// never called (keeps the static alive)
static inline void EffSetToolSeqHedAddr(void* p)
{
    g_EspToolSeqHedAddr = p;
}

void EspInit()
{
    u8* d = g_EspCommonDisplayList;

    g_pEspSys = NULL;
    EspFuncTblInit();
    EffSetId();
    memclr_asm(d, sizeof(g_EspCommonDisplayList));
    *d = 0;
    d++;
    *d = 0x80;
    d++;
    *(u16*) d = 4;
    d++;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 0;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 1;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 0;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 1;
    d++;
    *d = 0;
    d++;
    *d = 0;
    d++;
    *d = 1;
    DCStoreRange(g_EspCommonDisplayList, sizeof(g_EspCommonDisplayList));
}

void EspRoomInit()
{
    cEspSystem* p;
    cEspSystem* sys;
    EspTexWk* tw;
    EspEfmWk* ew;
    SstTbl* et;
    SstTbl* st;
    SstTbl* pt;
    cModel** m;
    int i;

    EspFreeSizeCheckAll();
    EffCrearRoomSeFunc();
    if (EFF_OWNER_MAX > 220) {
        pLog->err(0, 0, "EspRoomInit(): EFF_MAX > 220 [%d]", EFF_OWNER_MAX);
    }
#line 200 "D:/Bio4/Prog/eff_sys.cpp"
    p = (cEspSystem*) MEM_ALLOC(sizeof(cEspSystem), 1, 13);
    g_pEspSys = p;
    memclr_asm(p, sizeof(cEspSystem));
    sys = g_pEspSys;
    sys->areaState = 0;
    EffSetFinalCol(0xFF, 0xFF, 0xFF, 0xFF);
    g_pEspSys->pDmy = &g_DmyEsp;
    pEffParentWorld = &g_EffParentWorld;
    PSMTXIdentity(g_EffParentWorld.mat);
    sys->sstDispFlag = 0xFFFFFFFF;
    sys->coreKind = 0x45;
    sys->pEspBuf = NULL;
    tw = sys->texWk;
    for (i = 0; i < 0x100; i++) {
        tw->owner = 0xD2;
        tw++;
    }
    ew = sys->efmWk;
    for (i = 0; i < 0x100; i++) {
        ew->owner = 0xD2;
        ew++;
    }
    et = sys->estTbl;
    for (i = 0; i < EFF_OWNER_MAX; i++) {
        et->owner = 0xD2;
        et++;
    }
    st = sys->sstTbl;
    for (i = 0; i < EFF_OWNER_MAX; i++) {
        st->owner = 0xD2;
        st++;
    }
    pt = sys->pathTbl;
    for (i = 0; i < EFF_OWNER_MAX; i++) {
        pt->owner = 0xD2;
        pt++;
    }
    EspDataLoad(pG->pArc->ofs_14 + (u32) pG->pArc, 0, 0);
    EspDataLoad(pG->pArc->ofs_50 + (u32) pG->pArc, 0xD1, 0);
    g_nLoop = 200;
    sys->pEspBufSave = NULL;
    m = EspEvModList;
    for (i = 0; i < 0x80; i++) {
        *m = NULL;
        m++;
    }
}

GXTexObj* EspPullTexObj(u32 num)
{
    cEspSystem* sys = g_pEspSys;
    u32 start = 0;
    u32 cnt = 0;
    u32 i;
    GXTexObj* obj;

    if (num != 0) {
        do {
            if (sys->GetTexObjFlag(start + cnt) == 0) {
                cnt++;
            } else {
                start++;
                cnt = 0;
            }
            if (start + cnt >= EFF_TEXOBJ_MAX) {
                goto full;
            }
        } while (cnt != num);
    }
    obj = &sys->texObj[start];
    for (i = 0; i < num; i++) {
        sys->SetTexObjFlag(start + i, 1);
    }
    goto done;

full:
    pLog->err(0, 0, "ESP_PullTexObj(): TEXOBJ MAX!!");
    return NULL;

done:
    return obj;
}

int EspDataLoad(u32 addr, u32 owner, int flag)
{
    EffData* data = (EffData*) addr;
    cEspSystem* sys = g_pEspSys;
    EffIdTbl* ids;
    EffOfsTbl* tpls;
    EffOfsTbl* anms;
    EffIdTbl* efmIds;
    EffOfsTbl* efms;
    void* list;
    u32 i;

    if (data == NULL) {
        OWNER_ERR(owner, "EspDataLoad():EffData [%s:NULL] Invalid.", "EspDataLoad():EffData [%x:NULL] Invalid.");
        return 0;
    }
    if (sys->ownerCnt[owner] <= 0xC7) {
        sys->ownerCnt[owner]++;
    }
    if (sys->ownerCnt[owner] != 1) {
        if (flag != 1) {
            return 1;
        }
        OWNER_ERR2(owner, "EspDataLoad():[%s] data already regist.", "EspDataLoad():[%x] data already regist.", data);
        return 0;
    }
    if (data->version != 0xB) {
        OWNER_ERR2(owner, "EspDataLoad():EffData [%s:0x%x] Invalid.", "EspDataLoad():EffData [%x:0x%x] Invalid.",
                   data);
        return 0;
    }
    ids = (EffIdTbl*) ((u8*) data + data->ofsTexId);
    tpls = (EffOfsTbl*) ((u8*) data + data->ofsTpl);
    anms = (EffOfsTbl*) ((u8*) data + data->ofsAnm);
    for (i = 0; i < ids->num; i++) {
        TEXPalette* tpl = (TEXPalette*) ((u8*) tpls + tpls->ofs[i]);
        EspAnmData* anm = (EspAnmData*) ((u8*) anms + anms->ofs[i]);
        u16 id = ids->ent[i].id;
        espTexRegist(tpl, anm, id, owner);
    }
    list = (u8*) data + data->ofsEstList;
    estRegist((u8*) data + data->ofsEstData, list, owner);
    list = (u8*) data + data->ofsSstList;
    sstRegist((u8*) data + data->ofsSstData, list, owner);
    list = (u8*) data + data->ofsPathList;
    pathRegist((u8*) data + data->ofsPathData, list, owner);
    efmIds = (EffIdTbl*) ((u8*) data + data->ofsEfmId);
    efms = (EffOfsTbl*) ((u8*) data + data->ofsEfm);
    for (i = 0; i < efmIds->num; i++) {
        EffEfmEnt* e = (EffEfmEnt*) ((u8*) efms + efms->ofs[i]);
        void* model = (u8*) e + e->ofsModel;
        void* tpl = (u8*) e + e->ofsTpl;
        void* mot = NULL;
        void* x;
        u16 id;
        if (e->ofsMot != 0) {
            mot = (u8*) e + e->ofsMot;
        }
        x = NULL;
        if (e->ofsX != 0) {
            x = (u8*) e + e->ofsX;
        }
        id = efmIds->ent[i].id;
        efmRegist(model, tpl, mot, x, id, owner);
    }
    return 1;
}

int EffAreaDataLoad(SstArea* area)
{
    if (g_pEspSys->pSstArea != NULL) {
        pLog->err(0, 0, "EffAreaDataLoad() : data already regist.");
        return 0;
    }
    g_pEspSys->pSstArea = area;
    return 1;
}

int EspDataRelease(u32 owner, int flag, int warn)
{
    cEspSystem* sys = g_pEspSys;

    if (sys->ownerCnt[owner] == 0) {
        if (warn != 0) {
            OWNER_ERR(owner, "EspDataRease():OWNER[%s] is no regist", "EspDataRease():OWNER[%x] is no regist");
        }
        return 0;
    }
    sys->ownerCnt[owner]--;
    if (flag == 1) {
        if (sys->ownerCnt[owner] != 0) {
            return 1;
        }
    } else {
        sys->ownerCnt[owner] = 0;
    }
    espTexRelease(owner);
    estRelease(owner);
    sstRelease(owner);
    pathRelease(owner);
    efmRelease(owner);
    return 1;
}

EspTexWk* EspGetTexWk(int id, int quiet)
{
    EspTexWk* w = &g_pEspSys->texWk[id];

    if (w->owner == 0xD2) {
        if (quiet == 0) {
            pLog->err(0, 0, "ESP : TexId[%x] no data", id);
        }
        return NULL;
    }
    return w;
}

void EspTexSet(int id, int ptn)
{
    EspTexWk* w = &g_pEspSys->texWk[id];

    if (w->owner == 0xD2) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", id);
        return;
    }
    GXLoadTexObj(&w->pTexObj[ptn], 0);
    if (w->texHdr->format - 8 <= 1) {
        GXLoadTlut(&w->tlut, 0);
    }
    GXLoadTexMtxImm(w->mtx, 0x1E, 1);
}

GXTexObj* EspGetTexObj(int id, int ptn)
{
    EspTexWk* w = &g_pEspSys->texWk[id];

    if (w->owner == 0xD2) {
        pLog->err(0, 0, "ESP : TEX_ID[%x] no data", id);
        return NULL;
    }
    return &w->pTexObj[ptn];
}

GXTlutObj* EspGetTlutObj(int id)
{
    EspTexWk* w = &g_pEspSys->texWk[id];

    if (w->owner == 0xD2) {
        pLog->err(0, 0, "ESP : TEX_ID[%x] no data", id);
        return NULL;
    }
    if (w->texHdr->format - 8 <= 1) {
        return &w->tlut;
    }
    return NULL;
}

int EspGetTplAddr(int id, void** out)
{
    EspTexWk* w = &g_pEspSys->texWk[id];

    if (w->owner == 0xD2) {
        return 0;
    }
    *out = w->pTpl;
    return 1;
}

int EspGetTexOwner(int id, u32* out)
{
    EspTexWk* w = &g_pEspSys->texWk[id];

    *out = w->owner;
    if (w->owner == 0xD2) {
        return 0;
    }
    return 1;
}

int EspGetAnmAddr(int id, EspAnmData** out)
{
    EspTexWk* w = &g_pEspSys->texWk[id];

    if (w->owner == 0xD2) {
        return 0;
    }
    *out = w->pAnm;
    return 1;
}

int EspChkTexId(int id)
{
    EspTexWk* w = &g_pEspSys->texWk[id];

    if (w->owner == 0xD2) {
        return 0;
    }
    return 1;
}

static void EspCalcTplAddr(TEXPalette* tpl)
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

int espTexRegist(TEXPalette* tpl, EspAnmData* anm, u8 id, u32 owner)
{
    cEspSystem* sys = g_pEspSys;
    EspTexWk* w = &sys->texWk[id];
    TEXDescriptor* desc;
    TEXHeader* hdr;
    GXTexObj* obj;
    u32 i;

    if (w->owner != 0xD2) {
        return 0;
    }
    if (anm->nPtn != tpl->numDescriptors) {
        pLog->err(0, 0, "ESP : ID[%02x] TEX/ANM ptn num diff[%d / %d]", id, tpl->numDescriptors, anm->nPtn);
        return 0;
    }
    EspCalcTplAddr(tpl);
    desc = TEXGet(tpl, 0);
    w->nTex = anm->nPtn;
    w->pTexObj = EspPullTexObj(w->nTex);
    if (w->pTexObj == NULL) {
        pLog->err(0, 0, "ESP : ID[%02x] PullTexObj() work full!!", id);
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
            GXInitTexObjCI(obj, hdr->data, hdr->width, hdr->height, hdr->format, 0, 0, 0, 0);
            GXInitTlutObj(&w->tlut, desc->CLUTHeader->data, desc->CLUTHeader->format, desc->CLUTHeader->numEntries);
            GXLoadTlut(&w->tlut, 0);
        } else {
            GXInitTexObj(obj, hdr->data, hdr->width, hdr->height, hdr->format, 0, 0, 0);
        }
    }
    PSMTXIdentity(w->mtx);
    return 1;
}

int estRegist(void* data, void* list, u32 owner)
{
    cEspSystem* sys = g_pEspSys;
    SstTbl* t;

    if (owner > 0xD2) {
        OWNER_ERR(owner, "estRegist():OWNER_ID[%s] Invalid.", "estRegist():OWNER_ID[%x] Invalid.");
        return 0;
    }
    t = &sys->estTbl[owner];
    if (t->owner != 0xD2) {
        OWNER_ERR(owner, "estRegist():OWNER_ID[%s] is already used.", "estRegist():OWNER_ID[%x] is already used.");
        return 0;
    }
    t->data = (SstData*) data;
    t->list = (SstList*) list;
    t->owner = owner;
    return 1;
}

EspSeqData* EspGetEstAddr(u32 owner, int id, int quiet)
{
    cEspSystem* sys = g_pEspSys;
    SstTbl* t;
    SstList* list;
    u32* ofs;
    int no;
    u32 i;

    if (owner > 0xD2) {
        if (quiet == 0) {
            OWNER_ERR2(owner, "EST_SET:[%s/0x%02x] OWNER Invalid", "EST_SET:[%x/0x%02x] OWNER Invalid", id);
        }
        return NULL;
    }
    t = &sys->estTbl[owner];
    if (t->owner == 0xD2) {
        if (quiet == 0) {
            OWNER_ERR2(owner, "EST_SET:[%s/0x%02x] OWNER not init", "EST_SET:[%x/0x%02x] OWNER not init", id);
        }
        return NULL;
    }
    list = t->list;
    no = -1;
    for (i = 0; i < list->num; i++) {
        if (list->ent[i].no == (u16) id) {
            no = i;
            break;
        }
    }
    if (no == -1) {
        if (quiet == 0) {
            OWNER_ERR2(owner, "EST_SET:[%s/0x%02x] EST_ID Invalid", "EST_SET:[%x/0x%02x] EST_ID Invalid", id);
        }
        return NULL;
    }
    if (pG->flags_68 & 0x80) {
        if (owner != 0 || id != 6) {
            OWNER_WARN2(owner, "EST[%s/0x%02x] called.", "EST[%x/0x%02x] called.", id);
        }
    }
    ofs = t->data->ofs;
    return (EspSeqData*) ((u8*) t->data + ofs[no]);
}

void* EspGetPathAddr(u32 owner, int id)
{
    cEspSystem* sys = g_pEspSys;
    SstTbl* t;
    SstList* list;
    u32* ofs;
    int no;
    u32 i;

    if (owner > 0xD2) {
        OWNER_ERR2(owner, "ESP_PATH : OWNER_ID[%s/0x%02x] Invalid.", "ESP_PATH : OWNER_ID[%x/0x%02x] Invalid.", id);
        return NULL;
    }
    t = &sys->pathTbl[owner];
    if (t->owner == 0xD2) {
        OWNER_ERR2(owner, "ESP_PATH : OWNER_ID[%s/0x%02x] Not init.", "ESP_PATH : OWNER_ID[%x/0x%02x] Not init.",
                   id);
        return NULL;
    }
    list = t->list;
    no = -1;
    for (i = 0; i < list->num; i++) {
        if (list->ent[i].no == (u16) id) {
            no = i;
            break;
        }
    }
    if (no == -1) {
        pLog->err(0, 0, "ESP_PATH : PATH_ID[0x%x] Invalid.", id);
        return NULL;
    }
    ofs = t->data->ofs;
    return (u8*) t->data + ofs[no];
}

int sstRegist(void* data, void* list, u32 owner)
{
    cEspSystem* sys = g_pEspSys;
    SstTbl* t;

    if (owner > 0xD2) {
        OWNER_ERR(owner, "sstRegist():OWNER_ID[%s] Invalid", "sstRegist():OWNER_ID[%x] Invalid");
        return 0;
    }
    t = &sys->sstTbl[owner];
    if (t->owner != 0xD2) {
        OWNER_ERR(owner, "sstRegist():OWNER[%s] is already used.", "sstRegist():OWNER[%x] is already used.");
        return 0;
    }
    t->data = (SstData*) data;
    t->list = (SstList*) list;
    t->owner = owner;
    return 1;
}

int pathRegist(void* data, void* list, u32 owner)
{
    cEspSystem* sys = g_pEspSys;
    SstTbl* t;

    if (owner > 0xD2) {
        OWNER_ERR(owner, "pathRegist():OWNER_ID[%s] Invalid", "pathRegist():OWNER_ID[%x] Invalid");
        return 0;
    }
    t = &sys->pathTbl[owner];
    if (t->owner != 0xD2) {
        OWNER_ERR(owner, "pathRegist():OWNER[%s] is already used.", "pathRegist():OWNER[%x] is already used.");
        return 0;
    }
    t->data = (SstData*) data;
    t->list = (SstList*) list;
    t->owner = owner;
    return 1;
}

int efmRegist(void* model, void* tpl, void* mot, void* x, u8 id, u32 owner)
{
    cEspSystem* sys = g_pEspSys;
    EspEfmWk* w;

    if (owner > 0xD2) {
        OWNER_ERR(owner, "efmRegist():OWNER_ID[%s] Invalid", "efmRegist():OWNER_ID[%x] Invalid");
        return 0;
    }
    w = &sys->efmWk[id];
    if (w->owner != 0xD2) {
        if (pG->flags_6C & 0x8000) {
            if (owner <= 0xD0) {
                pLog->warn(0, 0, "efmRegist(): ID[0x%x]:OWNER[%s] already used[%s].", id, owner_name_tbl[owner],
                           owner_name_tbl[w->owner]);
            } else {
                pLog->warn(0, 0, "efmRegist(): ID[0x%x]:OWNER[%x] already used[%x].", id, owner, w->owner);
            }
        }
        return 0;
    }
    w->model = model;
    w->tpl = tpl;
    w->mot = (EspEfmMotTbl*) mot;
    w->x0C = x;
    w->owner = owner;
    return 1;
}

int espTexRelease(u32 owner)
{
    cEspSystem* sys = g_pEspSys;
    EspTexWk* w = sys->texWk;
    u32 i;
    u32 j;
    u32 start;

    for (i = 0; i < 0x100; i++, w++) {
        if (w->owner == owner) {
            j = w->pTexObj - sys->texObj;
            start = j;
            w->owner = 0xD2;
            for (; j < start + w->nTex; j++) {
                sys->SetTexObjFlag(j, 0);
            }
        }
    }
    return 1;
}

int estRelease(u32 owner)
{
    SstTbl* t = g_pEspSys->estTbl;
    int i;

    for (i = 0; i < EFF_OWNER_MAX; i++) {
        if (t->owner == owner) {
            t->owner = 0xD2;
        }
        t++;
    }
    return 1;
}

int sstRelease(u32 owner)
{
    SstTbl* t = g_pEspSys->sstTbl;
    int i;

    for (i = 0; i < EFF_OWNER_MAX; i++) {
        if (t->owner == owner) {
            t->owner = 0xD2;
        }
        t++;
    }
    return 1;
}

int pathRelease(u32 owner)
{
    SstTbl* t = g_pEspSys->pathTbl;
    int i;

    for (i = 0; i < EFF_OWNER_MAX; i++) {
        if (t->owner == owner) {
            t->owner = 0xD2;
        }
        t++;
    }
    return 1;
}

int efmRelease(u32 owner)
{
    EspEfmWk* w = g_pEspSys->efmWk;
    int i;

    for (i = 0; i < 0x100; i++) {
        if (w->owner == owner) {
            w->owner = 0xD2;
        }
        w++;
    }
    return 1;
}

int EspGetEfmAddr(int id, void** model, void** tpl)
{
    EspEfmWk* w = &g_pEspSys->efmWk[id];

    if (w->owner == 0xD2) {
        return 0;
    }
    *model = w->model;
    *tpl = w->tpl;
    return 1;
}

int EspGetEfmTplAddr(int id, void** tpl)
{
    EspEfmWk* w = &g_pEspSys->efmWk[id];

    if (w->owner == 0xD2) {
        return 0;
    }
    *tpl = w->tpl;
    return 1;
}

int EspGetEfmMotAddr(int id, u32 no, void** out)
{
    EspEfmWk* w = &g_pEspSys->efmWk[id];
    EspEfmMotTbl* mot;
    u32* ofs;

    if (w->owner == 0xD2) {
        return 0;
    }
    mot = w->mot;
    if (mot == NULL) {
        return 0;
    }
    if (no >= mot->num) {
        return 0;
    }
    ofs = mot->ofs;
    ofs += no;
    *out = (u8*) mot + *ofs;
    return 1;
}

u8 EspPullCoreKind()
{
    cEspSystem* sys = g_pEspSys;
    u8 kind = sys->coreKind;

    if (kind == 0xFF) {
        pLog->err(0, 0, "EspPullCoreKind() :  STACK OVER FLOW.");
        return 0;
    }
    sys->coreKind++;
    return kind;
}

int cEspSystem::GetTexObjFlag(u32 no)
{
    if (no >= EFF_TEXOBJ_MAX) {
        pLog->err(0, 0, "cEspSystem::GetTexObjFlag : TexNo over [%d/%d]", no, EFF_TEXOBJ_MAX);
        return 0;
    }
    if ((texObjFlag[no >> 3] >> (no & 7)) & 1) {
        return 1;
    }
    return 0;
}

void cEspSystem::SetTexObjFlag(u32 no, int flag)
{
    u8 bit;

    if (no >= EFF_TEXOBJ_MAX) {
        pLog->err(0, 0, "cEspSystem::GetTexObjFlag : TexNo over [%d/%d]", no, EFF_TEXOBJ_MAX);
    }
    bit = 1 << (no & 7);
    if (flag == 1) {
        texObjFlag[no >> 3] |= bit;
    } else {
        texObjFlag[no >> 3] &= ~bit;
    }
}

int EspGenGetMoveLoop()
{
    return g_nLoop;
}

void EspGenSetMoveLoop(int loop)
{
    g_nLoop = loop;
}

void EspGenLoopMove()
{
    u32 i;

    if (g_nLoop > 0x400) {
        g_nLoop = 0x400;
    }
    for (i = 0; i < g_nLoop; i++) {
        EspgenMove();
        EspMove();
        LightMgr.move();
    }
    g_nLoop = 0;
}

int EffIsSetFinalCol()
{
    return g_pEspSys->finalColSet;
}

void EffGetFinalCol(GXColor* col)
{
    cEspSystem* sys = g_pEspSys;

    col->r = sys->finalCol.r;
    col->g = sys->finalCol.g;
    col->b = sys->finalCol.b;
    col->a = sys->finalCol.a;
}

void EffSetFinalCol(u8 r, u8 g, u8 b, u8 a)
{
    cEspSystem* sys = g_pEspSys;

    sys->finalCol.r = r;
    sys->finalCol.g = g;
    sys->finalCol.b = b;
    sys->finalCol.a = a;
    if (*(u32*) &sys->finalCol == 0xFFFFFFFF) {
        sys->finalColSet = 1;
    } else {
        sys->finalColSet = 0;
    }
}

int GetAreaState(int no)
{
    if (g_pEspSys->areaState & (1 << no)) {
        return 1;
    }
    return 0;
}

void SetAreaState(int no, int on)
{
    u32 bit = 1 << no;
    cEspSystem* sys = g_pEspSys;

    if (on == 1) {
        sys->areaState |= bit;
    } else {
        sys->areaState &= ~bit;
    }
}

int EffGetAreaState(int no)
{
    return GetAreaState(no);
}

void EffSetAreaState(int no, int on)
{
    if (GetAreaState(no) == on) {
        return;
    }
    if (on != 0) {
        SstSet(1, (u16) no, no + 0xC, 0, 0x2F, 0);
    } else {
        u8 kind = no + 0xC;
        EffectEspDelete(0, kind, 0, NULL);
        EffectEspgenDelete(0, kind, 0);
        EffectEfmDelete(0, kind, 0);
    }
    SetAreaState(no, on);
}

void EffSetToolState(int state)
{
    g_pEspSys->toolState |= state;
}

u8 EffGetToolState()
{
    return g_pEspSys->toolState;
}

void EffClearToolState()
{
    g_pEspSys->toolState = 0;
}

extern "C" void EffSetToolStateCallBack(int no, void (*on)(), void (*off)())
{
    g_pEspSys->toolCb[no] = on;
    g_pEspSys->toolCb2[no] = off;
}

void EffCallToolStateCallBack()
{
    cEspSystem* sys = g_pEspSys;

    if (EffGetToolState() & 3) {
        if (sys->toolCb[0] != NULL) {
            sys->toolCb[0]();
        }
    } else {
        if (sys->toolCb2[0] != NULL) {
            sys->toolCb2[0]();
        }
    }
}

void RoomEfmRegist(cModel* m, u8 id)
{
    efmRegist(m->pInfo->pData, m->pInfo->pTpl, NULL, NULL, id, 1);
}

void RoomEfmRegist(void* model, void* tpl, u8 id)
{
    efmRegist(model, tpl, NULL, NULL, id, 1);
}

asm(".section .sdata; .balign 8");
