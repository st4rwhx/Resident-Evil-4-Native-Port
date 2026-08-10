#include "types.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "card.h"
#include "event.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "scheduler.h"
#include "libgpu.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "sce.h"
#include "em.h"
#include "em_set.h"
#include "emitem.h"
#include "emhit.h"
#include "obj.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "item.h"
#include "examine.h"
#include "mes.h"
#include "snd.h"
#include "pad.h"
#include "est.h"
#include "esp.h"
#include "shadow.h"
#include "room_data.h"
#include "etc_model.h"
#include "route_ck.h"
#include "sscrn.h"
#include "puzzle.h"
#include "dvd.h"
#include "act_btn.h"
#include "geometry.h"
#include "math_sub.h"
#include "eprintf.h"
#include "db_log.h"

// Scenario trigger areas: the room's AEV (areas) / ITA (items) records plus the areas created at
// run time, checked against the player, the partner and the enemies every frame.

extern "C" {
int strcmp(const char* a, const char* b);
int sprintf(char* dst, const char* fmt, ...);
void* memset(void* dst, int c, unsigned int n);
void OSReport(const char* fmt, ...);
int SubCharHideCheck();                                  // game/pl_npc.cpp
int getRoomEtcBreak(int no, cEm** em, int a);            // game/EtcModel.cpp
int RandomItemCk(int a, int* id, int* num, int b);       // game/em_sub.cpp
int ItemGetBinTplAddr(u8 id, void** bin, void** tpl);    // game/item_model.cpp
void* EmReadSearch(int id, void* addr, u32 size);        // game/read.cpp
}
u32 SubCharGetStatus();                                  // game/pl_npc.cpp
int DbMenuActiveCheck();                                 // game/db_menu.cpp
cObj* setItemObj(void* bin, void* tpl, Vec* pos, Vec* rot);  // game/obj19.cpp
cEm* EmSetEvent(EmListData* d);                          // game/em_set.cpp

#define MTX_COPY(src, dst)               \
    {                                    \
        MtxPtr d_;                       \
        MtxPtr s_ = (src);               \
        int i_ = 3;                      \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        d_ = (dst);                      \
        while (i_--) {                   \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        }                                \
    }

// cEm::setItem seen with an int 5th argument (the caller sign-extends the item colour byte).
class cEmSetItemView : public cModel {
public:
    virtual void setItem(u16 a, u16 b, u16 c, u16 d, int e);
};
#define EM_SET_ITEM(em, a, b, c, d, e) ((cEmSetItemView*) (em))->setItem(a, b, c, d, e)

// Bit test evaluated as a value (the original's `xori; andi.` shape).
static inline int bitOff(u32 v)
{
    return !(v & 1);
}

static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void U32Set(u32& d, u32 v) { d = v; }
static inline void S16Set(s16& d, s16 v) { d = v; }
static inline void S8Set(s8& d, s8 v) { d = v; }
static inline void PSet(void*& d, void* v) { d = v; }
static inline void PSet(u32& d, void* v) { d = (u32) v; }
static inline void PSet(cModel*& d, cModel* v) { d = v; }

static inline u32* eventFlags()
{
    return &pG->flags_174;
}
static inline u32* flags51BC()
{
    return &pG->flags_51BC;
}
static inline u32* doorUnlock()
{
    return pG->door_unlock;
}
static inline u32* itemFlags()
{
    return pG->item_flags;
}
static inline u32* itemFindFlags()
{
    return &pG->item_flags[4];
}
// pG->save_item as a pointer (the original adds the record offset to pG before the index).
static inline SceAtSaveItem* saveItemTbl()
{
    return pG->save_item;
}
// Halfword fields of the save items: the original forms the address as integer arithmetic with the index
// first (`idx*16 + ((u32)pG + ofs)`): non-struct MEM with an unflagged base, so pG is reloaded after
// every store and the field offset is added to pG before the index (sthx base, idx).
static inline u32 saveItemBase(int ofs)
{
    return (u32) pG + ofs;
}
// Non-struct store at a constant offset from a struct pointer (aliases every following global load, so
// the original's `lwz pPL` stays behind the store).
#define RAW_F32(p, ofs) (*(f32*) ((u32) (p) + (ofs)))
#define RAW_U32(p, ofs) (*(u32*) ((u32) (p) + (ofs)))

// em_dead row address as an integer (the original adds the list offset after the row index).
static inline u32 emDeadRow(int n)
{
    return n * 32 + (u32) pG + 0x501C;
}
#define EM_DEAD_BIT(n, i) (*(u32*) (((i) << 2) + emDeadRow(n)))
#define SAVE_ITEM_HALF(i, ofs) (*(u16*) (saveItemBase(ofs) + ((i) << 4)))
#define SAVE_ITEM_ROOM(i) SAVE_ITEM_HALF(i, 0x72EC)
#define SAVE_ITEM_ID(i) SAVE_ITEM_HALF(i, 0x72EE)
#define SAVE_ITEM_NUM(i) SAVE_ITEM_HALF(i, 0x72F0)
#define SAVE_ITEM_POS(i, k) (*(s16*) (saveItemBase(0x72F2 + (k) * 2) + ((i) << 4)))
#define SAVE_ITEM_TYPE(i) pG->save_item[i].type
#define SAVE_ITEM_ATNO(i) pG->save_item[i].atNo
#define SAVE_ITEM_EFF(i) pG->save_item[i].effType

// Room save record words: item flags at +8, item-found flags at +0x18 (separate pointer pseudos keep the addi).
static inline u32* roomItemFlags()
{
    return (u32*) (RoomData.getRoomSavePtr(pG->room_id) + 8);
}
static inline u32* roomItemFindFlags()
{
    return (u32*) (RoomData.getRoomSavePtr(pG->room_id) + 0x18);
}

#define MES_Y (0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1)

// One entry of the scenario area system (`SceAtSys`, 0x124 bytes).
struct SceAtReserve {
    u32 key;          // 0x00
    u8 saveNo;        // 0x04
    u8 pad_5[3];
};

struct SceAtSysWork {
    void* pAtData;             // 0x00   AEV file
    u32 pAtWork;               // 0x04   its records (+0x10), kept as an address
    void* pItemData;           // 0x08   ITA file
    u32 pItemWork;             // 0x0C
    u32 hitFlg[8];             // 0x10   areas hit this frame
    u32 execFlg[8];            // 0x30   areas executed this frame
    u32 ot[16];                // 0x50   ordering table, ot[15] is the list head
    u32 stop;                  // 0x90   pG->flags_170 saved by the semi-auto stop (bit31 = pending)
    u32 x94;                   // 0x94   pG->flags_170 saved by the door / skey tasks
    u8 hideActive;             // 0x98   a hide area is running
    u8 pad_99[3];
    SceAtReserve reserve[16];  // 0x9C
    u8 x11C;
    u8 x11D;
    u8 pad_11E[2];
    SceAtCamCtrl* pCamAt;      // 0x120  camera control area in effect
};

struct SceAtReleaseModel {
    s16 cnt;          // 0x00
    u8 pad_2[2];
    void* bin;        // 0x04
    void* tpl;        // 0x08
};

// Data file headers.
struct SceAtFileHead {
    char magic[4];    // 0x00  "AEV" / "ITA"
    u16 version;      // 0x04
    u16 num;          // 0x06
    u8 pad_8[8];
    SceAtWork work[1];  // 0x10
};

// AreaViewCheck's cone scratch: the original frame reserves 0x40 bytes for it (frame 0xC8 with the
// three other locals), not the 0x48 of `GeoCone[2]` (GeoCone was probably 0x20 without `radius` then).
struct SceAtViewCone {
    f32 w[16];
};

struct SceAtFuncTbl {
    int (*func)(SceAtWork* w, cModel* m);
    u32 exclusive;    // 1 = only one such area fires per check
};

static SceAtSysWork* pS;
static SceAtSysWork SceAtSys;
// Struct-member view of pS (the pLog trick): keeps its load below preceding stores through a work.
struct SceAtSysPtr {
    SceAtSysWork* p;
};
#define pSS (((SceAtSysPtr*) &pS)->p)
static SceAtReleaseModel releaseModelTbl[8];
static cModel* p_imodel_bak = NULL;
static void* lbl_80314D6C = NULL;

extern "C" {
static int sceAtFunc_normal(SceAtWork* w, cModel* m);
static int sceAtFunc_door(SceAtWork* w, cModel* m);
static int sceAtFunc_exec(SceAtWork* w, cModel* m);
static int sceAtFunc_item(SceAtWork* w, cModel* m);
static int sceAtFunc_flg(SceAtWork* w, cModel* m);
static int sceAtFunc_mes(SceAtWork* w, cModel* m);
static int sceAtFunc_save(SceAtWork* w, cModel* m);
static int sceAtFunc_shd_disp(SceAtWork* w, cModel* m);
static int sceAtFunc_damage(SceAtWork* w, cModel* m);
static int sceAtFunc_scr_at(SceAtWork* w, cModel* m);
static int sceAtFunc_field_info(SceAtWork* w, cModel* m);
static int sceAtFunc_stoop(SceAtWork* w, cModel* m);
static int sceAtFunc_skey(SceAtWork* w, cModel* m);
static int sceAtFunc_ladder(SceAtWork* w, cModel* m);
static int sceAtFunc_use(SceAtWork* w, cModel* m);
static int sceAtFunc_hide(SceAtWork* w, cModel* m);
static int sceAtFunc_pos_jump(SceAtWork* w, cModel* m);
static void sceInLock(SceAtWork* w);
static void sceAtSkey(SceAtWork* w);
static void sceAtGetItem(SceAtWork* w);
static void sceAtGetItem_NoModel(SceAtWork* w);
static void sceAtDeleteItem(SceAtWork* w);
static void initReleaseModelTbl();
static void setReleaseModelTbl(void* bin, void* tpl);
static void checkReleaseModelTbl();
static void sceAtCamCtrlCheck();
static void sceAtDebugDisp();
static void sceAtItemFindCheck();
static void sceAtDataLoopInit();
}

static SceAtFuncTbl sceAtFunc_tbl[21] = {
    {sceAtFunc_normal, 0},      // 0x00
    {sceAtFunc_door, 1},        // 0x01
    {sceAtFunc_exec, 0},        // 0x02
    {sceAtFunc_item, 1},        // 0x03
    {sceAtFunc_flg, 0},         // 0x04
    {sceAtFunc_mes, 1},         // 0x05
    {sceAtFunc_normal, 1},      // 0x06
    {sceAtFunc_normal, 0},      // 0x07
    {sceAtFunc_save, 1},        // 0x08
    {sceAtFunc_shd_disp, 0},    // 0x09
    {sceAtFunc_damage, 0},      // 0x0A
    {sceAtFunc_scr_at, 0},      // 0x0B
    {sceAtFunc_normal, 0},      // 0x0C  camera control
    {sceAtFunc_field_info, 0},  // 0x0D
    {sceAtFunc_stoop, 0},       // 0x0E
    {sceAtFunc_skey, 1},        // 0x0F
    {sceAtFunc_ladder, 1},      // 0x10
    {sceAtFunc_use, 0},         // 0x11
    {sceAtFunc_hide, 0},        // 0x12
    {sceAtFunc_pos_jump, 0},    // 0x13
    {sceAtFunc_normal, 0},      // 0x14  item parent
};

void SceAtInit(void* atData, void* itemData)
{
    int i;

    pS = &SceAtSys;
    pS->pAtData = 0;
    pS->pAtWork = 0;
    pS->pCamAt = 0;
    pS->hideActive = 0;
    ClearOTagR(pS->ot, 16);
    initReleaseModelTbl();
    for (i = 0; i < 16; i++) {
        SceAtSys.reserve[i].key = 0;
        SceAtSys.reserve[i].saveNo = 0;
    }
    SceAtWorkLoopInit();
    U8Set(pS->x11C, 0);
    U8Set(pS->x11D, 0);
    if (atData != 0) {
        if (strcmp((char*) atData, "AEV") != 0) {
            pLog->err(0, 0, "THIS DATA IS NOT SCENARIO ATARI DATA");
        } else if (((SceAtFileHead*) atData)->version != 0x104) {
            pLog->err(0, 0, "SceAt DATA IS OLD VERSION");
        } else {
            PSet(pS->pAtData, atData);
            PSet(pS->pAtWork, (u8*) atData + 0x10);
            for (i = ((SceAtFileHead*) pS->pAtData)->num - 1; i >= 0; i--) {
                SceAtWork* w = (SceAtWork*) (i * sizeof(SceAtWork) + pS->pAtWork);

                AddPrim(&pS->ot[w->x44], (u32*) w);
            }
        }
    }
    if (itemData != 0) {
        if (strcmp((char*) itemData, "ITA") != 0) {
            pLog->err(0, 0, "THIS DATA IS NOT ITEM SET DATA");
        } else if (((SceAtFileHead*) itemData)->version != 0x105) {
            pLog->err(0, 0, "SceItem DATA IS OLD VERSION");
        } else {
            PSet(pS->pItemData, itemData);
            PSet(pS->pItemWork, (u8*) itemData + 0x10);
            for (i = ((SceAtFileHead*) pS->pItemData)->num - 1; i >= 0; i--) {
                SceAtWork* w;

                ((SceAtWork*) (i * sizeof(SceAtWork) + pS->pItemWork))->no += 0x80;
                w = (SceAtWork*) (i * sizeof(SceAtWork) + pS->pItemWork);
                AddPrim(&pS->ot[w->x44], (u32*) w);
            }
        }
    }
}

SceAtWork* sceAtSetOtStart()
{
    return (SceAtWork*) &pS->ot[15];
}

SceAtWork* sceAtGetOtAddr(SceAtWork* p)
{
    u32 v;

    while ((v = p->next) != 0xFFFFFFFF) {
        p = (SceAtWork*) (v | 0x80000000);
        if ((s32) v < 0) {
            return p;
        }
    }
    return 0;
}

void SceAtClearHitFlg()
{
    memclr_asm(pS->hitFlg, sizeof(pS->hitFlg));
}

void SceAtSetHitFlg(u32 no)
{
    u32* f = pS->hitFlg;

    f[no >> 5] |= 0x80000000 >> (no & 31);
}

void SceAtClearExecFlg()
{
    memclr_asm(pS->execFlg, sizeof(pS->execFlg));
}

void SceAtSetExecFlg(u32 no)
{
    u32* f = pS->execFlg;

    f[no >> 5] |= 0x80000000 >> (no & 31);
}

void SceAtWorkLoopInit()
{
    U32Set(pG->sceat_x17C, 0);
    U32Set(pG->sceat_x180, 0);
    SceAtClearHitFlg();
    SceAtClearExecFlg();
}

static void sceAtDataLoopInit()
{
    SceAtWork* w = sceAtSetOtStart();

    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->flag)) {
            continue;
        }
        if (w->x35 == 0) {
            memclr_asm(w->data, sizeof(w->data));
        }
    }
}

void SceAtCheck()
{
    u32 i;
    cEm* em;

    if (SceAtCheckHideActive() == 1) {
        if (!(pG->flags_500C & 0x00100000)) {
            SceAtCheckHideProc();
        }
    }
    sceAtLink_check();
    if (pG->flags_170 & 0x00400000) {
        return;
    }
    checkReleaseModelTbl();
    SceAtWorkLoopInit();
    if (pG->flags_68 & 0x04000000) {
        return;
    }
    sceAtDebugDisp();
    if ((s32) pG->flags_60 < 0) {
        BitOff(pG->flags_500C, 0x40000000);
        BitOff(pG->flags_500C, 0x20000000);
        return;
    }
    ItemMgr.flagclear();
    sceAtDataLoopInit();
    sceAtItemFindCheck();
    sceAtCamCtrlCheck();
    if ((s32) pS->stop < 0) {
        if (pG->flags_500C & 0x40000000) {
            pS->stop &= 0x7FFFFFFF;
        } else {
            pG->flags_500C &= ~0x20000000;
        }
    }
    sceAtCheck_main(pPL, 1);
    for (i = 0; i < EmMgr.nArray; i++) {
        em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if (pSUB != 0 && pSUB == em) {
            sceAtCheck_main(em, 8);
            continue;
        }
        switch (em->id) {
        case 0x00:
        case 0x21:
        case 0x23:
        case 0x24:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2E:
        case 0x3B:
            continue;
        case 0x45:
            break;
        default:
            if (em->id > 0x3F) {
                continue;
            }
            break;
        }
        if (EmMoveActiveCheck(em) != 0) {
            sceAtCheck_main(em, 2);
        }
    }
    BitOff(pG->flags_500C, 0x40000000);
    BitOff(pG->flags_500C, 0x20000000);
}

