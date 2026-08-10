#ifndef OBJ_H
#define OBJ_H

#include "types.h"
#include "cManager.h"
#include "model.h"
#include "atariInfo.h"
#include "pendulum.h"
#include "main_mem.h"

// Per-object work layouts (game/obj03.cpp ...), all overlaid at cObj+0x328.
struct Obj03Work {
    u8 x0;        // 0x00
    u8 x1;        // 0x01
    u8 x2;        // 0x02
    u8 x3;        // 0x03
    f32 length;   // 0x04 path length
    f32 t;        // 0x08 current position on the path
    f32 speed;    // 0x0C
    u32 flags;    // 0x10 bit0: debug draw
    void* data;   // 0x14
    void* path;   // 0x18
};

class cObj;

// Effect owner info at the head of every Efm work (esp_efm.cpp copies the caller's EspInfo,
// esp.h, into it; EfmDeleteSub matches flg / kind / pEm against g_Core_*).
struct EfmCore {
    u16 flg;              // 0x00
    u8 kind;              // 0x02
    u8 x3;                // 0x03
    u32 x4;               // 0x04
    cModel* pEm;          // 0x08
};

// Effect model work (game/obj04.cpp `Efm04`): a thrown/falling particle-like model.
struct Efm04Work {
    EfmCore core;         // 0x00
    f32 spdDamp;          // 0x0C  speed *= spdDamp every frame
    Vec acc;              // 0x10  added to speed every frame
    Vec rotSpd;           // 0x1C  added to rot every frame
    f32 scaleXZ;          // 0x28
    f32 scaleY;           // 0x2C
    f32 scale;            // 0x30  scale = scale + scaleSpd, scaleSpd *= scaleDamp
    f32 scaleSpd;         // 0x34
    f32 scaleDamp;        // 0x38
    union {
        struct {
            u8 pad_3C[3];
            u8 alpha0;    // 0x3F  alpha at the end of the fade-in
        };
        struct {
            u8 r0;        // 0x3C  start colour (EspGenWork x9C..x9F)
            u8 g0;        // 0x3D
            u8 b0;        // 0x3E
            u8 a0;        // 0x3F
        };
    };
    f32 r;                // 0x40
    f32 g;                // 0x44
    f32 b;                // 0x48
    f32 a;                // 0x4C
    f32 rMul;             // 0x50  fade-out multipliers
    f32 gMul;             // 0x54
    f32 bMul;             // 0x58
    f32 aMul;             // 0x5C
    u16 fadeStart;        // 0x60
    u16 fadeLen;          // 0x62
    u16 moveStart;        // 0x64
    u16 scaleStart;       // 0x66
    u16 life;             // 0x68  0 = forever
    u16 frame;            // 0x6A
    cModel* parent;       // 0x6C  (esp_efm: the sequence's parent model)
    u32 parentSerial;     // 0x70
    cCoord* parentWorld;  // 0x74  pEffParentWorld when detached
    u8 rotFrame;          // 0x78  frame to re-orient along the parent (0xFF = never)
    u8 x79;
    u8 stopped;           // 0x7A  bit0: came to rest
    u8 x7B;
    u32 flags;            // 0x7C  bit0: floor collision, bit1: scenario collision, bit3: MotionMove
    f32 groundOfs;        // 0x80
    union {
        struct {
            f32 bounceXZ; // 0x84
            f32 bounceY;  // 0x88
            f32 x8C;      // 0x8C
        };
        Vec bounce;       // 0x84  (EfmSetObj04: EspGenWork xE4 * 0.1)
    };
};

// Effect model with loose parts (game/obj05.cpp `Efm05`): the obj04 scale / colour fade with the
// parts burst parameters; each parts keeps its own state in its cModel (efmStat / efmSpd / efmRotSpd).
struct Efm05Work {
    EfmCore core;         // 0x00
    Vec rotSpd;           // 0x0C  added to rot every frame
    f32 scaleXZ;          // 0x18
    f32 scaleY;           // 0x1C
    f32 scale;            // 0x20  scale = scale + scaleSpd, scaleSpd *= scaleDamp
    f32 scaleSpd;         // 0x24
    f32 scaleDamp;        // 0x28
    union {
        struct {
            u8 pad_2C[3];
            u8 alpha0;    // 0x2F  alpha at the end of the fade-in
        };
        struct {
            u8 r0;        // 0x2C  start colour (EspGenWork x9C..x9F)
            u8 g0;        // 0x2D
            u8 b0;        // 0x2E
            u8 a0;        // 0x2F
        };
    };
    f32 r;                // 0x30
    f32 g;                // 0x34
    f32 b;                // 0x38
    f32 a;                // 0x3C
    f32 rMul;             // 0x40  fade-out multipliers
    f32 gMul;             // 0x44
    f32 bMul;             // 0x48
    f32 aMul;             // 0x4C
    u16 fadeStart;        // 0x50
    u16 fadeLen;          // 0x52
    u16 x54;              // 0x54
    u16 scaleStart;       // 0x56
    u16 life;             // 0x58  0 = forever
    u16 frame;            // 0x5A
    u32 flags;            // 0x5C  bit0: floor collision, bit1: scenario collision, bit3: parts tip over
    Vec center;           // 0x60  burst centre relative to pos (Efm05RotMatrix rotates it)
    u8 pow;               // 0x6C  burst speed (* 4096 / range)
    u8 rangeStep;         // 0x6D  burst range growth per frame (0xFF: everything at once)
    u8 rnd;               // 0x6E  speed random (/ 32)
    u8 rotAmp;            // 0x6F  rotation speed random (* 0.005)
    f32 grav;             // 0x70  added to the parts speed y
    f32 spdDamp;          // 0x74  parts speed *= spdDamp
    u32 groundOfs;        // 0x78
    union {
        struct {
            f32 bounceXZ; // 0x7C
            f32 bounceY;  // 0x80
            u8 pad_84[4];
        };
        Vec bounce;       // 0x7C  (EfmSetObj05: EspGenWork xE4 * 0.1)
    };
    u32 seed;             // 0x88  fRandSeed1_1 seed
};

