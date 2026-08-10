// Sscrn/ss_main: sub screen DLL entry (D:/Bio4/Prog/ss_main.cpp): _prolog/_epilog/_unresolved, the
// SubScreenTask that runs the widget chains, the common id/model/light helpers, the exit and item
// examine widgets and the DLL's own model managers.
#include "types.h"
#include "global.h"
#include "light.h"
#include "atari.h"
#include "map_obj.h"
#include "widget.h"
#include "dbg_button.h"
#include "item.h"
#include "cockpit.h"
#include "mes.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main.h"
#include "snd.h"
#include "db_log.h"
#include "view.h"
#include "trans.h"
#include "model.h"
#include "camera.h"
#include "scheduler.h"
#include "gx.h"
#include "motion.h"
#include "esp.h"
#include "sscrn.h"

// `inline`, defined BEFORE ss_main.h: a deferred inline whose address SubScreenTask takes is output
// at the end of the file (after __static_initialization_and_destruction_0), in the order the deferred
// functions were queued. The original queued the synthesized widget destructors when they were
// synthesized (end of file); ours queues them at the class definition, so the definition must
// precede the widget classes to come out first (0xD5B4 before ~Widget and the three destructors).
extern "C" inline void LightSetModel2(cModel* m)
{
    LightMgr.setModel2(m);
}

#include "ss_main.h"

extern "C" void OSReport(const char* fmt, ...);
extern "C" int sprintf(char* s, const char* fmt, ...);
extern "C" int EspMove();
extern "C" int EspgenMove();
// SubScreenTask passes a second argument to MotionMove (pl_npc.cpp does the same: `li r4, 0`).
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

// _ctors/_dtors: the linker script's labels on the .ctors/.dtors lists (null terminated)
extern void (*_ctors[])(void);
extern void (*_dtors[])(void);

// The widget classes (SsExitInit / SsExitMain / SsItemExamine) are declared in ss_main.h.

extern "C" {
void SubScreenTask();
void clearZbuffer();
void sscrnCameraInit(SUB_SCREEN* wk, Camera* cam);
int sscrnKey2Game(SUB_SCREEN* wk);
void dispScrollBar(u32 top, u32 n, u32 num, IdUnit* bar, IdUnit* up, IdUnit* down);
void generalModelAlloc(SUB_SCREEN* wk);
int sscrnMainMenu(SUB_SCREEN* wk);
void idMainMenu(SUB_SCREEN* wk, int sw);
void idMainMenuFade(SUB_SCREEN* wk, int sw);
void sscrnModelFree(SUB_SCREEN* wk);
void weaponChangeRequest(u16 no, u16 type);
int weaponChangeReadCheck();
int weaponChangeMoveCheck();
}

static void weaponChangeTask();
static void sscrnModelTrans(cModel* m);

extern "C" void _prolog()
{
    void (**p)(void);

    for (p = _ctors; *p; p++) {
        (*p)();
    }
    OSReport("prolog...\n");
    SubScreenTask();
}

extern "C" void _epilog()
{
    void (**p)(void);

    for (p = _dtors; *p; p++) {
        (*p)();
    }
    OSReport("epilog...\n");
}

extern "C" void _unresolved()
{
    OSReport("unresolved...\n");
#line 139 "D:/Bio4/Prog/ss_main.cpp"
    HALT();
}

void sscrnCameraInit(SUB_SCREEN* wk, Camera* cam)
{
    const f32 zero = 0.0f;  // pool order: 0.0 first

    // Store order (sched1 weight rule): up.z is the last zero store in the source, so it carries the
    // zero register's death and is issued before the other zero stores; fovy is written last and
    // its late pool load lets up.z slip in front of it.
    cam->param.pos.z = 5000.0f;
    cam->up.y = 1.0f;
    cam->param.at.x = 0.0f;
    cam->param.at.y = 0.0f;
    cam->param.at.z = 0.0f;
    cam->param.pos.x = 0.0f;
    cam->param.pos.y = 0.0f;
    cam->up.x = 0.0f;
    cam->up.z = 0.0f;
    cam->param.fovy = 20.0f;
    CameraSetOrientationUp(cam);
    C_MTXPerspective(cam->projMat, cam->param.fovy, 1.3333334f, ZNEAR, ZFAR);
    cam->dist = PSVECDistance(&cam->param.pos, &cam->param.at);
    C_MTXLookAt(cam->viewMat, &cam->param.pos, &cam->up, &cam->param.at);
}