int sceAtCheck_main(cEm* em, int type)
{
    Vec pos;
    Vec front;
    char name[21] = {'N', 'D', 'E', 'I', 'F', 'M', 'P', 'J', 'T', 'S', 'd', 's', ' ', 'f', 'C', 'K', 'L', 'U', 'H', ' ', ' '};
    ItemInfo info;
    SceAtWork* w;
    int col = 0;
    int cnt = 0;
    int flag = 1;
    int hit;
    u32 ft;
    u8 t;
    int c;
    int kind;

    em->litArea.x0 &= ~1;
    pos = em->pos;
    pos.y += 250.0f;
    front.x = 0.0f;
    front.y = 250.0f;
    front.z = 550.0f;
    PSMTXMultVec(em->mat, &front, &front);
    if (type & 1) {
        if (pG->flags_500C & 0x40000000) {
            flag = 3;
        }
        if (pG->flags_500C & 0x20000000) {
            flag |= 4;
        }
        SatMgr.hitCheck(&pos, &front, &front, 0, 0, 0);
    }
    hit = 0;
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->flag)) {
            continue;
        }
        if (!(w->x39 & type)) {
            continue;
        }
        if (sceAtHitCheck(w, em, &front, &pos) == 0) {
            if (w->x35 == 9) {
                sceAtFunc_shd_disp_reverse(w);
            }
            continue;
        }
        SceAtSetHitFlg(w->no);
        if (w->x38 & 1) {
            col = 6;
        } else if (w->x38 & 2) {
            col = 4;
        } else if (w->x38 & 4) {
            col = 0;
        }
        eprintf2(8, 0x10, cnt * 8 + 0x168, 8, col, 0, "%c", name[w->x35]);
        cnt++;
        cnt &= 7;
        t = w->x35;
        if (t == 2 && w->func == 0) {
            continue;
        }
        ft = 2;
        if (w->func == 0) {
            ft = t;
        }
        if (w->x38 & 8) {
            c = 0;
            kind = w->x4A;
            if (w->x53 != 0) {
                c = (w->x53 == 1) << 7;
            }
            if (t == 1) {
                c |= 0x80;
            }
            switch (ft) {
            case 1:
                c |= 0x80;
                break;
            case 0xE:
                if (PlGetStatus() & 0x8000) {
                    continue;
                }
                break;
            case 0x12:
                if (SceAtCheckHideActive() != 0) {
                    continue;
                }
                if (SubCharHideCheck() != 1) {
                    continue;
                }
                if (RouteCkPosToPosDis(&pPL->pos, &pSUB->pos) < 5000.0f) {
                    ActBtn.set(kind, w->x44, (int) sceAtFunc_tbl[0x12].func, (int) w, 1, 6, 2, (int) em);
                }
                continue;
            case 3:
                if (pG->x4 != 0) {
                    itemInfo(w->item.id, &info);
                    if (info.type != 7) {
                        continue;
                    }
                }
                break;
            }
            ActBtn.set(kind, w->x44, (int) sceAtFunc_tbl[ft].func, (int) w, c, 1, 2, (int) em);
            continue;
        }
        if (!(t == 1 && w->func == 0 && (w->x38 & 2) && (flag & 4))) {
            if (!(w->x38 & flag)) {
                continue;
            }
        }
        if (ft > 0x14) {
            continue;
        }
        if (hit != 0 && sceAtFunc_tbl[ft].exclusive != 0) {
            continue;
        }
        SceAtSetExecFlg(w->no);
        if (sceAtFunc_tbl[ft].func(w, em) == 1) {
            hit = 1;
        }
        if (w->x38 & 0x80) {
            if (ft == 2 && w->func == 0) {
                continue;
            }
            SceAtSetEnable(w->no, 0);
        }
    }
    return hit;
}

// The area of `w` in world space: its parent's matrix applied (rotation ignored with flag bit3).
void sceAtGetArea(AreaData* out, SceAtWork* w)
{
    Mtx mat;
    Mtx pmat;

    *out = w->area;
    if (w->pParent == 0) {
        return;
    }
    if (w->parentParts >= 0) {
        MTX_COPY(w->pParent->getPartsPtr(w->parentParts)->mat, pmat);
    } else {
        MTX_COPY(w->pParent->mat, pmat);
    }
    if (w->flag & 8) {
        Vec zero = { 0.0f, 0.0f, 0.0f };
        low_RotMatrix(mat, &zero);
        mat[0][3] = pmat[0][3];
        mat[1][3] = pmat[1][3];
        mat[2][3] = pmat[2][3];
    } else {
        MTX_COPY(pmat, mat);
    }
    Vec p[4];
    switch (out->type) {
    case 1:
        p[0].y = p[1].y = p[2].y = p[3].y = out->u.xz4.y;
        p[0].x = out->u.xz4.p[0].x;
        p[0].z = out->u.xz4.p[0].z;
        p[1].x = out->u.xz4.p[1].x;
        p[1].z = out->u.xz4.p[1].z;
        p[2].x = out->u.xz4.p[2].x;
        p[2].z = out->u.xz4.p[2].z;
        p[3].x = out->u.xz4.p[3].x;
        p[3].z = out->u.xz4.p[3].z;
        PSMTXMultVec(mat, &p[0], &p[0]);
        PSMTXMultVec(mat, &p[1], &p[1]);
        PSMTXMultVec(mat, &p[2], &p[2]);
        PSMTXMultVec(mat, &p[3], &p[3]);
        out->u.xz4.y = p[0].y;
        out->u.xz4.p[0].x = p[0].x;
        out->u.xz4.p[0].z = p[0].z;
        out->u.xz4.p[1].x = p[1].x;
        out->u.xz4.p[1].z = p[1].z;
        out->u.xz4.p[2].x = p[2].x;
        out->u.xz4.p[2].z = p[2].z;
        out->u.xz4.p[3].x = p[3].x;
        out->u.xz4.p[3].z = p[3].z;
        break;
    case 2:
    case 3:
        p[0].x = out->u.cyl.x;
        p[0].y = out->u.cyl.y;
        p[0].z = out->u.cyl.z;
        PSMTXMultVec(mat, &p[0], &p[0]);
        out->u.cyl.x = p[0].x;
        out->u.cyl.y = p[0].y;
        out->u.cyl.z = p[0].z;
        break;
    }
}

int sceAtHitCheck(SceAtWork* w, cModel* m, Vec* front, Vec* pos)
{
    AreaData area;
    f32 ang;
    int ret;
    f32 ry;

    sceAtGetArea(&area, w);
    ang = (f32) (w->angle * 2) * (PI / 180.0f);
    if (w->pParent != 0) {
        // `rot.y` read in both arms: the cross-jumped `lfs` lands ahead of the flag test.
        if (w->parentParts >= 0) {
            ry = w->pParent->getPartsPtr(w->parentParts)->rot.y;
        } else {
            ry = w->pParent->rot.y;
        }
        if (!(w->flag & 8)) {
            ang = LIMIT_ANGLE(ang + ry);
        }
    }
    ret = 0;
    if (area.type == 3) {
        Vec cc;
        Vec c = {0.0f, 0.0f, 0.0f};
        SceAtViewCone cone;

        c.x = area.u.eye.x;
        c.y = area.u.eye.y;
        c.z = area.u.eye.z;
        cc = c;
        if (AreaViewCheck(&area, (GeoCone*) &cone) == 1) {
            ret = InScreenCheck(&cc) == 1;
        }
    } else {
        // Both arms written out (cross-jumped `AreaHitCheck` tail, the null test stays per arm).
        if (w->x37 & 1) {
            if (front != 0 && AreaHitCheck(&area, front) == 1) {
                ret = 1;
            }
        } else {
            if (pos != 0 && AreaHitCheck(&area, pos) == 1) {
                ret = 1;
            }
        }
        if (m != 0 && (w->x37 & 2) && ret == 1) {
            f32 d = LIMIT_ANGLE(ang - m->rot.y);
            int r = w->angleRange;

            if (d < (f32) (-r * 2) * (PI / 180.0f) || d > (f32) (r * 2) * (PI / 180.0f)) {
                ret = 0;
            }
        }
        if (w->x35 == 3 && ret == 1) {
            if (SceAtItemHitCheck(w, 0) == 0) {
                ret = 0;
            }
        }
    }
    return ret;
}

static int sceAtFunc_normal(SceAtWork* w, cModel* m)
{
    u32 i;

    for (i = 0; i < 16; i++) {
        if (w->hitModel[i] == 0) {
            w->hitModel[i] = m;
            break;
        }
    }
    return 0;
}

static void sceInLock(SceAtWork* w)
{
    switch (w->lockType) {
    case 1:
        SndCall(6, (s8) w->doorSe, &pPL->pos, 0, 0, 0);
        SceMesSet(0xA, 0x11, 1, 0x64, MES_Y);
        while (cMes.mes[0].flags2 & 1) {
            TaskSleep(1);
        }
        break;
    case 2:
        SndCall(6, (s8) w->doorSe, &pPL->pos, 0, 0, 0);
        SceMesSet(0xB, 0x11, 1, 0x64, MES_Y);
        while (cMes.mes[0].flags2 & 1) {
            TaskSleep(1);
        }
        doorUnlock()[w->lockFlag >> 5] |= 0x80000000 >> (w->lockFlag & 31);
        break;
    }
    SceAtStopSemiautoCheck();
    pG->flags_170 = pS->x94;
    w->flag |= 1;
    TaskExit();
}

int CheckAshleyActive()
{
    if (pSUB == 0) {
        return 0;
    }
    if (RouteCkPosToPosDis(&pPL->pos, &pSUB->pos) > 5000.0f || SceAtCheckHideActive() == 1 || (pG->flags_5014 & 0x20000000) ||
        (SubCharGetStatus() & 0x02000000)) {
        return 0;
    }
    return 1;
}

int CheckDoorJumpWithAshley()
{
    if (pSUB != 0 && !(pG->flags_51BC & 0x80)) {
        if (CheckAshleyActive() == 0) {
            return 0;
        }
    }
    return 1;
}

static int sceAtFunc_door(SceAtWork* w, cModel* m)
{
    u8 lt;

    if (DbMenuActiveCheck() == 1) {
        return 0;
    }
    if (CheckDoorJumpWithAshley() == 0) {
        cMes.MesSet(0x67, 0x64, MES_Y, 1, 0, 0, 4);
        return 1;
    }
    pS->x94 = pG->flags_170;
    KeyStop(0xEFCF0000);
    BitSet(pG->flags_170, -1);
    lt = w->lockType;
    if (lt != 0 && !(doorUnlock()[w->lockFlag >> 5] & (0x80000000 >> (w->lockFlag & 31)))) {
        switch (lt) {
        case 1:
        case 2:
            TaskExec(1, (TaskFunc) sceInLock, (int) w);
            w->flag &= ~1;
            return 1;
        }
    }
    if (w->doorFunc != 0) {
        SceSys.x10 = (int) w->doorFunc;
        SceSys.x14 = w->doorArg;
        w->doorFunc = 0;
    }
    SceSys.x75 = w->x77;
    FSet(pG->next_pos.x, w->dstPos.x);
    FSet(pG->next_pos.y, w->dstPos.y);
    FSet(pG->next_pos.z, w->dstPos.z);
    FSet(pG->next_angle, w->dstAngle);
    U16Set(pG->room_id_prev, pG->room_id);
    U8Set(pG->x4FA2, pG->x4F9E);
    pG->next_stage = w->dstStage;
    pG->next_room_no = w->dstRoom;
    pG->next_point = w->dstX4F9E;
    pG->door_no = w->doorNo;
    pG->x20 = 4;
    pG->x21 = 0;
    pG->x22 = 0;
    pG->x23 = 0;
    U16Set(pG->x4F90, 0);
    BitOff(pG->flags_54, 0x40);
    return 1;
}

static int sceAtFunc_exec(SceAtWork* w, cModel* m)
{
    if (w->func == 0) {
        return 0;
    }
    if (w->prio == 0) {
        SetTaskModelPtr(m, 0);
        ((void (*)(int)) w->func)(w->arg);
    } else {
        SceExec(w->prio, w->func, w->arg, w->x45, w->x44, m);
    }
    return 1;
}

static void initReleaseModelTbl()
{
    memclr_asm(releaseModelTbl, sizeof(releaseModelTbl));
}

static void setReleaseModelTbl(void* bin, void* tpl)
{
    u32 i;

    for (i = 0; i < 8; i++) {
        SceAtReleaseModel* t = &releaseModelTbl[i];

        if (t->cnt == 0) {
                        t->bin = bin;
            t->tpl = tpl;
            t->cnt = 3;
break;
        }
    }
}

static void checkReleaseModelTbl()
{
    u32 i;

    for (i = 0; i < 8; i++) {
        SceAtReleaseModel* t = &releaseModelTbl[i];

        if (t->cnt > 0) {
            t->cnt -= 1;
            if (t->cnt <= 0) {
                if (t->bin != 0) {
                    Mem_free(t->bin);
                }
                if (t->tpl != 0) {
                    Mem_free(t->tpl);
                }
                t->bin = 0;
                t->tpl = 0;
                t->cnt = 0;
            }
        }
    }
}

#line 990 "D:/Bio4/Prog/sce_at.cpp"
int itemZoom(SceAtWork* w)
{
    SceAtItem* it = &w->item;
    char name[0x20];
    char name2[0x20];
    void* bin;
    void* tpl;
    cObj* obj;
    int ret;

    if (it->id == 0x95 || it->id == 0x97) {
        if (it->pModel != 0) {
            p_imodel_bak = it->pModel;
            it->pModel = 0;
            w->item.flag |= 8;
        }
    }
    if (it->pModel == 0) {
        sprintf(name, "SS/cmn/itm%02x.bin", it->id);
        sprintf(name2, "SS/cmn/itm%02x.tpl", it->id);
        bin = 0;
        tpl = 0;
#line 1008 "D:/Bio4/Prog/sce_at.cpp"
        ret = DvdReadN(name, 0, 0, 0, 0, 5, __FILE__, __LINE__);
        if (Dvd.ReadCheck(ret, 0, 0, &bin) < 0) {
            pLog->err(0, 0, "Item model \"%s\" load faild!", name);
            return 0;
        }
#line 1018 "D:/Bio4/Prog/sce_at.cpp"
        ret = DvdReadN(name2, 0, 0, 0, 0, 5, __FILE__, __LINE__);
        if (Dvd.ReadCheck(ret, 0, 0, &tpl) < 0) {
            if (bin != 0) {
                Mem_free(bin);
            }
            pLog->err(0, 0, "Item model \"%s\" load faild!", name2);
            return 0;
        }
        obj = setItemObj(bin, tpl, (Vec*) &vecZero, (Vec*) &vecZero);
        if (obj == 0) {
            pLog->err(0, 0, "Item %d set faild!", it->id);
            if (bin != 0) {
                Mem_free(bin);
            }
            if (tpl != 0) {
                Mem_free(tpl);
            }
            return 0;
        }
        SceAtSetItemModel(w, obj);
        w->item.flag |= 2;
    }
    return 1;
}

void releaseModel(SceAtWork* w, int keep)
{
    if ((w->item.flag & 2) && w->item.pModel != 0) {
        cObj* obj = (cObj*) w->item.pModel;
        void* bin = obj->pInfo->pData;
        void* tpl = obj->pInfo->pTpl;

        ObjMgr.destroy(obj);
        setReleaseModelTbl(bin, tpl);
        w->item.pModel = 0;
        w->item.flag &= ~2;
    }
    if (w->item.flag & 8) {
        PSet(w->item.pModel, p_imodel_bak);
        p_imodel_bak = 0;
        w->item.flag &= ~8;
        if (keep == 0) {
            w->item.pModel->be_flag &= ~2;
        }
    }
}

#define ITEM_CANCEL()                                     \
    {                                                     \
        itemExam.reset();                                 \
        it->pModel->setNoSuspend(0);                      \
        if (it->flag & 4) {                               \
            it->pModel->be_flag &= ~2;                    \
            it->flag &= ~4;                               \
        }                                                 \
        releaseModel(w, 1);                               \
        BitOff(pG->flags_5010, 2);                        \
        BitOn(pG->flags_5014, 0x10000000);                \
        SceSys.x76 = 0;                                   \
        SceUpCutEnd();                                    \
        return;                                           \
    }

