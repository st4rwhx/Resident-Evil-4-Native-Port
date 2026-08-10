#ifndef SOFDEC_H
#define SOFDEC_H

#include "types.h"
#include "db_log.h"
#include "gx.h"
#include "vec.h"
#include "mwply.h"
#include "cString.h"

#line 8 "D:/Bio4/Prog/sofdec.h"

// Movie texture: the decoded frame either as Y8 + UV (IA8) planes (mode 0) or one RGBA8 texture
// (mode 1). The two layouts share the storage after width/height.
struct SofdecTex {
    int width;  // 0x00  (the u16 half at +2 is what GX gets)
    int height; // 0x04
    union {
        struct {
            GXTexObj texY;  // 0x08
            GXTexObj texUV; // 0x28
            void* bufY;     // 0x48
            u32 sizeY;      // 0x4C
            void* bufUV;    // 0x50
            u32 sizeUV;     // 0x54
        } yuv;
        struct {
            GXTexObj tex; // 0x08
            void* buf;    // 0x28
            u32 size;     // 0x2C
        } argb;
    };
}; // 0x58

// Render state: the model matrix and the movie texture.
struct SofdecDraw {
    Mtx mtx;         // 0x00
    u8 pad_30[0x40]; // 0x30
    SofdecTex tex;   // 0x70
}; // 0xC8

// Player state around the MWPLY handle.
struct SofdecApp {
    MWPLY hn;               // 0x00
    u8 pad_4[0x20];         // 0x04
    MWS_PLY_CPRM_SFD cprm;  // 0x24
    u8 pad_48[0xC];         // 0x48
    int stat;               // 0x54
    MWS_FRM frm;            // 0x58
    void* work;             // 0xE0
    int xE4;                // 0xE4
    int disp;               // 0xE8  1: draw the debug frame info
    char fname[0x44];       // 0xEC
}; // 0x130

// Sofdec movie player front end (game/sofdec.cpp, `Sofdec`, 0x240 bytes). The inline range check
// emits the file-name string into the .rodata of every unit that includes it.
class cSofdec {
public:
    u32 flag;         // 0x00  bit0: a movie is playing, bit2: paused, bit5: skipped, bit8: keep black
    u32 x04;          // 0x04
    SofdecApp app;    // 0x08
    SofdecDraw drw;   // 0x138
    s16 width;        // 0x200
    s16 height;       // 0x202
    int fadeIn;       // 0x204
    u32 save170;      // 0x208
    u32 save58;       // 0x20C
    u32 heapStart;    // 0x210
    u8 heapNo;        // 0x214
    s8 vcnt;          // 0x215
    u16 fno;          // 0x216
    int resized;      // 0x218
    int mode;         // 0x21C
    char path[0x20];  // 0x220

    // playing check: `if (Sofdec.flag & 1) return 1; return 0;` form (li 0 / li 1)
    int isPlay() {
        if (flag & 1) {
            return 1;
        }
        return 0;
    }
    u8* getData(u32 no) {
        if (no >= flag) {
            dbgAssert(__FILE__, __LINE__);
        }
        return (u8*) &x04 + no;
    }
    int chkFlag(u32 bit) {
        return (flag & bit) ? 1 : 0;
    }

    void drawTex();
    void drawQuad(SofdecDraw* d);
    void drawPolygon(SofdecDraw* d);
    void setCamera(SofdecDraw* d);
    void loadMvFrmFx(MWPLY hn, MWS_FRM* frm);
    void allocTexMem(SofdecTex* tex, int w, int h);
    void clrTexMem(SofdecTex* tex);
    void initDraw(SofdecDraw* d);
    void initApp(const char* fname);
    int startApp();
    void initSync();
    int appMain();
    void draw();
    void finishMovie();
    int initWork(const char* fname);
    int Initialize(const char* fname, u32 flags);
    int Initialize(cString& fname, u32 flags);
    int initSub(const char* fname, u32 flags);
    int Move();
    static void ThreadMove(cSofdec* s);
    void PlayPause(int pause);
    ~cSofdec() {}
};

extern cSofdec Sofdec;

extern "C" {
void ADXM_ExecMain();
void SofdecInit();
void UsrSfcnt2time(int tscale, int count, int* h, int* m, int* s, int* f);
void disp_info(SofdecApp* app);
void setTevPrm(int mapY, int mapUV);
void restoreTevPrm();
void ap_mwply_err_func(void* obj, const char* msg);
}

#endif
