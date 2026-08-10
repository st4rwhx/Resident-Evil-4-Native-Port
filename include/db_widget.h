#ifndef DB_WIDGET_H
#define DB_WIDGET_H

#include "types.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "main_mem.h"

// Effect tool (t_esp) window system: t_esp/db_widget.cpp (the primitive classes, vtables) and
// t_esp/db_window.cpp (DB_MOUSE/DB_KEYBORD/DB_PRIM_ARRAY). The drawing/input bridge (DB_DrawBox,
// DB_DrawString, DB_GetKeybordData...) is t_esp/db_port.cpp. Field names are ours.

extern "C" unsigned int strlen(const char* s);
extern "C" char* strcpy(char* dst, const char* src);
extern "C" void exit(int) __attribute__((noreturn));

void DB_DrawBox(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a);
void DB_DrawBoxFill(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a);
void DB_DrawString(f32 x, f32 y, const char* s, f32 r, f32 g, f32 b, f32 a);

struct DB_POINT {
    f32 x;
    f32 y;

    DB_POINT() {}
    DB_POINT(f32 x_, f32 y_) {
        x = x_;
        y = y_;
    }
};

struct DB_RECT {
    f32 x;
    f32 y;
    f32 w;
    f32 h;

    DB_RECT() {}
    DB_RECT(f32 x_, f32 y_, f32 w_, f32 h_) {
        x = x_;
        y = y_;
        w = w_;
        h = h_;
    }
    int ChkHitRect(DB_POINT* p);
};

struct DB_COLOR {
    f32 r;
    f32 g;
    f32 b;
    f32 a;

    DB_COLOR() { a = b = g = r = 1.0f; }
    // DB_WINDOW's member init: the constant comes from the caller (an unchanging pool MEM), the
    // inlined default ctor's own 1.0f loses RTX_UNCHANGING_P in integrate and waits for the vptr store
    DB_COLOR(f32 v) { a = b = g = r = v; }
    DB_COLOR(f32 r_, f32 g_, f32 b_, f32 a_) {
        r = r_;
        g = g_;
        b = b_;
        a = a_;
    }
};

// DB_NUMERIC::numType (SetNumPointer overload that bound the pointer)
enum {
    DB_NUM_S8 = 0,
    DB_NUM_U8 = 1,
    DB_NUM_S16 = 2,
    DB_NUM_U16 = 3,
    DB_NUM_S32 = 4,
    DB_NUM_U32 = 5,
    DB_NUM_F32 = 6,
};

// DB_PRIMITIVE::type
enum {
    DB_PRIM_BASE = 0,
    DB_PRIM_WINDOW = 1,
    DB_PRIM_WINDOW_TITLE = 2,
    DB_PRIM_BUTTON_CLOSE = 3,
    DB_PRIM_STRING = 4,
    DB_PRIM_BUTTON = 5,
    DB_PRIM_NUMERIC = 6,
};

// DB_PRIMITIVE::flag
#define DB_PRIM_FLAG_SELECTABLE 0x1
#define DB_PRIM_FLAG_MOUSE_ON 0x2

// DB_NUMERIC::numFlg
#define DB_NUM_FLAG_LOCK 0x1
#define DB_NUM_FLAG_NO_SELECT 0x2
#define DB_NUM_FLAG_HEX 0x4
#define DB_NUM_FLAG_NO_LIMIT 0x8
#define DB_NUM_FLAG_NO_FLOAT_MSG 0x10
#define DB_NUM_FLAG_LOOP 0x20
#define DB_NUM_FLAG_SIGNED_VIEW 0x40