static void sceAtGetItem(SceAtWork* w_)
{
    // COMPILER-DIFF: 13 (global-alloc pair w/cancel r24/r25): value pin of the parameter copy.
    register SceAtWork* w asm("r24") = w_;
    static int disp_flag_bak;
    static int sub_screen_open;
    static int swep_flag;
    SceAtItem* it = &w->item;
    cModel* model = w->item.pModel;
    int fh = cMes.getWork()->fontH;
    int ls = cMes.getWork()->lineSpace;
    int y = 0x129 - fh - ls;
    // `cancel` is the newest zero when `swep_flag = 0` is expanded (sel has no initializer), so
    // cse stores its r25 there and cancel lives from the top: it then ranks below sel in global
    // alloc (sel r29, cancel r25, as in the original).
    int cancel = 0;
    int mes = 0;
    int put = 1;
    int sel;
    int i;
    ItemInfo info;
    ItemWork tmp;

    SceUpCutStart();
    swep_flag = 0;
    BitOn(pG->flags_170, 0x40000000);
    itemInfo(it->id, &info);
    switch (info.type) {
    case 0:
    case 4:
    case 5:
    case 7:
    case 0xC:
        put = ItemMgr.get(it->id, it->num);
        itemInfo(it->id, &info);
        switch (info.type) {
        case 7:
            SndCall(0, 0, 0, 0, 0, 0);
            break;
        case 5:
        case 0xC:
            SndCall(0, 0x13, 0, 0, 0, 0);
            break;
        }
        cMes.MesSet(0x15, 0x64, y, 0x11, 0, 0, 4);
        cMes.mes[0].x1A = it->id;
        itemInfo(it->id, &info);
        if (info.type == 7) {
            cMes.mes[0].waitCnt = 0x1E;
        }
        mes = 0;
        break;
    case 8: {
        // COMPILER-DIFF: 13 (global-alloc rotation it/ItemMgr/money): money pinned to the
        // original's r29 settles the other two (it r31, the ItemMgr high r30).
        register u32 money asm("r29") = pG->x4F98;

        put = ItemMgr.get(it->id, it->num);
        if (it->id == 0x73) {
            cMes.mes[0].setNumber(ItemMgr.x28, 0);
            cMes.MesSet(0x94, 0x64, y, 0x10000011, 0, 0, 4);
        } else if (it->id == 0x75) {
            cMes.mes[0].setNumber(ItemMgr.x2C, 0);
            cMes.MesSet(0x95, 0x64, y, 0x10000011, 0, 0, 4);
        } else {
            if ((s32) money < (s32) pG->x4F98) {
                cMes.mes[0].setNumber(pG->x4F98 - money, 0);
            }
            cMes.MesSet(0x14, 0x64, y, 0x10000011, 0, 0, 4);
        }
        mes = 0;
        SndCall(0, 0x10, 0, 0, 0, 0);
        break;
    }
    case 3:
        itemInfo(ItemMgr.armId, &info);
        if (info.type == 3) {
            swep_flag = put;
        }
    case 1:
    case 9:
        mes = 1;
        cMes.MesSet(0x11, 0x64, y, 0x111, 0, 0, 4);
        cMes.mes[0].x1A = it->id;
        break;
    case 2:
        cMes.MesSet(0x13, 0x64, y, 0x211, 0, 0, 4);
        cMes.mes[0].x1A = it->id;
        if (it->num != 0) {
            cMes.mes[0].setNumber(it->num, 0);
        } else {
            itemInfo(it->id, &info);
            cMes.mes[0].setNumber(info.x3, 0);
        }
        mes = 1;
        break;
    case 0xE:
        cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
        mes = 1;
        cMes.mes[0].x1A = it->id;
        break;
    case 6:
        switch (it->id) {
        case 5:
        case 6:
        case 8:
        case 9:
        case 0xA:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x95:
        case 0x97:
            if ((s16) pG->pl_life >= (s16) pG->pl_life_max) {
                cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
            } else {
                cMes.MesSet(0x12, 0x64, y, 0x411, 0, 0, 4);
            }
            break;
        default:
            cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
            break;
        }
        cMes.mes[0].x1A = it->id;
        mes = 1;
        break;
    case 0xA:
        if (SubScreenOpen(0x40, 0) == 0) {
            SceSleep(1);
        }
        SubScreenWk.x2FA = it->id;
        sub_screen_open = put;
        SubScreenWk.x2FC = put;
        break;
    }
    itemExam.setup();
    SceSleep(1);
    if (it->pModel->isTrans() == 0) {
        it->pModel->be_flag |= 2;
        it->flag |= 4;
    }
    sel = 0;
    cancel = 0;
    BitOn(pG->flags_5010, 2);
    disp_flag_bak = pG->flags_58;
    BitSet(pG->flags_58, -1);
    BitOff(pG->flags_58, 0x00010000);
    BitOff(pG->flags_58, 0x04000000);
    BitOff(pG->flags_58, 0x00002000);
    BitOff(pG->flags_58, 0x00000800);
    itemExam.init(w->item.id, model, 0);
    LightMgr.offScr(0x20);
    LightMgr.create(0, 9, -2, 0);
    sub_screen_open = sel;
    if (mes != 0) {
        while (cMes.getWork()->result == 0 && cancel == 0) {
            itemExam.move();
            itemExam.trans();
            if (Key.trg & 0x40000000) {
                MessageControl* mc = &cMes;

                for (i = 0; i <= 15; i++) {
                    mc->Delete(i);
                }
                put = 0;
                cancel = 1;
            }
            SceSleep(1);
        }
        if (cancel == 0) {
            // COMPILER-DIFF: candidate #12 (r0 pin): cse1 follows the `bne` into the else arm and would
            // canonicalise `res == 2` to sel; canon_reg never replaces a hard register, so the pinned res
            // keeps `cmpwi r0,2` and sel (a pseudo: preferred as class head) keeps `mr; cmpwi sel,1`.
            register int res asm("r0") = cMes.getWork()->result;

            sel = res;
            if (sel == 1) {
                put = PutInCase(it->id, it->num, (s8) SubScreenWk.x2AE);
                if (put != 1) {
                    if (SubScreenOpen(4, 0) == 0) {
                        SceSleep(1);
                    }
                    SubScreenWk.x2FA = it->id;
                    SubScreenWk.x2FC = it->num;
                    sub_screen_open = sel;
                }
            } else if (res == 2) {
                put = 0;
            } else {
                u16 n = it->num;

                tmp.id = it->id;
                if (n == 0) {
                    itemInfo(it->id, &info);
                    tmp.num = info.x3;
                } else {
                    tmp.num = n;
                }
                ItemMgr.x12 = 0;
                // `put = 1` after the call (as in sceAtGetItem_NoModel): sched2 hoists the
                // callee-saved li above the call with the highest LUID, so `addi r4,&tmp` issues first.
                ItemMgr.use(&tmp);
                put = 1;
            }
        }
    } else {
        while (cMes.mes[0].flags2 & 1) {
            itemExam.move();
            itemExam.trans();
            SceSleep(1);
        }
    }
    itemExam.quit();
    pG->flags_58 = disp_flag_bak;
    if (CamCtrl.area_no != -1) {
        LightMgr.update(CamCtrl.area_no, 0);
    } else {
        LightMgr.update(0, 0);
    }
    EffectEspDelete(1, 0x3B, (u32) model, 0);
    EffectEspgenDelete(1, 0x3B, (int) model);
    EffectEfmDelete(1, 0x3B, (int) model);
    if (sub_screen_open != 0) {
        while (SubScreenWk.x34 == 0) {
            SceSleep(1);
        }
        if (SubScreenWk.x40 == 0) {
            ITEM_CANCEL();
        }
    } else {
        if (put == 0) {
            ITEM_CANCEL();
        }
    }
    sceAtItemFlgOn(it);
    if (swep_flag != 0) {
        PlReloadBullet();
    }
    SceAtSetEnable(w->no, 0);
    releaseModel(w, 0);
    if ((w->item.flag2 & 8) && w->item.saveNo >= 0) {
        memclr_asm(&pG->save_item[w->item.saveNo], sizeof(SceAtSaveItem));
    }
    if (w->flag & 4) {
        Mem_free(w);
        DelPrim(&pS->ot[15], (u32*) w);
    }
    SceSys.x76 = 0;
    SceUpCutEnd();
    BitOff(pG->flags_5010, 2);
    BitOn(pG->flags_5014, 0x10000000);
}

#define ITEM_CANCEL_NOMODEL()                             \
    {                                                     \
        SceSys.x76 = 0;                                   \
        BitOn(pG->flags_5014, 0x10000000);                \
        SceUpCutEnd();                                    \
        return;                                           \
    }

static void sceAtGetItem_NoModel(SceAtWork* w)
{
    static int sub_screen_open;
    static int swep_flag;
    SceAtItem* it = &w->item;
    int fh = cMes.getWork()->fontH;
    int ls = cMes.getWork()->lineSpace;
    int y = 0x129 - fh - ls;
    int cancel = 0;
    int mes = 0;
    int put = 1;
    int sel;
    // COMPILER-DIFF: codeless def: a mention of sel before the result block so cse1 makes sel the
    // head of the `sel = res` class (a block-local sel is replaced by the sign-extend temp: 4 refs, r31).
    asm("" : "=r"(sel));
    int i;
    ItemInfo info;
    ItemWork tmp;

    SceUpCutStart();
    pPL->setNoSuspend(1);
    BitOff(pG->flags_58, 0x40000000);
    swep_flag = 0;
    // COMPILER-DIFF: 12 (sched2 rank in block 0): the `it->id` load after the swep_flag store ranks
    // `li r31,0` (cancel) above `addi r29,&w->item` in the prologue.
    asm("" : "=m"(*(int*) &it->id) : "m"(swep_flag));
    itemInfo(it->id, &info);
    switch (info.type) {
    case 0:
    case 4:
    case 5:
    case 7:
    case 0xC:
        put = ItemMgr.get(it->id, it->num);
        itemInfo(it->id, &info);
        switch (info.type) {
        case 7:
            SndCall(0, 0, 0, 0, 0, 0);
            break;
        case 5:
        case 0xC:
            SndCall(0, 0x13, 0, 0, 0, 0);
            break;
        }
        cMes.MesSet(0x15, 0x64, y, 0x11, 0, 0, 4);
        cMes.mes[0].x1A = it->id;
        itemInfo(it->id, &info);
        if (info.type == 7) {
            cMes.mes[0].waitCnt = 0x1E;
        }
        mes = 0;
        break;
    case 8: {
        u32 money = pG->x4F98;

        put = ItemMgr.get(it->id, it->num);
        if (it->id == 0x73) {
            cMes.mes[0].setNumber(ItemMgr.x28, 0);
            cMes.MesSet(0x94, 0x64, y, 0x10000011, 0, 0, 4);
        } else if (it->id == 0x75) {
            cMes.mes[0].setNumber(ItemMgr.x2C, 0);
            cMes.MesSet(0x95, 0x64, y, 0x10000011, 0, 0, 4);
        } else {
            if ((s32) money < (s32) pG->x4F98) {
                cMes.mes[0].setNumber(pG->x4F98 - money, 0);
            }
            cMes.MesSet(0x14, 0x64, y, 0x10000011, 0, 0, 4);
        }
        mes = 0;
        SndCall(0, 0x10, 0, 0, 0, 0);
        break;
    }
    case 3:
        itemInfo(ItemMgr.armId, &info);
        if (info.type == 3) {
            swep_flag = put;
        }
    case 1:
    case 9:
        mes = 1;
        cMes.MesSet(0x11, 0x64, y, 0x111, 0, 0, 4);
        cMes.mes[0].x1A = it->id;
        break;
    case 2:
        cMes.MesSet(0x13, 0x64, y, 0x211, 0, 0, 4);
        cMes.mes[0].x1A = it->id;
        if (it->num != 0) {
            cMes.mes[0].setNumber(it->num, 0);
        } else {
            itemInfo(it->id, &info);
            cMes.mes[0].setNumber(info.x3, 0);
        }
        mes = 1;
        break;
    case 0xE:
        cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
        mes = 1;
        cMes.mes[0].x1A = it->id;
        break;
    case 6:
        switch (it->id) {
        case 5:
        case 6:
        case 8:
        case 9:
        case 0xA:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x95:
        case 0x97:
            if ((s16) pG->pl_life >= (s16) pG->pl_life_max) {
                cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
            } else {
                cMes.MesSet(0x12, 0x64, y, 0x411, 0, 0, 4);
            }
            break;
        default:
            cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
            break;
        }
        cMes.mes[0].x1A = it->id;
        mes = 1;
        break;
    case 0xA:
        if (SubScreenOpen(0x40, 0) == 0) {
            SceSleep(1);
        }
        SubScreenWk.x2FA = it->id;
        sub_screen_open = put;
        SubScreenWk.x2FC = put;
        break;
    }
    sub_screen_open = 0;
    cancel = 0;
    if (mes != 0) {
        while (cMes.getWork()->result == 0 && cancel == 0) {
            if (Key.trg & 0x40000000) {
                MessageControl* mc = &cMes;

                for (i = 0; i <= 15; i++) {
                    mc->Delete(i);
                }
                put = 0;
                cancel = 1;
            }
            SceSleep(1);
        }
        if (cancel == 0) {
            // COMPILER-DIFF: candidate #12 (r0 pin): cse1 follows the `bne` into the else arm and would
            // canonicalise `res == 2` to sel; canon_reg never replaces a hard register, so the pinned res
            // keeps `cmpwi r0,2` and sel (a pseudo: preferred as class head) keeps `mr; cmpwi sel,1`.
            register int res asm("r0") = cMes.getWork()->result;

            sel = res;
            if (sel == 1) {
                put = PutInCase(it->id, it->num, (s8) SubScreenWk.x2AE);
                if (put != 1) {
                    if (SubScreenOpen(4, 0) == 0) {
                        SceSleep(1);
                    }
                    SubScreenWk.x2FA = it->id;
                    SubScreenWk.x2FC = it->num;
                    sub_screen_open = sel;
                }
            } else if (res == 2) {
                ITEM_CANCEL_NOMODEL();
            } else {
                u16 n = it->num;

                tmp.id = it->id;
                if (n == 0) {
                    itemInfo(it->id, &info);
                    tmp.num = info.x3;
                } else {
                    tmp.num = n;
                }
                ItemMgr.x12 = 0;
                ItemMgr.use(&tmp);
                put = 1;
            }
        }
    } else {
        while (cMes.mes[0].flags2 & 1) {
            SceSleep(1);
        }
    }
    if (sub_screen_open != 0) {
        while (SubScreenWk.x34 == 0) {
            SceSleep(1);
        }
        if (SubScreenWk.x40 == 0) {
            ITEM_CANCEL_NOMODEL();
        }
    } else {
        if (put == 0) {
            ITEM_CANCEL_NOMODEL();
        }
    }
    sceAtItemFlgOn(it);
    if (swep_flag != 0) {
        PlReloadBullet();
    }
    SceAtSetEnable(w->no, 0);
    if ((w->item.flag2 & 8) && w->item.saveNo >= 0) {
        memclr_asm(&pG->save_item[w->item.saveNo], sizeof(SceAtSaveItem));
    }
    if (w->flag & 4) {
        Mem_free(w);
        DelPrim(&pS->ot[15], (u32*) w);
    }
    pPL->setNoSuspend(0);
    SceSys.x76 = 0;
    BitOn(pG->flags_5014, 0x10000000);
    SceUpCutEnd();
}

static int sceAtFunc_item(SceAtWork* w, cModel* m)
{
    SceAtItem* it = &w->item;
    int ret;
    ScePrim* p;

    KeyClear(0xEFCF0000);
    ret = itemZoom(w);
    if (ret == 1) {
        p = SceExec(5, (TaskFunc) sceAtGetItem, (int) w, 0, 0xF, 0);
        if (p != 0) {
            SceSys.x76 = ret;
            p->task->flag |= 2;
            it->pModel->setNoSuspend(1);
        }
    } else {
        p = SceExec(5, (TaskFunc) sceAtGetItem_NoModel, (int) w, 0, 0xF, 0);
        if (p != 0) {
            SceSys.x76 = 1;
            p->task->flag |= 2;
        }
    }
    return 1;
}

// Room save flag helpers as the original flag_rsf.h has them (HALT lines 17 / 21).
static inline u32* RsfFlags(u16 room)
{
    return (u32*) (RoomData.getRoomSavePtr(room) + 4);
}

static inline void RsfSet(u16 room, int no)
{
    if (no > 0x1F) {
#line 17 "D:/Bio4/Prog/flag_rsf.h"
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);
        *(volatile u32*) 0x11111111 = 0;
    }
    RsfFlags(room)[(u32) no >> 5] |= 0x80000000 >> (no & 31);
}

static inline void RsfClear(u16 room, int no)
{
    if (no > 0x1F) {
#line 21 "D:/Bio4/Prog/flag_rsf.h"
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);
        *(volatile u32*) 0x11111111 = 0;
    }
    RsfFlags(room)[(u32) no >> 5] &= ~(0x80000000 >> (no & 31));
}
#line 1400 "D:/Bio4/Prog/sce_at.cpp"