int sscrnKey2Game(SUB_SCREEN* wk)
{
    if (wk->type == 1) {
        if (Key.trg & 0x100000) {
            return 1;
        }
    } else {
        if (Key.trg & 0x40100000) {
            return 1;
        }
    }
    return 0;
}

void dispScrollBar(u32 top, u32 n, u32 num, IdUnit* bar, IdUnit* up, IdUnit* down)
{
    if (n < num) {
        f32 h = up->scr.y - down->scr.y;
        f32 rate = (f32) n / (f32) num;
        bar->sizeY = rate * h;
        rate = (f32) top / (f32) num;
        bar->scr.y = up->pos.y - rate * (up->scr.y - down->scr.y);
        bar->flags |= 8;
    } else {
        bar->flags &= ~8;
    }
}

// Struct-member view of the cModel manager pointers (game/sscrn.cpp MGR_PTR).
struct MgrPtr {
    void* p;
};
#define MGR_PTR(g) (((MgrPtr*) &(g))->p)

void generalModelAlloc(SUB_SCREEN* wk)
{
    int i;

    wk->x38 |= 1;
    ssModInfoMgr.roomInit();
    ssModInfoMgr.arrayAlloc(0xA0);
    ssPartsMgr.roomInit();
    ssPartsMgr.arrayAlloc(0x100);
    MGR_PTR(cModel::mm) = &ssModInfoMgr;
    MGR_PTR(cModel::pm) = &ssPartsMgr;
    MapMgr.roomInit();
    MapMgr.arrayAlloc(0xA0);
    for (i = 0; i < 0xA0; i++) {
        MapMgr.create(0, i);
    }
}

// Widget chain of the sub screen: every screen's Init/Main pair with its links (link 0 = the next
// widget of the screen's own chain; the main widgets link to the other screens' Init widgets and to
// the exit widget). Runs the current widget every frame until the screen closes.

