// em32 module (D:/Bio4/Prog/em32.cpp): the caged boss of the container area. It appears in four
// steps out of its cage (Parasite / LastMode / 2ndAppear .. 4thAppear), then walks the containers
// (em2c's blended walk / dash, StepUp / StepDown, the tunnel and ceiling attacks), catches the
// player (CatchHit / P_CatchHit / C_AtkHit with the plem32_* callbacks) and breaks the barred
// door (BreakBarred). Three forms: `mode` 0, 1 (the parasite shows) and 2 (the last form).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em32.h"
#include "emhit.h"
#include "obj00.h"
#include "obj01.h"
#include "obj.h"
#include "emBarred.h"
#include "embarrel.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "act_btn.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "dbmodule.h"
#include "foot_shadow.h"
#include "main_mem.h"

asm(".comm common_em32,52,4");

extern "C" void OSReport(const char* fmt, ...);
int GetWepDmVal(cEm* em, u32 a, int b);   // em10.h (not included: it pulls emwep.h's global plemBackjump)
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp
extern FootShadowTbl Em32_fs_tbl;     // game/foot_shadow.cpp
extern "C" void* memcpy(void* dst, const void* src, unsigned int n);
extern "C" cObj* SetObaModel(cObj* parent, int partsNo, Vec* ofs, f32 rad, u8 type, f32 h);   // game/obj20.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.h declares EmSetDieCnt without arguments; this module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");
// em.h declares cEmRack::setBreak without arguments; this module passes the breaking position.
void EmRackSetBreak(cEmRack* rack, Vec* pos) asm("setBreak__7cEmRack");
// COMPILER-DIFF #4: the original passes the int work field to the u16 parameter without the
// truncation ours emits (`lwz` instead of `lhz`); int-view declaration of the blend setter.
void em32BlendMotSetI(cEm32* em, void* m0, void* m1, void* m2, void* m3, int a, int b, int d) asm("em32BlendMotSet__FP5cEm32PvN31iiUs");

static void em32_R0_Init(cEm32* em);
static void em32_R0_Move(cEm32* em);
static void em32_R1_br_Dummy(cEm32* em);
static void em32_R1_Parasite(cEm32* em);
static void em32_R1_LastMode(cEm32* em);
static void em32_R1_2ndAppear(cEm32* em);
static void em32_R1_3rdAppear(cEm32* em);
static void em32_R1_3rdFall(cEm32* em);
static void em32_R1_4thAppear(cEm32* em);
static void em32_R1_Wait(cEm32* em);
static void em32_R1_Ambush(cEm32* em);
static void em32_R1_Walk(cEm32* em);
static void em32_R1_Dash(cEm32* em);
static void em32_R1_Back(cEm32* em);
static void em32_R1_AtkWalk(cEm32* em);
static void em32_R1_Turn(cEm32* em);
static void em32_R1_Threat(cEm32* em);
static void em32_R1_AmbushAtk(cEm32* em);
static void em32_R1_Atk(cEm32* em);
static void em32_R1_br_Catch(cEm32* em);
static void em32_R1_Catch(cEm32* em);
static void em32_R1_CatchHit(cEm32* em);
static void plem32_CatchHit(cPlayer* pl);
static void em32_R1_LongAtk(cEm32* em);
static void em32SitAction(cEm32* em);
static void em32SitUpAction(cEm32* em);
static void em32BackjumpAction(cEm32* em);
static void em32EscapeAction(cEm32* em);
static void em32_R1_StepUp(cEm32* em);
static void em32_R1_StepWait(cEm32* em);
static void em32_R1_StepDown(cEm32* em);
static void em32_R1_TunnelAtk(cEm32* em);
static void em32_R1_JumpUp(cEm32* em);
static void em32_R1_JumpDown(cEm32* em);
static void em32_R1_C_Wait(cEm32* em);
static void em32_R1_br_C_Atk(cEm32* em);
static void em32_R1_C_Atk(cEm32* em);
static void em32_R1_C_AtkHit(cEm32* em);
static void plem32_C_AtkHit(cPlayer* pl);
static void em32_R1_P_Atk(cEm32* em);
static void em32_R1_br_P_Catch(cEm32* em);
static void em32_R1_P_Catch(cEm32* em);
static void em32_R1_P_CatchHit(cEm32* em);
static void plem32_P_CatchHit(cPlayer* pl);
static void em32_R1_Ground(cEm32* em);
static void em32_R1_BreakBarred(cEm32* em);
static void em32_R0_Damage(cEm32* em);
static void em32_R1_Dm_Normal(cEm32* em);
static void em32_R0_Die(cEm32* em);
static void em32_R1_Die_Normal(cEm32* em);
static void plemDivide(cPlayer* pl);

// The player callbacks that emwep.h also declares carry C linkage here (the module's own local
// copies keep the unmangled names in the REL symbol table).
extern "C" {
static void plemSit(cPlayer* pl);
static void plemBackjump(cPlayer* pl);
static void plemEscape(cPlayer* pl);
}

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// The enemy a player damage callback belongs to (pl_sub SetPlDamage's first argument).
#define PL_EM(pl) ((cEm32*) (pl)->dmgType)

#define VIB_TBL ((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc))

// Struct-member view of the player pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Scalar reference stores: pG / the player pointer are reloaded after them (st_room.h).
static inline void IntSet(int& d, int v) { d = v; }
static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void F32Set(f32& d, f32 v) { d = v; }
static inline void PSet(void*& d, void* v) { d = v; }

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em32DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

// Collision flag bits set / cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOn(cAtariInfo* at, u16 b) { at->flags |= b; }
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->flags &= mask; }

// Difficulty tables: pG is reloaded after every store (reference stores; the inline takes the work
// pointer so that the store keeps the work base instead of folding into the enemy's).
static inline void em32Timer2Set(Em32Work* w, int a, int b, int c, int e)
{
    if (pG->x4F88 <= 3) {
        IntSet(w->timer2, a);
    }
    if (pG->x4F88 <= 1) {
        IntSet(w->timer2, b);
    }
    if (pG->x4F88 > 6) {
        IntSet(w->timer2, c);
    }
    if (pG->x4F88 > 9) {
        IntSet(w->timer2, e);
    }
}

// The attack-hit byte written from a promoted int (an SImode pseudo shared with the int stores).
static inline void em32AtkHitSet(Em32Work* w, int v)
{
    U8Set(w->x991, v);
}

// COMPILER-DIFF #12 (cse path knowledge): in a `case` arm reached through the switch's once-used label
// our cse still knows `w == em + 0x3E0` and folds the reference stores of the difficulty tables into
// `em`-based addresses; the original keeps them `w`-based there. The launder hides the equivalence.
#define EM32_W_FRESH(w) asm("" : "+r"(w))

// Non-struct store through the work pointer with the offset inside the MEM (`*(T*) ((u8*) w + ofs)`):
// cse gets no address pseudo to associate with `em + K` (the #12 fold above), so the store stays
// w-based, and as a scalar mem it keeps the following pG load below it. Unlike the launder it does not
// re-set `w`, so alias analysis keeps w's base value (an argument): the frame stores of a later call in
// the function can still pass a w-based store (C_Atk).
#define EM32_W_SET(w, T, field, v) (*(T*) ((u8*) (w) + (u32) &((Em32Work*) 0)->field) = (v))

static inline void em32Timer2SetW(Em32Work* w, int a, int b, int c, int e)
{
    if (pG->x4F88 <= 3) {
        EM32_W_SET(w, int, timer2, a);
    }
    if (pG->x4F88 <= 1) {
        EM32_W_SET(w, int, timer2, b);
    }
    if (pG->x4F88 > 6) {
        EM32_W_SET(w, int, timer2, c);
    }
    if (pG->x4F88 > 9) {
        EM32_W_SET(w, int, timer2, e);
    }
}

// The three effect systems attached to an esp kind are deleted together.
#define EM32_EFFECT_DELETE(kind, em)          \
    EffectEspDelete(0, kind, (u32) (em), 0);  \
    EffectEspgenDelete(0, kind, (int) (em));  \
    EffectEfmDelete(0, kind, (int) (em))

// Claws (parts 0x12..0x19) of an attack motion against the player.
#define EM32_CLAW_ATK(em, no)      \
    em32AtkCk(em, no, 0x12);       \
    em32AtkCk(em, no, 0x13);       \
    em32AtkCk(em, no, 0x14);       \
    em32AtkCk(em, no, 0x15);       \
    em32AtkCk(em, no, 0x16);       \
    em32AtkCk(em, no, 0x17);       \
    em32AtkCk(em, no, 0x18);       \
    em32AtkCk(em, no, 0x19)

// Tail (parts 0x54..0x5D) of the last form's attacks.
#define EM32_TAIL_ATK(em, no)      \
    em32AtkCk(em, no, 0x54);       \
    em32AtkCk(em, no, 0x55);       \
    em32AtkCk(em, no, 0x56);       \
    em32AtkCk(em, no, 0x57);       \
    em32AtkCk(em, no, 0x58);       \
    em32AtkCk(em, no, 0x59);       \
    em32AtkCk(em, no, 0x5A);       \
    em32AtkCk(em, no, 0x5B);       \
    em32AtkCk(em, no, 0x5C);       \
    em32AtkCk(em, no, 0x5D)

// Turn towards `target` with the blended walk: the yaw step is filtered into blendVal (which also
// picks the left / right blend motion) and applied as a fraction of PI/32.
#define EM32_BLEND_TURN(em, w, target, lim)                                        \
    {                                                                              \
        f32 ang = Muku(&(em)->pos, target, (em)->rot.y, 3.14159274f);              \
        if (ang > lim) {                                                           \
            ang = lim;                                                             \
        }                                                                          \
        if (ang < -lim) {                                                          \
            ang = -lim;                                                            \
        }                                                                          \
        {                                                                          \
            f32 blend = ang * 324.676086f;                                         \
            (w)->blendVal = (w)->blendVal * 0.899999976f + blend * 0.100000001f;   \
        }                                                                          \
        (em)->rot.y += (w)->blendVal * 0.00392156886f * 0.0981747732f;             \
        (em)->rot.y = LIMIT_ANGLE((em)->rot.y);                                    \
    }

// The same turn with the product written back into the angle variable (`ang *= K`: the multiply is
// tied to the dying Muku result).
#define EM32_BLEND_TURN2(em, w, target, lim)                                       \
    {                                                                              \
        f32 ang = Muku(&(em)->pos, target, (em)->rot.y, 3.14159274f);              \
        if (ang > lim) {                                                           \
            ang = lim;                                                             \
        }                                                                          \
        if (ang < -lim) {                                                          \
            ang = -lim;                                                            \
        }                                                                          \
        ang *= 324.676086f;                                                        \
        (w)->blendVal = (w)->blendVal * 0.899999976f + ang * 0.100000001f;         \
        (em)->rot.y += (w)->blendVal * 0.00392156886f * 0.0981747732f;             \
        (em)->rot.y = LIMIT_ANGLE((em)->rot.y);                                    \
    }

Em32Func Em32_R0_move_tbl[4] = {
    em32_R0_Init,
    em32_R0_Move,
    em32_R0_Damage,
    em32_R0_Die,
};

// Routine 1 table: {branch check, routine} per xFD (a flat table: `[xFD * 2 + 1]`).
static Em32Func Em32_R1_move_tbl[66] = {
    em32_R1_br_Dummy, em32_R1_Parasite,     // 0x00
    em32_R1_br_Dummy, em32_R1_LastMode,     // 0x01
    em32_R1_br_Dummy, em32_R1_2ndAppear,    // 0x02
    em32_R1_br_Dummy, em32_R1_3rdAppear,    // 0x03
    em32_R1_br_Dummy, em32_R1_3rdFall,      // 0x04
    em32_R1_br_Dummy, em32_R1_4thAppear,    // 0x05
    em32_R1_br_Dummy, em32_R1_Wait,         // 0x06
    em32_R1_br_Dummy, em32_R1_Ambush,       // 0x07
    em32_R1_br_Dummy, em32_R1_Walk,         // 0x08
    em32_R1_br_Dummy, em32_R1_Dash,         // 0x09
    em32_R1_br_Dummy, em32_R1_Back,         // 0x0A
    em32_R1_br_Dummy, em32_R1_AtkWalk,      // 0x0B
    em32_R1_br_Dummy, em32_R1_Turn,         // 0x0C
    em32_R1_br_Dummy, em32_R1_Threat,       // 0x0D
    em32_R1_br_Dummy, em32_R1_AmbushAtk,    // 0x0E
    em32_R1_br_Dummy, em32_R1_Atk,          // 0x0F
    em32_R1_br_Catch, em32_R1_Catch,        // 0x10
    em32_R1_br_Dummy, em32_R1_CatchHit,     // 0x11
    em32_R1_br_Dummy, em32_R1_LongAtk,      // 0x12
    em32_R1_br_Dummy, em32_R1_StepUp,       // 0x13
    em32_R1_br_Dummy, em32_R1_StepWait,     // 0x14
    em32_R1_br_Dummy, em32_R1_StepDown,     // 0x15
    em32_R1_br_Dummy, em32_R1_TunnelAtk,    // 0x16
    em32_R1_br_Dummy, em32_R1_JumpUp,       // 0x17
    em32_R1_br_Dummy, em32_R1_JumpDown,     // 0x18
    em32_R1_br_Dummy, em32_R1_C_Wait,       // 0x19
    em32_R1_br_C_Atk, em32_R1_C_Atk,        // 0x1A
    em32_R1_br_Dummy, em32_R1_C_AtkHit,     // 0x1B
    em32_R1_br_Dummy, em32_R1_P_Atk,        // 0x1C
    em32_R1_br_P_Catch, em32_R1_P_Catch,    // 0x1D
    em32_R1_br_Dummy, em32_R1_P_CatchHit,   // 0x1E
    em32_R1_br_Dummy, em32_R1_Ground,       // 0x1F
    em32_R1_br_Dummy, em32_R1_BreakBarred,  // 0x20
};

static Em32Func Em32_R2_move_tbl[1] = {
    em32_R1_Dm_Normal,
};

static Em32Func Em32_R3_move_tbl[1] = {
    em32_R1_Die_Normal,
};

// Attack parameters per em32AtkCk kind.
static EmAtkInfo em32_atk_tbl[6] = {
    { 400.0f, 8, 0x44C, 0, 10, 0 },
    { 400.0f, 8, 0, 4, 10, 0 },
    { 400.0f, 8, 0x44C, 0, 10, 0 },
    { 1200.0f, 8, 0, 4, 10, 0 },
    { 600.0f, 8, 0x44C, 0, 10, 0 },
    { 600.0f, 8, 0x44C, 0, 10, 0 },
};

// Tail cloth chain (em32ClothSet): the parts, their upper / lower neighbours and the swing limits.
static u8 em32_cloth_parts[12] = { 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x3D, 0x3E, 0x3F, 0x40 };
static u8 em32_cloth_up[12] = { 0xFF, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0xFF, 0x3D, 0x3E, 0x3F };
static u8 em32_cloth_down[12] = { 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0x3E, 0x3F, 0x40, 0xFF };
static f32 em32_cloth_max[12] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
asm(".section .data\n\t.balign 8\n\t.text");

extern "C" void _prolog()
{
    OSReport("em32 prolog Ok\n");
    EmInitFunc = Em32Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em32Init(cEm* em)
{
    new (em) cEm32();
}

void em32DmCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero;
    int near;
    int dmg;
    int flag;
    int wep;

    if (em->hp > 0 && em32DeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            if (w->dmGuard == 0) {
                w->dmGuard = 120;
                flag = 0;
                if (w->mode != 2) {
                    flag = 1;
                }
                if (w->flags & 0x800) {
                    flag = 1;
                }
                LifeDownSet2(em, 500, 0, flag);
                {
                    int f = w->flags & 0x800;
                    if (f) {
                        return;
                    }
                    if (em->hp > 0) {
                        w->dmgTotal = f;
                        em->dmWep = 0x16;
                        em->xFC = 2;
                        em->xFD = f;
                        em->xFE = f;
                        em->xFF = f;
                        return;
                    }
                    EmSetDie(em);
                    EmSetDieCntE(em);
                    em->xFC = 3;
                    em->xFD = f;
                    em->xFE = f;
                    em->xFF = f;
                    return;
                }
            }
            break;
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    zero = 0;
    em->dmHit = zero;
    near = 0;
    w->flags |= 0x200;
    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    wep = em->dmWep;
    em->dmType = 1;
    if (wep == 0x10) {
        em->dmType = 0x11;
    }
    dmg = em32SetDmVal(em);
    flag = 0;
    if (w->mode != 2) {
        flag = 1;
    }
    LifeDownSet2(em, dmg, 0, flag);
    em32BloodSet(em);
    SndCall(8, 0xA, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmSetDieCntE(em);
        em->xFC = 3;
        em->xFD = zero;
        em->xFE = zero;
        em->xFF = zero;
        return;
    }
    if (w->flags & 0x800) {
        return;
    }
    if (w->flags & 8) {
        return;
    }
    if (w->flags & 0x100) {
        return;
    }
    if (w->mode == 1 && (w->flags & 0x100000)) {
        if (em->hp <= em->hpMax * 3 / 8) {
            EmRoutineSet(em, 1, 1, 0, 0);
            return;
        }
    }
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0x11:
    case 0x26:
    case 0x2B:
        dmg = Rnd() % 10 + 10;
        break;
    case 0xB:
    case 0x1B:
    case 0x1D:
    case 0x27:
        dmg = Rnd() % 10 + 5;
        break;
    case 0xC:
        dmg = Rnd() % 20 + 20;
        break;
    case 9:
    case 0xA:
    case 0x10:
    case 0x14:
    case 0x15:
    case 0x28:
        dmg = Rnd() % 25 + 25;
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            dmg = Rnd() % 70 + 50;
        } else {
            dmg = Rnd() % 20 + 10;
        }
        break;
    case 5:
    case 6:
    case 0x2C:
        dmg = Rnd() % 100 + 50;
        break;
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        dmg = 1000;
        break;
    case 0xE:
        dmg = 0;
        break;
    }
    if (w->mode == 1) {
        dmg /= 2;
    }
    if (w->mode == 2) {
        dmg /= 4;
    }
    w->dmgTotal += dmg;
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        if (w->dmgTotal <= 100) {
            return;
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (w->dmgTotal <= 100) {
            return;
        }
        break;
    case 0xE:
        return;
    case 5:
    case 6:
    case 0xD:
    case 0x12:
    case 0x29:
    case 0x2C:
    default:
        break;
    }
    w->dmgTotal = 0;
    EmRoutineSet(em, 2, 0, 0, 0);
}

