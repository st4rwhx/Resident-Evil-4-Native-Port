// game/sscrn: sub screen (inventory / map / puzzle DLL) front end (D:/Bio4/Prog/sscrn.cpp).
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "event.h"
#include "dbg_button.h"
#include "item.h"
#include "cockpit.h"
#include "mercenaries.h"
#include "sce.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "pl_wep.h"
#include "obj.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main_sub.h"
#include "main.h"
#include "pad.h"
#include "scheduler.h"
#include "snd.h"
#include "db_log.h"
#include "datactrl.h"
#include "room_data.h"
#include "view.h"
#include "motion.h"
#include "model.h"
#include "sscrn.h"

extern "C" {
void OSReport(const char* fmt, ...);
void* memset(void* dst, int c, unsigned int n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, unsigned int n);
}

// The original cUnit::beginEvent takes an int (every caller passes one: sce_com's
// cManager<T>::beginEvent(int) loops, OpeSetOpenTerm's `pPL->beginEvent(0)`); the shared cUnit
// declaration still has the no-argument form, so the call goes through this view of the vtable.
class cUnitEvent {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEvent();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};
#define BEGIN_EVENT(p, mode) ((cUnitEvent*) (p))->beginEvent(mode)

// Struct-member view of the cModel manager pointers: a plain scalar store lets the scheduler hoist
// the following pG load above it (the read.cpp EmInitFunc trick).
struct MgrPtr {
    void* p;
};
#define MGR_PTR(g) (((MgrPtr*) &(g))->p)

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

#define SS_ARAM 0xD00000
#define SS_ARAM_SIZE 0x300000

