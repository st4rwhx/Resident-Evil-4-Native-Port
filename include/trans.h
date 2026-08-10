#ifndef TRANS_H
#define TRANS_H

#include "types.h"
#include "vec.h"
#include "gx.h"
#include "model.h"

// game/trans.cpp: the model renderer (ordering table registration, skinning, material / TEV setup,
// display list submission).

// Per-model texture swap table (cModel+0x31C): `num` pairs of (destination, source) texture ids.
class cTexChg {
public:
    u8 num;      // 0x00
    u8 tbl[1];   // 0x01  pairs (dst, src)

    void move(GXTexObj* texObj);
};

// The texture animation / blend block of a cModelInfo (0xDC..0xFC), which the renderer reads
// through a pointer (`ModelTexInfo* t = MODEL_TEX(info)`).
struct ModelTexInfo {
    u16 flags;        // 0x00  = cModelInfo::flagsDC
    u16 blendRatio;   // 0x02
    u8 frame;         // 0x04  texture animation frame
    u8 blendType;     // 0x05
    u8 pad_6[2];
    u8* blendTbl;     // 0x08  = texBlendTbl
    f32 u;            // 0x0C  uv scroll offset
    f32 v;            // 0x10
    f32 su;           // 0x14  uv scroll speed
    f32 sv;           // 0x18
    u8* anim;         // 0x1C  texture animation table ([1] frames, [4 + frame] texture id)
};
#define MODEL_TEX(info) ((ModelTexInfo*) &(info)->flagsDC)

// C++ linkage
void lightSetEm(cModel* m);
int commonScreenMat(cModel* m);
void ModelRender(cModel* m);

extern "C" {
void org_LoadTexObj(u32 id, int map);
void Trans();
void lightSetObj(cModel* m);
void emTrans(cModel* m);
void objTrans(cModel* m);
void ModelTrans(cModel* model);
int commonScreenMatSub(cModel* m, cModelInfo* info);
void calcWeightMat(cModel* m);
void Render();
void commonModelTrans(cModel* m, cModelInfo* info, Mtx viewMat, int flag);
void SetPrimBuffPtr();
void* GetPrimBuff(int size);
void shaderReset();
void setupGQR6(u32 v);
void CalcTplAddrC8(struct TEXPalette* tpl);
void SpecularInit(struct TEXPalette* spec, struct TEXPalette* ind, struct TEXPalette* ind2, struct TEXPalette* thermo);
void GlobalIlmTexInit(struct TEXPalette* tpl);
void GetEfbTex(cModel* m);
void ClearZbuf();
}

extern GXTexObj g_Get_tex_obj;   // EFB copy the refraction shader samples (esp_sub/id_sys/esp18 reuse it)
// trans.cpp also owns specular_mat, Specular[9] (esp.h declares it as a scalar), GlobalIlmTex[5],
// IndTex[2] and ThermoTlut; declare them extern locally where needed.

#endif