// Rigid body effect model work (game/obj09.cpp, set up by esp_efm EfmSetObj09): a box of
// `size` with mass / moments of inertia, pushed by `spd` (momentum). Extends to cObj+0x3D8.
struct Efm09Work {
    EfmCore core;         // 0x00
    f32 mass;             // 0x0C  size.x * size.y * size.z / 1e9 * mass_mul
    union {
        struct {
            f32 momentX;  // 0x10  moment_mul * mass * (size.y^2 + size.z^2) / 12
            f32 momentY;  // 0x14
            f32 momentZ;  // 0x18
        };
        Vec moment;       // 0x10  (obj09 dwdt)
    };
    u8 pad_1C[4];
    Vec pos;              // 0x20  = basePos at set up
    Vec basePos;          // 0x2C  EspGenWork x0C + random (y + 0.0001)
    Mtx mat;              // 0x38  identity at set up
    Vec spd;              // 0x68  EspGenWork x24 + random, * mass * 100 (obj09: velocity)
    Vec x74;              // 0x74  0 at set up (obj09: world angular velocity, mat * rotSpd)
    Vec size;             // 0x80  EspGenWork xD8..xE0 * 100 + 250
    Vec force;            // 0x8C  force accumulated by AddForce, cleared every CalcVel
    Vec torque;           // 0x98  torque accumulated by AddForce
    Vec rotSpd;           // 0xA4  EspGenWork x70 + random (overlaps cObj x3D0 / callBack): local angular velocity
};

// Obstacle model work (game/obj20.cpp `SetObaModel`).
struct ObaModelWork {
    u8 pad_0[0xC];
    Vec ofs;              // 0x0C  position relative to the parent (parts) matrix
    int partsNo;          // 0x18  parts of the parent followed by type 0
    cObj* parent;         // 0x1C
};

// Fading attachment work (game/obj26.cpp): scales toward `tgtScale`, then shrinks and fades out.
struct Obj26Work {
    u8 pad_0[8];
    cObj* parent;         // 0x08  followed parts 2 of this object
    u8 pad_C[0xC];
    Vec tgtScale;         // 0x18
};

// Bell work (game/obj14.cpp): a hit-receiving enemy plus a pendulum chain for the swing.
struct BellWork {
    u8 pad_0[0xA];
    u16 ringTimer;        // 0x0A  frames the "rung" state is reported to pG (90 after a hit)
    class cEmHit* pEmHit; // 0x0C
    struct PenCloth cloth;  // 0x10 .. 0x70
};

// Floating island work (game/obj1c.cpp): drifts back toward its home position, plays crash
// motions and spawns effects while the player is on it.
struct IslandWork {
    u32 x00;              // 0x00
    u8 pad_4[8];
    int crashTimer;       // 0x0C  frames since setCrashBig (ckCrash)
    int estTimer;         // 0x10  frames until the next idle effect
    int crashEstWait;     // 0x14  frames the crash effect is suppressed
    Vec spd;              // 0x18  push speed (setCrashBig)
    Vec home;             // 0x24  position it drifts back to
    u8 espKind;           // 0x30  effect kind (EspPullCoreKind)
    u8 pad_31[3];
    void* motIdle;        // 0x34  motions: idle / crash, and their big-scale (>= 1.5) variants
    void* motCrash;       // 0x38
    void* motIdleBig;     // 0x3C
    void* motCrashBig;    // 0x40
};

