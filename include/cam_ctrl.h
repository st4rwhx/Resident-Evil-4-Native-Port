#ifndef CAM_CTRL_H
#define CAM_CTRL_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "cam_qfps.h"

class cCamera;
class cModel;
struct CameraCut;

// ---------------------------------------------------------------------------
// Room camera data ("B40x" file). Layout after the 0x10 header:
//   CameraAreaRec[numArea]   0x10 each
//   CameraAreaInfo[numArea]  0x30 each
//   CameraCut[numCut]        0x34 each
//   CameraLerp[numLerp]      0x10 each
// File offsets are relocated to pointers by CameraControl::calcAddr.
// ---------------------------------------------------------------------------

struct CameraAreaInfo {  // hit area
    u8 enable;    // 0x00
    s8 area_no;   // 0x01
    s8 camera_no; // 0x02
    u8 attr;      // 0x03  bit 4 = ?, bit 8 = ?, 0x20 set from 8 by calcAddr, 0x40 = check dir, 0x80 = no light update
    f32 dir;      // 0x04  facing angle the player must have (attr & 0x40)
    u8 attr2;     // 0x08  matched against battle/state attribute
    u8 x9;        // 0x09
    u8 pad_A[0x20 - 0x0A];
    f32 height;   // 0x20
    f32 base_y;   // 0x24
    s32 num;      // 0x28  polygon vertex count
    Vec* points;  // 0x2C
};

struct CameraAreaRec {  // area -> cut link
    u8 type;              // 0x00  camera type of the linked cut (t_camera tcTypeTbl)
    u8 pad_1[7];
    CameraAreaInfo* area; // 0x08
    CameraCut* cut;       // 0x0C
};

struct CameraCut {
    u8 x0;          // 0x00
    s8 camera_no;   // 0x01
    s8 type;        // 0x02  CameraControl state selector
    u8 flags;       // 0x03  bit 0: aim_ofs valid
    Vec aim_ofs;    // 0x04  added to the player position to get the aim point
    u16* frames;    // 0x10  key frame times
    f32 floor_ratio; // 0x14  shoulder camera floor ratio (cam_qfps setAreaData)
    u8 pad_18[0x20 - 0x18];
    s32 num;        // 0x20  key count
    Vec* pos;       // 0x24
    Vec* at;        // 0x28
    f32* roll;      // 0x2C
    f32* fovy;      // 0x30
};

struct CameraLerp {
    u8 enable;     // 0x00
    s8 area_from;  // 0x01
    s8 cam_from;   // 0x02
    s8 area_to;    // 0x03
    s8 cam_to;     // 0x04
    u8 pad_5[3];
    s32 frame;     // 0x08
    u8 pad_C[4];
};

struct CameraDataHeader {
    char version[4]; // 0x00  "B400".."B404"
    u8 numCut;       // 0x04
    u8 numArea;      // 0x05
    u8 numLerp;      // 0x06
    u8 pad_7[0x10 - 0x07];
};

// Per-attach-camera record registered by other units (only the frame count is used here).
struct AttachCamera {
    u8 parts[5];    // 0x00  motion parts index feeding each channel (0xFF = none): 0/1 pos, 2/3 rot, 4 misc
    u8 type;        // 0x05  0 = off, 1 = follows the model matrix, 2 = own matrix copy (MotionSetCore)
    u8 frame;       // 0x06  (u8)(out[4].y / 100)
    u8 pad_7;
    Mtx* pMat;      // 0x08  &model->mat or &mat
    Mtx mat;        // 0x0C
    Vec out[5];     // 0x3C  interpolated channels (MotionMoveCore)
    u16 hist[5][3]; // 0x78  key history per channel / axis
};

// B-spline rail work used by the Track/RailPan/RailBehind cameras (static CamBSpline, 0x3B8).
// Parametrize() fits the cut's key positions with de_Boor_Cox basis functions (up to 26 keys),
// searchRail() picks the segment/parameter nearest the aim point, BSpline() evaluates the curve.
struct CameraBSpline {
    s32 k;          // 0x000  spline degree (min(2, num - 1))
    f32 t;          // 0x004  curve parameter
    s32 seg;        // 0x008  key index the parameter was searched from
    s32 num;        // 0x00C  key count
    f32 px[26];     // 0x010  control points
    f32 py[26];     // 0x078
    f32 pz[26];     // 0x0E0
    f32 ax[26];     // 0x148
    f32 ay[26];     // 0x1B0
    f32 az[26];     // 0x218
    f32 roll[26];   // 0x280
    f32 fovy[26];   // 0x2E8
    f32 basis[26];  // 0x350  de_Boor_Cox output

    CameraBSpline() {}  // empty: makes CamBSpline emit at its definition (cam_ctrl .bss order)
};

// ---------------------------------------------------------------------------

class CameraInterpolation {
public:
    CameraParam param; // 0x00
    s32 frame;         // 0x20

    void set(int frame, CameraParam* p);
    void move(CameraParam* p);
};

class CameraSmooth {
public:
    u8 pad_0[0xF8];
    u32 flags;         // 0xF8  bit 0 = reinit on next move
    f32 ratio;         // 0xFC
    CameraParam param; // 0x100
    u8 pad_120[0x12C - 0x120];

    void init(CameraParam* p);
    void move(CameraParam* p);
    CameraParam* getParam() { return &param; }
};

