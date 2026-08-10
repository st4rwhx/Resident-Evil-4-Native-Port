// cLight is 0x154 bytes in this file (light.h)
#define LIGHT_H_CLIGHT_154
#include "types.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "ctrl.h"
#include "dbg_var.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "main_mem.h"
#include "db_log.h"
#include "dbmodule.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "obj.h"
#include "file.h"
#include "math_sub.h"
#include "vec.h"
#include "gx.h"
#include "scroll.h"
#include "em.h"
#include "etc_model.h"
#include "main.h"
#include "db_cam.h"
#include "player.h"

// The module's 0x34-byte COMMON block (uninitialised template statics of the original build; the split
// skeleton defines it as `common_<mod>`, see em10.cpp / st_room.h): once db_light is compiled in a module
// the block must come from a compiled object, or make_rel refuses the link.
#define DB_LIGHT_STR2(x) #x
#define DB_LIGHT_STR(x) DB_LIGHT_STR2(x)
asm(".comm common_" DB_LIGHT_STR(REL_MODULE) ",52,4");

// Light editor (D:/Bio4/Prog/db_light.cpp): cLightTool (the editor), cDbLit (the .lit cuts being edited,
// one Debug_alloc'd cLightEnv per cut) and cLitPathTool (the light path table). The same object is in
// t_camera / t_light / t_event; Tools adds SetToolLight in front of it (tools/db_light_tools.cpp,
// DB_LIGHT_SET_TOOL_LIGHT), t_esp also cLightTool::setLogMode (tools/db_light_esp.cpp,
// DB_LIGHT_SET_LOG_MODE); t_sce / t_movie carry an older build (tools/db_light_v2.cpp, unwritten).

extern "C" {
int sprintf(char* buf, const char* fmt, ...);
char* strcpy(char* dst, const char* src);
void* memset(void* dst, int c, unsigned int n);
f32 atan2f(f32 y, f32 x);
f64 atan2(f64 y, f64 x);
f32 asinf(f32 x);
f32 cosf(f32 x);
f32 sinf(f32 x);
}

// A colour as one word (DrawTile swatches). The user copy constructor makes it BLKmode: every inlined
// drawColorTile shares one frame slot (see the FadeSet colour pair note in docs/matching.md).
struct GXColorW {
    u32 w;
    GXColorW() {}
    GXColorW(const GXColorW& c) { w = c.w; }
};

// Focus block of cLightEnv (0x28..0x30), copied as a whole by lightPasteFocus.
struct LightFocus {
    s32 depth;
    u8 x2C;
    u8 level;
    u8 mode;
    u8 blurAlpha;
};

// Light data file being edited: cut count / version / max light count like cLit, then one pointer per
// cut (Debug_alloc'd cLightEnv + its cLightWork entries; the tool keeps up to 256 cuts).
class cDbLit {
public:
    u16 nCut;             // 0x00
    u8 version;           // 0x02
    u8 nMaxLight;         // 0x03
    cLightEnv* cut[256];  // 0x04

    cDbLit();
    u32 size();
    cLightEnv* getCut(u16 no);
    int isCut(u16 no) { return cut[no] != NULL; }
    int fileLoad(const char* path);
    int init(cLit* lit);
    void preEventSave();
    int fileSave(const char* path);
    u32 createLit(cLit* dst);
};

// Light path table being edited: one Debug_alloc'd copy per path, plus the path under edit.
class cLitPathTool {
public:
    cLightPathData* path[256];  // 0x000
    u8 edit[0x258];             // 0x400

    cLitPathTool();
    ~cLitPathTool();
    int expand(cLightPathHeader* hdr);
    int createPath(cLightPathHeader* dst);
};

class cLightTool {
public:
    u32 flags;             // 0x00  bit0: object move, bit2: analyze, bit3: cut select follows the camera,
                           //       bit4: bounding boxes, bit5: log errors
    u8 routine;            // 0x04  routine_tbl index
    u8 editNo;             // 0x05  edit_tbl index
    u8 sub;                // 0x06  sub routine of the current editor
    u8 init;               // 0x07
    u8 x8;                 // 0x08
    u8 x9;                 // 0x09
    u8 xA;                 // 0x0A
    u8 xB;                 // 0x0B
    u8 xC;                 // 0x0C
    u8 xD;                 // 0x0D
    u8 xE;                 // 0x0E
    u8 xF;                 // 0x0F
    u8 x10;                // 0x10
    u8 x11;                // 0x11
    u8 x12;                // 0x12
    u8 x13;                // 0x13
    u8 cutNo;              // 0x14  cut being edited
    u8 gameCutNo;          // 0x15  cut the game camera selects
    u8 areaNum;            // 0x16
    u8 pad_17;
    int mode;              // 0x18  0 room local, 1 room server, 2 event, 3 core, 4 tool, 5 item
    u8 ret;                // 0x1C  move() result: 1 = running, 2 = player mode, 0 = quit
    u8 state;              // 0x1D  0 init, 1 camera mode, 2 player mode, 10 mode select
    u8 color;              // 0x1E
    u8 colorBak;           // 0x1F
    cVarLoop<u8> modeSel;  // 0x20
    u8 cursor;             // 0x28
    u8 blink;              // 0x29
    u8 subCursor;          // 0x2A
    u8 rows;               // 0x2B  light table rows per page
    u8 anaNum;             // 0x2C
    u8 pad_2D[3];
    int col;               // 0x30  light table column
    int row;               // 0x34  light table row
    int top;               // 0x38  first light / cut shown
    u8* anaTbl;            // 0x3C  lightAnalysis: 4 bytes per scroll object
    cLight light;          // 0x40  copy buffer
    cLightEnv* pCopyCut;   // 0x194
    JOY joy;               // 0x198
    JOY joy1;              // 0x400
    f32 logX;              // 0x668
    f32 logY;              // 0x66C
    cDbLit lit;            // 0x670
    cLitPathTool litPath;  // 0xA74

    cLightTool();
    ~cLightTool();
    int move();
    u32 dblCk(u32 bit);
    int editEnable();
    void updateLit();
    void printCursor(int x, int y);
    void clearWork();
    void clearSubMenu();
#ifdef DB_LIGHT_SET_LOG_MODE
    void setLogMode(int on);
#endif
    int lightAnalysis();
};

// The tool pointer is a struct member: every store through it reloads the pointer.
struct cLightToolPtr {
    cLightTool* p;
};

void RotVector(Vec* v, Vec* rot);
f32 LIMIT_ANGLE(f32 a);
void moveOnPlaneXZ(Vec* pos, Vec* dir);
int tcCurrentCameraNo();
extern int DebugMenuSelected;
cModel* getRoomEtcOnLight(int no);

// Debug heap pointers are checked for the MEM1 range before use.
#define PTR_OK(p) (!((u32)(p) < 0x80000000 || (u32)(p) > 0x82FFFFFF))
// Error messages go through pLog when bit 5 is set.
#define TOOL_ERR(args...)            \
    if (pTool->flags & 0x20) {       \
        pLog->err(0, 0, args);       \
    }

// Menu tables (.data: `const char*` arrays, not const pointers)
static const char* light_id_name[] = {
    "NORMAL", "FLICK", "WAVE", "SPOT ROTATE", "SHADOW", "PATH", "FADE", "SHINE", "SPOT LOCK", "----",
};
static const char* light_type_name[] = {
    "CONSTANT", "LINEAR", "QUADRATIC", "SPOT LIGHT", "CUSTOM", "PARALLEL", "SPOT QUAD", "LOCAL AMBIENT",
    "----", "----", "----", "----", "----", "----", "----", "----",
};
static const char* shadow_type_name[] = {
    "NORMAL", "PARALLEL", "FIX", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----", "----", "----", "----",
};
static const char* light_type_short[] = {
    "CNST", "LINE", "QUAD", "SPOT", "CSTM", "PARA", "SPQU", "LAMB", "----", "----", "----", "----", "----",
    "----", "----", "----",
};
static const char* shadow_type_short[] = {
    "NORM", "PARA", "FIX ", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----", "----", "----",
};
static const char* self_shd_name[] = {
    "OFF", "1", "2", "3", "4", "5", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----",
};
static const char* soft_shd_name[] = {
    "OFF", "1", "2", "3", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----", "----",
};
static const char* aniso_name[] = {"GX_ANISO_1", "GX_ANISO_2", "GX_ANISO_4"};
static const char* parent_name[] = {"WORLD", "ENEMY", "SCROLL", "EtcModel", "OBJ"};
static const char* parent_short[] = {"WL", "EM", "SC", "ET", "OM"};
static const char* path_room_server = "y:/room/st%x/r%x%02x/r%x%02x%02x.lit";
static const char* path_room_local = "x:\\soft/room/St%x/r%x%02x/r%x%02x%02x.lit";
static const char* path_event = "x:\\soft/room/Event/r%x%02x/%s/etc/%s_%03d.lit";
static const char* path_event_action = "x:\\soft/room/Event/Action/%s/etc/%s_%03d.lit";
static const char* path_tool = "x:\\soft/room/tool%02x.lit";
static const char* path_item = "x:\\soft/room/SubScreen/light/item%03d.lit";
static const char* path_core = "x:\\soft/room/etc/core/core%02x.lit";
static const char* path_litpath = "x:\\soft/room/etc/core/litpath.bin";

static const GXColor blackTemplate = {0, 0, 0, 0};
static const f32 yAxis[3] = {0.0f, 1.0f, 0.0f};  // 4-aligned (an f32[3], not a Vec) after the vtables

// The path header pointer is a struct member too: its load stays after the path table stores.
struct cLitPathPtr {
    cLightPathHeader* p;
};
static cLitPathPtr LitPathPtr;
#define pLitPath (LitPathPtr.p)
static cLightToolPtr LightToolPtr;
#define pTool (LightToolPtr.p)
// The light count is a struct member: its load is not hoisted above the stores through pTool.
struct LightWorkNum {
    u32 n;
};
static LightWorkNum LightWorkNum_;
#define nLightWork (LightWorkNum_.n)
static cLightEnv* pLightEnv;

static void menu();
static void edit();
static void edit_menu();
static void edit_cutsel();
static void edit_cutsel_main();
static void edit_cutsel_sub();
int lightCopyCut(int no);
int lightPasteCut(int no);
int lightPasteAmbient(int no);
int lightPasteFog(int no);
int lightPasteMFog(int no);
int lightPasteShadow(int no);
int lightPasteFocus(int no);
int lightPasteBlur(int no);
int lightPasteTune(int no);
int lightPasteScale(int no);
int lightPasteCutAll(int no);
int lightPasteCutAll2(int no);
cLightEnv* copyCut(cLightEnv* src);
static void edit_light();
static void edit_light_select();
static void edit_light_select_sub();
void lightCopyWork(cLight* dst, cLight* src);
void lightCutWork(int no);
void lightInsertWork(int no);
static void edit_light_no();
static void edit_light_id();
static void edit_light_id_normal();
static void edit_light_id_flick();
static void edit_light_id_wave();
static void edit_light_id_round();
static void edit_light_id_shadow();
static void edit_light_id_path();
static void edit_light_id_fade();
static void edit_light_id_shine();
static void edit_light_id_spotlock();
static void edit_light_eid();
static void edit_light_parent();
void posTranslate(cLight* l, u8 type, u32 id);
static void edit_light_pos();
static void edit_light_radius();
static void edit_light_color();
static void edit_light_intensity();
static void edit_light_type();
void edit_light_type_shadow();
int shadow_select_type();
static void edit_light_type_shadow_fit();
static void edit_light_type_shadow_parallel();
static void edit_light_type_shadow_fix();
static void edit_light_kind();
static void edit_light_attr();
static void edit_light_priority();
int select_type();
static void edit_light_type_constant();
static void edit_light_type_quad();
static void edit_light_type_spotlight();
static void edit_light_type_direct();
static void edit_light_type_localamb();
f32 func_attn(cLight* l, f32 d);
void draw_light_graph(cLight* l);
static void edit_light_type_parallel();
static void edit_light_prop_sub();
static void edit_ambient();
static void edit_fog();
static void edit_mirror_fog();
void edit_fog_common(LightFog* fog);
static void edit_focus();
void draw_tone_curve();
static void edit_blur();
static void edit_mipmap();
static void edit_tune();
static void edit_scale();
static void edit_param();
static void edit_wind();
static void path();
static void load();
static void save();
static void option();
static void quit();
void printEditTable();
void DrawTile(int x, int y, int w, int h, GXColor* color);
// COMPILER-DIFF: candidate #18 (by-value aggregate view): under the V4 ABI a by-value GXColor is passed
// by invisible reference, so the caller copies it into a keep-0 stack temp (calls.c) that every call of
// the statement sequence reuses and passes its address in r7 -- the editColor shape. The real DrawTile
// takes a pointer (mangled P7GXColor); an inlined by-value wrapper allocates one keep-1 temp per call.
void DrawTileV(int x, int y, int w, int h, GXColor color) asm("DrawTile__FiiiiP7GXColor");
int LitLoadWork(cDbLit* lit, int no);
int LitSaveWork(cDbLit* lit, int no);
int editColor(int x, int y, GXColor* col);
const char* strFogType(int type);
int fogTypeNext(int type);
int fogTypeBack(int type);
void initLightWork(cLight* l);
void clear_move_free();
void clear_type_free();
int getCutNo();
void drawLightInfo_SpotShadow(cLight* l, u32 color);
void drawLightInfo(cLight* l, u32 color);
int pathSelect(int x, int y, u8 no, u8 flag, int mode);
int pathEdit(int x, int y, u8 no, u8 flag, int mode);
void drawPath(int x, int y, cLightPathData* p, u8 flag, u32 cur);


// Colour swatch: the colour word goes through a local of this inline, so its address is a fresh
// `addi r7, r1, ofs` before every DrawTile call (gcse never sees the hard-register argument set).
static inline void drawColorTile(int x, int y, int w, int h, u32 c)
{
    GXColorW col;
    col.w = c;
    DrawTile(x, y, w, h, (GXColor*) &col);
}

// The current light of the light table.
static inline cLight* curLight()
{
    return LightMgr.getWorkPtr(pTool->top + pTool->row);
}

#ifdef DB_LIGHT_SET_TOOL_LIGHT
// Tools / t_esp / t_sce / t_movie builds: load tool%02x.lit as the current light set (-1: reapply the
// current camera area's lit). The first function of those objects.
static int SetToolLight(int no)
{
    char path[0x100];

    if (no == -1) {
        int area = CamCtrl.area_no;
        if (area == -1) {
            area = 0;
        }
        LightMgr.update(area, -1);
        return 1;
    }
    sprintf(path, path_tool, no);
    cDbLit lit;
    if (lit.fileLoad(path)) {
        if (LitLoadWork(&lit, 0) == 0) {
            return 0;
        }
        return 1;
    }
    return 0;
}
#endif

cLightTool::cLightTool() : modeSel(0, 2, 0)
{
    pTool = this;
    ret = 1;
    rows = 7;
    mode = 1;
    routine = editNo = sub = init = x8 = x9 = xA = xB = xC = xD = xE = xF = x10 = x11 = x12 = x13 = 0;
    cursor = 0;
    blink = 0;
    subCursor = 0;
    state = 0;
    row = 0;
    col = 0;
    top = 0;
    color = pGS->debug_mode;
    colorBak = pGS->debug_mode;
    pLightEnv = LightMgr.getEnvPtr();
    nLightWork = LightMgr.nArray;
    lit.init(*LightMgr.getLitPPtr());
    areaNum = CamCtrl.AreaNum();
    gameCutNo = cutNo = getCutNo();
    LitLoadWork(&lit, cutNo);
    pCopyCut = NULL;
    initLightWork(&light);
    pTool->flags |= 0x20;
    anaTbl = (u8*) Debug_alloc(ObjMgr.nArray * 4, 1);
    if (!PTR_OK(anaTbl)) {
        TOOL_ERR("cLightTool() MEMORY ERROR");
    }
    flags |= 4;
    anaNum = 0;
    logX = 24.0f;
    logY = 140.0f;
}

cLightTool::~cLightTool()
{
    pLog->modeReset();
}

static inline cLight* lightWorkNoChk(u32 no)
{
    return (cLight*) ((u8*) LightMgr.pArray + LightMgr.size * no);
}

static inline cObj* objWorkChkP(u32 no)
{
    cObjMgr* m = &ObjMgr;
    if (no >= m->nArray) {
        return 0;
    }
    return (cObj*) ((u8*) m->pArray + m->size * no);
}

int cLightTool::move()
{
    static void (*routine_tbl[])() = {menu, edit, path, load, save, option, quit};
    int i;

    eprintf(0x18, 0xE, 0, color, "LIGHT TOOL");
    switch (mode) {
    case 0:
    case 1:
        eprintf(0x1B0, 0xE, 0, color, "CUT%02d/%02d", cutNo, areaNum);
        break;
    case 4:
        eprintf(0x1B8, 0xE, 0, color, "TOOL %02d", cutNo);
        break;
    case 2:
        eprintf(0x1A8, 0xE, 0, color, "ROOM%02d/%02d", cutNo, areaNum);
        break;
    case 3:
        eprintf(0x1B8, 0xE, 0, color, "CORE %02d", cutNo);
        break;
    }
    switch (state) {
    case 0:
        joy = Joy[0];
        joy1 = Joy[1];
        ret = 1;
        break;
    case 1:
        eprintf(0xD8, 0, (pG->flags_51E4 & 0x10) ? 0 : 0x14, color, "CAMERA MODE");
        Joy[1] = Joy[0];
        Joy[1].trg &= ~JOY_START;
        memclr_asm(&joy, sizeof(JOY));
        ret = 1;
        break;
    case 2:
        gameCutNo = cutNo = getCutNo();
        eprintf(0xD8, 0, (pG->flags_51E4 & 0x10) ? 0 : 0x14, color, "PLAYER MODE");
        joy1 = Joy[1];
        ret = 2;
        break;
    case 10:
        eprintf(0xD8, 0x38, 4, 0, "MODE SELECT");
        {
            int c;
            if (modeSel == 0) c = 0; else c = 0x14;
            eprintf(0xD8, 0x46, c, 0, "LIGHT");
            if (modeSel == 1) c = 0; else c = 0x14;
            eprintf(0xD8, 0x54, c, 0, "CAMERA");
            if (modeSel == 2) c = 0; else c = 0x14;
            eprintf(0xD8, 0x62, c, 0, "PREVIEW");
        }
        if (Joy[0].rep & JOY_UP) {
            modeSel--;
        }
        if (Joy[0].rep & JOY_DOWN) {
            modeSel++;
        }
        if (Joy[0].trg & (JOY_START | JOY_B | JOY_A)) {
            state = modeSel;
            Joy[0].trg &= ~(JOY_START | JOY_B | JOY_A);
            int st = state;
            switch (st) {
            case 0:
                pG->flags_60 |= 0x10000000;
                color = st;
                break;
            case 1:
                color = st;
                break;
            case 2:
                color = 1;
                updateLit();
                pG->flags_60 &= ~0x10000000;
                break;
            }
        }
        break;
    }
    if (Joy[0].trg & JOY_START) {
        if (state != 10) {
            modeSel.val = state;
            state = 10;
        }
    }
    blink++;
    if (Joy[0].rep) {
        blink = 0;
    }
    for (u32 n = 0; n < LightMgr.nArray; n++) {
        lightWorkNoChk(n)->x140 = n;
    }
    routine_tbl[routine]();
    LightMgr.move();
    if (pG->flags_60 & 0x02000000) {
        if (state == 1) {
            CameraMove();
        } else {
            BitOff(pG->flags_170, 0x40000000);
            pG->flags_60 &= ~0x10000000;
        }
    } else {
        CameraMove();
    }
    if (pTool->flags & 1) {
        ObjMgr.move();
    }
    CtrlMgr.move();
    if (pTool->flags & 4) {
        lightAnalysis();
    }
    if (pTool->flags & 0x10) {
        for (i = 0; i < ObjMgr.nArray; i++) {
            cObj* obj = objWorkChkP(i);
            if (obj->isAlive() && obj->lightInfo.getLightNum()) {
                obj->drawAllBoundingBox(obj->pInfo);
            }
        }
    }
    logX += (f32) Joy[0].ssx * 0.1f;
    logY -= (f32) Joy[0].ssy * 0.1f;
    {
        int x = (int) logX;
        int y = (int) logY;
        cLog* l = pLog.p;
        l->x = x;
        l->y = y;
    }
    return ret;
}

u32 cLightTool::dblCk(u32 bit)
{
    return flags & bit;
}

int cLightTool::editEnable()
{
    if (mode == 3) {
        return 1;
    }
    if (mode == 2) {
        return 1;
    }
    if (mode == 4) {
        return 1;
    }
    if (dblCk(8)) {
        return 1;
    }
    return cutNo == gameCutNo;
}

void cLightTool::updateLit()
{
    if (editEnable()) {
        LitSaveWork(&lit, cutNo);
    }
    if (LightMgr.x1AC & 1) {
        if (PTR_OK(LightMgr.x1B0)) {
            Mem_free(LightMgr.x1B0);
        } else {
            TOOL_ERR("cLightTool::updateLit() PTR ERR %08X", LightMgr.x1B0);
        }
    }
    LightMgr.x1AC |= 1;
#line 577 "D:/Bio4/Prog/db_light.cpp"
    LightMgr.x1B0 = (cLit*) MEM_ALLOC(lit.size(), 1, 13);
    if (!PTR_OK(LightMgr.x1B0)) {
        TOOL_ERR("cLightTool::updateLit() MEM ALLOC FAILED");
        return;
    }
    LightMgr.dbSetRoomLit(LightMgr.x1B0);
    lit.createLit(LightMgr.x1B0);
}

static void menu()
{
    static const char* menu_name[] = {"EDIT", "PATH", "LOAD", "SAVE", "OPTION", "QUIT"};
    int i;
    const char** name;

    eprintf(0x20, 0x2A, 4, pTool->color, "MENU");
    name = menu_name;
    for (i = 0; i < 6; i++) {
        eprintf(0x20, 0x38 + i * 14, 0, pTool->color, *name++);
    }
    pTool->printCursor(3, pTool->cursor + 4);
    if (pTool->joy.rep & (JOY_UP | JOY_SUP)) {
        pTool->cursor = (pTool->cursor + 5) % 6;
    } else if (pTool->joy.rep & (JOY_DOWN | JOY_SDOWN)) {
        pTool->cursor = (pTool->cursor + 7) % 6;
    }
    if (pTool->joy.rep & JOY_A) {
        pTool->routine = pTool->cursor + 1;
        pTool->editNo = pTool->sub = pTool->init = 0;
        pTool->clearWork();
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->cursor = 5;
    }
}

static void edit()
{
    static void (*edit_tbl[])() = {
        edit_menu, edit_cutsel, edit_light, edit_ambient, edit_fog, edit_mirror_fog, edit_focus, edit_blur,
        edit_mipmap, edit_tune, edit_scale, edit_param, edit_wind,
    };

    edit_tbl[pTool->editNo]();
}