void SubScreenTask()
{
    SUB_SCREEN* wk = &SubScreenWk;
    SsTermMain* termMain = 0;
    SsShopInit* shopInit = 0;
    SsShopMain* shopMain = 0;
    SsTermInit* termInit = 0;
    SsExitInit* exitInit = new SsExitInit;
    SsExitMain* exitMain;
    SsPzzlInit* pzzlInit;
    SsPzzlMain* pzzlMain;
    SsItemInit* itemInit;
    SsItemMain* itemMain;
    SsMapInit* mapInit;
    SsMapMain* mapMain;
    SsFileInit* fileInit;
    SsFileMain* fileMain;
    SsCapInit* capInit;
    SsCapMain* capMain;
    Widget<SUB_SCREEN>* cur;

    exitMain = new SsExitMain;
    exitInit->connect(0, exitMain);
    cur = 0;
    if (wk->type & 0x10) {
        shopInit = new SsShopInit;
        shopMain = new SsShopMain;
        shopInit->connect(0, shopMain);
        shopMain->connect(0, exitInit);
        cur = shopInit;
        cur->init(wk);
    } else if (wk->type & 0x20) {
        termInit = new SsTermInit;
        termMain = new SsTermMain;
        termInit->connect(0, termMain);
        termMain->connect(0, exitInit);
        cur = termInit;
        cur->init(wk);
    } else {
        pzzlInit = new SsPzzlInit;
        pzzlMain = new SsPzzlMain;
        itemInit = new SsItemInit;
        itemMain = new SsItemMain;
        mapInit = new SsMapInit;
        mapMain = new SsMapMain;
        fileInit = new SsFileInit;
        fileMain = new SsFileMain;
        capInit = new SsCapInit;
        capMain = new SsCapMain;
        pzzlInit->connect(0, pzzlMain);
        pzzlMain->connect(0, itemInit);
        pzzlMain->connect(2, mapInit);
        pzzlMain->connect(3, fileInit);
        pzzlMain->connect(4, exitInit);
        itemInit->connect(0, itemMain);
        itemMain->connect(0, pzzlInit);
        itemMain->connect(2, mapInit);
        itemMain->connect(3, fileInit);
        itemMain->connect(5, exitInit);
        itemMain->connect(4, capInit);
        mapInit->connect(0, mapMain);
        mapMain->connect(0, pzzlInit);
        mapMain->connect(1, itemInit);
        mapMain->connect(3, fileInit);
        mapMain->connect(4, exitInit);
        fileInit->connect(0, fileMain);
        fileMain->connect(0, pzzlInit);
        fileMain->connect(1, itemInit);
        fileMain->connect(3, mapInit);
        fileMain->connect(4, exitInit);
        capInit->connect(0, capMain);
        capMain->connect(0, itemInit);
        capMain->connect(1, exitInit);
        wk->x210 = (u8*) wk->pBuf + 0x2E5E00;
        if (pG->x4FB8 != 1) {
            char name[0x40];
            int req;
            weaponFilename(name, WeaponId2WeaponNo(ItemMgr.armId));
#line 412 "D:/Bio4/Prog/ss_main.cpp"
            req = DVD_READ_N(name, wk->x210, 0, 0, 0, 0x11);
            Dvd.ReadCheck(req, 0, 0, 0);
        }
        generalModelAlloc(wk);
        playerModelInit();
        if (wk->type & 2) {
            cur = mapInit;
            cur->init(wk);
        } else if (wk->type & 4) {
            wk->x1E4 = wk->pPzzl;
            cur = pzzlMain;
            cur->init(wk);
        } else if (wk->type & 0x80) {
            cur = itemInit;
            cur->init(wk);
        } else if (wk->type & 0x40) {
            cur = fileInit;
            cur->init(wk);
        } else {
            wk->x1E4 = wk->pPzzl;
            cur = pzzlMain;
            cur->init(wk);
        }
    }
    wk->x250 = 0;
    wk->x251 = 0;
    TaskExec(2, (TaskFunc) weaponChangeTask, 0);
    while (wk->x28) {
        // `&MapMgr` for the two model loops at the top of the body: loop.c hoists it (lis + addi
        // into r23) as one two-use invariant; declared inside the `if (wk->x44 == 0)` block below,
        // the set is `maybe_never` and used in two blocks, so it is not movable.
        cMapMgr* mgr = &MapMgr;
        if (exitInit != cur && exitMain != cur && shopInit != cur && shopMain != cur && termInit != cur &&
            termMain != cur && weaponChangeMoveCheck()) {
            f32 rate = (f32) (s16) wk->x26A / 10.0f;
            if (wk->x269 == 0) {
                wk->x26A--;
                if ((s16) wk->x26A < 0) {
                    wk->x26A = 0;
                }
            } else {
                wk->x26A++;
                if ((s16) wk->x26A > 10) {
                    wk->x26A = 10;
                }
            }
            if (ssPlModel) {
                ssPlModel->x158 = 1.0f - rate;
            }
            if (ssWepModel) {
                ssWepModel->x158 = 1.0f - rate;
            }
            if (ssPlMotion) {
                MotionMoveF(ssPlModel, 0);
            }
            // Dead test (never-read store): its `high pG` is set in this block and survives as the
            // register of the 0x19/0x1F/0x20 arm (cse1 canon_reg: the arm's own high dies inside the
            // extended block, so the earlier one stays canonical); the 0x1C arm is reached in a fresh
            // cse block, keeps its high, and gcse PRE makes it redundant with the reaching register
            // inserted at this block's end -> the target's two `lis pG@ha` (r29/r30) before the
            // ssWepModel2 test, and no combine_movables hoist (both highs are used in other blocks
            // inside the maybe_never region). The store is trivially dead, so jump folds the branch
            // before gcse and sched2 sees one block.
            {
                int dmy;
                if (pG->x4FB8 == 7) {
                    dmy = 0;
                }
            }
            if (ssWepModel2 && ssWepModel) {
                switch (WeaponId2WeaponNo(ItemMgr.armId)) {
                case 0x19:
                case 0x1F:
                case 0x20:
                    if (pG->x4FB8 == 0) {
                        MotionMoveF(ssWepModel, 0);
                    } else {
                        ssWepModel->matUpdate();
                    }
                    break;
                case 0x1C:
                    if (pG->x4FB8 == 4) {
                        MotionMoveF(ssWepModel, 0);
                    } else {
                        ssWepModel->matUpdate();
                    }
                    break;
                default:
                    ssWepModel->matUpdate();
                    break;
                }
            }
        }
        cur->move(wk);
        cur = cur->cur;
        SscrnDebugMenu(wk);
        LightMgr.move();
        if (IdSub.setCk(2)) {
            int d[8];
            int v = pG->x4F98;
            int i;
            for (i = 0; i < 8; i++) {
                d[i] = v % 10;
                v /= 10;
            }
            for (i = 0; i < 8; i++) {
                IdUnit* u;
                u = IdSub.unitPtr(i + 1, 2);
                u->flags |= 8;
                u->flags_7F |= 2;
                u->no = d[i];
            }
            for (i = 7; i > 0 && d[i] == 0; i--) {
                IdSub.unitPtr(i + 1, 2)->flags &= ~8;
            }
        }
        Cckpt.move();
        IdSub.move();
        IdNum.move();
        CameraMove();
        EffClearToolState();
        EspgenMove();
        EspMove();
        EspGenLoopMove();
        IdSub.trans();
        IdNum.trans();
        if (wk->x44 == 0) {
            cModel* m;
            void (*func)(cModel*);
            // `m->next` read before the call (`lwz r30, 4(r30)` above the `blrl`).
            func = sscrnModelTrans;
            m = mgr->pAlive;
            while (m) {
                cModel* p = m;
                m = (cModel*) m->next;
                func(p);
            }
            func = LightSetModel2;
            m = mgr->pAlive;
            while (m) {
                cModel* p = m;
                m = (cModel*) m->next;
                func(p);
            }
        }
        TaskSleep(1);
    }
    TaskKill(2);
    TaskChain((TaskFunc) SubScreenExit, 0);
}