// Thrown / shot object work (game/obj08.cpp): a projectile with gravity, scenario / enemy /
// player hit checks and up to four effect sets.
struct Obj08Work {
    u32 flags;            // 0x00  bit0 start motion, bit1 motion running, bit3 rotate, bit4 enemy hit check, bit5 player hit check
    void* pMot;           // 0x04
    u8 pad_8[2];
    u16 motPrm;           // 0x0A
    Vec rotSpd;           // 0x0C
    Vec spd;              // 0x18
    f32 grav;             // 0x24
    f32 rad;              // 0x28  hit radius (min 1.0)
    cModel* parent;       // 0x2C  thrower (its id goes to SndCall)
    int life;             // 0x30  frames left (-1 = forever)
    void* pAtk;           // 0x34  EmAtkHitCk attack data
    u32 atkFlags;         // 0x38  low 16 bits: GetWepTargetList flag, low byte: damage kind
    u32 estNo[4];         // 0x3C  effects: 0 ?, 1 scenario hit / timeout, 2 floor hit, 3 enemy / player hit
    u32 estPrm[4];        // 0x4C
    u16 seBlk;            // 0x5C  hit SE (0xFFFF = none)
    u16 seNo;             // 0x5E
    u8 estFlag;           // 0x60  1: the enemy-hit effect follows the target instead of the hit point
};

// Hanging object work (game/obj00.cpp): follows a parts of its parent (`oya`) with a slerp
// blend, or falls as a three-point rope (obj00FallMove).
struct Obj00Work {
    u32 flags;            // 0x00  bit2: falling, bit3: blending toward the parent, bit5: fading out
    void* pMot;           // 0x04
    int motA;             // 0x08  MotionSetCore 4th argument
    u32 motPrm;           // 0x0C  low 16 bits: MotionSetCore 6th argument
    cModel* oya;          // 0x10  parent
    int partsNo;          // 0x14  parts of the parent to follow
    f32 rate;             // 0x18  blend rate (1.0 = parent matrix)
    f32 rateSpd;          // 0x1C
    s16 fallSpd[3][3];    // 0x20  rope point speeds * 10
    u8 pad_32[2];
    Mtx mat;              // 0x34  previous parent matrix
    u8 seBlk;             // 0x64  landing SE
    u8 seNo;              // 0x65
    u8 seId;              // 0x66
    u8 sePlayed;          // 0x67
};

// Hanging / thrown object work (game/obj12.cpp `cObj12`): the obj00 layout with a life counter,
// the landing SE moved to 0x68 and a rope `type` selecting the three rope offsets.
struct Obj12Work {
    u32 flags;            // 0x00  bit2: falling, bit3: blending toward the parent, bit7: keep the parent matrix, bit8: thrown, bit9: fading out after `life`
    void* pMot;           // 0x04
    int motResult;        // 0x08  MotionMove result of this frame
    u32 motPrm;           // 0x0C
    cModel* oya;          // 0x10  parent
    int partsNo;          // 0x14
    f32 rate;             // 0x18  blend rate (1.0 = parent matrix)
    f32 rateSpd;          // 0x1C
    s16 fallSpd[3][3];    // 0x20  rope point speeds * 10 (fallSpd[0] is the throw speed)
    u8 pad_32[2];
    Mtx mat;              // 0x34  previous parent matrix
    int life;             // 0x64  frames before the fade out
    u8 seBlk;             // 0x68  landing SE (0xFF = none)
    u8 seNo;              // 0x69
    u8 seId;              // 0x6A
    u8 sePlayed;          // 0x6B
    u8 type;              // 0x6C  rope offsets table index (setFall)
};

// Event costume / cloth model work (game/obj18.cpp): a model that follows a parts of its parent
// (like obj00) and runs one of the cloth simulations by `type`.
struct Obj18Work {
    u32 flags;            // 0x00  bit3: blending toward the parent, bit6: cloth simulation off
    u8 pad_4[0xC];
    cModel* oya;          // 0x10  parent
    int partsNo;          // 0x14
    f32 rate;             // 0x18  blend rate (1.0 = parent matrix)
    f32 rateSpd;          // 0x1C
    u8 pad_20[0x14];
    Mtx mat;              // 0x34  previous parent matrix
    u32 x64;              // 0x64
    u32 type;             // 0x68  SetObj18 type (cloth set)
    u32 cmf;              // 0x6C  Obj18CmfSet/Get flag bits
    cObj* child;          // 0x70  ribbon / rope object created by SetObj18
    int x74;              // 0x74  bit26 (0x04000000): event ControlTransFlag skips the child flags
    union {
        u8 pad_78[0xC];
        u32 evName[3];    // 0x78  event model name of the packet that created it (event ExePacket_SetOm)
    };
    u8 debugFlag;         // 0x84
    u8 pad_85[3];
};