void cEm32::move()
{
    Em32Work* w = EM32_WK(this);
    f32 d;

    if (xFC) {
        em32DmCk(this);
    }
    w->flags &= 0xFFF632A0;
    if (w->wait) {
        w->wait--;
    }
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->longAtkWait) {
        w->longAtkWait--;
    }
    if (w->dmGuard) {
        w->dmGuard--;
    }
    if (w->mode != 2 && hp <= 1) {
        w->wait = 30;
        w->longAtkWait = 30;
    }
    w->breakNo = 0xFF;
    w->breakNo2 = 0xFF;
    if (xFC) {
        em32RouteCk(this);
    }
    em32GetPlPos(this);
    Em32_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em32BodyMove(this);
    em32NeckMove(this);
    partsWorldCalc();
    em32ScaleCompress(this);
    d = SQRTF((oldPos.x - pos.x) * (oldPos.x - pos.x) + (oldPos.z - pos.z) * (oldPos.z - pos.z));
    EmAtCheck(this);
    atari.move();
    if (!(w->flags & 0x40)) {
        SatMgr.check(this, 0);
    }
    if (SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z)) < d * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    em32ClothMove(this);
    if (w->pTexModel && !(w->flags & 0x40000)) {
        w->pTexModel->setTexBlendTbl(w->texBlend);
        w->pTexModel->setBlendRatio(0xFF);
        w->pTexModel->setBlendType(2);
    }
    if (hp > 0 && (be_flag & 2) && !(w->flags & 0x10000)) {
        if (w->breathTimer) {
            w->breathTimer--;
        } else {
            w->breathTimer = 59;
            SndCall(8, 0x30, &pos, id, 0, this);
        }
    }
    em32BreathSeStopCk(this);
    if (w->flags & 0x20000) {
        be_flag |= 0x10;
        if (w->flags & 0x10000) {
            if (shdCol <= 0xF6) {
                shdCol += 8;
            } else {
                shdCol = 0xFF;
            }
        } else {
            if (shdCol > 8) {
                shdCol -= 8;
            } else {
                shdCol = 0;
            }
        }
    } else {
        be_flag &= ~0x10;
    }
}

static void em32_R0_Init(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cModelInfo* info;
    cModel* p;
    Vec v;
    f32 fzero;
    int zero;

    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em32() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    info = ModInfoMgr.create(ARC(6), ARC(5));
    if (info) {
        em->addModel(info);
    }
    zero = 0;
    w->pTexModel = (cModelInfo*) zero;
    info = ModInfoMgr.create(ARC(7), ARC(5));
    if (info) {
        em->addModel(info);
        w->pTexModel = info;
    }
    em->be_flag |= 0x1000000;
    EspDataLoad((u32) ARC(8), 0x2A, 0);
    em32PlDivideModelInit(em);
    w->pCatchObj = (cObj*) zero;
    w->pCtrl12 = GetCtrlCtrl12();
    em->pFootShadowTbl = &Em32_fs_tbl;
    em32ClothSet(em);
    ((cParts*) em->getPartsPtr(0x21))->motParts.flags |= 0x1000;
    ((cParts*) em->getPartsPtr(0x27))->motParts.flags |= 0x1000;
    ((cParts*) em->getPartsPtr(0x2D))->motParts.flags |= 0x1000;
    ((cParts*) em->getPartsPtr(0x33))->motParts.flags |= 0x1000;
#line 836 "D:/Bio4/Prog/em32.cpp"
    em->p2A4 = MEM_ALLOC(0x98, 1, 0xD);
    // Compound literals: the zero template is shared with plem32_P_CatchHit's light init.
    em->lightInfo.init2(0, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 10000.0f, 10000.0f, 10000.0f }), 2);
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 1500.0f, 1, 0x2000, 10);
    em->atari.setPriority(1);
    em->litArea.on(1);
    fzero = 0.0f;
    v.x = fzero;
    v.y = fzero;
    v.z = fzero;
    SetObaModel((cObj*) em, 0x1C, &v, 700.0f, 0, 900.0f);
    SetObaModel((cObj*) em, 0x1D, &v, 700.0f, 0, 900.0f);
    SetObaModel((cObj*) em, 0x1E, &v, 700.0f, 0, 900.0f);
    YarareInit(em, fzero, fzero, fzero, 300.0f, 200.0f, 2, 5);
    YarareAdd(em, &w->hit[0], fzero, fzero, fzero, 250.0f, 200.0f, 3, 5);
    YarareAdd(em, &w->hit[1], fzero, fzero, fzero, 320.0f, 200.0f, 4, 5);
    YarareAdd(em, &w->hit[2], fzero, fzero, fzero, 200.0f, 100.0f, 6, 5);
    YarareAdd(em, &w->hit[3], fzero, fzero, -200.0f, 350.0f, 200.0f, 0x1B, 5);
    YarareAdd(em, &w->hit[4], fzero, fzero, -200.0f, 350.0f, 200.0f, 0x1C, 5);
    YarareAdd(em, &w->hit[5], fzero, fzero, -200.0f, 500.0f, 200.0f, 0x1D, 5);
    YarareAdd(em, &w->hit[6], fzero, fzero, -200.0f, 500.0f, 200.0f, 0x1E, 5);
    YarareAdd(em, &w->hit[7], fzero, fzero, -200.0f, 400.0f, 200.0f, 0x1F, 5);
    YarareAdd(em, &w->hit[8], fzero, 50.0f, -150.0f, 350.0f, 150.0f, 0x20, 5);
    YarareAdd(em, &w->hit[9], -450.0f, fzero, fzero, 130.0f, 450.0f, 9, 3);
    YarareAdd(em, &w->hit[10], -500.0f, fzero, fzero, 115.0f, 500.0f, 0xA, 3);
    YarareAdd(em, &w->hit[11], -150.0f, fzero, fzero, 150.0f, 150.0f, 0xC, 3);
    YarareAdd(em, &w->hit[12], fzero, fzero, fzero, 150.0f, 450.0f, 0x12, 3);
    YarareAdd(em, &w->hit[13], fzero, fzero, fzero, 150.0f, 300.0f, 0x13, 3);
    YarareAdd(em, &w->hit[14], fzero, fzero, fzero, 150.0f, 300.0f, 0x14, 3);
    YarareAdd(em, &w->hit[15], fzero, fzero, fzero, 150.0f, 250.0f, 0x15, 3);
    YarareAdd(em, &w->hit[16], fzero, fzero, fzero, 130.0f, 250.0f, 0x16, 3);
    YarareAdd(em, &w->hit[17], fzero, -500.0f, fzero, 150.0f, 600.0f, 0x22, 1);
    YarareAdd(em, &w->hit[18], fzero, -800.0f, fzero, 150.0f, 800.0f, 0x23, 1);
    YarareAdd(em, &w->hit[19], fzero, -500.0f, fzero, 150.0f, 600.0f, 0x28, 1);
    YarareAdd(em, &w->hit[20], fzero, -800.0f, fzero, 150.0f, 800.0f, 0x29, 1);
    YarareAdd(em, &w->hit[21], fzero, -500.0f, fzero, 150.0f, 600.0f, 0x2E, 1);
    YarareAdd(em, &w->hit[22], fzero, -800.0f, fzero, 150.0f, 800.0f, 0x2F, 1);
    YarareAdd(em, &w->hit[23], fzero, -500.0f, fzero, 150.0f, 600.0f, 0x34, 1);
    YarareAdd(em, &w->hit[24], fzero, -800.0f, fzero, 150.0f, 800.0f, 0x35, 1);
    YarareAdd(em, &w->hit[25], fzero, fzero, fzero, 250.0f, 300.0f, 0x56, 0);
    YarareAdd(em, &w->hit[26], fzero, fzero, fzero, 250.0f, 300.0f, 0x57, 0);
    YarareAdd(em, &w->hit[27], fzero, fzero, fzero, 250.0f, 300.0f, 0x58, 0);
    em->lockParts = 2;
    em->lockOfs.x = fzero;
    em->lockOfs.y = fzero;
    em->lockOfs.z = fzero;
    w->flags = zero;
    w->neckAng = fzero;
    w->wait = zero;
    w->atkWait = Rnd() % 150 + 150;
    w->x7C4 = zero;
    w->dmgTotal = zero;
    w->sndId = zero;
    // The 600/0xFF constants are REG_EQUIV pseudos the original never allocates: reload re-materialises
    // them in the spill registers r10/r9 (59 takes r11); ours local-allocs them the other way round.
    {
        register int lw asm("r10"); // COMPILER-DIFF: #13
        register int bn asm("r9"); // COMPILER-DIFF: #13
        lw = 600;
        w->longAtkWait = lw;
        w->voiceTimer = 59;
        bn = 0xFF;
        w->breakNo = bn;
    }
    w->espKind[0] = EspPullCoreKind();
    w->espKind[1] = EspPullCoreKind();
    w->espKind[2] = EspPullCoreKind();
    EstSet((int) em, -1, 0, 0, 0x2A, 4, 0, w->espKind[0], (u32) em, (void*) zero);
    w->mode = zero;
#line 1000 "D:/Bio4/Prog/em32.cpp"
    w->pMot = (MotionWorkSub*) MEM_ALLOC(0xD0, 1, 0xD);
    if (w->pMot) {
        memclr_asm(w->pMot, 0xD0);
    }
    p = em->getPartsPtr(0x53);
    p->scale.x = fzero;
    p->scale.y = fzero;
    p->scale.z = fzero;
    // hp = 500 and the routine bytes 6/1 are REG_EQUIV pseudos re-materialised by the original's reload
    // (r0, r11, r0 again): with no dying register the `sth hp` stays in source order ahead of the
    // routine stores. sched2 issues `li r0,1` after `stb r11,0xfd` (both priority 15; ours has one more
    // dependent on the stb) unless the `li` gets a dependent, see the asm below.
    {
        register int hp asm("r0"); // COMPILER-DIFF: #13
        register int six asm("r11"); // COMPILER-DIFF: #13
        register int one asm("r0"); // COMPILER-DIFF: #13
        hp = 500;
        em->hp = hp;
        six = 6;
        one = 1;
        em->xFC = one;
        em->xFD = six;
        em->xFE = zero;
        em->xFF = zero;
        MotionSetCore(em, &em->mot, ARC(9), 0, 0, 1, 0);
        // sched2's tie between `li r0,1` and `stb r11,0xfd` (both priority 15) is broken by dependent counts
        // (5 vs 6): the code-less read of `one` gives the `li` its sixth dependent so it is issued first like
        // the original's. Placed after the call so the anchor (priority 8, tied to the callee-saved `w`)
        // ranks below the argument `li`s and takes no issue slot of the block.
        asm("" : "+r"(w) : "r"(one)); // COMPILER-DIFF: #13 (codeless anchor)
    }
    MotionMoveF(em, 0);
    em32_R0_Move(em);
    if (w->pMot) {
        w->pMot->speedRate = 1.0f;
        MotionSetCore(em, w->pMot, ARC(0xAD), 0, 0, 4, 0);
    }
    OSReport("em32 free size = 0x%x\n", sizeof(Em32Work));
}

static void em32_R0_Move(cEm32* em)
{
    Em32_R1_move_tbl[em->xFD * 2](em);
    Em32_R1_move_tbl[em->xFD * 2 + 1](em);
}

static void em32_R1_br_Dummy(cEm32* em)
{
}

// The parasite shows: the invisible form ends, the effects of the first form are replaced.
static void em32_R1_Parasite(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;

    w->flags |= 0x4800;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x43), (int) ARC(0x44), 3, 1, 0);
        w->hit[25].flags |= 1;
        w->hit[26].flags |= 1;
        w->hit[27].flags |= 1;
        em32TexrenderInit(em);
        EstSet((int) em, -1, 0, 0, 0x2A, 5, 0, w->espKind[1], (u32) em, (void*) step);
        EM32_EFFECT_DELETE(w->espKind[0], em);
        EstSet((int) em, -1, 0, 0, 0x2A, 6, 0, w->espKind[0], (u32) em, (void*) step);
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        SndCall(8, 0x17, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->wait = 10;
            U8Set(w->mode, 1);
            EmRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    }
}

// The last form: the motion event turns the mode to 2.
static void em32_R1_LastMode(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;

    w->flags |= 0x800;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x7B), (int) ARC(0x7C), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x22, 0, 0, (u32) em, (void*) step);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->x978 = 450;
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em->motEvent & 1) {
            w->mode = 2;
        }
        break;
    }
}

static void em32_R1_2ndAppear(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero;

    w->flags |= 0x800;
    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        em->be_flag &= ~2;
        EM32_EFFECT_DELETE(w->espKind[0], em);
        em->xFE++;
    case 1:
        MotionSetCore(em, &em->mot, ARC(0x53), (int) ARC(0x54), 0, 5, 0);
        MotionMoveF(em, 0);
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        em->flags_3C8 &= ~1;
        em->xFE++;
    case 2:
        zero = 0;
        MotionSetCore(em, &em->mot, ARC(0x53), (int) ARC(0x54), 0, 5, 0);
        em32SetYarareMark(em, 1);
        EstSet((int) em, -1, 0, 0, 0x2A, 4, 1, w->espKind[0], (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x2A, 0xC, 1, w->espKind[0], (u32) em, (void*) zero);
        em->be_flag |= 2;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        }
        break;
    }
}

static void em32_R1_3rdAppear(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero;

    w->flags |= 0x800;
    em->dmType = 2;
    if (em->xFE == 0) {
        em->be_flag &= ~2;
        em->alpha = 0.0f;
        {
            u32 t = em->flags_3C8 ^ 1;
            zero = t & 1;
        }
        if (zero == 0) {
            em->flags_3C8 &= ~1;
            em32SetYarareMark(em, 1);
            em32GetJumpDownNo(em);
            EmRoutineSet(em, 1, 0x18, zero, 1);
        }
    }
}

static void em32_R1_3rdFall(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 0x800;
    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x79), 0, 0, 1, 0);
        em->be_flag |= 2;
        em->alpha = 1.0f;
        em32SetYarareMark(em, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->be_flag &= ~2;
            em->alpha = 0.0f;
            em->xFE++;
        }
        break;
    }
}

static void em32_R1_4thAppear(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 0x800;
    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        em->xFE++;
        em->be_flag &= ~2;
    case 1:
        MotionSetCore(em, &em->mot, ARC(0x7A), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        em->xFE++;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x7A), 0, 0, 1, 0);
        em32SetYarareMark(em, 1);
        EstSet((int) em, -1, 0, 0, 0x2A, 4, 1, w->espKind[0], (u32) em, 0);
        em->be_flag |= 2;
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        break;
    }
}

// Wait end: the next floor routine by the target angle, the form and chance.
static inline void em32NextWalkSet(cEm32* em, Em32Work* w, int r)
{
    if (w->targetAngAbs > 1.30899692f) {
        EmRoutineSet(em, 1, 0xC, r, r);
    } else if (w->mode == 2) {
        EmRoutineSet(em, 1, 0xB, r, r);
    } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
        EmRoutineSet(em, 1, 8, r, r);
    } else {
        EmRoutineSet(em, 1, 9, r, r);
    }
}

static void em32_R1_Wait(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int wait;

    w->flags |= 0x10;
    em32SetYarareMark(em, 1);
    switch (em->xFE) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(9), 0, 30, 5, 0);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x3E), 0, 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0xA), 0, 30, 5, 0);
            break;
        }
        w->flags |= 0x200;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (!(w->flags & 0x200)) {
            if (em->plDist2 < 64000000.0f) {
                w->flags |= 0x200;
            }
            if (em->plDist2 < 225000000.0f && w->routeAngAbs < 0.52359879f) {
                w->flags |= 0x200;
            }
        } else {
            wait = w->wait;
            if (wait == 0) {
                em32NextWalkSet(em, w, wait);
            }
        }
        break;
    }
    if (em->hp <= 1 && em->pos.x <= 25338.0f && em32JumpUpCk(em)) {
        return;
    }
    em32BreathSe(em);
}

