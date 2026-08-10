#ifndef MOTION_H
#define MOTION_H

// game/motion.cpp: key-frame motion playback for cModel hierarchies (Em, Obj, player, ...).

#include "types.h"
#include "vec.h"
#include "model.h"
#include "cam_ctrl.h"

// MotionSeqKey / MotionData / MotionWork are defined in model.h (cModel::mot at 0x1D8).

// The motion-driven model view (cMotBase::set(cMotModel*), MOTION(m)): cModel carries the work
// itself now, so this adds nothing.
class cMotModel : public cModel {
public:
};

// MotionParts (cParts::motParts, 0x174) is defined in model.h.

// Parts-side IK state (game/ik.cpp), between the bind matrix (0xF8) and MotionParts (0x174).
struct IkParts {
    Mtx bindMat;     // 0xF8  bind pose matrix (PARTS_BIND_MAT)
    f32 len;         // 0x128 bone length to the child parts
    Mtx mat;         // 0x12C orientation of the IK plane (SetOrientationZY transposed)
    Vec axis;        // 0x15C bend axis in parts space
    Vec dir;         // 0x168 bind pose direction from the effector to the root
};

#define MOTION(m) (&((cMotModel*)(m))->mot)
#define MOTION_PARTS(p) ((MotionParts*)((u8*)(p) + 0x174))
#define IK_PARTS(p) ((IkParts*)((u8*)(p) + 0xF8))
#define PARTS_BIND_MAT(p) (*(Mtx*)((u8*)(p) + 0xF8))

// HermiteInterpolation parameter block.
struct HermitePrm {
    f32 frame;     // 0x00
    f32 maxFrame;  // 0x04
    u32 flags;     // 0x08  bit0: search backwards, bit1: reverse, bit2: loop, bit3: ignore the key history
    u8 type;       // 0x0C  Fcc type
    u8 pad_D[3];
    u8* key;       // 0x10
};

extern "C" {
void PartsWorldPosCalc(cModel* m);
void MotionBlendOff(cModel* m);
void MotionPause(cModel* m);
void MotionClear(cModel* m, int flag);
u16 MotionMove(cModel* m);
u16 MotionMoveSub(cModel* m, MotionWork* w);
void MotionMoveCore(cModel* m, MotionWork* w, int flag);
void MotionHokan(cModel* m, MotionWork* w);
void MotionGetSpeed(cModel* m, MotionWork* w, int flag, Vec* pos, Vec* rot);
void MotionAddSpeed(cModel* m, MotionWork* w, Vec* pos, Vec* rot);
void MotionGetPosition(cModel* m, Vec* pos, Vec* rot);
u16 MotionSequenceCtrl(MotionWork* w);
u16 FcvGetMaxFrame(u16* data);
f32 MotionGetMaxFrame(MotionWork* w);
f32 MotionGetCurrentFrame(MotionWork* w);
int MotionCheckCrossFrame(MotionWork* w, f32 frame);
int MotionGetState(cModel* m);
int HermiteInterpolation(HermitePrm* prm, Vec* out, u16* hist);
int Fcc_next_axis_addr(int type, int n);
void IKInit(cModel* m, MotionWork* w);
void InverseKinematics(cModel* m, int flag);
void cModel_matBlend(cModel* m, f32 rate) asm("matBlend__6cModelf");   // cModel::matBlend (model.cpp); the C name is what motion.cpp calls
}
void MotionSetCore(cModel* m, void* w, void* data, int seq, int hokan, int flags, int frame);

#endif