static int sceAtFunc_flg(SceAtWork* w, cModel* m)
{
    SceAtFlg* f = &w->flg;

    switch (f->kind) {
    case 0:
        if (f->off == 0) {
            eventFlags()[f->no >> 5] |= 0x80000000 >> (f->no & 31);
        } else {
            eventFlags()[f->no >> 5] &= ~(0x80000000 >> (f->no & 31));
        }
        break;
    case 1: {
        u8 off = f->off;

        if (off == 0) {
            u16 no = f->no;
            u16 room = pG->room_id;

            RsfSet(room, no);
        } else {
            u16 no = f->no;
            u16 room = pG->room_id;

            RsfClear(room, no);
        }
        break;
    }
    case 2:
        if (f->off == 0) {
            flags51BC()[f->no >> 5] |= 0x80000000 >> (f->no & 31);
        } else {
            flags51BC()[f->no >> 5] &= ~(0x80000000 >> (f->no & 31));
        }
        break;
    }
    return 0;
}

static int sceAtFunc_mes(SceAtWork* w, cModel* m)
{
    SceAtMesData* d = &w->mes;

    if (d->x4 != 0) {
        SceExec(5, (TaskFunc) SceAtSetMes, (int) d, 0, 2, 0);
    } else {
        SceAtSetMes(d);
    }
    return 1;
}

void SceAtSetMes(SceAtMesData* m)
{
    u32 flags = 0x10;

    if (m->x4 != 0) {
        SceUpCutStart();
        CamCtrl.CutCall((s8) (m->x4 - 1));
        flags = 0x30;
    }
    if (m->no >= 0) {
        if (m->type == 0) {
            SceMesSet(m->no, flags, 1, 0x64, MES_Y);
        } else {
            SceMesSet(m->no, flags | 1, 1, 0x64, MES_Y);
        }
    }
    if (m->x6 != 0) {
        if (m->x5 == 0) {
            SndCall(6, m->x6 - 1, 0, 0, 0, 0);
        } else {
            SndCall(0, m->x6 - 1, 0, 0, 0, 0);
        }
    }
    if (m->x4 != 0) {
        if (m->no >= 0) {
            SceMesWait();
        }
        if (!(m->x8 & 4)) {
            CamCtrl.Comeback(0);
        }
        SceUpCutEnd();
    }
}

static int sceAtFunc_save(SceAtWork* w, cModel* m)
{
    if (pSUB != 0 && ((pG->flags_5014 & 0x20000000) || (SubCharGetStatus() & 0x02000000))) {
        cMes.MesSet(0x97, 0x64, MES_Y, 1, 0, 0, 4);
    } else {
        CardSave(w->value, 1);
    }
    return 1;
}

static int sceAtFunc_shd_disp(SceAtWork* w, cModel* m)
{
    SceAtShdDisp* s = &w->shd;

    if (s->done == 1) {
        return 0;
    }
    if (s->on == 1) {
        ShdGetObjPtr(w->shd.no)->be_flag |= 0x80;
    } else {
        ShdGetObjPtr(w->shd.no)->be_flag &= ~0x80;
    }
    s->done = 1;
    return 0;
}

void sceAtFunc_shd_disp_reverse(SceAtWork* w)
{
    SceAtShdDisp* s = &w->shd;

    if (s->done != 0) {
        if (s->on == 1) {
            ShdGetObjPtr(w->shd.no)->be_flag &= ~0x80;
        } else {
            ShdGetObjPtr(w->shd.no)->be_flag |= 0x80;
        }
        s->done = 0;
    }
}

static int sceAtFunc_damage(SceAtWork* w, cModel* m)
{
    Vec pt[4];
    Vec c;
    int time = w->dmg.time;

    if (time == 0) {
        time = 1;
    }
    if (w->x39 & 1) {
        int dead = 1;

        if ((pPL->flags_324 & 0xFFFF0000) == 0) {
            dead = 0;
        }
        if (dead == 0 && (s16) pG->pl_life > 0) {
            u8 fl = w->dmg.flags;
            int a = 0;
            int b = 0xFF;

            if (fl & 1) {
                a = 1;
            }
            if (w->dmg.time != 0) {
                b = (u8) w->dmg.time;
            }
            if (fl & 2) {
                pPL->setDamage(w->dmg.kind, w->dmg.arg, w->dmg.power, a, b);
            } else {
                pPL->setDamage(w->dmg.kind, w->dmg.arg, 123.0f, a, b);
            }
        }
    }
    if (w->x39 & 8) {
        if (pSUB != 0) {
            int dead = 1;

            if ((pSUB->flags_324 & 0xFFFF0000) == 0) {
                dead = 0;
            }
            if (dead == 0 && (s16) pG->sub_life > 0) {
                u8 fl = w->dmg.flags;
                int a = 0;
                int b = 0xFF;

                if (fl & 1) {
                    a = 1;
                }
                if (w->dmg.time != 0) {
                    b = (u8) w->dmg.time;
                }
                if (fl & 2) {
                    pSUB->setDamage(w->dmg.kind, w->dmg.arg, w->dmg.power, a, b);
                } else {
                    pSUB->setDamage(w->dmg.kind, w->dmg.arg, 123.0f, a, b);
                }
            }
        }
    }
    if (w->x39 & 2) {
        switch (w->area.type) {
        case 1:
            pt[0].x = w->area.u.xz4.p[0].x;
            pt[0].y = w->area.u.xz4.y;
            pt[0].z = w->area.u.xz4.p[0].z;
            pt[1].x = w->area.u.xz4.p[3].x;
            pt[1].y = w->area.u.xz4.y;
            pt[1].z = w->area.u.xz4.p[3].z;
            pt[2].x = w->area.u.xz4.p[2].x;
            pt[2].y = w->area.u.xz4.y;
            pt[2].z = w->area.u.xz4.p[2].z;
            pt[3].x = w->area.u.xz4.p[1].x;
            pt[3].y = w->area.u.xz4.y;
            pt[3].z = w->area.u.xz4.p[1].z;
            DmgMgr.set(w->dmg.kind, time, pt, w->area.u.xz4.h);
            break;
        case 2:
            c.x = w->area.u.cyl.x;
            c.y = w->area.u.cyl.y;
            c.z = w->area.u.cyl.z;
            DmgMgr.set(w->dmg.kind, time, &c, w->area.u.cyl.r, w->area.u.cyl.h);
            break;
        }
    }
    return 0;
}

static int sceAtFunc_scr_at(SceAtWork* w, cModel* m)
{
    return 0;
}

static int sceAtFunc_field_info(SceAtWork* w, cModel* m)
{
    if (w->field.value == 0) {
        ((cEm*) m)->litArea.x0 |= 1;
    }
    return 0;
}

static int sceAtFunc_stoop(SceAtWork* w, cModel* m)
{
    PlSetCrouch();
    return 0;
}

static int sceAtFunc_skey(SceAtWork* w, cModel* m)
{
    pS->x94 = pG->flags_170;
    KeyStop(0xEFCF0000);
    pG->flags_170 = -1;
    TaskExec(1, (TaskFunc) sceAtSkey, (int) w);
    return 0;
}

static void sceAtSkey(SceAtWork* w)
{
    cMes.MesSet(0xC, 0x64, MES_Y, 1, 0, 0, 4);
    while (cMes.mes[0].flags2 & 1) {
        TaskSleep(1);
    }
    pG->flags_170 = pS->x94;
    TaskExit();
}

void sceAtLadder(SceAtWork* w)
{
    CamCtrl.CutCall((s8) (w->ladder.cut1 - 1));
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if (w->ladder.cut2 != 0) {
        CamCtrl.CutCall((s8) (w->ladder.cut2 - 1));
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    if (w->ladder.cut3 != 0) {
        CamCtrl.CutCall((s8) (w->ladder.cut3 - 1));
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    while (PlGetStatus() & 0x40000) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceSys.x134 = 0;
}

static int sceAtFunc_ladder(SceAtWork* w, cModel* m)
{
    Vec pos;
    f32 ang;

    sceAtGetLadderPos(&w->ladder, &pos, &ang);
    PlSetLadder(&pos, w->ladder.level, ang);
    if (w->ladder.cut1 != 0) {
        SceSys.x134 = SceExec(5, (TaskFunc) sceAtLadder, (int) w, 0, 2, 0);
    }
    return 0;
}

void sceAtGetLadderPos(SceAtLadder* l, Vec* pos, f32* ang)
{
    Vec ofs = {0.0f, 0.0f, 300.0f};
    Vec rot;
    Vec tmp = {0.0f, 0.0f, 0.0f};
    Mtx mat;

    tmp.y = l->angle;
    rot = tmp;
    low_RotMatrix(mat, &rot);
    TransMatrix(mat, &l->pos);
    PSMTXMultVec(mat, &ofs, pos);
    *ang = l->angle + PI;
    *ang = LIMIT_ANGLE(*ang);
}

int sceAtCheckLadderUp(SceAtLadder* l, cModel* m)
{
    AreaData area;
    Vec pos;
    f32 ang;
    u32 i;

    sceAtGetLadderPos(l, &pos, &ang);
    AreaDataInit(&area, &pos, 2, 500.0f, 2000.0f);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (em->id <= 0x20 && m != em) {
            if (AreaHitCheck(&area, &em->pos) == 1) {
                return 0;
            }
        }
    }
    return 1;
}

static int sceAtFunc_use(SceAtWork* w, cModel* m)
{
    ItemMgr.available(w->useItem[1]);
    return 0;
}

static int sceAtFunc_hide(SceAtWork* w, cModel* m)
{
    SubCharCtrlHide(&w->hide.pos, w->hide.mode);
    w->hide.step = 1;
    SndCall(6, 0x5E, 0, 0, 0, 0);
    return 0;
}

void SceAtDataSet_hide(int no, void (*func)(int))
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtDataSet_hide(): AT NOT FOUND");
    } else if (w->x35 != 0x12) {
        pLog->err(0, 0, "SceAtDataSet_hide(): ID is not HIDE");
    } else {
        w->hide.func = func;
    }
}

int SceAtCheckHideActive()
{
    if ((SubCharGetStatus() & 0x04000000) || (SubCharGetStatus() & 0x08000000) || (SubCharGetStatus() & 0x10000000) ||
        pS->hideActive != 0) {
        return 1;
    }
    return 0;
}

void SceAtCheckHideProc()
{
    static int timer;
    SceAtWork* w = sceAtSetOtStart();
    int off;
    u8 step;
    ScePrim* p;

    while ((w = sceAtGetOtAddr(w)) != 0) {
        off = !(w->flag & 1);
        if (off) {
            continue;
        }
        if (w->x35 != 0x12) {
            continue;
        }
        step = w->hide.step;
        if (step != 0) {
            goto FOUND;
        }
    }
    return;
FOUND:
    switch (step) {
    case 0:
        break;
    case 1:
        if (SubCharGetStatus() & 0x08000000) {
            if (w->hide.func != 0) {
                SceKill(w->hide.func);
                SceExec(0x12, (TaskFunc) w->hide.func, 0, 0, 2, 0);
            }
            w->hide.step++;
            pS->hideActive = step;
        }
        break;
    case 2:
        if (PlSetWhistle() != 0) {
            w->hide.step++;
            KeyStop(0xEFCF0000);
            timer = 0x19;
        }
        break;
    case 3:
        if (timer == 0) {
            p = 0;
            if (w->hide.func != 0) {
                SceKill(w->hide.func);
                p = SceExec(0x12, (TaskFunc) w->hide.func, 1, 0, 2, 0);
            }
            BitOff(pG->flags_170, 0x80000000);
            SubCharCtrlHide(&pPL->pos, 0);
            if (w->hide.cut != 0) {
                SceUpCutStart();
                BitOff(pG->flags_58, 0x20000000);
                pSUB->setNoSuspend(1);
                BitOff(pG->flags_170, 0x1000);
                if (p != 0) {
                    p->task->flag |= 2;
                }
                CamCtrl.CutCall((s8) (w->hide.cut - 1));
                w->hide.step++;
            } else {
                w->hide.step = off;
                pS->hideActive = off;
            }
        }
        timer--;
        break;
    case 4:
        if (CamCtrl.IsMotionEnd() == 1) {
            pSUB->setNoSuspend(0);
            SceUpCutEnd();
            CamCtrl.Comeback(0);
            w->hide.step = off;
            pS->hideActive = off;
        }
        break;
    }
}

// OPEN (register only): the original loads dstAngle into f0 and the 0.0 constant into f13 (ours
// swapped: local-alloc qty priority); store orders, chains and a zero local tried.
static int sceAtFunc_pos_jump(SceAtWork* w, cModel* m)
{
    Vec rot;
    // COMPILER-DIFF: #13. The original's 0.0 is a reload-materialised constant (f13, the FPR after
    // the local-alloc'd dstAngle load in f0); ours allocates the 3-ref 0.0 first (f0). Both values
    // pinned: the angle too, so the y store gets the same call anti-dependent as the z/x stores
    // and the three stores keep the source order.
    register f32 a asm("fr0");
    register f32 z asm("fr13");

    pPL->setPos(&w->jumpPos);
    a = w->dstAngle;
    z = 0.0f;
    rot.y = a;
    rot.x = z;
    rot.z = z;
    pPL->setAng(&rot);
    CamCtrl.qfps.setPlayerLocation(pPL->mat, pPL->pFloorNrm);
    return 0;
}

void SceAtStopSemiautoCheck()
{
    pS->stop |= 0x80000000;
}

void SceAtRoomSet()
{
    SceAtWork* w = sceAtSetOtStart();

    while ((w = sceAtGetOtAddr(w)) != 0) {
        u8 t = w->x35;

        switch (t) {
        case 0xB:
            if (w->flag & 1) {
                sceAtSetScrAt(w);
            }
            break;
        case 1:
            if (bitOff(w->x38) && !(w->x38 & 8)) {
                w->x53 = 1;
                w->x4A = 0x10;
                w->x38 = (w->x38 & 0x80) | 8;
                w->x44 = 2;
            }
            break;
        case 5:
            if (!(w->x38 & 8)) {
                w->x4A = 1;
                w->x38 = (w->x38 & 0x80) | 8;
                w->x44 = 2;
            }
            break;
        case 0xE:
            if (!(w->x38 & 8)) {
                w->x4A = 0x13;
                w->x38 = (w->x38 & 0x80) | 8;
                w->x44 = 5;
            }
            break;
        case 8:
            if (!(w->x38 & 8)) {
                w->x4A = 0x2F;
                w->x38 = (w->x38 & 0x80) | 8;
                w->x44 = 5;
            }
            break;
        case 0x10:
            if (!(w->x38 & 8)) {
                w->x38 = (w->x38 & 0x80) | 8;
                if (w->ladder.level > 0) {
                    w->x4A = 8;
                } else {
                    w->x4A = 9;
                }
                w->x44 = 5;
            }
            break;
        case 0x12:
            if (!(w->x38 & 8)) {
                w->x4A = 0x20;
                w->x38 = (w->x38 & 0x80) | 8;
                w->x44 = 5;
            }
            break;
        case 0x11:
            w->x38 = 1;
            break;
        }
    }
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->flag)) {
            continue;
        }
        if (w->x35 == 3) {
            if (w->item.id == 0x1000) {
                EmReadSearch(0x24, 0, 0);
            }
            sceAtSetItem(w);
        }
        if (pG->x4F93 == 0) {
            if (w->x52 & 2) {
                SceAtSetEnable(w->no, 0);
            }
        } else {
            if (w->x52 & 1) {
                SceAtSetEnable(w->no, 0);
            }
        }
    }
}

void sceAtSetScrAt(SceAtWork* w)
{
    Vec pos;
    f32 h;

    if (w->area.type != 1) {
        return;
    }
    if (w->scr.created != 0) {
        return;
    }
    Vec rot = { 0.0f, 0.0f, 0.0f };
    Vec poly[4];
    if (w->pParent != 0) {
        cModel* p = w->pParent;

        pos = p->pos;
        poly[0].x = p->scale.x * w->area.u.xz4.p[0].x;
        poly[0].y = p->scale.y * w->area.u.xz4.y;
        poly[0].z = p->scale.z * w->area.u.xz4.p[0].z;
        poly[1].x = p->scale.x * w->area.u.xz4.p[1].x;
        poly[1].y = p->scale.y * w->area.u.xz4.y;
        poly[1].z = p->scale.z * w->area.u.xz4.p[1].z;
        poly[2].x = p->scale.x * w->area.u.xz4.p[2].x;
        poly[2].y = p->scale.y * w->area.u.xz4.y;
        poly[2].z = p->scale.z * w->area.u.xz4.p[2].z;
        poly[3].x = p->scale.x * w->area.u.xz4.p[3].x;
        poly[3].y = p->scale.y * w->area.u.xz4.y;
        poly[3].z = p->scale.z * w->area.u.xz4.p[3].z;
    } else {
        pos.x = w->area.u.xz4.p[0].x;
        pos.y = w->area.u.xz4.y;
        pos.z = w->area.u.xz4.p[0].z;
        poly[0].x = 0.0f;
        poly[0].y = 0.0f;
        poly[0].z = 0.0f;
        poly[1].x = w->area.u.xz4.p[1].x - w->area.u.xz4.p[0].x;
        poly[1].y = 0.0f;
        poly[1].z = w->area.u.xz4.p[1].z - w->area.u.xz4.p[0].z;
        poly[2].x = w->area.u.xz4.p[2].x - w->area.u.xz4.p[0].x;
        poly[2].y = 0.0f;
        poly[2].z = w->area.u.xz4.p[2].z - w->area.u.xz4.p[0].z;
        poly[3].x = w->area.u.xz4.p[3].x - w->area.u.xz4.p[0].x;
        poly[3].y = 0.0f;
        poly[3].z = w->area.u.xz4.p[3].z - w->area.u.xz4.p[0].z;
    }
    h = w->area.u.xz4.h;
    if (!(w->scr.flags & 2)) {
        if (!(w->scr.flags & 4)) {
            w->scr.attr |= 0x40;
        }
        w->scr.pSat = SatMgr.create(&pos, &rot, poly, w->scr.attr, w->scr.flag, h);
    }
    if (bitOff(w->scr.flags)) {
        w->scr.pEat = EatMgr.create(&pos, &rot, poly, w->scr.attr2, w->scr.flag, h);
    }
    w->scr.created = 1;
}

