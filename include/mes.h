#ifndef MES_H
#define MES_H

#include "types.h"
#include "vec.h"
#include "gx.h"
#include "tpl.h"

// game/mes.cpp: in-game message system (fonts, message queues, control codes).

// Language table (MesData.lang): 0 JP, 1 EN, 2 DE, 3 FR, 4 ES, 5 IT.
struct MessageData {
    u32 lang;      // 0x00
    u8* ptr[5];    // 0x04  message tables (type 0..4), each: u32 x0, u32 ofs[lang]

    u16* getAddr(int no, int type);
    int getMesNum(int type);
    int getSpaceWidth();
    void setPtr(int type, u8* p) { ptr[type] = p; }
};

// One queued glyph (MsgQueue entries, 0x10 bytes).
class MessageFont;
struct MesQue {
    u16 x;              // 0x00
    u16 y;              // 0x02
    u16 code;           // 0x04
    u8 w;               // 0x06
    u8 h;               // 0x07
    u32 color;          // 0x08
    MessageFont* font;  // 0x0C
};

// One texture sheet of a font (0x64 bytes).
struct MesFontTex {
    GXTexObj tex;       // 0x00
    GXTlutObj tlut;     // 0x20
    Mtx mtx;            // 0x2C
    TEXHeader* pTex;    // 0x5C
    TEXPalette* pTpl;   // 0x60
};

// Font (MesFont[4], 0xDC bytes each): a TPL with up to two sheets and a glyph width table.
class MessageFont {
public:
    u32 flags;          // 0x00  bit 0 = loaded
    TEXPalette* pTpl;   // 0x04
    MesFontTex tex[2];  // 0x08
    u8* pWidth;         // 0xD0  per glyph: left, right (s8 pairs)
    u16 texW;           // 0xD4  sheet 0 width
    u16 texH;           // 0xD6  sheet 0 height
    u8 cellW;           // 0xD8
    u8 cellH;           // 0xD9
    u8 pad_DA[2];

    s16 getSize(s16 code, s8* left, s8* right);
    void create(int w, int h, TEXPalette* tpl, u8* width);
    void destroy();
    int chkFlag(u32 b) { return (flags & b) ? 1 : 0; }
};

// One message slot (MessageControl::mes[16], 0xEC bytes).
class Message {
public:
    u32 saveStop;       // 0x00  pG->flags_170 saved while the message stops the game
    u32 flags;          // 0x04  bit 0 = active, bit 1 = first frame
    u8 x8;              // 0x08  code01 step
    u8 x9;
    u8 xA;
    u8 xB;
    u32 flags2;         // 0x0C  bit 0 = active, bit 1 = finished, bit 3 = width check pass
    f32 scaleX;         // 0x10
    f32 scaleY;         // 0x14
    s8 fontW;           // 0x18
    s8 fontH;           // 0x19
    u16 x1A;            // 0x1A  message number for code10 (type 3 table)
    u16 ot;             // 0x1C  ordering table
    u16 otNo;           // 0x1E
    MessageFont* font;  // 0x20
    u16 x;              // 0x24  cursor
    u16 y;              // 0x26
    u16 lineX[16];      // 0x28
    u16 baseY;          // 0x48
    u16 lineW[16];      // 0x4A
    s16 maxW;           // 0x6A
    u16 x6C;
    u16 waitCnt;        // 0x6E
    u8 x70;             // 0x70  code08 started
    s8 x71;             // 0x71  code0d
    s8 line;            // 0x72
    u8 x73;
    u16 numW;           // 0x74  width added by numbers/tables (code0a)
    union {
        u16 lineH;      // 0x76
        struct {
            u8 lineH_hi;    // 0x76
            s8 lineSpace;   // 0x77  (embox emBoxAction: prompt y = 336 - fontH - lineSpace - 1)
        };
    };
    u16 charSpace;      // 0x78
    u16 x7A;
    u32 color;          // 0x7C
    u32 attr;           // 0x80
    s16 speed;          // 0x84
    s16 speedCnt;       // 0x86
    s16 speedSave;      // 0x88
    s16 skip;           // 0x8A
    u16 x8C;
    s16 waitEnd;        // 0x8E
    u16 jumpTbl[3];     // 0x90
    s8 jumpIdx;         // 0x96
    s8 jumpCnt;         // 0x97
    u16* pMsg;          // 0x98
    u16* savePtr;       // 0x9C
    MessageFont* saveFont;  // 0xA0
    u32 number;         // 0xA4
    u16 digit;          // 0xA8
    u8 pad_AA[2];
    u32 numberSave;     // 0xAC
    u16 digitSave;      // 0xB0
    u8 pad_B2[6];
    MesQue* qbase;      // 0xB8
    MesQue* qp;         // 0xBC
    MesQue* selCur[8];  // 0xC0  glyphs of the selection cursors
    s8 selNum;          // 0xE0
    s8 result;          // 0xE1  menu selection (0 = none yet)
    s8 cursor;          // 0xE2
    s8 cursorAnim;      // 0xE3
    u8 xE4;             // 0xE4  code12
    u8 pad_E5[3];

    virtual ~Message() {}

    int chkFlag(u32 b) { return (flags & b) ? 1 : 0; }
    void clrActive() { flags &= ~1; }
    void init(int no, int x, int y, u32 attr, int col, MessageFont* font);
    void move();
    void WidthCk();
    void QueSet(int code, MessageFont* font);
    void setNumber(u32 num, u16 digits);
    void putSelCursol();
    void putNextCursol(int reset);
    void setJump(u16 pos);
    void trans();
    int CommandExec();
    int CommandArg();
    void WaitEnd();
    int code00();
    int code01();
    int code02();
    int code03();
    int code04();
    int code05();
    int code06();
    int code07();
    int code08();
    int code09();
    int code0a();
    int code0b();
    int code0c();
    int code0d();
    int code0e();
    int code0f();
    int code10();
    int code11();
    int code12();
};

typedef Message MesWork;

// game/mes.cpp
class MessageControl {
public:
    u32 x0;
    Message mes[16];        // 0x04
    void* fontBuf[4];       // 0xEC4
    u32 state;              // 0xED4
    u8 pad_ED8[0x11F8 - 0xED8];
    u32 x11F8;              // 0x11F8

    virtual ~MessageControl() {}

    MesWork* getWork() { return &mes[0]; }
    // Slot address the way the original computes it (index scaled first, then the base).
    Message* getMes(int no) { return (Message*) (no * sizeof(Message) + (u32) this + sizeof(u32)); }

    void setLayout(int no, int layout);
    void setLanguage(int lang);
    void setupFont(int w, int h, TEXPalette* tpl, int no);
    void releaseFont(int no);
    int loadFont(int w, int h, const char* name, int no);
    void init();
    void gameInit();
    void roomInit();
    void loadCommonFont();
    void loadSystemFont();
    void stageInit();
    void loadStageFont();
    void loadEventFont();
    void setState(u32 b);
    void unsetState(u32 b);
    int checkState(u32 b);
    void Move();
    void Trans();
    void setFontSize(int no, s8 w, s8 h);
    void MesSet(int no, int x, int y, u32 attr, int slot, int col, int type);
    void Delete(int no);
    void WaitEnd(int no);
};

// ROM font glyph renderer (game/mes.cpp), used by the dvd error screen before the message
// system is up.
class RomFont {
public:
    void* pFont;  // 0x00  OSFontHeader

    RomFont(void* font);
    void setup(void* image);
    void draw(int x, int y, int cx, int cy);
};

extern MessageControl cMes;
extern MessageData MesData;
extern u32 mes_col_tbl[10];

#endif