static void edit_menu()
{
    static const char* edit_name[] = {
        "CUT SELECT", "LIGHT", "AMBIENT", "FOG", "MIRROR FOG", "FOCUS", "BLUR", "MIPMAP", "LIT TUNE",
        "LIT SCALE", "PARAMETER", "WIND",
    };
    u32 i;
    const char** name;

    eprintf(0x20, 0x2A, 4, pTool->color, "EDIT WORK");
    name = edit_name;
    for (i = 0; i < sizeof(edit_name) / sizeof(char*); i++) {
        eprintf(0x20, 0x38 + i * 14, 0, pTool->color, *name++);
    }
    pTool->printCursor(3, pTool->cursor + 4);
    if (pTool->joy.rep & (JOY_UP | JOY_SUP)) {
        pTool->cursor = (pTool->cursor + 11) % (sizeof(edit_name) / sizeof(char*));
    } else if (pTool->joy.rep & (JOY_DOWN | JOY_SDOWN)) {
        pTool->cursor = (pTool->cursor + 13) % (sizeof(edit_name) / sizeof(char*));
    }
    if (pTool->joy.rep & JOY_A) {
        pTool->editNo = pTool->cursor + 1;
        pTool->sub = pTool->init = pTool->x8 = pTool->x9 = pTool->xA = pTool->xB = 0;
        pTool->top = 0;
        pTool->clearWork();
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->routine = 0;
        pTool->editNo = 0;
        pTool->clearWork();
    }
}

static void edit_cutsel()
{
    static const char* cut_onoff[] = {"1", "2", "4", "x"};
    static void (*cutsel_tbl[])() = {edit_cutsel_main, edit_cutsel_sub};
    int i;
    cLightEnv* env;
    // COMPILER-DIFF: #4 (reverse): the original stores `i + 6` into a narrow local as a plain `mr r30, r0` (no
    // mask); a `u8 line` gives `clrlwi 24`, an `int line` folds the copy into the `addi`, so the sum goes through a
    // pinned, laundered r0 temp into an `int` line (the copy is opaque to loop.c, so `line * 14` stays a mulli)
    int line;

    eprintf(0x20, 0x2A, 4, pTool->color, "CUT TABLE");
    eprintf(0x20, 0x46, 4, pTool->color, "NO  LI AMB FOG  MFOG SHDW FOCUS BLR TUNE SCL");
    for (i = 0; i < 20; i++) {
        env = pTool->lit.getCut(pTool->top + i);
        eprintf(0x20, 0x54 + i * 14, pTool->top + i == pTool->cutNo ? 0 : 0x14, pTool->color, "%03d", pTool->top + i);
        {
            register int t asm("r0"); // COMPILER-DIFF: 4 (unmasked narrow store, see `line`)
            t = i + 6;
            asm("" : "+r"(t)); // combine would fold the hard-reg copy into the addi
            line = t;
        }
        if (PTR_OK(env)) {
            eprintf(0x40, 0x54 + i * 14, 0, pTool->color, "%2d               %d%d%d  %5d %3d", env->nLight, 0, 0, 0,
                    env->x28 / 10, env->blurAlpha);
            drawColorTile(0x58, 0x57 + i * 14, 0x18, 8, env->x0);
            drawColorTile(0x78, 0x57 + i * 14, 0x20, 8, *(u32*) &env->bgColor);
            drawColorTile(0xA0, 0x57 + i * 14, 0x20, 8, *(u32*) &env->mfog.color);
            if (env->tuneOn & 1) {
                drawColorTile(0x140, 0x57 + i * 14, 0x20, 8, *(u32*) &env->tune[0]);
            } else {
                eprintf(0x140, 0x54 + i * 14, 0, pTool->color, "OFF");
            }
            eprintf(0x168, line * 14, 0, pTool->color, "%s", cut_onoff[env->tevScale[0] & 3]);
            eprintf(0x170, line * 14, 0, pTool->color, "%s", cut_onoff[env->tevScale[1] & 3]);
            eprintf(0x178, line * 14, 0, pTool->color, "%s", cut_onoff[env->pad_42[0] & 3]);
        }
    }
    cutsel_tbl[pTool->sub]();
}

static void edit_cutsel_main()
{
    static const int cutsel_col[9] = {3, 10, 14, 19, 24, 29, 35, 39, 44};
    int base;

    if (pTool->init == 0) {
        LitSaveWork(&pTool->lit, pTool->cutNo);
        pTool->top = 0;
        pTool->cursor = pTool->cutNo;
        pTool->col = pTool->row = 0;
        pTool->init = 1;
    }
    base = pTool->top - 6;
    pTool->printCursor(cutsel_col[pTool->col], pTool->cursor - base);
    if ((pTool->joy.rep & JOY_UP) || (pTool->joy.on & JOY_SUP)) {
        if (pTool->cursor != 0) {
            pTool->cursor--;
        }
        if (pTool->top > pTool->cursor) {
            pTool->top = pTool->cursor;
        }
    }
    if ((pTool->joy.rep & JOY_DOWN) || (pTool->joy.on & JOY_SDOWN)) {
        if (pTool->cursor <= 0xFE) {
            pTool->cursor++;
        }
        if (pTool->cursor > pTool->top + 19) {
            pTool->top = pTool->cursor - 19;
        }
    }
    if (pTool->joy.rep & JOY_R) {
        // if/else with a store in each arm (cross-jumped after reload): the join block is a new
        // cse ebb, so the next `pTool` load re-materialises `lis LightToolPtr@ha`; a clamp into a
        // temp (`if (n > 0xFE) n = 0xFF;`) is a skippable block that keeps the shared @ha register.
        if (pTool->cursor + 10 < 0xFF) {
            pTool->cursor += 10;
        } else {
            pTool->cursor = 0xFF;
        }
        if (pTool->cursor > pTool->top + 19) {
            pTool->top = pTool->cursor - 19;
        }
    }
    if (pTool->joy.rep & JOY_L) {
        if (pTool->cursor - 10 > 0) {
            pTool->cursor -= 10;
        } else {
            pTool->cursor = 0;
        }
        if (pTool->top > pTool->cursor) {
            pTool->top = pTool->cursor;
        }
    }
    // the stick bits are the other way round from joy.h's names here
    if ((pTool->joy.rep & JOY_RIGHT) || (pTool->joy.on & 0x20000)) {
        pTool->col = (pTool->col + 10) % 9;
    }
    if ((pTool->joy.rep & JOY_LEFT) || (pTool->joy.on & 0x10000)) {
        pTool->col = (pTool->col + 8) % 9;
    }
    if ((pTool->joy.rep & JOY_A) && pTool->col == 0) {
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->lit, pTool->cutNo);
        }
        pTool->cutNo = pTool->cursor;
        LitLoadWork(&pTool->lit, pTool->cutNo);
        LitSaveWork(&pTool->lit, pTool->cutNo);
    }
    if (pTool->joy.trg & JOY_Y) {
        pTool->clearSubMenu();
        pTool->sub = 1;
    }
    if (pTool->joy.rep & JOY_B) {
        if (!pTool->editEnable()) {
            pTool->cutNo = pTool->gameCutNo;
            LitLoadWork(&pTool->lit, pTool->cutNo);
        }
        pTool->editNo = 0;
        pTool->sub = 0;
        pTool->init = 0;
        pTool->clearWork();
    }
}

static void edit_cutsel_sub()
{
    static const char* cutsel_sub_name[] = {
        "CUT", "COPY", "PASTE", "INSERT", "COPY TO BLANK CUT", "COPY TO ALL CUT",
    };
    int i;
    const char** name;
    int no;

    eprintf(0x150, 0x62, 4, pTool->color, "SUB MENU");
    name = cutsel_sub_name;
    for (i = 0; i < 6; i++) {
        eprintf(0x150, 0x70 + i * 14, 0, pTool->color, *name++);
    }
    pTool->printCursor(0x29, pTool->subCursor + 8);
    if ((pTool->joy.rep & JOY_A) || (pTool->joy.trg & JOY_Y)) {
        no = pTool->subCursor;
        switch (no) {
        case 0:
            if (pTool->lit.getCut(pTool->cursor) != NULL) {
                lightCopyCut(pTool->cursor);
                Debug_free(pTool->lit.getCut(pTool->cursor));
                pTool->lit.cut[pTool->cursor] = 0;
            }
            break;
        case 1:
            lightCopyCut(pTool->cursor);
            break;
        case 2:
            switch (pTool->col) {
            case 0:
                lightPasteCut(pTool->cursor);
                break;
            case 1:
                lightPasteAmbient(pTool->cursor);
                break;
            case 2:
                lightPasteFog(pTool->cursor);
                break;
            case 3:
                lightPasteMFog(pTool->cursor);
                break;
            case 4:
                lightPasteShadow(pTool->cursor);
                break;
            case 5:
                lightPasteFocus(pTool->cursor);
                break;
            case 6:
                lightPasteBlur(pTool->cursor);
                break;
            case 7:
                lightPasteTune(pTool->cursor);
                break;
            case 8:
                lightPasteScale(pTool->cursor);
                break;
            }
            if (pTool->cursor == pTool->cutNo) {
                LitLoadWork(&pTool->lit, pTool->cutNo);
            }
            break;
        case 3:
            lightPasteCut(pTool->cursor);
            if (pTool->cursor == pTool->cutNo) {
                LitLoadWork(&pTool->lit, pTool->cutNo);
            }
            break;
        case 4:
            lightPasteCutAll(pTool->cursor);
            break;
        case 5:
            lightPasteCutAll2(pTool->cursor);
            break;
        }
        pTool->sub = 0;
    }
    if (pTool->joy.rep & JOY_UP) {
        pTool->subCursor = (pTool->subCursor + 5) % 6;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->subCursor = (pTool->subCursor + 7) % 6;
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
}

int lightCopyCut(int no)
{
    if (!pTool->lit.isCut(no)) {
        return 0;
    }
    if (pTool->pCopyCut) {
        Debug_free(pTool->pCopyCut);
    }
    pTool->pCopyCut = copyCut(pTool->lit.getCut(no));
    return 1;
}

int lightPasteCut(int no)
{
    if (pTool->pCopyCut == NULL) {
        return 0;
    }
    if (pTool->lit.isCut(no)) {
        Debug_free(pTool->lit.getCut(no));
    }
    pTool->lit.cut[(u16) no] = copyCut(pTool->pCopyCut);
    return 1;
}

int lightPasteAmbient(int no)
{
    if (pTool->lit.isCut(no) && pTool->pCopyCut) {
        pTool->lit.getCut(no)->x0 = pTool->pCopyCut->x0;
        return 1;
    }
    return 0;
}

int lightPasteFog(int no)
{
    if (pTool->lit.isCut(no) && pTool->pCopyCut) {
        pTool->lit.getCut(no)->fog = pTool->pCopyCut->fog;
        return 1;
    }
    return 0;
}

int lightPasteMFog(int no)
{
    if (pTool->lit.isCut(no) && pTool->pCopyCut) {
        *(LightFog*) ((u8*) pTool->lit.getCut(no) + 0x18) = *(LightFog*) ((u8*) pTool->pCopyCut + 0x18);
        return 1;
    }
    return 0;
}

int lightPasteShadow(int no)
{
    return 0;
}

int lightPasteFocus(int no)
{
    if (pTool->lit.isCut(no) && pTool->pCopyCut) {
        *(LightFocus*) &pTool->lit.getCut(no)->x28 = *(LightFocus*) &pTool->pCopyCut->x28;
        return 1;
    }
    return 0;
}

int lightPasteBlur(int no)
{
    if (pTool->lit.isCut(no) && pTool->pCopyCut) {
        pTool->lit.getCut(no)->blurAlpha = pTool->pCopyCut->blurAlpha;
        return 1;
    }
    return 0;
}

int lightPasteTune(int no)
{
    if (pTool->lit.isCut(no) && pTool->pCopyCut) {
        *(LightFog*) ((u8*) pTool->lit.getCut(no) + 0x30) = *(LightFog*) ((u8*) pTool->pCopyCut + 0x30);
        return 1;
    }
    return 0;
}

int lightPasteScale(int no)
{
    if (pTool->lit.isCut(no) && pTool->pCopyCut) {
        pTool->lit.getCut(no)->tevScale[0] = pTool->pCopyCut->tevScale[0];
        pTool->lit.getCut(no)->tevScale[1] = pTool->pCopyCut->tevScale[1];
        pTool->lit.getCut(no)->pad_42[0] = pTool->pCopyCut->pad_42[0];
        pTool->lit.getCut(no)->pad_42[1] = pTool->pCopyCut->pad_42[1];
        return 1;
    }
    return 0;
}

int lightPasteCutAll(int no)
{
    int i;

    if (!pTool->lit.isCut(no)) {
        return 0;
    }
    for (i = 0; i < pTool->areaNum; i++) {
        if (pTool->lit.isCut(i)) {
            if (pTool->lit.getCut(i)->nLight != 0) {
                continue;
            }
            Debug_free(pTool->lit.getCut(i));
        }
        pTool->lit.cut[(u16) i] = copyCut(pTool->lit.getCut(no));
    }
    return 1;
}

int lightPasteCutAll2(int no)
{
    int i;

    if (!pTool->lit.isCut(no)) {
        return 0;
    }
    for (i = 0; i < pTool->areaNum; i++) {
        if (pTool->lit.isCut(i)) {
            Debug_free(pTool->lit.getCut(i));
        }
        pTool->lit.cut[(u16) i] = copyCut(pTool->lit.getCut(no));
    }
    return 1;
}

cLightEnv* copyCut(cLightEnv* src)
{
    u32 size = src->nLight * sizeof(cLightWork) + sizeof(cLightEnv);
    cLightEnv* dst = (cLightEnv*) Debug_alloc(size, 1);
    memcpy(dst, src, size);
    return dst;
}

static void edit_light()
{
    static void (*edit_light_tbl[40])() = {
        edit_light_select, edit_light_no, edit_light_id, edit_light_eid, edit_light_parent, edit_light_pos,
        edit_light_radius, edit_light_color, edit_light_intensity, edit_light_type, edit_light_kind,
        edit_light_attr, edit_light_priority, NULL, NULL, NULL, NULL, NULL, NULL, NULL, edit_light_select_sub,
        edit_light_prop_sub,
    };

    edit_light_tbl[pTool->sub]();
    printEditTable();
}

static void edit_light_select()
{
    static int light_col[12] = {3, 6, 9, 0xF, 0x12, 0x20, 0x24, 0x28, 0x2C, 0x31, 0x36, 0x3B};
    static int light_col_w[12] = {6, 0x18, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32};
    u32 i;
    cLight* cur;

    pTool->printCursor(light_col[pTool->col], pTool->row + 0x18);
    if (pTool->joy.rep & 0x20002) {
        pTool->col = (pTool->col + 13) % 12;
    } else if (pTool->joy.rep & 0x10001) {
        pTool->col = (pTool->col + 11) % 12;
    }
    if ((pTool->joy.rep & JOY_UP) || (pTool->joy.on & JOY_SUP)) {
        if (pTool->row == 0) {
            if (pTool->top != 0) {
                pTool->top--;
            } else {
                pTool->row = pTool->rows - 1;
                pTool->top = nLightWork - pTool->rows - 1;
            }
        } else {
            pTool->row--;
        }
    } else if ((pTool->joy.rep & JOY_DOWN) || (pTool->joy.on & JOY_SDOWN)) {
        if (pTool->row < pTool->rows - 1) {
            pTool->row++;
        } else if (pTool->top < (int) (nLightWork - pTool->rows - 1)) {
            pTool->top++;
        } else {
            pTool->top = 0;
            pTool->row = 0;
        }
    }
    cur = curLight();
    if (!(cur->be_flag & 1)) {
        pTool->col = 0;
    }
    if (pTool->joy.rep & JOY_A) {
        pTool->sub = pTool->col + 1;
        pTool->init = pTool->x8 = pTool->x9 = pTool->xA = pTool->xB = 0;
    } else if (pTool->joy.trg & JOY_Y) {
        pTool->clearSubMenu();
        pTool->sub += 20;
    } else if (pTool->joy.rep & JOY_B) {
        pTool->editNo = 0;
        pTool->clearWork();
    }
    for (i = 0; i < nLightWork; i++) {
        cLight* l = LightMgr.getWorkPtr(i);
        if ((l->be_flag & 3) == 3) {
            if (l == cur) {
                u32 c = ((pG->flags_51E4 << 4) | 0xF) & 0xFF;
                drawLightInfo(l, c | (c << 24 | c << 16 | c << 8));
            } else {
                drawLightInfo(l, 0x40404040);
            }
        }
    }
}

static void edit_light_select_sub()
{
    static const char* light_sub_name[] = {
        "CUT", "COPY", "INSERT", "DELETE", "PAGE", "CUT", "COPY", "PASTE", "DELETE", "PAGE",
    };
    cLight* cur = curLight();
    u32 i;
    int y;
    int first;

    // First-entry init blocks carry a dead `first` flag set in both arms: the else arm makes the
    // block an if/else (the then arm ends in a jump, so cse cannot skip it), and the join block's
    // `pTool` load re-uses the block's own `lis LightToolPtr@ha` instead of the hoisted one.
    // A single dead set would be deleted by jump1 before cse; two sets survive until flow.
    if (pTool->xC == 0) {
        pTool->xC = 1;
        pTool->xD = pTool->cutNo;
        first = 1;
    } else {
        first = 0;
    }
    eprintf(0x140, 0x46, 4, pTool->color, "SUB MENU");
    i = 0;
    y = 0x54;
    for (; i < 5; i++) {
        int idx = i;
        if (pTool->col != 0) {
            idx = i + 5;
        }
        eprintf(0x140, y, 0, pTool->color, light_sub_name[idx]);
        y += 14;
    }
    eprintf(0x168, 0x8C, pTool->editEnable() ? 0 : 0x16, pTool->color, "%02d", pTool->cutNo);
    if (pTool->cutNo != pTool->xD) {
        eprintf(0x180, 0x8C, pTool->editEnable() ? 0 : 0x16, pTool->color, "--> %02d", pTool->xD);
    }
    pTool->printCursor(0x27, pTool->subCursor + 6);
    if (pTool->joy.rep & JOY_UP) {
        pTool->subCursor = (pTool->subCursor + 4) % 5u;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->subCursor = (pTool->subCursor + 6) % 5u;
    }
    if ((pTool->joy.rep & JOY_A) || (pTool->joy.trg & JOY_Y)) {
        switch (pTool->subCursor) {
        case 0:
            lightCopyWork(&pTool->light, cur);
            lightCutWork(pTool->top + pTool->row);
            break;
        case 1:
            lightCopyWork(&pTool->light, cur);
            break;
        case 2:
            switch (pTool->col) {
            case 0:
                if (pTool->light.be_flag & 1) {
                    lightInsertWork(pTool->top + pTool->row);
                    LightMgr.cManager<cLight>::create(pTool->light.type, pTool->top + pTool->row);
                    lightCopyWork(cur, &pTool->light);
                }
                break;
            case 1:
                cur->type = pTool->light.type;
                cur->sub = pTool->light.sub;
                break;
            case 2:
                cur->xF = pTool->light.xF;
                break;
            case 3:
                cur->parentType = pTool->light.parentType;
                break;
            case 4:
                cur->pos = pTool->light.pos;
                break;
            case 5:
                cur->x1C = pTool->light.x1C;
                break;
            case 6:
                cur->color = pTool->light.color;
                break;
            case 7:
                cur->power = pTool->light.power;
                break;
            case 8:
                cur->xD = pTool->light.xD;
                cur->spot = pTool->light.spot;
                break;
            }
            cur->calcParent();
            break;
        case 3:
            lightCutWork(pTool->top + pTool->row);
            break;
        case 4:
            if (pTool->editEnable()) {
                LitSaveWork(&pTool->lit, pTool->cutNo);
            }
            pTool->cutNo = pTool->xD;
            LitLoadWork(&pTool->lit, pTool->cutNo);
            LitSaveWork(&pTool->lit, pTool->cutNo);
            break;
        }
        pTool->sub -= 20;
    }
    if (pTool->subCursor == 4) {
        u8 num = (pTool->areaNum < 60) ? 60 : pTool->areaNum;
        if (pTool->joy.rep & JOY_RIGHT) {
            pTool->xD = (num + pTool->xD + 1) % num;
        } else if (pTool->joy.rep & JOY_LEFT) {
            pTool->xD = (num + pTool->xD - 1) % num;
        }
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->sub -= 20;
    }
}

void lightCopyWork(cLight* dst, cLight* src)
{
    dst->be_flag = src->be_flag;
    dst->xC = src->xC;
    dst->xD = src->xD;
    dst->type = src->type;
    dst->xF = src->xF;
    dst->pos = src->pos;
    dst->x1C = src->x1C;
    dst->color = src->color;
    dst->power = src->power;
    dst->kind = src->kind;
    dst->attr = src->attr;
    dst->x2B = src->x2B;
    dst->x30 = src->x30;
    dst->x32 = src->x32;
    dst->x34 = src->x34;
    dst->setParent(src->parentType, src->parentId);
    dst->spot = src->spot;
    dst->sub = src->sub;
    dst->path = src->path;
    dst->x138 = src->x138;
    dst->pad_139[0] = src->pad_139[0];
    dst->pad_139[1] = src->pad_139[1];
    dst->pad_139[2] = src->pad_139[2];
    dst->curColor = src->curColor;
}

void lightCutWork(int no)
{
    int i;

    for (i = no; i < (int) nLightWork - 2; i++) {
        cLight* p = LightMgr.getWorkPtr(i);
        cLight* n;
        if (p && p->isAlive()) {
            LightMgr.destroy(p);
        }
        n = LightMgr.getWorkPtr(i + 1);
        if (n && n->isAlive()) {
            cLightMgr* m = &LightMgr;
            m->cManager<cLight>::create(n->type, i);
            lightCopyWork(p, n);
            LightMgr.destroy(n);
        }
    }
}

void lightInsertWork(int no)
{
    int i;

    for (i = nLightWork - 2; i >= no; i--) {
        cLight* p = LightMgr.getWorkPtr(i + 1);
        cLight* n;
        if (p && p->isAlive()) {
            LightMgr.destroy(p);
        }
        n = LightMgr.getWorkPtr(i);
        if (n && n->isAlive()) {
            cLightMgr* m = &LightMgr;
            m->cManager<cLight>::create(n->type, i + 1);
            lightCopyWork(p, n);
            LightMgr.destroy(n);
        }
    }
}

static void edit_light_no()
{
    u32 no = pTool->top + pTool->row;
    cLight* cur = LightMgr.getWorkPtr(no);

    if (cur->be_flag & 1) {
        cur->be_flag ^= 2;
    } else {
        initLightWork(LightMgr.cManager<cLight>::create(0, no));
    }
    pTool->sub = 0;
}

// Per-type work areas at cLight+0x78 (light01.cpp .. light08.cpp keep their own copies).
struct Light01Work {
    u8 pad_0[4];
    s8 range;  // 0x04
};
struct Light02Work {
    f32 base;  // 0x00
    f32 amp;   // 0x04
    f32 freq;  // 0x08
};
struct Light03Work {
    f32 rot[3];  // 0x00
};
struct Light04Work {
    u16 flags;      // 0x00  bit0: inverse texture, bit1: light position set, bit2: use texture
    u8 kind;        // 0x02  0 normal, 1..4 cast, 5 foot
    s8 gndDist;     // 0x03
    s16 rotX;       // 0x04  (parallel / fix)
    s16 rotY;       // 0x06
    u8 texNo;       // 0x08  (fix: range)
    u8 selfShd;     // 0x09
    u8 softShd;     // 0x0A
    u8 multiShd;    // 0x0B
    Vec lightPos;   // 0x0C  (fit)
    u8 range;       // 0x18
};
struct Light05Work {
    cLightPathData* pStart;  // 0x00
    cLightPathData* pCur;    // 0x04
    u8 flag;                 // 0x08  bit0 loop, bit1 inverse
    u8 pad_9[3];
    u8 pathNo;               // 0x0C
    u8 pathIdx;              // 0x0D
};
struct Light06Work {
    f32 start;  // 0x00
    f32 speed;  // 0x04
    f32 rate;   // 0x08
};
struct Light07Work {
    u8 pad_0[8];
    f32 x8;        // 0x08  (the third editor line writes here: a bug in the original)
    f32 speed[3];  // 0x0C
};

// Stick step of the numeric editors: A held multiplies it by 10.
#define STICK_STEP(scale) ((f32) pTool->joy.sx * step / (scale))
#define STICK_MUL() f32 step = (pTool->joy.on & JOY_A) ? 10.0f : 1.0f

static void edit_light_id()
{
    static void (*light_id_tbl[])() = {
        edit_light_id_normal, edit_light_id_flick, edit_light_id_wave, edit_light_id_round,
        edit_light_id_shadow, edit_light_id_path, edit_light_id_fade, edit_light_id_shine,
        edit_light_id_spotlock, edit_light_id_normal,
    };
    cLight* cur = curLight();
    int first;

    if (pTool->xA == 0) {
        pTool->xB = cur->type;
        pTool->xA = 1;
        first = 1;
    } else {
        first = 0;
    }
    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", cur->type, light_id_name[cur->type]);
    if (cur->type != pTool->xB) {
        eprintf(0xC0, 0x9A, 6, pTool->color, "-> %d %s", pTool->xB, light_id_name[pTool->xB]);
    }
    light_id_tbl[cur->type]();
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
        pLog->modeSet(0x18, 0x8C, 0x1E, 10);
    }
}