// OnCalcMsg codes (DB_PRIM_ARRAY::ActiveChangeKeybord*)
enum {
    DB_CALC_MIN = 0,
    DB_CALC_SUB_X1000 = 1,
    DB_CALC_SUB_X100 = 2,
    DB_CALC_SUB_X10 = 3,
    DB_CALC_SUB = 4,
    DB_CALC_SUB_X01 = 5,
    DB_CALC_ADD_X01 = 6,
    DB_CALC_ADD = 7,
    DB_CALC_ADD_X10 = 8,
    DB_CALC_ADD_X100 = 9,
    DB_CALC_ADD_X1000 = 10,
    DB_CALC_MAX = 11,
    DB_CALC_DEFAULT = 12,
    DB_CALC_NONE = 13,
};

// Value range per DB_NUMERIC type. Both objects carry a copy that only dead code references:
// db_widget.cpp as a namespace-scope static (output at end of file, behind the vtables),
// db_window.cpp as the local static of a dead-stripped helper (output mid-file).
#define DB_NUM_RANGE_INIT                           \
    {                                               \
        { -128.0f, 127.0f },                        \
        { 0.0f, 255.0f },                           \
        { -32768.0f, 32767.0f },                    \
        { 0.0f, 65535.0f },                         \
        { -2147483648.0f, 2147483647.0f },          \
        { 0.0f, 4294967295.0f },                    \
        { 0.0f, 255.0f },                           \
    }

class DB_KEYBORD;
class DB_PRIMITIVE;

typedef void (*DB_PRIM_CALLBACK)(DB_PRIMITIVE*);

class DB_PRIMITIVE {
public:
    int type;                   // 0x00
    DB_PRIMITIVE* parent;       // 0x04
    DB_PRIMITIVE* child;        // 0x08
    DB_PRIMITIVE* prev;         // 0x0C
    DB_PRIMITIVE* next;         // 0x10
    DB_POINT pos;               // 0x14  relative to the parent
    DB_POINT drawPos;           // 0x1C  screen position (DrawRequest)
    DB_RECT rect;               // 0x24  hit rectangle (relative)
    DB_POINT size;              // 0x34
    DB_POINT base;              // 0x3C
    int id;                     // 0x44
    u32 flag;                   // 0x48
    int click[3];               // 0x4C  per mouse button
    int active;                 // 0x58
    int mouseOn;                // 0x5C
    int select;                 // 0x60
    DB_PRIM_CALLBACK onHitCb;   // 0x64
    DB_PRIM_CALLBACK updateCb;  // 0x68
    DB_PRIM_CALLBACK drawCb;    // 0x6C
    // 0x70 vptr

    DB_PRIMITIVE();
    virtual ~DB_PRIMITIVE();
    void SetOnHitCallback(DB_PRIM_CALLBACK cb);
    void CallOnHitCallback();
    void SetUpdateCallback(DB_PRIM_CALLBACK cb);
    void CallUpdateCallback();
    void SetDrawCallback(DB_PRIM_CALLBACK cb);
    void CallDrawCallback();
    void SetSize(f32 w, f32 h);
    void SetBase(f32 x, f32 y);
    int AddChild(DB_PRIMITIVE* p);
    int AddBrother(DB_PRIMITIVE* p);
    virtual void Update();
    void DrawRequest();
    virtual void Draw();
    int ChkClick(DB_POINT* p, int btn);
    int ChkDoubleClick(DB_POINT* p, int btn);
    int ChkMouseUp(DB_POINT* p, int btn);
    int ChkMouseOn(DB_POINT* p);
    int ChkMouseDrag(DB_POINT* p, int btn);
    virtual void OnClick(DB_POINT* p, int btn);
    virtual void OnDoubleClick(DB_POINT* p, int btn);
    virtual void OnMouseUp(DB_POINT* p, int btn);
    virtual void OnMouseDrag(DB_POINT* p, int btn);
    virtual void OnKeybord(DB_KEYBORD* key);
    virtual void OnCalcMsg(int msg);
    virtual void OnCalcMsgFloat(f32 v);
};