// Grenade work (game/obj01.cpp): flies under gravity, bounces off the scenario, can be held by a
// model (follows its parts) and explodes / lands in water after `life` frames.
struct Obj01Work {
    u32 flags;            // 0x00  bit0 start motion, bit1 motion running, bit2 water / bounce check, bit3 rotate parts 0
    void* pMot;           // 0x04
    u8 pad_8[2];
    u16 motPrm;           // 0x0A
    Vec rotSpd;           // 0x0C
    Vec spd;              // 0x18
    f32 grav;             // 0x24
    f32 rad;              // 0x28  bounce radius
    int life;             // 0x2C  frames until the explosion (0 = now)
    cModel* hold;         // 0x30  model holding it (follows `holdParts`)
    int holdParts;        // 0x34
    Vec holdOfs;          // 0x38
    Vec holdRot;          // 0x44
    int estNo0;           // 0x50  explosion effects (-1 = none: fade out instead)
    int estPrm0;          // 0x54
    int estNo1;           // 0x58
    int estPrm1;          // 0x5C
    int estNo2;           // 0x60  water splash
    int estPrm2;          // 0x64
    int estNo3;           // 0x68  underwater explosion
    int estPrm3;          // 0x6C
    u32 type;             // 0x70  0 plain, 1 hand grenade, 2 incendiary, 3 flash, 4 ?
    int holdTimer;        // 0x74  frames until it leaves the holder's hand
    u8 seDone;            // 0x78  landing SE state
    u8 pad_79[3];
    u32 flags7C;          // 0x7C  bit3: water splash done
};

// Thrown weapon item work (game/obj10.cpp `cWepItem`): the grenade layout (Obj01Work) with the
// landing SE counters split out.
struct WepItemWork {
    u32 flags;            // 0x00  bit0 start motion, bit1 motion running, bit2 water / bounce check, bit3 rotate parts 0
    void* pMot;           // 0x04
    u8 pad_8[2];
    u16 motPrm;           // 0x0A
    Vec rotSpd;           // 0x0C
    Vec spd;              // 0x18
    f32 grav;             // 0x24
    f32 rad;              // 0x28  bounce radius
    int life;             // 0x2C  frames until the explosion (0 = now)
    cModel* hold;         // 0x30  model holding it (follows `holdParts`)
    int holdParts;        // 0x34
    Vec holdOfs;          // 0x38
    Vec holdRot;          // 0x44
    int estNo0;           // 0x50  explosion effects (-1 = none: fade out instead)
    int estPrm0;          // 0x54
    int estNo1;           // 0x58
    int estPrm1;          // 0x5C
    int estNo2;           // 0x60  water splash
    int estPrm2;          // 0x64
    int estNo3;           // 0x68  underwater explosion
    int estPrm3;          // 0x6C
    u32 type;             // 0x70  0 plain, 1 water bomb, 2 explosive
    int holdTimer;        // 0x74  frames until it leaves the holder's hand
    u8 seLeft;            // 0x78  bounce SEs left to play
    u8 seCnt;             // 0x79  bounce SEs played
    u8 pad_7A[2];
    u32 flags7C;          // 0x7C  bit3: water splash done
};

// Gatling gun work (game/obj15.cpp `cObjGatling`): a mounted gun the player (or `ride`) fires
// at `target`; three cEmHit hit boxes take the damage, `eat` is its effect collision piece.
struct GatlingWork {
    u32 x00;              // 0x00
    int breakTimer;       // 0x04  frames of barrel spin-down after the break
    u8 pad_8[2];
    s16 cnt;              // 0x0A  frames since firing started (shots every 3rd frame after 30)
    u8 fire;              // 0x0C  setFire: start firing
    u8 firing;            // 0x0D
    u8 ammo;              // 0x0E  shots left (setReload: 40)
    u8 breakMode;         // 0x0F  0: weapon damage 0xD / 0x12 breaks it
    f32 rotY;             // 0x10  base yaw
    f32 maxRot;           // 0x14  yaw step limit (pi)
    int targetTimer;      // 0x18  frames until `target` reverts to the player
    u8 seOn;              // 0x1C  spin SE playing
    u8 pad_1D[3];
    u32 seHandle;         // 0x20
    class cSat* eat;      // 0x24
    class cEmHit* hit[3]; // 0x28
    u8 pad_34[8];
    class cEm* ride;      // 0x3C  enemy riding the gun
    class cModel* target; // 0x40  aimed-at model (player when NULL)
};

// Helicopter missile work (game/objMissile.cpp `cObjMissile`): hangs from a parts of the
// helicopter (setParent), then flies toward `target` (setFire) and explodes (objMissileBomb).
struct MissileWork {
    u32 x00;              // 0x00
    int timer;            // 0x04  fire wait / flight frames
    int hitWait;          // 0x08  frames before the hit checks start
    cModel* parent;       // 0x0C
    int partsNo;          // 0x10
    int noNormalize;      // 0x14  keep the parent parts matrix as it is
    Vec target;           // 0x18
    class cEmHit* hit;    // 0x24
    u8 hasTarget;         // 0x28
    u8 pad_29[3];
    Vec spd;              // 0x2C
};

