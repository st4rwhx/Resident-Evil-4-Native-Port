#ifndef TEXRENDER_H
#define TEXRENDER_H

#include "types.h"
#include "gx.h"
#include "camera.h"
#include "cam_motion.h"

class cModel;

// Render-to-texture manager (game/TexRender.cpp): up to 8 EFB copies per frame.
class TexRenderMng {
public:
    int used;        // 0x00
    GXTexObj texObj; // 0x04
    void* buf;       // 0x24  sx * sy * 4 bytes
    u8 texId;        // 0x28  0xF8 + slot
    u8 x29;
    u16 mask;        // 0x2A  8 << slot
    u32 sx;          // 0x2C  texture size (EFB copy is 2x)
    u32 sy;          // 0x30
    int repType;     // 0x34  0 mirror, 1 repeat, 2 clamp

    TexRenderMng();
    void Init();
    int AllocBuf();
    void ReAllocBuf();
};                   // 0x38

// Event data block TexRenderCamAddOt is handed (only the fields CamRenderPrev reads).
struct TexRenderEvt {
    u8 pad_0[0x44];
    u32 flags;       // 0x44  bit30: use frameB, bit27: use frameEnd - 1
    u8 pad_48[0x98 - 0x48];
    int frame;       // 0x98
    int frameEnd;    // 0x9C
    u8 pad_A0[0xE0 - 0xA0];
    int frameB;      // 0xE0
};

// Camera used while rendering to texture: a CameraMotion built in place, plus the saved main camera.
struct TexRenderCam {
    CameraMotion cam;    // 0x000
    u8 pad_1D4[0x200 - 0x1D4];
    cCamera* pCam;       // 0x200  &cam
    Camera save;         // 0x204  pG->Cam while the render camera is active
    TexRenderEvt* pEvt;  // 0x2FC
    void* data;          // 0x300  motion data for CameraMotion
};

extern TexRenderMng g_RndMgr[8];
extern u32 g_RndMgrNum;
extern int g_TexUse;

extern "C" {
TexRenderMng* GetTexRenderMgrAddr(int no);
void TexRenderMgrInit();
void TexRenderMgrRoomInit();
int GetTexRenderMgr(TexRenderMng** out);
void RenderTexRenderMgr(TexRenderMng* m);
void CopyTexRenderMgr(TexRenderMng* m);
void TransTexRenderMgr();
void TexRenderInit(TexRenderMng** out, int size, int repType);
void TexRenderModSet(cModel* m, int parts, u8* tbl, TexRenderMng* mgr, int keepBlendType, int keepRefrect, int keepD6, int keep12C, f32 alpha);
void TexRenderModRes(cModel* m);
void TexRenderModAddOt(int ot, cModel* m);
void TexRenderModAddOtMirror(int ot, cModel* m);
void TexRenderCamAddOt(int ot, TexRenderCam* c, TexRenderEvt* evt, void* data);
void CamRenderPrev(TexRenderCam* c);
void CamRenderAfter(TexRenderCam* c);
}

#endif
