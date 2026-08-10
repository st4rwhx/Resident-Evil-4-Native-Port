#include "light.h"
#include "atari.h"
#include "id_sys.h"
#include "texture.h"
#include "main_mem.h"
#include "db_log.h"
#include "gx.h"

cTexSys* g_pIdTexSys;

void IdTexGameInit()
{
    cTexSys* sys;

#line 46 "D:/Bio4/Prog/id_tex.cpp"
    sys = (cTexSys*) MEM_ALLOC(sizeof(cTexSys), 1, 13);
    g_pIdTexSys = sys;
    sys->Init("IdTex", 0x200);
    IdTexRoomInit();
}

void IdTexRoomInit()
{
    g_pIdTexSys->Clear();
}

void IdTexRelease(int id)
{
    g_pIdTexSys->TexRelease(id);
}

// Effect texture pack: version 0xB, id table at 0x04, TPL table at 0x18, animation table at 0x1C.
struct IdTexData {
    u32 version;  // 0x00  == 0xB
    u32 ofsId;    // 0x04  -> TexIdTbl
    u8 pad_8[0x18 - 0x8];
    u32 ofsTpl;   // 0x18  -> TexOfsTbl of TPLs
    u32 ofsAnm;   // 0x1C  -> TexOfsTbl of TexAnms
};

int IdTexDataLoad(void* data, int id)
{
    u32 addr = (u32) data;
    IdTexData* d;
    TexIdTbl* idTbl;
    TexOfsTbl* tplTbl;
    TexOfsTbl* anmTbl;
    u32 i;

    // The table offsets live in r9/r11 (BASE_REGS): the base is an integer, not the pointer parameter.
    // The empty asm keeps cse from folding `addr` back into `data`'s pointer-flagged pseudo.
    asm("" : "+r"(addr));
    d = (IdTexData*) addr;
    if (d->version != 0xB) {
        pLog->err(0, 0, "IdDataLoad():EffData [0x%x] Invalid.", addr);
        return 0;
    }
    idTbl = (TexIdTbl*) (addr + d->ofsId);
    tplTbl = (TexOfsTbl*) (addr + d->ofsTpl);
    anmTbl = (TexOfsTbl*) (addr + d->ofsAnm);
    for (i = 0; i < idTbl->num; i++) {
        TEXPalette* tpl = (TEXPalette*) ((u8*) tplTbl + tplTbl->ofs[i]);
        TexAnm* anm = (TexAnm*) ((u8*) anmTbl + anmTbl->ofs[i]);
        u8 texId = idTbl->ent[i].id;
        int check = 1;

        if (texId == 0x80) {
            check = 0;
        }
        g_pIdTexSys->TexRegist(tpl, anm, texId, id, 0, check);
    }
    return 1;
}

static inline int getTexObj(u8 id, u16 no, GXTexObj** t)
{
    return g_pIdTexSys->GetTexObj(id, no, t);
}

void IdTexSet(u8 id, u8 no)
{
    Mtx m;
    GXTexObj* tex;
    GXTlutObj* tlut;
    // The original re-extends `no` for the u8 parameter (`clrlwi r5, r4, 24`, narrow-argument compiler
    // difference); the empty asm hides the incoming promotion from combine (emobj setYarare).
    int n = no;

    asm("" : "+r"(n));
    if (g_pIdTexSys->GetTexObj(id, (u8) n, &tex) == 0) {
        pLog->err(0, 0, "IdTexSet: TexId[%x] no data", id);
        return;
    }
    GXLoadTexObj(tex, 0);
    if (g_pIdTexSys->GetTlutObj(id, &tlut) != 0) {
        GXLoadTlut(tlut, 0);
    }
    GXSetZMode(0, 3, 0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetNumTexGens(1);
    PSMTXIdentity(m);
    GXLoadTexMtxImm(m, 0x1E, 1);
    GXSetTexCoordGen(0, 1, 4, 0x1E);
}

int IdGetAnmAddr(u8 id, TexAnm** out)
{
    return g_pIdTexSys->GetAnmAddr(id, out);
}

void IdChannelSet(IdUnit* u)
{
    GXColor c;

    GXSetTevOp(0, 0);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
    c.r = (u8) u->col[0];
    c.g = (u8) u->col[1];
    c.b = (u8) u->col[2];
    c.a = (u8) u->col[3];
    GXSetChanMatColor(4, c);
}

TexWk* IdGetTexWk(u8 id, int quiet)
{
    return g_pIdTexSys->GetTexWk(id, quiet);
}