void sceAtDeleteScrAt(SceAtWork* w)
{
    if (w->scr.created == 1) {
        if (!(w->scr.flags & 2)) {
            SatMgr.destroy(w->scr.pSat);
        }
        if (bitOff(w->scr.flags)) {
            EatMgr.destroy(w->scr.pEat);
        }
        w->scr.pSat = 0;
        w->scr.pEat = 0;
        w->scr.created = 0;
    }
}

void SceAtCheckMoveScrAt()
{
    SceAtWork* w = sceAtSetOtStart();

    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->flag)) {
            continue;
        }
        if (w->x35 != 0xB) {
            continue;
        }
        if (w->pParent == 0) {
            continue;
        }
        if (w->scr.pSat != 0) {
            w->scr.pSat->setCoord(&w->pParent->pos, &w->pParent->rot);
        }
        if (w->scr.pEat != 0) {
            w->scr.pEat->setCoord(&w->pParent->pos, &w->pParent->rot);
        }
    }
}

SceAtWork* SceAtPtr(int no)
{
    SceAtWork* w = sceAtSetOtStart();

    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (w->no == no) {
            return w;
        }
    }
    return 0;
}

// Bit table test with the table as an integer address: the index is not pointer-flagged, so the word offset
// lands in a base register and comes first in `lwzx`.
static inline u32 bitTblChk(u32 tbl, u32 i)
{
    return *(u32*) (((i >> 5) << 2) + tbl) & (0x80000000 >> (i & 31));
}

int sceAtPullAtNo(u8* out)
{
    u32 used[8];
    SceAtWork* w;
    u32 i;

    memclr_asm(used, sizeof(used));
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        ((u32*) used)[w->no >> 5] |= 0x80000000 >> (w->no & 31);
    }
    for (i = 0; i < 256; i++) {
        if (!bitTblChk((u32) used, i)) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

void SceAtSetDoorFunc(int no, TaskFunc func, int arg)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtSetDoorFunc(): AT NOT FOUND");
    } else {
        w->doorFunc = (void (*)()) func;
        w->doorArg = arg;
    }
}

void SceAtDataSet_exec(int no, int prio, int a, TaskFunc func, void* obj, int b)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtDataSet_exec(): AT NOT FOUND");
        return;
    }
    if (w->x35 == 0xF) {
        w->skey.obj = obj;
        w->skey.func = func;
        w->skey.flag = b;
        if (w->prio > 0x12) {
            w->skey.prio = 0x12;
        } else {
            w->skey.prio = prio;
        }
        return;
    }
    if (a != 0) {
        if (w->prioBak == 0) {
            w->prioBak = w->x38;
        }
        w->x38 = a;
    }
    if (w->prio > 0x12) {
        w->prio = 0x12;
    } else {
        w->prio = prio;
    }
    w->func = func;
    w->arg = (int) obj;
    w->x45 = b;
}

void SceAtDataReset(int no)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtDataReset(): AT NOT FOUND");
        return;
    }
    if (w->prioBak != 0) {
        w->x38 = w->prioBak;
        w->prioBak = 0;
    }
    w->prio = 0;
    w->func = 0;
}

void SceAtSetEnable(int no, int on)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtSetEnable(): AT NOT FOUND");
        return;
    }
    if (on == 1) {
        w->flag |= 1;
    } else {
        w->flag &= ~1;
    }
    switch (w->x35) {
    case 3:
        if (on == 1) {
            sceAtSetItem(w);
        } else {
            sceAtDeleteItem(w);
        }
        break;
    case 0xB:
        if (on == 1) {
            sceAtSetScrAt(w);
        } else {
            sceAtDeleteScrAt(w);
        }
        break;
    }
}

// Never-called inline the original kept the string of.
static inline int SceAtCheckEnable(int no)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtCheckEnable(): AT NOT FOUND");
        return 0;
    }
    return w->flag & 1;
}

int SceAtHitCheck(u32 no)
{
    u32* f = pS->hitFlg;

    if (f[no >> 5] & (0x80000000 >> (no & 31))) {
        return 1;
    }
    return 0;
}

void SceAtExecute(int no)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtExecute(): AT NOT FOUND");
        return;
    }
    sceAtFunc_tbl[w->x35].func(w, 0);
}

int SceAtCheckHitModel(int no, cModel* m)
{
    SceAtWork* w = SceAtPtr(no);
    int i;

    if (w == 0) {
        pLog->err(0, 0, "SceAtCheckHitModel(): AT NOT FOUND");
        return 0;
    }
    for (i = 0; i < 16; i++) {
        if (w->hitModel[i] == m) {
            return 1;
        }
    }
    return 0;
}

void SceAtSetActColor(int no, int col)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtSetActColor(): AT NOT FOUND");
    } else {
        w->x53 = col;
    }
}

void SceAtGetCenterPos(Vec* out, int no)
{
    AreaData area;
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtGetCenterPos(): AT NOT FOUND");
    } else {
        sceAtGetArea(&area, w);
        AreaGetCenterPos(out, &area);
    }
}

int SceAtSetParent(SceAtWork* w, cModel* parent, int flag)
{
    if (parent == 0) {
        pLog->err(0, 0, "sceAtSetParent(): pParent == NULL");
        return 0;
    }
    if (w->pParent == parent) {
        return 0;
    }
    Vec inv = { 0.0f, 0.0f, 0.0f };
    if (parent->scale.x != 0.0f) {
        inv.x = 1.0f / parent->scale.x;
    }
    if (parent->scale.y != 0.0f) {
        inv.y = 1.0f / parent->scale.y;
    }
    if (parent->scale.z != 0.0f) {
        inv.z = 1.0f / parent->scale.z;
    }
    w->flag |= flag;
    w->parentParts = -1;
    w->pParent = parent;
    switch (w->area.type) {
    case 1:
        w->area.u.xz4.y -= parent->pos.y;
        w->area.u.xz4.p[0].x -= parent->pos.x;
        w->area.u.xz4.p[0].z -= parent->pos.z;
        w->area.u.xz4.p[1].x -= parent->pos.x;
        w->area.u.xz4.p[1].z -= parent->pos.z;
        w->area.u.xz4.p[2].x -= parent->pos.x;
        w->area.u.xz4.p[2].z -= parent->pos.z;
        w->area.u.xz4.p[3].x -= parent->pos.x;
        w->area.u.xz4.p[3].z -= parent->pos.z;
        w->area.u.xz4.y *= inv.y;
        w->area.u.xz4.p[0].x *= inv.x;
        w->area.u.xz4.p[0].z *= inv.z;
        w->area.u.xz4.p[1].x *= inv.x;
        w->area.u.xz4.p[1].z *= inv.z;
        w->area.u.xz4.p[2].x *= inv.x;
        w->area.u.xz4.p[2].z *= inv.z;
        w->area.u.xz4.p[3].x *= inv.x;
        w->area.u.xz4.p[3].z *= inv.z;
        break;
    case 2:
    case 3:
        w->area.u.cyl.x -= parent->pos.x;
        w->area.u.xz4.y -= parent->pos.y;
        w->area.u.cyl.z -= parent->pos.z;
        w->area.u.cyl.x *= inv.x;
        w->area.u.xz4.y *= inv.y;
        w->area.u.cyl.z *= inv.z;
        break;
    default:
        return 0;
    }
    if (w->x35 == 3) {
        w->item.pos.x -= parent->pos.x;
        w->item.pos.y -= parent->pos.y;
        w->item.pos.z -= parent->pos.z;
        w->item.pos.x *= inv.x;
        w->item.pos.y *= inv.y;
        w->item.pos.z *= inv.z;
        w->item.ofs.x *= inv.x;
        w->item.ofs.y *= inv.y;
        w->item.ofs.z *= inv.z;
        if (w->item.effNo != 0) {
            sceAtItemEffDelete(&w->item);
            sceAtItemEffSet(w, 0);
        }
        if (w->item.pModel != 0) {
            w->item.pModel->pos.x -= w->pParent->pos.x;
            w->item.pModel->pos.y -= w->pParent->pos.y;
            w->item.pModel->pos.z -= w->pParent->pos.z;
            w->item.pModel->pos.x *= inv.x;
            w->item.pModel->pos.y *= inv.y;
            w->item.pModel->pos.z *= inv.z;
            sceAtSetItemModelParent(w);
        }
    }
    return 1;
}

int SceAtSetParent(int no, cObj* obj, int flag)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "sceAtSetParent(): AT NOT FOUND");
        return 0;
    }
    return SceAtSetParent(w, obj, flag);
}

// Dead-stripped by the original linker (STRIP_UNUSED): only its constant pool (1e10) survives in
// `.rodata` between SceAtSetParent(int, cObj*, int) and InScreenCheck.
static int sceAtFarCheck(Vec* a, Vec* b)
{
    f32 dx = a->x - b->x;
    f32 dz = a->z - b->z;

    if (dx * dx + dz * dz > 10000000000.0f) {
        return 1;
    }
    return 0;
}

int InScreenCheck(Vec* pos)
{
    Vec scr;
    Vec p = *pos;

    GetScreenPos(&p, &scr);
    if (Screen.width * 0.25f < scr.x && Screen.width * 0.75f > scr.x && Screen.height * 0.1f < scr.y &&
        Screen.height * 0.9f > scr.y) {
        return 1;
    }
    return 0;
}

void SceAtExecRoomJump(u16 room, Vec* pos, Vec* rot, int a)
{
    SceAtWork w;

    w.x52 = 0;
    w.dstStage = room >> 8;
    w.dstRoom = room;
    w.dstPos.x = pos->x;
    w.dstPos.y = pos->y;
    w.dstPos.z = pos->z;
    w.dstAngle = rot->y;
    w.dstX4F9E = a;
    w.doorFunc = 0;
    sceAtFunc_door(&w, 0);
}

SceAtField* SceAtCheckFieldInfo(Vec* pos)
{
    SceAtWork* w;

    if (pS == 0) {
        return 0;
    }
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->flag)) {
            continue;
        }
        if (w->x35 != 0xD) {
            continue;
        }
        if (sceAtHitCheck(w, 0, pos, pos) == 1) {
            return &w->field;
        }
    }
    return 0;
}

int SceAtCheckLadder(cModel* m, Vec* pos, f32* ang, u8* level)
{
    SceAtWork* w;
    SceAtLadder* l;
    Vec* mp;

    if (pS == 0) {
        return 0;
    }
    if (m == 0) {
        return 0;
    }
    w = sceAtSetOtStart();
    mp = &m->pos;
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->flag)) {
            continue;
        }
        if (w->x35 != 0x10) {
            continue;
        }
        if (sceAtHitCheck(w, 0, mp, mp) != 1) {
            continue;
        }
        l = &w->ladder;
        if (sceAtCheckLadderUp(l, m) == 1) {
            sceAtGetLadderPos(l, pos, ang);
            *level = w->ladder.level;
            return 1;
        }
    }
    return 0;
}

int SceAtSearchLadder(cModel* m, Vec* pos, f32* ang, u8* level)
{
    SceAtWork* w;
    SceAtLadder* l;
    SceAtLadder* found;
    f32 best;
    f32 d;

    if (pS == 0 || m == 0) {
        return 0;
    }
    best = 4000000.0f;
    found = 0;
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->flag)) {
            continue;
        }
        if (!(w->x35 & 0x10)) {
            continue;
        }
        l = &w->ladder;
        if (sceAtCheckLadderUp(l, m) != 1) {
            continue;
        }
        d = PSVECSquareDistance(&m->pos, &l->pos);
        if (best > d) {
            best = d;
            found = l;
        }
    }
    if (found == 0) {
        return 0;
    }
    sceAtGetLadderPos(found, pos, ang);
    *level = found->level;
    return 1;
}

static void sceAtCamCtrlCheck()
{
    static SceAtCamCtrl auto_work;
    SceAtCamCtrl* found = 0;
    SceAtCamCtrl* c;
    SceAtWork* w;
    f32 best = 100000000.0f;
    f32 r;
    f32 r2;
    f32 d;
    f32 dy;
    f32 a;
    int ret;

    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->flag)) {
            continue;
        }
        if (w->x35 != 0xC) {
            continue;
        }
        c = &w->cam;
        // `r * r` in both arms: jump2 cross-jumps the shared `fmuls` into the join, ahead of the pPL load.
        if (pS->pCamAt == c) {
            r = pS->pCamAt->range + pS->pCamAt->range2;
            r2 = r * r;
        } else {
            r = c->range;
            r2 = r * r;
        }
        d = PSVECSquareDistance(&pPL->pos, &c->pos);
        if (!(r2 > d)) {
            continue;
        }
        if (!(best > d)) {
            continue;
        }
        dy = pPL->pos.y - c->pos.y;
        if (dy * dy > 250000.0f) {
            continue;
        }
        switch (c->mode) {
        case 0:
            a = LIMIT_ANGLE(c->angle - pPL->rot.y);
            if (pS->pCamAt == c) {
                if (a < -1.3962634f || a > 1.3962634f) {
                    continue;
                }
            } else {
                if (a < -1.0471976f || a > 1.0471976f) {
                    continue;
                }
            }
            break;
        case 1:
            a = GetXZAngleLocal(&pPL->pos, &c->pos, pPL->rot.y);
            if (pS->pCamAt == c) {
                if (a < -2.3561945f || a > 0.17453292f) {
                    continue;
                }
            } else {
                if (a < -1.5707964f || a > -0.17453292f) {
                    continue;
                }
            }
            break;
        }
        best = d;
        found = c;
    }
    if (found != 0) {
        RAW_U32(pS, 0x120) = (u32) found;
        CamCtrl.qfps.LRinfo(found);
        return;
    }
    ret = PlCornerCheck();
    if (ret < 0) {
        return;
    }
    if (ret > 1) {
        if (ret == 2) {
            RAW_U32(pS, 0x120) = (u32) &auto_work;
            auto_work.pos = pPL->pos;
            auto_work.angle = pPL->rot.y;
            CamCtrl.qfps.LRinfo(&auto_work);
        }
    } else {
        if (pS->pCamAt != 0) {
            f32 lim = 250000.0f;

            if (lim < PSVECSquareDistance(&pPL->pos, &pS->pCamAt->pos)) {
                RAW_U32(pS, 0x120) = 0;
            } else {
                a = LIMIT_ANGLE(pS->pCamAt->angle - pPL->rot.y);
                if (a < -1.2217305f || a > 1.2217305f) {
                    RAW_U32(pS, 0x120) = 0;
                }
            }
        }
        CamCtrl.qfps.LRinfo(pS->pCamAt);
    }
}

static void sceAtDebugDisp()
{
    AreaData eye;
    Mtx mat;
    Mtx pmat;
    SceAtWork* w;
    // COMPILER-DIFF: gcse PRE pseudo numbering. One extra pseudo before the matrix copies: without it the
    // second copy's `s_ + 16` / `d_ + 16` expressions (regs 123/124) hash to buckets 76/0 of the 77-bucket
    // table, so the dst giv is numbered (and allocated, r8) before the src giv; the original has src in r8.
    int dead = 0;

    if (pG->debug_mode != 0x11 && !(pG->flags_60 & 0x00400000)) {
        return;
    }
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->flag)) {
            continue;
        }
        eprintf(0xC8, 0x20, 0, 0x11, "[SCENARIO ATARI VIEW]");
        if (w->pParent == 0) {
            AreaDataDisp(&w->area, 0x80808080, 1, 0);
        } else {
            if (w->parentParts >= 0) {
                MTX_COPY(w->pParent->getPartsPtr(w->parentParts)->mat, pmat);
            } else {
                MTX_COPY(w->pParent->mat, pmat);
            }
            if (w->flag & 8) {
                Vec zero = { 0.0f, 0.0f, 0.0f };
                low_RotMatrix(mat, &zero);
                mat[0][3] = pmat[0][3];
                mat[1][3] = pmat[1][3];
                mat[2][3] = pmat[2][3];
            } else {
                MTX_COPY(pmat, mat);
            }
            AreaDataDisp(&w->area, 0x80808080, 1, mat);
        }
        if (w->x35 == 3 && (w->item.flag & 1)) {
            SceAtDataEyeTriggreCopy(&eye, w);
            AreaDataDisp(&eye, 0x80808080, 1, 0);
        }
    }
}