static void em32_R1_Ambush(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int timer;
    int mode;

    w->flags |= 0x10;
    em32SetYarareMark(em, 1);
    switch (em->xFE) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(9), 0, 5, 5, 0);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x3E), 0, 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0xA), 0, 30, 5, 0);
            break;
        }
        w->timer = 15;
        w->flags &= ~1;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        timer = w->timer;
        if (timer) {
            w->timer--;
            break;
        }
        mode = w->mode;
        switch (mode) {
        case 0:
        default:
            if (em32AmbushAtkCk(em) == 0 && (w->flags & 1)) {
                f32 a = w->plAngAbs;
                if (a < 1.04719758f && w->plDist2 < 6250000.0f) {
                    if (Rnd() % 10 > 4 && w->plDist2 < 4000000.0f) {
                        EmRoutineSet(em, 1, 0xF, timer, timer);
                    } else {
                        EmRoutineSet(em, 1, 0x10, 0, 0);
                    }
                } else {
                    f32 ang = w->plAngAbs;
                    if (ang < 0.52359879f && w->plDist2 > 9000000.0f && w->plDist2 < 25000000.0f) {
                        EmRoutineSet(em, 1, 0x12, 0, 0);
                    }
                }
            }
            break;
        case 1:
            if (w->routeAngAbs < 0.52359879f && em->plDist2 < 9000000.0f && Rnd() % 10 > 4) {
                EmRoutineSet(em, mode, 0x1D, timer, timer);
            } else if (w->routeAngAbs < 0.785398185f && em->plDist2 < 9000000.0f) {
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            }
            break;
        case 2:
            break;
        }
        if (em32DeadCk(em)) {
            if ((w->flags & 0x1000) && em32StepUpCk2(em)) {
                break;
            }
            if (w->mode == 0 && Rnd() % 10 > 7 && em32JumpUpCk(em)) {
                break;
            }
            if ((w->x991 || Rnd() % 10 > 4) && em->plDist2 < 16000000.0f && em32StepUpCk2(em)) {
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    em32BreathSe(em);
}

// Floor routine tail shared by Walk / Dash: the step up checks, the rack break and the stuck check
// (a macro: the literal stays at its compare).
#define EM32_WALK_END_CK(em, w, lim)                                                 \
    {                                                                                \
        int zero;                                                                    \
                                                                                     \
        if (((w)->flags & 0x1000) && em32StepUpCk2(em)) {                            \
            return;                                                                  \
        }                                                                            \
        if ((em)->hp <= 1 && (em)->pos.x <= lim && em32JumpUpCk(em)) {               \
            return;                                                                  \
        }                                                                            \
        zero = em32StepUpCk3(em);                                                    \
        if (zero) {                                                                  \
            return;                                                                  \
        }                                                                            \
        em32RackBreakCk(em);                                                         \
        if ((w)->stuckCnt > 30 && (w)->mode != 0) {                                  \
            (w)->stuckCnt = zero;                                                    \
            if (Rnd() % 10 > 4) {                                                    \
                EmRoutineSet(em, 1, 0x20, zero, zero);                               \
                return;                                                              \
            }                                                                        \
        }                                                                            \
        em32BreathSe(em);                                                            \
    }

static void em32_R1_Walk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    PlArc* arc;
    int ret;
    int mode;

    w->flags |= 0x10;
    em32SetYarareMark(em, 1);
    switch (em->xFE) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            w->blendM0 = ARC(0xB);
            w->blendM3 = ARC(0xC);
            w->blendM1 = ARC(0xD);
            w->blendM2 = ARC(0xE);
            break;
        case 1:
            w->blendM0 = ARC(0x3F);
            w->blendM3 = ARC(0x40);
            w->blendM1 = ARC(0x41);
            w->blendM2 = ARC(0x42);
            break;
        case 2:
            w->blendM0 = ARC(0x3A);
            w->blendM3 = ARC(0x3B);
            w->blendM1 = ARC(0x3C);
            w->blendM2 = ARC(0x3D);
            break;
        }
        w->blendC = 5;
        w->blendCnt = 10;
        if (em->xFF) {
            w->blendCnt = 0;
        }
        w->blendSeq = 0;
        w->blendVal = 0.0f;
        em->xFE++;
    case 1:
        EM32_BLEND_TURN(em, w, &w->targetPos, 0.785398185f);
        em32BlendMotSetI(em, w->blendM0, w->blendM1, w->blendM2, w->blendM3, 0, 0, w->blendC);
        if (MotionMoveF(em, 0)) {
            if (em32StepUpCk(em)) {
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
                break;
            }
        }
        mode = w->mode;
        switch (mode) {
        case 0:
        default:
            if (em->pos.x > 7000.0f || (int) em->flags_3C8 < 0) {
                EmRoutineSet(em, 1, 0, 0, 0);
                break;
            }
            ret = em32AmbushAtkCk(em);
            if (ret) {
                break;
            }
            if (w->flags & 1) {
                if (w->plAngAbs < 1.04719758f && w->plDist2 < 6250000.0f) {
                    if (Rnd() % 10 > 4 && w->plDist2 < 4000000.0f) {
                        EmRoutineSet(em, 1, 0xF, ret, ret);
                    } else {
                        EmRoutineSet(em, 1, 0x10, 0, 0);
                    }
                    break;
                }
                {
                    int wait = w->longAtkWait;
                    if (wait == 0 && w->plAngAbs < 0.52359879f && w->plDist2 > 9000000.0f &&
                        w->plDist2 < 25000000.0f) {
                        EmRoutineSet(em, 1, 0x12, wait, wait);
                        break;
                    }
                }
            }
            if (w->atkWait == 0 && em->plDist2 > 36000000.0f) {
                if (Rnd() % 10 > 7) {
                    em32JumpUpCk(em);
                } else {
                    w->atkWait = Rnd() % 150 + 150;
                }
            }
            break;
        case 1:
            if (em->hp <= em->hpMax * 3 / 8 && (w->flags & 0x100000)) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            ret = em->flags_3C8 & 0x40000000;
            if (ret) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            if (w->routeAngAbs < 0.52359879f && em->plDist2 < 9000000.0f && Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x1D, ret, ret);
                break;
            }
            if (w->routeAngAbs < 0.785398185f && em->plDist2 < 9000000.0f) {
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            }
            break;
        case 2:
            if (w->routeAngAbs < 0.52359879f && em->plDist2 < 25000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            }
            break;
        }
        if (em->motEvent & 1) {
            em32AtkCk(em, 4, 0x54);
            em32AtkCk(em, 4, 0x55);
            em32AtkCk(em, 4, 0x56);
            em32AtkCk(em, 4, 0x57);
            em32AtkCk(em, 4, 0x58);
            em32AtkCk(em, 4, 0x59);
            em32AtkCk(em, 4, 0x5A);
            em32AtkCk(em, 4, 0x5B);
            em32AtkCk(em, 4, 0x5C);
            em32AtkCk(em, 4, 0x5D);
        }
        break;
    }
    EM32_WALK_END_CK(em, w, 25338.0f);
}

static void em32_R1_Dash(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int ret;
    int mode;
    int timer;

    w->flags |= 0x10;
    em32SetYarareMark(em, 1);
    switch (em->xFE) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            w->blendM0 = ARC(0x21);
            w->blendM3 = ARC(0x22);
            w->blendM1 = ARC(0x23);
            w->blendM2 = ARC(0x24);
            break;
        case 1:
            w->blendM0 = ARC(0x5D);
            w->blendM3 = ARC(0x5E);
            w->blendM1 = ARC(0x5F);
            w->blendM2 = ARC(0x60);
            break;
        case 2:
            w->blendM0 = ARC(0x30);
            w->blendM3 = ARC(0x31);
            w->blendM1 = ARC(0x32);
            w->blendM2 = ARC(0x33);
            break;
        }
        w->blendC = 5;
        w->blendCnt = 10;
        w->blendSeq = 0;
        w->timer = (Rnd() & 3) + 3;
        w->blendVal = 0.0f;
        em->xFE++;
    case 1:
        EM32_BLEND_TURN(em, w, &w->targetPos, 0.785398185f);
        em32BlendMotSetI(em, w->blendM0, w->blendM1, w->blendM2, w->blendM3, 0, 0, w->blendC);
        if (MotionMoveF(em, 0)) {
            timer = w->timer;
            if (timer == 0) {
                w->wait = 60;
                EmRoutineSet(em, 1, 6, timer, timer);
                break;
            }
            w->timer = timer - 1;
            if (em32StepUpCk(em)) {
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
                break;
            }
        }
        mode = w->mode;
        switch (mode) {
        case 0:
        default:
            if (em->pos.x > 7000.0f || (int) em->flags_3C8 < 0) {
                EmRoutineSet(em, 1, 0, 0, 0);
                break;
            }
            ret = em32AmbushAtkCk(em);
            if (ret) {
                break;
            }
            if (w->flags & 1) {
                f32 a = w->plAngAbs;
                if (a < 1.04719758f && w->plDist2 < 6250000.0f) {
                    if (Rnd() % 10 > 4 && w->plDist2 < 4000000.0f) {
                        EmRoutineSet(em, 1, 0xF, ret, ret);
                    } else {
                        EmRoutineSet(em, 1, 0x10, 0, 0);
                    }
                    break;
                }
                {
                    int wait = w->longAtkWait;
                    f32 ang = w->plAngAbs;
                    if (wait == 0 && ang < 0.52359879f && w->plDist2 > 9000000.0f &&
                        w->plDist2 < 25000000.0f) {
                        EmRoutineSet(em, 1, 0x12, wait, wait);
                        break;
                    }
                }
            }
            if (w->atkWait == 0 && em->plDist2 > 36000000.0f) {
                if (Rnd() % 10 > 7) {
                    em32JumpUpCk(em);
                } else {
                    w->atkWait = Rnd() % 150 + 150;
                }
            }
            break;
        case 1:
            if (em->hp <= em->hpMax * 3 / 8 && (w->flags & 0x100000)) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            ret = em->flags_3C8 & 0x40000000;
            if (ret) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            if (w->routeAngAbs < 0.52359879f && em->plDist2 < 9000000.0f && Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x1D, ret, ret);
                break;
            }
            if (w->routeAngAbs < 0.785398185f && em->plDist2 < 9000000.0f) {
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            }
            break;
        case 2:
            if (w->routeAngAbs < 0.52359879f && em->plDist2 < 25000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            }
            break;
        }
        break;
    }
    EM32_WALK_END_CK(em, w, 25338.0f);
}

static void em32_R1_Back(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(0x75), (int) ARC(0x76), 3, 1, 0);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x75), (int) ARC(0x76), 3, 1, 0);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0x61), (int) ARC(0x62), 3, 1, 0);
            break;
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2 && w->routeAngAbs < 0.52359879f && em->plDist2 < 25000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    em32BreathSe(em);
}

// Attack walk of the last form: the claws are out (parts 0x54..0x5D hit the player) and the step
// ends with a threat or a turn.
static void em32_R1_AtkWalk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;
    int zero;
    int hit;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        w->blendM0 = ARC(0x6D);
        w->blendM3 = ARC(0x6E);
        w->blendM1 = ARC(0x6F);
        w->blendM2 = ARC(0x70);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x25, 0, w->espKind[1], (u32) em, (void*) step);
        w->blendC = 1;
        w->blendCnt = 10;
        w->blendVal = 0.0f;
        w->blendSeq = step;
        w->x991 = step;
        em->xFE++;
    case 1:
        EM32_BLEND_TURN2(em, w, &w->targetPos, 0.785398185f);
        em32BlendMotSetI(em, w->blendM0, w->blendM1, w->blendM2, w->blendM3, 0, 0, w->blendC);
        if (MotionMoveF(em, 0)) {
            if (w->x991) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else {
                em->xFE++;
            }
        } else if (em->motEvent & 1) {
            EM32_TAIL_ATK(em, 4);
        }
        break;
    case 2:
        zero = 0;
        w->blendM0 = ARC(0x4F);
        w->blendM3 = ARC(0x50);
        w->blendM1 = ARC(0x51);
        w->blendM2 = ARC(0x52);
        w->blendC = 5;
        EstSet((int) em, -1, 0, 0, 0x2A, 0x26, 0, w->espKind[1], (u32) em, (void*) zero);
        w->blendCnt = 10;
        w->blendSeq = zero;
        w->x991 = zero;
        em->xFE++;
    case 3:
        EM32_BLEND_TURN2(em, w, &w->targetPos, 0.785398185f);
        em32BlendMotSetI(em, w->blendM0, w->blendM1, w->blendM2, w->blendM3, 0, 0, w->blendC);
        if (MotionMoveF(em, 0) || (em->motEvent & 4)) {
            if (w->x991) {
                goto threat;
            }
            GameAddPoint(0xB);
            hit = w->x991;
            if (hit) {
            threat:
                w->wait = 60;
                EM32_EFFECT_DELETE(w->espKind[1], em);
                EmRoutineSet(em, 1, 0xD, 0, 0);
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EM32_EFFECT_DELETE(w->espKind[1], em);
                EmRoutineSet(em, 1, 0xC, hit, hit);
                break;
            }
            if (w->stuckCnt > 30) {
                w->stuckCnt = hit;
                if (Rnd() % 10 > 4) {
                    EM32_EFFECT_DELETE(w->espKind[1], em);
                    EmRoutineSet(em, 1, 0x20, hit, hit);
                    break;
                }
            }
            if (Rnd() % 10 > 7) {
                EM32_EFFECT_DELETE(w->espKind[1], em);
                EmRoutineSet(em, 1, 0x1F, 0, 0);
                break;
            }
            if (pG->x4F88 <= 9) {
                if (Rnd() % 10 > 7) {
                    EM32_EFFECT_DELETE(w->espKind[1], em);
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                    break;
                }
                if (em->plDist2 > 64000000.0f) {
                    EM32_EFFECT_DELETE(w->espKind[1], em);
                    EmRoutineSet(em, 1, 8, 0, 0);
                    break;
                }
            }
            EstSet((int) em, -1, 0, 0, 0x2A, 0x26, 0, w->espKind[1], (u32) em, 0);
        }
        if (em->motEvent & 1) {
            EM32_TAIL_ATK(em, 4);
        }
        break;
    }
    em32RackBreakCk(em);
}

static void em32_R1_Turn(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            if (w->targetAngAbs < 2.35619450f) {
                if (w->targetAng < 0.0f) {
                    MotionSetCore(em, &em->mot, ARC(0x19), (int) ARC(0x1A), 10, 1, 0);
                } else {
                    MotionSetCore(em, &em->mot, ARC(0x17), (int) ARC(0x18), 10, 1, 0);
                }
            } else {
                MotionSetCore(em, &em->mot, ARC(0x1F), (int) ARC(0x20), 10, 1, 0);
            }
            break;
        case 1:
            if (w->targetAngAbs < 2.35619450f) {
                if (w->targetAng < 0.0f) {
                    MotionSetCore(em, &em->mot, ARC(0x67), (int) ARC(0x68), 10, 1, 0);
                } else {
                    MotionSetCore(em, &em->mot, ARC(0x65), (int) ARC(0x66), 10, 1, 0);
                }
            } else {
                MotionSetCore(em, &em->mot, ARC(0x63), (int) ARC(0x64), 10, 1, 0);
            }
            break;
        case 2:
            if (w->targetAngAbs < 2.35619450f) {
                if (w->targetAng < 0.0f) {
                    MotionSetCore(em, &em->mot, ARC(0x38), (int) ARC(0x39), 10, 1, 0);
                } else {
                    MotionSetCore(em, &em->mot, ARC(0x36), (int) ARC(0x37), 10, 1, 0);
                }
            } else {
                MotionSetCore(em, &em->mot, ARC(0x34), (int) ARC(0x35), 10, 1, 0);
            }
            break;
        }
        w->x991 = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->x991) {
                w->wait = 60;
                EmRoutineSet(em, 1, 6, 0, 0);
            } else if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2 && w->routeAngAbs < 0.52359879f && em->plDist2 < 25000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    em32RackBreakCk(em);
    em32BreathSe(em);
}

// Attack end: the step up / jump up checks (a macro: the literal stays at its compare, an inline's
// f32 parameter would become a pseudo hoisted into a callee-saved FPR).
#define EM32_ATK_END_CK(em, w, near, jc)                                                \
    if (((w)->flags & 0x1000) && em32StepUpCk2(em)) {                                   \
        return;                                                                         \
    }                                                                                   \
    if ((w)->mode == 0 && Rnd() % 10 > jc && em32JumpUpCk(em)) {                         \
        return;                                                                         \
    }                                                                                   \
    if (((w)->x991 || Rnd() % 10 > 4) && (em)->plDist2 < near && em32StepUpCk2(em)) {    \
        return;                                                                         \
    }