static void edit_light_id_normal()
{
    cLight* cur = curLight();

    if (pTool->joy.rep & JOY_RIGHT) {
        pTool->xB = (pTool->xB + 11) % 10;
    } else if (pTool->joy.rep & JOY_LEFT) {
        pTool->xB = (pTool->xB + 9) % 10;
    } else if (pTool->joy.rep & JOY_A) {
        if (pTool->xB != cur->type) {
            cur->type = pTool->xB;
            clear_move_free();
        }
    }
}

static void edit_light_id_flick()
{
    cLight* cur = curLight();
    Light01Work* w = (Light01Work*) cur->work;
    int n;
    int v;

    switch (pTool->init) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        drawColorTile(0x60, 0xD2, 0x30, 0x30, *(u32*) &cur->color);
        if (pTool->joy.rep & 0x20002) {
            v = w->range;
            if (pTool->joy.on & JOY_A) {
                n = v + 10;
            } else {
                n = v + 1;
            }
            w->range = n;
            if (n & 0x80) {
                w->range = 0;
            }
        }
        if (pTool->joy.rep & 0x10001) {
            v = w->range;
            if (pTool->joy.on & JOY_A) {
                n = v - 10;
            } else {
                n = v - 1;
            }
            w->range = n;
            if (n & 0x80) {
                w->range = 0x7F;
            }
        }
        break;
    }
    if (pTool->joy.rep & JOY_UP) {
        pTool->init = (pTool->init + 1) % 2;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->init = (pTool->init + 3) % 2;
    }
    if (pTool->joy.trg & JOY_Y) {
        w->range = 0;
    }
    pTool->printCursor(7, pTool->init + 11);
    eprintf(0x40, 0xA8, 0, pTool->color, "COLOR FLICK RANGE %d", w->range);
    drawLightInfo(cur, 0xFFFFFFFF);
}

static void edit_light_id_wave()
{
    cLight* cur = curLight();
    Light02Work* w = (Light02Work*) cur->work;

    switch (pTool->init) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        {
            STICK_MUL();
            w->base += STICK_STEP(10000.0f);
        }
        break;
    case 2:
        {
            STICK_MUL();
            w->amp += STICK_STEP(10000.0f);
        }
        break;
    case 3:
        {
            STICK_MUL();
            w->freq += STICK_STEP(10000.0f);
        }
        break;
    }
    if (pTool->joy.rep & JOY_UP) {
        pTool->init = (pTool->init + 3) % 4;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->init = (pTool->init + 5) % 4;
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "CENTER %3.3f", w->base);
    eprintf(0x40, 0xB6, 0, pTool->color, "RANGE  %3.3f", w->amp);
    eprintf(0x40, 0xC4, 0, pTool->color, "SPEED   %3.3f", w->freq);
    pTool->printCursor(7, pTool->init + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

static void edit_light_id_round()
{
    cLight* cur = curLight();
    Light03Work* w = (Light03Work*) cur->work;

    switch (pTool->init) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->joy.trg & JOY_Y) {
            w->rot[0] = 0.0f;
        }
        {
            STICK_MUL();
            w->rot[0] += STICK_STEP(10000.0f);
        }
        break;
    case 2:
        if (pTool->joy.trg & JOY_Y) {
            w->rot[1] = 0.0f;
        }
        {
            STICK_MUL();
            w->rot[1] += STICK_STEP(10000.0f);
        }
        break;
    case 3:
        if (pTool->joy.trg & JOY_Y) {
            w->rot[2] = 0.0f;
        }
        {
            STICK_MUL();
            w->rot[2] += STICK_STEP(10000.0f);
        }
        break;
    }
    w->rot[0] = LIMIT_ANGLE(w->rot[0]);
    w->rot[1] = LIMIT_ANGLE(w->rot[1]);
    w->rot[2] = LIMIT_ANGLE(w->rot[2]);
    if (pTool->joy.rep & JOY_UP) {
        pTool->init = (pTool->init + 3) % 4;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->init = (pTool->init + 5) % 4;
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "ROT X %3.3f", w->rot[0]);
    eprintf(0x40, 0xB6, 0, pTool->color, "ROT Y %3.3f", w->rot[1]);
    eprintf(0x40, 0xC4, 0, pTool->color, "ROT Z %3.3f", w->rot[2]);
    eprintf(0x40, 0xEE, 0, pTool->color, "PUSH [Y] TO SET 0.0");
    pTool->printCursor(7, pTool->init + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

static void edit_light_id_shadow()
{
    static const char* shadow_kind_name[] = {"NORMAL", "CAST", "CAST_ADD", "CAST2", "CAST_ADD2", "FOOT"};
    static const char* shadow_onoff[] = {"OFF", "ON"};
    cLight* cur = curLight();
    Light04Work* w;
    int step;
    u8 col;

    pLog->modeSet(0xC8, 0x8C, 0x1E, 10);
    w = (Light04Work*) cur->work;
    if ((*(u32*) &cur->color & 0xFFFFFF00) == 0) {
        cur->color.r = 0xFF;
        w->texNo = 0x5A;
    }
    if (cur->xD > 2) {
        cur->xD = 0;
    }
    switch (pTool->init) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->joy.rep & JOY_RIGHT) {
            w->kind++;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            w->kind--;
        }
        if (w->kind & 0x80) {
            w->kind = 5;
        }
        if (w->kind > 5) {
            w->kind = 0;
        }
        break;
    case 2:
        if (w->kind - 1 > 3u) {
            if (pTool->joy.rep & JOY_RIGHT) {
                w->flags |= 1;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                w->flags &= ~1;
            }
        }
        break;
    case 3:
        if (pTool->joy.rep & JOY_RIGHT) {
            w->flags |= 4;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            w->flags &= ~4;
        }
        break;
    case 4:
        if (pTool->joy.trg & JOY_Y) {
            w->gndDist = 0;
        }
        step = (pTool->joy.on & JOY_A) ? 16 : 1;
        if (pTool->joy.rep & JOY_RIGHT) {
            w->gndDist += step;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            w->gndDist -= step;
        }
        break;
    }
    if (pTool->joy.rep & JOY_UP) {
        pTool->init = (pTool->init + 4) % 5;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->init = (pTool->init + 6) % 5;
    }
    col = 0x14;
    if (w->flags & 1) {
        col = 0;
    }
    if (w->kind - 1 <= 3u) {
        col = 0;
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "KIND   : %s", shadow_kind_name[w->kind]);
    if (w->kind == 5) {
        eprintf(0x40, 0xB6, 0x14, pTool->color, "USE_TEX: ");
        eprintf(0x40, 0xC4, 0x14, pTool->color, "INV_TEX:");
        eprintf(0x40, 0xC4, col, pTool->color, "         %s", shadow_onoff[(w->flags >> 2) & 1]);
        eprintf(0x40, 0xD2, 0, pTool->color, "GND_DIST:");
        eprintf(0x40, 0xD2, 0, pTool->color, "          %2d", w->gndDist);
    } else {
        // COMPILER-DIFF: #2 -- the original zero-extends the u8 `col` once before this arm's two
        // uses (`clrlwi r30,r30,24`); ours knows the promoted value fits. The launder + (u8) casts
        // reproduce the mask and the register assignment; `volatile` keeps the mask before the first call
        // (the plain asm let sched1 issue it one call later, the flrAtDataLoad rule).
        int c = col;
        asm volatile("" : "+r"(c));
        eprintf(0x40, 0xB6, 0, pTool->color, "USE_TEX: ");
        eprintf(0x40, 0xC4, 0, pTool->color, "INV_TEX:");
        eprintf(0x40, 0xC4, (u8) c, pTool->color, "         %s", shadow_onoff[(w->flags >> 2) & 1]);
        eprintf(0x40, 0xD2, 0, pTool->color, "TEX_NO :");
        eprintf(0x40, 0xD2, (u8) c, pTool->color, "         %2x", (u8) w->gndDist);
    }
    if (w->kind - 1 <= 3u) {
        eprintf(0x40, 0xB6, 0x17, pTool->color, "         ON");
    } else {
        eprintf(0x40, 0xB6, 0, pTool->color, "         %s", shadow_onoff[w->flags & 1]);
    }
    eprintf(0x40, 0xEE, 0, pTool->color, "PUSH [Y] TO SET 0");
    pTool->printCursor(7, pTool->init + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

static void edit_light_id_path()
{
    cLight* cur = curLight();
    Light05Work* w = (Light05Work*) cur->work;

    switch (pTool->init) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->joy.rep & 0x20002) {
            w->pathNo++;
            cur->x138 = 0;
        }
        if (pTool->joy.rep & 0x10001) {
            w->pathNo--;
            cur->x138 = 0;
        }
        if (pTool->joy.rep & JOY_A) {
            cLightPathData* p = pTool->litPath.path[w->pathNo];
            if (PTR_OK(p)) {
                memcpy(pTool->litPath.edit, p, p->getSize());
                pTool->x10 = pTool->x11 = pTool->x12 = pTool->x13 = 0;
                pTool->init = 100;
            }
        }
        break;
    case 2:
        if (pTool->joy.rep & (JOY_LEFT | JOY_RIGHT)) {
            w->flag ^= 1;
        }
        break;
    case 3:
        if (pTool->joy.rep & (JOY_LEFT | JOY_RIGHT)) {
            w->flag ^= 2;
        }
        break;
    case 100:
        if (pTool->joy.rep & JOY_B) {
            pTool->joy.rep &= ~JOY_B;
            pTool->init = 1;
        }
        break;
    }
    if (pTool->init < 100) {
        if (pTool->joy.rep & JOY_UP) {
            pTool->init = (pTool->init + 3) % 4;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->init = (pTool->init + 5) % 4;
        }
        pTool->printCursor(7, pTool->init + 11);
        eprintf(0x40, 0xA8, 0, pTool->color, "PATH No:%d", w->pathNo);
        eprintf(0x40, 0xB6, 0, pTool->color, "LOOP    %s", (w->flag & 1) ? "OFF" : "ON");
        eprintf(0x40, 0xC4, 0, pTool->color, "INVERSE %s", (w->flag & 2) ? "ON" : "OFF");
        drawLightInfo(cur, 0xFFFFFFFF);
        if (PTR_OK(w->pStart)) {
            drawPath(0xC8, 0x64, w->pStart, w->flag, 0xFFFFFFFF);
        } else {
            eprintf(0xC8, 0xA8, 0, pTool->color, "NO DATA");
        }
    } else {
        eprintf(0x40, 0xA8, 0, pTool->color, "EDIT PATH No:%d", w->pathNo);
        pathEdit(0x20, 0xC4, w->pathNo, w->flag, 0);
    }
}

static void edit_light_id_fade()
{
    cLight* cur = curLight();
    Light06Work* w = (Light06Work*) cur->work;

    switch (pTool->init) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->joy.rep & JOY_RIGHT) {
            ((Light06Work*) cur->work)->start += 0.1f;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            ((Light06Work*) cur->work)->start -= 0.1f;
        }
        if (pTool->joy.trg & JOY_Y) {
            ((Light06Work*) cur->work)->start = 0.0f;
        }
        break;
    case 2:
        if (pTool->joy.rep & JOY_RIGHT) {
            w->speed += 0.01f;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            w->speed -= 0.01f;
        }
        if (pTool->joy.trg & JOY_Y) {
            w->speed = 0.0f;
        }
        break;
    case 3:
        if (pTool->joy.rep & JOY_A) {
            cur->x138 = 0;
        }
        break;
    }
    if (pTool->joy.rep & JOY_UP) {
        pTool->init = (pTool->init + 3) % 4;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->init = (pTool->init + 5) % 4;
    }
    pTool->printCursor(7, pTool->init + 11);
    eprintf(0x40, 0xA8, 0, pTool->color, "START %f", w->start);
    eprintf(0x40, 0xB6, 0, pTool->color, "SPEED %f", w->speed);
    eprintf(0x40, 0xC4, 0, pTool->color, "PLAY");
    drawLightInfo(cur, 0xFFFFFFFF);
}