void SsExitInit::init(SUB_SCREEN* wk)
{
    state = 0;
}

void SsExitInit::move(SUB_SCREEN* wk)
{
    switch (state) {
    case 0:
        FadeSetW(0, 3, 0, 0);
        state++;
    case 1:
        if (Fade[0].flags & 1) {
            break;
        }
        if (weaponChangeReadCheck() == 0) {
            break;
        }
        state++;
    case 2:
        sscrnModelFree(wk);
        sscrnLightClear(wk);
        IdTexRelease(8);
        IdSubErase();
        IdNumErase();
        IdFreeBuffer();
        IdSub.roomInit();
        IdNum.roomInit();
        state++;
    case 3:
        transit(0, wk);
        break;
    }
}

void SsExitMain::move(SUB_SCREEN* wk)
{
    wk->x28 = 0;
}

void SsItemExamine::init(SUB_SCREEN* wk)
{
    state = 0;
}

// `&local` arguments through an inlined helper are recomputed at every call (`addi r4, r1, 0x208`)
// instead of being kept in a callee-saved register (integrate.c substitutes the frame address into
// the hard-register argument set).
static inline void ssItemInfo(u16 id, ItemInfo* info)
{
    itemInfo(id, info);
}
static inline int ssReadCheck(int req, int* size)
{
    return Dvd.ReadCheck(req, size, 0, 0);
}