// Gondola work (game/objGondola.cpp `cObjGondola`): a cable car the player / partner / up to
// five enemies ride; five scenario collision quads follow it.
struct GondolaWork {
    u32 x00;              // 0x00
    int timer;            // 0x04  break: frames before the sub motion starts
    u8 ridePL;            // 0x08  player is on board (ckRide)
    u8 pad_9[3];
    int rideSUB;          // 0x0C  partner is on board
    int cnt;              // 0x10  counts down every frame
    Vec x14;              // 0x14
    class cEm* rideEm[5]; // 0x20
    class cSat* sat[5];   // 0x34
    class cSat* sat2[5];  // 0x48
    struct MotionWork* subWork;  // 0x5C  sub (vibration / break) motion work (setSubMotion)
    void* subMot;         // 0x60  vibration motion (setVib)
    void* breakMot;       // 0x64  break motion (R0_Break)
};

// Mine trolley work (game/objTrolley.cpp `cObjTrolley`): three cars (parts 0 / 4 / 8) with a
// scenario and an effect collision piece each.
struct TrolleyWork {
    u32 flags;            // 0x00  bit0: start (setStart), bit1: 2nd start, bit2: stopped (ckStop)
    int timer;            // 0x04
    void* mot[9];         // 0x08  setMotion table: 0 run, 1 2nd run, 2/3 break (xFF), 4..8 player escape / die
    class cSat* sat[3];   // 0x2C  scenario pieces per car (the SetTrolley / SatClear loops run over 5)
    class cSat* sat2[5];  // 0x38  effect pieces per car
    u8 ride;              // 0x4C  the player rides the trolley
};

// Falling pillar work (game/objPillar.cpp `cObjPillar`).
struct PillarWork {
    u32 flags;            // 0x00  bit0: set (ckSet), cleared by setBreak / setThrow / setFall
    int timer;            // 0x04  frames before the fade out
    int rnd;              // 0x08  Rnd() & 1: action button type 3 / 4
    void* motBreak;       // 0x0C  setMotion
    void* motThrow0;      // 0x10  setThrow: lift, throw
    void* motThrow1;      // 0x14
    void* motEscape;      // 0x18  R0_Escape
    void* motFall0;       // 0x1C  setFall: fall, land
    void* motFall1;       // 0x20
    void* plMot;          // 0x24  player escape motion (plemEscape MotionSetCore)
    int plMotA;           // 0x28  its 4th argument
    Vec basePos;          // 0x2C  position at R0_Set (attack line end, plemEscape2 heading)
    Vec target;           // 0x38  setBreak position (plemEscape heading)
    Vec spd;              // 0x44  throw / fall speed
    u32 seHandle;         // 0x50  SndCall handle of the rolling SE
    class cSat* sat;      // 0x54  effect collision piece (objPillarEatSet)
    u8 escaped;           // 0x58  1: the player escaped / was hit (no more action button)
};

// Chain link work (game/obj1d.cpp): hangs between two parts of a parent model, fades out when
// the parent is lost.
struct ChainWork {
    u32 flags;            // 0x00  bit1: keep the parent parts matrices as they are (no axis normalize)
    int timer;            // 0x04  frames before the fade-out (LostWait)
    u8 pad_8[4];
    cModel* parent;       // 0x0C
    int parts1;           // 0x10
    int parts2;           // 0x14
    Vec ofs1;             // 0x18  offset in parts1
    Vec ofs2;             // 0x24  offset in parts2
    struct PenCloth* cloth;  // 0x30
};

// Ladder / tower work (game/objYagura.cpp).
struct YaguraWork {
    u8 pad_0[0x20];
    void* pMotionVib;     // 0x20  vibration motion set by setVib()
};

// Player weapon object work (game/objWep.cpp `cObjWep`, a cObj subclass; see pl_wep.h).
struct ObjWepWork {
    void* pMotNormal;     // 0x00 (0x328)  idle motion (pWepArc)
    void* pMotEmpty;      // 0x04 (0x32C)  idle motion with an empty magazine
    f32 lockRandPitch;    // 0x08 (0x330)  lock random: pitch range (pl_wep PlWepLockRand), degrees -> radians in setAbility
    f32 lockRandYaw;      // 0x0C (0x334)
    f32 lockRandPitchStep;  // 0x10 (0x338)
    f32 lockRandYawStep;  // 0x14 (0x33C)
    u8 x18;               // 0x18 (0x340)  handgun modules (wep02 objMauser/objRuger init): three bytes from a const table
    u8 x19;               // 0x19 (0x341)
    u8 x1A;               // 0x1A (0x342)
    u8 x1B;               // 0x1B (0x343)  (wep04 objXd9: 4th table byte)
    u8 pad_1C[4];
    cModel* parent;       // 0x20 (0x348)  model the weapon hangs on (parentSet)
    u16 x24;              // 0x24 (0x34C)  (cObjLauncher::init: 0x35)
    u8 mode;              // 0x26 (0x34E)  0 stay, 1 ready, 2 fire, 3 down, 4 reload, 5 drop (move dispatch)
    u8 step;              // 0x27 (0x34F)  step inside the mode
    u8 disp;              // 0x28 (0x350)  bit0 draw the laser this frame, bit1 drawn last frame, bits 2-4 setDisp types 0/1/2
    u8 pad_29[3];
    u32 seHandle;         // 0x2C (0x354)  SndCall handle stopped by resetMotion
    Vec marker;           // 0x30 (0x358)  laser sight end / hit marker position (pl_wep getMarkerPos, PlWepHitCheck2)
    class cEm* target;    // 0x3C (0x364)  enemy the laser points at (GetWepTargetPos)
};