static void edit_light_id_shine()
{
    cLight* cur = curLight();
    Light07Work* w = (Light07Work*) cur->work;

    switch (pTool->init) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->joy.trg & JOY_Y) {
            w->speed[0] = 0.0f;
        }
        {
            STICK_MUL();
            w->speed[0] += STICK_STEP(10000.0f);
        }
        break;
    case 2:
        if (pTool->joy.trg & JOY_Y) {
            w->speed[1] = 0.0f;
        }
        {
            STICK_MUL();
            w->speed[1] += STICK_STEP(10000.0f);
        }
        break;
    case 3:
        if (pTool->joy.trg & JOY_Y) {
            w->speed[2] = 0.0f;
        }
        {
            STICK_MUL();
            w->x8 += STICK_STEP(10000.0f);
        }
        break;
    }
    w->speed[0] = LIMIT_ANGLE(w->speed[0]);
    w->speed[1] = LIMIT_ANGLE(w->speed[1]);
    w->speed[2] = LIMIT_ANGLE(w->speed[2]);
    if (pTool->joy.rep & JOY_UP) {
        pTool->init = (pTool->init + 3) % 4;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->init = (pTool->init + 5) % 4;
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "SPEED X %3.3f", w->speed[0]);
    eprintf(0x40, 0xB6, 0, pTool->color, "SPEED Y %3.3f", w->speed[1]);
    eprintf(0x40, 0xC4, 0, pTool->color, "SPEED Z %3.3f", w->speed[2]);
    eprintf(0x40, 0xEE, 0, pTool->color, "PUSH [Y] TO SET 0.0");
    pTool->printCursor(7, pTool->init + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

static void edit_light_id_spotlock() {}
static void edit_light_eid()
{
    cLight* cur = curLight();

    pTool->printCursor(0x14, pTool->init + 11);
    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->color, "ENABLE MASK");
    eprintf(0xA8, 0x9A, !(cur->xF & 1) ? 0x14 : 0, pTool->color, "PLAYER");
    eprintf(0xA8, 0xA8, (cur->xF & 2) ? 0 : 0x14, pTool->color, "ENEMY");
    eprintf(0xA8, 0xB6, (cur->xF & 4) ? 0 : 0x14, pTool->color, "OBJ");
    eprintf(0xA8, 0xC4, (cur->xF & 8) ? 0 : 0x14, pTool->color, "EFFECT");
    eprintf(0xA8, 0xD2, (cur->xF & 0x10) ? 0 : 0x14, pTool->color, "SCROLL");
    eprintf(0xA8, 0xE0, (cur->xF & 0x20) ? 0 : 0x14, pTool->color, "ITEM");
    eprintf(0xA8, 0xEE, (cur->xF & 0x40) ? 0 : 0x14, pTool->color, "SUBCHAR");
    eprintf(0xA8, 0xFC, (cur->xF & 0x80) ? 0 : 0x14, pTool->color, "THERMO");
    if (pTool->joy.rep & JOY_UP) {
        pTool->init = (pTool->init + 7) % 8;
    } else if (pTool->joy.rep & JOY_DOWN) {
        pTool->init = (pTool->init + 9) % 8;
    }
    if (pTool->joy.rep & JOY_A) {
        switch (pTool->init) {
        case 0:
            cur->xF ^= 1;
            break;
        case 1:
            cur->xF ^= 2;
            break;
        case 2:
            cur->xF ^= 4;
            break;
        case 3:
            cur->xF ^= 8;
            break;
        case 4:
            cur->xF ^= 0x10;
            break;
        case 5:
            cur->xF ^= 0x20;
            break;
        case 6:
            cur->xF ^= 0x40;
            break;
        case 7:
            cur->xF ^= 0x80;
            break;
        }
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
    if (cur->xD == 7) {
        eprintf(0xE0, 0xC4, 0x16, pTool->color, "<- NOT SUPPORT");
        cur->xF &= ~8;
    }
}

static void edit_light_parent()
{
    static int parent_num = 5;
    cLight* cur = curLight();
    u32 num = 0;
    cModel* etc;
    cObj* obj;
    u32 n;
    cModel* m;
    int first;
    u32 no;

    if (pTool->init == 0) {
        pTool->cursor = 0;
        pTool->x8 = cur->parentType;
        pTool->init = 1;
        first = 1;
    } else {
        first = 0;
    }
    switch (pTool->cursor) {
    case 0:
        if (pTool->joy.rep & JOY_RIGHT) {
            pTool->x8 = (parent_num + pTool->x8 + 1) % parent_num;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            pTool->x8 = (parent_num + pTool->x8 - 1) % parent_num;
        }
        if (pTool->joy.rep & JOY_A) {
            posTranslate(cur, pTool->x8, 0);
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->x8 = cur->parentType;
        }
        break;
    case 1:
        switch (cur->parentType) {
        default:
            TOOL_ERR("INVALED LIGHT PARENT %d", cur->parentType);
        case 0:
            n = 0;
            break;
        case 1:
            n = 250;
            break;
        case 2:
            n = 250;
            break;
        case 3:
            n = 0x40;
            break;
        case 4:
            n = ObjMgr.nArray;
            break;
        }
        if (pTool->joy.rep & JOY_RIGHT) {
            cur->parentId = (cur->parentId & 0xFFFF0000) | ((n + (cur->parentId & 0xFFFF) + 1) % n);
        }
        if (pTool->joy.rep & JOY_LEFT) {
            cur->parentId = (cur->parentId & 0xFFFF0000) | ((n + (cur->parentId & 0xFFFF) - 1) % n);
        }
        break;
    case 2:
        n = 100;
        if (pTool->joy.rep & JOY_RIGHT) {
            no = cur->parentId >> 16;
            no = (no + 101) % n;
            cur->parentId = (cur->parentId & 0xFFFF) | (no << 16);
        }
        if (pTool->joy.rep & JOY_LEFT) {
            no = cur->parentId >> 16;
            no = (no + 99) % n;
            cur->parentId = (cur->parentId & 0xFFFF) | (no << 16);
        }
        break;
    }
    cur->calcParent();
    eprintf(0x40, 0x8C, 4, pTool->color, "PARENT");
    eprintf(0x40, 0x9A, 0, pTool->color, "TYPE  %s", parent_name[cur->parentType]);
    if (cur->parentType != pTool->x8) {
        eprintf(0xB8, 0x9A, 0, pTool->color, "-> %s", parent_name[pTool->x8]);
    }
    switch (cur->parentType) {
    case 0:
        num = 1;
        break;
    case 1:
        eprintf(0x40, 0xA8, 0, pTool->color, "ID    %02X - %s", cur->parent.no, cEmMgr::idName[cur->parent.no]);
        num = 3;
        eprintf(0x40, 0xB6, 0, pTool->color, "PARTS %d", cur->parent.partsNo);
        break;
    case 2:
        eprintf(0x40, 0xA8, 0, pTool->color, "ID    %d", cur->parent.no);
        num = 3;
        eprintf(0x40, 0xB6, 0, pTool->color, "PARTS %d", cur->parent.partsNo);
        break;
    case 3:
        if (getRoomEtcOnLight(cur->parentId, &etc, 0)) {
            eprintf(0x40, 0xA8, 0, pTool->color, "No    %d", cur->parent.no);
        } else {
            eprintf(0x40, 0xA8, 0x14, pTool->color, "No    %d", cur->parent.no);
        }
        num = 2;
        break;
    case 4:
        eprintf(0x40, 0xA8, 0, pTool->color, "ID    %d", cur->parent.no);
        eprintf(0x40, 0xB6, 0, pTool->color, "PARTS %d", cur->parent.partsNo);
        obj = (cObj*) ((u8*) ObjMgr.pArray + ObjMgr.size * cur->parent.no);
        if (obj) {
            switch (obj->id) {
            case 2:
                eprintf(0x40, 0xD2, 0, pTool->color, "OBJID %02x : SCR MODEL", 2);
                break;
            case 0x18: {
                Obj18Work* w;
                eprintf(0x40, 0xD2, 0, pTool->color, "OBJID %02x : EVENT MODEL", 0x18);
                w = (Obj18Work*) obj->work;
                eprintf(0x40, 0xE0, 0, pTool->color, "NAME %s", ((Obj18Work*) obj->work)->evName);
                eprintf(0x40, 0xEE, 0, pTool->color, "TYPE %2d", w->type);
                break;
            }
            default:
                eprintf(0x40, 0xD2, 0, pTool->color, "OBJID %02x", obj->id);
                break;
            }
        }
        num = 3;
        break;
    }
    if (pTool->joy.rep & JOY_UP) {
        pTool->cursor = (num + pTool->cursor - 1) % num;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->cursor = (num + pTool->cursor + 1) % num;
    }
    pTool->printCursor(7, pTool->cursor + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
    m = cur->getCoord();
    if (m) {
        f32 r = cur->x1C * (f32) (pG->flags_51E4 % 30) / 0.1f;
        Draw_sphere(&m->worldPos, r, 0xA0A0A0FF, 1, 1);
        Draw_pos(&m->worldPos, (int) r);
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = pTool->init = 0;
        pTool->cursor = 0;
    }
}

// Re-parents the light, keeping its world position: the applied position is converted into the new
// parent's coordinates.
void posTranslate(cLight* l, u8 type, u32 id)
{
    Vec pos = l->curPos;
    cModel* m;
    Mtx inv;

    l->setParent(type, id);
    if (type == 0) {
        l->pos = pos;
    } else {
        m = l->getCoord();
        if (m) {
            PSMTXInverse(m->mat, inv);
            PSMTXMultVec(inv, &pos, &l->pos);
        } else {
            l->pos.x = 0.0f;
            l->pos.y = 0.0f;
            l->pos.z = 0.0f;
        }
    }
}

static void edit_light_pos()
{
    cLight* cur = curLight();
    f32 step = (pTool->joy.on & JOY_A) ? 7.0f : 1.0f;
    Vec* pos = &cur->pos;

    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->color, "%6.0f %6.0f %6.0f", cur->pos.x, cur->pos.y, cur->pos.z);
    Vec v = {0.0f, 0.0f, 0.0f};
    v.x += (f32) pTool->joy.sx * step;
    v.y += (f32) pTool->joy.sy * step;
    moveOnPlaneXZ(&v, &v);
    v.y += (f32) pTool->joy.trigR * step * 0.5f;
    v.y -= (f32) pTool->joy.trigL * step * 0.5f;
    PSVECAdd(pos, &v, pos);
    if (pTool->joy.rep & JOY_UP) {
        cur->pos.z += 1.0f;
    } else if (pTool->joy.rep & JOY_DOWN) {
        cur->pos.z -= 1.0f;
    }
    if (pTool->joy.rep & JOY_RIGHT) {
        cur->pos.x += 1.0f;
    } else if (pTool->joy.rep & JOY_LEFT) {
        cur->pos.x -= 1.0f;
    }
    if (pTool->joy.rep & JOY_X) {
        cur->pos = vecZero;
    }
    if (pTool->joy.trg & JOY_Y) {
        cur->pos = pG->Cam.param.at;
    }
    drawLightInfo(cur, 0x80808080);
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
}

static void edit_light_radius()
{
    cLight* cur = curLight();
    f32 step = (pTool->joy.on & JOY_A) ? 7.0f : 1.0f;
    int r;

    switch (pTool->init) {
    case 0:
        pTool->cursor = 0;
        pTool->init = 1;
    case 1:
        cur->x1C += (f32) pTool->joy.sx * step * 0.5f;
        cur->x1C += (f32) pTool->joy.sy * step * 0.5f;
        if (pTool->joy.rep & JOY_RIGHT) {
            cur->x1C += 100.0f;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            cur->x1C -= 100.0f;
        }
        if (cur->x1C < 0.0f) {
            cur->x1C = 0.0f;
        }
        drawLightInfo(cur, 0xFFFFFFFF);
        break;
    case 2:
        r = (int) ((f32) pTool->joy.sx * step * 0.5f + (f32) (int) cur->x30);
        if (r < 0) {
            r = 0;
        }
        cur->x30 = r;
        drawLightInfo(cur, 0xFFFFFFFF);
        break;
    }
    pTool->printCursor(7, pTool->init + 10);
    if (pTool->joy.rep & JOY_UP) {
        pTool->init = 1;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->init = 2;
    }
    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    if (cur->x1C != 0.0f) {
        eprintf(0x40, 0x9A, 0, pTool->color, "RADIUS     %6.0f", cur->x1C);
    } else {
        eprintf(0x40, 0x9A, 0, pTool->color, "RADIUS INFINITY");
    }
    if ((f32) (int) cur->x30 != 0.0f) {
        eprintf(0x40, 0xA8, 0, pTool->color, "HIT RADIUS %6d", cur->x30);
    } else {
        eprintf(0x40, 0xA8, 0, pTool->color, "HIT RADIUS");
        eprintf(0x98, 0xA8, 0x14, pTool->color, "NO HIT ADJUST");
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
}

static void edit_light_color()
{
    cLight* cur = curLight();

    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    if (editColor(8, 11, &cur->color) == 0) {
        pTool->sub = 0;
    }
}

static void edit_light_intensity()
{
    cLight* cur = curLight();
    f32 step = (Joy[0].on & JOY_A) ? 10.0f : 1.0f;

    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    if (cur->xD == 7) {
        eprintf(0x40, 0x9A, 0, pTool->color, "NOT USED");
        cur->power = 1.0f;
    } else {
        eprintf(0x40, 0x9A, 0, pTool->color, "INTENSITY %3.3f", cur->power);
    }
    cur->power += (f32) pTool->joy.sx * step / 1000.0f;
    cur->power += (f32) pTool->joy.sy * step / 1000.0f;
    if (pTool->joy.rep & (JOY_UP | JOY_RIGHT)) {
        cur->power += 0.1f;
    }
    if (pTool->joy.rep & (JOY_DOWN | JOY_LEFT)) {
        cur->power -= 0.1f;
    }
    if (cur->power < 0.1f) {
        cur->power = 0.1f;
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
}

static void edit_light_type()
{
    static void (*light_type_tbl[16])() = {
        edit_light_type_constant, edit_light_type_constant, edit_light_type_quad, edit_light_type_spotlight,
        edit_light_type_direct, edit_light_type_parallel, edit_light_type_spotlight, edit_light_type_localamb,
        edit_light_type_constant, edit_light_type_constant, edit_light_type_constant, edit_light_type_constant,
        edit_light_type_constant, edit_light_type_constant, edit_light_type_constant, edit_light_type_constant,
    };
    cLight* cur = curLight();

    if (cur->type == 4) {
        edit_light_type_shadow();
        return;
    }
    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    light_type_tbl[cur->xD]();
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
}

void edit_light_type_shadow()
{
    cLight* cur = curLight();

    static void (*shadow_type_tbl[3])() = {
        edit_light_type_shadow_fit, edit_light_type_shadow_parallel, edit_light_type_shadow_fix,
    };

    eprintf(0x40, 0x8C, 4, pTool->color, "SHADOW PROPATY");
    shadow_type_tbl[cur->xD]();
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
}

int shadow_select_type()
{
    cLight* cur = curLight();
    int first;

    if (pTool->x8 == 0) {
        pTool->x9 = cur->xD;
        pTool->x8 = 1;
        first = 1;
    } else {
        first = 0;
    }
    if (pTool->joy.rep & JOY_RIGHT) {
        pTool->x9 = (pTool->x9 + 4) % 3;
    }
    if (pTool->joy.rep & JOY_LEFT) {
        pTool->x9 = (pTool->x9 + 2) % 3;
    }
    if (pTool->joy.rep & JOY_A) {
        cur->xD = pTool->x9;
    }
    eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", pTool->x9, shadow_type_name[pTool->x9]);
    return 1;
}

// Shadow sub-editor lines shared by the three shadow types: self / soft / multi shadow levels.
#define SHADOW_LEVEL_EDIT(field, max)                       \
    if (pTool->joy.rep & JOY_RIGHT) {                       \
        w->field++;                                         \
    }                                                       \
    if (pTool->joy.rep & 0x20000) {                         \
        w->field++;                                         \
    }                                                       \
    if (w->field > max) {                                   \
        w->field = max;                                     \
    }                                                       \
    if (w->field != 0) {                                    \
        if (pTool->joy.rep & JOY_LEFT) {                    \
            w->field--;                                     \
        }                                                   \
        if (pTool->joy.rep & 0x10000) {                     \
            w->field--;                                     \
        }                                                   \
    }
#define SHADOW_MULTI_EDIT()                                 \
    if (pTool->joy.rep & JOY_RIGHT) {                       \
        w->multiShd = 1;                                    \
    }                                                       \
    if (pTool->joy.rep & 0x20000) {                         \
        w->multiShd = 1;                                    \
    }                                                       \
    if (w->multiShd != 0) {                                 \
        if (pTool->joy.rep & JOY_LEFT) {                    \
            w->multiShd = 0;                                \
        }                                                   \
        if (pTool->joy.rep & 0x10000) {                     \
            w->multiShd = 0;                                \
        }                                                   \
    }
#define SHADOW_ROT_EDIT(field)                                          \
    if (pTool->joy.trg & JOY_Y) {                                       \
        w->field = 0;                                                   \
    }                                                                   \
    w->field += (int) ((f32) pTool->joy.sy * step / 100.0f);            \
    w->field += (int) ((f32) pTool->joy.sx * step / 100.0f);            \
    while (w->field & 0x8000) {                                         \
        w->field += 360;                                                \
    }                                                                   \
    while (w->field > 359) {                                            \
        w->field -= 360;                                                \
    }

static void edit_light_type_shadow_fit()
{
    cLight* cur = curLight();
    Light04Work* w = (Light04Work*) cur->work;
    f32 step = 5.0f;
    int ret = 1;
    int c;

    switch (pTool->init) {
    case 0:
        ret = shadow_select_type();
        break;
    case 1:
        if (pTool->joy.rep & JOY_A) {
            if (((Light04Work*) cur->work)->flags & 2) {
                ((Light04Work*) cur->work)->flags &= ~2;
            } else {
                ((Light04Work*) cur->work)->flags |= 2;
            }
        }
        break;
    case 2:
        w->lightPos.x += (f32) pTool->joy.sx * step;
        w->lightPos.z -= (f32) pTool->joy.sy * step;
        w->lightPos.y += (f32) pTool->joy.trigR * step * 0.5f;
        w->lightPos.y -= (f32) pTool->joy.trigL * step * 0.5f;
        Draw_pos(&w->lightPos, 500);
        if (pTool->joy.trg & JOY_Y) {
            w->lightPos = cur->pos;
        }
        break;
    case 3:
        SHADOW_LEVEL_EDIT(selfShd, 4);
        break;
    case 4:
        SHADOW_LEVEL_EDIT(softShd, 3);
        break;
    case 5:
        SHADOW_MULTI_EDIT();
        break;
    case 6:
        w->range += (int) ((f32) pTool->joy.sy * step / 100.0f);
        w->range += (int) ((f32) pTool->joy.sx * step / 100.0f);
        if (Joy[0].on & JOY_LEFT) {
            if (w->range != 0) {
                w->range--;
            }
        }
        if (Joy[0].on & JOY_RIGHT) {
            if (w->range <= 0x59) {
                w->range++;
            }
        }
        if (w->range > 0x80) {
            w->range = 0;
        }
        if (w->range > 0x5A) {
            w->range = 0x5A;
        }
        break;
    }
    pG->flags_64 |= 0x04000000;
    if (ret) {
        if (pTool->joy.rep & JOY_UP) {
            pTool->init = (pTool->init + 6) % 7;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->init = (pTool->init + 8) % 7;
        }
    }
    if (pTool->init != 0) {
        eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", cur->xD, shadow_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "LIT_POS SET ");
    // COMPILER-DIFF: 2 (the original zero-extends the u8 `col` once at the join for both uses; the
    // launder sits in the arms so that gcse sees the extension's operand unmodified in the join block)
    if (w->flags & 2) {
        eprintf(0x40, 0xA8, 0, pTool->color, "                ON");
        c = 0;
        asm("" : "+r"(c));
    } else {
        eprintf(0x40, 0xA8, 0, pTool->color, "                OFF");
        c = 0x14;
        asm("" : "+r"(c));
    }
    eprintf(0x40, 0xB6, (u8) c, pTool->color, "LIGHT_POS %6f %6f %6f", w->lightPos.x, w->lightPos.y, w->lightPos.z);
    eprintf(0x40, 0xC4, 0, pTool->color, "SELF_SHD %s", self_shd_name[w->selfShd]);
    eprintf(0x40, 0xD2, 0, pTool->color, "SOFT_SHD %s", soft_shd_name[w->softShd]);
    if (w->multiShd == 0) {
        eprintf(0x40, 0xE0, 0, pTool->color, "MULTI_SHD OFF");
    } else {
        eprintf(0x40, 0xE0, 0, pTool->color, "MULTI_SHD ON");
    }
    eprintf(0x40, 0xEE, 0, pTool->color, "RANGE     %d", w->range);
    eprintf(0x40, 0xFC, (u8) c, pTool->color, " [Y_BUTTON] Position Reset");
    pTool->printCursor(7, pTool->init + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

static void edit_light_type_shadow_parallel()
{
    cLight* cur = curLight();
    Light04Work* w = (Light04Work*) cur->work;
    f32 step = 5.0f;
    int ret = 1;

    switch (pTool->init) {
    case 0:
        ret = shadow_select_type();
        break;
    case 1:
        SHADOW_ROT_EDIT(rotX);
        break;
    case 2:
        SHADOW_ROT_EDIT(rotY);
        break;
    case 3:
        SHADOW_LEVEL_EDIT(selfShd, 4);
        break;
    case 4:
        SHADOW_LEVEL_EDIT(softShd, 3);
        break;
    case 5:
        SHADOW_MULTI_EDIT();
        break;
    case 6:
        w->range += (int) ((f32) pTool->joy.sy * step / 100.0f);
        w->range += (int) ((f32) pTool->joy.sx * step / 100.0f);
        if (Joy[0].on & JOY_LEFT) {
            if (w->range != 0) {
                w->range--;
            }
        }
        if (Joy[0].on & JOY_RIGHT) {
            if (w->range <= 0x59) {
                w->range++;
            }
        }
        if (w->range > 0x80) {
            w->range = 0;
        }
        if (w->range > 0x5A) {
            w->range = 0x5A;
        }
        break;
    }
    pG->flags_64 |= 0x04000000;
    if (ret) {
        if (pTool->joy.rep & JOY_UP) {
            pTool->init = (pTool->init + 6) % 7;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->init = (pTool->init + 8) % 7;
        }
    }
    if (pTool->init != 0) {
        eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", cur->xD, shadow_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "ROT_X : %d", w->rotX);
    eprintf(0x40, 0xB6, 0, pTool->color, "ROT_Y : %d", w->rotY);
    eprintf(0x40, 0xC4, 0, pTool->color, "SELF_SHD  %s", self_shd_name[w->selfShd]);
    eprintf(0x40, 0xD2, 0, pTool->color, "SOFT_SHD  %s", soft_shd_name[w->softShd]);
    if (w->multiShd == 0) {
        eprintf(0x40, 0xE0, 0, pTool->color, "MULTI_SHD OFF");
    } else {
        eprintf(0x40, 0xE0, 0, pTool->color, "MULTI_SHD ON");
    }
    eprintf(0x40, 0xEE, 0, pTool->color, "RANGE     %d", w->range);
    pTool->printCursor(7, pTool->init + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

static void edit_light_type_shadow_fix()
{
    cLight* cur = curLight();
    Light04Work* w = (Light04Work*) cur->work;
    f32 step = 5.0f;
    int ret = 1;

    switch (pTool->init) {
    case 0:
        ret = shadow_select_type();
        break;
    case 1:
        SHADOW_ROT_EDIT(rotX);
        break;
    case 2:
        SHADOW_ROT_EDIT(rotY);
        break;
    case 3:
        w->texNo += (int) ((f32) pTool->joy.sy * step / 100.0f);
        w->texNo += (int) ((f32) pTool->joy.sx * step / 100.0f);
        if (w->texNo > 0x80) {
            w->texNo = 0;
        }
        if (w->texNo > 0x5A) {
            w->texNo = 0x5A;
        }
        break;
    }
    if (ret) {
        if (pTool->joy.rep & JOY_UP) {
            pTool->init = (pTool->init + 3) % 4;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->init = (pTool->init + 5) % 4;
        }
    }
    if (pTool->init != 0) {
        eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", cur->xD, shadow_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "ROT_X : %d", w->rotX);
    eprintf(0x40, 0xB6, 0, pTool->color, "ROT_Y : %d", w->rotY);
    eprintf(0x40, 0xC4, 0, pTool->color, "RANGE : %d", w->texNo);
    pTool->printCursor(7, pTool->init + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

static void edit_light_kind()
{
    cLight* cur = curLight();

    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->color, "KIND  %02x", cur->kind);
    if (pTool->joy.rep & JOY_RIGHT) {
        cur->kind++;
    }
    if (pTool->joy.rep & JOY_LEFT) {
        cur->kind--;
    }
    if (pTool->joy.rep & JOY_UP) {
        cur->kind += 0x10;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        cur->kind -= 0x10;
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
}

static void edit_light_attr()
{
    cLight* cur = curLight();

    pTool->printCursor(0x14, pTool->init + 11);
    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->color, "ATTRIBUTE");
    eprintf(0xA8, 0x9A, !(cur->attr & 1) ? 0x14 : 0, pTool->color, "NO TUNE");
    eprintf(0xA8, 0xA8, (cur->attr & 2) ? 0 : 0x14, pTool->color, "ELEC LIGHT");
    eprintf(0xA8, 0xB6, (cur->attr & 4) ? 0 : 0x14, pTool->color, "TAIMATU");
    eprintf(0xA8, 0xC4, (cur->attr & 8) ? 0 : 0x14, pTool->color, "--------");
    if (pTool->joy.rep & JOY_UP) {
        pTool->init = (pTool->init + 3) % 4;
    } else if (pTool->joy.rep & JOY_DOWN) {
        pTool->init = (pTool->init + 5) % 4;
    }
    if (pTool->joy.rep & JOY_A) {
        switch (pTool->init) {
        case 0:
            cur->attr ^= 1;
            break;
        case 1:
            cur->attr ^= 2;
            break;
        case 2:
            cur->attr ^= 4;
            break;
        case 3:
            cur->attr ^= 8;
            break;
        }
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
}

static void edit_light_priority()
{
    cLight* cur = curLight();

    eprintf(0x40, 0x8C, 4, pTool->color, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->color, "PRIORITY %d", cur->x2B);
    if (pTool->joy.rep & (JOY_UP | JOY_RIGHT)) {
        cur->x2B = (cur->x2B + 8) % 7;
    } else if (pTool->joy.rep & (JOY_DOWN | JOY_LEFT)) {
        cur->x2B = (cur->x2B + 6) % 7;
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->sub = 0;
    }
}

int select_type()
{
    cLight* cur = curLight();
    int ret;
    int col;
    int first;

    if (pTool->x8 == 0) {
        pTool->x9 = cur->xD;
        pTool->x8 = 1;
        first = 1;
    } else {
        first = 0;
    }
    if (pTool->joy.rep & JOY_RIGHT) {
        pTool->x9 = (pTool->x9 + 9) % 8;
    }
    if (pTool->joy.rep & JOY_LEFT) {
        pTool->x9 = (pTool->x9 + 7) % 8;
    }
    if (pTool->joy.rep & JOY_A) {
        if (cur->xD != pTool->x9) {
            cur->xD = pTool->x9;
            clear_type_free();
        }
    }
    if (cur->xD == pTool->x9) {
        col = 0;
        ret = 1;
    } else {
        col = 6;
        ret = 0;
    }
    eprintf(0x40, 0x9A, col, pTool->color, "%2d %s", pTool->x9, light_type_name[pTool->x9]);
    return ret;
}

static void edit_light_type_constant()
{
    cLight* cur = curLight();
    int ret = 1;

    switch (pTool->init) {
    case 0:
        ret = select_type();
        break;
    case 1: {
        f32 step = (Joy[0].on & JOY_A) ? 10.0f : 1.0f;
        cur->power += (f32) pTool->joy.sx * step / 10000.0f;
        cur->power += (f32) pTool->joy.sy * step / 10000.0f;
        if (pTool->joy.rep & JOY_RIGHT) {
            cur->power += 0.1f;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            cur->power -= 0.1f;
        }
        if (cur->power < 0.001f) {
            cur->power = 0.001f;
        }
        break;
    }
    }
    if (ret) {
        if (pTool->joy.rep & JOY_UP) {
            pTool->init = (pTool->init + 1) % 2;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->init = (pTool->init + 3) % 2;
        }
    }
    if (pTool->init != 0) {
        eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", cur->xD, light_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "INTENSITY %3.3f", cur->power);
    pTool->printCursor(7, pTool->init + 11);
}

// Quadratic / local ambient: the smooth-edge width in the spot block.
#define EDIT_SMOOTH_EDGE()                                                              \
    cLight* cur = curLight();                                                                   \
    LightSpot* sp = &cur->spot;                                                                 \
    int ret = 1;                                                                                \
                                                                                                \
    switch (pTool->init) {                                                                      \
    case 0:                                                                                     \
        ret = select_type();                                                                    \
        break;                                                                                  \
    case 1: {                                                                                   \
        f32 step = (pTool->joy.on & JOY_A) ? 5.0f : 1.0f;                                       \
        sp->normal.x += (f32) pTool->joy.sx * step;                                             \
        sp->normal.x += (f32) pTool->joy.sy * step;                                             \
        if (pTool->joy.trg & JOY_Y) {                                                           \
            sp->normal.x = 0.0f;                                                                \
        }                                                                                       \
        break;                                                                                  \
    }                                                                                           \
    }                                                                                           \
    if (ret) {                                                                                  \
        if (pTool->joy.rep & JOY_UP) {                                                          \
            pTool->init = (pTool->init + 1) % 2;                                                \
        }                                                                                       \
        if (pTool->joy.rep & JOY_DOWN) {                                                        \
            pTool->init = (pTool->init + 3) % 2;                                                \
        }                                                                                       \
    }                                                                                           \
    if (pTool->init != 0) {                                                                     \
        eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", cur->xD, light_type_name[cur->xD]);      \
    }                                                                                           \
    eprintf(0x40, 0xA8, 0, pTool->color, "SMOOTH EDGE %6f", sp->normal.x);                      \
    pTool->printCursor(7, pTool->init + 11);                                                    \
    drawLightInfo(cur, 0xFFFFFFFF);

static void edit_light_type_quad()
{
    EDIT_SMOOTH_EDGE();
}

static void edit_light_type_spotlight()
{
    // direction editor scratch: the unit's first .bss object (a function static of the first function
    // with one; the .sym's "global" scope at .bss+0 cannot be told from a local S+A field of 0)
    static Vec spotRot;
    cLight* cur = curLight();
    LightSpot* sp = &cur->spot;
    int ret = 1;
    Mtx m;
    Vec pos;
    Vec dir;
    f32 step;

    switch (pTool->init) {
    case 0:
        ret = select_type();
        break;
    case 1:
        if (PSVECMag(&sp->normal) < 0.9f) {
            sp->normal.x = 0.0f;
            sp->normal.y = 0.0f;
            sp->normal.z = 1.0f;
        }
        spotRot.x = 0.0f;
        {
            f32 sx = -(f32) pTool->joy.sx / 1000.0f;
            Vec* rot = &spotRot;
            rot->z = 0.0f;
            rot->y = sx;
            RotMatrix(m, rot);
            PSMTXMultVec(m, &sp->normal, &sp->normal);
            PSVECCrossProduct(&sp->normal, (const Vec*) yAxis, rot);
            PSMTXRotAxisRad(m, rot, (f32) pTool->joy.sy / 1000.0f);
            PSMTXMultVec(m, &sp->normal, &sp->normal);
        }
        if (sp->normal.x == 0.0f && sp->normal.y == 0.0f && sp->normal.z == 0.0f) {
#line 2977 "D:/Bio4/Prog/db_light.cpp"
            pLog->err(0, 0, "VECNormalize:[%s/%d]", __FILE__, __LINE__);
            sp->normal.x = sp->normal.y = sp->normal.z = 0.0f;
        } else {
            PSVECNormalize(&sp->normal, &sp->normal);
        }
        break;
    case 2: {
        step = (pTool->joy.on & JOY_A) ? 3.0f : 1.0f;
        sp->cutoff += (f32) pTool->joy.sy * step / 100.0f;
        sp->cutoff += (f32) pTool->joy.sx * step / 100.0f;
        if (sp->cutoff < 1.0f) {
            sp->cutoff = 1.0f;
        }
        if (sp->cutoff > 90.0f) {
            sp->cutoff = 90.0f;
        }
        spotRot.x = 0.0f;
        spotRot.y = 0.0f;
        spotRot.z = 0.0f;
        spotRot.x = atan2f(-sp->normal.y, sp->normal.z);
        break;
    }
    case 3: {
        step = (pTool->joy.on & JOY_A) ? 5.0f : 1.0f;
        sp->fade += (f32) pTool->joy.sx * step;
        sp->fade += (f32) pTool->joy.sy * step;
        if (pTool->joy.trg & JOY_Y) {
            sp->fade = 0.0f;
        }
        break;
    }
    }
    if (ret) {
        if (pTool->joy.rep & JOY_UP) {
            pTool->init = (pTool->init + 3) & 3;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->init = (pTool->init + 5) & 3;
        }
    }
    if (Joy[0].on & JOY_A) {
        f32 r;
        cur->getPos(&pos);
        cur->getNormal(&sp->normal, &dir);
        if (cur->x1C == 0.0f) {
            r = 3000.0f;
        } else {
            r = cur->x1C;
        }
        PSVECScale(&dir, &dir, r);
        Draw_corn3(&pos, &dir, sp->cutoff, 0xFFFFFFFF);
    }
    if (pTool->init != 0) {
        eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", cur->xD, light_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "DIRECTION");
    eprintf(0x40, 0xB6, 0, pTool->color, "SPOT LIGHT RANGE %3.2f", sp->cutoff);
    eprintf(0x40, 0xC4, 0, pTool->color, "SMOOTH EDGE %6f", sp->fade);
    pTool->printCursor(7, pTool->init + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// Custom attenuation editor: A0..K2 of the spot block, stepped by the gear table.
#define EDIT_DIRECT_PARAM(field)                                              \
    sp->field += (f32) pTool->joy.sx * gear_step[gear] * step;                \
    if (pTool->joy.trg & JOY_Y) {                                             \
        sp->field = 0.0f;                                                     \
    }

static void edit_light_type_direct()
{
    static Vec rot;
    static int gear;
    static f32 gear_step[4] = {1e-9f, 1e-7f, 1e-5f, 1e-3f};
    cLight* cur = curLight();
    LightSpot* sp = &cur->spot;
    f32 step = (pTool->joy.on & JOY_A) ? 100.0f : 1.0f;
    int ret = 1;
    Mtx m;

    if (PSVECMag(&sp->normal) < 0.9f) {
        sp->normal.x = 0.0f;
        sp->normal.z = 1.0f;
        sp->normal.y = 0.0f;
    }
    switch (pTool->init) {
    case 0:
        ret = select_type();
        gear = 0;
        break;
    case 1:
        EDIT_DIRECT_PARAM(cutoff);
        break;
    case 2:
        EDIT_DIRECT_PARAM(fade);
        break;
    case 3:
        EDIT_DIRECT_PARAM(a2);
        break;
    case 4:
        EDIT_DIRECT_PARAM(k0);
        break;
    case 5:
        EDIT_DIRECT_PARAM(k1);
        break;
    case 6:
        EDIT_DIRECT_PARAM(k2);
        break;
    case 7:
        if (PSVECMag(&sp->normal) < 0.9f) {
            sp->normal.x = 0.0f;
            sp->normal.z = 1.0f;
            sp->normal.y = 0.0f;
        }
        rot.x = 0.0f;
        {
            f32 sx = -(f32) pTool->joy.sx / 1000.0f;
            Vec* r = &rot;
            r->z = 0.0f;
            r->y = sx;
            RotMatrix(m, r);
            PSMTXMultVec(m, &sp->normal, &sp->normal);
            PSVECCrossProduct(&sp->normal, (const Vec*) yAxis, r);
            PSMTXRotAxisRad(m, r, (f32) pTool->joy.sy / 1000.0f);
            PSMTXMultVec(m, &sp->normal, &sp->normal);
        }
        if (sp->normal.x == 0.0f && sp->normal.y == 0.0f && sp->normal.z == 0.0f) {
#line 3099 "D:/Bio4/Prog/db_light.cpp"
            pLog->err(0, 0, "VECNormalize:[%s/%d]", __FILE__, __LINE__);
            sp->normal.z = 0.0f;
            sp->normal.y = 0.0f;
            sp->normal.x = 0.0f;
        } else {
            PSVECNormalize(&sp->normal, &sp->normal);
        }
        drawLightInfo(cur, 0xFFFFFFFF);
        break;
    }
    if (ret) {
        if (pTool->joy.rep & JOY_UP) {
            pTool->init = (pTool->init + 7) % 8;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->init = (pTool->init + 9) % 8;
        }
    }
    if (pTool->joy.rep & JOY_RIGHT) {
        gear = (gear + 1) % 4;
    }
    if (pTool->joy.rep & JOY_LEFT) {
        gear = (gear + 3) % 4;
    }
    pTool->printCursor(7, pTool->init + 11);
    if (pTool->init != 0) {
        eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", cur->xD, light_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "A0 %5.4f", sp->cutoff);
    eprintf(0x40, 0xB6, 0, pTool->color, "A1 %5.4f", sp->fade);
    eprintf(0x40, 0xC4, 0, pTool->color, "A2 %5.4f", sp->a2);
    eprintf(0x40, 0xD2, 0, pTool->color, "K0 %5.4f", sp->k0);
    eprintf(0x40, 0xE0, 0, pTool->color, "K1 %5.4f", sp->k1);
    eprintf(0x40, 0xEE, 0, pTool->color, "K2 %5.4f", sp->k2);
    eprintf(0x40, 0xFC, 0, pTool->color, "NORMAL");
    eprintf(0x40, 0x118, 0, pTool->color, "GEAR %d", gear + 1);
    draw_light_graph(cur);
}

static void edit_light_type_localamb()
{
    EDIT_SMOOTH_EDGE();
}

// Attenuation of a custom light at distance d (the angular term is evaluated at 0).
f32 func_attn(cLight* l, f32 d)
{
    LightSpot* s = &l->spot;
    f32 c = 0.0f;
    return (s->cutoff + s->fade * c + s->a2 * c) / (s->k0 + d * s->k1 + d * d * s->k2);
}

// Attenuation curve of a custom light: a gx x gy .. gw x gh graph, the player distance and 1000-unit marks.
// (the static names decide the gcse hash order of their `high` pseudos and thus the r20/r21
// assignment of the loop's gx/gy address registers: x0/y0 reproduce it, gx/gy do not)
void draw_light_graph(cLight* l)
{
    static f32 x0 = 180.0f;
    static f32 y0 = 250.0f;
    static f32 w0 = 300.0f;
    static f32 h0 = 200.0f;
    static f32 s0 = 100.0f;
    Vec a;
    Vec b;
    f32 scale;
    int i;
    f32 x;
    f32 t;
    f32 v;
    u32 lcol;

    if (l->x1C != 0.0f) {
        scale = l->x1C / w0;
    } else {
        scale = 10000000.0f / w0;
    }
    a.x = x0;
    a.y = y0;
    a.z = 0.0f;
    b.x = x0 + w0;
    b.y = y0;
    b.z = 0.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    a.x = x0;
    a.y = y0;
    a.z = 0.0f;
    b.x = x0;
    b.y = y0 - h0;
    b.z = 0.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    for (i = 1; i < (int) w0; i++) {
        v = func_attn(l, (f32) i * scale) * s0;
        if (v > h0) {
            v = h0;
        }
        a.x = x0 + (f32) i;
        a.y = y0 - v;
        a.z = 0.0f;
        v = func_attn(l, (f32) (i + 1) * scale) * s0;
        if (v > h0) {
            v = h0;
        }
        b.x = x0 + (f32) (i + 1);
        b.y = y0 - v;
        b.z = 0.0f;
        Draw_line(&a, &b, 0xE0E0E0E0);
    }
    a = pPL->pos;
    a.y += 1200.0f;
    x = GetDistance3(&l->pos, &a);
    if (x < l->x1C || l->x1C == 0.0f) {
        t = x / scale;
        a.x = x0 + t;
        a.y = y0;
        a.z = 0.0f;
        b.x = x0 + t;
        b.y = y0 - h0;
        b.z = 0.0f;
        lcol = 0xFFFF0000;
    } else {
        a.x = x0 + w0;
        a.y = y0;
        a.z = 0.0f;
        b.x = x0 + w0;
        b.y = y0 - h0;
        b.z = 0.0f;
        lcol = 0xFF000080;
    }
    Draw_line(&a, &b, lcol);
    eprintf((int) x0 + 0x78, (int) y0 + 8, 0, pTool->color, "%3.6f", func_attn(l, x));
    for (x = 1000.0f; x < l->x1C || l->x1C == 0.0f; x += 1000.0f) {
        a.x = x0 + x / scale;
        a.y = y0;
        a.z = 0.0f;
        b.x = x0 + x / scale;
        b.y = y0 - h0;
        b.z = 0.0f;
        Draw_line(&a, &b, 0x80808080);
    }
    eprintf((int) x0, (int) y0 + 8, 0, pTool->color, "%1.6f", func_attn(l, 1.0f));
    v = func_attn(l, w0 * scale);
    {
        // COMPILER-DIFF: 2 + #17: the original's colour lives in r5 (a copy preference ours never gets)
        // and is zero-extended for the int argument before the nested call; the pin gives both.
        register int col5 asm("r5");
        col5 = 0;
        if (v > 0.04f) {
            col5 = 6;
        }
        eprintf((int) x0 + 0xE6, (int) y0 + 8, (u8) col5, pTool->color, "%3.6f", func_attn(l, w0 * scale));
    }
}
// Parallel light: the direction is edited as two angles (static `ang`: x = pitch, y = yaw, z unused),
// converted back to the unit normal (scaled by 1e6 in the light).
static void edit_light_type_parallel()
{
    static Vec ang;
    const f32 k = 1000000.0f;  // pool order: 1e6 before the step constants
    cLight* cur = curLight();
    LightSpot* sp = &cur->spot;
    f32 step = (pTool->joy.on & JOY_A) ? 10.0f : 1.0f;
    int ret = 1;

    switch (pTool->init) {
    case 0: {
        Vec* a = &ang;
        f32 cx;
        f32 cz;
        pTool->cursor = 0;
        a->x = asinf(sp->normal.y / 1000000.0f);
        cx = sp->normal.x / 1000000.0f;
        cx = cx / cosf(a->x);
        cz = sp->normal.z / 1000000.0f;
        cz = cz / cosf(a->x);
        a->y = atan2f(cx, cz);
        a->z = 0.0f;
        pTool->init = 1;
    }
    case 1: {
        Vec* a = &ang;
        f32 c;
        f32 d;
        a->x = LIMIT_ANGLE(a->x);
        a->y = LIMIT_ANGLE(a->y);
        c = cosf(a->x);
        c *= sinf(a->y);
        c *= k;
        sp->normal.x = c;
        sp->normal.y = sinf(a->x) * k;
        d = cosf(a->x);
        d *= cosf(a->y);
        d *= k;
        sp->normal.z = d;
        break;
    }
    }
    switch (pTool->cursor) {
    case 0:
        ret = select_type();
        break;
    case 1: {
        Vec* a = &ang;
        a->y += (f32) pTool->joy.sx * step / 1000.0f;
        a->y += (f32) pTool->joy.sy * step / 1000.0f;
        if (pTool->joy.trg & JOY_Y) {
            a->y = 0.0f;
        }
        break;
    }
    case 2:
        ang.x += (f32) pTool->joy.sx * step / 1000.0f;
        ang.x += (f32) pTool->joy.sy * step / 1000.0f;
        if (pTool->joy.trg & JOY_Y) {
            ang.x = 0.0f;
        }
        break;
    case 3:
        if (pTool->joy.rep & JOY_A) {
            sp->flags ^= 1;
        }
        break;
    case 4:
        step = (pTool->joy.on & JOY_A) ? 5.0f : 1.0f;
        sp->fade += (f32) pTool->joy.sx * step;
        sp->fade += (f32) pTool->joy.sy * step;
        if (pTool->joy.trg & JOY_Y) {
            sp->fade = 0.0f;
        }
        break;
    }
    if (ret) {
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 4) % 5;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 6) % 5;
        }
    }
    pTool->printCursor(7, pTool->cursor + 11);
    if (pTool->cursor != 0) {
        eprintf(0x40, 0x9A, 0, pTool->color, "%2d %s", cur->xD, light_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->color, "DIR Y:%3.0f", ang.y * 180.0f / 3.1415927f);
    eprintf(0x40, 0xB6, 0, pTool->color, "DIR X:%3.0f", ang.x * 180.0f / 3.1415927f);
    eprintf(0x40, 0xC4, (sp->flags & 1) ? 0 : 0x14, pTool->color, "LOCAL DIR");
    eprintf(0x40, 0xD2, 0, pTool->color, "SMOOTH EDGE %6f", sp->fade);
}
static void edit_light_prop_sub() {}
// Ambient colours of the cut: model / enemy+object / effect.
static void edit_ambient()
{
    static u32 amb_copy = 0;
    cLightEnv* env = LightMgr.getEnvPtr();
    int ret = 0;

    eprintf(0x20, 0x2A, 4, pTool->color, "AMBIENT");
    switch (pTool->x8) {
    case 0:
        pTool->x8 = 1;
        pTool->x9 = 0;
    case 1:
        if (pTool->joy.rep & JOY_UP) {
            pTool->x9 = (pTool->x9 + 2) % 3;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->x9 = (pTool->x9 + 4) % 3;
        }
        if (pTool->joy.rep & JOY_A) {
            pTool->x8 = 2;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->editNo = pTool->sub = 0;
            pTool->clearWork();
        } else if (pTool->joy.trg & JOY_Y) {
            pTool->x8 = 3;
            pTool->xA = 0;
        }
        break;
    case 2:
        switch (pTool->x9) {
        case 0:
            ret = editColor(4, 8, &env->amb);
            break;
        case 1:
            ret = editColor(4, 8, &env->ambSub);
            break;
        case 2:
            ret = editColor(4, 8, &env->ambEsp);
            break;
        }
        if (ret == 0) {
            pTool->x8 = 1;
        }
        break;
    case 3:
        eprintf(0x140, 0x46, 4, pTool->color, "SUB MENU");
        eprintf(0x140, 0x54, 0, pTool->color, "COPY");
        eprintf(0x140, 0x62, 0, pTool->color, "PASTE");
        pTool->printCursor(0x27, pTool->xA + 6);
        if (pTool->joy.rep & JOY_UP) {
            pTool->xA = (pTool->xA + 1) % 2;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->xA = (pTool->xA + 3) % 2;
        }
        if ((pTool->joy.rep & JOY_A) || (pTool->joy.trg & JOY_Y)) {
            switch (pTool->xA) {
            case 0:
                switch (pTool->x9) {
                case 0:
                    amb_copy = env->x0;
                    break;
                case 1:
                    amb_copy = env->xFC;
                    break;
                case 2:
                    amb_copy = env->x100;
                    break;
                }
                break;
            case 1:
                switch (pTool->x9) {
                case 0:
                    env->x0 = amb_copy;
                    break;
                case 1:
                    env->xFC = amb_copy;
                    break;
                case 2:
                    env->x100 = amb_copy;
                    break;
                }
                break;
            }
            pTool->x8 = 1;
        }
        if (pTool->joy.rep & JOY_B) {
            pTool->x8 = 1;
        }
        break;
    }
    eprintf(0x20, 0x38, 0, pTool->color, "SCROLL");
    drawColorTile(0x60, 0x38, 0x30, 0xD, env->x0);
    eprintf(0x20, 0x46, 0, pTool->color, "EM+OBJ");
    drawColorTile(0x60, 0x46, 0x30, 0xD, env->xFC);
    eprintf(0x20, 0x54, 0, pTool->color, "EFFECT");
    drawColorTile(0x60, 0x54, 0x30, 0xD, env->x100);
    pTool->printCursor(3, pTool->x9 + 4);
}
static void edit_fog()
{
    cLightEnv* env = LightMgr.getEnvPtr();

    eprintf(0x20, 0x2A, 4, pTool->color, "FOG");
    edit_fog_common(&env->fog);
}

static void edit_mirror_fog()
{
    cLightEnv* env = LightMgr.getEnvPtr();

    eprintf(0x20, 0x2A, 4, pTool->color, "MIRROR FOG");
    edit_fog_common(&env->mfog);
}

void edit_fog_common(LightFog* fog)
{
    f32 step = (pTool->joy.on & JOY_A) ? 20.0f : 1.0f;
    cLightEnv* env = LightMgr.getEnvPtr();

    switch (pTool->sub) {
    case 0:
        pTool->cursor = 0;
        pTool->sub = 1;
    case 1:
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 4) % 5;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 6) % 5;
        }
        switch (pTool->cursor) {
        case 0:
            if (pTool->joy.rep & JOY_RIGHT) {
                fog->type = fogTypeNext(fog->type);
            }
            if (pTool->joy.rep & JOY_LEFT) {
                fog->type = fogTypeBack(fog->type);
            }
            break;
        case 1:
            fog->start += (f32) pTool->joy.sx * step;
            break;
        case 2:
            fog->end += (f32) pTool->joy.sx * step;
            break;
        case 3:
            if (pTool->joy.rep & JOY_A) {
                pTool->cursor = 0;
                pTool->sub = 2;
            }
        case 4:
            env->farRate += (f32) pTool->joy.sx * step * 0.0001f;
            if (pTool->joy.rep & JOY_LEFT) {
                env->farRate -= 0.1f;
            }
            if (pTool->joy.rep & JOY_RIGHT) {
                env->farRate += 0.1f;
            }
            if (env->farRate > 1.0f) {
                env->farRate = 1.0f;
            }
            if (env->farRate < 0.0f) {
                env->farRate = 0.0f;
            }
            break;
        }
        if (fog->end < fog->start) {
            fog->end = fog->start + 1.0f;
        }
        if (pTool->joy.rep & JOY_B) {
            pTool->sub = 0;
            pTool->editNo = 0;
        }
        break;
    case 2:
        if (editColor(0x14, 10, &fog->color) == 0) {
            pTool->cursor = 3;
            pTool->sub = 0;
        }
        break;
    }
    eprintf(0x20, 0x38, 0, pTool->color, "TYPE     %s", strFogType(fog->type));
    eprintf(0x20, 0x46, 0, pTool->color, "START    %6.0f", fog->start);
    eprintf(0x20, 0x54, 0, pTool->color, "END      %6.0f", fog->end);
    eprintf(0x20, 0x62, 0, pTool->color, "COLOR");
    eprintf(0x20, 0x70, 0, pTool->color, "FAR PLAY %1.2f", env->farRate);
    drawColorTile(0x50, 0x62, 0x30, 0xE, *(u32*) &fog->color);
    LightMgr.setFog();
}
static void edit_focus()
{
    static const char* focus_mode_name[] = {"NEAR", "FAR", "FollowPL NEAR", "FollowPL FAR"};
    cLightEnv* env = LightMgr.getEnvPtr();
    f32 step = (pTool->joy.on & JOY_A) ? 10.0f : 1.0f;

    switch (pTool->sub) {
    case 0:
        pTool->cursor = 0;
        pTool->sub = 1;
    case 1:
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 2) % 3;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 4) % 3;
        }
        switch (pTool->cursor) {
        case 0:
            env->x28 += (int) ((f32) pTool->joy.sx * step);
            break;
        case 1:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->x2D = (env->x2D + 12) % 11u;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->x2D = (env->x2D + 10) % 11u;
            }
            break;
        case 2:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->x2E++;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->x2E--;
            }
            env->x2E &= 3;
            break;
        }
        if (pTool->joy.rep & JOY_B) {
            pTool->sub = 0;
            pTool->editNo = 0;
        }
        break;
    }
    eprintf(0x20, 0x2A, 4, pTool->color, "FOCUS");
    eprintf(0x20, 0x38, 0, pTool->color, "DIST %7d", env->x28);
    eprintf(0x20, 0x46, 0, pTool->color, "LEVEL %d", env->x2D);
    eprintf(0x20, 0x54, 0, pTool->color, "MODE  %s", focus_mode_name[env->x2E]);
}
// Contrast tone curve of the blur filter: axes, the (in, out) knee and the end segments.
void draw_tone_curve()
{
    static f32 sz = 0.25f;
    static f32 gamma2 = 0.25f;
    static f32 gamma3 = 0.1f;
    cLightEnv* env = LightMgr.getEnvPtr();
    Vec a;
    Vec b;
    f32 g;
    f32 in;
    f32 out;

    a.x = 0.0f; a.y = 0.0f; a.z = 0.0f; b.x = 0.0f; b.y = 0.0f; b.z = 0.0f;
    if ((u8) env->contrast[0] == 0) {
        return;
    }
    a.x = sz * 255.0f + 64.0f;
    a.y = 150.0f;
    b = a;
    b.x += sz * 255.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    b = a;
    b.y -= sz * 255.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    b = a;
    b.x += (f32) (int) (u8) env->contrast[2] * sz;
    b.y -= (f32) (int) (u8) env->contrast[2] * sz;
    Draw_line(&a, &b, 0xFFFFFFFF);
    g = 0.5f;
    if ((u8) env->contrast[0] == 2) {
        g = gamma2;
    }
    if ((u8) env->contrast[0] == 3) {
        g = gamma3;
    }
    a = b;
    out = (255.0f - (f32) (int) (u8) env->contrast[2]) / 255.0f;
    in = (f32) (int) (u8) env->contrast[1] / 255.0f;
    out = out - out * ((1.0f - g) * in);
    b.x += out * 255.0f * sz;
    b.y -= (255.0f - (f32) (int) (u8) env->contrast[2]) * sz;
    Draw_line(&a, &b, 0xFFFFFFFF);
    a = b;
    b.x = sz * 255.0f + 64.0f + sz * 255.0f;
    b.y = 150.0f - sz * 255.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
}
// Blur filter of the cut: type / rate / power and the contrast level / power / bias.
static void edit_blur()
{
    int step = (pTool->joy.on & JOY_A) ? 10 : 1;
    cLightEnv* env = LightMgr.getEnvPtr();
    f32 fstep = (pTool->joy.on & JOY_A) ? 1.0f : 0.1f;
    const char* type_name[] = {"NORMAL", "SPREAD", "ADD", "SUBTRACT"};
    f32 f;

    eprintf(0x20, 0x2A, 4, pTool->color, "BLUR");
    if (pTool->cursor <= 2) {
        pTool->printCursor(3, pTool->cursor + 4);
    } else {
        pTool->printCursor(3, pTool->cursor + 6);
    }
    if (pTool->joy.rep & JOY_UP) {
        pTool->cursor = (pTool->cursor + 5) % 6;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        pTool->cursor = (pTool->cursor + 7) % 6;
    }
    switch (pTool->cursor) {
    case 0:
        env->blurType += (u8) ((f32) pTool->joy.sx * fstep);
        if (pTool->joy.rep & JOY_RIGHT) {
            env->blurType += step;
        }
        if (env->blurType > 2) {
            env->blurType = 0;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            env->blurType -= step;
        }
        if (env->blurType > 2) {
            env->blurType = 2;
        }
        break;
    case 1:
        f = (f32) env->blurAlpha + (f32) pTool->joy.sx * fstep;
        if (f < 0.0f) {
            f = 0.0f;
        }
        if (f > 255.0f) {
            f = 255.0f;
        }
        env->blurAlpha = f;
        if (pTool->joy.rep & JOY_RIGHT) {
            env->blurAlpha += step;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            env->blurAlpha -= step;
        }
        break;
    case 2:
        env->blurPower += (s8) ((f32) pTool->joy.sx * fstep * 0.2f);
        if (pTool->joy.rep & JOY_RIGHT) {
            env->blurPower += step;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            env->blurPower -= step;
        }
        break;
    case 3:
        f = (f32) (u8) env->contrast[0] + (f32) pTool->joy.sx * fstep;
        if (f < 0.0f) {
            f = 0.0f;
        }
        if (f > 3.0f) {
            f = 3.0f;
        }
        env->contrast[0] = (u8) f;
        if ((u8) env->contrast[0] != 3 && (pTool->joy.rep & JOY_RIGHT)) {
            env->contrast[0] += step;
        }
        if ((u8) env->contrast[0] != 0 && (pTool->joy.rep & JOY_LEFT)) {
            env->contrast[0] -= step;
        }
        if ((u8) env->contrast[0] > 3) {
            env->contrast[0] = 3;
        }
        break;
    case 4:
        f = (f32) (u8) env->contrast[1] + (f32) pTool->joy.sx * fstep;
        if (f < 0.0f) {
            f = 0.0f;
        }
        if (f > 255.0f) {
            f = 255.0f;
        }
        env->contrast[1] = (u8) f;
        if (pTool->joy.rep & JOY_RIGHT) {
            env->contrast[1] += step;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            env->contrast[1] -= step;
        }
        break;
    case 5:
        f = (f32) (u8) env->contrast[2] + (f32) pTool->joy.sx * fstep;
        if (f < 0.0f) {
            f = 0.0f;
        }
        if (f > 255.0f) {
            f = 255.0f;
        }
        env->contrast[2] = (u8) f;
        if (pTool->joy.rep & JOY_RIGHT) {
            env->contrast[2] += step;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            env->contrast[2] -= step;
        }
        break;
    }
    if (env->blurType > 2) {
        env->blurType = 0;
    }
    eprintf(0x20, 0x38, 0, pTool->color, "TYPE   %s", type_name[env->blurType]);
    if (env->blurAlpha == 0) {
        eprintf(0x20, 0x46, 0, pTool->color, "RATE   OFF");
    } else {
        eprintf(0x20, 0x46, 0, pTool->color, "RATE   %d", env->blurAlpha);
    }
    eprintf(0x20, 0x54, 0, pTool->color, "POW    %d", env->blurPower);
    eprintf(0x20, 0x70, 4, pTool->color, "CONTRAST");
    if ((u8) env->contrast[0] == 0) {
        eprintf(0x20, 0x7E, 0, pTool->color, "LEVEL  OFF");
    } else {
        eprintf(0x20, 0x7E, 0, pTool->color, "LEVEL  %d", (u8) env->contrast[0]);
    }
    eprintf(0x20, 0x8C, 0, pTool->color, "POW    %d", (u8) env->contrast[1]);
    eprintf(0x20, 0x9A, 0, pTool->color, "BIAS   %d", (u8) env->contrast[2]);
    if (pTool->joy.rep & JOY_B) {
        pTool->editNo = 0;
    }
    draw_tone_curve();
    LightMgr.setEnv(env, -1);
}
// Mipmap settings of the cut: min / max LOD, LOD bias and anisotropy.
static void edit_mipmap()
{
    cLightEnv* env = LightMgr.getEnvPtr();
    f32 step = (pTool->joy.on & JOY_A) ? 0.01f : 0.001f;

    eprintf(0x20, 0x2A, 4, pTool->color, "MIPMAP");
    switch (pTool->sub) {
    case 0:
        pTool->cursor = 0;
        pTool->sub = 1;
        env->minLod %= 10;
        env->maxLod %= 10;
    case 1:
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 3) % 4;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 5) % 4;
        }
        switch (pTool->cursor) {
        case 0:
            if ((pTool->joy.rep & JOY_RIGHT) && env->minLod <= 4) {
                env->minLod++;
            }
            if ((pTool->joy.rep & JOY_LEFT) && env->minLod != 0) {
                env->minLod--;
            }
            break;
        case 1:
            if ((pTool->joy.rep & JOY_RIGHT) && env->maxLod <= 4) {
                env->maxLod++;
            }
            if ((pTool->joy.rep & JOY_LEFT) && env->maxLod != 0) {
                env->maxLod--;
            }
            break;
        case 2:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->lodBias += 1.0f;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->lodBias -= 1.0f;
            }
            if (pTool->joy.trg & JOY_Y) {
                env->lodBias = 0.0f;
            }
            env->lodBias += (f32) pTool->joy.sx * step;
            if (env->lodBias < -4.0f) {
                env->lodBias = -4.0f;
            }
            if (env->lodBias > 3.99f) {
                env->lodBias = 3.99f;
            }
            break;
        case 3:
            if ((pTool->joy.rep & JOY_RIGHT) && env->aniso <= 1) {
                env->aniso++;
            }
            if ((pTool->joy.rep & JOY_LEFT) && env->aniso != 0) {
                env->aniso--;
            }
            break;
        }
        if (pTool->joy.rep & JOY_B) {
            pTool->sub = 0;
            pTool->editNo = 0;
        }
        break;
    }
    eprintf(0x20, 0x38, 0, pTool->color, "MIN LOD %d", env->minLod);
    eprintf(0x20, 0x46, 0, pTool->color, "MAX LOD %d", env->maxLod);
    eprintf(0x20, 0x54, 0, pTool->color, "LODBIAS %3.2f", env->lodBias);
    eprintf(0x20, 0x62, 0, pTool->color, "ANISO   %s", aniso_name[env->aniso]);
    LightMgr.setMipmap(env);
}
// Lit tune: the room / core switch, the three tune colours and the manager's colour blend rate.
static void edit_tune()
{
    static const char* tune_name[] = {"", "LIGHT", "AMBIENT", "EFFECT"};
    cLightEnv* env = LightMgr.getEnvPtr();
    u32 i;
    const char** name;
    f32 d;

    switch (pTool->sub) {
    case 0:
        pTool->cursor = 0;
        pTool->sub = 1;
    case 1:
        switch (pTool->cursor) {
        case 0:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->tuneOn &= ~1;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->tuneOn |= 1;
            }
            break;
        case 1:
            if (pTool->joy.rep & JOY_A) {
                pTool->cursor = 0;
                pTool->sub = 2;
            }
            break;
        case 2:
            if (pTool->joy.rep & JOY_A) {
                pTool->cursor = 0;
                pTool->sub = 3;
            }
            break;
        case 3:
            if (pTool->joy.rep & JOY_A) {
                pTool->cursor = 0;
                pTool->sub = 4;
            }
            break;
        }
        d = (f32) pTool->joy.sx * 0.0005f;
        LightMgr.colBrendRate += (pTool->joy.on & JOY_A) ? d * 10.0f : d;
        if (LightMgr.colBrendRate < 0.0f) {
            LightMgr.colBrendRate = 0.0f;
        }
        if (LightMgr.colBrendRate > 1.0f) {
            LightMgr.colBrendRate = 1.0f;
        }
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 3) & 3;
        } else if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 5) & 3;
        }
        if (!(env->tuneOn & 1)) {
            pTool->cursor = 0;
        }
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->joy.rep & JOY_B) {
            pTool->sub = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        }
        break;
    case 2:
        if (editColor(4, 0x14, &env->tune[0]) == 0) {
            pTool->sub = 1;
        }
        break;
    case 3:
        if (editColor(4, 0x14, &env->tune[1]) == 0) {
            pTool->sub = 1;
        }
        break;
    case 4:
        if (editColor(4, 0x14, &env->tune[2]) == 0) {
            pTool->sub = 1;
        }
        break;
    }
    eprintf(0x20, 0x2A, 4, pTool->color, "LIT TUNE");
    eprintf(0x20, 0x38, !(env->tuneOn & 1) ? 0x14 : 0, pTool->color, "ROOM");
    eprintf(0x40, 0x38, 0, pTool->color, "/");
    eprintf(0x48, 0x38, (env->tuneOn & 1) ? 0x14 : 0, pTool->color, "CORE");
    eprintf(0x78, 0x38, 0, pTool->color, "%3.0f%%", LightMgr.colBrendRate * 100.0f);
    name = tune_name;
    for (i = 0; i < 4; i++) {
        eprintf(0x20, 0x38 + i * 0xE, 0, pTool->color, *name++);
    }
    if (env->tuneOn & 1) {
        drawColorTile(0x60, 0x49, 0x38, 8, *(u32*) &env->tune[0]);
        drawColorTile(0x60, 0x57, 0x38, 8, *(u32*) &env->tune[1]);
        drawColorTile(0x60, 0x65, 0x38, 8, *(u32*) &env->tune[2]);
    } else {
        drawColorTile(0x60, 0x49, 0x38, 8, 0xC8C0F080);
        drawColorTile(0x60, 0x57, 0x38, 8, 0);
        drawColorTile(0x60, 0x65, 0x38, 8, 0);
    }
    LightMgr.setTune(env);
}
// TEV colour scale of the models and of the player.
static void edit_scale()
{
    static const char* scale_name[] = {"x1", "x2", "x4", "err"};
    cLightEnv* env = LightMgr.getEnvPtr();

    switch (pTool->sub) {
    case 0:
        pTool->cursor = 0;
        pTool->sub = 1;
        break;
    case 1:
        switch (pTool->cursor) {
        case 0:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->tevScale[0] = (env->tevScale[0] + 4) % 3;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->tevScale[0] = (env->tevScale[0] + 2) % 3;
            }
            break;
        case 1:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->tevScale[1] = (env->tevScale[1] + 4) % 3;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->tevScale[1] = (env->tevScale[1] + 2) % 3;
            }
            break;
        }
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 1) & 1;
        } else if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 3) & 1;
        }
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->joy.rep & JOY_B) {
            pTool->sub = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        }
        break;
    }
    eprintf(0x20, 0x2A, 4, pTool->color, "LIT TUNE");
    eprintf(0x20, 0x38, 0, pTool->color, "MODEL  TEV SCALE %s", scale_name[env->tevScale[0]]);
    eprintf(0x20, 0x46, 0, pTool->color, "PLAYER TEV SCALE %s", scale_name[env->tevScale[1]]);
    LightMgr.setEnv(env, -1);
}
// Fog interpolation frames.
static void edit_param()
{
    cLightEnv* env = LightMgr.getEnvPtr();

    switch (pTool->sub) {
    case 0:
        pTool->cursor = 0;
        pTool->sub = 1;
        break;
    case 1:
        switch (pTool->cursor) {
        case 0:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->hokan = (env->hokan + 251) % 250;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->hokan = (env->hokan + 249) % 250;
            }
            break;
        }
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 0) % 1;
        } else if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 2) % 1;
        }
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->joy.rep & JOY_B) {
            pTool->sub = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        }
        break;
    }
    eprintf(0x20, 0x2A, 4, pTool->color, "PARAMETER");
    eprintf(0x20, 0x38, 0, pTool->color, "HOKAN %d", env->hokan);
}
// Cloth wind of the cut: direction (set from the stick through the camera), power, frequency.
// Draws the wind as an arrow at the camera target.
static void edit_wind()
{
    cLightEnv* env = LightMgr.getEnvPtr();
    Vec stick;
    Vec a;
    Vec b;
    Vec c;
    Vec rot;

    switch (pTool->sub) {
    case 0:
        pTool->cursor = 0;
        pTool->sub = 1;
        break;
    case 1:
        switch (pTool->cursor) {
        case 0:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->wind.dir++;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->wind.dir--;
            }
            CamStick2World(&pG->Cam, &Joy[0], &stick);
            if (Joy[0].on & 0xF0000) {
                env->wind.dir = (int) (atan2(stick.x, stick.z) * 127.0 / 3.14159265f);  // f32 PI widened: pool 0x400921FB60000000
            }
            break;
        case 1:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->wind.power++;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->wind.power--;
            }
            env->wind.power += (Joy[0].sx + Joy[0].sy) / 10;
            if (pTool->joy.trg & JOY_Y) {
                env->wind.power = 0;
            }
            break;
        case 2:
            if (pTool->joy.rep & JOY_RIGHT) {
                env->wind.x2++;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                env->wind.x2--;
            }
            env->wind.x2 += (Joy[0].sx + Joy[0].sy) / 10;
            if (pTool->joy.trg & JOY_Y) {
                env->wind.x2 = 0;
            }
            break;
        }
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 2) % 3u;
        } else if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 4) % 3u;
        }
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->joy.rep & JOY_B) {
            pTool->sub = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        }
        break;
    }
    env->wind.set();
    pPL->moveCloth();
    a = pG->Cam.param.pos;
    b = pG->Cam.param.at;
    PSVECSubtract(&b, &a, &b);
