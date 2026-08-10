// game/mes: in-game message system (D:/Bio4/Prog/mes.cpp).
#include "types.h"
#include "vec.h"
// Declared before global.h/mes.h name MesData: uninitialised objects are emitted in first-declaration
// order and the original .bss is cMes, MesFont, MesData, MsgQueue.
class MessageFont;
extern MessageFont MesFont[4];
#include "gx.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "light.h"
#include "mes.h"
#include "dvd.h"
#include "db_log.h"
#include "id_sys.h"
#include "cockpit.h"
#include "pad.h"
#include "trans_ot.h"
#include "view.h"
#include "snd.h"

extern "C" {
int sprintf(char* buf, const char* fmt, ...);
void C_MTXOrtho(f32 m[4][4], f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
void calcTplOffset(TEXPalette* tpl);  // game/model.cpp
u16 getCharCode(u16 code);
int isCtrlCode(u16 code);
void setAttribute(MesFontTex* t);
void draw(MesQue* q);
void messageCamera();
void messageTrans(MesQue* q);
}

// Dolphin OS ROM font header (only the fields RomFont reads).
struct OSFontHeader {
    u16 fontType;     // 0x00
    u16 firstChar;    // 0x02
    u16 lastChar;     // 0x04
    u16 invalChar;    // 0x06
    u16 ascent;       // 0x08
    u16 descent;      // 0x0A
    u16 width;        // 0x0C
    u16 leading;      // 0x0E
    u16 cellWidth;    // 0x10
    u16 cellHeight;   // 0x12
    u32 sheetSize;    // 0x14
    u16 sheetFormat;  // 0x18
    u16 sheetColumn;  // 0x1A
    u16 sheetRow;     // 0x1C
    u16 sheetWidth;   // 0x1E
    u16 sheetHeight;  // 0x20
    u16 widthTable;   // 0x22
    u32 sheetImage;   // 0x24
    u32 sheetFullSize;  // 0x28
};

static inline void SetU16(u16& d, u16 v) { d = v; }
// Message slot address as an expression (not an inline call): the multiply lands in the same pseudo
// as the sum, which is what the original codegen shows.
#define MES(no) ((Message*) ((no) * sizeof(Message) + (u32) this + sizeof(u32)))
static inline void PtrSet(void*& d, void* v) { d = v; }

// Font file: offsets to the TPL and to the width table.
struct MesFontFile {
    u32 tplOfs;    // 0x00
    u32 widthOfs;  // 0x04
};

u32 mes_col_tbl[10] = {
    0xE5D9CFF0, 0x87CFA5FF, 0xCD7D5FFF, 0x87AFFFFF, 0xA55FFFFF,
    0x707070FF, 0x707070FF, 0x52DF73FF, 0x00000000, 0x00000000,
};

MessageControl cMes;
MessageFont MesFont[4];
MessageData MesData;
static MesQue MsgQueue[3][0x100];

u16 getCharCode(u16 code)
{
    return code - 0x80;
}

int isCtrlCode(u16 code)
{
    return code < 0x80;
}

RomFont::RomFont(void* font)
{
    Mtx44 proj;
    Mtx m;

    C_MTXOrtho(proj, 0.0f, 408.0f, 0.0f, 544.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXSetZMode(1, 7, 1);
    GXSetNumChans(0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 3);
    GXSetTevOrder(0, 0, 0, 0xFF);
    GXSetBlendMode(1, 1, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 3, 0);
    pFont = font;
}

#define FONT_HDR ((OSFontHeader*) pFont)

void RomFont::setup(void* image)
{
    Mtx m;
    GXTexObj tex;

    GXInitTexObj(&tex, image, FONT_HDR->sheetWidth, FONT_HDR->sheetHeight, FONT_HDR->sheetFormat, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    PSMTXScale(m, 1.0f / (f32) (int) FONT_HDR->sheetWidth, 1.0f / (f32) (int) FONT_HDR->sheetHeight, 1.0f);
    GXLoadTexMtxImm(m, 0x1E, 1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x1E, 0, 0x7D);
}

#define WGFIFO_S16(v) (GXWGFifo->s16 = (v))

void RomFont::draw(int x, int y, int cx, int cy)
{
    s16 x0 = x;
    s16 y0 = y;
    s16 u0 = cx;
    s16 v0 = cy;
    s16 x1 = x0 + ((OSFontHeader*) pFont)->cellWidth;
    s16 y1 = y0 + ((OSFontHeader*) pFont)->cellHeight;
    s16 u1 = cx + ((OSFontHeader*) pFont)->cellWidth;
    s16 v1 = cy + ((OSFontHeader*) pFont)->cellHeight;

    GXBegin(0x80, 0, 4);
    WGFIFO_S16(x0);
    WGFIFO_S16(y0);
    WGFIFO_S16(0);
    WGFIFO_S16(u0);
    WGFIFO_S16(v0);
    WGFIFO_S16(x1);
    WGFIFO_S16(y0);
    WGFIFO_S16(0);
    WGFIFO_S16(u1);
    WGFIFO_S16(v0);
    WGFIFO_S16(x1);
    WGFIFO_S16(y1);
    WGFIFO_S16(0);
    WGFIFO_S16(u1);
    WGFIFO_S16(v1);
    WGFIFO_S16(x0);
    WGFIFO_S16(y1);
    WGFIFO_S16(0);
    WGFIFO_S16(u0);
    WGFIFO_S16(v1);
}

s16 MessageFont::getSize(s16 code, s8* left, s8* right)
{
    int idx = code * 2;

    if (code == 0) {
        idx = 2;
    }
    *left = pWidth[idx];
    *right = pWidth[idx + 1];
    return *right - *left;
}

void MessageFont::create(int w, int h, TEXPalette* tpl, u8* width)
{
    u32 i;
    TEXDescriptor* d;
    MesFontTex* t;

    pTpl = tpl;
    if ((s32) tpl->descriptorArray >= 0) {
        tpl->descriptorArray = (TEXDescriptor*) ((u32) tpl->descriptorArray + (u32) tpl);
        d = tpl->descriptorArray;
        for (i = 0; i < tpl->numDescriptors; i++, d++) {
            d->textureHeader = (TEXHeader*) ((u8*) tpl + (u32) d->textureHeader);
            d->CLUTHeader = (CLUTHeader*) ((u8*) tpl + (u32) d->CLUTHeader);
            if (d->textureHeader->unpacked == 0) {
                d->textureHeader->data = (u8*) tpl + (u32) d->textureHeader->data;
                d->textureHeader->unpacked = 1;
            }
            if (d->CLUTHeader->unpacked == 0) {
                d->CLUTHeader->data = (u8*) tpl + (u32) d->CLUTHeader->data;
                d->CLUTHeader->unpacked = 1;
            }
        }
    }
    d = pTpl->descriptorArray;
    t = tex;
    for (i = 0; i < pTpl->numDescriptors; i++, d++, t++) {
        TEXHeader* th = d->textureHeader;
        t->pTpl = tpl;
        t->pTex = th;
        if (d->textureHeader->format - 8 <= 1) {
            if (d->textureHeader->unpacked) {
                GXInitTexObjCI(&t->tex, th->data, th->width, th->height, th->format, 0, 0, 0, 0);
            }
            if (d->CLUTHeader->unpacked == 1) {
                CLUTHeader* c = d->CLUTHeader;
                GXInitTlutObj(&t->tlut, c->data, c->format, c->numEntries);
            }
        } else {
            GXInitTexObj(&t->tex, th->data, th->width, th->height, th->format, 0, 0, 0);
        }
        PSMTXScale(t->mtx, 1.0f, 1.0f, 1.0f);
    }
    pWidth = width;
    flags = 1;
    t = tex;
    cellW = w;
    cellH = h;
    texW = t->pTex->width;
    texH = t->pTex->height;
}

void MessageFont::destroy()
{
    calcTplOffset(pTpl);
    flags = 0;
}

// Message table: u32 header, then per language an offset to a block of message offsets.
struct MesTblBlock {
    u32 x0;
    u32 count;    // 0x04
    u32 ofs[1];   // 0x08
};

u16* MessageData::getAddr(int no, int type)
{
    u32* tbl = (u32*) ptr[type];
    MesTblBlock* blk = (MesTblBlock*) ((u8*) tbl + tbl[lang + 1]);

    if (no > (int) blk->count - 1) {
        return NULL;
    }
    return (u16*) ((u8*) blk + blk->ofs[no]);
}

int MessageData::getMesNum(int type)
{
    u32* tbl = (u32*) ptr[type];
    MesTblBlock* blk = (MesTblBlock*) ((u8*) tbl + tbl[lang + 1]);

    return blk->count;
}

int MessageData::getSpaceWidth()
{
    if (lang == 0) {
        return 13;
    }
    return 8;
}

void MessageControl::setLayout(int no, int layout)
{
    static s8 layout_tbl[2][9][6] = {
        {
            { 0x19, 0x1B, 0x00, 0x01, 0x00, 0x1B },
            { 0x1A, 0x1C, 0x00, 0x01, 0x00, 0x16 },
            { 0x16, 0x18, 0x00, 0x01, 0x00, 0x18 },
            { 0x16, 0x16, 0x00, 0x00, 0x00, 0x16 },
            { 0x18, 0x15, 0x00, 0x00, 0x00, 0x19 },
            { 0x17, 0x17, 0x00, 0x00, 0x00, 0x1C },
            { 0x11, 0x13, 0x00, 0x00, 0x00, 0x00 },
            { 0x16, 0x17, 0x00, 0x01, 0x00, 0x16 },
            { 0x16, 0x15, 0x00, 0x01, 0x00, 0x13 },
        },
        {
            { 0x16, 0x20, -1, -1, 0x00, 0x19 },
            { 0x17, 0x20, 0x00, 0x00, 0x00, 0x19 },
            { 0x12, 0x18, -1, -1, 0x00, 0x13 },
            { 0x15, 0x19, 0x00, 0x00, 0x00, 0x14 },
            { 0x15, 0x1C, 0x00, 0x00, 0x00, 0x19 },
            { 0x17, 0x17, 0x00, 0x00, 0x00, 0x1C },
            { 0x10, 0x15, -1, -1, 0x00, 0x00 },
            { 0x14, 0x19, -1, -1, 0x00, 0x15 },
            { 0x14, 0x19, -1, -1, 0x00, 0x15 },
        },
    };
    static s8* p_layout;

    p_layout = layout_tbl[MesData.lang][layout];
    setFontSize(no, p_layout[0], p_layout[1]);
    SetU16(MES(no)->charSpace, p_layout[3]);
    SetU16(MES(no)->lineH, p_layout[5]);
}

void MessageControl::setLanguage(int lang)
{
    switch (lang) {
    case 0:
        MesData.lang = lang;
        break;
    case 1:
        MesData.lang = lang;
        break;
    case 3:
        MesData.lang = 2;
        break;
    case 4:
        MesData.lang = 3;
        break;
    case 5:
        MesData.lang = 4;
        break;
    case 6:
        MesData.lang = 5;
        break;
    case 2:
        MesData.lang = 1;
        break;
    default:
        pLog->err(0, 0, "MesCtrl::setLanguage() Invalid LANG_TYPE");
        MesData.lang = 1;
        break;
    }
}

void MessageControl::setupFont(int w, int h, TEXPalette* tpl, int no)
{
    MesFontFile* f = (MesFontFile*) tpl;

    fontBuf[no] = tpl;
    MesFont[no].create(w, h, (TEXPalette*) ((u8*) f + f->tplOfs), (u8*) f + f->widthOfs);
}

void MessageControl::releaseFont(int no)
{
    MesFont[no].destroy();
}

#line 703 "D:/Bio4/Prog/mes.cpp"
int MessageControl::loadFont(int w, int h, const char* name, int no)
{
    int req = DvdReadN(name, fontBuf[no], 0, 0, 0, 0x11, __FILE__, __LINE__);

    if (Dvd.ReadCheck(req, 0, 0, 0) != 1) {
        pLog->err(0, 0, "MesCtrl::fontLoad() Font load failed");
        return 0;
    }
    setupFont(w, h, (TEXPalette*) fontBuf[no], no);
    return 1;
}

void MessageControl::init()
{
    u32 size;
    u32 sz = 0;
    int i;
    Message* m;

    if (Dvd.FileExistCheck("Font/common_j.fnt", &size) != -1) {
        sz = size;
    }
    if (sz != 0) {
#line 734 "D:/Bio4/Prog/mes.cpp"
        fontBuf[0] = mem_alloc(sz, __FILE__, __LINE__, 1, 0xD);
    } else {
        pLog->err(0, 0, "MesCtrl::init() Font file not found.");
    }
    if (Dvd.FileExistCheck("Font/system_j.fnt", &size) != -1) {
        sz = size;
    }
    if (sz != 0) {
#line 748 "D:/Bio4/Prog/mes.cpp"
        fontBuf[1] = mem_alloc(sz, __FILE__, __LINE__, 1, 0xD);
    } else {
        pLog->err(0, 0, "MesCtrl::init() Font file not found.");
    }
    loadCommonFont();
    loadSystemFont();
    setLanguage(pSys->language);
    x11F8 = 0;
    state = 0;
    m = mes;
    for (i = 0; i < 16; m++, i++) {
        if (i <= 2) {
            m->qbase = MsgQueue[i];
        } else {
            m->qbase = NULL;
        }
    }
}

void MessageControl::gameInit()
{
    MesData.setPtr(0, (u8*) (pG->pArc->ofs_28 + (u32) pG->pArc));
    MesData.setPtr(1, (u8*) (pG->pArc->ofs_28 + (u32) pG->pArc));
    MesData.setPtr(2, (u8*) (pG->pArc->ofs_28 + (u32) pG->pArc));
    MesData.setPtr(3, (u8*) (pG->pArc->ofs_54 + (u32) pG->pArc));
    pG->x1C = 1;
    loadCommonFont();
    setLanguage(pSys->language);
    x11F8 = 0;
    state = 0;
}

void MessageControl::roomInit()
{
    int i;

    for (i = 0; i < 16; i++) {
        Delete(i);
    }
    MesData.setPtr(0, (u8*) (pG->pArc->ofs_28 + (u32) pG->pArc));
    MesData.setPtr(1, (u8*) pG->pRoomMes);
    setLayout(0, 0);
    if (checkState(1)) {
        loadStageFont();
    }
}

void MessageControl::loadCommonFont()
{
    if (pSys->language == 0) {
        loadFont(0x1C, 0x1C, "Font/common_j.fnt", 0);
    } else {
        loadFont(0x20, 0x20, "Font/common_p.fnt", 0);
    }
}

void MessageControl::loadSystemFont()
{
    if (pSys->language == 0) {
        loadFont(0x14, 0x14, "Font/system_j.fnt", 1);
    } else {
        setupFont(0x20, 0x20, (TEXPalette*) fontBuf[0], 1);
    }
}

void MessageControl::stageInit()
{
    char name[0x100];
    u32 size;
    u32 sz = 0;

    if (pG->stage_no != 0) {
        sprintf(name, "Font/stage%1d_j.fnt", pG->stage_no);
        if (Dvd.FileExistCheck(name, &size) != -1 && size > sz) {
            sz = size;
        }
        sprintf(name, "Font/event%1d_j.fnt", pG->stage_no);
        if (Dvd.FileExistCheck(name, &size) != -1 && size > sz) {
            sz = size;
        }
    } else {
        if (Dvd.FileExistCheck("Font/stage1_j.fnt", &size) != -1 && size > sz) {
            sz = size;
        }
    }
    if (sz != 0) {
#line 874 "D:/Bio4/Prog/mes.cpp"
        PtrSet(pG->pStageFont, mem_alloc(sz, __FILE__, __LINE__, 1, 0xD));
        fontBuf[2] = pG->pStageFont;
    } else {
        pLog->err(0, 0, "MesCtrl::init() Font file not found.");
    }
    loadStageFont();
}

void MessageControl::loadStageFont()
{
    char name[0x100];

    if (pSys->language == 0 && pG->stage_no != 0) {
        sprintf(name, "Font/stage%1d_j.fnt", pG->stage_no);
        loadFont(0x1C, 0x1C, name, 2);
        unsetState(1);
    }
}

void MessageControl::loadEventFont()
{
    char name[0x100];

    if (pSys->language == 0 && pG->stage_no != 0) {
        sprintf(name, "Font/event%1d_j.fnt", pG->stage_no);
        loadFont(0x1C, 0x1C, name, 2);
        setState(1);
    }
}

void MessageControl::setState(u32 b)
{
    state |= b;
}

void MessageControl::unsetState(u32 b)
{
    state &= ~b;
}

int MessageControl::checkState(u32 b)
{
    return (state & b) ? 1 : 0;
}

void MessageControl::Move()
{
    Message* m = &mes[15];
    Message* p = &mes[0];
    int act = 0;
    int i;

    if (m->flags & 1) {
        act = 1;
    }
    if (act) {
        m->move();
    } else {
        for (i = 0; i < 16; i++, p++) {
            int a = 0;
            if (p->flags & 1) {
                a = 1;
            }
            if (a) {
                p->move();
            }
        }
    }
}

void MessageControl::Trans()
{
    Message* p = &mes[0];
    Message* m;
    int act;
    int i;

    if (pG->flags_58 & 0x800) {
        return;
    }
    m = &mes[15];
    act = 0;
    if (m->flags & 1) {
        act = 1;
    }
    if (act) {
        m->trans();
    } else {
        for (i = 0; i < 16; i++, p++) {
            int a = 0;
            if (p->flags & 1) {
                a = 1;
            }
            if (a) {
                p->trans();
            }
        }
    }
}

void MessageControl::setFontSize(int no, s8 w, s8 h)
{
    Message* m = getMes(no);
    m->fontW = w;
    m->fontH = h;
}

void MessageControl::MesSet(int no, int x, int y, u32 attr, int slot, int col, int type)
{
    MessageFont* font;
    Message* m;
    int ok;

    if (pSys->language == 0) {
        if (type == 4) {
            if (attr & 1) {
                font = &MesFont[0];
            } else if (attr & 2) {
                font = &MesFont[2];
            } else if (attr & 4) {
                font = &MesFont[3];
            } else {
                font = &MesFont[0];
            }
        } else {
            font = &MesFont[type];
        }
    } else {
        font = &MesFont[0];
    }
    if (slot == 15) {
        font = &MesFont[1];
    }
    ok = 0;
    if (font->flags & 1) {
        ok = 1;
    }
    if (!ok) {
        pLog->err(0, 0, "MesSet(): Font not found", 0);
        return;
    }
    m = &mes[slot];
    m->init(no, x, y, attr, col, font);
    m->scaleX = (f32) m->fontW / (f32) font->cellW;
    m->scaleY = (f32) m->fontH / (f32) font->cellH;
    if (!(attr & 0x80)) {
        BitSet(m->saveStop, pG->flags_170);
        if (!(attr & 0x10)) {
            BitSet(pG->flags_170, 0xFFFFFFFF);
            BitOff(pG->flags_170, 0x40);
            KeyStop(0xEFCF0000);
        }
    }
}

void MessageControl::Delete(int no)
{
    if (no > 15) {
        return;
    }
    MES(no)->clrActive();
    mes[no].flags2 &= ~1;
}

void MessageControl::WaitEnd(int no)
{
    if (no > 15) {
        return;
    }
    mes[no].WaitEnd();
}

void Message::init(int no, int x, int y, u32 attr, int col, MessageFont* fnt)
{
    int i;

    font = fnt;
    flags |= 3;
    flags2 = (flags2 & ~2) | 1;
    xB = 0;
    xA = 0;
    x9 = 0;
    x8 = 0;
    cursor = 0;
    result = 0;
    selNum = 0;
    for (i = 15; i >= 0; i--) {
        lineX[i] = x;
    }
    this->x = x;
    this->y = y;
    baseY = y;
    color = mes_col_tbl[col];
    qp = qbase;
    this->attr = attr;
    waitEnd = 0;
    waitCnt = 0;
    result = 0;
    savePtr = NULL;
    saveFont = NULL;
    jumpCnt = 0;
    scaleX = 1.0f;
    jumpIdx = 0;
    x71 = -1;
    scaleY = 1.0f;
    if (attr & 0x40) {
        speed = 0;
    } else {
        speed = 1;
    }
    speedCnt = 0;
    skip = 0;
    if (attr & 1) {
        pMsg = MesData.getAddr(no, 0);
    } else if (attr & 2) {
        pMsg = MesData.getAddr(no, 1);
    } else if (attr & 4) {
        pMsg = MesData.getAddr(no, 2);
    } else if (attr & 8) {
        pMsg = MesData.getAddr(no, 3);
    } else if (attr & 0x10) {
        pMsg = MesData.getAddr(no, 4);
    }
    if (pMsg == NULL) {
        pMsg = MesData.getAddr(0, 0);
        pLog->err(0, 0, "Message::init() Msg[%02d] Address Error", no);
    }
    ot = 0x15;
    otNo = 1;
}

void Message::move()
{
    int ret;
    int code;

    if (!(flags & 2) && (attr & 0x80)) {
        flags &= ~1;
        flags2 &= ~1;
    }
    flags &= ~2;
    if (Key.trg & 0xC0000000) {
        skip = 1;
    }
    for (;;) {
        if (isCtrlCode(*pMsg)) {
            ret = CommandExec();
            if (ret == 2 && *pMsg != 0xA) {
                break;
            }
        } else {
            if (qp != NULL && qp >= qbase + 0x100) {
                pLog->err(0, 0, "Message [%d]: Overflow!", 0);
                return;
            }
            if (!(attr & 0xC0) && skip == 0) {
                if (speedCnt++ < speed) {
                    return;
                }
            }
            code = getCharCode(*pMsg);
            asm("" : : "r"(code));  // COMPILER-DIFF: candidate (combine: the original keeps the call-result copy and `cmpwi` apart, ours fuses them into `mr.`)
            if (code == 0) {
                x += (s16) ((f32) MesData.getSpaceWidth() * scaleX);
            } else {
                QueSet(code, NULL);
            }
            speedCnt = 0;
        }
        pMsg++;
    }
}

void Message::WidthCk()
{
    int n = 0;
    int i;
    s8 l, r;
    u16* save;
    u16 code;

    qp = qbase;
    y = baseY;
    flags2 |= 8;
    save = pMsg;
    maxW = 0;
    numW = 0;
    for (i = 15; i >= 0; i--) {
        lineW[i] = 0;
    }
    while (flags2 & 8) {
        if (isCtrlCode(*pMsg)) {
            switch (*pMsg) {
            case 3:
                n++;
                break;
            case 1:
            case 4:
                if ((s16) lineW[n] > 0) {
                    n++;
                }
                flags2 &= ~8;
                break;
            case 2:
                CommandExec();
                break;
            case 7:
                CommandExec();
                speed = speedSave;
                break;
            case 0xA:
                if (CommandExec() != 0) {
                    lineW[n] += numW;
                }
                break;
            case 0xE:
                if (CommandExec() != 2) {
                    break;
                }
                flags2 &= ~8;
                // falls through into case 0xF (the original has no break here)
            case 0xF:
                CommandExec();
                break;
            case 0x10:
                CommandExec();
                break;
            case 0x11:
                CommandExec();
                break;
            default:
                pMsg += CommandArg();
                break;
            }
            if (n > 15) {
                pLog->err(0, 0, "Message [%d]: line overflow!!", 0);
            }
        } else {
            if (qp != NULL && qp >= qbase + 0x100) {
                pLog->err(0, 0, "Message [%d]: queue overflow!!", 0);
                break;
            }
            code = getCharCode(*pMsg);
            asm("" : "+r"(code));  // COMPILER-DIFF: candidate (combine: the original keeps the call-result copy and `cmpwi` apart, ours fuses them into `mr.`)
            if (code != 0) {
                s16 w = font->getSize(code, &l, &r);
                int a = (s16) ((f32) w * scaleX) + charSpace;
                lineW[n] += a;
            } else {
                lineW[n] += (s16) ((f32) MesData.getSpaceWidth() * scaleX);
            }
        }
        pMsg++;
    }
    maxW = 0;
    for (i = 0; i < n; i++) {
        if ((s16) lineW[i] > maxW) {
            maxW = lineW[i];
        }
    }
    if (!(attr & 0x20000)) {
        for (i = 0; i < n; i++) {
            if (attr & 0x80000) {
                lineX[i] = (0x200 - (s16) lineW[i]) >> 1;
            } else if (attr & 0x40000) {
                int w = 0x200 - lineW[i];
                lineX[i] = w - (0x200 - maxW) / 2;
            } else {
                lineX[i] = (0x200 - maxW) >> 1;
            }
        }
    } else if (attr & 0x40000) {
        for (i = 0; i < n; i++) {
            lineX[i] -= lineW[i];
        }
    }
    if (pSys->language == 0 && (attr & 0x1000) && n == 1) {
        y += lineH;
    }
    if (attr & 0x10000) {
        y = (0x180 - n * (s16) lineH) >> 1;
    }
    pMsg = save;
    qp = qbase;
    jumpIdx = 0;
}

void Message::QueSet(int code, MessageFont* fnt)
{
    s8 l, r;
    s16 w, h;

    if (fnt == NULL) {
        fnt = font;
    }
    w = (s16) ((f32) fnt->getSize(code, &l, &r) * scaleX);
    h = (s16) ((f32) (int) fnt->cellH * scaleY);
    if (!(flags2 & 8)) {
        if (qp != NULL) {
            qp->x = x;
            qp->y = y;
            qp->color = color;
            qp->code = code;
            qp->w = w;
            qp->h = h;
            qp->font = fnt;
            qp++;
        } else {
            MesQue q;
            q.x = x;
            q.y = y;
            q.color = color;
            q.code = code;
            q.h = h;
            q.font = fnt;
            q.w = w;
            messageTrans(&q);
        }
    }
    x += w + (s16) charSpace;
}

void Message::setNumber(u32 num, u16 digits)
{
    s16 i;
    int d;
    int t;

    numberSave = num;
    number = num;
    if (digits == 0 && num != 0) {
        do {
            num /= 10;
            digits++;
        } while (num != 0);
    }
    digit = 1;
    i = 1;
    if (i < digits) {
        d = 1;
        do {
            t = (u16) d * 10;
            d = t;
            i++;
        } while (i < digits);
        digit = t;
    }
    digitSave = digit;
}

void Message::putSelCursol()
{
    int i;

    for (i = 0; i < selNum; i++) {
        selCur[i]->code = (cursor == i);
    }
}

void Message::putNextCursol(int reset)
{
    if (reset == 0) {
        cursorAnim = 0x14;
    }
    cursorAnim++;
    if (cursorAnim > 0x1E) {
        cursorAnim = 1;
    }
}

void Message::setJump(u16 pos)
{
    if (jumpCnt <= 2) {
        jumpTbl[jumpCnt] = pos;
        jumpCnt++;
    } else {
        pLog->warn(0, 0, "JumpTbl is Max!");
    }
}

void setAttribute(MesFontTex* t)
{
    GXSetCullMode(0);
    GXSetZMode(0, 3, 1);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetTevOp(0, 0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetChanCtrl(4, 0, 1, 1, 0, 0, 2);
    if (t->pTex->format - 8 <= 1) {
        GXLoadTlut(&t->tlut, 0);
    }
    GXLoadTexObj(&t->tex, 0);
    GXLoadTexMtxImm(t->mtx, 0x1E, 1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x1E, 0, 0x7D);
    GXSetBlendMode(1, 4, 5, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 0, 3, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
}

void draw(MesQue* q)
{
    MessageFont* font = q->font;
    s8 l, r;
    GXColor fog;
    f32 texH = (f32) font->texH;
    f32 texW = (f32) font->texW;
    u32 col = q->color;
    u8 ca = col & 0xFF;
    u8 cb = (col >> 8) & 0xFF;
    u8 cg = (col >> 16) & 0xFF;
    u8 cr = col >> 24;
    s16 x = q->x;
    s16 y = q->y;
    u8 w = q->w;
    u8 h = q->h;
    MesFontTex* t = &font->tex[0];
    s16 cw;
    int cols, rows;
    s16 u, v;
    s16 x1, y1;
    u8 cellW, cellH;

    font->getSize(q->code, &l, &r);
    cellW = font->cellW;
    cols = font->texW / cellW;
    cw = r - l;
    cellH = font->cellH;
    rows = font->texH / cellH;
    u = (q->code % cols) * cellW;
    v = (q->code / cols) * cellH;
    while (v >= rows * cellW) {
        v -= rows * cellH;
    }
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    setAttribute(t);
    GXBegin(0x80, 0, 4);
    u += l;
    x1 = x + w;
    y1 = y + h;
    GXWGFifo->s16 = x;
    GXWGFifo->s16 = y;
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2f32((f32) u / texW, (f32) v / texH);
    GXWGFifo->s16 = x1;
    GXWGFifo->s16 = y;
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2f32((f32) (u + cw) / texW, (f32) v / texH);
    GXWGFifo->s16 = x1;
    GXWGFifo->s16 = y1;
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2f32((f32) (u + cw) / texW, (f32) (v + cellH) / texH);
    GXWGFifo->s16 = x;
    GXWGFifo->s16 = y1;
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2f32((f32) u / texW, (f32) (v + cellH) / texH);
    LightMgr.setFog();
}

void messageCamera()
{
    Mtx44 proj;
    Mtx m;

    C_MTXOrtho(proj, 0.0f, 384.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
}

void messageTrans(MesQue* q)
{
    messageCamera();
    draw(q);
}

void Message::trans()
{
    MesQue* q;

    for (q = qbase; q < qp; q++) {
        if (attr & 0x20) {
            AddOtDirect(ot, q, (void (*)()) messageTrans, otNo, 0x1000, NULL, 0.0f);
        } else {
            messageTrans(q);
        }
    }
}

int Message::CommandExec()
{
    switch (*pMsg) {
    case 0x00:
        return code00();
    case 0x01:
        return code01();
    case 0x02:
        return code02();
    case 0x03:
        return code03();
    case 0x04:
        return code04();
    case 0x05:
        return code05();
    case 0x06:
        return code06();
    case 0x07:
        return code07();
    case 0x08:
        return code08();
    case 0x09:
        return code09();
    case 0x0A:
        return code0a();
    case 0x0B:
        return code0b();
    case 0x0C:
        return code0c();
    case 0x0D:
        return code0d();
    case 0x0E:
        return code0e();
    case 0x0F:
        return code0f();
    case 0x10:
        return code10();
    case 0x11:
        return code11();
    case 0x12:
        return code12();
    }
    return 3;
}

int Message::CommandArg()
{
    static const s16 arg[19] = { 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1 };

    return arg[*pMsg];
}

void Message::WaitEnd()
{
    waitEnd = 1;
}

int Message::code00()
{
    if (qbase != NULL) {
        qp = qbase;
        memclr_asm(qbase, 0x1000);
    } else {
        qp = NULL;
    }
    WidthCk();
    x = lineX[0];
    line = 0;
    speedCnt = 0;
    waitEnd = 0;
    selNum = 0;
    speedSave = 0;
    x70 = 0;
    cursorAnim = 0;
    skip = 0;
    return 0;
}

int Message::code01()
{
    switch (x8) {
    case 0:
        x8++;
        break;
    case 1:
        flags2 |= 2;
        if (!(attr & 0x01000000)) {
            flags2 &= ~1;
            flags &= ~1;
            if (!(attr & 0x10)) {
                pG->flags_170 = saveStop;
            }
        }
        if (IdSys.setCk(0x21) && !(pG->flags_500C & 0x00040000)) {
            Cckpt.lifeMeterDisp(1);
        }
        break;
    }
    return 2;
}

int Message::code02()
{
    u16 no;

    pMsg++;
    no = *pMsg;
    if (no == 0xFFFF) {
        no = jumpTbl[jumpIdx++];
        if (no == 0xFFFF) {
            return 0;
        }
    }
    savePtr = pMsg;
    saveFont = font;
    if (attr & 1) {
        pMsg = MesData.getAddr(no, 0);
    } else if (attr & 2) {
        pMsg = MesData.getAddr(no, 1);
    } else if (attr & 4) {
        pMsg = MesData.getAddr(no, 2);
    } else if (attr & 8) {
        pMsg = MesData.getAddr(no, 3);
    }
    return 0;
}

int Message::code03()
{
    y += lineH;
    line++;
    x = lineX[line];
    return 0;
}

int Message::code04()
{
    if (attr & 0x80) {
        return 0;
    }
    pMsg++;
    code00();
    return 2;
}

int Message::code05()
{
    if (attr & 0x80) {
        return 0;
    }
    pMsg++;
    speed = *pMsg;
    speedCnt = 0;
    return 0;
}

int Message::code06()
{
    pMsg++;
    color = mes_col_tbl[*pMsg];
    return 0;
}

int Message::code07()
{
    if (speedSave == 0) {
        speedSave = speed;
    }
    speed = 0;
    selCur[selNum++] = qp;
    QueSet(0, NULL);
    return 0;
}

int Message::code08()
{
    int ret = 2;

    if (attr & 0x02000000) {
        return 0;
    }
    if (x70 == 0) {
        if (attr & 0x00400000) {
            cursor = selNum - 1;
            putSelCursol();
        }
        x70 = 1;
        return 2;
    }
    if (waitCnt != 0) {
        waitCnt--;
        return 2;
    }
    if (selNum != 0) {
        if (Key.trg & 0x80000000) {
            ret = 0;
            result = cursor + 1;
            if (result == 1) {
                if (attr & 0x100) {
                    SndCall(0, 0xF, NULL, 0, 0, NULL);
                }
                if (attr & 0x200) {
                    SndCall(0, 0x11, NULL, 0, 0, NULL);
                }
                if (attr & 0x400) {
                    SndCall(0, 0x13, NULL, 0, 0, NULL);
                }
            } else if (result == 3) {
                if (attr & 0x400) {
                    SndCall(0, 0x12, NULL, 0, 0, NULL);
                }
            }
        } else if (Key.trg & 0x40000000) {
            if (attr & 0x00100000) {
                if (attr & 0x00200000) {
                    result = selNum;
                    ret = 0;
                } else {
                    result = -1;
                    ret = 0;
                }
            } else if (attr & 0x00200000) {
                cursor = selNum - 1;
            }
        } else if (attr & 0x00800000) {
            s8 old = cursor;
            if (Key.trg & 0x01000000) {
                cursor--;
                if (cursor < 0) {
                    cursor = selNum - 1;
                }
            } else if (Key.trg & 0x02000000) {
                cursor++;
                if (cursor >= selNum) {
                    cursor = 0;
                }
            }
            if (old != cursor) {
                if (attr & 0x100) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else if (attr & 0x200) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else if (attr & 0x400) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else {
                    SndCall(0, 0xA, NULL, 0, 0, NULL);
                }
            }
        } else {
            s8 old = cursor;
            if (Key.trg & 0x08000000) {
                cursor--;
                if (cursor < 0) {
                    cursor = selNum - 1;
                }
            } else if (Key.trg & 0x04000000) {
                cursor++;
                if (cursor >= selNum) {
                    cursor = 0;
                }
            }
            if (old != cursor) {
                if (attr & 0x100) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else if (attr & 0x200) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else if (attr & 0x400) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else {
                    SndCall(0, 0xA, NULL, 0, 0, NULL);
                }
            }
        }
        putSelCursol();
    } else {
        putNextCursol(1);
        if (Key.trg & 0xC0000000) {
            ret = 0;
            putNextCursol(0);
        }
    }
    return ret;
}

int Message::code09()
{
    int ret = 2;

    if (attr & 0x80) {
        return 0;
    }
    if (waitEnd == 0) {
        waitEnd = pMsg[1];
    } else if (waitEnd == -1) {
        return 2;
    } else {
        waitEnd--;
        if (waitEnd == 0) {
            pMsg++;
            ret = 0;
        }
    }
    return ret;
}

int Message::code0a()
{
    u32 d;
    u16 code;
    MessageFont* fnt;
    s8 l, r;
    s16 w;
    int a;

    if (qp >= qbase + 0x100) {
        return 2;
    }
    d = number / digit;
    if (d > 9) {
        d = 0;
    }
    if (MesData.lang == 0) {
        if (attr & 0x10000000) {
            code = d + 0xE;
        } else {
            code = d + 3;
        }
        if (font == &MesFont[1]) {
            fnt = &MesFont[1];
        } else {
            fnt = &MesFont[0];
        }
    } else {
        code = d + 3;
        fnt = font;
    }
    w = fnt->getSize(code, &l, &r);
    a = (s16) ((f32) w * scaleX) + charSpace;
    numW += a;
    QueSet(code, fnt);
    number -= d * digit;
    digit /= 10;
    if (digit == 0) {
        number = numberSave;
        digit = digitSave;
        if (attr & 0x10000000) {
            if (MesData.lang == 0) {
                code = 0xD;
            } else {
                code = 0xAC;
            }
            w = fnt->getSize(code, &l, &r);
            a = (s16) ((f32) w * scaleX) + charSpace;
            numW += a;
            QueSet(code, fnt);
        }
        return 2;
    }
    pMsg--;
    return 0;
}

int Message::code0b()
{
    pMsg++;
    x = *pMsg;
    return 0;
}

int Message::code0c()
{
    pMsg++;
    y = *pMsg;
    return 0;
}

int Message::code0d()
{
    pMsg++;
    x71 = *pMsg;
    return 0;
}

int Message::code0e()
{
    if (savePtr != NULL) {
        pMsg = savePtr;
        font = saveFont;
        savePtr = NULL;
        saveFont = NULL;
        return 0;
    }
    return 2;
}

int Message::code0f()
{
    u16 no;

    pMsg++;
    no = *pMsg;
    if (no != 0xFFFF || (no = jumpTbl[jumpIdx++]) != 0xFFFF) {
        savePtr = pMsg;
        saveFont = font;
        pMsg = MesData.getAddr(no, 0);
        font = &MesFont[0];
    }
    return 0;
}

int Message::code10()
{
    savePtr = pMsg;
    saveFont = font;
    pMsg = MesData.getAddr(x1A, 3);
    return 0;
}

int Message::code11()
{
    u16 no;

    pMsg++;
    no = *pMsg;
    if (no == 0xFFFF) {
        no = jumpTbl[jumpIdx++];
        if (no == 0xFFFF) {
            return 0;
        }
    }
    savePtr = pMsg;
    saveFont = font;
    pMsg = MesData.getAddr(no, 3);
    if (MesData.lang == 0) {
        font = &MesFont[0];
    }
    return 0;
}

int Message::code12()
{
    pMsg++;
    xE4 = *pMsg;
    return 0;
}