// Rocket launcher work (game/objRocket.cpp `cObjLauncher` : cObjWep).
struct LauncherWork {
    ObjWepWork wep;       // 0x00 .. 0x40
    u32 flags;            // 0x40 (0x368)  bit0: a rocket is in flight
    Vec from;             // 0x44 (0x36C)  launch line (getMarkerPos)
    Vec to;               // 0x50 (0x378)
    class cObjRocket* rocket;  // 0x5C (0x384)  loaded rocket (loadRocket)
};

// Bow work (wep28 module `cObjBow` : cObjWep).
struct BowWork {
    ObjWepWork wep;       // 0x00 .. 0x40
    class cObjWep* allow; // 0x40 (0x368)  the arrow object shown on the bow (cObjAllow, ObjMgr id 0x10)
};

// Rocket work (game/objRocket.cpp `cObjRocket`).
struct RocketWork {
    Vec oldPos;           // 0x00 (0x328)  position before this frame's motion (hit line start)
    int timer;            // 0x0C (0x334)  flight frames left (300)
};

// Spear work (game/obj1b.cpp `cObjSpear`): thrown (R1_Throw), stuck in a parts of the enemy it hit
// (R1_Parent), then falling as a three-point rope (R1_Fall) and fading out (R1_LostWait / Lost).
struct SpearWork {
    u32 flags;            // 0x00  bit0: keep the parent parts matrix as it is (no axis normalize)
    int timer;            // 0x04  LostWait: frames before the fade (120); Throw: frames between the flight SEs
    int timer2;           // 0x08  Throw: flight frames left (60)
    u8 pad_C[0xC];
    cModel* parent;       // 0x18
    int partsNo;          // 0x1C
    Vec spd[3];           // 0x20  rope point speeds (R1_Fall)
    Vec throwSpd;         // 0x44
    int x50;              // 0x50
    int parentTimer;      // 0x54  frames until the spear falls off its parent (1800)
    int estTimer;         // 0x58  frames of the stuck-in-boss effect (600, every 2nd frame)
    u8 seBlk;             // 0x5C  landing SE (0xFF = none)
    u8 seNo;              // 0x5D
    u8 seId;              // 0x5E
    u8 sePlayed;          // 0x5F
    u8 type;              // 0x60  rope offsets table row (R1_Fall)
    u8 se2Blk;            // 0x61
    u8 se2No;             // 0x62
    u8 se2Id;             // 0x63
    u8 se3Blk;            // 0x64
    u8 se3No;             // 0x65
    u8 se3Id;             // 0x66
    u8 throwSeBlk;        // 0x67  flight SE (0xFF = none)
    u8 throwSeNo;         // 0x68
    u8 throwSeId;         // 0x69
    u8 estNo;             // 0x6A  landing effect (0xFF = none)
    u8 estPrm;            // 0x6B
    u8 x6C;               // 0x6C
    u8 x6D;               // 0x6D
    u8 x6E;               // 0x6E
    u8 x6F;               // 0x6F
    u8 espId;             // 0x70  effect owner id deleted on landing (0x32)
};

// Giant robot statue work (game/objRobo.cpp `cObjRobo`): the Salazar statue that walks after the
// player over the bridge; two scenario / effect collision pieces per side, 14 hit boxes.
struct RoboWork {
    s8 routine;           // 0x00  R0Tbl index
    s8 step;              // 0x01
    u8 pad_2[6];
    int pillar;           // 0x08  r226: index of the bridge pillar being pushed over (playerPillarDownCk)
    class cSat* sat[2];   // 0x0C  scenario pieces (front / back)
    class cSat* eat[2];   // 0x14  effect pieces
    class cEmHit* hit[14];  // 0x1C
    class cSat* eat2;     // 0x54  effect piece at the model position
    cObj* smd[2];         // 0x58  scroll objects following the feet (SetObjSmd)
    f32 fallSpdY;         // 0x60
    int cnt;              // 0x64
    int hitCnt[6];        // 0x68  frames each bridge piece has been hit
    f32 fallX;            // 0x80
    int timer;            // 0x84
};