#line 4082 "D:/Bio4/Prog/db_light.cpp"
    VECNormalize(&b, &b);
    PSVECScale(&b, &b, 1000.0f);
    PSVECAdd(&a, &b, &b);
    a.x = b.x;
    a.y = b.y - 100.0f;
    a.z = b.z;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    c.x = 0.0f;
    c.z = 300.0f;
    c.y = 0.0f;
    rot.x = 0.0f;
    rot.y = (f32) env->wind.dir * 3.14159265f / 127.0f;
    rot.z = 0.0f;
    RotVector(&c, &rot, &c);
    PSVECAdd(&c, &a, &c);
    Draw_line3d(&a, &c, 0xFFFFFFFF, 0);
    Draw_line3d(&b, &c, 0xFFFFFFFF, 0);
    eprintf(0x20, 0x2A, 4, pTool->color, "WIND (CLOTH)");
    eprintf(0x20, 0x38, 0, pTool->color, "DIRECTION %2.2f", (f32) env->wind.dir * 3.14159265f / 127.0f);
    eprintf(0x20, 0x46, 0, pTool->color, "POWER     %3.2f", (f32) env->wind.power * 0.01f * 20.0f);
    eprintf(0x20, 0x54, 0, pTool->color, "FREQUENCY %3.2f", (f32) env->wind.x2 * 0.01f * 1.0471976f);
}
// Light path table editor: select a path, then edit it.
static void path()
{
    eprintf(0x20, 0x2A, 4, pTool->color, "PATH EDIT");
    switch (pTool->editNo) {
    case 0:
        pTool->x10 = pTool->x11 = pTool->x12 = pTool->x13 = 0;
        pTool->editNo = 1;
    case 1:
        pTool->xB = pathSelect(0x20, 0x70, 0, 0, 0);
        if (pTool->joy.rep & JOY_A) {
            pTool->x10 = pTool->x11 = pTool->x12 = pTool->x13 = 0;
            pTool->editNo = 2;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->routine = pTool->editNo = 0;
            pTool->clearWork();
        }
        break;
    case 2:
        eprintf(0x20, 0x38, 0, pTool->color, "PATH %d", pTool->xB);
        pTool->xA = pathEdit(0x20, 0x70, pTool->xB, 0, 0);
        if (pTool->joy.rep & JOY_B) {
            pTool->x10 = pTool->x12 = pTool->x13 = 0;
            pTool->x11 = pTool->xB;
            pTool->editNo = 1;
        }
        break;
    }
}
// File number selector shared by the load / save pages: UP / DOWN step 0x10, LEFT / RIGHT step 1.
#define FILE_NO_SELECT()                     \
    if (pTool->joy.rep & JOY_UP) {           \
        pTool->cursor += 0x10;               \
    }                                        \
    if (pTool->joy.rep & JOY_DOWN) {         \
        pTool->cursor += 0xF0;               \
    }                                        \
    if (pTool->joy.rep & JOY_RIGHT) {        \
        pTool->cursor++;                     \
    }                                        \
    if (pTool->joy.rep & JOY_LEFT) {         \
        pTool->cursor += 0xFF;               \
    }                                        \
    pTool->cursor %= 0x100