#define MTX_COPY(src, dst)               \
    {                                    \
        MtxPtr d_ = (dst);               \
        MtxPtr s_ = (src);               \
        int i_ = 3;                      \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
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

SubScreenWork SubScreenWk;
IDSystem IdSub;
IDSystem IdNum;

int SscrnDataSize()
{
    return 4;
}

void SscrnDataSave(u32* dst)
{
    *dst = SubScreenWk.save;
}

void SscrnDataLoad(u32* src)
{
    SubScreenWk.save = *src;
}

void SubScreenAramRead()
{
    SubScreenWork* wk = &SubScreenWk;
    int stat;
    int size;
    int req;

    wk->aramSize = 0;
#line 119 "D:/Bio4/Prog/sscrn.cpp"
    req = DVD_READ_N("rel/Sscrn.rel", 0, SS_ARAM, 0, 0, 9);
    wk->relOfs = wk->aramSize;
    Dvd.ReadCheck(req, &stat, &size, (void**) &wk->pModule);
    wk->aramSize += size;
    sscrnDataFilename(wk, "ss_cmmn.dat");
#line 130 "D:/Bio4/Prog/sscrn.cpp"
    req = DVD_READ_N(wk->path, 0, SS_ARAM + wk->aramSize, 0, 0, 9);
    wk->cmmnOfs = wk->aramSize;
    Dvd.ReadCheck(req, &stat, &size, 0);
    wk->aramSize += size;
    sscrnDataFilename(wk, "ss_pzzl.dat");
#line 140 "D:/Bio4/Prog/sscrn.cpp"
    req = DVD_READ_N(wk->path, 0, SS_ARAM + wk->aramSize, 0, 0, 9);
    wk->pzzlOfs = wk->aramSize;
    Dvd.ReadCheck(req, &stat, &size, 0);
    wk->aramSize += size;
    OSReport("SubScrn Data: 0x%08x\n", wk->aramSize);
    OSReport("SubScrn Free: 0x%08x\n", SS_ARAM_SIZE - wk->aramSize);
}

void sscrnSetLanguage(SubScreenWork* wk, int lang)
{
    char* p = strchr(wk->path, '/') + 1;

    switch (lang) {
    case 0:
        strncpy(p, "jpn", 3);
        break;
    case 1:
        strncpy(p, "eng", 3);
        break;
    case 2:
        strncpy(p, "eng", 3);
        break;
    case 3:
        strncpy(p, "ger", 3);
        break;
    case 4:
        strncpy(p, "fra", 3);
        break;
    case 5:
        strncpy(p, "esp", 3);
        break;
    case 6:
        strncpy(p, "ita", 3);
        break;
    default:
        strncpy(p, "jpn", 3);
        break;
    }
}

void sscrnDataFilename(SubScreenWork* wk, const char* name)
{
    strcpy(strrchr(wk->path, '/') + 1, name);
}

void SubScreenGameInit()
{
    SubScreenWork* wk = &SubScreenWk;

    strcpy(wk->path, "SS/___/");
    sscrnSetLanguage(wk, pSys->language);
    wk->relAddr = 0;
    SubScreenAramRead();
    wk->x2AF = 0;
    wk->x2AE = 0;
    wk->x348 = 0;
    memset(&pG->ope_x82E8, 0, 0x44);
    pG->ope_mdt_no = 0x18;
    SubScreenRoomInit();
}

void SubScreenRoomInit()
{
    SubScreenWork* wk = &SubScreenWk;

    wk->type = 0;
    wk->flags = 0;
    wk->x34 = 0;
    wk->wait = 0;
    if (ItemMgr.search(0x7C)) {
        wk->x2AE = 0;
    }
    if (ItemMgr.search(0x7D)) {
        wk->x2AE = 1;
    }
    if (ItemMgr.search(0x7E)) {
        wk->x2AE = 2;
    }
    if (ItemMgr.search(0x7F)) {
        wk->x2AE = 3;
    }
    wk->x2AF = wk->x2AE;
    if (pG->x4FB8 == 1) {
        wk->x2AF = 0;
        wk->x2AE = 0;
    }
    BitOn(pG->flags_500C, 0x02000000);
    BitOff(pG->flags_500C, 0x00040000);
    BitOff(pG->flags_5014, 0x04000000);
    MapMgr.roomInit();
}

void SubScreenWait(int frames)
{
    SubScreenWk.wait = frames;
}

void SubScreenCall()
{
    SubScreenWork* wk = &SubScreenWk;

    if ((s16) pG->pl_life <= 0) {
        return;
    }
    if (pSUB && pSUB->id == 3 && (s16) pG->sub_life <= 0) {
        return;
    }
    if (!(pG->flags_500C & 0x02000000)) {
        return;
    }
    if (pPL->subScrCheck() == 1) {
        wk->wait--;
        if (wk->wait > 0) {
            return;
        }
        wk->wait = 0;
        if (Key.trg & 0x100000) {
            SubScreenOpen(1, 0);
        } else if (Key.trg & 0x200000) {
            if (!(pG->flags_5014 & 0x00200000)) {
                SubScreenOpen(2, 0);
            }
        }
    }
    if (wk->type) {
        pG->flags_500C &= ~0x02000000;
        if (TaskExec(1, SubScreenExec, 0) == 0) {
            SubScreenMiss();
            pG->flags_500C |= 0x02000000;
        }
    }
}

int sscrnStageNo()
{
    if (pG->flags_51C0 & 0x00010000) {
        return 3;
    } else if (pG->flags_51C0 & 0x00800000) {
        return 2;
    } else if (pG->flags_51BC & 4) {
        return 1;
    }
    return 0;
}

u16 sscrnRoomNo(u16 room)
{
    switch (room) {
    case 0x111:
    case 0x112:
    case 0x113:
    case 0x118:
    case 0x119:
    case 0x11A:
    case 0x11B:
        return room - 0x10;
    }
    return room;
}

int SubScreenOpen(int type, int flags)
{
    SubScreenWork* wk = &SubScreenWk;

    if (pG->flags_5014 & 0x04000000) {
        return 0;
    }
    BitOn(pG->flags_5014, 0x04000000);
    wk->type = type;
    wk->flags = flags;
    wk->x34 = 0;
    wk->x40 = 0;
    if (flags & 1) {
        SceEventStart(0);
    } else {
        if (pG->flags_5010 & 0x00200000) {
            wk->flags = flags | 2;
        }
        wk->save170 = pG->flags_170;
        pG->flags_170 = 0xFFFFFFFF;
        KeyStop(0xEFCF0000);
        pG->flags_170 &= ~0x40;
    }
    return 1;
}

void SubScreenMiss()
{
    SubScreenWork* wk = &SubScreenWk;

    if (wk->flags & 1) {
        SceEventEnd(0);
    } else {
        pG->flags_170 = wk->save170;
    }
    wk->flags = 0;
    wk->type = 0;
    pG->flags_5014 &= ~0x04000000;
}

void SubScreenExec()
{
    SubScreenWork* wk = &SubScreenWk;
    int step = 0;
    int cnt = 0;

    for (;;) {
        switch (step) {
        case 0:
            SndSubScreenInit();
            wk->x48 = 0;
            if (!(wk->type & 0x20)) {
                SndCall(0, 2, 0, 0, 0, 0);
            }
            wk->x269 = 0;
            wk->x26A = 0;
            if (pSUB && pSUB->id == 3) {
                wk->healing = SubCharCheckHealing();
            } else {
                wk->healing = 0;
            }
            BitOn(pG->flags_500C, 0x00040000);
            BitOff(pG->flags_500C, 0x100);
            BitOn(pG->flags_170, 0x100);
            BitOff(pG->flags_170, 0x08000000);
            MTX_COPY(pPL->mat, wk->plMat);
            if (pSUB) {
                MTX_COPY(pSUB->mat, wk->subMat);
            }
            wk->cam = pG->Cam;
            step++;
            wk->stage = sscrnStageNo();
            wk->room = sscrnRoomNo(pG->room_id);
            FadeSetW(0, 3, 0, 0);
        case 1:
            if (Fade[0].flags & 1) {
                break;
            }
            step++;
        case 2:
            pG->wep_no = WeaponId2WeaponNo(ItemMgr.armId);
            pG->wep_type = WeaponId2WeaponType(ItemMgr.armId);
            if (pG->flags_500C & 0x40) {
                CamCtrl.saveScopeParam();
                CamCtrl.endScope();
                wk->scope = 1;
                if (pG->flags_5010 & 0x04000000) {
                    wk->scope = 2;
                    pG->flags_5010 &= ~0x04000000;
                }
            } else {
                wk->scope = 0;
            }
            if (pG->flags_500C & 0x400) {
                CamCtrl.GetBinocularIDAddr(&wk->binoA, &wk->binoB);
                CamCtrl.LowerBinocular();
                wk->bino = 1;
            } else {
                wk->bino = 0;
            }
            if (ItemMgr.num(0xFE)) {
                wk->x1B8 = 1;
            } else {
                wk->x1B8 = 0;
            }
            wk->noBullet = 0;
            {
                ItemInfo info;
                itemInfo(ItemMgr.armId, &info);
                if (info.type == 3) {
                    if (ItemMgr.bulletNumCurrent() == 0) {
                        wk->noBullet = 1;
                    }
                }
            }
            {
                u32 t = pG->flags_5010 & 0x10000000;
                wk->x1B6 = t;
            }
            BitOff(pG->flags_5010, 0x10000000);
            wk->save58 = pG->flags_58;
            BitSet(pG->flags_58, 0xFFFFFFFF);
            BitOff(pG->flags_58, 0x10000);
            BitOff(pG->flags_58, 0x2000);
            BitOff(pG->flags_58, 0x800);
            BitOff(pG->flags_58, 0x04000000);
            BitOn(pG->flags_5010, 2);
            cnt = 0;
            step++;
            break;
        case 3:
            if (cnt++ > 0) {
                step++;
            }
            break;
        case 4:
            if (pG->flags_54 & 0x40000000) {
                IdTexRelease(6);
            }
            Cckpt.getCountDown()->saveDisp();
            systemVISetBlack(1);
            ScreenReSize(640, 448);
            systemVISetBlack(0);
            pG->flags_58 |= 0x400;
            FadeKill(1);
            switch (wk->type) {
            case 2:
            case 0x10:
            case 0x20:
            case 0x40:
            case 0x80:
                break;
            default:
                FadeSetW(0x80000000, 3, 0, 0);
                break;
            }
            TaskSuspend(0);
            RoomData.stopRelData();
            wk->pBuf = pG->pStageFont;
            DC.xA08 = 0;
            MemorySwap(wk->pBuf, SS_ARAM, SS_ARAM_SIZE);
            MemSuspendHeap(4);
            if (wk->type & 0x10) {
                wk->heapOfs = wk->aramSize + 0x50000;
            } else if (wk->type & 0x20) {
                wk->heapOfs = wk->pzzlOfs;
            } else {
                wk->heapOfs = wk->pzzlOfs + 0xE4000;
            }
            if (wk->type & 0x30) {
                MemCreateHeap(12, (u32) wk->pBuf + wk->heapOfs, (u32) wk->pBuf + SS_ARAM_SIZE);
            } else {
                MemCreateHeap(12, (u32) wk->pBuf + wk->heapOfs, (u32) wk->pBuf + 0x2E5E00);
            }
            MemSetCurrentHeap(12);
            if (wk->relAddr >= 0) {
                wk->relAddr = wk->relOfs + (u32) wk->pBuf;
                wk->pCmmn = (SsArc*) (wk->cmmnOfs + (u32) wk->pBuf);
                wk->pPzzl = (SsArc*) (wk->pzzlOfs + (u32) wk->pBuf);
            }
            wk->pModule = (OSModuleHeader*) wk->relAddr;
            {
                MessageControl* mes = &cMes;
                int i;
                for (i = 0; i < 16; i++) {
                    mes->Delete(i);
                }
            }
            if (pSys->language == 0) {
                cMes.setupFont(28, 28, (TEXPalette*) SS_ARC_PTR(wk->pCmmn, 4), 3);
            }
            cMes.setLayout(1, 2);
            cMes.setLayout(7, 2);
            if (wk->type == 0x20) {
                IdSub.gameInit(0x80);
            } else {
                IdSub.gameInit(0x200);
            }
            IdTexDataLoad(SS_ARC_PTR(wk->pCmmn, 6), 8);
            if (wk->type == 0x20) {
                IdNum.gameInit(0);
            } else {
                IdNum.gameInit(0x1B2);
            }
            IdSub.set(SS_ARC_PTR(wk->pCmmn, 7), 0xFF, 2, 0x13, 7, 0);
            if (!(wk->type & 0x10)) {
                IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xB), 0xFF, 0, 0xF, 0, 0);
            }
            IdSub.set(SS_ARC_PTR(wk->pCmmn, 0x11), 0xFF, 4, 0x13, 9, 0);
            IdSub.set(SS_ARC_PTR(wk->pCmmn, 9), 0xFF, 1, 9, 3, 0);
            switch (wk->type) {
            case 2:
                IdSys.dispSw(0x21, 0);
            case 4:
            case 0x40:
            case 0x80:
                IdSys.dispSw(0x21, 1);
                IdSub.dispSw(0, 0);
                break;
            case 0x20:
                IdSub.dispSw(1, 0);
                IdSub.dispSw(0, 0);
                IdSub.dispSw(2, 0);
                break;
            case 0x10:
                IdSub.dispSw(1, 1);
                IdSub.dispSw(2, 1);
                break;
            default:
                IdSys.dispSw(0x21, 1);
                IdSub.dispSw(0, 1);
                break;
            }
            Cckpt.life.fix(0);
#line 808 "D:/Bio4/Prog/sscrn.cpp"
            wk->x23C = MEM_ALLOC(0x3E800, 1, 13);
            if (wk->type == 2) {
                wk->x265 = 2;
                wk->x264 = 2;
            } else {
                wk->x265 = 1;
                wk->x264 = 1;
            }
            wk->x28 = 1;
            wk->x44 = 0;
            LightMgr.inSscrn();
            LightMgr.create(0, 9, -2, 0);
            {
                int i;
                for (i = 0; i < 8; i++) {
                    wk->x21C[i] = 0;
                }
            }
            {
                void* bss;
                if (wk->pModule->bssSize == 0) {
                    bss = 0;
                } else {
#line 834 "D:/Bio4/Prog/sscrn.cpp"
                    bss = MEM_ALLOC(wk->pModule->bssSize, 1, 13);
                }
                DLL_Link(wk->pModule, bss);
            }
            wk->x366 = 0;
            wk->debugMode = pG->debug_mode;
            {
                int v = 1;
                if ((pG->flags_68 & 0x40000000) == 0) {
                    v = 0;
                }
                wk->x354 = v;
            }
            step++;
            pG->flags_68 &= ~0x40000000;
        case 5:
            pG->flags_170 &= ~0x80000000;
            TaskChain(wk->pModule->prolog, 0);
            break;
        }
        TaskSleep(1);
    }
}