// Player sub weapon work (game/objSubWep.cpp `cSubWep`: hand grenade / incendiary / flash / egg).
struct SubWepWork {
    u32 effNo;            // 0x00 (0x328)  landing effect (AtEffInfo pair by type; 0xD2 = none)
    u8 effPrm;            // 0x04 (0x32C)
    u8 pad_5[3];
    s32 attr;             // 0x08 (0x330)  AtEffInfo::flags of the hit (bit31 set when known, bit0: solid ground)
    u8 pad_C[8];
    Vec rotSpd;           // 0x14 (0x33C)
    Vec spd;              // 0x20 (0x348)
    f32 grav;             // 0x2C (0x354)
    f32 rad;              // 0x30 (0x358)  bounce radius
    int life;             // 0x34 (0x35C)  frames until the explosion (-1: only on impact)
    u8 pad_38[0x7C - 0x38];
    u8 x7C;               // 0x7C (0x3A4)  (ctor: 3)
    u8 seCnt0;            // 0x7D (0x3A5)  floor bounce SEs played
    u8 seCnt1;            // 0x7E (0x3A6)  wall bounce SEs played
    u8 flags;             // 0x7F (0x3A7)  bit0: explodes on the floor (fire / light), bit1: egg, bit4: hit a wall
};

// Bulldozer work (game/objBull.cpp `cObjBull`): the player / partner ride parts 2 through the
// break / lift / collision routines; one scenario piece (two while moving) and an effect piece.
struct BullWork {
    u32 flags;            // 0x00  bit0 goal, bit1..4 break 1st..4th done, bit5 lift, bit6 truck go, bit7, bit8 lift wait
    int frame;            // 0x04  Collision: motion frames - 30
    u8 pad_8[4];
    void* mot[12];        // 0x0C  setMotion table: 0 break1st/set, 1 to2nd, 2 break2nd, 3 to lift, 4 lift, 5 to3rd, 6 break3rd, 7 to4th, 8 break4th, 9..11 collision
    class cSat* sat;      // 0x3C  scenario piece (type 1)
    class cSat* sat2;     // 0x40  scenario piece (type 8) while moving
    class cSat* eat;      // 0x44  effect piece (type 7)
    int cnt;              // 0x48  MotionMove calls of the current routine
    int timer;            // 0x4C  frames in the current routine (getMoveFrame*)
    u32 x50;              // 0x50  SetBull 5th argument
    u32 type;             // 0x54  SetBull 5th argument: setRide start routine (0: break1st, 1: break2nd, 2: lift wait, 3: to3rd, 4: break3rd)
    u8 break1st;          // 0x58  break repeats left
    u8 break2nd;          // 0x59
    u8 break3rd;          // 0x5A
    u8 break4th;          // 0x5B
    void (*adjustFunc)(cObj*);  // 0x5C  setAdjustMode: called before the player is carried along
    u8 adjustMode;        // 0x60  0: the riders are not carried along
    u8 ride;              // 0x61  the player rides the bulldozer (setRide)
    u8 x62;               // 0x62
    u8 breakTruck;        // 0x63  setBreakTruck: Collision continues with step 2
};

// Ladder work (game/obj13.cpp `cObjLadder`): a ladder the player / partner climbs (plobjLadderClimb),
// kicks down (plobjLadderDown) and stands up again (plobjLadderReset).
struct LadderWork {
    u32 flags;            // 0x00  bit0 motions set, bit1 off (setOff), bit2 partner climbing, bit3 transOld
    int status;           // 0x04  0 standing, 1 downed, 2 falling, 3 falling (timer done), 4 fall / reset motion
    int x08;              // 0x08
    u32 ladderNum;        // 0x0C  rungs (setLadderInfo; converted unsigned)
    u8 pad_10[4];
    Vec basePos;          // 0x14  position at SetLadder (R1_Set restores it)
    f32 baseRotY;         // 0x20
    int climbTimer;       // 0x24  frames the action button stays off after a climb (setClimb: 90)
    int resetReserve;     // 0x28  setResetReserve: 60
    int downTimer;        // 0x2C  setDown: 17 frames until status 3
    cObj* pair;           // 0x30  second ladder object sharing the collision flags
    int camera;           // 0x34  setCamera: camera cut of the climb (-1: the ladder cameras)
    void* mot[20];        // 0x38  setMotion table (player / partner climb, down, reset motions)
    u8 etcNo;             // 0x88  etc model number (GetEtcFlgPtr)
};