// The event tool's names the load / save pages use (EventDebug+0x20 / +0x48, inside its pad_0).
struct EvtDebugNames {
    u8 pad_0[0x20];
    char name[0x28];  // 0x20
    char str[0x18];   // 0x48
};

// Load page: source select (room local / server, event, tool, core, item), then the file number.
// Odd editNo values load the file the even one selected.
static void load()
{
    static u32 evtKey;
    static int evtAction;
    static u32 evName[8];
    static char evStr[8];
    char path[0x100];
    void* evt;
    EvtDebugNames* ev;
    u32* key;

    eprintf(0x20, 0x2A, 4, pTool->color, "LOAD");
    switch (pTool->editNo) {
    case 0:
        pTool->clearWork();
        pTool->editNo = 1;
        pTool->cursor = (pG->flags_60 & 0x02000000) ? 2 : 1;
    case 1:
        eprintf(0x20, 0x38, 0, pTool->color, "ROOM LOCAL");
        eprintf(0x20, 0x46, 0, pTool->color, "ROOM SERVER");
        eprintf(0x20, 0x54, 0, pTool->color, "EVENT");
        eprintf(0x20, 0x62, 0, pTool->color, "TOOL");
        eprintf(0x20, 0x70, 0, pTool->color, "CORE");
        eprintf(0x20, 0x7E, 0, pTool->color, "ITEM");
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 5) % 6;
        } else if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 7) % 6;
        }
        if (pTool->joy.rep & JOY_A) {
            switch (pTool->cursor) {
            case 0:
                pTool->editNo = 2;
                pTool->cursor = 0;
                break;
            case 1:
                pTool->editNo = 4;
                pTool->cursor = 0;
                break;
            case 2:
                pTool->editNo = 12;
                ev = (EvtDebugNames*) &EvtDebug;
                key = evName;
                strcpy((char*) key, ev->name);
                strcpy(evStr, ev->str);
                if (EvtMgr.GetEvt(key, &evt) == 1) {
                    evtKey = ((Event*) evt)->cut;
                    if (evStr[0] == 's' || evStr[0] == 'S') {
                        evtAction = 0;
                    } else {
                        evtAction = 1;
                    }
                }
                pTool->cursor = (u8) evtKey;
                break;
            case 3:
                pTool->editNo = 6;
                pTool->cursor = 0;
                break;
            case 4:
                pTool->editNo = 8;
                pTool->cursor = 0;
                break;
            case 5:
                pTool->editNo = 0x10;
                pTool->cursor = 0;
                break;
            }
        }
        if (pTool->joy.rep & JOY_B) {
            pTool->routine = pTool->editNo = 0;
            pTool->clearWork();
        }
        break;
    case 2:
        sprintf(path, path_room_server, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->color, "%s", path);
        FILE_NO_SELECT();
        if (Joy[0].on & JOY_Y) {
            eprintf(0x50, 0x70, 0x16, pTool->color, "OLD VERSION");
        }
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 3;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 3:
        sprintf(path, path_room_server, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        if (pTool->lit.fileLoad(path)) {
            pTool->cutNo = pTool->gameCutNo;
            LitLoadWork(&pTool->lit, pTool->cutNo);
            pTool->mode = (pTool->cursor != 1) ? 1 : 2;
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 4:
        sprintf(path, path_room_local, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->color, "%s", path);
        FILE_NO_SELECT();
        if (Joy[0].on & JOY_Y) {
            eprintf(0x50, 0x70, 0x16, pTool->color, "OLD VERSION");
        }
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 5;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 5:
        sprintf(path, path_room_local, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        if (pTool->lit.fileLoad(path)) {
            file_lock(path);
            pTool->cutNo = pTool->gameCutNo;
            LitLoadWork(&pTool->lit, pTool->cutNo);
            pTool->mode = (pTool->cursor != 1) ? 1 : 2;
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 6:
        sprintf(path, path_tool, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->color, "%s", path);
        FILE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 7;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 7:
        sprintf(path, path_tool, pTool->cursor);
        if (pTool->lit.fileLoad(path)) {
            pTool->cutNo = 0;
            LitLoadWork(&pTool->lit, pTool->cutNo);
            pTool->mode = 4;
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 8:
        sprintf(path, path_core, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->color, "%s", path);
        FILE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 9;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 9:
        sprintf(path, path_core, pTool->cursor);
        if (pTool->lit.fileLoad(path)) {
            file_lock(path);
            pTool->cutNo = 0;
            LitLoadWork(&pTool->lit, pTool->cutNo);
            pTool->mode = 3;
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 10:
        eprintf(0x20, 0x38, 0, pTool->color, "FILE OPEN ERROR");
        eprintf(0x20, 0x46, 0, pTool->color, "PUSH BUTTON TO CONTINUE");
        if (pTool->joy.rep & (JOY_A | JOY_B)) {
            pTool->editNo = 0;
        }
        break;
    case 12:
        if (evtAction == 0) {
            sprintf(path, path_event, pG->stage_no, pG->room_no, evStr, evStr, pTool->cursor);
        } else {
            sprintf(path, path_event_action, evStr, evStr, pTool->cursor);
        }
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%03d", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->color, "%s", path);
        FILE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 13;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 13:
        if (evtAction == 0) {
            sprintf(path, path_event, pG->stage_no, pG->room_no, evStr, evStr, pTool->cursor);
        } else {
            sprintf(path, path_event_action, evStr, evStr, pTool->cursor);
        }
        if (pTool->lit.fileLoad(path)) {
            file_lock(path);
            pTool->cutNo = pTool->gameCutNo = 0;
            LitLoadWork(&pTool->lit, pTool->cutNo);
            pTool->mode = (pTool->cursor != 1) ? 1 : 2;
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 0x10:
        sprintf(path, path_item, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->color, "%s", path);
        FILE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 0x11;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 0x11:
        sprintf(path, path_item, pTool->cursor);
        if (pTool->lit.fileLoad(path)) {
            pTool->cutNo = 0;
            LitLoadWork(&pTool->lit, pTool->cutNo);
            pTool->mode = 4;
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    }
}
// File number selector of the save pages: UP / RIGHT step 1, DOWN / LEFT step -1.
#define SAVE_NO_SELECT()                                 \
    if (pTool->joy.rep & (JOY_UP | JOY_RIGHT)) {         \
        pTool->cursor++;                                 \
    } else if (pTool->joy.rep & (JOY_DOWN | JOY_LEFT)) { \
        pTool->cursor += 0xFF;                           \
    }                                                    \
    pTool->cursor %= 0x100

// Save page: destination select, then the file number; odd editNo values write the file.
static void save()
{
    static u32 evtKey;
    static int evtAction;
    static u32 evName[8];
    static char evStr[8];
    char path[0x100];
    void* evt;
    EvtDebugNames* ev;
    u32* key;

    eprintf(0x20, 0x2A, 4, pTool->color, "SAVE");
    switch (pTool->editNo) {
    case 0:
        pTool->clearWork();
        switch (pTool->mode) {
        default:
            pTool->cursor = (pG->flags_60 & 0x02000000) ? 2 : 1;
            break;
        case 4:
            pTool->cursor = 2;
            break;
        case 3:
            pTool->cursor = 3;
            break;
        }
        pTool->editNo = 1;
    case 1:
        eprintf(0x20, 0x38, 0, pTool->color, "ROOM LOCAL");
        eprintf(0x20, 0x46, 0, pTool->color, "ROOM SERVER");
        eprintf(0x20, 0x54, 0, pTool->color, "EVENT");
        eprintf(0x20, 0x62, 0, pTool->color, "TOOL");
        eprintf(0x20, 0x70, 0, pTool->color, "CORE");
        eprintf(0x20, 0x7E, 0, pTool->color, "PATH");
        eprintf(0x20, 0x8C, 0, pTool->color, "ITEM");
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->joy.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 6) % 7;
        } else if (pTool->joy.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 8) % 7;
        }
        if (pTool->joy.rep & JOY_A) {
            switch (pTool->cursor) {
            case 0:
                pTool->editNo = 2;
                pTool->cursor = pTool->mode == 2;
                break;
            case 1:
                pTool->editNo = 4;
                pTool->cursor = pTool->mode == 2;
                break;
            case 2:
                pTool->editNo = 14;
                ev = (EvtDebugNames*) &EvtDebug;
                key = evName;
                strcpy((char*) key, ev->name);
                strcpy(evStr, ev->str);
                if (EvtMgr.GetEvt(key, &evt) == 1) {
                    evtKey = ((Event*) evt)->cut;
                    if (evStr[0] == 's' || evStr[0] == 'S') {
                        evtAction = 0;
                    } else {
                        evtAction = 1;
                    }
                }
                pTool->cursor = (u8) evtKey;
                break;
            case 3:
                pTool->editNo = 6;
                pTool->cursor = pTool->mode == 2;
                break;
            case 4:
                pTool->editNo = 8;
                pTool->cursor = pTool->mode == 2;
                break;
            case 5:
                pTool->editNo = 12;
                pTool->cursor = pTool->mode == 2;
                break;
            case 6:
                pTool->editNo = 0x10;
                pTool->cursor = pTool->mode == 2;
                break;
            }
        }
        if (pTool->joy.rep & JOY_B) {
            pTool->routine = pTool->editNo = 0;
            pTool->clearWork();
        }
        break;
    case 2:
        sprintf(path, path_room_server, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->color, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 3;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 3:
        sprintf(path, path_room_server, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->lit, pTool->cutNo);
        }
        if (pTool->lit.fileSave(path)) {
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 4:
        sprintf(path, path_room_local, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->color, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 5;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 5:
        sprintf(path, path_room_local, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->lit, pTool->cutNo);
        }
        if (pTool->lit.fileSave(path)) {
            file_unlock(path);
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 6:
        sprintf(path, path_tool, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->color, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 7;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 7:
        sprintf(path, path_tool, pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->lit, pTool->cutNo);
        }
        if (pTool->lit.fileSave(path)) {
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 8:
        sprintf(path, path_core, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->color, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 9;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 9:
        sprintf(path, path_core, pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->lit, pTool->cutNo);
        }
        if (pTool->lit.fileSave(path)) {
            file_unlock(path);
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 10:
        eprintf(0x20, 0x38, 0, pTool->color, "FILE OPEN ERROR");
        eprintf(0x20, 0x46, 0, pTool->color, "PUSH BUTTON TO CONTINUE");
        if (pTool->joy.rep & (JOY_A | JOY_B)) {
            pTool->editNo = 0;
        }
        break;
    case 12:
        sprintf(path, path_litpath);
        eprintf(0x20, 0x54, 0x16, pTool->color, "%s", path);
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 13;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 13:
        if (HDWrite(path_litpath, pLitPath, pLitPath->getSize())) {
            file_unlock(path);
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 14:
        if (evtAction == 0) {
            sprintf(path, path_event, pG->stage_no, pG->room_no, evStr, evStr, pTool->cursor);
        } else {
            sprintf(path, path_event_action, evStr, evStr, pTool->cursor);
        }
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%03d", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->color, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 15;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 15:
        if (evtAction == 0) {
            sprintf(path, path_event, pG->stage_no, pG->room_no, evStr, evStr, pTool->cursor);
        } else {
            sprintf(path, path_event_action, evStr, evStr, pTool->cursor);
        }
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->lit, pTool->cutNo);
        }
        pTool->lit.preEventSave();
        if (pTool->lit.fileSave(path)) {
            file_unlock(path);
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    case 0x10:
        sprintf(path, path_item, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->color, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->color, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->joy.rep & JOY_A) {
            pTool->editNo = 0x11;
        } else if (pTool->joy.rep & JOY_B) {
            pTool->clearWork();
            pTool->editNo = 1;
        }
        break;
    case 0x11:
        sprintf(path, path_item, pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->lit, pTool->cutNo);
        }
        if (pTool->lit.fileSave(path)) {
            pTool->routine = 0;
            pTool->editNo = 0;
            pTool->clearWork();
        } else {
            pTool->editNo = 10;
        }
        break;
    }
}
// Tool options: object move, cut select, elec power / path, analyze, kind on/off, player light mask,
// bounding box display.
static void option()
{
    static const char* onoff[] = {"OFF", "ON"};
    f32 step = (pTool->joy.on & JOY_A) ? 0.3f : 0.1f;
    cLightPathData* p;
    cLightPathData* q;
    u32 i;
    int c;

    eprintf(0x20, 0x2A, 4, pTool->color, "OPTION");
    switch (pTool->editNo) {
    case 0:
        switch (pTool->sub) {
        case 0:
            if (pTool->joy.rep & JOY_A) {
                pTool->flags ^= 1;
            }
            break;
        case 1:
            if (pTool->joy.rep & JOY_A) {
                pTool->flags ^= 8;
            }
            break;
        case 2:
            LightMgr.setElecPower(step * 0.01f * (f32) pTool->joy.sx);
            if (pTool->joy.rep & JOY_RIGHT) {
                LightMgr.setElecPower(step);
            }
            if (pTool->joy.rep & JOY_LEFT) {
                LightMgr.setElecPower(-step);
            }
            if (pTool->joy.trg & JOY_Y) {
                step = (LightMgr.elecPower == 1.0f) ? -1.0f : 1.0f;
                LightMgr.setElecPower(step);
            }
            break;
        case 3:
            if (pTool->joy.rep & JOY_RIGHT) {
                pTool->init++;
            }
            if (pTool->joy.rep & JOY_LEFT) {
                pTool->init--;
            }
            if (pTool->joy.rep & JOY_A) {
                LightMgr.setElecPower2(pTool->init, 1);
            }
            if (pTool->joy.trg & JOY_Y) {
                p = pTool->litPath.path[pTool->init];
                if (p != NULL) {
                    memcpy(pTool->litPath.edit, p, p->getSize());
                    pTool->x10 = pTool->x11 = pTool->x12 = pTool->x13 = 0;
                    pTool->editNo = 1;
                }
            }
            q = LightMgr.getPathPtr(pTool->init);
            if (PTR_OK(q)) {
                drawPath(0x32, 0xFA, q, 0, 0xFFFFFFFF);
            } else {
                eprintf(0x32, 0xFA, 0, pTool->color, "NO DATA");
            }
            break;
        case 4:
            if (pTool->joy.rep & JOY_A) {
                pTool->flags ^= 4;
            }
            break;
        case 5:
            if (pTool->joy.rep & JOY_A) {
                pTool->editNo = 2;
                pTool->sub = 0;
            }
            break;
        case 6:
            if (pTool->joy.rep & JOY_A) {
                switch (pPL->lightInfo.x50) {
                case 1:
                    pPL->lightInfo.x50 = 2;
                    break;
                case 2:
                    pPL->lightInfo.x50 = 4;
                    break;
                case 4:
                    pPL->lightInfo.x50 = 8;
                    break;
                case 8:
                    pPL->lightInfo.x50 = 0x10;
                    break;
                case 0x10:
                    pPL->lightInfo.x50 = 0x40;
                    break;
                case 0x40:
                    pPL->lightInfo.x50 = 1;
                    break;
                }
            }
            break;
        case 7:
            if (pTool->joy.rep & JOY_A) {
                pTool->flags ^= 0x10;
            }
            break;
        }
        if (pTool->joy.rep & JOY_UP) {
            pTool->sub = (pTool->sub + 7) % 8;
        }
        if (pTool->joy.rep & JOY_DOWN) {
            pTool->sub = (pTool->sub + 9) % 8;
        }
        eprintf(0x20, 0x38, 0, pTool->color, "OBJ MOVE      %s", onoff[(pTool->flags & 1) ? 1 : 0]);
        eprintf(0x20, 0x46, 0, pTool->color, "CUT SELECT    %s", onoff[(pTool->flags & 8) ? 1 : 0]);
        eprintf(0x20, 0x54, 0, pTool->color, "ELEC POWER    %1.2f", LightMgr.elecPower);
        eprintf(0x20, 0x62, 0, pTool->color, "ELEC PATH     %d", pTool->init);
        eprintf(0x20, 0x70, 0, pTool->color, "ANALYZE       %s", onoff[(pTool->flags & 4) >> 2]);
        eprintf(0x20, 0x7E, 0, pTool->color, "KIND ON/OFF");
        switch (pPL->lightInfo.x50) {
        case 1:
            eprintf(0x20, 0x8C, 0, pTool->color, "PL EMASK      PLAYER");
            break;
        case 2:
            eprintf(0x20, 0x8C, 0, pTool->color, "PL EMASK      ENEMY");
            break;
        case 4:
            eprintf(0x20, 0x8C, 0, pTool->color, "PL EMASK      OBJ");
            break;
        case 8:
            eprintf(0x20, 0x8C, 0, pTool->color, "PL EMASK      EFFECT");
            break;
        case 0x10:
            eprintf(0x20, 0x8C, 0, pTool->color, "PL EMASK      SCROLL");
            break;
        case 0x40:
            eprintf(0x20, 0x8C, 0, pTool->color, "PL EMASK      SUBCHAR");
            break;
        }
        eprintf(0x20, 0x9A, 0, pTool->color, "BB DISP       %s", onoff[(pTool->flags & 0x10) ? 1 : 0]);
        pTool->printCursor(3, pTool->sub + 4);
        if (pTool->joy.rep & JOY_B) {
            pTool->routine = 0;
        }
        break;
    case 1:
        pathEdit(0x32, 0xFA, pTool->init, 0, 0);
        if (pTool->joy.rep & JOY_B) {
            pTool->editNo = 0;
        }
        break;
    case 2:
        eprintf(0x40, 0x8C, 4, pTool->color, "KIND");
        for (i = 0; i < 32; i++) {
            eprintf(0x40 + i * 8, 0x9A, LightMgr.checkKind(i) ? 0 : 0x14, pTool->color, "%d", i % 10);
        }
        if (pTool->joy.rep & JOY_RIGHT) {
            pTool->sub++;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            pTool->sub--;
        }
        if (pTool->joy.rep & JOY_A) {
            if (LightMgr.checkKind(pTool->sub)) {
                LightMgr.offKind(pTool->sub);
            } else {
                LightMgr.onKind(pTool->sub);
            }
        }
        if (pTool->joy.rep & JOY_B) {
            pTool->editNo = 0;
        }
        c = (pG->flags_51E4 % 30 > 14) ? 0x14 : 0;
        eprintf((pTool->sub + 8) << 3, 0xA8, c, pTool->color, "^");
        break;
    }
}
// Quit confirmation: YES leaves the tool (restoring the debug page colour), NO goes back to the menu.
static void quit()
{
    eprintf(0x20, 0x2A, 4, pTool->color, "QUIT ?");
    if (pTool->editNo == 0) {
        pTool->cursor = 1;
        pTool->editNo = 1;
    }
    eprintf(0x30, 0x46, pTool->cursor == 0 ? 0 : 0x14, pTool->color, "YES");
    eprintf(0x30, 0x54, pTool->cursor == 1 ? 0 : 0x14, pTool->color, "NO");
    if (pTool->joy.rep & JOY_UP) {
        pTool->cursor = 0;
    } else if (pTool->joy.rep & JOY_DOWN) {
        pTool->cursor = 1;
    }
    if (pTool->joy.rep & JOY_A) {
        if (pTool->cursor == 0) {
            pTool->ret = 0;
            pGS->debug_mode = pTool->colorBak;
            pLog->modeReset();
            pTool->updateLit();
            pTool->clearWork();
            pTool->routine = pTool->editNo = pTool->sub = pTool->init = pTool->x8 = pTool->x9 = pTool->xA = pTool->xB = 0;
            pTool->xC = pTool->xD = pTool->xE = pTool->xF = 0;
        } else {
            pTool->clearWork();
            pTool->routine = 0;
        }
    }
    if (pTool->joy.rep & JOY_B) {
        pTool->clearWork();
        pTool->routine = 0;
    }
}
// The light table header: defined before the inline row printer so that its strings precede the
// row printer's strings in .rodata (an inline's string literals are emitted at parse time).
static const char* table_head[] = {
    "NO ID EMASK PA POSITION===== RAD COL INT TYPE KIND ATTR PR",
    "NO =======================================================",
};

// One row of the light table (first page of columns), written as a MACRO over printEditTable's own
// `x`, `y` and `c`: the target keeps ONE `x` pseudo (r30) for the outer `x = 4` and the row's columns,
// so the row is not an inline with its own locals. The E.. columns use the loop's `y` variable while
// the "%02d"/"P" columns read `0x150 + i * 14` (see printEditTable).
#define PRINT_EDIT_TAIL(l, Y, c)                                                                    \
    {                                                                                               \
        GXColor col;                                                                                \
                                                                                                    \
        eprintf(x * 8, (Y), c, pTool->color, "%s", (l->xF & 2) ? "E" : "-");                        \
        x++;                                                                                        \
        eprintf(x * 8, (Y), c, pTool->color, "%s", (l->xF & 4) ? "O" : "-");                        \
        x++;                                                                                        \
        eprintf(x * 8, (Y), c, pTool->color, "%s", (l->xF & 8) ? "E" : "-");                        \
        x++;                                                                                        \
        eprintf(x * 8, (Y), c, pTool->color, "%s", (l->xF & 0x10) ? "S" : "-");                     \
        x += 2;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->color, "%s", parent_short[l->parentType]);                    \
        x += 3;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->color, "%4.0f %3.0f %4.0f", l->pos.x / 1000.0f,               \
                l->pos.y / 1000.0f, l->pos.z / 1000.0f);                                            \
        x += 14;                                                                                    \
        if (l->x1C != 0.0f) {                                                                       \
            eprintf(x * 8, (Y), c, pTool->color, "%3d", (int) (l->x1C / 1000.0f));                  \
        } else {                                                                                    \
            eprintf(x * 8, (Y), c, pTool->color, "INF");                                            \
        }                                                                                           \
        x += 4;                                                                                     \
        if (l->type == 4) {                                                                         \
            /* a four-member chain: `a` gets its own load, the re-evaluated rhs of the innermost   \
               assignment is the one load shared by b, g, r (a three-member chain reloads) */       \
            col.r = col.g = col.b = col.a = l->color.r;                                             \
        } else {                                                                                    \
            col = l->color;                                                                         \
        }                                                                                           \
        /* COMPILER-DIFF: candidate #18 (by-value aggregate view): the swatch's 4-byte temp at frame \
           offset 0 with `col` in the next slot (the editColor form) */                             \
        DrawTileV(x * 8 + 1, (Y) + 1, 0x16, 0xC, col);                                              \
        x += 4;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->color, "%1.1f", l->power);                                    \
        x += 4;                                                                                     \
        if (l->type != 4) {                                                                         \
            if (l->xD <= 7) {                                                                       \
                eprintf(x * 8, (Y), c, pTool->color, "%s", light_type_short[l->xD]);               \
            } else {                                                                                \
                eprintf(x * 8, (Y), c, pTool->color, "ERR!");                                       \
            }                                                                                       \
        } else {                                                                                    \
            if (l->xD <= 2) {                                                                       \
                eprintf(x * 8, (Y), c, pTool->color, "%s", shadow_type_short[l->xD]);              \
            } else {                                                                                \
                eprintf(x * 8, (Y), c, pTool->color, "ERR!");                                       \
            }                                                                                       \
        }                                                                                           \
        x += 5;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->color, "%s %02x", (l->kind & 0x80) ? "E" : " ", l->kind);     \
        x += 5;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->color, "%02X", l->attr);                                      \
        x += 5;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->color, "%d", l->x2B);                                         \
    }