static void em32_R1_Threat(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;
    int mode;
    int ret;

    w->flags |= 0x10;
    switch (step) {
    case 0: {
        int hokan = 10;

        AtariOn(&em->atari, 0x300);
        if (em->xFF) {
            w->flags |= 0x40;
            hokan = 0;
        }
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(0x4D), (int) ARC(0x4E), hokan, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2A, 9, 0, w->espKind[2], (u32) em, (void*) step);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x3E), 0, hokan, 5, 0);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0x83), (int) ARC(0x84), hokan, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2A, 0x23, 0, w->espKind[2], (u32) em, (void*) step);
            break;
        }
        em->xFE++;
    }
    case 1:
        ret = MotionMoveF(em, 0);
        if (ret) {
            mode = w->mode;
            switch (mode) {
            case 0:
            default:
                if ((w->flags & 1) && w->plAngAbs < 1.04719758f && w->plDist2 < 6250000.0f) {
                    if (Rnd() % 10 > 4 && w->plDist2 < 4000000.0f) {
                        EmRoutineSet(em, 1, 0xF, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0x10, 0, 0);
                    }
                    return;
                }
                break;
            case 1:
                if (w->routeAngAbs < 0.52359879f && em->plDist2 < 9000000.0f && Rnd() % 10 > 4) {
                    EmRoutineSet(em, mode, 0x1D, 0, 0);
                    return;
                }
                if (w->routeAngAbs < 0.785398185f && em->plDist2 < 9000000.0f) {
                    EmRoutineSet(em, 1, 0x1C, 0, 0);
                    return;
                }
                break;
            case 2:
                if (Rnd() % 10 > 7) {
                    EmRoutineSet(em, 1, 0x1F, 0, 0);
                    return;
                }
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                if (w->routeAngAbs < 0.52359879f && em->plDist2 < 25000000.0f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                }
            } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        } else if (w->targetAngAbs > 1.30899692f && em->plDist2 > 12250000.0f) {
            EmRoutineSet(em, 1, 0xC, ret, ret);
        }
        break;
    }
    if (em->hp <= 1 && em->pos.x <= 25338.0f) {
        em32JumpUpCk(em);
    }
}

// The ambush swipe: a quarter turn to the side the player is on, the claws hit during the motion.
static void em32_R1_AmbushAtk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    f32 ang;

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            MotionSetCore(em, &em->mot, ARC(0x4B), (int) ARC(0x4C), 10, 1, 0);
            w->turnAng = em->rot.y - 1.57079637f;
        } else {
            MotionSetCore(em, &em->mot, ARC(0x49), (int) ARC(0x4A), 10, 1, 0);
            w->turnAng = em->rot.y + 1.57079637f;
        }
        w->turnAng = LIMIT_ANGLE(w->turnAng);
        w->x991 = 0;
        w->timer2 = 24;
        w->timer3 = 0;
        em->xFE++;
    case 1:
        if (em->motEvent & 8) {
            ang = Muku(&em->pos, &w->targetPos, w->turnAng, 0.0981747732f);
            w->turnAng += ang;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->rot.y += ang;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x991 == 0) {
                GameAddPoint(0xB);
            }
            EM32_ATK_END_CK(em, w, 16000000.0f, 4);
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (em->plDist2 < 25000000.0f) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else {
            if (em->motEvent & 1) {
                EM32_CLAW_ATK(em, 0);
            }
            if (w->timer3) {
                w->timer3--;
            } else if (w->timer2 && w->x991 == 0 && em->plDist2 < 36000000.0f) {
                w->timer2--;
                ActBtn.set(0x25, 0xB, (int) em32SitAction, (int) em, 1, 3, 0, w->x991);
            }
        }
        break;
    }
}

static void em32_R1_Atk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x45), (int) ARC(0x46), 15, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2A, 0xB, 0, w->espKind[2], (u32) em, (void*) step);
        w->x991 = step;
        w->timer2 = 30;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->x991 == 0) {
                GameAddPoint(0xB);
            }
            EM32_ATK_END_CK(em, w, 16000000.0f, 7);
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if ((em->plDist2 < 25000000.0f && pG->x4F88 <= 9) || w->x991) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em->motEvent & 1) {
            EM32_CLAW_ATK(em, 0);
        }
        break;
    }
}

static void em32_R1_br_Catch(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (em->hp > 0 && em->xFE != 0) {
        int lost = !(w->flags & 1);
        if (lost == 0 && (w->flags & 0x80000) && (em->motEvent & 1)) {
            f32 ang = GetXZAngle(&pPL->pos, &em->pos);
            fabsf(Muku2(pPL->rot.y, ang, 3.14159274f));
            EM32_CLAW_ATK(em, 1);
            if (w->x991) {
                em->stat = 0x01110000;
            }
        }
    }
}

static void em32_R1_Catch(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x13), (int) ARC(0x14), 15, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x15, 0, w->espKind[2], (u32) em, (void*) step);
        w->x991 = step;
        w->timer2 = 30;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            EM32_ATK_END_CK(em, w, 16000000.0f, 7);
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if ((em->plDist2 < 25000000.0f && pG->x4F88 <= 9) || w->x991) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
}

// The player is caught in the claws: shaken (the button mash frees him), then bitten or thrown.
static void em32_R1_CatchHit(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;
    int dead;

    w->flags |= 0x800;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x2B), 0, 0, 1, 0);
        PlSetDamageSe(0);
        EmCatchPLSet(em, 3.14159274f, 1, (int) plem32_CatchHit, -244.559998f, 0.0f, -1746.93994f);
        GameAddPoint(2);
        PlGachaInit();
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
        w->sndId2 = SndCall(8, 0x15, &em->pos, em->id, 0, em);
        w->timer = step;
        w->timer2 = step;
        w->x10 = step;
        em->dmg.set(0, 0);
        em->xFE++;
    case 1:
        if (EmCatchMotionMove(em, 1.0f, 1.0f)) {
            em->dmType = 2;
            em->xFE = 4;
            break;
        }
        if (w->timer2) {
            w->timer2--;
        } else {
            w->timer2 = 4;
            EstSet((int) em, -1, 0, 0, 0x2A, 7, 0, 0, (u32) em, 0);
        }
        if (em->frame > 9.69999981f && em->frame < 10.3000002f) {
            w->x10 = SndCall(8, 0xE, &em->pos, em->id, 0, em);
        }
        if (em->frame > 29.7000008f && em->frame < 30.2999992f) {
            SndCall(8, 0x12, &pPL->pos, em->id, 0, pPL);
        }
        LifeDownSet2(pPL, 20, 0, 1);
        PlGachaMove();
        if ((s16) pG->pl_life > 1 && (u32) PlGachaGet() > 30) {
            em->xFE = 2;
            break;
        }
        dead = em32DeadCk(em);
        if (dead) {
            em->xFE = 2;
            break;
        }
        if (em32BetweenHitCk(em)) {
            SndStop(w->x10, 0);
            SndStop(w->sndId2, 0);
            em->xFC = 2;
            em->xFD = dead;
            em->xFE = dead;
            em->xFF = dead;
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x2C), (int) ARC(0x2D), 0, 1, 0);
        EmCatchPLSet(em, 3.14159274f, 1, (int) plem32_CatchHit, 244.399994f, 0.0f, -1558.33997f);
        pPL->xFE = step;
        w->timer = 15;
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
            EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            AtariOn(&em->atari, 0x300);
            if (MotionMoveF(em, 0)) {
                if ((w->flags & 0x1000) && em32StepUpCk2(em)) {
                    break;
                }
                if (w->mode == 0 && Rnd() % 10 > 7 && em32JumpUpCk(em)) {
                    break;
                }
                if (Rnd() % 10 > 4 && em->plDist2 < 16000000.0f && em32StepUpCk2(em)) {
                    break;
                }
                if (w->targetAngAbs > 1.30899692f) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                } else {
                    w->wait = 60;
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                }
                break;
            }
        }
        if (em->motEvent & 1) {
            SndStop(w->x10, 0);
            SndStop(w->sndId2, 0);
        }
        break;
    case 4:
        MotionSetCore(em, &em->mot, ARC(0x2E), (int) ARC(0x2F), 0, 1, 0);
        EmCatchPLSet(em, 3.14159274f, 1, (int) plem32_CatchHit, 0.0f, 0.0f, -1456.30005f);
        pPL->xFE = step;
        pG->pl_life = 0;
        em->xFE++;
    case 5:
        em->dmType = 2;
        EmCatchMotionMove(em, 1.0f, 1.0f);
        break;
    }
    em->x3A8 = em->pos;
}

static void plem32_CatchHit(cPlayer* pl)
{
    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x89), 0, 0, 1, 0);
        PlSetFace(1);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if ((((cEm32*) pPL->dmgType)->stat & 0xFFFF0000) == 0x01110000) {
            break;
        }
        goto end;
    case 2:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x8A), 0, 0, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x2A, 0x29, 0, 0, (u32) pl, 0);
        pl->x3E0 = 15;
        pl->xFE++;
    case 3:
        if (pl->x3E0) {
            pl->x3E0--;
            EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else if (MotionMoveF(pl, 0)) {
            AtariOn(&pl->atari, 0x300);
        end:
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 4:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x8B), 0, 0, 1, 0);
        pl->xFE++;
    case 5:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (pl->frame > 39.7000008f && pl->frame < 40.2999992f) {
            VibSetData(VIB_TBL, 0xB, 1);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// The long reach attack: a lunge at the player (escape prompt while he is in front).
static void em32_R1_LongAtk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;
    int hit;
    Mtx inv;
    Vec lp;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x47), (int) ARC(0x48), 10, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2A, 0xA, 0, w->espKind[2], (u32) em, (void*) step);
        EM32_W_FRESH(w);   // COMPILER-DIFF #12
        em32AtkHitSet(w, step);
        IntSet(w->longAtkWait, 600);
        IntSet(w->timer2, 25);
        IntSet(w->timer3, 15);
        IntSet(w->timer, 10);
        if (pG->x4F88 <= 3) {
            IntSet(w->timer2, 27);
            IntSet(w->timer3, 13);
        }
        if (pG->x4F88 <= 1) {
            IntSet(w->timer3, 10);
            IntSet(w->timer2, 30);
        }
        if (pG->x4F88 > 6) {
            IntSet(w->timer2, 22);
            IntSet(w->timer3, 18);
        }
        if (pG->x4F88 > 9) {
            IntSet(w->timer2, 20);
            IntSet(w->timer3, 20);
        }
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.196349546f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x991 == 0) {
                GameAddPoint(0xB);
            }
            EM32_ATK_END_CK(em, w, 16000000.0f, 7);
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if ((em->plDist2 < 25000000.0f && pG->x4F88 <= 9) || w->x991) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else {
            if (em->motEvent & 1) {
                EM32_CLAW_ATK(em, 2);
            }
            if (w->timer3) {
                w->timer3--;
            } else if (w->timer2) {
                hit = w->x991;
                if (hit == 0) {
                    w->timer2--;
                    PSMTXInverse(em->mat, inv);
                    PSMTXMultVec(inv, &pPL->pos, &lp);
                    if (lp.x > -1000.0f && lp.x < 1000.0f && lp.z > -500.0f && lp.z < 7000.0f) {
                        ActBtn.set(0x25, 0xB, (int) em32EscapeAction, (int) em, 1, 3, 0, hit);
                    }
                }
            }
        }
        break;
    }
}

static void em32SitAction(cEm32* em)
{
    SetPlDamage((int) em, plemSit);
    GameAddPoint(9);
}

static void em32SitUpAction(cEm32* em)
{
    SetPlDamage((int) em, plemSit);
    pPL->xFF = 1;
    GameAddPoint(9);
}

// Player ducks under the swipe (xFF: gets up again).
static void plemSit(cPlayer* pl)
{
    f32 ang;

    pl->subArc = PL_EM(pl)->subArc;
    pl->dmg.set(0, 30);
    switch (pl->xFE) {
    case 0:
        ang = fabsf(Muku(&pl->pos, &PL_EM(pl)->pos, pl->rot.y, 3.14159274f));
        if (pl->xFF) {
            if (ang < 1.57079637f) {
                MotionSetCore(pl, &pl->mot, PL_ARC(0x8F), 0, 3, 1, 0);
            } else {
                MotionSetCore(pl, &pl->mot, PL_ARC(0x8E), 0, 3, 1, 0);
            }
        } else {
            if (ang < 1.57079637f) {
                MotionSetCore(pl, &pl->mot, PL_ARC(0x90), 0, 3, 1, 0);
            } else {
                MotionSetCore(pl, &pl->mot, PL_ARC(0x91), 0, 3, 1, 0);
            }
        }
        GameAddPoint(0xB);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em32BackjumpAction(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->actionSet = 1;
    SetPlDamage((int) em, plemBackjump);
    GameAddPoint(9);
}

// Player jumps back out of the tunnel attack, facing away from the enemy.
static void plemBackjump(cPlayer* pl)
{
    int fe;
    f32 ry;
    f32 d;

    pl->subArc = PL_EM(pl)->subArc;
    fe = pl->xFE;
    pl->dmg.x1 = 0x1E;
    switch (fe) {
    case 0:
        ry = LIMIT_ANGLE(PL_EM(pl)->rot.y + 1.57079637f);
        d = fabsf(Muku2(pl->rot.y, ry, 3.14159274f));
        if (d < 0.785398185f) {
            pl->rot.y = ry;
        }
        if (d > 2.35619450f) {
            pl->rot.y = ry + 3.14159274f;
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        MotionSetCore(pl, &pl->mot, PL_ARC(0x94), 0, 3, 1, 0);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, (void*) fe);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        GameAddPoint(0xB);
        pl->x3E0 = 45;
        pl->x3E4 = fe;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
        } else if (Key.on & 0x1F) {
            pl->x3E4 = 1;
        }
        if (pl->frame > 10.6999998f && pl->frame < 11.3000002f) {
            SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 21.7000008f && pl->frame < 22.2999992f) {
            SndCall(5, 0x14, &pl->pos, 0, 0, pl);
        }
        if ((pl->frame > 36.7000008f && pl->frame < 37.2999992f) ||
            (pl->frame > 49.7000008f && pl->frame < 50.2999992f)) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if ((pl->frame > 37.7000008f && pl->frame < 38.2999992f) ||
            (pl->frame > 50.7000008f && pl->frame < 51.2999992f)) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0) || pl->x3E4) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em32EscapeAction(cEm32* em)
{
    SetPlDamage((int) em, plemEscape);
    GameAddPoint(9);
}

// Player escapes the lunge with a side roll (away from the wall when one is near).
static void plemEscape(cPlayer* pl)
{
    Vec a;
    Vec b;
    int side;

    pl->subArc = PL_EM(pl)->subArc;
    pl->dmg.x1 = 0x1E;
    switch (pl->xFE) {
    case 0:
        // The side from the angle is overridden by the random pick right after (dead in the
        // original too: only the compare's 0.0 load survives, hoisted before Rnd).
        if (Muku(&PL_EM(pl)->pos, &pl->pos, pl->rot.y, 3.14159274f) > 0.0f) {
            side = 1;
        } else {
            side = 0;
        }
        side = Rnd() & 1;
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 2000.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        PSMTXMultVec(pl->mat, &a, &a);
        PSMTXMultVec(pl->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            side = 1;
        }
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = -2000.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        PSMTXMultVec(pl->mat, &a, &a);
        PSMTXMultVec(pl->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            side = 0;
        }
        if (side) {
            MotionSetCore(pl, &pl->mot, PL_ARC(0x92), (int) PL_ARC(0x93), 3, 1, 0);
        } else {
            MotionSetCore(pl, &pl->mot, PL_ARC(0x92), (int) PL_ARC(0x93), 3, 0x41, 0);
        }
        GameAddPoint(0xB);
        SndCall(1, 0x48, &pl->pos, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        pl->x3E0 = 50;
        pl->x3E4 = 15;
        pl->xFE++;
    case 1:
        em32EscapeCamMove(PL_EM(pl));
        if (pl->frame > 11.6999998f && pl->frame < 12.3000002f) {
            EstSet(0, -1, &pl->pos, 0, 3, 0x13, 0, 0, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (pl->x3E4) {
            pl->rot.y += Muku(&pl->pos, &PL_EM(pl)->pos, pl->rot.y, 0.392699093f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        if (MotionMoveF(pl, 0)) {
            pl->x3E0 = 0;
        }
        if (pl->x3E0) {
            pl->x3E0--;
        } else {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Escape camera: a fixed view behind the player, pulled in front of the nearest wall.
void em32EscapeCamMove(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Camera* cam = &pG->Cam;
    Vec pos;
    Vec at;
    Vec hit;
    Vec d;
    f32 len;

    w->cam.param.fovy = cam->param.fovy;
    pos.x = -376.0f;
    pos.y = 575.0f;
    pos.z = -1831.0f;
    at.x = -244.0f;
    at.y = 809.0f;
    at.z = 52.5999985f;
    PSMTXMultVec(pPLS->mat, &pos, &pos);
    PSMTXMultVec(pPLS->mat, &at, &at);
    PosToPos(&cam->param.at, &at, &w->cam.param.at, 1.0f);
    PosToPos(&cam->param.pos, &pos, &w->cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &w->cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 3617 "D:/Bio4/Prog/em32.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->cam.param.at, &d, &w->cam.param.pos);
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    w->cam.dist = SQRTF((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
                        (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
                        (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z));
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

// Step up onto a container: the run-up towards stepPos, then the tunnel attack / wait / step down.
static void em32_R1_StepUp(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec v;
    Vec* pos;
    int ret;

    w->flags |= 0x940;
    switch (em->xFE) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(0x1B), (int) ARC(0x1C), 10, 1, 0);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x57), (int) ARC(0x58), 10, 1, 0);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0x57), (int) ARC(0x58), 10, 1, 0);
            break;
        }
        PSVECSubtract(&w->stepPos, &em->pos, &w->spd);
        w->spd.y = 0.0f;
#line 3669 "D:/Bio4/Prog/em32.cpp"
        VECNormalize(&w->spd, &v);
        PSVECScale(&v, &v, 1000.0f);
        PSVECAdd(&w->spd, &v, &w->spd);
        em->xFE++;
    case 1:
        if (em->motEvent & 1) {
            if (w->flags & 0x1000) {
                pos = &em->pos;
                em->rot.y = LIMIT_ANGLE(em->rot.y += Muku2(em->rot.y, w->stepAng, 0.392699093f));
            } else {
                em->rot.y = LIMIT_ANGLE(em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.392699093f));
                pos = &em->pos;
            }
            PSVECScale(&w->spd, &v, 0.1f);
            PSVECAdd(pos, &v, pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (MotionMoveF(em, 0)) {
            w->flags |= 0x400;
            if (em32TunnelAtkCk(em)) {
                break;
            }
            if (em->xFF) {
                EmRoutineSet(em, 1, 0x14, 0, 0);
            } else if (em32PlInTunnelCk(em)) {
                EmRoutineSet(em, 1, 0x14, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x15, 0, 0);
            }
        }
        break;
    }
}

static void em32_R1_StepWait(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int timer;

    w->flags |= 0x940;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(9), 0, 10, 1, 0);
        w->timer = 90;
        em->xFE++;
    case 1:
        w->flags |= 0x400;
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        }
        if ((em->xFF == 0 || w->timer == 0) && em32TunnelAtkCk(em)) {
            break;
        }
        if (em32PlInTunnelCk(em)) {
            break;
        }
        timer = w->timer;
        if (timer == 0) {
            em->xFC = 1;
            em->xFD = 0x15;
            em->xFE = timer;
            em->xFF = timer;
        }
        break;
    }
    em32BreathSe(em);
}

static void em32_R1_StepDown(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;
    Vec v;
    Vec* pos;
    int flag;

    w->flags |= 0x940;
    switch (step) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(0x1D), (int) ARC(0x1E), 10, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2A, 0x18, 0, 0, (u32) em, (void*) step);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x59), (int) ARC(0x5A), 10, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2A, 0x19, 0, 0, (u32) em, (void*) step);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0x59), (int) ARC(0x5A), 10, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2A, 0x19, 0, 0, (u32) em, (void*) step);
            break;
        }
        em32GetStepDownPos(em);
        PSVECSubtract(&w->stepPos, &em->pos, &w->spd);
        w->spd.y = 0.0f;
        em->xFE++;
    case 1:
        if (em->motEvent & 1) {
            if (w->flags & 0x1000) {
                pos = &em->pos;
                em->rot.y = LIMIT_ANGLE(em->rot.y += Muku2(em->rot.y, w->stepAng, 0.392699093f));
            } else {
                em->rot.y = LIMIT_ANGLE(em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.392699093f));
                pos = &em->pos;
            }
            PSVECScale(&w->spd, &v, 0.3f);
            PSVECAdd(pos, &v, pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (MotionMoveF(em, 0)) {
            w->flags &= ~0x400;
            flag = w->flags & 0x1000;
            if (flag) {
                w->flags &= ~0x1000;
                EmRoutineSet(em, 1, 7, 0, 0);
            } else {
                w->atkWait = Rnd() % 150 + 150;
                if (w->targetAngAbs > 1.30899692f) {
                    EmRoutineSet(em, 1, 0xC, flag, flag);
                } else if (w->mode == 2) {
                    EmRoutineSet(em, 1, 0xB, flag, flag);
                } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
                    EmRoutineSet(em, 1, 8, flag, flag);
                } else {
                    EmRoutineSet(em, 1, 9, flag, flag);
                }
            }
        }
        break;
    }
}