// Item examine: reads the item's model (.bin) and texture (.tpl) into the examine buffer, shows it
// with the examine camera and returns to the caller widget on cancel.
void SsItemExamine::move(SUB_SCREEN* wk)
{
    static int exam_read_req;
    static u16 exam_id;
    char name[0x100];
    char name2[0x100];
    ItemInfo info;
    int size;

    switch (state) {
    case 0: {
        ssItemInfo(wk->x248->id, &info);
        switch (info.type) {
        case 1:
            exam_id = ItemMgr.weaponId(wk->x248);
            break;
        case 9:
            if (wk->x248->x6 == 1) {
                exam_id = ItemMgr.weaponId(ItemMgr.at(wk->x248->x8));
            } else {
                exam_id = wk->x248->id;
            }
            break;
        default:
            exam_id = wk->x248->id;
            break;
        }
        PSet(wk->x240, wk->x23C);
        ssItemInfo(exam_id, &info);
        if (info.type == 0xD) {
            sprintf(name, "SS/item/cap%02d.bin", exam_id - 0xDB);
        } else {
            sprintf(name, "SS/item/idm%03x.bin", exam_id);
        }
#line 718 "D:/Bio4/Prog/ss_main.cpp"
        exam_read_req = DVD_READ_N(name, wk->x240, 0, 0, 0, 0x10);
        state++;
    }
    case 1: {
        int ret = ssReadCheck(exam_read_req, &size);
        if (ret == 0) {
            break;
        }
        if (ret == 1) {
            int s = size;
            if (s & 0x1F) {
                size = s + (u8) (0x20 - (s & 0x1F));
            }
            wk->x244 = (u8*) wk->x23C + size;
            state++;
        } else {
            transit(0, wk);
        }
        break;
    }
    case 2: {
        ssItemInfo(exam_id, &info);
        if (info.type == 0xD) {
            sprintf(name2, "SS/item/cap%02d.tpl", exam_id - 0xDB);
        } else {
            sprintf(name2, "SS/item/idm%03x.tpl", exam_id);
        }
#line 751 "D:/Bio4/Prog/ss_main.cpp"
        exam_read_req = DVD_READ_N(name2, wk->x244, 0, 0, 0, 0x10);
        state++;
    }
    case 3: {
        int ret = ssReadCheck(exam_read_req, &size);
        if (ret == 0) {
            break;
        }
        if (ret == 1) {
            if ((u32) wk->x244 + size > (u32) wk->x23C + 0x3E800) {
                pLog->err(0, 0, "Item Examine: model is too large.");
            }
            state++;
        } else {
            transit(0, wk);
        }
        break;
    }
    case 4: {
        // light info origin / size (emitted into .rodata here, before this function's pool)
        static const Vec exam_light_ofs = {0.0f, 0.0f, 0.0f};
        static const Vec exam_light_size = {10000.0f, 10000.0f, 0.0f};
        cMap* m = wk->x24C;
        m->modelInit(wk->x240, wk->x244);
        m->be_flag |= 0x4000;
        m->lightInfo.init2(0, 1, &exam_light_ofs, &exam_light_size, 0x20);
        m->partsMatCalc();
        m->partsWorldCalc();
        ssItemInfo(exam_id, &info);
        if (info.type == 1) {
            ItemWork* w = 0;
            ssItemInfo(wk->x248->id, &info);
            switch (info.type) {
            case 1:
                w = wk->x248;
                break;
            case 9:
                w = ItemMgr.at(wk->x248->x8);
                break;
            }
            if (w) {
                exam.level((w->x6 >> 12) + 1, ((w->x6 >> 8) & 0xF) + 1, ((w->x6 >> 4) & 0xF) + 1, (w->x6b[1] & 0xF) + 1);
            }
        }
        ssItemInfo(exam_id, &info);
        if (info.type == 0xD) {
            exam.init(exam_id, m, 2);
        } else {
            exam.init(exam_id, m, 1);
        }
        state++;
    }
    case 5: {
        IdUnit* pos;
        exam.move();
        exam.trans();
        pos = IdSub.unitPtr(0xFE, 0x27);
        cMes.setLayout(7, 2);
        cMes.MesSet(exam_id, (int) ((pos->scr.x + 320.0f) * 0.8f), (int) ((240.0f - pos->scr.y) * 0.8f), 0x20084, 7, 0, 4);
        if (Key.trg & 0x20000) {
            ssItemInfo(exam_id, &info);
            if (info.type == 0xD) {
                SndStrReq(1, exam_id - 0x14, 0x80000003, 0, 0, 0.0f);
            }
        }
        if (Key.trg & 0xC0000000) {
            exam.quit();
            LightMgr.offKind(0x7F);
            wk->x24C->be_flag &= ~2;
            transit(0, wk);
            SndCall(0, 5, 0, 0, 0, 0);
        }
        break;
    }
    }
}