void SubScreenExitCore(SubScreenWork* wk)
{
    if (pG->flags_500C & 0x00040000) {
        MapMgr.roomInit();
        DLL_Unlink(wk->pModule);
        wk->relAddr = 0;
        MemDestroyHeap(12);
        MemSignalHeap(4);
        MemSetCurrentHeap(4);
        MemorySwap(wk->pBuf, SS_ARAM, SS_ARAM_SIZE);
        DC.xA08 = 1;
        RoomData.restartRelData();
        MGR_PTR(cModel::mm) = &ModInfoMgr;
        MGR_PTR(cModel::pm) = &PartsMgr;
        pG->flags_500C &= ~0x00040000;
    }
}

// Fade from black to clear with the clear word passed in: SubScreenExit sets it to 0 before
// cMes.roomInit() (the `li r30,0` before that call), which keeps the zero a loop-body pseudo that
// loop.c does not hoist and does not merge with the `type = flags = 0` zero (the FadeSetW inline's
// own zero constant is combined with it by combine_movables and hoisted to the prologue).
static inline void FadeSetBlackOut(u32 clear, u32 time, u32 z, int late)
{
    FadeColorPair col;

    *(u32*) &col.start = 0xFF;
    *(u32*) &col.end = clear;
    FadeSet(0x80000000, &col.start, &col.end, time, z, late);
}