// The tunnel attack from the container roof: a roar (back jump prompt), then the claw sweep.
static void em32_R1_TunnelAtk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero;
    int act;

    w->flags |= 0xC50;
    switch (em->xFE) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(0x4D), 0, 10, 5, 0);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x3E), 0, 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0xA), 0, 30, 5, 0);
            break;
        }
        w->actionSet = 0;
        w->x991 = 0;
        IntSet(w->timer, 15);
        if (pG->x4F88 <= 3) {
            IntSet(w->timer2, 18);
        }
        if (pG->x4F88 <= 1) {
            IntSet(w->timer2, 20);
        }
        if (pG->x4F88 > 6) {
            IntSet(w->timer2, 12);
        }
        if (pG->x4F88 > 9) {
            IntSet(w->timer2, 10);
        }
        w->x10 = Rnd() & 1;
        em->xFE++;
    case 1:
        act = w->actionSet;
        if (act) {
            w->timer = 0;
        } else {
            switch (w->x10) {
            case 0:
            default:
                ActBtn.set(0x25, 0xB, (int) em32BackjumpAction, (int) em, 2, 3, 0, act);
                break;
            case 1:
                ActBtn.set(0x25, 0xB, (int) em32BackjumpAction, (int) em, 2, 4, 0, act);
                break;
            }
            if (w->actionSet) {
                w->timer = 0;
            }
        }
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 2:
        zero = 0;
        MotionSetCore(em, &em->mot, ARC(0x55), (int) ARC(0x56), 10, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x21, 0, 0, (u32) em, (void*) zero);
        w->x991 = zero;
        w->timer = 1;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x14, 0, 1);
            break;
        }
        if ((em->motEvent & 4) && w->x991 == 0 && em->xFF == 0) {
            w->breakNo2 = w->pPoint->no;
            if (em32TunnelAtkCk(em)) {
                em->xFF = 1;
                break;
            }
        }
        if (w->timer && w->x991 == 0 && w->actionSet == 0) {
            switch (w->x10) {
            case 0:
            default:
                ActBtn.set(0x25, 0xB, (int) em32BackjumpAction, (int) em, 2, 3, 0, 0);
                break;
            case 1:
                ActBtn.set(0x25, 0xB, (int) em32BackjumpAction, (int) em, 2, 4, 0, 0);
                break;
            }
        }
        if (em->motEvent & 1) {
            EM32_TAIL_ATK(em, 5);
            w->timer = 0;
        }
        break;
    }
}

// Jump up onto the container roof (pPoint: the jump point).
static void em32_R1_JumpUp(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec v;
    Em32Point* pt;

    w->flags |= 0x940;
    switch (em->xFE) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(0xF), (int) ARC(0x10), 10, 1, 0);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x73), (int) ARC(0x74), 10, 1, 0);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0x73), (int) ARC(0x74), 10, 1, 0);
            break;
        }
        PSVECSubtract(&w->pPoint->pos, &em->pos, &w->spd);
        w->spd.y = 0.0f;
        em->xFE++;
    case 1:
        if (em->motEvent & 4) {
            PSVECScale(&w->spd, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (em->motEvent & 1) {
            pt = w->pPoint;
            if (pt->flag == 0) {
                pt->flag = 1;
                w->breakNo = w->pPoint->no;
                EstSet(0, -1, &w->pPoint->pos, 0, 1, 1, 0, 0, 0, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            w->flags |= 0x80;
            EmRoutineSet(em, 1, 0x19, 0, 0);
        }
        break;
    }
}

// Jump down from the roof (pPoint: the landing point, or straight down).
static void em32_R1_JumpDown(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step;
    Vec v;
    Em32Point* pt;
    int ret;

    w->flags |= 0x940;
    em32SetYarareMark(em, 1);
    step = em->xFE;
    switch (step) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(0x11), (int) ARC(0x12), 10, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2A, 0x16, 0, 0, (u32) em, (void*) step);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0xA9), (int) ARC(0xAA), 10, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2A, 0x17, 0, 0, (u32) em, (void*) step);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0xA9), (int) ARC(0xAA), 10, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2A, 0x17, 0, 0, (u32) em, (void*) step);
            break;
        }
        EstSet((int) em, -1, 0, 0, 0x2A, 4, 0, w->espKind[0], (u32) em, 0);
        if (w->pPoint) {
            PSVECSubtract(&w->pPoint->pos, &em->pos, &w->spd);
        } else {
            w->spd.x = 0.0f;
            w->spd.y = 0.0f;
            w->spd.z = 0.0f;
        }
        em->be_flag |= 2;
        w->spd.y = 0.0f;
        em->xFE++;
    case 1:
        em->alpha += 0.1f;
        if (em->alpha > 1.0f) {
            em->alpha = 1.0f;
        }
        if (em->motEvent & 4) {
            PSVECScale(&w->spd, &v, 0.15f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (em->motEvent & 1) {
            pt = w->pPoint;
            if (pt && pt->flag == 0) {
                pt->flag = 1;
                w->breakNo = w->pPoint->no;
                EstSet(0, -1, &w->pPoint->pos, 0, 1, 0, 0, 0, 0, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            w->flags &= ~0x80;
            w->atkWait = Rnd() % 150 + 150;
            ret = em->xFF;
            if (ret) {
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, ret, ret);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, ret, ret);
            } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
                EmRoutineSet(em, 1, 8, ret, ret);
            } else {
                EmRoutineSet(em, 1, 9, ret, ret);
            }
        }
        break;
    }
}

// On the container roof: the wait before the ceiling attack (the player hides in the tunnel).
static void em32_R1_C_Wait(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec v;
    int timer;

    em->dmType = 2;
    w->flags |= 0x840;
    em32SetYarareMark(em, 0);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0xF), 0, 10, 1, 0);
        w->timer = 15;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        timer = w->timer;
        if (timer) {
            w->timer--;
            break;
        }
        em->alpha -= 0.1f;
        if (em->alpha < 0.0f) {
            em->alpha = 0.0f;
            em->be_flag &= ~2;
            if (em->hp <= 1) {
                em->hp = timer;
            }
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(9), 0, 10, 1, 0);
        EM32_EFFECT_DELETE(w->espKind[0], em);
        EM32_W_FRESH(w);   // COMPILER-DIFF #12
        IntSet(w->timer, Rnd() % 30 + 60);
        if (pG->x4F88 <= 3) {
            w->timer = Rnd() % 60 + 90;
        }
        if (pG->x4F88 <= 1) {
            w->timer = Rnd() % 60 + 120;
        }
        if (pG->x4F88 > 6) {
            w->timer = Rnd() % 60 + 45;
        }
        if (pG->x4F88 > 9) {
            w->timer2 = Rnd() % 60 + 30;
        }
        em->xFE++;
    case 3:
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 2000.0f;
        PSMTXMultVec(pPL->mat, &v, &em->pos);
        F32Set(em->pos.y, pPL->pos.y + 6000.0f);
        em->rot.y = pPLS->rot.y + 3.14159274f;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (em->hp <= 0) {
            break;
        }
        {
            int t = w->timer;
            if (t > 44 && (pPL->xFC != 0 || (u32) (pPL->xFD - 1) > 2)) {
            } else if (t) {
                w->timer = t - 1;
            } else {
                // The countdown at zero skips the reload test (the shared test block is reached by the
                // hold path and the decrement path only).
                goto ceiling;
            }
        }
        if (w->timer) {
            break;
        }
    ceiling:
        if (em32CeilingAtkCk(em)) {
            break;
        }
        em32JumpDownCk(em);
        break;
    }
    em32BreathSe(em);
}

static void em32_R1_br_C_Atk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (em->hp > 0 && em->xFE != 0) {
        if (em->motEvent & 1) {
            EM32_CLAW_ATK(em, 3);
        }
        if (w->x991) {
            em->stat = 0x011B0000;
        }
    }
}

// The ceiling attack: the drop from the roof onto the player (sit prompt while he is in front).
static void em32_R1_C_Atk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step;
    Vec v;
    Em32Point* pt;
    int x10;

    w->flags |= 0x840;
    em32SetYarareMark(em, 0);
    step = em->xFE;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x15), (int) ARC(0x16), 3, 1, 0);
        PSVECSubtract(&w->pPoint->pos, &em->pos, &w->spd);
        EM32_W_SET(w, int, timer2, 25);   // COMPILER-DIFF #12 (scalar w-based stores, see EM32_W_SET)
        EM32_W_SET(w, f32, spd.y, 0.0f);
        EM32_W_SET(w, u8, x991, step);
        EM32_W_SET(w, int, timer, step);
        em32Timer2SetW(w, 37, 30, 23, 20);
        em->alpha = 0.0f;
        em->be_flag |= 2;
        EstSet((int) em, -1, 0, 0, 0x2A, 0x12, 0, 0, (u32) em, (void*) step);
        w->x10 = Rnd() & 1;
        if (pGS->x4F88 <= 3) {
            w->x10 = step;
        }
        em->xFE++;
    case 1:
        em->alpha += 0.1f;
        if (em->alpha > 1.0f) {
            em->alpha = 1.0f;
        }
        if (em->motEvent & 4) {
            PSVECScale(&w->spd, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (em->motEvent & 2) {
            pt = w->pPoint;
            if (pt->flag == 0) {
                pt->flag = 1;
                w->breakNo = w->pPoint->no;
                EstSet(0, -1, &w->pPoint->pos, 0, 1, 0, 0, 0, 0, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            if ((w->x991 || (Rnd() & 1)) && em32JumpDownCk(em)) {
                return;
            }
            EmRoutineSet(em, 1, 0x19, 0, 0);
        } else if (w->timer) {
            w->timer--;
        } else if (w->timer2 && w->x991 == 0 && w->routeAngAbs < 1.57079637f && em->plDist2 < 16000000.0f) {
            x10 = w->x10;
            w->timer2--;
            switch (x10) {
            case 0:
            default:
                ActBtn.set(0x13, 0xB, (int) em32SitUpAction, (int) em, 1, 3, 0, w->x991);
                break;
            case 1:
                ActBtn.set(0x13, 0xB, (int) em32SitUpAction, (int) em, 1, 4, 0, w->x991);
                break;
            }
        }
        break;
    }
}

// The player is caught from the ceiling: bitten (the head comes off) unless he shakes free.
static void em32_R1_C_AtkHit(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step;

    w->flags |= 0x840;
    em->dmg.set(0, 2);
    em32SetYarareMark(em, 0);
    step = em->xFE;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x27), (int) ARC(0x28), 0, 1, 0);
        PlSetDamageSe(0);
        EmCatchPLSet(em, 3.14159274f, 1, (int) plem32_C_AtkHit, -83.8300018f, 0.0f, -2411.40991f);
        GameAddPoint(2);
        PlGachaInit();
        AtariOff(&em->atari, 0xFCFF);
        EstSet((int) em, -1, 0, 0, 0x2A, 0xD, 0, 0, (u32) em, (void*) step);
        EstSet((int) pPL, -1, 0, 0, 0x2A, 0x14, 0, w->espKind[1], (u32) em, (void*) step);
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
        w->sndId2 = SndCall(8, 0x15, &em->pos, em->id, 0, em);
        w->timer = 125;
        w->x10 = step;
        w->timer2 = step;
        em->xFE++;
    case 1:
        EmCatchMotionMove(em, 1.0f, 1.0f);
        if (w->timer) {
            w->timer--;
            LifeDownSet2(pPLS, 20, 0, 1);
            PlGachaMove();
            if ((s16) pG->pl_life > 1 && (u32) PlGachaGet() > 30) {
                em->xFE++;
                break;
            }
            if (w->timer2) {
                w->timer2--;
            } else {
                w->timer2 = 4;
                EstSet((int) em, -1, 0, 0, 0x2A, 7, 0, 0, (u32) em, 0);
            }
        }
        if (em->motEvent & 1) {
            em32PlHeadLost();
            U16Set(pG->pl_life, 0);
            VibSetData(VIB_TBL, 0xB, 1);
        }
        if (em->motEvent & 4) {
            em32PlHeadFall();
        }
        if (em->frame > 13.6999998f && em->frame < 14.3000002f) {
            w->x10 = SndCall(8, 0xE, &em->pos, em->id, 0, em);
        }
        if (em->frame > 17.7000008f && em->frame < 18.2999992f) {
            SndCall(8, 0x12, &pPL->pos, em->id, 0, pPL);
        }
        if (em->frame > 188.699997f && em->frame < 189.300003f) {
            em->be_flag &= ~2;
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x29), (int) ARC(0x2A), 0, 1, 0);
        EmCatchPLSet(em, 3.14159274f, 1, (int) plem32_C_AtkHit, 58.7799988f, 0.0f, -468.209991f);
        pPL->xFE = step;
        EM32_EFFECT_DELETE(w->espKind[1], em);
        w->timer = 15;
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
            EmCatchMotionMove(em, 1.0f, 1.0f);
        } else if (MotionMoveF(em, 0)) {
            AtariOn(&em->atari, 0x300);
            if ((w->x991 || (Rnd() & 1)) && em32JumpDownCk(em)) {
                break;
            }
            EmRoutineSet(em, 1, 0x19, 0, 0);
            break;
        }
        if (em->motEvent & 1) {
            SndStop(w->x10, 0);
            SndStop(w->sndId2, 0);
        }
        break;
    }
    em->x3A8 = em->pos;
}

