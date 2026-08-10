#include "types.h"
#include "vec.h"
#include "gx.h"
#include "global.h"
#include "main_mem.h"
#include "dvd.h"
#include "tpl.h"
#include "va_ppc.h"
#include "eprintf.h"

extern "C" {
void OSReport(const char* fmt, ...);
int vsprintf(char* buf, const char* fmt, va_list ap);
}

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

// System work (game/main.cpp `pSys`); only the flag word is known.
struct SystemWork {
    u32 flags;  // 0x00  bit 30 = progressive/60Hz screen scaling
};
extern SystemWork* pSys;

// Current text environment
struct MojiWork {
    u16 x;      // 0x00
    u16 y;      // 0x02
    int color;  // 0x04
    int x8;     // 0x08
    u8 cur_no;  // 0x0C
    u8 page;    // 0x0D  debug page the text belongs to (0 = every page)
    u8 pad_E[2];
};

// Buffered message: x, y as little-endian byte pairs, color, font w/h, then the string.
#define MESS_KEEP_SIZE 0x4000
#define MESS_PTR_NUM 0x300

MojiWork Moji;
GXTexObj fontTexObj;
static Mtx fontTMtx;
int eprintf_init = 0;
static char* mess_ptr_buff;  // MESS_PTR_NUM message pointers
#define MESS_PTR(i) (((char**) mess_ptr_buff)[i])
static char* mess_keep_buffer;
static char* mess_keep_ptr;

u32 color_data[] = {
    0xB0B0B0FF, 0x0000FFFF, 0xFF0000FF, 0xFF00FFFF, 0x00FF00FF, 0x00FFFFFF, 0xFFFF00FF, 0x808080FF, 0x808080FF,
    0x000080FF, 0x800000FF, 0x800080FF, 0x008000FF, 0x008080FF, 0x808000FF, 0x404040FF, 0xA0A0A0FF, 0x686868FF,
    0x00C000FF, 0x005800FF, 0x585858FF, 0x303030FF, 0xFF9600FF, 0x905100FF, 0x60A0A0FF,
};

extern "C" {
int EprintfSetCurrentNo();
void EprintfSetEnv(int x, int y, int color, int page, int a);
void eprintf_main(int w, int h, const char* fmt, va_list ap);
int Sp_char_ck(int c);
void EprintfBufferClear();
void EprintfBuffering(int w, int h, char* str);
void font_draw(char* str, int color, int y, int x, int z, int w, int h);
void EprintfDrawing();
void EprintfFlush();
void EprintfInit();
}

int BtoX(int bits)
{
    int i;
    int x = 0;
    for (i = 7; i >= 0; i--) {
        x <<= 4;
        x += (bits >> i) & 1;
    }
    return x;
}

int EprintfSetCurrentNo()
{
    return Moji.cur_no;
}

void eprintf(int x, int y, int color, int a, const char* fmt, ...)
{
    va_list ap;
    if (eprintf_init) {
        va_start(ap, fmt);
        EprintfSetEnv(x, y, color, a, 0);
        eprintf_main(8, 14, fmt, ap);
        va_end(ap);
    }
}

void eprintf2(int w, int h, int x, int y, int color, int a, const char* fmt, ...)
{
    va_list ap;
    if (eprintf_init) {
        va_start(ap, fmt);
        EprintfSetEnv(x, y, color, a, 0);
        eprintf_main(w, h, fmt, ap);
        va_end(ap);
    }
}

void EprintfSetEnv(int x, int y, int color, int page, int a)
{
    Moji.x = x;
    Moji.y = y;
    Moji.page = page;
    Moji.x8 = a;
    Moji.color = color;
}

void eprintf_main(int w, int h, const char* fmt, va_list ap)
{
    char buf[512];
    int size;

    if (Moji.page == pG->debug_mode || Moji.page == 0) {
        if (*fmt != 0) {
            size = vsprintf(buf, fmt, ap) + 1;
            if (size > 0x1FF) {
#line 220 "D:/Bio4/Prog/eprintf.cpp"
                HALT();
            }
            EprintfBuffering(w, h, buf);
        }
    }
}

int Sp_char_ck(int c)
{
    switch (c) {
    case 1:
        return 0x7D;
    case 2:
        return 0x60;
    case 3:
        return 0x7C;
    case 4:
        return 0x7B;
    }
    return c;
}

void EprintfBufferClear()
{
    int i;
    for (i = 0; i < MESS_PTR_NUM; i++) {
        *(u32*) (mess_ptr_buff + i * 4) = 0;
    }
    mess_keep_ptr = mess_keep_buffer;
}

void EprintfBuffering(int w, int h, char* str)
{
    char* dst = NULL;
    int i;

    // mess_keep_ptr is re-read inside the loop and the body is guarded by one `if`: the single early
    // return lets cse follow the taken jump, so gcse PRE (not cse) merges the two loads and leaves the
    // `lwz r0; mr r7,r0; cmplw r0` copy.
    if (mess_keep_ptr >= mess_keep_buffer + MESS_KEEP_SIZE - 0x40) {
        return;
    }
    for (i = 0; i < MESS_PTR_NUM; i++) {
        if (MESS_PTR(i) == NULL) {
            dst = mess_keep_ptr;
            MESS_PTR(i) = dst;
            break;
        }
    }
    if (dst != NULL) {
        dst[0] = Moji.x & 0xFF;
        dst[1] = Moji.x >> 8;
        dst[2] = Moji.y & 0xFF;
        dst[3] = Moji.y >> 8;
        dst[4] = Moji.color;
        dst[5] = w;
        dst[6] = h;
        dst += 7;
        while (*str != 0) {
            *dst++ = *str++;
        }
        *dst = 0;
        mess_keep_ptr = dst + 1;
    }
}