void SceAtDataEyeTriggreCopy(AreaData* out, SceAtWork* w)
{
    SceAtItem* it;

    out->flag = 1;
    out->type = 3;
    if (w->x35 != 3) {
        return;
    }
    it = &w->item;
    out->u.eye.y = it->pos.y;
    out->u.eye.r = 100.0f;
    out->u.eye.x = w->item.pos.x;
    out->u.eye.z = it->pos.z;
    out->u.eye.ang_x = it->rot.x;
    out->u.eye.ang_y = it->rot.y;
    out->u.eye.open = it->rot.z;
}

static void sceAtItemFindCheck()
{
    Vec pos;
    Vec rot;
    SceAtWork* w = sceAtSetOtStart();
    SceAtItem* it;
    int off;
    cModel* pm;

    while ((w = sceAtGetOtAddr(w)) != 0) {
        off = !(w->flag & 1);
        if (off) {
            continue;
        }
        if (w->x35 != 3) {
            continue;
        }
        it = &w->item;
        sceAtItemFindFlgCk(it);
        if ((it->flag2 & 0x10) && it->pModel != 0) {
            cEmItem* em = (cEmItem*) it->pModel;

            if (em->ckStatus() == 3 && it->seDamage != 0) {
                pos = it->pModel->pos;
                pos.y -= 300.0f;
                SndCall(6, it->seDamage, &pos, 0, 0, 0);
                it->seDamage = off;
            }
            if (em->ckStatus() == 1) {
                it->flag2 &= ~0x10;
                it->pos = it->pModel->pos;
                it->pos.y += 100.0f;
                sceAtItemFindFlgOn(it);
                if (it->seFind != 0) {
                    pos = it->pModel->pos;
                    pos.y += 300.0f;
                    SndCall(6, it->seFind, &pos, 0, 0, 0);
                }
                if (!(it->flag2 & 2)) {
                    SceAtItemAutoArea(&w->area, &em->pos, it->size);
                }
                pm = it->pModel;
                if (pm != 0) {
                    Vec* rp = &rot;

                    rp->x = 0.0f;
                    rp->y = 0.0f;
                    rp->z = 0.0f;
                    pm->setAng(rp);
                }
                sceAtItemEffDelete(it);
                it->effType = 2;
                sceAtItemEffSet(w, it->pModel);
                sceAtCheckItemModelParent(w);
            }
        }
        if ((it->flag2 & 0x40) && it->pModel != 0) {
            cEm* m = (cEm*) w->item.pModel;
            f32 fl = EatMgr.getFloor(&m->pos, 0.0f, 100000.0f, 0, 0);

            m->pos.y -= m->x328.y;
            m->x328.y += 10.0f;
            if (m->pos.y < fl) {
                m->pos.y = fl;
                it->flag2 &= ~0x40;
                sceAtItemFindFlgOn(it);
                if (it->seFind != 0) {
                    pos = it->pModel->pos;
                    pos.y += 300.0f;
                    SndCall(6, it->seFind, &pos, 0, 0, 0);
                }
                if (!(it->flag2 & 2)) {
                    SceAtItemAutoArea(&w->area, &m->pos, it->size);
                }
                if (it->pModel != 0) {
                    it->pModel->rot.x = 0.0f;
                    it->pModel->rot.y = 0.0f;
                    it->pModel->rot.z = 0.0f;
                }
                sceAtItemEffDelete(it);
                it->effType = 2;
                sceAtItemEffSet(w, it->pModel);
                sceAtCheckItemModelParent(w);
            }
        }
        if (it->flag2 & 0x20) {
            if (it->timer != 0) {
                if (pG->flags_51E4 % 30 == 0) {
                    it->timer--;
                    if (it->timer == 6) {
                        sceAtItemEffDelete(it);
                        sceAtItemDisappearEffSet(w, 0);
                    }
                }
            } else {
                w->item.flag2 &= ~0x20;
                SceAtSetEnable(w->no, 0);
                if (w->flag & 4) {
                    Mem_free(w);
                    DelPrim(&pS->ot[15], (u32*) w);
                }
            }
        }
    }
}

void SceAtItemFlgOn(u16 flagNo, u16 saveNo)
{
    if (flagNo != 0) {
        itemFlags()[flagNo >> 5] |= 0x80000000 >> (flagNo & 31);
    } else if (saveNo != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            roomItemFlags()[saveNo >> 5] |= 0x80000000 >> (saveNo & 31);
        }
    }
}

int SceAtItemFlgCk(u16 flagNo, u16 saveNo)
{
    u32 r = 0;

    if (flagNo != 0) {
        r = itemFlags()[flagNo >> 5] & (0x80000000 >> (flagNo & 31));
    } else if (saveNo != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            r = roomItemFlags()[saveNo >> 5] & (0x80000000 >> (saveNo & 31));
        }
    }
    if (r != 0) {
        return 1;
    }
    return 0;
}

int SceAtItemFlgCk(int no)
{
    SceAtWork* w = SceAtPtr(no);

    if (w != 0 && w->x35 == 3) {
        return sceAtItemFlgCk(&w->item);
    }
    return 0;
}

int SceAtItemFindFlgCk(int no)
{
    SceAtWork* w = SceAtPtr(no);

    if (w != 0 && w->x35 == 3) {
        return sceAtItemFindFlgCk(&w->item);
    }
    return 0;
}

void sceAtItemFlgOn(SceAtItem* it)
{
    u16 no = it->flagNo;

    if (no != 0) {
        itemFlags()[no >> 5] |= 0x80000000 >> (no & 31);
    } else if (it->findFlagNo != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            roomItemFlags()[it->findFlagNo >> 5] |= 0x80000000 >> (it->findFlagNo & 31);
        }
    }
}

int sceAtItemFlgCk(SceAtItem* it)
{
    u32 r = 0;
    u16 no = it->flagNo;

    if (no != 0) {
        r = itemFlags()[no >> 5] & (0x80000000 >> (no & 31));
    } else if (it->findFlagNo != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            r = roomItemFlags()[it->findFlagNo >> 5] & (0x80000000 >> (it->findFlagNo & 31));
        }
    }
    if (r != 0) {
        return 1;
    }
    return 0;
}

void sceAtItemFindFlgOn(SceAtItem* it)
{
    u16 no = it->flagNo;

    if (no != 0) {
        itemFindFlags()[no >> 5] |= 0x80000000 >> (no & 31);
    } else if (it->findFlagNo != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            roomItemFindFlags()[it->findFlagNo >> 5] |= 0x80000000 >> (it->findFlagNo & 31);
        }
    }
}

int sceAtItemFindFlgCk(SceAtItem* it)
{
    u32 r = 0;
    u16 no = it->flagNo;

    if (no != 0) {
        r = itemFindFlags()[no >> 5] & (0x80000000 >> (no & 31));
    } else if (it->findFlagNo != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            r = roomItemFindFlags()[it->findFlagNo >> 5] & (0x80000000 >> (it->findFlagNo & 31));
        }
    }
    if (r != 0) {
        return 1;
    }
    return 0;
}

int SceAtDestroy(int no)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtDestroy(): AT NOT FOUND");
        return 0;
    }
    SceAtSetEnable(w->no, 0);
    DelPrim(&pS->ot[15], (u32*) w);
    if (w->flag & 4) {
        Mem_free(w);
    }
    return 1;
}

#line 3850 "D:/Bio4/Prog/sce_at.cpp"
int SceAtCreateExecAt(cModel* m, Vec* pos, int a, int b, int c, f32 h, int d, f32 ang, f32 range, int e, int prio, TaskFunc func, int arg, u8 flag)
{
    SceAtWork* w;

#line 3859 "D:/Bio4/Prog/sce_at.cpp"
    w = (SceAtWork*) MEM_CALLOC(sizeof(SceAtWork), 1, 13);
    if (w == 0) {
        return -1;
    }
    if (sceAtPullAtNo(&w->no) == 0) {
        return -1;
    }
    w->x37 = a;
    w->x38 = b;
    w->x39 = c;
    w->x4A = e;
    w->x44 = d;
    w->prio = prio;
    w->x45 = flag;
    w->pParent = m;
    w->func = func;
    w->arg = arg;
    w->flag = 7;
    w->x35 = 2;
    w->parentParts = -1;
    w->angle = (s8) (ang * 0.5f * 57.295776f);
    w->angleRange = (s8) (range * 0.5f * 57.295776f);
    AreaDataInit(&w->area, &m->pos, 1, 1500.0f, h);
    w->area.u.xz4.y = (pos[0].y + pos[1].y + pos[2].y + pos[3].y) * 0.25f;
    w->area.u.xz4.p[0].x = pos[0].x;
    w->area.u.xz4.p[0].z = pos[0].z;
    w->area.u.xz4.p[1].x = pos[1].x;
    w->area.u.xz4.p[1].z = pos[1].z;
    w->area.u.xz4.p[2].x = pos[2].x;
    w->area.u.xz4.p[2].z = pos[2].z;
    w->area.u.xz4.p[3].x = pos[3].x;
    w->area.u.xz4.p[3].z = pos[3].z;
    AddPrim(&pSS->ot[w->x44], (u32*) w);
    return w->no;
}

#line 3936 "D:/Bio4/Prog/sce_at.cpp"
int SceAtCreateFieldAt(cModel* m, Vec* pos, int a, int b, int c, f32 h, int d, f32 ang, int e, f32 range, int val, SceAtField** out)
{
    SceAtWork* w;

#line 3946 "D:/Bio4/Prog/sce_at.cpp"
    w = (SceAtWork*) MEM_CALLOC(sizeof(SceAtWork), 1, 13);
    *out = 0;
    if (w == 0) {
        return -1;
    }
    if (sceAtPullAtNo(&w->no) == 0) {
        return -1;
    }
    w->x37 = a;
    w->x38 = b;
    w->x39 = c;
    w->x4A = e;
    w->x44 = d;
    w->prio = 0;
    w->func = 0;
    w->arg = 0;
    w->x45 = 0;
    w->flag = 7;
    w->x35 = 0xD;
    w->parentParts = -1;
    w->pParent = m;
    w->angle = (s8) (ang * 0.5f * 57.295776f);
    w->angleRange = (s8) (range * 0.5f * 57.295776f);
    AreaDataInit(&w->area, &m->pos, 1, 1500.0f, h);
    w->area.u.xz4.y = (pos[0].y + pos[1].y + pos[2].y + pos[3].y) * 0.25f;
    w->area.u.xz4.p[0].x = pos[0].x;
    w->area.u.xz4.p[0].z = pos[0].z;
    w->area.u.xz4.p[1].x = pos[1].x;
    w->area.u.xz4.p[1].z = pos[1].z;
    w->area.u.xz4.p[2].x = pos[2].x;
    w->area.u.xz4.p[2].z = pos[2].z;
    w->area.u.xz4.p[3].x = pos[3].x;
    w->area.u.xz4.p[3].z = pos[3].z;
    w->field.value = val;
    w->field.pModel = m;
    AddPrim(&pSS->ot[w->x44], (u32*) w);
    *out = &w->field;
    return w->no;
}

#line 3995 "D:/Bio4/Prog/sce_at.cpp"
int SceAtCreateItemAt(Vec* pos, u16 id, int num, int effType, int saveNo, cModel* parent, int parts)
{
    SceAtWork* w;
    void* bin;
    void* tpl;
    int ok;
    cObj* obj;

#line 4005 "D:/Bio4/Prog/sce_at.cpp"
    w = (SceAtWork*) MEM_CALLOC(sizeof(SceAtWork), 1, 13);
    if (w == 0) {
        return -1;
    }
    if (sceAtPullAtNo(&w->no) == 0) {
        return -1;
    }
    if (saveNo < 0) {
        if (sceAtCheckSaveItem(id) == 1) {
            saveNo = sceAtPullItemSaveWork();
            if (saveNo >= 0) {
                SAVE_ITEM_ROOM(saveNo) = pG->room_id;
                SAVE_ITEM_TYPE(saveNo) = 0;
                SAVE_ITEM_ATNO(saveNo) = 0;
                SAVE_ITEM_ID(saveNo) = id;
                SAVE_ITEM_NUM(saveNo) = num;
                SAVE_ITEM_EFF(saveNo) = effType;
                SAVE_ITEM_POS(saveNo, 0) = (s16) (pos->x / 10.0f);
                SAVE_ITEM_POS(saveNo, 1) = (s16) (pos->y / 10.0f);
                SAVE_ITEM_POS(saveNo, 2) = (s16) (pos->z / 10.0f);
                w->item.flag2 |= 8;
            } else {
                pLog->err(0, 0, "SceAtCreateItemAt(): lack save work");
            }
        } else if (saveNo == -1) {
            w->item.timer = 0x3D;
            w->item.flag2 |= 0x20;
        }
    } else {
        w->item.flag2 |= 8;
    }
    w->pParent = parent;
    w->parentParts = parts;
    w->flag = 7;
    w->x37 |= 1;
    w->x35 = 3;
    w->x39 = 1;
    w->x38 = 8;
    w->x44 = 8;
    w->x4A = 0x28;
    SceAtItemAutoArea(&w->area, pos, 0.0f);
    w->item.num = num;
    w->item.id = id;
    w->item.flagNo = 0;
    w->item.findFlagNo = 0;
    switch (effType) {
    case -1:
    case 0:
    case 6:
        w->item.effType = sceAtCheckItemEffectCol(id);
        break;
    default:
        w->item.effType = effType;
        break;
    }
    w->item.pModel = 0;
    w->item.saveNo = saveNo;
    if (w->item.effType == 8) {
        w->item.pos.x = pos->x;
        w->item.pos.y = pos->y + 1000.0f;
        w->item.pos.z = pos->z;
    } else {
        w->item.pos.x = pos->x;
        w->item.pos.y = pos->y + 100.0f;
        w->item.pos.z = pos->z;
    }
    ok = ItemGetBinTplAddr(id, &bin, &tpl) ? 1 : 0;
    if (ok == 1) {
        obj = setItemObj(bin, tpl, (Vec*) &vecZero, (Vec*) &vecZero);
        SceAtSetItemModel(w, obj);
    }
    if (w->item.effType != 8 && w->item.pModel != 0) {
        w->item.pModel->be_flag &= ~2;
    }
    sceAtCheckItemModelParent(w);
    sceAtItemEffSet(w, 0);
    AddPrim(&pS->ot[w->x44], (u32*) w);
    return w->no;
}

void SceAtReserveItemAt(int key, Vec* pos, u16 id, int num, int effType, int saveNo)
{
    int i;

    if (saveNo >= 0) {
        return;
    }
    if (sceAtCheckSaveItem(id) != 1) {
        return;
    }
    for (i = 0; i < 16; i++) {
        if (SceAtSys.reserve[i].key == key) {
            return;
        }
    }
    for (i = 0; i < 16; i++) {
        if (SceAtSys.reserve[i].key == 0) {
            break;
        }
    }
    if (i == 16) {
        pLog->err(0, 0, "SceAtReserveItemAt(): reserve work over");
        return;
    }
    saveNo = sceAtPullItemSaveWork();
    SceAtSys.reserve[i].saveNo = saveNo;
    SceAtSys.reserve[i].key = key;
    if (saveNo >= 0) {
        SAVE_ITEM_ROOM(saveNo) = pG->room_id;
        SAVE_ITEM_TYPE(saveNo) = 0;
        SAVE_ITEM_ATNO(saveNo) = 0;
        SAVE_ITEM_ID(saveNo) = id;
        SAVE_ITEM_NUM(saveNo) = num;
        SAVE_ITEM_EFF(saveNo) = effType;
        SAVE_ITEM_POS(saveNo, 0) = (s16) (pos->x / 10.0f);
        SAVE_ITEM_POS(saveNo, 1) = (s16) (pos->y / 10.0f);
        SAVE_ITEM_POS(saveNo, 2) = (s16) (pos->z / 10.0f);
    } else {
        pLog->err(0, 0, "SceAtReserveItemAt(): save work over");
    }
}