void SubScreenExit()
{
    SubScreenWork* wk = &SubScreenWk;
    int step = 0;
    int cnt = 0;
    int wepNo = 0;
    int wepType = 0;
    int wepLv = 0;

    for (;;) {
        switch (step) {
        case 0:
            if (!(wk->x34 & 8)) {
                SndCall(0, 3, 0, 0, 0, 0);
            }
            cMes.Delete(0);
            wepNo = WeaponId2WeaponNo(ItemMgr.armId);
            wepType = WeaponId2WeaponType(ItemMgr.armId);
            if (ItemMgr.pArm) {
                wepLv = ItemMgr.pArm->x8 >> 13;
            } else {
                wepLv = 0;
            }
            cnt = 0;
            step++;
            break;
        case 1:
            if (cnt++ > 0) {
                step = 2;
            }
            break;
        case 2:
            SubScreenExitCore(wk);
            cnt = 0;
            step = 3;
            sscrnDataFilename(wk, "ss_pzzl.dat");
#line 979 "D:/Bio4/Prog/sscrn.cpp"
            Dvd.ReadCheck(DVD_READ_N(wk->path, 0, SS_ARAM + wk->pzzlOfs, 0, 0, 9), 0, 0, 0);
            pG->flags_58 &= ~0x400;
            break;
        case 3:
            if (cnt++ > 0) {
                step++;
            }
            break;
        case 4:
            if (pG->x4FB8 != 1 && (pG->wep_no != wepNo || pG->wep_type != wepType || pG->wep_x4FB2 != wepLv)) {
                cPlayer* pl;
                if (wk->flags & 2) {
                    ItemMgr.arm(0);
                    wepLv = 0;
                    wepNo = WeaponId2WeaponNo(ItemMgr.armId);
                    wepType = WeaponId2WeaponType(ItemMgr.armId);
                }
                pl = pPL;
                SndBlkStop(2);
                pl->weaponRelease();
                pl->weaponLoad(wepNo, wepType);
                pG->wep_x4FB2 = wepLv;
                pl->weaponInit();
                wk->scope = 0;
                wk->noBullet = 0;
            }
            {
                int change = 0;
                if (pG->x4FB8 == 0) {
                    if (ItemMgr.num(0xFE)) {
                        change = wk->x1B8 == 0;
                    } else if (wk->x1B8 == 1) {
                        change = 1;
                    }
                }
                if (change) {
                    PlSetCostume();
                    PlChangeData();
                }
            }
            systemVISetBlack(1);
            ScreenReSize(512, 448);
            systemVISetBlack(0);
            pG->Cam = wk->cam;
            View.move();
            BitSet(pG->flags_58, wk->save58);
            if (wk->bino == 0) {
                pG->flags_170 &= ~0x80000000;
            }
            BitOff(pG->flags_5010, 2);
            if (wk->x1B6) {
                pG->flags_5010 |= 0x10000000;
            }
            {
                u32 i;
                for (i = 0; i < 10; i++) {
                    if (pPL->pWep->pObj) {
                        pPL->pWep->pObj->move();
                    }
                }
            }
            IdSys.dispSw(0x21, 1);
            Cckpt.life.fix(0);
            if (wk->scope) {
                CamCtrl.startScope(0, 0);
                CamCtrl.loadScopeParam();
            }
            if (wk->bino) {
                CamCtrl.HoldBinocular(wk->binoA, wk->binoB, 0, 0);
            }
            if (wk->noBullet) {
                if (ItemMgr.bulletNumCurrent()) {
                    PlReloadBullet();
                }
            }
            {
                u32 clear = 0;
                cMes.roomInit();
                if (pG->flags_54 & 0x40000000) {
                    mercId.set();
                }
                {
                    Cockpit* ck = &Cckpt;
                    ck->countDown.loadDisp();
                }
                FadeSetBlackOut(clear, 3, 0, 0);
            }
            TaskSignal(0);
            SndSubScreenExit();
            BitOn(pG->flags_500C, 0x02000000);
            BitOn(pG->flags_500C, 0x100);
            BitOff(pG->flags_5014, 0x04000000);
            {
                u32 mode;
                if (wk->scope == 2) {
                    mode = 2;
                } else if (CamCtrl.area_no != -1) {
                    mode = 1;
                } else {
                    mode = 0;
                }
                LightMgr.outSscrn(mode);
            }
            if (wk->flags & 1) {
                SceEventEnd(0);
            } else {
                pG->flags_170 = wk->save170;
            }
            wk->type = 0;
            wk->flags = 0;
            pG->debug_mode = wk->debugMode;
            if (wk->x354) {
                pG->flags_68 |= 0x40000000;
            }
            step++;
        case 5:
            TaskExit();
            break;
        }
        TaskSleep(1);
    }
}