// The light table: one row per light of the current cut.
void printEditTable()
{
    int page = pTool->col > 11;
    int i;
    int y;
    int no;
    int x;
    u8 c;
    cLight* l;

    eprintf(0x20, 0x142, 4, pTool->color, table_head[page]);
    for (i = 0, no = pTool->top; i < pTool->rows; i++, no++) {
        l = LightMgr.getWorkPtr(no);
        x = 4;
        if (pTool->editEnable()) {
            if ((l->be_flag & 3) == 3) {
                c = (l->type == 4) ? 5 : 0;
            } else {
                c = 0x14;
            }
        } else {
            c = 0x16;
        }
        eprintf(x * 8, 0x150 + i * 14, c, pTool->color, "%02d", no);
        // The row's `y`: a SECOND giv of the same value, spelled so that cse cannot fold it into the
        // `0x150 + i * 14` pseudo above (loop.c combines the two givs: `mr r26,r23` for the "P" column's
        // expression, `mr r29,r23` for this variable).
        y = (i + 24) * 14;
        if (l->be_flag & 1) {
            if (page == 0) {
                eprintf(7 * 8, 0x150 + i * 14, c, pTool->color, "%02d", l->type);
                // COMPILER-DIFF: candidate #12 (gcse cprop): the target keeps `li r30,10` and `li r3,80`
                // as pseudos of the pre-diamond block (`x + 1` and the "P" call's r3 not folded); ours
                // const-propagates both into the "P" join block unless the two constants are laundered
                // after cse folded them (the launders are the last sets of the block: nothing to propagate,
                // and `t80` is not a single-set constant).
                x = 10;
                {
                    int t80 = x * 8;
                    asm("" : "+r"(t80));
                    asm("" : "+r"(x));
                    eprintf(t80, 0x150 + i * 14, c, pTool->color, "%s", (l->xF & 1) ? "P" : "-");
                }
                x++;
                PRINT_EDIT_TAIL(l, y, c);
            }
        } else {
            eprintf(0x38, 0x150 + i * 14, 0x14, pTool->color, "EMPTY WORK");
        }
    }
}

void cLightTool::printCursor(int x, int y)
{
    if (!(blink & 8)) {
        eprintf(x * 8, y * 14, 0, pTool->color, ">");
    }
}

// White ambient for DrawTile: a struct returned by value (its temporary is the last frame slot).
static inline GXColor whiteCol()
{
    GXColor c;
    c.r = c.g = c.b = c.a = 0xFF;
    return c;
}