static void plem32_C_AtkHit(cPlayer* pl)
{
    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x87), 0, 0, 0x201, 0);
        AtariOff(&pl->atari, 0xFCFF);
        PlSetFace(1);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (pl->frame > 259.700012f && pl->frame < 260.299988f) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x88), 0, 0, 0x201, 0);
        EstSet((int) pl, -1, 0, 0, 0x2A, 0x28, 0, 0, (u32) pl, 0);
        pl->x3E0 = 15;
        pl->xFE++;
    case 3:
        if (pl->x3E0) {
            pl->x3E0--;
            EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else {
            AtariOn(&pl->atari, 0x300);
            if (MotionMoveF(pl, 0)) {
                AtariOn(&pl->atari, 0x300);
                EndPlDamage();
                pl->dmg.set(0, 30);
            }
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// Tail swing of the second form (parts 0x54..0x5D, the two tips also sweep a line).
static void em32_R1_P_Atk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;
    cModel* p;
    Vec v;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x71), (int) ARC(0x72), 10, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x1E, 0, 0, (u32) em, (void*) step);
        w->x991 = step;
        w->timer = 15;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.0981747732f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x991 == 0) {
                GameAddPoint(0xB);
            }
            if (w->x991) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xA, 0, 0);
            } else if (w->routeAngAbs < 0.785398185f && em->plDist2 < 9000000.0f) {
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            } else if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    if (em->motEvent & 1) {
        EM32_TAIL_ATK(em, 4);
        p = em->getPartsPtr(0x5A);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        em32AtkCk2(em, 4, &v, &p->oldWorldPos);
        p = em->getPartsPtr(0x5D);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        em32AtkCk2(em, 4, &v, &p->oldWorldPos);
    }
}

static void em32_R1_br_P_Catch(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (em->hp > 0 && em->xFE != 0) {
        int lost = !(w->flags & 1);
        if (lost == 0 && (w->flags & 0x80000) && (em->motEvent & 1)) {
            f32 ang = GetXZAngle(&pPL->pos, &em->pos);
            fabsf(Muku2(pPL->rot.y, ang, 3.14159274f));
            EM32_CLAW_ATK(em, 1);
            if (w->x991) {
                em->stat = 0x011E0000;
            }
        }
    }
}

static void em32_R1_P_Catch(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0xA3), (int) ARC(0xA4), 15, 1, 0);
        w->x991 = step;
        w->timer2 = 30;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            if (w->mode == 0 && Rnd() % 10 > 7 && em32JumpUpCk(em)) {
                return;
            }
            if ((w->x991 || Rnd() % 10 > 4) && em->plDist2 < 16000000.0f && em32StepUpCk2(em)) {
                return;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (em->plDist2 < 25000000.0f && pG->x4F88 <= 9) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
}

// The player is caught by the second form: shaken, then cut in two (em32PlDivideSet2).
static void em32_R1_P_CatchHit(cEm32* em)
{
    Em32Work* w;
    int step;

    em->dmg.set(0, 2);
    w = EM32_WK(em);
    step = em->xFE;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0xA5), (int) ARC(0xA6), 0, 1, 0);
        PlSetDamageSe(0);
        EmCatchPLSet(em, 3.14159274f, 1, (int) plem32_P_CatchHit, 248.539993f, 0.0f, -3618.96997f);
        GameAddPoint(2);
        PlGachaInit();
        EstSet((int) em, -1, 0, 0, 0x2A, 0xE, 0, w->espKind[1], (u32) em, (void*) step);
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
        w->sndId2 = SndCall(8, 0x1C, &em->pos, em->id, 0, em);
        w->x10 = step;
        w->timer = step;
        w->timer2 = step;
        em->xFE++;
    case 1:
        EmCatchMotionMove(em, 1.0f, 1.0f);
        if (em->motEvent & 1) {
            pG->pl_life = 0;
        }
        if ((s16) pG->pl_life > 1) {
            LifeDownSet2(pPL, 20, 0, 1);
            PlGachaMove();
            if ((u32) PlGachaGet() > 30) {
                em->xFE++;
                break;
            }
        }
        if (em->motEvent & 2) {
            w->x10 = SndCall(8, 0xE, &em->pos, em->id, 0, em);
        }
        if (em32BetweenHitCk(em)) {
            SndStop(w->x10, 0);
            SndStop(w->sndId2, 0);
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0xA7), (int) ARC(0xA8), 0, 1, 0);
        if (w->pDivide[0]) {
            w->pDivide[0]->be_flag &= ~2;
        }
        if (w->pDivide[1]) {
            w->pDivide[1]->be_flag &= ~2;
        }
        EmCatchPLSet(em, 3.14159274f, 1, (int) plem32_P_CatchHit, -127.949997f, 0.0f, -2747.37012f);
        pPL->xFE = step;
        EM32_EFFECT_DELETE(w->espKind[1], em);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x13, 0, 0, (u32) em, 0);
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->timer = 15;
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
            EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            AtariOn(&em->atari, 0x300);
            if (MotionMoveF(em, 0)) {
                if ((w->flags & 0x1000) && em32StepUpCk2(em)) {
                    break;
                }
                if (w->mode == 0 && Rnd() % 10 > 7 && em32JumpUpCk(em)) {
                    break;
                }
                if (Rnd() % 10 > 4 && em->plDist2 < 16000000.0f && em32StepUpCk2(em)) {
                    break;
                }
                if (w->targetAngAbs > 1.30899692f) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                } else {
                    w->wait = 60;
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                }
                break;
            }
        }
        if (em->motEvent & 1) {
            SndStop(w->x10, 0);
            SndStop(w->sndId2, 0);
        }
        break;
    }
    em->x3A8 = em->pos;
}

static void plem32_P_CatchHit(cPlayer* pl)
{
    Em32Work* w = EM32_WK(PL_EM(pl));
    int step;
    cObj* obj;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    step = pl->xFE;
    switch (step) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x9F), 0, 0, 1, 0);
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        PlSetFace(1);
        if (pSys->region) {
            EstSet((int) pl, -1, 0, 0, 0x2A, 0x2C, 0, w->espKind[1], (u32) pl->dmgType, (void*) step);
        } else {
            EstSet((int) pl, -1, 0, 0, 0x2A, 0x2D, 0, w->espKind[1], (u32) pl->dmgType, (void*) step);
        }
        pl->xFE++;
        break;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (pl->frame > 109.699997f && pl->frame < 110.300003f && pSys->region) {
            em32PlDivideSet2(PL_EM(pl));
            pl->be_flag &= ~2;
        }
        if ((((cEm32*) pPL->dmgType)->stat & 0xFFFF0000) != 0x011E0000) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 2:
        pl->be_flag |= 2;
        MotionSetCore(pl, &pl->mot, PL_ARC(0xA0), 0, 0, 1, 0);
        pl->pWep->setTrans(0, 0);
        obj = ObjMgr.create(0xB);
        w->pCatchObj = obj;
        if (obj) {
            obj->modelInit(PL_ARC(0xAC), PL_ARC(0xAB));
            w->pCatchObj->atari.flags &= 0xFCFF;
            w->pCatchObj->pParts->pParent = pPLS->getPartsPtr(0xA);
            w->pCatchObj->lightInfo.init2(1, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 500.0f, 0.0f, 0.0f }), 1);
            w->pCatchObj->wep.parent = pPLS;
            w->pCatchObj->getPartsPtr(1)->rot.y = 3.14159274f;
        }
        pl->x3E0 = 15;
        pl->xFE++;
    case 3:
        if (pl->x3E0) {
            pl->x3E0--;
            EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else if (MotionMoveF(pl, 0)) {
            if (w->pCatchObj) {
                ObjMgr.destroy(w->pCatchObj);
                w->pCatchObj = 0;
            }
            pl->pWep->setTrans(1, 0);
            AtariOn(&pl->atari, 0x300);
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// The last form pins the player to the ground (back jump prompt), then stamps and roars.
static void em32_R1_Ground(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;
    Mtx m;
    Vec v;
    int zero;

    w->flags |= 0x810;
    w->flags |= 0x10000;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x7D), (int) ARC(0x7E), 10, 1, 0);
        w->timer = 30;
        EM32_W_FRESH(w);   // COMPILER-DIFF #12
        IntSet(w->timer2, Rnd() % 90 + 90);
        IntSet(w->timer3, 20);
        if (pG->x4F88 <= 3) {
            IntSet(w->timer3, 25);
        }
        EstSet((int) em, -1, 0, 0, 0x2A, 0x1A, 0, 0, (u32) em, (void*) step);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        } else if (w->timer) {
            w->timer--;
        } else {
            AtariOff(&em->atari, 0xFCFF);
        }
        break;
    case 2:
        AtariOff(&em->atari, 0xFCFF);
        w->x991 = 0;
        w->actionSet = 0;
        w->x10 = Rnd() & 1;
        em->rot.y = pPLS->rot.y;
        GetPlPos(&em->pos, 0, 18.0f);
        PSMTXRotRad(m, 'y', em->rot.y);
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -2500.0f;
        PSMTXMultVec(m, &v, &em->pos);
        em->xFE++;
    case 3:
        w->flags |= 0x8000;
        if (w->actionSet == 0) {
            em->rot.y = pPL->rot.y;
            GetPlPos(&em->pos, 0, 18.0f);
            PSMTXRotRad(m, 'y', em->rot.y);
            TransMatrix(m, &em->pos);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -2500.0f;
            PSMTXMultVec(m, &v, &em->pos);
        }
        MotionSetCore(em, &em->mot, ARC(0x7F), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        if (w->timer2) {
            w->timer2--;
        } else {
            em->xFE++;
        }
        if (w->timer2 < w->timer3 && w->x991 == 0 && w->actionSet == 0) {
            switch (w->x10) {
            case 0:
            default:
                ActBtn.set(0x25, 0xB, (int) em32BackjumpAction, (int) em, 2, 3, 0, 0);
                break;
            case 1:
                ActBtn.set(0x25, 0xB, (int) em32BackjumpAction, (int) em, 2, 4, 0, 0);
                break;
            }
        }
        break;
    case 4:
        if (w->actionSet == 0) {
            em->rot.y = pPL->rot.y;
            GetPlPos(&em->pos, 0, 18.0f);
            PSMTXRotRad(m, 'y', em->rot.y);
            TransMatrix(m, &em->pos);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -2500.0f;
            PSMTXMultVec(m, &v, &em->pos);
        }
        zero = 0;
        MotionSetCore(em, &em->mot, ARC(0x7F), (int) ARC(0x80), 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x1B, 0, 0, (u32) em, (void*) zero);
        w->x991 = zero;
        w->timer = 5;
        em->xFE++;
    case 5:
        w->flags |= 0x8000;
        if (MotionMoveF(em, 0)) {
            if (Rnd() % 10 > 4) {
                w->timer2 = Rnd() % 60 + 20;
                em->xFE = 2;
            } else {
                em->xFE++;
            }
        } else if (em->motEvent & 1) {
            EM32_TAIL_ATK(em, 4);
        }
        break;
    case 6:
        zero = 0;
        em32GetGroundPos(em);
        MotionSetCore(em, &em->mot, ARC(0x81), (int) ARC(0x82), 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x1C, 0, 0, (u32) em, (void*) zero);
        w->x991 = zero;
        w->timer = 30;
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        } else if (w->timer) {
            w->timer--;
        } else {
            AtariOn(&em->atari, 0x300);
        }
        break;
    }
}

// The last form breaks the barred door (the tail sweeps the racks).
static void em32_R1_BreakBarred(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;
    cModel* p;
    Vec v;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x85), (int) ARC(0x86), 10, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x24, 0, w->espKind[1], (u32) em, (void*) step);
        w->x991 = step;
        w->timer = 15;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    if (em->motEvent & 1) {
        EM32_TAIL_ATK(em, 4);
        p = em->getPartsPtr(0x5A);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        em32AtkCk2(em, 4, &v, &p->oldWorldPos);
        p = em->getPartsPtr(0x5D);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        em32AtkCk2(em, 4, &v, &p->oldWorldPos);
        em32BreakBarred(em);
    }
}

static void em32_R0_Damage(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 8;
    Em32_R2_move_tbl[em->xFD](em);
}

static void em32_R1_Dm_Normal(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(0x25), (int) ARC(0x26), 3, 1, 0);
            SndCall(8, 0x20, &em->pos, em->id, 0, em);
            if (em->dmWep != 0x17) {
                EstSet((int) em, -1, 0, 0, 0x2A, 0x1D, 0, 0, (u32) em, (void*) step);
            }
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x69), (int) ARC(0x6A), 3, 1, 0);
            SndCall(8, 0x20, &em->pos, em->id, 0, em);
            if (em->dmWep != 0x17) {
                EstSet((int) em, -1, 0, 0, 0x2A, 0x1F, 0, 0, (u32) em, (void*) step);
            }
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0x6B), (int) ARC(0x6C), 3, 1, 0);
            SndCall(8, 0x20, &em->pos, em->id, 0, em);
            if (em->dmWep != 0x17) {
                EstSet((int) em, -1, 0, 0, 0x2A, 0x20, 0, 0, (u32) em, (void*) step);
            }
            break;
        }
        EM32_EFFECT_DELETE(w->espKind[2], em);
        EstSet((int) em, -1, 0, 0, 0x2A, 8, 0, 0, (u32) em, 0);
        w->x978 = 450;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->plDist2 < 25000000.0f && em32StepUpCk2(em)) {
                return;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
                return;
            } else if (Rnd() % 10 > 4 || em->plDist2 < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        if (em->motEvent & 4) {
            if (em->hp <= 1 && em->pos.x <= 25338.0f && em32JumpUpCk(em)) {
                return;
            }
            if (w->mode == 2 && Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x1F, 0, 0);
            }
        }
        break;
    }
}

static void em32_R0_Die(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 8;
    Em32_R3_move_tbl[em->xFD](em);
}

// Death: the body is set on the floor of the last area, fades and sinks after the drop item.
static void em32_R1_Die_Normal(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->xFE;
    Vec pos = { 54602.0f, 4315.0f, 10346.0f };

    w->flags |= 0x4000;
    switch (step) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->setPos(&pos);
        em->rot.y = 0.0f;
        MotionSetCore(em, &em->mot, ARC(0x77), (int) ARC(0x78), 3, 1, 0);
        EM32_EFFECT_DELETE(w->espKind[2], em);
        EM32_EFFECT_DELETE(w->espKind[0], em);
        EM32_EFFECT_DELETE(w->espKind[1], em);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x27, 1, w->espKind[1], (u32) em, (void*) step);
        em->clearStatus(5);
        w->scale = 1.0f;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->setStatus(8);
            EmSetDropItem(em);
            w->flags |= 0x2000;
            em->xFE++;
        }
        break;
    case 2:
        w->timer = 30;
        w->timer2 = 150;
        w->flags |= 0x20;
        w->scale = 1.0f;
        SndCall(8, 0x38, &em->pos, em->id, 0, em);
        EstSet((int) em, -1, 0, 0, 0x2A, 0x2B, 1, w->espKind[1], (u32) em, 0);
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
        } else {
            w->scale -= 0.003f;
            if (w->scale < 0.1f) {
                w->scale = 0.1f;
            }
            em->pos.y -= 6.0f;
        }
        TransMatrix(em->mat, &em->pos);
        if (w->timer2) {
            w->timer2--;
        } else {
            em->alpha -= 0.1f;
            if (em->alpha <= 0.0f) {
                em->alpha = 0.0f;
                em->be_flag &= ~2;
                em->be_flag |= 0x4000;
                em->xFE++;
            }
        }
        break;
    }
}

// The route to the player from the chest (parts 0x1C) at the enemy's height; the two line checks
// above it decide whether the player is visible (bit0) and reachable (bit19).
void em32RouteCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    Vec c;
    Vec d;
    cModel* p;

    if (em->hp <= 0) {
        return;
    }
    p = em->getPartsPtr(0x1C);
    a = p->worldPos;
    a.y = em->pos.y;
    RouteCkPosToPos(&a, &pPL->pos, &w->routePos);
    b = a;
    b.y += 1500.0f;
    c.x = pPL->pos.x;
    c.y = pPL->pos.y + 1500.0f;
    c.z = pPL->pos.z;
    if (EatMgr.hitCheck(&b, &c, 0, 0, 0, 0x4000) == 0) {
        w->flags |= 1;
    }
    b = a;
    b.y += 1500.0f;
    c.x = pPL->pos.x;
    c.y = pPL->pos.y + 1500.0f;
    c.z = pPL->pos.z;
    if (EatMgr.hitCheck(&b, &c, 0, 0, 0, 0) == 0) {
        w->flags |= 0x80000;
    }
    w->routeAng = Muku(&a, &w->routePos, em->rot.y, 3.14159274f);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->xFC == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->plDist2 = 10000000000000000.0f;
    }
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->plDist2;
    w->pTarget = pPLS;
    w->flags &= ~4;
    if (pGS->flags_60 & 0x4000) {
        d = em->pos;
        d.y += 250.0f;
        Draw_line3d(&d, &w->targetPos, 0xFFFFFF40, 0);
    }
}

