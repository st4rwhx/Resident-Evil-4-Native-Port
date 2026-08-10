#ifndef TEXTURE_H
#define TEXTURE_H

#include "types.h"
#include "vec.h"
#include "gx.h"
#include "tpl.h"

// Texture animation data; only the texture count is used here.
struct TexAnm {
    u8 pad_0[8];
    u16 numTex;  // 0x08
};

// Texture data file fed to cTexSys::DataLoad (version 3): three offset tables.
struct TexData {
    u32 version;  // 0x00  == 3
    u32 ofsId;    // 0x04  -> TexIdTbl
    u32 ofsTpl;   // 0x08  -> TexOfsTbl of TPLs
    u32 ofsAnm;   // 0x0C  -> TexOfsTbl of TexAnms
};

struct TexIdEnt {
    u16 id;   // 0x00
    u16 x2;
    u32 x4;
};

struct TexIdTbl {
    u32 num;          // 0x00
    TexIdEnt ent[1];  // 0x04
};

struct TexOfsTbl {
    u32 num;     // 0x00
    u32 ofs[1];  // 0x04  relative to the table
};

// One registered texture set (0x54 bytes).
struct TexWk {
    GXTexObj* pTexObj;   // 0x00  first of nTex objects pulled from cTexSys::pTexObj
    u16 nTex;            // 0x04
    u8 pad_6[2];
    GXTlutObj tlut;      // 0x08
    TEXHeader* texHdr;   // 0x14  header of texture 0
    Mtx mtx;             // 0x18
    TEXPalette* pTpl;    // 0x48
    TexAnm* pAnm;        // 0x4C
    u32 owner;           // 0x50  0 = free
};

// game/texture.cpp
class cTexSys {
public:
    TexWk wk[256];       // 0x0000
    GXTexObj* pTexObj;   // 0x5400  nTexObj objects
    u8* pFlag;           // 0x5404  in-use bits
    u32 x5408;           // 0x5408
    u32 nTexObj;         // 0x540C
    const char* name;    // 0x5410

    void Init(const char* name, u32 num);
    void Clear();
    int GetTexObjFlag(u32 no);
    void SetTexObjFlag(u32 no, int flag);
    int DataLoad(TexData* data, u32 owner, int clamp);
    GXTexObj* PullTexObj(u32 num);
    void CalcTplAddr(TEXPalette* tpl);
    int TexRegist(TEXPalette* tpl, TexAnm* anm, u8 id, u32 owner, int clamp, int check);
    int GetTplAddr(u32 id, TEXPalette** out);
    int GetTexObj(u32 id, u32 no, GXTexObj** out);
    int GetAnmAddr(u32 id, TexAnm** out);
    int GetTlutObj(u32 id, GXTlutObj** out);
    TexWk* GetTexWk(u32 id, int quiet);
    int TexRelease(u32 owner);
};

extern int lod_enable;

#endif