// Enemy head work (game/obj16.cpp `cObj16`): a head model hung on parts `partsNo` of `body` that
// looks at the player (obj16NeckMove), bites (R1_Atk / R1_Critical) and fades out when its
// enemies die.
struct Obj16Work {
    u32 flags;            // 0x00  bit0: the lost-wait timer runs (setLostWait / clearLostWait)
    int timer;            // 0x04  routine step timer
    int atkTimer;         // 0x08  R1_Atk: attack frames left
    u8 pad_C[4];
    cModel* target;       // 0x10  enemy whose position / id the SEs use (SetObj16 3rd argument)
    cModel* body;         // 0x14  enemy the head is attached to (SetObj16 4th argument)
    int partsNo;          // 0x18  parts of `body` the head follows
    int seTimer;          // 0x1C  frames between the type 2 / 3 loop SEs
    u32 seHandle;         // 0x20  SndCall handle of the loop SE (SndStop)
    int lostWait;         // 0x24  frames before the fade out when the enemies are dead (150)
    int x28;              // 0x28
    int dieEffTimer;      // 0x2C  frames before the die effect (setDieEff: 3)
    int estTimer;         // 0x30  frames between the idle effects
    f32 neckAng;          // 0x34  neck yaw toward the player (smoothed)
    void* mot[11];        // 0x38  setMotData: 0-2 idle, 3-6 bite, 7-9 (unused), 10 ...
    void* plMot;          // 0x64  setPlDmgMot: player damage motion (plemDmMStar)
    int plMotA;           // 0x68  its MotionSetCore 4th argument
    int x6C;              // 0x6C
    s16 atkWait;          // 0x70  frames the kind 2 attack is disabled after a hit (90)
    u8 x72;               // 0x72
    u8 x73;               // 0x73
    u8 x74;               // 0x74
    u8 active;            // 0x75  the head is awake (R1_CoreMove picks the awake motions)
    u8 x76;               // 0x76  (60, counts down)
    u8 espKind;           // 0x77  effect owner kind (0x3D)
    u8 espKind2;          // 0x78  effect owner kind of the attack effects (0x3E)
    u8 x79;               // 0x79
    u8 atkEnable;         // 0x7A  ckAtkEnable: R1_CoreMove ran this frame
    u8 atkHit;            // 0x7B  ckAtkHit: obj16AtkCk hit the player this frame
    Vec scale;            // 0x7C  target scale (setScale), blended into cModel::scale by move
    class cCtrl* ctrl12;  // 0x88  GetCtrlCtrl12() (Ctrl12Set on a hit)
};

// Map object work (game/obj.cpp), sizeof 0x3D8: the cModel (0x320; motion work `mot` / `pMotion`
// / `motFrame`.., `sub2B4.atari`, `sub2B4.pFootShadowTbl` are cModel members, see model.h), the
// scroll block and the per-object work area. Per-object modules keep their state in `work`.
class cObj : public cModel {
public:
    u8 pad_320[4];        // 0x320
    s32 blk;              // 0x324  scroll block the object belongs to (-2 free, -1 SetObjSmd)
    // 0x328: per-object work area (Efm09Work runs to the end of the object: x3D0 / callBack are
    // inside the union so that they keep their offsets)
    union {
        u8 work[0x3D8 - 0x328];  // 0x328 per-object work area
        struct {
            u8 pad_work[0x3D0 - 0x328];
            u8 x3D0;              // 0x3D0
            u8 pad_3D1[3];
            void (*callBack)(cObj*);  // 0x3D4
        };
        Obj03Work obj03;
        Efm04Work efm04;
        Efm05Work efm05;
        Efm09Work efm09;
        ObaModelWork obaModel;
        Obj26Work obj26;
        YaguraWork yagura;
        BellWork bell;
        IslandWork island;
        Obj08Work o8;
        ChainWork chain;
        Obj00Work o0;
        Obj12Work o12;
        Obj18Work o18;
        Obj01Work o1;
        WepItemWork wepItem;
        GatlingWork gatling;
        MissileWork missile;
        GondolaWork gondola;
        TrolleyWork trolley;
        PillarWork pillar;
        ObjWepWork wep;
        LauncherWork launcher;
        BowWork bow;
        RocketWork rocket;
        SpearWork spear;
        RoboWork robo;
        SubWepWork subWep;
        BullWork bull;
        LadderWork ladder;
        Obj16Work o16;
    };

    cObj();
    virtual ~cObj() {}
};

class cObjMgr : public cManager<cObj> {
public:
    u32 x34;

    cObjMgr();
    virtual void* memAlloc(u32 size) { return MemAlloc(size, 1); }
    virtual void memFree(void* p) { MemFree(p); }
    virtual void memClear(cObj* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual void destroy(cObj* p);
    virtual int construct(cObj* p, u32 id);   // calls the int overload (obj.cpp)
    int construct(cObj* p, int id);           // placement-new of the per-id class, or ObjInitFunc[id]
    void move();                              // dieCheck, then objMove on every live object
};

extern cObjMgr ObjMgr;

// Work `no` of ObjMgr, 0 when out of range. A free inline: an in-class one would be emitted out of
// line in obj.cpp (the unit owns cObjMgr's vtable), which the DOL does not have.
static inline cObj* ObjMgrWork(u32 no)
{
    if (no >= ObjMgr.nArray) {
        return 0;
    }
    return (cObj*)((u8*)ObjMgr.pArray + ObjMgr.size * no);
}

struct EspGenWork;
extern "C" {
// game/esp_efm.cpp: creates the obj04 / obj05 / obj09 effect model of a sequence record
// (`info` is the caller's EspInfo, esp.h). esp_sub.cpp EspSeqSet is the only caller.
cObj* EfmSeqSet(EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate, Vec* ofs);
// game/obj04.cpp / game/obj05.cpp: orient the model along `m`
void Efm04RotMatrix(cObj* obj, Mtx m);
void Efm05RotMatrix(cObj* obj, Mtx m);
}

#endif