// Grid of selectable primitives of a window (keyboard focus).
class DB_ACTIVE_SELECT {
public:
    u32 w;                  // 0x00
    u32 h;                  // 0x04
    DB_PRIMITIVE** tbl;     // 0x08  [h][w]
    DB_PRIMITIVE* active;   // 0x0C
    int num;                // 0x10
    u32 selX;               // 0x14
    u32 selY;               // 0x18
    int keyMode;            // 0x1C  0 = DB_PRIM_ARRAY::ActiveChangeKeybordNormal, 1 = ...MiniWin
    // 0x20 vptr

    DB_ACTIVE_SELECT();
    virtual ~DB_ACTIVE_SELECT();
    void SetSelX(u32 x);
    void SetSelY(u32 y);
    int AddPrimitive(DB_PRIMITIVE* p, int x, int y);
    DB_PRIMITIVE* GetActivePrimitive();
    int SetActivePrimitive(DB_PRIMITIVE* p);
    DB_PRIMITIVE* SetActiveUp();
    DB_PRIMITIVE* SetActiveDown();
    DB_PRIMITIVE* SetActiveLeft();
    DB_PRIMITIVE* SetActiveRight();
    DB_PRIMITIVE* SetActiveNext();
    DB_PRIMITIVE* SetActiveDefault();
};

class DB_WINDOW;
typedef void (*DB_WINDOW_CALLBACK)(DB_WINDOW*);

// DB_WINDOW::keyFlag
#define DB_WIN_KEY_NO_TITLE 0x1
#define DB_WIN_KEY_NO_CLOSE 0x2
#define DB_WIN_KEY_ESC_CLOSE 0x4

class DB_WINDOW : public DB_PRIMITIVE {
public:
    DB_COLOR color;                         // 0x74
    u32 keyFlag;                            // 0x84
    int bring;                              // 0x88  bring to front request
    DB_WINDOW_CALLBACK closeCb;             // 0x8C
    DB_WINDOW_CALLBACK activeChangeCb;      // 0x90
    DB_ACTIVE_SELECT sel;                   // 0x94
    // 0xB8

    DB_WINDOW();
    int CallActiveChangeCallback(DB_PRIMITIVE* p, DB_KEYBORD* k);   // p, k unused
    void SetActiveChangeCallback(DB_WINDOW_CALLBACK cb);
    void SetCloseCallback(DB_WINDOW_CALLBACK cb);
    virtual void OnClick(DB_POINT* p, int btn);
    int AddSelectablePrimitive(DB_PRIMITIVE* p, int x, int y);
    void Close();
    virtual void Draw();
    virtual void OnKeybord(DB_KEYBORD* key);
};

class DB_WINDOW_TITLE : public DB_PRIMITIVE {
public:
    char str[256];      // 0x74
    f32 cr;             // 0x174
    f32 cg;
    f32 cb;
    f32 ca;
    // 0x184

    DB_WINDOW_TITLE(const char* s);
    void SetStringColor(f32 r, f32 g, f32 b, f32 a);
    int SetString(const char* s);
    virtual void Draw();
    virtual void OnMouseDrag(DB_POINT* p, int btn);
    virtual void OnDoubleClick(DB_POINT* p, int btn);
};

class DB_BUTTON_CLOSE : public DB_PRIMITIVE {
public:
    DB_BUTTON_CLOSE();
    virtual void Draw();
    virtual void OnClick(DB_POINT* p, int btn);
};

class DB_STRING : public DB_PRIMITIVE {
public:
    char* str;          // 0x74
    u32 len;            // 0x78
    u32 max;            // 0x7C
    f32 cr;             // 0x80
    f32 cg;
    f32 cb;
    f32 ca;
    // 0x90

    DB_STRING(u32 max, const char* s);
    virtual ~DB_STRING();
    void SetColor(f32 r, f32 g, f32 b, f32 a);
    int SetString(const char* s);
    virtual void Draw();
};

class DB_BUTTON : public DB_STRING {
public:
    DB_PRIM_CALLBACK cb;    // 0x90
    // 0x94