int OpeGetMdtNo()
{
    return pG->ope_mdt_no;
}

void OpeSetMdtNo(u32 no)
{
    u32* tbl = pG->ope_mdt_bits;

    BitOn(tbl[no >> 5], 0x80000000 >> (no & 0x1F));
    pG->ope_mdt_no = no;
}

int OpeMdtSetInit()
{
    int no = SubScreenWk.mdtNo;

    OpeSetMdtNo(no);
    return no;
}

void OpeOwTypeSet(u8 type)
{
    pG->ope_ow_type = type;
    pG->ope_x82FC = 0;
}

// setPos through an inline helper: the caller's `&pos` (a `(plus vsv N)`) is substituted for the
// read-only pointer parameter in the hard-register argument set (fresh `addi r4, r1, 0x68`, never
// a pseudo), so the following `Vec* r = &pos` for setAng is a fresh pseudo that cse cannot merge
// with it (`addi r9, r1, 0x68`, `stfs f31, 8(r9)`). See docs/matching.md "FadeSet colour pair".
static inline void PlSetPosW(cPlayer* pl, Vec* v)
{
    pl->setPos(v);
}

void OpeSetOpenTerm(int no, f32 x, f32 y, f32 z, f32 ang)
{
    SubScreenWork* wk = &SubScreenWk;
    wk->cancel = 0;
    cPlayer* pl = pPL;
    int strTbl[24] = {3, 3, 0x33, 3, 0x33, 3, 3, 0x33, 3, 0x33, 0x33, 3, 3, 3, 0x33, 3, 3, 3, 3, 0x33, 3, 3, 3, 3};
    Vec pos;
    Vec rot;
    int i;

    while (pl->checkEvent() != 1) {
        SceSleep(1);
    }
    if (x != 0.0f) {
        wk->savePos = pPL->pos;
        wk->saveRot = pPL->rot;
        pos.x = x;
        pos.y = y;
        pos.z = z;
        PlSetPosW(pPL, &pos);
        {
            Vec* r = &pos;
            pos.x = 0.0f;
            pos.y = ang;
            r->z = 0.0f;
            pPL->setAng(r);
        }
    }
    SceEventStart(0);
    pG->flags_54 |= 0x400;
    wk->pObj = 0;
    wk->mdtNo = no;
    OpeMdtSetInit();
    BEGIN_EVENT(pl, 0);
    pl->setNoSuspend(1);
    PlSetEyeMode(1);
    wk->strBlk = SndStrPlayBlock(1, strTbl[no], 0.0f);
    MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlArc, 0x79), 0, 0, 0x201, 0);
    SceSleep(1);
    pG->flags_54 &= ~0x400;
    for (i = 0; i <= 20; i++) {
        if (Key.trg & 0x20000000) {
            OpeSetOpenTermCancel();
            goto END;
        }
        SceSleep(1);
    }
    wk->pObj = (cObjWep*) ObjMgr.createBack(0xB);
    if (wk->pObj == 0) {
        pLog->err(0, 0, "OpeSetOpenTerm cObjWep CREATE FAILED");
        return;
    }
    if (wk->pObj->modelInit(PL_ARC_PTR(pG->pPlArc, 0x77), PL_ARC_PTR(pG->pPlArc, 0x78)) == 0) {
        pLog->err(0, 0, "OpeSetOpenTerm modelInit() failed.");
        ObjMgr.destroy(wk->pObj);
        return;
    }
    {
        pos.x = 111.0f;
        pos.y = -22.0f;
        pos.z = 66.0f;
        rot.x = -0.48869219f;
        rot.y = 0.31415927f;
        rot.z = -0.73303829f;
        wk->pObj->parentSet(pl, 0x10, &pos, &rot);
    }
    wk->pObj->setNoSuspend(1);
    pl->setLeftHand(2);
    while (MotionGetState(pl) == 0) {
        if (Key.trg & 0x20000000) {
            OpeSetOpenTermCancel();
            goto END;
        }
        SceSleep(1);
    }
    SubScreenOpen(0x20, 0);
    SubScreenWait(0);
    SceSleep(1);