void SceAtCancelItemAt(int key)
{
    int i;

    for (i = 0; i <= 15; i++) {
        if (SceAtSys.reserve[i].key == key) {
            memclr_asm(&pG->save_item[SceAtSys.reserve[i].saveNo], sizeof(SceAtSaveItem));
            SceAtSys.reserve[i].saveNo = SceAtSys.reserve[i].key = 0;
            break;
        }
    }
}

int sceAtCheckItemEffectCol(u16 id)
{
    ItemInfo info;

    if (id == 0x8C) {
        return 8;
    }
    itemInfo(id, &info);
    switch (info.type) {
    case 1:
    case 2:
    case 3:
    case 4:
        return 5;
    case 6:
        return 4;
    case 0:
    case 5:
    case 7:
        return 2;
    case 8:
        return 3;
    case 0xC:
        return 3;
    default:
        return 3;
    }
}

int sceAtCheckSaveItem(u16 id)
{
    ItemInfo info;

    itemInfo(id, &info);
    if (info.type == 5 || info.type == 7) {
        return 1;
    }
    return 0;
}

// Never emitted (only its string literal reaches .rodata, ahead of SceAtLinkEtcDead's).
static inline void SceAtLinkEmFlag(int no)
{
    if (SceAtPtr(no) == 0) {
        pLog->err(0, 0, "SceAtLinkEmFlag(): AT NOT FOUND");
    }
}

void SceAtLinkEtcDead(int no, int etcNo, int on)
{
    cEm* em;
    SceAtWork* w;

    if (0) {
        SceAtLinkEmFlag(no);
    }
    w = SceAtPtr(no);
    if (w == 0) {
        pLog->err(0, 0, "SceAtLinkEtcDead(): AT NOT FOUND");
        return;
    }
    if (getRoomEtcBreak(etcNo, &em, 1) != 1) {
        return;
    }
    if (bitOff(*GetEtcFlgPtr(etcNo, pG->room_id))) {
        w->linkNo = etcNo;
        w->linkType = 2;
        if (on == 1) {
            SceAtSetEnable(no, 0);
        } else {
            SceAtSetEnable(no, 1);
        }
    } else {
        w->linkType = 0;
        w->linkNo = 0;
        if (on == 1) {
            SceAtSetEnable(no, 1);
        } else {
            SceAtSetEnable(no, 0);
        }
    }
}

void sceAtLink_check()
{
    cEm* em;
    SceAtWork* w = sceAtSetOtStart();
    int flag;

    while ((w = sceAtGetOtAddr(w)) != 0) {
        switch (w->linkType) {
        case 0:
            break;
        case 1:
            em = GetEmPtrFromList(w->linkNo);
            if (em != 0) {
                if (w->x35 == 3) {
                    flag = em->checkStatus(8) == 1;
                } else {
                    flag = em->checkStatus(5) == 0;
                }
            } else {
                u32 d;
                int no = w->linkNo;

                if (pG->emlist_no >= 0) {
                    d = EM_DEAD_BIT(pG->emlist_no, no >> 5) & (0x80000000 >> (no & 31));
                } else {
                    d = 0;
                }
                flag = 0;
                if (d != 0) {
                    flag = 1;
                }
            }
            if (flag == 1) {
                if (w->flag & 1) {
                    SceAtSetEnable(w->no, 0);
                } else if (w->x35 == 3) {
                    if (w->item.effType == 0 || w->item.effType == 6) {
                        w->item.effType = sceAtCheckItemEffectCol(w->item.id);
                    }
                    if (sceAtCheckSaveItem(w->item.id) == 0) {
                        SceAtSetEnable(w->no, 1);
                        sceAtItemFlgOn(&w->item);
                        w->item.timer = 0x3D;
                        w->item.flag2 |= 0x20;
                    } else {
                        SceAtSetEnable(w->no, 1);
                    }
                } else {
                    SceAtSetEnable(w->no, 1);
                }
                w->linkType = 0;
                w->linkNo = 0;
            }
            if (w->x35 == 3 && bitOff(w->item.flag2)) {
                em = GetEmPtrFromList(w->linkNo);
                if (em != 0 && bitOff(w->item.flag2)) {
                    SceAtSetEmItem(em, w);
                }
            }
            break;
        case 2: {
            cEm* etc;

            if (getRoomEtcBreak(w->linkNo, &etc, 0) == 1) {
                if (*GetEtcFlgPtr(w->linkNo, pG->room_id) & 1) {
                    if (w->flag & 1) {
                        SceAtSetEnable(w->no, 0);
                    } else {
                        SceAtSetEnable(w->no, 1);
                    }
                    w->linkType = 0;
                    w->linkNo = 0;
                }
            } else {
                pLog->err(4, 0, "ITEM SET[%d] failed: ETC[%d] not found", w->no - 0x80, w->linkNo);
            }
            break;
        }
        }
    }
}

int SceAtSetEmItem(cEm* em, int no)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        return 0;
    }
    return SceAtSetEmItem(em, w);
}

int SceAtSetEmItem(cEm* em, SceAtWork* w)
{
    if (em == 0) {
        return 0;
    }
    EM_SET_ITEM(em, w->item.id, w->item.num, w->item.flagNo, w->item.findFlagNo, (s8) w->item.effType);
    w->linkType = 0;
    return 1;
}

void SceAtSetSaveItem()
{
    Vec pos;
    int i;
    SceAtWork* w;

    for (i = 0; i <= 0xFF; i++) {
        if (SAVE_ITEM_ROOM(i) == 0) {
            continue;
        }
        if (SAVE_ITEM_ROOM(i) != pG->room_id) {
            continue;
        }
        switch (SAVE_ITEM_TYPE(i)) {
        case 0:
            pos.x = (f32) SAVE_ITEM_POS(i, 0) * 10.0f;
            pos.y = (f32) SAVE_ITEM_POS(i, 1) * 10.0f;
            pos.z = (f32) SAVE_ITEM_POS(i, 2) * 10.0f;
            SceAtCreateItemAt(&pos, SAVE_ITEM_ID(i), SAVE_ITEM_NUM(i), SAVE_ITEM_EFF(i), i, 0, -1);
            break;
        case 1:
            w = SceAtPtr(SAVE_ITEM_ATNO(i));
            U16Set(w->item.id, SAVE_ITEM_ID(i));
            w->item.num = SAVE_ITEM_NUM(i);
            w->item.flag2 |= 8;
            w->item.saveNo = i;
            break;
        }
    }
}

int sceAtPullItemSaveWork()
{
    int i;

    for (i = 0; i < 256; i++) {
        if (saveItemTbl()[i].room == 0) {
            return i;
        }
    }
    return -1;
}

void SceAtInitSaveItem()
{
    memclr_asm(pG->save_item, sizeof(pG->save_item));
}

int SceAtCheckSaveItemId(int id)
{
    int i;

    for (i = 0; i < 256; i++) {
        if (SAVE_ITEM_ROOM(i) != 0 && SAVE_ITEM_ID(i) == id) {
            return 1;
        }
    }
    return 0;
}

void sceAtCheckItemModelParent(SceAtWork* w)
{
    SceAtWork* p;
    SceAtItem* it;

    if (w->pParent != 0) {
        return;
    }
    p = sceAtSetOtStart();
    it = &w->item;
    while ((p = sceAtGetOtAddr(p)) != 0) {
        if (bitOff(p->flag)) {
            continue;
        }
        if (p->x35 != 0x14) {
            continue;
        }
        if (p->pParent == 0) {
            continue;
        }
        if (sceAtHitCheck(p, 0, &it->pos, &it->pos) != 1) {
            continue;
        }
        SceAtSetParent(w, p->pParent, 0);
    }
}

void sceAtSetItemModelParent(SceAtWork* w)
{
    if (w->pParent == 0) {
        return;
    }
    Vec inv = { 0.0f, 0.0f, 0.0f };
    if (w->pParent->scale.x != 0.0f) {
        inv.x = 1.0f / w->pParent->scale.x;
    }
    if (w->pParent->scale.y != 0.0f) {
        inv.y = 1.0f / w->pParent->scale.y;
    }
    if (w->pParent->scale.z != 0.0f) {
        inv.z = 1.0f / w->pParent->scale.z;
    }
    w->item.pModel->be_flag &= ~0x4000;
    w->item.pModel->pParts->scale = inv;
    w->item.pModel->setParent(w->pParent, &w->item.pModel->pos, &w->item.pModel->rot);
}

int SceAtSetItemModel(int no, cModel* m)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtSetItemModel(): AT NOT FOUND");
        return 0;
    }
    SceAtSetItemModel(w, m);
    return 1;
}

int SceAtSetItemModel(SceAtWork* w, cModel* m)
{
    if (w == 0) {
        pLog->err(0, 0, "SceAtSetItemModel(): pData == NULL");
        return 0;
    }
    if (m == 0) {
        pLog->err(0, 0, "SceAtSetItemModel(): pObj == NULL");
        return 0;
    }
    w->item.pModel = m;
    if (w->item.id == 0xAF) {
        m->x12F = 1;
    }
    m->pos.x = w->item.pos.x;
    m->pos.y = w->item.pos.y;
    m->pos.z = w->item.pos.z;
    if (w->item.rot.z > 0.0f) {
        m->rot.x = w->item.rot.x;
        m->rot.y = w->item.rot.y;
        m->rot.z = 0.0f;
    }
    m->setNoSuspend(0);
    sceAtSetItemModelParent(w);
    return 1;
}

int SceAtSetShootDownItem(SceAtWork* w, void* bin, void* tpl)
{
    Vec rot = { 0.0f, 0.0f, 0.0f };
    cEmItem* em;

    if (w->item.rot.z > 0.0f) {
        rot.x = w->item.rot.x;
        rot.y = w->item.rot.y;
    }
    em = SetEmItem(bin, tpl, &w->item.pos, &rot, 0, 0);
    if (em == 0) {
        w->item.pModel = em;
        return 0;
    }
    switch (w->item.id) {
    case 0x58:
    case 0x59:
        YarareInitCube(em, 0.0f, -85.0f, 0.0f, 85.0f, 170.0f, 300.0f, 0, 1);
        break;
    }
    em->setNoSuspend(1);
    w->item.pModel = em;
    return 1;
}

cModel* SceAtItemModelPtr(int no)
{
    SceAtWork* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtItemModelPtr(): AT NOT FOUND");
        return 0;
    }
    if (w->x35 != 3) {
        pLog->err(0, 0, "SceAtItemModelPtr(): not ID == ITEM");
        return 0;
    }
    return w->item.pModel;
}

int SceAtItemHitCheck(SceAtWork* w, Vec* pos)
{
    Vec p;
    SceAtItem* it = &w->item;

    if (it->flag2 & 0x10) {
        return 0;
    }
    if (it->flag2 & 4) {
        return 1;
    }
    if (pos == 0) {
        p = it->pos;
        if (w->pParent != 0) {
            Mtx mat;
            Mtx pmat;

            if (w->parentParts >= 0) {
                MTX_COPY(w->pParent->getPartsPtr(w->parentParts)->mat, pmat);
            } else {
                MTX_COPY(w->pParent->mat, pmat);
            }
            if (w->flag & 8) {
                Vec zero = { 0.0f, 0.0f, 0.0f };
                low_RotMatrix(mat, &zero);
                mat[0][3] = pmat[0][3];
                mat[1][3] = pmat[1][3];
                mat[2][3] = pmat[2][3];
            } else {
                MTX_COPY(pmat, mat);
            }
            PSMTXMultVec(mat, &p, &p);
        }
    } else {
        p = *pos;
    }
    {
        Vec pl;
        Vec q;

        pl = pPL->pos;
        q = p;
        pl.y = q.y = q.y + 200.0f;
        if (EatMgr.hitCheck(&q, &pl, 0, 0, 0, 0) == 0) {
            return 1;
        }
        pl = pPL->pos;
        q = p;
        pl.y = q.y = q.y + 600.0f;
        if (EatMgr.hitCheck(&q, &pl, 0, 0, 0, 0) == 0) {
            return 1;
        }
        pl = pPL->pos;
        q = p;
        q.y += 200.0f;
        pl.y += 1800.0f;
        return EatMgr.hitCheck(&q, &pl, 0, 0, 0, 0) == 0;
    }
}

int SceAtCheckSystemItemSet(u32 id, int* outId, int* outNum, Vec* pos, Vec* rot)
{
    EmListData d;
    int num;
    int no;

    switch (id) {
    case 0x1000:
        d.id = 0x24;
        d.type = 0;
        d.x3 = 0;
        d.flags4 = 1;
        d.pos[0] = (s16) (pos->x / 10.0f);
        d.pos[1] = (s16) (pos->y / 10.0f);
        d.pos[2] = (s16) (pos->z / 10.0f);
        if (rot->z > 0.0f) {
            d.rot[0] = (s16) (rot->x * 57.295776f) * 0x8000 / 360;
            d.rot[1] = (s16) (rot->y * 57.295776f) * 0x8000 / 360;
            d.rot[2] = 0;
        } else {
            d.rot[0] = 0;
            d.rot[1] = 0;
            d.rot[2] = 0;
        }
        d.hp = 1000;
        d.x1A = 1;
        d.xB = 0;
        EmSetEvent(&d);
        goto fail;
    // The fail tail is written out in both RandomItemCk arms: jump2 first cross-jumps each copy into
    // the `fail:` block (fall-through candidate) and only then finds the 0x1002 arm's `b fail` equal
    // to the 0x1001 arm's, so the 0x1001 copy of `bl; cmpwi; beq; b` survives (a shared `goto fail`
    // makes the 0x1001 arm the scanned one and keeps the 0x1002 copy).
    case 0x1001:
        if (RandomItemCk(0x10, outId, outNum, 0) != 1) {
            *outId = 0xFFFF;
            *outNum = 0;
            return 0;
        }
        break;
    case 0x1002:
        if (RandomItemCk(0x10, outId, outNum, 1) != 1) {
            *outId = 0xFFFF;
            *outNum = 0;
            return 0;
        }
        break;
    case 0x1003:
        // The switches run on the result variables (the case-5 `num = 5` of 0x1004 folds into the
        // switch register, 0x1005's arms load straight into `no`).
        no = pG->x4FB8;
        switch (no) {
        default:
        case 0:
            no = 0x18;
            num = 0xF;
            break;
        case 2:
            no = 0x20;
            num = 0x64;
            break;
        case 4:
            no = 0x72;
            num = 0x14;
            break;
        case 3:
        case 5:
            *outId = 1;
            *outNum = 1;
            goto ok;
        }
        *outId = no;
        *outNum = num;
        break;
    case 0x1004:
        num = pG->x4FB8;
        switch (num) {
        default:
        case 0:
            no = 0x18;
            num = 0xA;
            break;
        case 2:
            no = 7;
            num = 0xA;
            break;
        case 4:
            no = 0x72;
            num = 0xA;
            break;
        case 3:
            no = 0x20;
            num = 0x32;
            break;
        case 5:
            no = 0;
            num = 5;
            break;
        }
        *outId = no;
        *outNum = num;
        break;
    case 0x1005:
        no = pG->x4FB8;
        switch (no) {
        case 0:
            no = 4;
            num = 0x14;
            break;
        case 2:
            no = 4;
            num = 0x14;
            break;
        case 4:
            no = 0xE;
            num = 1;
            break;
        case 3:
            no = 0x20;
            num = 0x19;
            break;
        case 5:
            no = 4;
            num = 0x14;
            break;
        default:
            no = 4;
            num = 0x14;
            break;
        }
        *outId = no;
        *outNum = num;
        break;
    default:
        goto other;
    }
ok:
    return 1;
other:
    // The plain-id fallback sits behind the `return 1` in the original layout.
    if (id <= 0xFFF) {
        *outId = id;
        *outNum = 0;
        goto ok;
    }
fail:
    *outId = 0xFFFF;
    *outNum = 0;
    return 0;
}

