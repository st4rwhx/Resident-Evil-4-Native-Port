#ifndef SSCRN_H
#define SSCRN_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "main_sub.h"

// Sub screen (inventory / map / files / puzzle) front end, game/sscrn.cpp. The screen itself is
// the Sscrn.rel DLL, linked into the ARAM-swapped area while it is open.
class cObjWep;
class cMap;
struct ItemWork;
struct SUB_SCREEN;
struct SsFileWork;
struct ItemScreenWork;

// Sub screen data archive (ss_cmmn.dat / ss_pzzl.dat): a table of byte offsets to its sub-files.
struct SsArc {
    u32 ofs[0x12];
};
#define SS_ARC_PTR(arc, no) ((void*) ((arc)->ofs[no] + (u32) (arc)))

// The work is the `SUB_SCREEN` of the Sscrn module's `Widget<SUB_SCREEN>` template (the module's
// mangled names carry the tag); SubScreenWork is the DOL-side alias.
struct SUB_SCREEN {
    char path[0x28];          // 0x000  "SS/<lang>/<file>" (sscrnSetLanguage / sscrnDataFilename)
    u8 x28;                   // 0x028
    u8 pad_29[3];
    s32 type;                 // 0x02C  open type: 1 inventory, 2, 0x10, 0x20 puzzle, 0x40, 0x80
    s32 flags;                // 0x030  bit0 event, bit1 (flags_5010 bit21 at open), bit3 no sound
    s32 x34;                  // 0x034
    s32 x38;
    s32 wait;                 // 0x03C  frames left before SubScreenCall may open (SubScreenWait)
    s32 x40;
    s32 x44;
    s32 x48;
    int (*x4C)(SUB_SCREEN*);  // 0x04C  screen exit routine (Sscrn ss_*: sscrn_*_out), run until it returns 1
    u32 save170;              // 0x050  pG->flags_170 while open
    u32 save58;               // 0x054  pG->flags_58 while open
    Camera cam;               // 0x058  pG->Cam while open
    Mtx plMat;                // 0x150  player matrix at open
    Mtx subMat;               // 0x180  partner matrix at open
    u8 stage;                 // 0x1B0  sscrnStageNo()
    u8 pad_1B1;
    u16 room;                 // 0x1B2  sscrnRoomNo()
    u8 scope;                 // 0x1B4  1: the scope was up, 2: and flags_5010 bit 26
    u8 bino;                  // 0x1B5  the binocular was up
    u8 x1B6;                  // 0x1B6  flags_5010 bit 28 (always 0: the mask is stored as a byte)
    u8 noBullet;              // 0x1B7  the equipped weapon (type 3) was empty
    u8 x1B8;                  // 0x1B8  item 0xFE owned
    u8 pad_1B9[3];
    s32 healing;              // 0x1BC  SubCharCheckHealing()
    void* pBuf;               // 0x1C0  MRAM area swapped with the ARAM copy (pG->pStageFont)
    u32 aramSize;             // 0x1C4  bytes read to ARAM (SubScreenAramRead)
    u32 heapOfs;              // 0x1C8  heap 12 starts at pBuf + heapOfs
    u32 relOfs;               // 0x1CC  Sscrn.rel offset in the area
    u32 cmmnOfs;              // 0x1D0  ss_cmmn.dat offset
    u32 pzzlOfs;              // 0x1D4  ss_pzzl.dat offset
    s32 relAddr;              // 0x1D8  Sscrn.rel address (0 while unlinked)
    SsArc* pCmmn;             // 0x1DC
    SsArc* pPzzl;             // 0x1E0
    SsArc* x1E4;              // 0x1E4  puzzle screen data (SubScreenTask: pPzzl)
    SsArc* pItem;             // 0x1E8  ss_item.dat archive (Sscrn ss_item)
    SsArc* pTerm;             // 0x1EC  ss_term.dat archive (Sscrn ss_term)
    void* pOpData;            // 0x1F0  op/opNN.das (Sscrn ss_term: the message/sequence archive at +0x400)
    SsArc* pMapCmn;           // 0x1F4  ss_map.dat archive (Sscrn ss_map: common map data, pPzzl while the map is open)
    SsArc* pMapArea;          // 0x1F8  SS/cmn/map_objNN.dat archive of the current area (Sscrn ss_map)
    SsArc* pFile;             // 0x1FC  ss_file.dat archive (Sscrn ss_file)
    SsArc* pExam;             // 0x200  item examine id data archive (examine ItemExamine::idSet)
    SsArc* pShop;             // 0x204  ss_shop.dat archive (Sscrn ss_shop: read to pBuf + aramSize)
    void* pPartner;           // 0x208  SS/cmn/ss_ocNNN.dat (Sscrn ss_term: the partner model data)
    void* pTplBuf;            // 0x20C  0x20000-byte file picture TPL buffer (Sscrn ss_file)
    void* x210;               // 0x210  weapon model data (pBuf + 0x2E5E00, Sscrn SubScreenTask / weaponChangeTask)
    void* binoA;              // 0x214  CameraControl::GetBinocularIDAddr
    void* binoB;              // 0x218
    class cLight* x21C[8];    // 0x21C  screen lights (Sscrn sscrnLightCreate / sscrnLightClear)
    void* x23C;               // 0x23C  0x3E800-byte buffer
    void* x240;               // 0x240  item examine model data (Sscrn SsItemExamine: x23C)
    void* x244;               // 0x244  item examine texture data
    ItemWork* x248;           // 0x248  selected item slot (Sscrn CapSelect)
    cMap* x24C;               // 0x24C  MapMgr work 2 (Sscrn CapSelect)
    u8 x250;                  // 0x250  Sscrn weapon change task state (3 = done)
    s8 x251;                  // 0x251  weapon change request slot
    s16 x252;                 // 0x252  weapon change fade counter (Sscrn weaponChangeTask)
    struct {
        s32 req;              // 0x254  request pending
        u16 no;               // 0x258  weapon number
        u16 type;             // 0x25A  weapon type
    } wepChange[2];           // 0x254  Sscrn weaponChangeRequest
    u8 x264;                  // 0x264  2 for type 2, else 1
    u8 x265;                  // 0x265
    u8 x266;                  // 0x266  2: the player model is shown (Sscrn ss_file)
    u8 x267;                  // 0x267  Sscrn ss_item: 0 select, 1 command, 2 combine (cleared every frame)
    u8 x268;                  // 0x268  Sscrn ss_item: the cursor moved this frame
    u8 x269;                  // 0x269
    u16 x26A;                 // 0x26A
    s8 x26C;                  // 0x26C  Sscrn ss_item: command cursor
    u8 pad_26D[3];
    Mtx plMapMat;             // 0x270  player matrix on the map (Sscrn ss_map mapPositionCheck)
    u8 x2A0;                  // 0x2A0
    u8 mapRooms;              // 0x2A1  Sscrn ss_map: model count of the area's rooms (door models start there)
    s8 mapFloor;              // 0x2A2  Sscrn ss_map: player floor (y / 100 rounded)
    u8 pad_2A3[0x2AD - 0x2A3];
    u8 x2AD;                  // 0x2AD  Sscrn ss_map mapCameraInit clears it
    u8 x2AE;                  // 0x2AE  item 0x7C..0x7F owned -> 0..3
    u8 x2AF;                  // 0x2AF
    class pzlPlayer* x2B0;    // 0x2B0  puzzle (case) player of the Sscrn puzzle screen
    u8 x2B4;                  // 0x2B4  Sscrn ss_shop: the bought piece is in hand (case placement)
    u8 pad_2B5[0x2FA - 0x2B5];
    u16 x2FA;                 // 0x2FA  item id handed to the opened sub screen (sce_at sceAtGetItem)
    u16 x2FC;                 // 0x2FC  its count
    u8 pad_2FE[2];
    ItemWork* x300;           // 0x300  Sscrn ss_pzzl: the extra piece's slot (get() result)
    ItemScreenWork* pItemWk;  // 0x304  Sscrn ss_item cursor state (9 bytes)
    struct SsMapWork* pMapWk; // 0x308  Sscrn ss_map work (mark models, camera, viewport; 0x104C bytes)
    SsFileWork* pFileWk;      // 0x30C  Sscrn ss_file cursor/page state
    s8* x310;                 // 0x310  Sscrn ss_cap cursor {row, column, row * 6 + column}
    struct ShopWork* pShopWk; // 0x314  Sscrn ss_shop list/cursor state (0x48 bytes)
    class Merchant* pMerchant;// 0x318  Sscrn ss_shop: the shop session (game/merchant.cpp Merchant)
    s32 mdtNo;                // 0x31C  OpeSetOpenTerm number
    s32 strBlk;               // 0x320  SndStrPlayBlock handle
    cObjWep* pObj;            // 0x324  OpeSetOpenTerm weapon object
    Vec savePos;              // 0x328  player position before OpeSetOpenTerm
    Vec saveRot;              // 0x334
    s32 cancel;               // 0x340  OpeSetOpenTermCancel
    OSModuleHeader* pModule;  // 0x344  linked sub screen DLL
    union {
        u32 save;             // 0x348  SscrnDataSave/Load word
        struct {
            u8 x348;          // 0x348  (SubScreenGameInit clears it)
            u8 x349;
            u8 x34A;
            u8 x34B;
        };
    };
    u32 x34C;                 // 0x34C  Sscrn debug menu: bit0 open, bit4 debug disp, bit5 memory disp, bit6 reveil
    s32 debugMode;            // 0x350  pG->debug_mode while open
    s32 x354;                 // 0x354  pG->flags_68 bit 30 while open
    u8 pad_358[0x366 - 0x358];
    u8 x366;                  // 0x366
    u8 x367;                  // 0x367  Sscrn ss_item: debug item-make menu open
    s8 x368;                  // 0x368  item-make menu cursor (0/1 = the two id slots, 2 = remove)
    u8 pad_369[3];
    int x36C[2];              // 0x36C  item-make menu item ids
};
typedef SUB_SCREEN SubScreenWork;