END:
    OpeSetOpenTermEnd();
    // COMPILER-DIFF: tie (global-alloc live length): x (4 refs / 216 insns) and z (2 / 54) both truncate
    // to priority 370 and the lower pseudo (x) took f30; the original allocated z first. One codeless
    // real insn inside x's range but past z's death makes it 217 -> 368.
    asm("" : "=m"(pos.x));
    if (x != 0.0f) {
        pPL->setPos(&wk->savePos);
        pPL->setAng(&wk->saveRot);
    }
    pG->flags_54 &= ~0x400;
    SceEventEnd(0);
}

void OpeSetOpenTermCancel()
{
    SubScreenWk.cancel = 1;
}

void OpeSetOpenTermEnd()
{
    SubScreenWork* wk = &SubScreenWk;
    cPlayer* pl = pPL;

    SndStrStopBlock(wk->strBlk);
    if (wk->pObj) {
        ObjMgr.destroy(wk->pObj);
        pl->setLeftHand(0x63);
        wk->pObj = 0;
    }
    PlSetEyeMode(0);
    FadeSetW(0x80000000, 3, 0, 0);
    FadeKill(2);
    FadeSetW(0x80000001, 10, 0, 0);
}

// The next unit (lib/ppcdown.c, an SDK library) starts 32-byte aligned in .text and .bss and the
// split object carries the padding: 12 zero bytes after cManager<cMap>::roomInit in .text and
// 0x1C bytes of .bss after IdNum. The .text gap comes from lib/ppcdown.s's `.balign 32` (a
// `.long 0, 0, 0` here would land before the folded roomInit instantiation); the .bss gap is a
// zero-initialised static referenced only by a never-called inline (the dmg.cpp trick).
static u8 sscrn_pad[0x1C];
static inline u8* sscrnPad()
{
    return sscrn_pad;
}