    DB_BUTTON();
    void SetCallback(DB_PRIM_CALLBACK cb);
    virtual void OnClick(DB_POINT* p, int btn);
    virtual void OnKeybord(DB_KEYBORD* key);
};

class DB_NUMERIC : public DB_STRING {
public:
    void* pNum;         // 0x90
    int numType;        // 0x94
    f32 def;            // 0x98
    f32 step;           // 0x9C
    f32 max;            // 0xA0
    f32 min;            // 0xA4
    int keta;           // 0xA8
    int ketaFloat;      // 0xAC
    int edit;           // 0xB0  keyboard entry in progress
    int minus;          // 0xB4
    f32 unit;           // 0xB8
    u32 numFlg;         // 0xBC
    const char** nameTbl;   // 0xC0
    u32 nameNum;        // 0xC4
    // 0xC8

    DB_NUMERIC();
    void SetNumFlg(u32 flg);
    f32 GetNumFloat();
    void SetNumFloat(f32 v);
    void SetDefault(f32 v);
    void SetKeta(u32 n);
    void SetKetaFloat(int n);
    void SetNumPointer(s8* p);
    void SetNumPointer(u8* p);
    void SetNumPointer(s16* p);
    void SetNumPointer(u16* p);
    void SetNumPointer(s32* p);
    void SetNumPointer(u32* p);
    void SetNumPointer(f32* p);
    void ClearToDefault();
    virtual void OnDoubleClick(DB_POINT* p, int btn);
    int SetStringInt();
    int SetStringFloat();
    virtual void Update();
    virtual void OnCalcMsg(int msg);
    virtual void OnCalcMsgFloat(f32 v);
    virtual void OnKeybord(DB_KEYBORD* key);
};

class DB_NUMERIC2 : public DB_NUMERIC {
public:
    void* pNum2;        // 0xC8
    int lastMsg;        // 0xCC
    // 0xD0

    DB_NUMERIC2();
    void SetNumPointer2(s8* p);
    void SetNumPointer2(u8* p);
    void SetNumPointer2(s16* p);
    void SetNumPointer2(u16* p);
    void SetNumPointer2(s32* p);
    void SetNumPointer2(f32* p);
    void SetNumFloat2(f32 v);
    f32 GetNumFloat2();
    virtual void OnCalcMsg(int msg);
    virtual void OnCalcMsgFloat(f32 v);
};

class DB_SLIDEBAR : public DB_PRIMITIVE {
public:
    f32 length;     // 0x74
    f32 min;        // 0x78
    f32 max;        // 0x7C
    f32 rate;       // 0x80
    int numType;    // 0x84
    void* pNum;     // 0x88
    // 0x8C

    DB_SLIDEBAR();
    void UpdateHoldNum();
    virtual void Update();
    virtual void OnMouseDrag(DB_POINT* p, int btn);
    virtual void Draw();
};

// Mouse snapshot (DB_GetMouseData); passed by value to DB_PRIM_ARRAY::ChkMouseButton.
class DB_MOUSE {
public:
    DB_POINT pos;       // 0x00
    DB_POINT oldPos;    // 0x08
    DB_POINT move;      // 0x10
    int on[3];          // 0x18
    int trg[3];         // 0x24
    int click[3];       // 0x30
    int up[3];          // 0x3C
    int dblClick[3];    // 0x48
    int drag[3];        // 0x54
    // 0x60

    DB_MOUSE();
};

// Pad-as-keyboard snapshot (DB_GetKeybordData): 12 keys with on / trigger / repeat flags.
class DB_KEYBORD {
public:
    u8 x0;              // 0x00
    u8 chr;             // 0x01  typed character (DB_NUMERIC::OnKeybord)
    f32 stickX;         // 0x04
    f32 stickY;         // 0x08
    f32 xC;             // 0x0C
    int stickUp;        // 0x10
    int stickDown;      // 0x14
    int stickLeft;      // 0x18
    int stickRight;     // 0x1C
    int on[13];         // 0x20  up, down, left, right, x30, A, B, X, Y, L, R, Z, start
    int trg[13];        // 0x54
    int rep[12];        // 0x88
    u8 cnt[12];         // 0xB8
    // 0xC4