static void sscrnModelTrans(cModel* m)
{
    if (m->be_flag & 2) {
        ModelTrans(m);
    }
}

void sscrnMainMenuInit(SUB_SCREEN* wk, int no)
{
    idMainMenu(wk, no);
}

int sscrnMainMenu(SUB_SCREEN* wk)
{
    int ret = 0;
    s8 old = wk->x265;

    if (Key.trg & 0x100000) {
        wk->x265 = 4;
        wk->x264 = 4;
        ret = 1;
    } else if (Key.trg & 0x40000000) {
        if (old == 4) {
            wk->x265 = old;
            wk->x264 = old;
            ret = 1;
        } else {
            wk->x265 = 4;
        }
    } else if (Key.rep & 0x08000000) {
        wk->x265--;
    } else if (Key.rep & 0x04000000) {
        wk->x265++;
    } else if (Key.trg & 0x80000000) {
        wk->x266 = wk->x264;
        wk->x264 = wk->x265;
        ret = 1;
        if (old != 4) {
            wk->x34 = 0;
            SndCall(0, 4, 0, 0, 0, 0);
        }
    }
    wk->x265 = (s8) wk->x265 < 0 ? 4 : ((s8) wk->x265 > 4 ? 0 : wk->x265);
    if (old != (s8) wk->x265) {
        idMainMenu(wk, 1);
        SndCall(0, 0xA, 0, 0, 0, 0);
    }
    return ret;
}

void idMainMenu(SUB_SCREEN* wk, int sw)
{
    IdUnit* u;
    int i;

    for (i = 0; i < 5; i++) {
        u = IdSub.unitPtr(i, 0);
        u->flags &= ~8;
    }
    if (sw) {
        u = IdSub.unitPtr(wk->x265, 0);
        u->flags |= 8;
        IdSub.setTime(u, 0);
    }
}

void idMainMenuFade(SUB_SCREEN* wk, int sw)
{
    if (wk->type != 0x10) {
        IdUnit* u = IdSub.unitPtr(7, 0);
        if (sw) {
            u->dir &= ~0xF;
        } else {
            u->dir |= 0xF;
        }
    }
}

void IdSubErase()
{
    IdSub.kill(0xFF, 0x1C);
    IdSub.kill(0xFF, 0x1D);
    IdSub.kill(0xFF, 0x1E);
    IdSub.kill(0xFF, 0x1F);
    IdSub.kill(0xFF, 0x14);
    IdSub.kill(0xFF, 0x15);
    IdSub.kill(0xFF, 0x16);
    IdSub.kill(0xFF, 0x10);
    IdSub.kill(0xFF, 0x11);
    IdSub.kill(0xFF, 0x12);
    IdSub.kill(0xFF, 0x18);
    IdSub.kill(0xFF, 0x19);
    IdSub.kill(0xFF, 0x1A);
    IdSub.kill(0xFF, 0x80);
    IdSub.kill(0xFF, 0x81);
    IdSub.kill(0xFF, 0x82);
    IdSub.kill(0xFF, 0x83);
    IdSub.kill(0xFF, 0x84);
    IdTexRelease(9);
}

void IdNumErase()
{
    int i;

    for (i = 0; i < 0x3E; i++) {
        IdNum.killI(0xFF, 0x40 + i);
    }
    IdNum.kill(0xFF, 0x10);
    IdNum.kill(0xFF, 0x11);
    IdNum.kill(0xFF, 0x12);
    IdNum.kill(0xFF, 0x14);
    IdNum.kill(0xFF, 0x15);
    IdNum.kill(0xFF, 0x16);
}

// Clears the Z buffer with a full screen quad at the far plane (the model screens draw over the 2D
// background).
void clearZbuffer()
{
    static f32 clear_z = -0.99999f;
    Mtx44 proj;
    Mtx mtx;

    GXSetColorUpdate(0);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 1);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, 1.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetNumTexGens(0);
    GXSetChanCtrl(0, 0, 0, 0, 0, 2, 2);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
    GXSetTevOrder(0, 0xFF, 0xFF, 0xFF);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 7);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    {
        GXColor col = {0x08, 0x08, 0x80, 0x1C};
        GXSetChanMatColor(4, col);
    }
    GXSetBlendMode(1, 4, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(0.0f, 0.0f, clear_z);
    GXPosition3f32(512.0f, 0.0f, clear_z);
    GXPosition3f32(512.0f, 448.0f, clear_z);
    GXPosition3f32(0.0f, 448.0f, clear_z);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
}