void DrawTile(int x, int y, int w, int h, GXColor* color)
{
    GXColor col = *color;

    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    GXSetChanAmbColor(0, whiteCol());
    GXSetChanMatColor(0, col);
    Mtx44 proj;
    Mtx mtx;
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(0, 1, 0, 0);
    GXSetCullMode(0);
    GXSetZMode(1, 3, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3s16(x, y, 2);
    GXPosition3s16(x + w, y, 2);
    GXPosition3s16(x + w, y + h, 2);
    GXPosition3s16(x, y + h, 2);
}

void cLightTool::clearWork()
{
    cursor = 0;
    row = 0;
    col = 0;
}

void cLightTool::clearSubMenu()
{
    subCursor = xC = xD = xE = xF = 0;
}

#ifdef DB_LIGHT_SET_LOG_MODE
// t_esp build: error logging on / off (the log switch of the tool and of the manager).
void cLightTool::setLogMode(int on)
{
    if (on == 1) {
        pTool->flags |= 0x20;
    } else {
        pTool->flags &= ~0x20;
    }
    LightMgr.logOn = on;
}
#endif

cDbLit::cDbLit()
{
    u32 i;

    nCut = 0;
    version = 0;
    nMaxLight = 0;
    for (i = 0; i < 256; i++) {
        cut[i] = NULL;
    }
}

u32 cDbLit::size()
{
    u32 i;
    u32 size;

    for (i = 0; i < 256; i++) {
        if (cut[i]) {
            nCut = i + 1;
        }
    }
    size = nCut * 4 + 4;
    for (i = 0; i < nCut; i++) {
        if (cut[i]) {
            size += cut[i]->getSize();
        }
    }
    return size;
}

cLightEnv* cDbLit::getCut(u16 no)
{
    return cut[no];
}

int LitLoadWork(cDbLit* lit, int no)
{
    cLightEnv* env = lit->getCut(no);

    LightMgr.destroyAll();
    if (env != NULL) {
        LightMgr.setEnv(env, -1);
        LightMgr.loadLit(env->getLightWork(0), env->nLight);
        return 1;
    }
    memclr_asm(pLightEnv, sizeof(cLightEnv));
    return 0;
}

int LitSaveWork(cDbLit* lit, int no)
{
    u32 n;

    if (lit->isCut(no)) {
        Debug_free(lit->getCut(no));
    }
    n = LightMgr.countScr();
    pLightEnv->nLight = n;
    lit->cut[(u16) no] = (cLightEnv*) Debug_alloc(n * sizeof(cLightWork) + sizeof(cLightEnv), 1);
    *lit->getCut(no) = *pLightEnv;
    LightMgr.saveLit(lit->getCut(no)->getLightWork(0));
    return 1;
}

int cDbLit::fileLoad(const char* path)
{
    cLit* buf;
    int ret;

    if (HDReadDebugAlloc(path, (void**) &buf, 1) == 0) {
        TOOL_ERR("cDbLit::fileLoad() FILE NOT FOUND [%s]", path);
        return 0;
    }
    buf->versionUp();
    ret = init(buf);
    Debug_free(buf);
    return ret;
}

int cDbLit::init(cLit* lit)
{
    u32 i;
    u32* tbl;

    if (!PTR_OK(lit)) {
        TOOL_ERR("cDbLit::init() MEMORY ERROR");
        return 0;
    }
    for (i = 0; i < 256; i++) {
        if (cut[i]) {
            Debug_free(cut[i]);
        }
        cut[i] = NULL;
    }
    *(u32*) this = *(u32*) lit;
    tbl = (u32*) (lit + 1);
    for (i = 0; i < nCut; i++) {
        u32 ofs = tbl[i];
        if (ofs) {
            cLightEnv* src = (cLightEnv*) ((u8*) lit + ofs);
            u32 size = src->nLight * sizeof(cLightWork) + sizeof(cLightEnv);
            cLightEnv* dst = (cLightEnv*) Debug_alloc(size, 1);
            memcpy(dst, src, size);
            cut[i] = dst;
        } else {
            cut[i] = (cLightEnv*) ofs;
        }
    }
    version = 0x2C;
    return 1;
}

void cDbLit::preEventSave()
{
    u32 i;

    for (i = 1; i < 256; i++) {
        if (cut[i]) {
            Debug_free(cut[i]);
            cut[i] = NULL;
        }
    }
}

int cDbLit::fileSave(const char* path)
{
    u32 size;
    cLit* buf;
    int ret = 0;

    size = this->size();
    if (size == 0) {
        return 0;
    }
    buf = (cLit*) Debug_alloc(size, 1);
    if (buf == NULL) {
        return 0;
    }
    size = createLit(buf);
    if (size) {
        if (HDWrite(path, buf, size)) {
            ret = 1;
        }
    }
    Debug_free(buf);
    return ret;
}

u32 cDbLit::createLit(cLit* dst)
{
    u32 i;
    u32 ofs;
    u32 size;
    u32* tbl;
    u32 n;
    cLightEnv* c;

    if (!PTR_OK(dst)) {
        TOOL_ERR("cDbLit::createLit() POINTER ERR %08X", dst);
        return 0;
    }
    version = 0x2C;
    nMaxLight = nCut = 0;
    for (i = 0; i < 256; i++) {
        if (cut[i]) {
            nCut = i + 1;
            if (cut[i]->nLight > nMaxLight) {
                nMaxLight = cut[i]->nLight;
            }
        }
    }
    *(u32*) dst = *(u32*) this;
    tbl = (u32*) (dst + 1);
    ofs = nCut * 4 + 4;
    for (i = 0; i < nCut; i++) {
        c = cut[i];
        if (c) {
            tbl[i] = ofs;
            ofs += sizeof(cLightEnv) + c->nLight * sizeof(cLightWork);
        } else {
            tbl[i] = (u32) c;
        }
    }
    dst = (cLit*) &tbl[nCut];
    size = nCut * 4 + 4;
    // the same `c` as the table loop: the memcpy argument ties it to r4 in both loops
    for (i = 0; i < nCut; i++) {
        c = cut[i];
        if (c) {
            n = c->nLight * sizeof(cLightWork) + sizeof(cLightEnv);
            memcpy(dst, c, n);
            dst = (cLit*) ((u8*) dst + n);
            size += n;
        }
    }
    return size;
}

// RGBA editor at text cell (x, y): the colour is edited as floats (kept across calls) and written back
// every frame; Y held links R / G / B. Returns 0 when B leaves the editor.
int editColor(int x, int y, GXColor* col)
{
    static int state = 0;
    static f32 r;
    static f32 g;
    static f32 b;
    static f32 a;
    int ret = 1;
    f32 step;
    int link;
    GXColor c;

    switch (state) {
    case 0:
        r = col->r;
        g = col->g;
        b = col->b;
        a = col->a;
        state = 1;
    case 1:
        col->r = r;
        col->g = g;
        col->b = b;
        col->a = a;
        break;
    }
    step = (pTool->joy.on & JOY_A) ? 1.5f : 0.1f;
    link = pTool->joy.on & JOY_Y;
    pTool->printCursor(x - 1, y + pTool->cursor);
    eprintf(x << 3, y * 14, 0, pTool->color, "R %3d", col->r);
    // the original copies the deferred .rodata template into a register once (`lwz r24`) and stores
    // that word into the swatch argument's copy temp before each DrawTile (a `{0,0,0,0}` initializer
    // folds to `li 0`); the by-value DrawTileV view gives the one reused 4-byte temp at frame offset 0
    // (its address is a fresh `addi r7,r1,8` per call) with `c` purged into the slot after it.
    GXColor black = blackTemplate;
    c.r = 0xFF;
    c.g = 0;
    c.b = 0;
    DrawTileV(((x + 6) << 3), y * 14 + 4, 0x80, 6, black);
    DrawTileV(((x + 6) << 3), y * 14 + 4, col->r >> 1, 6, c);
    y++;
    eprintf(x << 3, y * 14, 0, pTool->color, "G %3d", col->g);
    c.r = 0;
    c.g = 0xFF;
    c.b = 0;
    DrawTileV(((x + 6) << 3), y * 14 + 4, 0x80, 6, black);
    DrawTileV(((x + 6) << 3), y * 14 + 4, col->g >> 1, 6, c);
    y++;
    eprintf(x << 3, y * 14, 0, pTool->color, "B %3d", col->b);
    c.r = 0;
    c.g = 0;
    c.b = 0xFF;
    DrawTileV(((x + 6) << 3), y * 14 + 4, 0x80, 6, black);
    DrawTileV(((x + 6) << 3), y * 14 + 4, col->b >> 1, 6, c);
    y++;
    eprintf(x << 3, y * 14, 0, pTool->color, "A %1.1f", (f32) col->a * 0.0078125f);
    c.r = 200;
    c.g = 200;
    c.b = 200;
    DrawTileV(((x + 6) << 3), y * 14 + 4, 0x80, 6, black);
    DrawTileV(((x + 6) << 3), y * 14 + 4, col->a >> 1, 6, c);
    y += 2;
    DrawTileV(((x + 8) << 3), y * 14, 0x2A, 0x2A, *col);
    if (link) {
        eprintf(x << 3, y * 14, 0, pTool->color, "LINK");
    }
    y++;
    if (pTool->joy.on & JOY_A) {
        eprintf(x << 3, y * 14, 0, pTool->color, "TURBO");
    }
    if (link) {
        if (pTool->joy.rep & JOY_RIGHT) {
            FSet(r, r + step * 10.0f);
            FSet(g, g + step * 10.0f);
            FSet(b, b + step * 10.0f);
        }
        if (pTool->joy.rep & JOY_LEFT) {
            r -= step * 10.0f;
            g -= step * 10.0f;
            b -= step * 10.0f;
        }
        r += (f32) pTool->joy.sx * step / 20.0f;
        if (r < 0.0f) {
            r = 0.0f;
        } else if (r > 255.0f) {
            r = 255.0f;
        }
        g += (f32) pTool->joy.sx * step / 20.0f;
        if (g < 0.0f) {
            g = 0.0f;
        } else if (g > 255.0f) {
            g = 255.0f;
        }
        b += (f32) pTool->joy.sx * step / 20.0f;
        if (b < 0.0f) {
            b = 0.0f;
        } else if (b > 255.0f) {
            b = 255.0f;
        }
    } else {
        switch (pTool->cursor) {
        case 0:
            if (pTool->joy.rep & JOY_RIGHT) {
                FSet(r, r + step * 10.0f);
            }
            if (pTool->joy.rep & JOY_LEFT) {
                r -= step * 10.0f;
            }
            r += (f32) pTool->joy.sx * step / 20.0f;
            if (r < 0.0f) {
                r = 0.0f;
            } else if (r > 255.0f) {
                r = 255.0f;
            }
            break;
        case 1:
            if (pTool->joy.rep & JOY_RIGHT) {
                FSet(g, g + step * 10.0f);
            }
            if (pTool->joy.rep & JOY_LEFT) {
                g -= step * 10.0f;
            }
            g += (f32) pTool->joy.sx * step / 20.0f;
            if (g < 0.0f) {
                g = 0.0f;
            } else if (g > 255.0f) {
                g = 255.0f;
            }
            break;
        case 2:
            if (pTool->joy.rep & JOY_RIGHT) {
                FSet(b, b + step * 10.0f);
            }
            if (pTool->joy.rep & JOY_LEFT) {
                b -= step * 10.0f;
            }
            b += (f32) pTool->joy.sx * step / 20.0f;
            if (b < 0.0f) {
                b = 0.0f;
            } else if (b > 255.0f) {
                b = 255.0f;
            }
            break;
        case 3:
            if (pTool->joy.rep & JOY_RIGHT) {
                FSet(a, a + step * 10.0f);
            }
            if (pTool->joy.rep & JOY_LEFT) {
                a -= step * 10.0f;
            }
            a += (f32) pTool->joy.sx * step / 20.0f;
            if (a < 0.0f) {
                a = 0.0f;
            } else if (a > 255.0f) {
                a = 255.0f;
            }
            break;
        }
    }
    if (pTool->cursor == 3 && (pTool->joy.trg & JOY_Y)) {
        a = 128.0f;
    }
    if (pTool->joy.rep & JOY_UP) {
        pTool->cursor = (pTool->cursor + 3) % 4;
    } else if (pTool->joy.rep & JOY_DOWN) {
        pTool->cursor = (pTool->cursor + 5) % 4;
    }
    if (!(pTool->joy.on & 0x30000)) {
        if (pTool->joy.trg & 0x80000) {
            pTool->cursor = (pTool->cursor + 3) % 4;
        }
        if (pTool->joy.trg & 0x40000) {
            pTool->cursor = (pTool->cursor + 5) % 4;
        }
    }
    if (pTool->joy.rep & JOY_B) {
        state = 0;
        ret = 0;
    }
    return ret;
}

const char* strFogType(int type)
{
    const char* s;

    switch (type) {
    case 0:
        s = "NONE";
        break;
    case 2:
        s = "LINEAR";
        break;
    case 4:
        s = "EXP";
        break;
    case 5:
        s = "EXP2";
        break;
    case 6:
        s = "REV EXP";
        break;
    case 7:
        s = "REV EXP2";
        break;
    default:
        s = "????";
        break;
    }
    return s;
}

int fogTypeNext(int type)
{
    switch (type) {
    case 0:
        return 2;
    case 2:
        return 4;
    case 4:
        return 5;
    case 5:
        return 6;
    case 6:
        return 7;
    case 7:
        return 0;
    }
    return 0;
}

int fogTypeBack(int type)
{
    switch (type) {
    case 0:
        return 7;
    case 2:
        return 0;
    case 4:
        return 2;
    case 5:
        return 4;
    case 6:
        return 5;
    case 7:
        return 6;
    }
    return 0;
}

void initLightWork(cLight* l)
{
    l->be_flag |= 6;
    l->type = 0;
    l->xD = 2;
    l->xF |= 0x57;
    l->color.r = 0x80;
    l->color.g = 0x80;
    l->color.b = 0x80;
    l->color.a = 0x80;
    l->pos.x = 0.0f;
    l->pos.y = 0.0f;
    l->pos.z = 0.0f;
    l->x1C = 3000.0f;
    l->power = 1.0f;
    l->setParent(0, 0);
    l->kind = 0;
    l->attr = 0;
    l->x2B = 3;
    memclr_asm(&l->spot, sizeof(LightSpot));
    memclr_asm(&l->sub, 0x40);
    memclr_asm(&l->path, sizeof(LightPath));
    l->x138 = l->pad_139[0] = l->pad_139[1] = l->pad_139[2] = 0;
    l->curColor.r = 0x80;
    l->curColor.g = 0x80;
    l->curColor.b = 0x80;
    l->curColor.a = 0x80;
}

void clear_move_free()
{
    cLight* cur = curLight();

    memclr_asm(&cur->sub, sizeof(LightSub));
    if (cur->type == 1) {
        cur->sub.color = cur->color;
    }
}

void clear_type_free()
{
    memclr_asm(&curLight()->spot, sizeof(LightSpot));
}

int getCutNo()
{
    int no;

    if (pG->flags_60 & 0x02000000) {
        return 0;
    }
    if ((pG->flags_60 & 0x80000000) && DebugMenuSelected == 7) {
        no = tcCurrentCameraNo();
    } else {
        int cam = CamCtrl.CurrentCameraNo();
        no = (*LightMgr.getLitPPtr())->getSafeCutNo(cam);
    }
    return (u8) no;
}

// Shadow light with a parallel (type 2) direction: draws its position and the shadow cone.
void drawLightInfo_SpotShadow(cLight* l, u32 color)
{
    Light04Work* w = (Light04Work*) l->work;
    Vec dir;
    Vec n;
    Vec rot;
    Vec pos;
    Vec axis = {0.0f, 1.0f, 0.0f};
    Mtx m;
    Mtx m2;
    f32 len;

    l->getPos(&pos);
    Draw_pos(&pos, 300);
    dir.x = 0.0f;
    dir.y = -1.0f;
    dir.z = 0.0f;
    PSVECNormalize(&dir, &dir);
    rot.x = (f32) (s16) (u16) w->rotX * 3.1415927f * 2.0f / 360.0f;
    rot.y = (f32) (s16) (u16) w->rotY * 3.1415927f * 2.0f / 360.0f;
    rot.z = 0.0f;
    PSMTXRotRad(m, 'x', rot.x);
    PSMTXRotAxisRad(m2, &axis, rot.y);
    PSMTXConcat(m2, m, m);
    PSMTXMultVecSR(m, &dir, &dir);
    l->getNormal(&dir, &n);
    {
        // COMPILER-DIFF: 5 (the original's interblock scheduler hoists Draw_corn2's `mr r3/r4` above
        // the `len == 0` branch; the laundered pointers put the copies there)
        Vec* pp = &pos;
        Vec* pn = &n;
        asm("" : "+r"(pp), "+r"(pn));
        len = l->x1C;
        if (len == 0.0f) {
            len = 2000.0f;
        }
        Draw_corn2(pp, pn, len, (f32) w->texNo, 0xFFFFFFFF);
    }
}
// Position sphere / hit radius / direction line of a light.
void drawLightInfo(cLight* l, u32 color)
{
    Vec pos;
    Vec t;
    Vec n;

    if ((*(u32*) &l->xC & 0x00FFFF00) == 0x00020400) {
        drawLightInfo_SpotShadow(l, color);
        return;
    }
    pos = l->curPos;
    if (l->x1C != 0.0f) {
        Draw_sphere(&pos, l->x1C, color, 1, 1);
    }
    if ((f32) (int) l->x30 != 0.0f) {
        Draw_sphere(&pos, (f32) l->x30, 0xFFFF0044, 1, 1);
    }
    Draw_pos(&pos, 300);
    if (l->xD == 3 || l->xD == 4 || l->xD == 6) {
        l->getNormal(&l->normal, &n);
        PSVECScale(&n, &t, 500.0f);
        PSVECAdd(&t, &pos, &t);
        Draw_line3d(&pos, &t, (l->color.a << 24) | (l->color.r << 16) | (l->color.g << 8) | l->color.b, 0);
    }
}
// Path table: pick a path (LEFT / RIGHT), create / delete with A through a YES / NO prompt.
int pathSelect(int x, int y, u8 no, u8 flag, int mode)
{
    cLightPathData* p;
    cLightPathData* np;

    eprintf(x + 0x88, y - 0xE, 0, pTool->color, "SELECT A PATH NO: %d", pTool->x11);
    switch (pTool->x10) {
    case 0:
        if (pTool->joy.rep & JOY_RIGHT) {
            pTool->x11++;
            memset_asm(pTool->litPath.edit, 0xFF, sizeof(pTool->litPath.edit));
            p = pTool->litPath.path[pTool->x11];
            if (p != NULL) {
                memcpy(pTool->litPath.edit, p, p->getSize());
            }
            pTool->col = 0;
        }
        if (pTool->joy.rep & JOY_LEFT) {
            pTool->x11--;
            memset_asm(pTool->litPath.edit, 0xFF, sizeof(pTool->litPath.edit));
            p = pTool->litPath.path[pTool->x11];
            if (p != NULL) {
                memcpy(pTool->litPath.edit, p, p->getSize());
            }
            pTool->col = 0;
        }
        if ((pTool->joy.rep & JOY_A) && pTool->litPath.path[pTool->x11] == NULL) {
            pTool->joy.rep &= ~JOY_A;
            pTool->x12 = 0;
            pTool->x10 = 1;
        }
        if (pTool->joy.trg & JOY_Y) {
            pTool->x12 = 0;
            pTool->x10 = 1;
        }
        break;
    case 1:
        if (pTool->litPath.path[pTool->x11] != NULL) {
            eprintf(0xA0, 0x54, 0, pTool->color, "DELETE?");
            eprintf(0xB0, 0x62, pTool->x12 == 0 ? 0x14 : 0, pTool->color, "YES");
            eprintf(0xB0, 0x70, pTool->x12 != 0 ? 0x14 : 0, pTool->color, "NO");
            if (pTool->joy.rep & JOY_UP) {
                pTool->x12 = 1;
            } else if (pTool->joy.rep & JOY_DOWN) {
                pTool->x12 = 0;
            }
            if (pTool->joy.rep & JOY_A) {
                if (pTool->x12 == 1) {
                    Debug_free(pTool->litPath.path[pTool->x11]);
                    pTool->litPath.path[pTool->x11] = NULL;
                    pTool->litPath.createPath(pLitPath);
                }
                pTool->x10 = 0;
            }
        } else {
            eprintf(0xA0, 0x54, 0, pTool->color, "CREATE?");
            eprintf(0xB0, 0x62, pTool->x12 == 0 ? 0x14 : 0, pTool->color, "YES");
            eprintf(0xB0, 0x70, pTool->x12 != 0 ? 0x14 : 0, pTool->color, "NO");
            if (pTool->joy.rep & JOY_UP) {
                pTool->x12 = 1;
            } else if (pTool->joy.rep & JOY_DOWN) {
                pTool->x12 = 0;
            }
            if (pTool->joy.rep & JOY_A) {
                if (pTool->x12 == 1) {
                    np = (cLightPathData*) Debug_alloc(2, 1);
                    pTool->litPath.path[pTool->x11] = np;
                    np->data[0] = 200;
                    np->data[1] = 0xFF;
                    pTool->litPath.edit[0] = 200;
                    pTool->litPath.edit[1] = 0xFF;
                }
                pTool->x10 = 0;
            }
        }
        if (pTool->joy.rep & JOY_B) {
            pTool->x10 = 0;
        }
        break;
    }
    if (PTR_OK(pTool->litPath.path[pTool->x11])) {
        drawPath(x, y, (cLightPathData*) pTool->litPath.edit, 0, 0xFFFFFFFF);
        eprintf(x + 0x140, y + 0x68, 0, pTool->color, "%2.2fsec",
                (f32) (((cLightPathData*) pTool->litPath.edit)->getSize() - 1) / 30.0f);
    } else {
        eprintf(x + 0x20, y + 0x2A, 0, pTool->color, "NO DATA");
    }
    return pTool->x11;
}
// Path editor: LEFT / RIGHT move along the steps (RIGHT past the end appends one), UP / DOWN and the
// stick set the brightness of the step, Y ends the path there.
int pathEdit(int x, int y, u8 no, u8 flag, int mode)
{
    static f32 val;
    cLightPathData* p;
    u32 size;

    if (pTool->x10 == 0) {
        val = (f32) pTool->litPath.edit[0];
        pTool->x10 = 1;
    }
    if (pTool->joy.rep & 0x20002) {
        pTool->x11++;
        if (pTool->litPath.edit[pTool->x11] == 0xFF) {
            pTool->litPath.edit[pTool->x11] = (pTool->x11 != 0) ? pTool->litPath.edit[pTool->x11 - 1] : 0;
            pTool->litPath.edit[pTool->x11 + 1] = 0xFF;
        }
        val = (f32) pTool->litPath.edit[pTool->x11];
    }
    if (pTool->joy.rep & 0x10001) {
        if (pTool->x11 != 0) {
            pTool->x11--;
            val = (f32) pTool->litPath.edit[pTool->x11];
        }
    }
    if (pTool->joy.rep & JOY_UP) {
        val += (pTool->joy.on & JOY_A) ? 6.0f : 2.0f;
    }
    if (pTool->joy.rep & JOY_DOWN) {
        val -= (pTool->joy.on & JOY_A) ? 6.0f : 2.0f;
    }
    val += (f32) pTool->joy.sy * ((pTool->joy.on & JOY_A) ? 0.15f : 0.04f);
    if (val < 0.0f) {
        val = 0.0f;
    } else if (val >= 200.0f) {
        val = 200.0f;
    }
    pTool->litPath.edit[pTool->x11] = (u8) val;
    if (pTool->litPath.path[no] != NULL) {
        Debug_free(pTool->litPath.path[no]);
        size = ((cLightPathData*) pTool->litPath.edit)->getSize();
        pTool->litPath.path[no] = (cLightPathData*) Debug_alloc(size, 1);
        memcpy(pTool->litPath.path[no], pTool->litPath.edit, size);
    }
    pTool->litPath.createPath(pLitPath);
    if (pTool->joy.trg & JOY_Y) {
        pTool->litPath.edit[pTool->x11 + 1] = 0xFF;
    }
    if (PTR_OK(pTool->litPath.path[no])) {
        drawPath(x, y, (cLightPathData*) pTool->litPath.edit, 0, pTool->x11);
        eprintf(x + 0x120, y + 0x68, 0, pTool->color, "%3d%% %2.2f/%2.2f", (pTool->litPath.edit[pTool->x11] + 1) >> 1,
                (f32) pTool->x11 / 30.0f, (f32) (((cLightPathData*) pTool->litPath.edit)->getSize() - 1) / 30.0f);
    } else {
        eprintf(x + 0x20, y + 0x2A, 0, pTool->color, "NO DATA");
    }
    return pTool->x11;
}
// Brightness graph of a light path: axes, one bar per step (flag bit1 inverts), step `cur` highlighted.
void drawPath(int x, int y, cLightPathData* p, u8 flag, u32 cur)
{
    Vec a;
    Vec b;
    u32 i;
    u32 xi;
    f32 f;
    u8* d = p->data;  // the data pointer steps (`lbzu`), `i` stays a counter for the `i == cur` tests

    a.x = (f32) x;
    a.y = (f32) (y - 10);
    a.z = 0.0f;
    b.x = (f32) x;
    b.y = (f32) (y + 110);
    b.z = 0.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    a.x = (f32) (x - 10);
    a.y = (f32) (y + 100);
    a.z = 0.0f;
    b.x = (f32) (x + 410);
    b.y = (f32) (y + 100);
    b.z = 0.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    for (i = 0, xi = x; *d <= 200; xi += 4, i++, d++) {
        a.x = (f32) xi;
        a.y = (f32) y;
        a.z = 0.0f;
        b.x = (f32) xi;
        b.y = (f32) (y + 100);
        b.z = 0.0f;
        Draw_line(&a, &b, (i == cur) ? 0x40A0A0A0 : 0x40404040);
        f = (f32) *d * 0.5f;
        if (flag & 2) {
            f = 100.0f - f;
        }
        a.x = (f32) xi;
        a.y = (f32) (y + 100);
        a.z = 0.0f;
        b.x = (f32) xi;
        b.y = (f32) (y + 100) - f;
        b.z = 0.0f;
        Draw_line(&a, &b, (i == cur) ? 0xFFFF0000 : 0xFFA0A0A0);
    }
}

static inline cObj* objWorkNoChk(u32 no)
{
    return (cObj*) ((u8*) ObjMgr.pArray + ObjMgr.size * no);
}

int cLightTool::lightAnalysis()
{
    u32 i;
    u32 n;

    if (!PTR_OK(anaTbl)) {
        return 0;
    }
    n = ObjMgr.nArray;
    memclr_asm(anaTbl, n * 4);
    anaNum = 0;
    for (i = 0; i < n; i++) {
        if (objWorkNoChk(i)->isAlive()) {
            cObj* obj = objWorkNoChk(i);
            if (obj->x12E == 2) {
                if (obj->lightInfo.getLightNum() > 4) {
                    int id = SmdGetWorkId(obj);
                    u8* p = (u8*) (anaNum * 4 + (u32) anaTbl);
                    if (id != -1) {
                        p[3] = id;
                    } else {
                        p[3] = 0xFF;
                    }
                    anaTbl[anaNum * 4] = obj->lightInfo.getLightNum();
                    anaNum++;
                }
            }
        }
    }
    {
        u32 pln = pPL->lightInfo.getLightNum();
        eprintf(0x1C8, 0x1C, 0, 0, "PL");
        eprintf(0x1E0, 0x1C, pln > 3 ? 0x16 : 0, 0, "%2d", pln);
    }
    for (i = 0; i < anaNum; i++) {
        if (anaTbl[i * 4 + 3] == 0xFF) {
            eprintf(0x1C8, 0x2A + i * 14, 0x16, 0, "-- %2d", anaTbl[i * 4]);
        } else {
            eprintf(0x1C8, 0x2A + i * 14, 0x16, 0, "%2d %2d", anaTbl[i * 4 + 3], anaTbl[i * 4]);
        }
    }
    return 1;
}

cLitPathTool::cLitPathTool()
{
    memclr_asm(path, sizeof(path));
    memclr_asm(edit, sizeof(edit));
    if (!(LightMgr.x1AC & 2)) {
        cLightPathHeader* hdr;
        u32 size;
        LightMgr.x1AC |= 2;
        hdr = (cLightPathHeader*) LightMgr.getPathHeader();
        size = hdr->getSize() + 0x2800;
        if (size <= 0xC7FF) {
            size = 0xC800;
        }
        pLitPath = (cLightPathHeader*) Debug_alloc(size, 0);
        if (!PTR_OK(pLitPath)) {
            TOOL_ERR("cLitPathTool() Memory Alloc Failed");
            return;
        }
        memcpy(pLitPath, hdr, size);
        LightMgr.initPath((LightPathHeader*) pLitPath);
    } else {
        // through a reference: the store address (`lis LitPathPtr@ha`) is evaluated before the call
        // and kept in a callee-saved register; a plain store forms it after the call
        cLightPathHeader*& p = pLitPath;
        p = (cLightPathHeader*) LightMgr.getPathHeader();
    }
    expand(pLitPath);
    if (PTR_OK(path[0])) {
        memcpy(edit, path[0], path[0]->getSize());
    }
}

cLitPathTool::~cLitPathTool()
{
}

int cLitPathTool::expand(cLightPathHeader* hdr)
{
    u32 i;

    if (!PTR_OK(hdr)) {
        return 0;
    }
    memclr_asm(path, sizeof(path));
    for (i = 0; i < hdr->num; i++) {
        u32 ofs = ((u32*) (hdr + 1))[i];
        if (ofs) {
            cLightPathData* src = hdr->getPathData(i);
            u32 size = src->getSize();
            cLightPathData* dst = (cLightPathData*) Debug_alloc(size, 1);
            path[i] = dst;
            memcpy(dst, src, size);
        } else {
            path[i] = (cLightPathData*) ofs;
        }
    }
    return 1;
}

// Path header as the tool writes it: count, then the offset table (cLightPathHeader keeps the table
// implicit; the tool indexes it).
struct LitPathHdr {
    u8 num;      // 0x00
    u8 pad_1[3];
    u32 ofs[1];  // 0x04
};

int cLitPathTool::createPath(cLightPathHeader* dst)
{
    LitPathHdr* h = (LitPathHdr*) dst;
    u32 i;
    u8* p;

    h->pad_1[0] = h->pad_1[1] = h->pad_1[2] = h->num = 0;
    for (i = 0; i < 256; i++) {
        if (path[i]) {
            h->num = i + 1;
        }
    }
    p = (u8*) &h->ofs[h->num];
    for (i = 0; i < h->num; i++) {
        if (path[i]) {
            u8* s = path[i]->data;
            ((u32*) dst)[i + 1] = (u32) p - (u32) dst;
            *p = *s;
            while (*s != 0xFF) {
                s++;
                p++;
                *p = *s;
            }
            p++;
        } else {
            ((u32*) dst)[i + 1] = (u32) path[i];
        }
    }
    return 1;
}

// the next object's .data is 8-aligned: the split object carries the 4-byte pad
asm(".section .data; .balign 8");