void sceAtSetItem(SceAtWork* w)
{
    ItemInfo info;
    Vec rot;
    cEm* em = 0;
    int id;
    int num;
    void* bin;
    void* tpl;
    SceAtItem* it = &w->item;
    int ok = 1;
    int mask = 2;
    int r;
    int ok2;
    cObj* obj;

    if (pG->x4FB8 == 0) {
        mask = 1;
    }
    switch (w->linkType) {
    case 1: {
        u32 d;
        int no;
        cEm* p;

        p = GetEmPtrFromList(w->linkNo);
        no = w->linkNo;
        if (pG->emlist_no >= 0) {
            d = EM_DEAD_BIT(pG->emlist_no, no >> 5) & (0x80000000 >> (no & 31));
        } else {
            d = 0;
        }
        if (d == 0) {
            ok = 0;
        } else if (bitOff(it->flag2)) {
            if (p == 0) {
                ok = 0;
            } else {
                it->pos = p->pos;
            }
        }
        break;
    }
    case 2:
        if (getRoomEtcBreak(w->linkNo, &em, 1) == 0) {
            ok = 0;
        } else if (bitOff(*GetEtcFlgPtr(w->linkNo, pG->room_id))) {
            ok = 0;
        } else if (bitOff(it->flag2) && em != 0) {
            it->pos = em->pos;
            it->pos.y += 50.0f;
            if (it->rot.z == 0.0f) {
                RAW_F32(it, 0x30) = 0.0f;
                RAW_F32(it, 0x38) = 1.0f;
                it->rot.y = GetXZAngle(&em->pos, &pPL->pos);
            }
        }
        break;
    }
    if (sceAtItemFlgCk(it) != 0) {
        goto disable;
    }
    if ((it->modeMask & mask) == 0 && it->modeMask != 0) {
        goto disable;
    }
    if (ok != 1) {
        goto disable;
    }
    rot.x = it->rot.x;
    rot.y = it->rot.y;
    rot.z = it->rot.z;
    r = SceAtCheckSystemItemSet(it->id, &id, &num, &it->pos, &rot);
    if (r != 1) {
        w->flag &= ~1;
        sceAtItemFlgOn(it);
        return;
    }
    if (it->id != id) {
        int s;

        it->id = id;
        it->num = num;
        s = sceAtPullItemSaveWork();
        if (s >= 0) {
            SAVE_ITEM_ROOM(s) = pG->room_id;
            SAVE_ITEM_TYPE(s) = r;
            SAVE_ITEM_ATNO(s) = w->no;
            SAVE_ITEM_ID(s) = id;
            SAVE_ITEM_NUM(s) = num;
            SAVE_ITEM_EFF(s) = -1;
        }
    }
    w->linkType = 0;
    w->linkNo = 0;
    if (!(it->flag2 & 0x80)) {
        if (!(w->x38 & 8)) {
            w->x38 = 8;
            w->x4A = 0x28;
        } else {
            w->x38 &= 0x7F;
        }
    } else {
        w->x38 = 2;
    }
    if (bitOff(it->flag2)) {
        if (it->flag2 & 0x10) {
            if (it->pModel != 0) {
                it->pos = it->pModel->pos;
            }
            if (sceAtItemFindFlgCk(it) == 1) {
                it->flag2 &= ~0x10;
                it->pos.y = EatMgr.getFloor(&it->pos, 0.0f, 100000.0f, 0, 0);
                it->rot.z = 0.0f;
                it->effType = 2;
            }
        }
        if (it->flag2 & 0x40) {
            if (sceAtItemFindFlgCk(it) == 1) {
                it->flag2 &= ~0x40;
                it->pos.y = EatMgr.getFloor(&it->pos, 0.0f, 100000.0f, 0, 0);
                it->effType = 2;
            }
        }
    }
    if (!(it->flag2 & 2)) {
        SceAtItemAutoArea(&w->area, &it->pos, it->size);
    }
    if (it->effType == 6) {
        it->effType = sceAtCheckItemEffectCol(it->id);
    }
    if (it->pModel == 0) {
        ok2 = ItemGetBinTplAddr(it->id, &bin, &tpl) ? 1 : 0;
        if (ok2 == 0) {
            bin = (void*) (pG->pArc->ofs_20 + (u32) pG->pArc);
            tpl = (void*) (pG->pArc->ofs_24 + (u32) pG->pArc);
        }
        if (it->flag2 & 0x10) {
            SceAtSetShootDownItem(w, bin, tpl);
            if (it->effNo == 0) {
                sceAtItemEffSet(w, w->item.pModel);
            }
        } else if (it->flag2 & 0x40) {
            obj = setItemObj(bin, tpl, (Vec*) &vecZero, (Vec*) &vecZero);
            SceAtSetItemModel(w, obj);
            if (ok2 == 0 && it->effType == 0) {
                it->effType = 1;
            }
            ((cEm*) w->item.pModel)->x328.y = 0.0f;
            if (it->effNo == 0) {
                sceAtItemEffSet(w, w->item.pModel);
            }
        } else {
            if (ok2 == 1) {
                obj = setItemObj(bin, tpl, (Vec*) &vecZero, (Vec*) &vecZero);
                SceAtSetItemModel(w, obj);
            } else if (it->effType == 0) {
                it->effType = 1;
            }
            if (it->effNo == 0) {
                sceAtItemEffSet(w, 0);
            }
        }
    } else {
        it->pModel->be_flag |= 2;
        if (it->effNo == 0) {
            sceAtItemEffSet(w, 0);
        }
    }
    sceAtCheckItemModelParent(w);
    return;

disable:
    w->flag &= ~1;
    if (it->pModel != 0) {
        it->pModel->be_flag &= ~2;
    }
}

void SceAtItemAutoArea(AreaData* area, Vec* pos, f32 size)
{
    Vec p = *pos;

    p.y -= 2000.0f;
    {
        f32 h = 3000.0f;

        if (size == 0.0f) {
            size = 1500.0f;
        }
        AreaDataInit(area, &p, 2, size + size, h);
    }
}

static void sceAtDeleteItem(SceAtWork* w)
{
    SceAtItem* it = &w->item;

    sceAtItemEffDelete(it);
    if (it->pModel != 0) {
        it->pModel->be_flag &= ~2;
    }
}

void sceAtItemEffDelete(SceAtItem* it)
{
    if (it->effType != 0 && it->effNo != 0) {
        EffectEspDelete(0, it->effNo, 0, 0);
        EffectEspgenDelete(0, it->effNo, 0);
        EffectEfmDelete(0, it->effNo, 0);
        it->effNo = 0;
    }
}

void sceAtItemEffSet(SceAtWork* w, cModel* m)
{
    Vec p;
    Vec q;
    SceAtItem* it = &w->item;
    cModel* parent;
    int c;
    int kind;

    if ((s8) pG->x4 != 0) {
        return;
    }
    it->effNo = 0;
    if (it->effType == 0) {
        return;
    }
    it->effNo = EspPullCoreKind();
    if (it->effNo == 0) {
        return;
    }
    parent = w->pParent;
    if (parent == 0) {
        if (m == 0) {
            p.x = w->item.pos.x + it->ofs.x;
            p.y = it->pos.y + it->ofs.y;
            p.z = it->pos.z + it->ofs.z;
            switch (it->effType) {
            case 1:
                EstSet(0, -1, &p, 0, 0, 0x21, 0xC00, it->effNo, (u32) parent, parent);
                break;
            case 3:
                EstSet(0, -1, &p, 0, 0, 0x2C, 0xC00, it->effNo, (u32) parent, parent);
                break;
            case 5:
                EstSet(0, -1, &p, 0, 0, 0x2F, 0xC00, it->effNo, (u32) parent, parent);
                break;
            case 4:
                EstSet(0, -1, &p, 0, 0, 0x31, 0xC00, it->effNo, (u32) parent, parent);
                break;
            case 2:
                EstSet(0, -1, &p, 0, 0, 0x33, 0xC00, it->effNo, (u32) parent, parent);
                EstSet(0, -1, &p, 0, 0, 0x21, 0xC00, it->effNo, (u32) parent, parent);
                break;
            case 7:
                EstSet(0, -1, &p, 0, 0, 0x46, 0xC00, it->effNo, (u32) parent, parent);
                EstSet(0, -1, &p, 0, 0, 0x21, 0xC00, it->effNo, (u32) parent, parent);
                break;
            case 8:
                q = p;
                q.y -= 800.0f;
                EstSet(0, -1, &q, 0, 0, 0x33, 0xC00, it->effNo, (u32) parent, parent);
                EstSet(0, -1, &p, 0, 0, 0x21, 0xC00, it->effNo, (u32) parent, parent);
                break;
            case 9:
                EstSet(0, -1, &p, 0, 0, 0x4D, 0xC00, it->effNo, (u32) parent, parent);
                break;
            case 6:
                break;
            }
        } else {
            if (it->effType == 1) {
                p.x = it->ofs.x;
                p.y = it->ofs.y;
                p.z = it->ofs.z;
            } else {
                p.x = m->pos.x + it->ofs.x;
                p.y = m->pos.y + it->ofs.y;
                p.z = m->pos.z + it->ofs.z;
            }
            switch (it->effType) {
            case 1:
                EstSet((int) m, -1, &p, 0, 0, 0x2D, 0xC00, it->effNo, 0, 0);
                break;
            case 3:
                EstSet(0, -1, &p, 0, 0, 0x2C, 0xC00, it->effNo, 0, 0);
                break;
            case 5:
                EstSet(0, -1, &p, 0, 0, 0x2F, 0xC00, it->effNo, 0, 0);
                break;
            case 4:
                EstSet(0, -1, &p, 0, 0, 0x31, 0xC00, it->effNo, 0, 0);
                break;
            case 2:
                EstSet(0, -1, &p, 0, 0, 0x33, 0xC00, it->effNo, 0, 0);
                EstSet(0, -1, &p, 0, 0, 0x21, 0xC00, it->effNo, 0, 0);
                break;
            case 7:
                EstSet(0, -1, &p, 0, 0, 0x46, 0xC00, it->effNo, 0, 0);
                EstSet(0, -1, &p, 0, 0, 0x21, 0xC00, it->effNo, 0, 0);
                break;
            case 8:
                q = p;
                q.y -= 800.0f;
                EstSet(0, -1, &q, 0, 0, 0x33, 0xC00, it->effNo, 0, 0);
                EstSet(0, -1, &p, 0, 0, 0x21, 0xC00, it->effNo, 0, 0);
                break;
            case 9:
                EstSet(0, -1, &p, 0, 0, 0x4D, 0xC00, it->effNo, 0, 0);
                break;
            case 6:
                break;
            }
        }
        return;
    }
    switch (w->parentParts) {
    case -1:
    case 0:
        break;
    case 2:
        if (pG->room_id != 0x30F) {
            return;
        }
        break;
    case 4:
    case 8:
        if (pG->room_id != 0x21B) {
            return;
        }
        break;
    default:
        return;
    }
    parent = w->pParent;
    p.x = it->pos.x + it->ofs.x;
    p.y = it->pos.y + it->ofs.y;
    p.z = it->pos.z + it->ofs.z;
    kind = 0;
    c = 0;
    switch (it->effType) {
    case 3:
        switch (w->parentParts) {
        case -1:
        case 0:
            kind = 0x48;
            break;
        case 2:
            c = 1;
            kind = 0x1C;
            break;
        case 4:
            c = 1;
            kind = 6;
            break;
        case 8:
            c = 1;
            kind = 0xC;
            break;
        }
        EstSet((int) parent, -1, &p, 0, c, kind, 0xC00, it->effNo, 0, 0);
        break;
    case 5:
        switch (w->parentParts) {
        case -1:
        case 0:
            kind = 0x49;
            break;
        case 2:
            c = 1;
            kind = 0x1D;
            break;
        case 4:
            c = 1;
            kind = 8;
            break;
        case 8:
            c = 1;
            kind = 0xE;
            break;
        }
        EstSet((int) parent, -1, &p, 0, c, kind, 0xC00, it->effNo, 0, 0);
        break;
    case 4:
        switch (w->parentParts) {
        case -1:
        case 0:
            kind = 0x4A;
            break;
        case 2:
            c = 1;
            kind = 0x1E;
            break;
        case 4:
            c = 1;
            kind = 0xA;
            break;
        case 8:
            c = 1;
            kind = 0x10;
            break;
        }
        EstSet((int) parent, -1, &p, 0, c, kind, 0xC00, it->effNo, 0, 0);
        break;
    case 2:
        switch (w->parentParts) {
        case -1:
        case 0:
            kind = 0x4B;
            break;
        case 2:
            c = 1;
            kind = 0x1F;
            break;
        case 4:
        case 8:
            return;
        }
        EstSet((int) parent, -1, &p, 0, c, kind, 0xC00, it->effNo, 0, 0);
        // fall through
    case 1:
    case 7:
    case 8:
    case 9:
        EstSet((int) parent, -1, &p, 0, 0, 0x2D, 0xC00, it->effNo, 0, 0);
        break;
    case 6:
        break;
    }
}

void sceAtItemDisappearEffSet(SceAtWork* w, cModel* m)
{
    Vec p;
    SceAtItem* it = &w->item;
    int c;
    int kind;

    if ((s8) pG->x4 != 0) {
        return;
    }
    it->effNo = 0;
    if (it->effType == 0) {
        return;
    }
    it->effNo = EspPullCoreKind();
    if (it->effNo == 0) {
        return;
    }
    if (w->pParent == 0) {
        if (m == 0) {
            p.x = w->item.pos.x + it->ofs.x;
            p.y = it->pos.y + it->ofs.y;
            p.z = it->pos.z + it->ofs.z;
            switch (it->effType) {
            case 3:
                EstSet(0, -1, &p, 0, 0, 0x2E, 0xC00, it->effNo, (u32) m, m);
                break;
            case 5:
                EstSet(0, -1, &p, 0, 0, 0x30, 0xC00, it->effNo, (u32) m, m);
                break;
            case 4:
                EstSet(0, -1, &p, 0, 0, 0x32, 0xC00, it->effNo, (u32) m, m);
                break;
            case 2:
                EstSet(0, -1, &p, 0, 0, 0x34, 0xC00, it->effNo, (u32) m, m);
                break;
            case 7:
                EstSet(0, -1, &p, 0, 0, 0x47, 0xC00, it->effNo, (u32) m, m);
                break;
            case 8:
                p.y -= 800.0f;
                EstSet(0, -1, &p, 0, 0, 0x34, 0xC00, it->effNo, (u32) m, m);
                break;
            case 1:
            case 6:
            case 9:
                break;
            }
        } else {
            p.x = m->pos.x + it->ofs.x;
            p.y = m->pos.x + it->ofs.y;
            p.z = m->pos.x + it->ofs.z;
            switch (it->effType) {
            case 3:
                EstSet(0, -1, &p, 0, 0, 0x2E, 0xC00, it->effNo, 0, 0);
                break;
            case 5:
                EstSet(0, -1, &p, 0, 0, 0x30, 0xC00, it->effNo, 0, 0);
                break;
            case 4:
                EstSet(0, -1, &p, 0, 0, 0x32, 0xC00, it->effNo, 0, 0);
                break;
            case 2:
                EstSet(0, -1, &p, 0, 0, 0x34, 0xC00, it->effNo, 0, 0);
                break;
            case 7:
                EstSet(0, -1, &p, 0, 0, 0x47, 0xC00, it->effNo, 0, 0);
                break;
            case 8:
                p.y -= 800.0f;
                EstSet(0, -1, &p, 0, 0, 0x34, 0xC00, it->effNo, 0, 0);
                break;
            case 1:
            case 6:
            case 9:
                break;
            }
        }
        return;
    }
    switch (w->parentParts) {
    case -1:
    case 0:
        break;
    case 2:
        if (pG->room_id != 0x30F) {
            return;
        }
        break;
    case 4:
    case 8:
        if (pG->room_id != 0x21B) {
            return;
        }
        break;
    default:
        return;
    }
    p.x = it->pos.x + it->ofs.x;
    p.y = it->pos.y + it->ofs.y;
    p.z = it->pos.z + it->ofs.z;
    kind = 0;
    c = 0;
    m = w->pParent;
    switch (it->effType) {
    case 3:
        switch (w->parentParts) {
        case -1:
        case 0:
            kind = 0x53;
            break;
        case 2:
            c = 1;
            kind = 0x20;
            break;
        case 4:
            c = 1;
            kind = 7;
            break;
        case 8:
            c = 1;
            kind = 0xD;
            break;
        }
        EstSet((int) m, -1, &p, 0, c, kind, 0xC00, it->effNo, 0, 0);
        break;
    case 5:
        switch (w->parentParts) {
        case -1:
        case 0:
            kind = 0x54;
            break;
        case 2:
            c = 1;
            kind = 0x21;
            break;
        case 4:
            c = 1;
            kind = 9;
            break;
        case 8:
            c = 1;
            kind = 0xF;
            break;
        }
        EstSet((int) m, -1, &p, 0, c, kind, 0xC00, it->effNo, 0, 0);
        break;
    case 4:
        switch (w->parentParts) {
        case -1:
        case 0:
            kind = 0x55;
            break;
        case 2:
            c = 1;
            kind = 0x22;
            break;
        case 4:
            c = 1;
            kind = 0xB;
            break;
        case 8:
            c = 1;
            kind = 0x11;
            break;
        }
        EstSet((int) m, -1, &p, 0, c, kind, 0xC00, it->effNo, 0, 0);
        break;
    case 2:
        switch (w->parentParts) {
        case -1:
        case 0:
            kind = 0x56;
            break;
        case 2:
            c = 1;
            kind = 0x23;
            break;
        default:
            return;
        }
        EstSet((int) m, -1, &p, 0, c, kind, 0xC00, it->effNo, 0, 0);
        break;
    case 1:
    case 6:
    case 7:
    case 8:
    case 9:
        break;
    }
}
