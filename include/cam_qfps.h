#ifndef CAM_QFPS_H
#define CAM_QFPS_H

#include "types.h"
#include "vec.h"
#include "camera.h"

struct CameraAreaRec;

// Shoulder camera offsets in player space (0x2C bytes), one per [left/right][up/mid/down] site.
// g_readyOfs[16]/g_transOfs[7] are the per-area tables (game/cam_qfps.cpp); db_cam edits a copy.
struct QfpsOfs {
    Vec campos;    // 0x00
    Vec campos2;   // 0x0C  close point
    Vec target;    // 0x18
    f32 x24;       // 0x24  roll
    f32 fovy;      // 0x28
};

extern QfpsOfs g_readyOfs[16][2][3];
extern QfpsOfs g_transOfs[7][2][3];

// Over-the-shoulder ("quasi FPS") camera, game/cam_qfps.cpp. 0x214 bytes.
class CameraQuasiFPS {
public:
    Camera cam;                   // 0x000 (cam.param at 0xA4 is what CameraControl::Move copies)
    QfpsOfs (*ready_tbl[14])[3];  // 0x0F8  ready table per camera type (checkCameraType 0..0xC), [13] = area copy
    QfpsOfs (*trans_tbl[6])[3];   // 0x130  transition table per type (0..5)
    QfpsOfs (*blend_src)[3];      // 0x148  current ready table
    QfpsOfs (*blend_dst)[3];      // 0x14C  current transition table
    QfpsOfs* cur;                 // 0x150  offsets of the current site
    QfpsOfs* old;                 // 0x154  offsets blended from (g_readyOfs[15] / g_transOfs[6] copies)
    void* lr_info;                // 0x158
    Vec pos_ofs;                  // 0x15C  one-shot translation applied to the base matrix
    Vec dir_ofs;                  // 0x168  one-shot look direction applied to the base matrix
    Mtx pl_mat;                   // 0x174  player matrix saved by setPlayerLocation
    Vec* pl_nrm;                  // 0x1A4  player floor normal (cModel::pFloorNrm)
    f32 x1A8;                     // 0x1A8
    f32 smooth_ratio;             // 0x1AC  CamSmth.ratio while the player moves
    u8 trans_type;                // 0x1B0
    u8 ready_type;                // 0x1B1
    u8 reset;                     // 0x1B2  1 = first frame after init
    u8 pad_1B3[0x1E4 - 0x1B3];
    f32 blend_ratio;              // 0x1E4
    s32 blend_count;              // 0x1E8
    s32 blend_timer;              // 0x1EC
    Vec shoulder_aim;             // 0x1F0
    u8 site;                      // 0x1FC  0 right/up-mid-down, 1 left, 2 right far, 3 left far (db_cam)
    u8 pad_1FD[3];
    f32 angle_y;                  // 0x200
    f32 angle_x;                  // 0x204
    f32 floor_ratio;              // 0x208
    s16 search_frame;             // 0x20C
    s16 search_count;             // 0x20E
    u32 flags;                    // 0x210  bit0 use pl_mat, bit2 blending, bit3 blend frozen

    void LRinfo(void* p);
    int LRcheck();
    void calcDepressionRatio();
    void setPlayerLocation(Mtx m, Vec* nrm);
    void calcBaseMatrix(Mtx m);
    int checkFBLR();
    void setBlendRatio(f32 r);
    void setBlendCount(int n);
    f32 getFloorRatio();
    void setFloorRatio(f32 ratio);
    void checkCameraType();
    void calcOffset(QfpsOfs* out);
    void hitCheck(Mtx m, QfpsOfs* ofs, CameraParam* out);
    void setBlendData(void* src, void* dst);
    void getAreaData(QfpsOfs (*ready)[3], QfpsOfs (*trans)[3]);
    void setAreaData(QfpsOfs (*ready)[3], QfpsOfs (*trans)[3]);
    void setAreaData(struct CameraCut* cut);
    void offsetCorrection();
    void bindDefaultCamera();
    void bindAreaCamera(CameraAreaRec* rec);
    void init();
    void move();
};

#endif