    DB_KEYBORD();
    void Update();
    void ClearAllKey();
};

// Every primitive and every window of the tool.
class DB_PRIM_ARRAY {
public:
    u32 numPrim;                    // 0x0000
    DB_PRIMITIVE* prim[0x400];      // 0x0004
    DB_PRIMITIVE* active;           // 0x1004
    u32 numWin;                     // 0x1008
    DB_WINDOW* win[0x100];          // 0x100C
    DB_WINDOW* activeWin;           // 0x140C
    DB_WINDOW* oldActiveWin;        // 0x1410
    // 0x1414

    DB_PRIM_ARRAY();
    ~DB_PRIM_ARRAY();
    int ChkMouseButton(DB_PRIMITIVE* p, DB_MOUSE m, int btn);
    int RegistPrimitive(DB_PRIMITIVE* p);
    int DeletePrimitive(DB_PRIMITIVE* p);
    int RegistWindow(DB_WINDOW* w);
    DB_WINDOW* MakeWindowPrimitive();
    DB_WINDOW_TITLE* MakeWindowTitlePrimitive();
    DB_BUTTON_CLOSE* MakeButtonClosePrimitive();
    DB_STRING* MakeStringPrimitive();
    DB_BUTTON* MakeButtonPrimitive();
    DB_NUMERIC* MakeNumericPrimitive();
    DB_NUMERIC2* MakeNumeric2Primitive();
    DB_WINDOW* CreateNormalWindow(const char* title, DB_POINT* pos, f32* w, f32* h, u32* keyFlag);
    DB_STRING* CreateString(DB_PRIMITIVE* parent, const char* s, DB_POINT* pos);
    DB_NUMERIC* CreateNumeric(DB_WINDOW* w, s8* p, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC* CreateNumeric(DB_WINDOW* w, u8* p, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC* CreateNumeric(DB_WINDOW* w, u16* p, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC* CreateNumeric(DB_WINDOW* w, s32* p, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC* CreateNumeric(DB_WINDOW* w, u32* p, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC* CreateNumeric(DB_WINDOW* w, f32* p, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC2* CreateNumeric2(DB_WINDOW* w, s8* p, s8* p2, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC2* CreateNumeric2(DB_WINDOW* w, u8* p, u8* p2, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC2* CreateNumeric2(DB_WINDOW* w, s16* p, s16* p2, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC2* CreateNumeric2(DB_WINDOW* w, u16* p, u16* p2, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC2* CreateNumeric2(DB_WINDOW* w, s32* p, s32* p2, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_NUMERIC2* CreateNumeric2(DB_WINDOW* w, f32* p, f32* p2, DB_POINT* pos, int* selX, int selY, u32 flg);
    DB_BUTTON* CreateButton(DB_WINDOW* w, const char* s, DB_POINT* pos, DB_PRIM_CALLBACK cb, int* selX, int selY);
    DB_WINDOW* BringWindow(DB_WINDOW* w);
    DB_PRIMITIVE** PullPrimitivePtr();
    DB_WINDOW** PullWindowPtr();
    void ClearAllActive();
    void ButtonUpdate(DB_MOUSE* m);
    void SelectUpdate(DB_MOUSE* m);
    void ActivePrimitiveUpdate();
    void ActiveChangeKeybordNormal(DB_KEYBORD* k);
    void ActiveChangeKeybordMiniWin(DB_KEYBORD* k);
    void ActiveChangeKeybord(DB_KEYBORD* k);
    void Update(DB_MOUSE* m, DB_KEYBORD* k);
    void Draw();
};

#endif