// Sscrn ss_item cursor state (SUB_SCREEN::pItemWk, MEM_ALLOC(9)): two item columns.
struct ItemScreenWork {
    s8 x0;
    s8 col;      // 0x1  current column (-1 = main menu)
    s8 idx[2];   // 0x2  cursor index per column
    s8 sel[2];   // 0x4  selected index per column
    s8 comb[2];  // 0x6  combine partner index per column (-1 = none)
    s8 x8;
};

extern SubScreenWork SubScreenWk;

extern "C" {
int SscrnDataSize();
void SscrnDataSave(u32* dst);
void SscrnDataLoad(u32* src);
void SubScreenAramRead();
void sscrnSetLanguage(SubScreenWork* wk, int lang);
void sscrnDataFilename(SubScreenWork* wk, const char* name);
void SubScreenGameInit();
void SubScreenRoomInit();
void SubScreenWait(int frames);
void SubScreenCall();
int sscrnStageNo();
u16 sscrnRoomNo(u16 room);
int SubScreenOpen(int type, int flags);
void SubScreenMiss();
void SubScreenExec();
void SubScreenExitCore(SubScreenWork* wk);
void SubScreenExit();
int OpeGetMdtNo();
void OpeSetMdtNo(u32 no);
int OpeMdtSetInit();
void OpeOwTypeSet(u8 type);
void OpeSetOpenTerm(int no, f32 x, f32 y, f32 z, f32 ang);
void OpeSetOpenTermCancel();
void OpeSetOpenTermEnd();
}

#endif