// Head (parts 3) turns towards the player while it walks (addRot.y, damped).
void em32NeckMove(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cParts* p;
    cModel* pp;
    Vec v;

    if (w->mode == 2) {
        w->flags &= ~0x10;
    }
    em->getPartsPtr(4);
    pp = pPL->getPartsPtr(4);
    v.x = 0.0f;
    v.y = 250.0f;
    v.z = 0.0f;
    PSMTXMultVec(pp->mat, &v, &v);
    if (w->flags & 0x10) {
        w->neckAng = w->neckAng * 0.899999976f + Muku(&em->pos, &pPL->pos, em->rot.y, 1.04719758f) * 0.100000001f;
    } else {
        w->neckAng = w->neckAng * 0.899999976f;
    }
    p = (cParts*) em->getPartsPtr(3);
    p->motParts.flags |= 0x40000000;
    p->addRot.x = 0.0f;
    p->addRot.y = w->neckAng;
    p->addRot.z = 0.0f;
}

void em32ClothSet(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero = 0;

    w->cloth.num = 12;
    w->cloth.pParts = em32_cloth_parts;
    w->cloth.pLeft = (u8*) zero;
    w->cloth.pRight = (u8*) zero;
    w->cloth.pUpLeft = (u8*) zero;
    w->cloth.x14 = zero;
    w->cloth.pUp = em32_cloth_up;
    w->cloth.pDown = em32_cloth_down;
    w->cloth.pWindS = (f32*) zero;
    w->cloth.pWindR = (f32*) zero;
    w->cloth.x20 = zero;
    w->cloth.pRate = (f32*) zero;
    w->cloth.pMax = em32_cloth_max;
    w->cloth.pAt = (PlClothAt*) zero;
    w->cloth.nAt = zero;
    w->cloth.x3C = 40.0f;
    w->cloth.x40 = 0.6f;
    w->cloth.x44 = 3;
    w->cloth.x48 = 0.0f;
    w->cloth.x4C = 0.05f;
    w->cloth.x50 = 0.0f;
    w->cloth.flags = zero;
    w->cloth.x54 = zero;
    PenClothSet(em, (PenCloth*) &w->cloth, 100.0f);
}

// The last form's tail follows the cloth chain; the two tail tips copy their parents' matrices.
void em32ClothMove(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cModel* p;
    cModel* p2;

    if (w->mode != 2) {
        return;
    }
    if (w->flags & 0x8000) {
        return;
    }
    PenClothMove2(em, (PenCloth*) &w->cloth);
    p = em->getPartsPtr(0x16);
    p2 = em->getPartsPtr(0x51);
    PSMTXCopy(p->mat, p2->mat);
    p = em->getPartsPtr(0x17);
    p2 = em->getPartsPtr(0x52);
    PSMTXCopy(p->mat, p2->mat);
}

void em32BlendMotSet(cEm32* em, void* m0, void* m1, void* m2, void* m3, int a, int b, u16 d)
{
    Em32Work* w = EM32_WK(em);
    MotionWork* bm;
    int m3i, dd;
    // COMPILER-DIFF: #2 -- the original zero-extends the u16 parameter at both MotionSetCore calls
    // (`clrlwi r8, r25, 16`); ours drops the mask (combine's setup_incoming_promotions knows r10's
    // upper bits). The tied-operand launder (em2c BlendMotSet) makes an opaque 3-ref copy, declared
    // before the fabsf barrier; the same launder on m3 (whose prologue copy the target issues before
    // d's) restores the copies' priority tie.
    asm("" : "=r"(m3i) : "0"((int) m3)); // COMPILER-DIFF: #2
    asm("" : "=r"(dd) : "0"((int) d));   // COMPILER-DIFF: #2
    f32 val = fabsf(w->blendVal);
    void* m;
    int arg;

    MotionSetCore(em, &em->mot, m0, m3i, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    if (w->blendVal > 0.0f) {
        m = m1;
        arg = a;
    } else {
        m = m2;
        arg = b;
    }
    bm = EM32_BLEND_MOT(w);
    MotionSetCore(em, bm, m, arg, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    em->motBlend = bm;
    bm->blendRate = val * 0.00390625f;
    if (w->blendCnt) {
        w->blendCnt--;
    }
    w->blendSeq++;
    if ((u32) w->blendSeq >= em->frameMax) {
        w->blendSeq = 0;
    }
}

int em32AtkCk(cEm32* em, int no, int parts)
{
    cModel* p = em->getPartsPtr(parts);

    return em32AtkCk2(em, no, &p->worldPos, &p->oldWorldPos);
}

// The attack box `no` swept from oldPos to pos against the player / partner.
int em32AtkCk2(cEm32* em, int no, Vec* pos, Vec* oldPos)
{
    Em32Work* w = EM32_WK(em);
    int hit;

    if (w->x991) {
        return 0;
    }
    hit = EmAtkHitCk(&em32_atk_tbl[no], pos, oldPos, 0);
    if (hit) {
        if (hit & 1) {
            w->x991 = 1;
            EmPlBloodSet2(em, pos, 1, 0x2A, 0x2A);
            switch ((u32) no) {
            case 0:
                SndCall(8, 7, &em->pos, em->id, 0, em);
                pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(8, 0, 0);
                break;
            case 4:
                SndCall(8, 0x1B, &em->pos, em->id, 0, em);
                if ((s16) pG->pl_life <= 0) {
                    em32PlDivideSet(em);
                }
                break;
            case 2:
                SndCall(8, 7, &em->pos, em->id, 0, em);
                pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(8, 0, 0);
                break;
            }
        }
        if (hit & 2) {
            EmSubBloodSet(em, pos, 1, 0xFF, 0xFF);
            w->x991 = 1;
        }
        QuakeExec(0, 0, 5, 22.0f, 2);
        VibSetData(VIB_TBL, 7, 1);
        return 1;
    }
    return 0;
}

// A step-up point (EMI type 0x11, sub 0) within 6 m and 60 degrees of the player direction, with
// the player in sight: the run-up starts (routine 0x13).
int em32StepUpCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    f32 ang;
    int i;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    a = em->pos;
    b = pPLS->pos;
    a.y += 1000.0f;
    b.y += 1000.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
        return 0;
    }
    ang = GetXZAngle(&em->pos, &pPL->pos);
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);
        int sub;

        if (e->type != 0x11) {
            continue;
        }
        sub = e->sub;
        if (sub != 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            36000000.0f) {
            continue;
        }
        if (!(fabsf(Muku2(ang, GetXZAngle(&em->pos, &e->pos), 3.14159274f)) > 1.04719758f)) {
            w->stepPos = e->pos;
            EmRoutineSet(em, 1, 0x13, sub, sub);
            return 1;
        }
    }
    return 0;
}

// The nearest step-up point on the enemy's side of the two container rows, in sight: the run-up
// starts (xFF set: the player is on the roof).
int em32StepUpCk2(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    f32 best;
    f32 ang;
    f32 d;
    int found;
    int i;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    best = 169000000.0f;
    found = 0;
    ang = em->rot.y;
    if (w->flags & 0x1000) {
        ang = GetXZAngle(&em->pos, &w->stepTarget);
    }
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 0) {
            continue;
        }
        d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        if (d > best) {
            continue;
        }
        if ((e->pos.x < -11550.0f && em->pos.x > -11550.0f) || (e->pos.x > -11550.0f && em->pos.x < -11550.0f)) {
            continue;
        }
        if ((e->pos.x < 6921.0f && em->pos.x > 6921.0f) || (e->pos.x > 6921.0f && em->pos.x < 6921.0f)) {
            continue;
        }
        a = em->pos;
        b = e->pos;
        b.y = a.y = em->pos.y + 3000.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            continue;
        }
        if ((w->flags & 0x1000) && fabsf(Muku2(ang, GetXZAngle(&em->pos, &e->pos), 3.14159274f)) > 1.04719758f) {
            continue;
        }
        best = d;
        found = 1;
        w->stepPos = e->pos;
    }
    if (found == 0) {
        return 0;
    }
    if (w->flags & 0x1000) {
        EmRoutineSet(em, 1, 0x13, 0, 0);
    } else {
        em->xFD = 0x13;
        em->xFE = 0;
        em->xFC = 1;
        em->xFF = 1;
    }
    return 1;
}

// Step up when a container roof lies ahead (scenario attribute bit22 in front).
int em32StepUpCk3(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    f32 ang;
    int i;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    if (em32PlInTunnelCk(em) && em32StepUpCk2(em)) {
        return 1;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 1600.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (!(SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x400000)) {
        return 0;
    }
    ang = GetXZAngle(&em->pos, &pPL->pos);
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);
        int sub;

        if (e->type != 0x11) {
            continue;
        }
        sub = e->sub;
        if (sub != 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            36000000.0f) {
            continue;
        }
        if ((e->pos.x < -11550.0f && em->pos.x > -11550.0f) || (e->pos.x > -11550.0f && em->pos.x < -11550.0f)) {
            continue;
        }
        if ((e->pos.x < 6921.0f && em->pos.x > 6921.0f) || (e->pos.x > 6921.0f && em->pos.x < 6921.0f)) {
            continue;
        }
        if (!(fabsf(Muku2(ang, GetXZAngle(&em->pos, &e->pos), 3.14159274f)) > 1.04719758f)) {
            w->stepPos = e->pos;
            EmRoutineSet(em, 1, 0x13, sub, sub);
            return 1;
        }
    }
    return 0;
}

// The player is inside one of the two container tunnels (and his way ahead is not under a roof).
int em32PlInTunnelCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec p;
    Vec a;
    Vec b;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    GetPlPos(&p, 0, 20.0f);
    a = pPL->pos;
    b = p;
    a.y += 500.0f;
    b.y += 500.0f;
    PosToPos(&a, &b, &b, 0.5f);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x400000)) {
        return 0;
    }
    if (pPL->pos.x > 13125.0f && pPL->pos.x < 21684.0f && pPL->pos.z > 0.0f && pPL->pos.z < 2500.0f) {
        return 1;
    }
    if (pPL->pos.x > 18336.0f && pPL->pos.x < 21566.0f && pPL->pos.z > 0.0f && pPL->pos.z < 2703.0f) {
        return 1;
    }
    return 0;
}

// The roof point (EMI type 0x11, sub 0, state 1) whose tunnel the player is in: the tunnel attack
// starts from 2 m behind it.
int em32TunnelAtkCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Mtx m;
    Vec v;
    int i;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    if (em32PlInTunnelCk(em) == 0) {
        return 0;
    }
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);
        int sub;
        int state;

        if (e->type != 0x11) {
            continue;
        }
        sub = e->sub;
        if (sub != 0) {
            continue;
        }
        state = e->state;
        if (state != 1) {
            continue;
        }
        PSMTXRotRad(m, 'y', e->rotY);
        TransMatrix(m, &e->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(m, &v, &v);
        if (!((w->plPos.x - v.x) * (w->plPos.x - v.x) + (w->plPos.z - v.z) * (w->plPos.z - v.z) > 1000000.0f)) {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -2000.0f;
            PSMTXMultVec(m, &v, &em->pos);
            em->rot.y = e->rotY;
            w->pPoint = (Em32Point*) e;
            EmRoutineSet(em, state, 0x16, sub, sub);
            return 1;
        }
    }
    return 0;
}

// A jump-up point (EMI type 0x12) within 1.5 m: the jump onto the roof starts.
int em32JumpUpCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    EmiData* emi = (EmiData*) pG->pRoomEmi;
    int i;

    if (emi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 0x12) {
            continue;
        }
        if (!((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) > 2250000.0f)) {
            w->pPoint = (Em32Point*) e;
            EmRoutineSet(em, 1, 0x17, 0, 0);
            return 1;
        }
    }
    return 0;
}

// A jump-down point (EMI type 0x12, not sub 1) within 50 cm: the jump off the roof starts.
int em32JumpDownCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    EmiData* emi = (EmiData*) pG->pRoomEmi;
    int i;

    if (emi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    if (em->hp <= 1) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 0x12) {
            continue;
        }
        if (e->sub == 1) {
            continue;
        }
        if (!((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) > 250000.0f)) {
            w->pPoint = (Em32Point*) e;
            EmRoutineSet(em, 1, 0x18, 0, 0);
            return 1;
        }
    }
    return 0;
}

// The nearest jump point (EMI type 0x12) within 5 m becomes pPoint.
void em32GetJumpDownNo(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    f32 best;
    f32 d;
    int i;

    w->pPoint = 0;
    if (pGS->pRoomEmi == 0) {
        return;
    }
    best = 25000000.0f;
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);

        if (e->type != 0x12) {
            continue;
        }
        d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        if (d > best) {
            continue;
        }
        PSet((void*&) w->pPoint, e);
        best = d;
    }
}

// A jump point within 5 m of the player whose 2 m step towards him lands within 50 cm: the ceiling
// attack starts from there.
int em32CeilingAtkCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Mtx m;
    Vec v;
    f32 ang;
    f32 d;
    int i;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (w->mode != 0) {
        return 0;
    }
    if (em->hp <= 1) {
        return 0;
    }
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);

        if (e->type != 0x12) {
            continue;
        }
        if (e->sub == 1) {
            continue;
        }
        d = (w->plPos.x - e->pos.x) * (w->plPos.x - e->pos.x) + (w->plPos.z - e->pos.z) * (w->plPos.z - e->pos.z);
        if (d > 25000000.0f) {
            continue;
        }
        ang = GetXZAngle(&e->pos, &w->plPos);
        PSMTXRotRad(m, 'y', ang);
        TransMatrix(m, &e->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 2000.0f;
        PSMTXMultVec(m, &v, &v);
        d = (w->plPos.x - v.x) * (w->plPos.x - v.x) + (w->plPos.z - v.z) * (w->plPos.z - v.z);
        if (!(d > 250000.0f)) {
            w->pPoint = (Em32Point*) e;
            em->pos = e->pos;
            em->rot.y = ang;
            EmRoutineSet(em, 1, 0x1A, 0, 0);
            return 1;
        }
    }
    return 0;
}

// The player's head comes off (regions other than Japan).
void em32PlHeadLost()
{
    if (pSys->region == 0) {
        PlSetDamageSe(0xD);
        return;
    }
    pPL->setHead(0);
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
}

// The head object flies away.
void em32PlHeadFall()
{
    Vec ofs;
    Vec spd;
    Vec rot;
    cObj* obj;

    if (pSys->region == 0) {
        return;
    }
    pPL->getPartsPtr(3);
    spd.x = 0.0f;
    spd.y = 80.0f;
    spd.z = -50.0f;
    rot = pPL->rot;
    rot.z = 1.22173047f;
    rot.y = LIMIT_ANGLE(rot.y);
    ofs.x = 763.440002f;
    ofs.y = 6000.0f;
    ofs.z = 810.409973f;
    PSMTXMultVec(pPL->mat, &ofs, &ofs);
    spd.x = 0.0f;
    spd.y = 80.0f;
    spd.z = 0.0f;
    obj = SetObj01(PL_ARC_PTR(pG->pPlArc, 0xC), PL_ARC_PTR(pG->pPlArc, 7), &ofs, &rot, &spd, 15.0f, 150.0f, 1000, 0x11);
    if (obj) {
        obj->lightInfo.x50 = 1;
        Obj01SetEst(obj, 0, -1, 4, 0, -1, 0, -1, 0, -1);
    }
}

// Room script hook: the enemy is placed at one of the eight scripted positions with the matching
// routine (0/1 the cage, 2/4/5/6 the appearances, 7 the second form in the open, 3 refills the hp).
void cEm32::setNext(int no)
{
    Em32Work* w = EM32_WK(this);
    Vec pos[8] = {
        { -2216.05005f, 0.0f, 1314.91003f },
        { 9393.0f, 6000.0f, 4016.0f },
        { 0.0f, 0.0f, 0.0f },
        { -20388.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 50810.0f, 4315.0f, 9845.0f },
        { -2261.05005f, 0.0f, 1041.91003f },
        { 10189.0f, 0.0f, 4379.0f },
    };
    f32 rot[8] = { 3.14159274f, 0.785398185f, 0.0f, -0.879999995f, 0.0f, 0.879999995f, 3.14159274f, 0.785398185f };

    if ((u32) no > 7) {
        return;
    }
    if (no == 3) {
        if (w->mode != 2) {
            hp = 500;
        }
        return;
    }
    setPos(&pos[no]);
    this->rot.y = rot[no];
    MotionSetCore(this, &mot, PL_ARC_PTR(subArc, 0x11), 0, 0, 1, 0);
    MotionMoveF(this, 0);
    partsWorldCalc();
    oldPos = this->pos;
    w->x7C4 = no;
    alpha = 1.0f;
    be_flag |= 2;
    w->pPoint = 0;
    switch ((u32) no) {
    case 0:
    default:
        w->flags &= ~0x1000;
        hp = 500;
        EmRoutineSet(this, 1, 2, 0, 0);
        break;
    case 1:
        w->flags &= ~0x1000;
        hp = 1500;
        EmRoutineSet(this, no, 3, 0, 0);
        break;
    case 2:
        U8Set(w->mode, 1);
        hp = hpMax;
        AtariOff(&atari, 0xFCFF);
        EmRoutineSet(this, 1, 4, 0, 0);
        break;
    case 4:
        U8Set(w->mode, 1);
        hp = hpMax;
        AtariOff(&atari, 0xFCFF);
        flags_3C8 &= ~1;
        w->flags |= 0x100000;
        if (w->pTexModel) {
            w->flags |= 0x140000;
            w->pTexModel->resetTexBlendTbl();
        }
        EmRoutineSet(this, 1, 5, 0, 0);
        break;
    case 5:
        U8Set(w->mode, 1);
        hp = hpMax;
        AtariOff(&atari, 0xFCFF);
        flags_3C8 &= ~1;
        w->flags |= 0x100000;
        if (w->pTexModel) {
            w->flags |= 0x140000;
            w->pTexModel->resetTexBlendTbl();
        }
        w->flags |= 0x20000;
        EmRoutineSet(this, 1, 0xD, 0, 1);
        break;
    case 6:
        hp = hpMax;
        AtariOff(&atari, 0xFCFF);
        flags_3C8 &= ~1;
        xFD = 0xD;
        xFE = 0;
        xFC = 1;
        xFF = 1;
        break;
    case 7:
        U8Set(w->mode, 1);
        w->wait = 10;
        EM32_EFFECT_DELETE(w->espKind[1], this);
        em32TexrenderInit(this);
        EmRoutineSet(this, 1, 6, 0, 1);
        break;
    }
}