void font_draw(char* str, int color, int y, int x, int z, int w, int h)
{
    int c;
    u32 col;
    int v;
    int u;
    u8 a;
    u8 r;
    u8 b;
    u8 g;

    c = (u8) *str;
    if (c == ' ' || c == 0) {
        return;
    }
    c = Sp_char_ck(c) - 0x20;
    col = color_data[color];
    u = (c & 0x1F) * 8;
    v = ((c >> 5) & 7) * 16;
    a = col & 0xFF;
    b = (col >> 8) & 0xFF;
    g = (col >> 16) & 0xFF;
    r = col >> 24;
    if (r == 0 && g == 0 && b == 0) {
        r = g = b = 0xFF;
    }
    GXBegin(0x80, 0, 4);
    // s16 copies declared after GXBegin: `x + w` adds to the extended value (add r0,r5,r22)
    s16 sx = x;
    s16 sy = y;
    s16 sz = z;
    GXPosition3s16(sx, sy, sz);
    GXColor4u8(r, g, b, a);
    GXTexCoord2s16(u, v);
    GXPosition3s16(sx + w, sy, sz);
    GXColor4u8(r, g, b, a);
    GXTexCoord2s16(u + 8, v);
    GXPosition3s16(sx + w, sy + h, sz);
    GXColor4u8(r, g, b, a);
    GXTexCoord2s16(u + 8, v + 16);
    GXPosition3s16(sx, sy + h, sz);
    GXColor4u8(r, g, b, a);
    GXTexCoord2s16(u, v + 16);
}

void EprintfDrawing()
{
    Mtx44 proj;
    Mtx m;
    Mtx inv;
    Mtx t;
    int i;

    if (!eprintf_init) {
        return;
    }
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    PSMTXInverse(m, inv);
    PSMTXTranspose(inv, t);
    GXLoadNrmMtxImm(t, 0);
    GXSetCullMode(2);
    GXSetZMode(1, 3, 1);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    if (pG->debug_mode == 0) {
        return;
    }
    GXSetTevOp(0, 0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetChanCtrl(4, 0, 1, 1, 0, 0, 2);
    GXSetNumTexGens(1);
    GXLoadTexObj(&fontTexObj, 0);
    GXLoadTexMtxImm(fontTMtx, 30, 1);
    GXSetTexCoordGen(0, 1, 4, 30);
    GXSetBlendMode(1, 4, 5, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXSetVtxAttrFmt(0, 13, 1, 3, 8);
    // The loop test reads the table through a cast (not MEM_IN_STRUCT_P), the body through MESS_PTR:
    // gcse then keeps both `lwz mess_ptr_buff` loads per iteration like the original.
    for (i = 0; i < MESS_PTR_NUM && *(u32*) (mess_ptr_buff + i * 4) != 0; i++) {
        u8* p = (u8*) MESS_PTR(i);
        s16 x = p[0]; // `x |= hi << 8` keeps the low byte in the variable's register
        x |= p[1] << 8;
        s16 y = p[2];
        y |= p[3] << 8;
        int color = p[4];
        int w = p[5];
        s16 h = p[6];
        s16 x0 = x;
        p += 7;
        if (pSys->flags & 0x40000000) {
            h = (f32) h / 1.33333333f;
            y = (f32) y / 1.33333333f + 56.0f;
        }
        while (*p != 0) {
            if (*p == '\n') {
                y += h;
                x = x0;
            } else {
                font_draw((char*) p, color, y, x, 0, w, h);
                x += w;
            }
            p++;
        }
    }
}

void EprintfFlush()
{
    EprintfDrawing();
    EprintfBufferClear();
}

void EprintfInit()
{
    eprintf_init = 0;
    char name[] = "etc/moji8.tpl";
    void* addr;
    int req;
    TEXHeader* img;
#line 832 "D:/Bio4/Prog/eprintf.cpp"
    req = DvdReadN(name, NULL, 0, 0, 0, 3, __FILE__, __LINE__);
    if (Dvd.ReadCheck(req, NULL, NULL, &addr) >= 0) {
        img = (TEXHeader*) ((u8*) addr + 0x14);
        GXInitTexObj(&fontTexObj, (void*) ((u32) img->data + (u32) addr), img->width, img->height, img->format, img->wrapS,
                     img->wrapT, 0);
        PSMTXScale(fontTMtx, 1.0f, 256.0f / img->height, 1.0f);
        mess_keep_buffer = (char*) Debug_alloc(MESS_KEEP_SIZE, 1);
        mess_ptr_buff = (char*) Debug_alloc(MESS_PTR_NUM * 4, 1);
        eprintf_init = 1;
    }
}

asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
