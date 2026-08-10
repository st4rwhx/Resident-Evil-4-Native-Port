#ifndef EXAMINE_H
#define EXAMINE_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "model.h"
#include "id_sys.h"
#include "light.h"

// Item examine view (game/examine.cpp): shows an item model in front of the item camera.

// Per-item display parameters (exam_info / exam_info_ext, 0x20 bytes).
struct ExamInfo {
    u16 id;     // 0x00  item id
    u16 x2;     // 0x02
    Vec rot;    // 0x04  initial rotation (degrees)
    f32 scale;  // 0x10  camera distance divisor
    s32 light;  // 0x14  light cut selected from pG->pArc (0..4)
    s32 rot0;   // 0x18  rotation axis for mode 0 (0 world Y, 1 model Y)
    s32 rot1;   // 0x1C  rotation axis for mode 1
};

class ItemExamine {
public:
    IDSystem* pIdSys;    // 0x00  IdSys (mode 0) / IdSub (mode 1, 2)
    u32 saveFlag;        // 0x04  model->be_flag at init
    Vec savePos;         // 0x08
    Vec saveRot;         // 0x14
    u8 saveX12F;         // 0x20
    u8 mode;             // 0x21  0 in game, 1 sub screen, 2 puzzle
    s8 lv[4];            // 0x22  weapon tune levels (power, speed, reload, bullet)
    u8 pad_26[2];
    cCoord* saveParent;  // 0x28  model->pParts->pParent at init
    Vec savePartsPos;    // 0x2C
    Vec savePartsRot;    // 0x38
    u16 id;              // 0x44  item id
    u8 pad_46[2];
    cModel* model;       // 0x48
    ExamInfo* info;      // 0x4C
    cLight* light[3];    // 0x50

    void setup();
    void idSet();
    void init(u16 id, cModel* model, u8 mode);
    void level(s8 a, s8 b, s8 c, s8 d);
    void move();
    void trans();
    void quit();
    void reset();
};

extern ItemExamine itemExam;
extern Camera itemCamera;
extern ExamInfo exam_info_ext[2];
extern f32 cap_dist_min;
extern f32 cap_dist_max;
extern f32 cap_xrad_max;

#endif