void sscrnModelClear(SUB_SCREEN* wk)
{
    u32 i;

    for (i = 3; i < 0xA0; i++) {
        MapMgr.getWork(i)->be_flag &= ~2;
    }
}

void sscrnModelFree(SUB_SCREEN* wk)
{
    int off = !(wk->x38 & 1);

    if (off) {
        return;
    }
    {
        MGR_PTR(cModel::mm) = &ModInfoMgr;
        MGR_PTR(cModel::pm) = &PartsMgr;
        MapMgr.destroyAll();
        ssModInfoMgr.arrayFree();
        ssPartsMgr.arrayFree();
        MapMgr.arrayFree();
        wk->x38 &= ~1;
    }
}

void sscrnLightClear(SUB_SCREEN* wk)
{
    int i;

    for (i = 0; i < 8; i++) {
        LightMgr.destroy(wk->x21C[i]);
        wk->x21C[i] = 0;
    }
}

void sscrnLightCreate(SUB_SCREEN* wk, cLit* lit)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (wk->x21C[i] == 0) {
            wk->x21C[i] = LightMgr.create(lit, 0, i, 0);
        }
    }
}

// Three digit number display with the IdNum table `id`: unit 0 is the frame (placed at `pos`), units
// 1..3 the digits (colour of IdSub 0x14/0xFD or 0xFE when flags bit 1 is set), 0x11..0x13 their
// shadows; flags bit 0 hides the leading zeros.
void numDisp(u8 id, int num, Vec* pos, u32 flags)
{
    IdUnit* col0 = IdSub.unitPtr(0xFD, 0x14);
    IdUnit* col1 = IdSub.unitPtr(0xFE, 0x14);
    IdUnit* u;
    int i;

    u = IdNum.unitPtr(0, id);
    u->flags &= ~8;
    for (i = 1; i <= 3; i++) {
        u = IdNum.unitPtr(i, id);
        u->flags &= ~8;
        if (flags & 2) {
            u->col0[0] = col1->col0[0];
            u->col0[1] = col1->col0[1];
            u->col0[2] = col1->col0[2];
            u->col0[3] = col1->col0[3];
        } else {
            u->col0[0] = col0->col0[0];
            u->col0[1] = col0->col0[1];
            u->col0[2] = col0->col0[2];
            u->col0[3] = col0->col0[3];
        }
    }
    for (i = 0x11; i <= 0x13; i++) {
        u = IdNum.unitPtr(i, id);
        u->flags &= ~8;
    }
    if (pos) {
        u8 d[3];
        int on;
        for (i = 0; i < 3; i++) {
            d[i] = num % 10;
            num /= 10;
        }
        on = 0;
        for (i = 2; i >= 0; i--) {
            if ((flags & 1) && on == 0) {
                if (d[i] == 0 && i != 0) {
                    continue;
                }
                on = 1;
            }
            u = IdNum.unitPtr(i + 1, id);
            u->flags |= 8;
            u->flags_7F |= 2;
            u->no = d[i];
            u = IdNum.unitPtr(i + 0x11, id);
            u->flags |= 8;
        }
        u = IdNum.unitPtr(0, id);
        u->flags |= 8;
        u->scr = *pos;
    }
}

void weaponChangeRequest(u16 no, u16 type)
{
    SUB_SCREEN* wk = &SubScreenWk;

    if (pG->x4FB8 == 1) {
        return;
    }
    switch (pG->x4FB8) {
    case 0:
    case 2:
    case 3:
    case 4:
    case 5:
        wk->wepChange[wk->x251].req = 1;
        wk->wepChange[wk->x251].no = no;
        wk->wepChange[wk->x251].type = type;
        break;
    }
}

int weaponChangeReadCheck()
{
    SUB_SCREEN* wk = &SubScreenWk;

    if (wk->wepChange[0].req == 0 && wk->wepChange[1].req == 0) {
        return 1;
    }
    return 0;
}