int cEm32::getBreakNo()
{
    return EM32_WK(this)->breakNo;
}

int cEm32::getBreakNo2()
{
    return EM32_WK(this)->breakNo2;
}

// The texture render manager of the second form's blend texture.
void em32TexrenderInit(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    u8* tbl = w->texBlend;

    if (w->pTexModel == 0) {
        return;
    }
    w->pTex = Ctrl12GetTexRenderEm32(w->pCtrl12);
    if (w->pTex == 0) {
        pLog->err(0, 0, "em32TexrenderInit:: Manager alloc failed!!");
        return;
    }
    tbl[0] = 1;
    tbl[1] = 0;
    tbl[4] = 0xF7;
    tbl[5] = w->pTex->texId;
    w->pTex->repType = 1;
    w->pTex->sy = w->pTex->sx = 0x40;
    EffectEspDelete(w->pTex->mask | 0x801, w->espKind[1], (u32) em, 0);
    EffectEspgenDelete(w->pTex->mask | 0x801, w->espKind[1], (int) em);
    EffectEfmDelete(w->pTex->mask | 0x801, w->espKind[1], (int) em);
    EstSet(0, -1, 0, 0, 0x2A, 0, w->pTex->mask | 0x801, w->espKind[1], (u32) em, 0);
}

// The player's position 18 frames ahead (plPos) and the angle / squared distance to it.
void em32GetPlPos(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cModel* p = pPL->getPartsPtr(0);
    Vec d;

    PSVECSubtract(&p->worldPos, &p->x88, &d);
    PSVECScale(&d, &d, 18.0f);
    PSVECAdd(&pPL->pos, &d, &w->plPos);
    w->plAng = Muku(&em->pos, &w->plPos, em->rot.y, 3.14159274f);
    w->plAngAbs = fabsf(w->plAng);
    w->routeAng = fabsf(w->plAng);
    w->plDist2 = (em->pos.x - w->plPos.x) * (em->pos.x - w->plPos.x) + (em->pos.z - w->plPos.z) * (em->pos.z - w->plPos.z);
}

// Step-down target: the nearest EMI type 0x11 / sub 1 point (within 4 m of the player and in his
// sight, or within 5 m of stepTarget while flags bit12 is set), else the nearest at all.
void em32GetStepDownPos(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    f32 best;
    f32 d;
    int found;
    int i;
    EmiEntry* e;

    w->stepPos = pPLS->pos;
    if (pG->pRoomEmi == 0) {
        return;
    }
    best = 10000000000000000.0f;
    found = 0;
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 1) {
            continue;
        }
        if (w->flags & 0x1000) {
            d = (w->stepTarget.x - e->pos.x) * (w->stepTarget.x - e->pos.x) +
                (w->stepTarget.z - e->pos.z) * (w->stepTarget.z - e->pos.z);
            if (d > best) {
                continue;
            }
        } else {
            d = (pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z);
            if (d < 12250000.0f) {
                continue;
            }
            if (d > best) {
                continue;
            }
            a = pPL->pos;
            b = e->pos;
            a.y += 500.0f;
            b.y += 500.0f;
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                continue;
            }
        }
        best = d;
        found = 1;
        w->stepPos = e->pos;
    }
    if (found) {
        return;
    }
    best = 10000000000000000.0f;
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 1) {
            continue;
        }
        if (w->flags & 0x1000) {
            d = (w->stepTarget.x - e->pos.x) * (w->stepTarget.x - e->pos.x) +
                (w->stepTarget.z - e->pos.z) * (w->stepTarget.z - e->pos.z);
        } else {
            d = (pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z);
        }
        if (d > best) {
            continue;
        }
        best = d;
        memcpy((u8*) w + 0x79C, &e->pos, sizeof(Vec));
    }
}

int em32SetDmVal(cEm32* em)
{
    int near = 0;
    int dmg;

    if (em->dmPart->rad < 16000000.0f) {
        near = 1;
    }
    dmg = 20;
    if (em->dmWep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmWep, near);
    }
    return dmg;
}

// The ambush swipe from the cage (not on the easy difficulties): the player is behind the cage bars
// on the side the enemy faces, within 1.5 m of the swipe point, and both lines are clear.
int em32AmbushAtkCk(cEm32* em)
{
    Vec b;
    Vec a;
    Vec c;
    Vec d;
    f32 ang;

    if (pG->x4F88 <= 1) {
        return 0;
    }
    ang = Muku(&em->pos, &pPL->pos, em->rot.y, 3.14159274f);
    a = em->pos;
    a.y += 500.0f;
    b = pPL->pos;
    b.y += 500.0f;
    if (ang > 0.0f) {
        c.x = 0.0f;
        c.y = 500.0f;
        c.z = 3217.64990f;
        d.x = 4000.0f;
        d.y = 500.0f;
        d.z = 3217.64990f;
        PSMTXMultVec(em->mat, &c, &c);
        PSMTXMultVec(em->mat, &d, &d);
        if ((b.x - d.x) * (b.x - d.x) + (b.z - d.z) * (b.z - d.z) > 1000000.0f) {
            return 0;
        }
        if (SatMgr.hitCheck(&a, &c, 0, 0, 0, 0)) {
            return 0;
        }
        if (SatMgr.hitCheck(&c, &b, 0, 0, 0, 0)) {
            return 0;
        }
        EmRoutineSet(em, 1, 0xE, 0, 0);
        return 1;
    }
    c.x = 0.0f;
    c.y = 500.0f;
    c.z = 3217.64990f;
    d.x = -4000.0f;
    d.y = 500.0f;
    d.z = 3217.64990f;
    PSMTXMultVec(em->mat, &c, &c);
    PSMTXMultVec(em->mat, &d, &d);
    if ((b.x - d.x) * (b.x - d.x) + (b.z - d.z) * (b.z - d.z) > 1000000.0f) {
        return 0;
    }
    if (SatMgr.hitCheck(&a, &c, 0, 0, 0, 0)) {
        return 0;
    }
    if (SatMgr.hitCheck(&c, &b, 0, 0, 0, 0)) {
        return 0;
    }
    EmRoutineSet(em, 1, 0xE, 0, 1);
    return 1;
}

void em32BloodSet(cEm32* em)
{
    int near = 0;

    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    switch (em->dmWep) {
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        EmDmBloodSet2(em, 0x2A, 2, 0, 0, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            EmDmBloodSet2(em, 0x2A, 3, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x2A, 1, 0, 0, 0);
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x26:
    case 0x2B:
        EmDmBloodSet2(em, 0x2A, 1, 0, 0, 0);
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
        EmDmBloodSet2(em, 0x2A, 3, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    case 0x16:
    case 0x2A:
    default:
        break;
    }
}

// The two halves of the cut player: the body models by costume (pG->costume), the parts models
// of the player archive on top, hidden until em32PlDivideSet.
#define EM32_DIVIDE_MODELS(w, bin1, bin2)                                                              \
    obj = SetObj00(ARC(bin1), PL_ARC_PTR(pG->pPlArc, 5), &pPL->pos, &pPL->rot);                      \
    (w)->pDivide[0] = obj;                                                                             \
    if (obj) {                                                                                         \
        info = ModInfoMgr.create(ARC(0x96), PL_ARC_PTR(pG->pPlArc, 7));                                \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        info = ModInfoMgr.create(ARC(0x97), PL_ARC_PTR(pG->pPlArc, 7));                                \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        info = ModInfoMgr.create(ARC(0x98), PL_ARC_PTR(pG->pPlArc, 7));                                \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        info = ModInfoMgr.create(ARC(0x99), PL_ARC_PTR(pG->pPlArc, 0x11));                             \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        info = ModInfoMgr.create(ARC(0x9A), PL_ARC_PTR(pG->pPlArc, 0x11));                             \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        (w)->pDivide[0]->be_flag &= ~2;                                                                \
        (w)->pDivide[0]->lightInfo.x50 = 1;                                                            \
    }                                                                                                  \
    obj = SetObj00(ARC(bin2), PL_ARC_PTR(pG->pPlArc, 5), &pPL->pos, &pPL->rot);                      \
    (w)->pDivide[1] = obj;                                                                             \
    if (obj) {                                                                                         \
        obj->be_flag &= ~2;                                                                            \
        (w)->pDivide[1]->lightInfo.x50 = 1;                                                            \
    }

void em32PlDivideModelInit(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cObj* obj;
    cModelInfo* info;

    switch (pG->costume) {
    case 0:
    case 1:
    default:
        EM32_DIVIDE_MODELS(w, 0x95, 0x9B);
        break;
    case 2:
        EM32_DIVIDE_MODELS(w, 0xAE, 0xAF);
        break;
    case 3:
        EM32_DIVIDE_MODELS(w, 0xB0, 0xB1);
        break;
    }
}

class cObj00 : public cObj {
public:
    void setScrAtari(f32 r);
};

// The player is cut in two by the tail: the halves take his place and fall.
void em32PlDivideSet(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    SetPlDamage((int) em, plemDivide);
    pG->pl_life = 0;
    pPLS->be_flag &= ~2;
    pPLS->rot.y = GetXZAngle(&em->pos, &pPLS->pos);
    if (w->pDivide[0]) {
        ((cObj00*) w->pDivide[0])->setScrAtari(300.0f);
        w->pDivide[0]->be_flag |= 2;
        w->pDivide[0]->pos = pPLS->pos;
        w->pDivide[0]->rot = pPLS->rot;
        MotSetObj00(w->pDivide[0], ARC(0x9C), 1, 0);
        EstSet((int) w->pDivide[0], -1, 0, 0, 0x2A, 0x10, 0, 0, (u32) w->pDivide[0], 0);
    }
    if (w->pDivide[1]) {
        ((cObj00*) w->pDivide[1])->setScrAtari(300.0f);
        w->pDivide[1]->be_flag |= 2;
        w->pDivide[1]->pos = pPLS->pos;
        w->pDivide[1]->rot = pPLS->rot;
        MotSetObj00(w->pDivide[1], ARC(0x9D), 1, 0);
        EstSet((int) w->pDivide[0], -1, 0, 0, 0x2A, 0x11, 0, 0, (u32) w->pDivide[0], 0);
    }
    VibSetData(VIB_TBL, 0xB, 1);
}

// The caught player is cut in two: the halves drop from the claws.
void em32PlDivideSet2(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (w->pDivide[0]) {
        w->pDivide[0]->be_flag |= 2;
        w->pDivide[0]->pos = pPLS->pos;
        w->pDivide[0]->rot = pPLS->rot;
        MotSetObj00(w->pDivide[0], ARC(0xA1), 1, 0);
    }
    if (w->pDivide[1]) {
        w->pDivide[1]->be_flag |= 2;
        w->pDivide[1]->pos = pPLS->pos;
        w->pDivide[1]->rot = pPLS->rot;
        MotSetObj00(w->pDivide[1], ARC(0xA2), 1, 0);
        EstSet((int) w->pDivide[1], -1, 0, 0, 0x2A, 0xF, 0, w->espKind[1], (u32) em, 0);
    }
    VibSetData(VIB_TBL, 0xB, 1);
}

static void plemDivide(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x9E), 0, 3, 1, 0);
        AtariOff(&pl->atari, 0xFCFF);
        pl->xFE++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
    pl->subArc = pl->subArc2;
}

// The racks (enemy id 0x45) within 3 m of the point 2 m ahead break.
void em32RackBreakCk(cEm32* em)
{
    Vec v;
    u32 i;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 2000.0f;
    PSMTXMultVec(em->mat, &v, &v);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = EmMgrWork(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x45) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((v.x - e->pos.x) * (v.x - e->pos.x) + (v.y - e->pos.y) * (v.y - e->pos.y) +
                (v.z - e->pos.z) * (v.z - e->pos.z) >
            9000000.0f) {
            continue;
        }
        EmRackSetBreak((cEmRack*) e, &em->pos);
    }
}

// The hit boxes take (on) / ignore (off) the damage marks; the tail tip boxes always report.
void em32SetYarareMark(cEm32* em, int on)
{
    Em32Work* w = EM32_WK(em);
    int i;

    if (on) {
        w->hit[27].flags |= 1;
        em->hitInfo.flags &= ~0x40;
        for (i = 0; i < 30; i++) {
            w->hit[i].flags &= ~0x40;
        }
    } else {
        w->hit[27].flags |= 1;
        em->hitInfo.flags |= 0x40;
        for (i = 0; i < 30; i++) {
            w->hit[i].flags |= 0x40;
        }
    }
}

int cEm32::ckDie()
{
    if (EM32_WK(this)->flags & 0x2000) {
        return 1;
    }
    return 0;
}

// The barred doors (enemy id 0x4E) in the box ahead break towards the enemy.
void em32BreakBarred(cEm32* em)
{
    Mtx inv;
    Vec lp;
    u32 i;

    PSMTXInverse(em->mat, inv);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = EmMgrWork(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x4E) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        PSMTXMultVec(inv, &e->pos, &lp);
        if (lp.x > -2000.0f && lp.x < 2000.0f && lp.y > -500.0f && lp.y < 500.0f && lp.z > 0.0f && lp.z < 1500.0f) {
            ((cEmBarred*) e)->setBreak(&em->pos);
            e->rot.y = em->rot.y;
            SndCall(6, 0xD, &e->pos, 0, 0, 0);
        }
    }
}

// The second motion work (the body) runs with the main one.
void em32BodyMove(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (w->pMot && !(w->flags & 0x4000)) {
        MotionMoveCore(em, (MotionWork*) w->pMot, 0);
        MotionSequenceCtrl((MotionWork*) w->pMot);
    }
}

void em32BreathSe(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (w->voiceTimer) {
        w->voiceTimer--;
        return;
    }
    w->voiceTimer = 59;
    w->sndId = SndCall(8, 0x33, &em->pos, em->id, 0, em);
}

// Stops the breath voice on the motion sounds that voice the enemy.
void em32BreathSeStopCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    u32 no;

    if (em->seNo == 0) {
        return;
    }
    no = em->seNo - 1;
    switch (no) {
    case 4:
    case 5:
    case 7:
    case 8:
    case 9:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x10:
    case 0x11:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x20:
    case 0x23:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        break;
    }
}

// The dying body shrinks: every parts matrix is scaled by `scale` (x / z) around its world position.
void em32ScaleCompress(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Mtx m;
    Vec scale;
    cModel* p;

    if (!(w->flags & 0x20)) {
        return;
    }
    PSMTXIdentity(m);
    scale.x = 1.0f;
    scale.y = w->scale;
    scale.z = 1.0f;
    ScaleMatrix(m, &scale);
    for (p = em->pParts; p; p = p->pParts) {
        PSMTXConcat(m, p->mat, p->mat);
        p->mat[0][3] = p->worldPos.x;
        p->mat[1][3] = p->worldPos.y;
        p->mat[2][3] = p->worldPos.z;
    }
}

// The nearest ground attack point (EMI type 7) within 1e16: the enemy stands there facing its
// rotY (turned half round when it points away).
void em32GetGroundPos(cEm32* em)
{
    Vec pos;
    f32 best;
    f32 d;
    f32 ry;
    int i;

    if (pG->pRoomEmi == 0) {
        return;
    }
    pos = em->pos;
    best = 10000000000000000.0f;
    ry = em->rot.y;
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);

        if (e->type != 7) {
            continue;
        }
        d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        if (d > best) {
            continue;
        }
        best = d;
        pos = e->pos;
        ry = e->rotY;
    }
    if (fabsf(Muku2(em->rot.y, ry, 3.14159274f)) > 1.57079637f) {
        ry += 3.14159274f;
    }
    em->rot.y = LIMIT_ANGLE(ry);
    em->pos = pos;
}

// Something (the scenario or an object) lies between the enemy and the player at knee height.
int em32BetweenHitCk(cEm32* em)
{
    Vec a;
    Vec b;

    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 1;
    }
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 1;
    }
    return 0;
}