class CameraControl {
public:
    u8 x0;                        // 0x00
    u8 x1;                        // 0x01
    u8 attach_num;                // 0x02
    u8 x3;                        // 0x03
    AttachCamera* attach_cam[3];  // 0x04
    cModel* attach_model[3];      // 0x10
    cModel* attach_cur;           // 0x1C
    f32 scope_param0;             // 0x20
    f32 scope_param1;             // 0x24
    u8 flags_28;                  // 0x28  bit 0 = data valid, bit 2 = disabled
    u8 pad_29[3];
    u32 flags_2C;                 // 0x2C
    u32 flags_30;                 // 0x30
    u8 state;                     // 0x34
    u8 sub_state;                 // 0x35
    u8 x36;                       // 0x36
    u8 prev_state;                // 0x37
    CameraParam cur;              // 0x38
    u32 counter_58;               // 0x58
    CameraDataHeader* data;       // 0x5C
    Camera camera;                // 0x60
    Mtx prev_mat;                 // 0x158  camera matrix CamStick2World keeps while the cut changes
    u8 pad_188[0x250 - 0x188];
    s32 x250;                     // 0x250  nonzero blocks the fall-check in Check()
    CameraInterpolation interp;   // 0x254
    CameraQuasiFPS qfps;          // 0x278
    u8 extra_buf[0x200];          // 0x48C  placement storage for cCamera subclasses
    cCamera* extra;               // 0x68C
    s8 area_no;                   // 0x690
    s8 x691;                      // 0x691
    s8 camera_no;                 // 0x692
    u8 area_attr;                 // 0x693
    CameraAreaRec* area_rec;      // 0x694
    s32 battle_timer;             // 0x698
    Vec aim;                      // 0x69C
    Vec up_pos;                   // 0x6A8
    Vec up_at;                    // 0x6B4
    Vec up_vec;                   // 0x6C0
    f32 x6CC;                     // 0x6CC
    f32 x6D0;                     // 0x6D0
    f32 x6D4;                     // 0x6D4
    f32 x6D8;                     // 0x6D8
    f32 x6DC;                     // 0x6DC
    s32 x6E0;                     // 0x6E0
    f32 x6E4;                     // 0x6E4
    f32 x6E8;                     // 0x6E8
    Vec dbg_pos;                  // 0x6EC
    Vec dbg_at;                   // 0x6F8

    int HermiteExport(CameraCut* cut, u8* buf);
    int IsChangeCamera();
    void Comeback(int);
    void Disable();
    void AreaCheckOnOff(int mode);
    u8 AreaNum();
    int CurrentAreaNo();
    int CurrentCameraNo();
    CameraCut* DataSearch(int no);
    CameraLerp* LerpDataSearch(int area_from, int cam_from, int area_to, int cam_to);
    CameraDataHeader* calcAddr(CameraDataHeader* data);
    void RoomDataRead(CameraDataHeader* room);
    void CoreDataRead(CameraDataHeader* data);
    void AreaOnOff(int area_no, int camera_no, int on);
    void SetAreaAttr(int area_no, int camera_no, u8 attr);
    void UnsetAreaAttr(int area_no, int camera_no, u8 attr);
    void CutCall(int no);
    void switchCamera(CameraAreaRec* rec);
    void areaHitCheck();
    void roomInit();
    void Check();
    void Move();
    void CalcAim(CameraCut* cut);
    f32 getCameraPitch();
    void r0_Wait();
    void r0_Debug();
    void r0_Fix();
    void r0_Pan();
    void r0_Track();
    void r0_RailPan();
    void r0_UpCut();
    void r0_RailBehind();
    void r0_Free();
    void resetCameraAngle();
    f32 getCameraDirection();
    void debugDrawRail(CameraCut* cut);
    void UpCutCall(int no, Vec* pos, Vec* at, Vec* up, int data_sel);
    void startPushObject();
    void endPushObject();
    void StartLookDownEm(void* em);
    void EndLookDownEm();
    void startScope(Vec* pos, Vec* at);
    void endScope();
    void getTrajectory(Vec* pos, Vec* at);
    void saveScopeParam();
    void loadScopeParam();
    void SetBinocularRange(f32 a, f32 b, f32 c, f32 d);
    void HoldBinocular(void* id_a, void* id_b, Vec* pos, Vec* at);
    void LowerBinocular();
    void GetBinocularIDAddr(void** a, void** b);
    void MotionSet(void* motion, int frame, f32 speed);
    int IsMotionSet();
    int IsMotionEnd();
    void setMotionBaseMatPtr(Mtx* mat);
    void* getMotionInfoPtr();
    void clearAttachCamera();
    void registAttachCamera(AttachCamera* cam, cModel* model);
    void deleteAttachCamera(AttachCamera* cam, cModel* model);
    cModel* getAttachModel(cModel* model);
    AttachCamera* getAttachCamera(cModel* model);
    void checkAttachCamera();

    // Empty ctor/dtor: cam_ctrl's `__static_initialization_and_destruction_0` and the
    // `global constructors/destructors keyed to g_pToolCamData` pair.
    CameraControl() {}
    ~CameraControl() {}
};

extern CameraControl CamCtrl;
extern CameraSmooth CamSmth;
extern void* g_pToolCamData;

int cameraDataVersion(char* data);
int cameraHitCheck(Vec* pos, Vec* nrm, Vec* from, Vec* to);
void CameraSetCutData(Camera* cam, CameraCut* cut);
int areaAttr(CameraAreaInfo* area, u8 attr, u8 attr2);
int areaHit(Vec* pos, CameraAreaInfo* area, f32 dir);
int area_hit_p3(Vec* pos, CameraAreaInfo* area);
int area_hit_pN(Vec* pos, CameraAreaInfo* area);
void CamCtrlShoulderSetSearchFrame(s16 frame);
void CamCtrlShoulderSetAim(Vec* aim);
void Parametrize(CameraCut* cut, CameraBSpline* bs);
void BSpline(CameraBSpline* bs, Camera* cam, int mode);
void searchRail(CameraBSpline* bs, CameraCut* cut, Vec* aim, int mode);


#endif