int weaponChangeMoveCheck()
{
    return SubScreenWk.x250 != 3 && SubScreenWk.x250 != 4;
}

// Weapon change task (TaskExec priority 2): fades the character and weapon models out, reads the
// requested weapon's model into the weapon buffer, rebuilds the character model and fades back in.
static void weaponChangeTask()
{
    SUB_SCREEN* wk = &SubScreenWk;
    static u16 wep_no;
    static u16 wep_type;
    static s8 wep_slot;
    static int wep_read_req;
    static int fade_out_frame = 5;
    static int fade_in_frame = 5;
    char name[0x40];
    int stat;
    int size;

    for (;;) {
        switch ((s8) wk->x250) {
        case 0:
            if (wk->wepChange[wk->x251].req) {
                wep_slot = wk->x251;
                wk->x251 = wk->x251 == 0;
                wk->x250++;
                wk->x252 = fade_out_frame;
            }
            break;
        case 1: {
            f32 rate;
            wk->x252--;
            rate = (f32) wk->x252 / (f32) fade_out_frame;
            if (ssPlModel) {
                ssPlModel->alpha = rate;
            }
            if (ssWepModel2) {
                ssWepModel->alpha = rate;
            }
            if (wk->x252 <= 0) {
                if (ssPlModel) {
                    ssPlModel->be_flag &= ~2;
                }
                if (ssWepModel2) {
                    ssWepModel->be_flag &= ~2;
                }
                wk->x250++;
            }
            break;
        }
        case 2:
            SndBlkStop(2);
            wk->x250++;
        case 3:
            wep_no = wk->wepChange[wep_slot].no;
            wep_type = wk->wepChange[wep_slot].type;
            weaponFilename(name, wep_no);
#line 1439 "D:/Bio4/Prog/ss_main.cpp"
            wep_read_req = DVD_READ_N(name, wk->x210, 0, 0, 0, 0x10);
            if (wep_read_req <= 0) {
                break;
            }
            wk->x250++;
            break;
        case 4:
            if (Dvd.ReadCheck(wep_read_req, &stat, &size, 0) != 1) {
                break;
            }
            wk->x250++;
        case 5:
            switch (pG->x4FB8) {
            case 0:
                leonModelInit(wep_no, wep_type);
                break;
            case 1:
                ashleyModelInit();
                break;
            case 2:
                adaModelInit(wep_no, wep_type);
                break;
            case 4:
                klauserModelInit(wep_no, wep_type);
                break;
            case 3:
                hunkModelInit(wep_no, wep_type);
                break;
            case 5:
                weskerModelInit(wep_no, wep_type);
                break;
            }
            if (ssPlModel) {
                BitOn(ssPlModel->be_flag, 2);
                ssPlModel->alpha = 0.0f;
            }
            if (ssWepModel2) {
                BitOn(ssWepModel->be_flag, 2);
                ssWepModel->alpha = 0.0f;
            }
            wk->wepChange[wep_slot].req = 0;
            wk->x252 = 0;
            wk->x250++;
            break;
        case 6: {
            f32 rate;
            wk->x252++;
            rate = (f32) wk->x252 / (f32) fade_in_frame;
            if (ssPlModel) {
                ssPlModel->alpha = rate;
            }
            if (ssWepModel2) {
                ssWepModel->alpha = rate;
            }
            if (wk->x252 >= fade_in_frame) {
                if (ssPlModel) {
                    ssPlModel->alpha = 1.0f;
                }
                if (ssWepModel2) {
                    ssWepModel->alpha = 1.0f;
                }
                wk->x250 = 0;
            }
            break;
        }
        }
        TaskSleep(1);
    }
}

// Defined after the function-local statics above: objects with constructors are emitted at their
// definition, the statics at their declaration (.bss 0x27C..0x28C, then the managers).
cSsPartsMgr ssPartsMgr;
cSsModInfoMgr ssModInfoMgr;


// The split object's .data is 4 bytes longer than the variables (the next unit's .data starts
// 8-aligned in the REL), like ss_file.
asm(".section .data; .balign 8; .section .text");
