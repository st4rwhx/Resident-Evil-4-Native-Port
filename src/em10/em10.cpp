// em10/em10.cpp: the Ganado enemy library (D:/Bio4/Prog/em10.cpp), the same object in the 16 modules
// em10..em17, em19..em1f, em20 (config/G4BE08/modules.py). cEm10 and its routines, the per-weapon
// damage reactions, the route / attack / find checks and the player-side event routines (plem10*).

#include "sscrn.h"
#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
#include "em10.h"
#include "em_sub.h"
#include "em_set.h"
#include "emhit.h"
#include "emwep.h"
#include "emdoor.h"
#include "emwindow.h"
#include "emswitch.h"
#include "emshield.h"
#include "at_mod.h"
#include "esp.h"
#include "est.h"
#include "embarrel.h"
#include "ctrl.h"
#include "snd.h"
#include "quake.h"
#include "pad.h"
#include "main.h"
#include "act_btn.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "pl_cloth.h"
#include "cockpit.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "route_ck.h"
#include "foot_shadow.h"
#include "dbmodule.h"
#include "mercenaries.h"
#include "motion.h"
#include "game.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "joy.h"
#include "em_cloth.h"
#include "item.h"
#include "sce_at.h"
#include "sce.h"
#include "gx_sub.h"

// The 0x34-byte COMMON block every original module carries (uninitialised static data members of
// a shared header, see include/st_room.h): the split object of every Ganado module defines it as
// `common_<mod>`, unreferenced. REL_MODULE comes from configure.py.
#define EM10_STR2(x) #x
#define EM10_STR(x) EM10_STR2(x)
asm(".comm common_" EM10_STR(REL_MODULE) ",52,4");

// motion.h declares the one-argument form; the enemies pass a second argument (pl_npc.cpp).
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");
extern "C" double atan2(double y, double x);

// Collision flag bits set / cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`, pl_npc.cpp).
static inline void AtariOn(cAtariInfo* at, u16 b) { at->flags |= b; }
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->flags &= mask; }

// Routine dispatch tables (.data).
static void em10_R0_Init(cEm10* em);
static void em10_R0_Move(cEm10* em);
static void em10_R0_Damage(cEm10* em);
static void em10_R0_Die(cEm10* em);
static void em10DmSetWep00(cEm10* em);
static void em10DmSetWep02(cEm10* em);
static void em10DmSetWep03(cEm10* em);
static void em10DmSetWep09(cEm10* em);
static void em10DmSetWep23(cEm10* em);
static void em10_R1_br_Dummy(cEm10* em);
static void em10_R1_br_Wait(cEm10* em);
static void em10_R1_Wait(cEm10* em);
static void em10_R1_Keeper(cEm10* em);
static void em10_R1_Hide(cEm10* em);
static void em10_R1_HideFall(cEm10* em);
static void em10_R1_HideJump(cEm10* em);
static void em10_R1_R10CParasite(cEm10* em);
static void em10_R1_R10CPCancel(cEm10* em);
static void em10_R1_R204Prayer(cEm10* em);
static void em10_R1_R222DragonA(cEm10* em);
static void em10_R1_R222DragonB(cEm10* em);
static void em10_R1_R222DragonC(cEm10* em);
static void em10_R1_R227Barrel(cEm10* em);
static void em10_R1_R21BTrolleyJump(cEm10* em);
static void em10_R1_R21BTrolleyJump2(cEm10* em);
static void em10_R1_R303FireDash(cEm10* em);
static void em10_R1_R10FGJump(cEm10* em);
static void em10_R1_R10FGondola(cEm10* em);
static void em10_R1_R209DashSit(cEm10* em);
static void em10_R1_StickClaw(cEm10* em);
static void em10_R1_R11DAppear1(cEm10* em);
static void em10_R1_R11DAppear2(cEm10* em);
static void em10_R1_R212Drill(cEm10* em);
static void em10_R1_R209Gatling(cEm10* em);
static void em10_R1_R201EventWait(cEm10* em);
static void em10_R1_FindLost(cEm10* em);
static void em10_R1_R100WalkStay(cEm10* em);
static void em10_R1_R202Finger(cEm10* em);
static void em10_R1_StayWalk(cEm10* em);
static void em10_R1_AttackWait(cEm10* em);
static void em10_R1_R100TurnWalk(cEm10* em);
static void em10_R1_R100Cliff(cEm10* em);
static void em10_R1_R101Bucket(cEm10* em);
static void em10_R1_R101Suki(cEm10* em);
static void em10_R1_Work(cEm10* em);
static void em10_R1_UFOCatch(cEm10* em);
static void em10_R1_R300TakeAshley(cEm10* em);
static void em10_R1_R30FBullJump(cEm10* em);
static void em10_R1_R320Gatling(cEm10* em);
static void em10_R1_R321DeadBody(cEm10* em);
static void em10_R1_R300Gatling(cEm10* em);
static void em10_R1_R101Cart(cEm10* em);
static void em10_R1_br_EvtDash(cEm10* em);
static void em10_R1_EvtDash(cEm10* em);
static void em10_R1_br_EvtWalk(cEm10* em);
static void em10_R1_EvtWalk(cEm10* em);
static void em10_R1_Pickup(cEm10* em);
static void em10_R1_Find(cEm10* em);
static void em10_R1_C_SawStart(cEm10* em);
static void em10_R1_BombIgnition(cEm10* em);
static void em10_R1_br_Walk(cEm10* em);
static void em10_R1_Walk(cEm10* em);
static void em10_R1_br_Dash(cEm10* em);
static void em10_R1_Dash(cEm10* em);
static void em10_R1_br_Back(cEm10* em);
static void em10_R1_Back(cEm10* em);
static void em10_R1_br_Goto(cEm10* em);
static void em10_R1_Goto(cEm10* em);
static void em10_R1_GuardWalk(cEm10* em);
static void em10_R1_Turn180(cEm10* em);
static void em10_R1_Threat(cEm10* em);
static void em10_R1_SideStep(cEm10* em);
static void em10_R1_HideSide(cEm10* em);
static void em10_R1_AppearSide(cEm10* em);
static void em10_R1_SitDown(cEm10* em);
static void em10_R1_Stay(cEm10* em);
static void em10_R1_RoofWait(cEm10* em);
static void em10_R1_Guard(cEm10* em);
static void em10_R1_DownWakeWait(cEm10* em);
static void em10_R1_DownWake(cEm10* em);
static void em10_R1_Crash(cEm10* em);
static void em10_R1_ClimbOver(cEm10* em);
static void em10_R1_DoorAtk(cEm10* em);
static void em10_R1_RackAtk(cEm10* em);
static void em10_R1_WindowAtk(cEm10* em);
static void em10_R1_LadderClimb(cEm10* em);
static void em10_R1_VLadderClimb(cEm10* em);
static void em10_R1_LadderReset(cEm10* em);
static void em10_R1_JumpDown(cEm10* em);
static void em10_R1_Jump(cEm10* em);
static void em10_R1_JumpUp(cEm10* em);
static void em10_R1_Trade(cEm10* em);
static void em10_R1_Drive(cEm10* em);
static void em10_R1_Catapult(cEm10* em);
static void em10_R1_RockPush(cEm10* em);
static void em10_R1_ParasiteAtk(cEm10* em);
static void em10_R1_ShotBowgun(cEm10* em);
static void em10_R1_ShotRocket(cEm10* em);
static void em10_R1_ShotGatling(cEm10* em);
static void em10_R1_ThrowAxe(cEm10* em);
static void em10_R1_ThrowBomb(cEm10* em);
static void em10_R1_FixBomber(cEm10* em);
static void em10_R1_R305Bomber(cEm10* em);
static void em10_R1_R408Bomber(cEm10* em);
static void em10_R1_RocketWait(cEm10* em);
static void em10_R1_AxeAtk(cEm10* em);
static void em10_R1_ShieldAtk(cEm10* em);
static void em10_R1_TorchFrame(cEm10* em);
static void em10_R1_SukiAtk(cEm10* em);
static void em10_R1_ScytheAtk(cEm10* em);
static void em10_R1_ClawAtk(cEm10* em);
static void em10_R1_br_CSawWalkAtk(cEm10* em);
static void em10_R1_CSawWalkAtk(cEm10* em);
static void em10_R1_ClawWalkAtk(cEm10* em);
static void em10_R1_br_ClawCriAtk(cEm10* em);
static void em10_R1_ClawCriAtk(cEm10* em);
static void em10_R1_ClawCriHit(cEm10* em);
static void plem10_ClawCriHit(cPlayer* pl);
static void em10_R1_br_C_SawAtk(cEm10* em);
static void em10_R1_C_SawAtk(cEm10* em);
static void em10_R1_C_SawHit(cEm10* em);
static void plem10_C_SawHit(cPlayer* pl);
static void em10_R1_br_C_SawCriAtk(cEm10* em);
static void em10_R1_C_SawCriAtk(cEm10* em);
static void em10_R1_C_SawCriHit(cEm10* em);
static void plem10_C_SawCriHit(cPlayer* pl);
static void em10_R1_br_Catch(cEm10* em);
static void em10_R1_Catch(cEm10* em);
static void em10_R1_NeckHang(cEm10* em);
static void plem10_NeckHang(cPlayer* pl);
static void em10_R1_NeckHang_Luis(cEm10* em);
static void subem10_NeckHang_Luis(cSubChar* sub);
static void em10_R1_NeckHang_Ashley(cEm10* em);
static void subem10_NeckHang_Ashley(cSubChar* sub);
static void em10_R1_Backhold(cEm10* em);
static void plem10_Backhold(cPlayer* pl);
static void em10_R1_Bombhold(cEm10* em);
static void plem10_Bombhold(cPlayer* pl);
static void em10_R1_br_DashCatch(cEm10* em);
static void em10_R1_DashCatch(cEm10* em);
static void em10_R1_TakeAway(cEm10* em);
static void subem10_TakeAway(cSubChar* sub);
static void em10_R1_Dm_Small(cEm10* em);
static void em10_R1_Dm_Head(cEm10* em);
static void em10_R1_Dm_Flash(cEm10* em);
static void em10_R1_Dm_Claw(cEm10* em);
static void em10_R1_Dm_Claw_Big(cEm10* em);
static void em10_R1_Dm_Gatling(cEm10* em);
static void em10_R1_Dm_FS(cEm10* em);
static void em10_R1_Dm_KneeKick(cEm10* em);
static void em10_R1_Dm_NeckBreak(cEm10* em);
static void em10_R1_Dm_Showtay(cEm10* em);
static void em10_R1_Dm_Heel(cEm10* em);
static void em10_R1_Dm_DashUp(cEm10* em);
static void em10_R1_Dm_DashDown(cEm10* em);
static void em10_R1_Dm_Blow(cEm10* em);
static void em10_R1_Dm_Fence(cEm10* em);
static void em10_R1_Dm_Ladder(cEm10* em);
static void em10_R1_Dm_Roof(cEm10* em);
static void em10_R1_Dm_KneeDown(cEm10* em);
static void em10_R1_Dm_KnockOut(cEm10* em);
static void em10_R1_Dm_Down(cEm10* em);
static void em10_R1_Dm_Frame(cEm10* em);
static void em10_R1_Dm_TakeAway(cEm10* em);
static void em10_R1_Die_Cramp(cEm10* em);
static void em10_R1_Die_Lost(cEm10* em);
static void em10_R1_Die_Down(cEm10* em);
static void em10_R1_Die_Normal(cEm10* em);
static void em10_R1_Die_RunDown(cEm10* em);
static void em10_R1_Die_Bomb(cEm10* em);
static void plemDmFrame(cPlayer* pl);
static void plem10DmGondolaShake(cPlayer* pl);
static void subem10DmGondolaShake(cSubChar* sub);
static void plemDmMStar(cPlayer* pl);
static void plemDmStun(cPlayer* pl);
static void em10KickAction(cEm10* em);
static void em10KneeDownAction(cEm10* em);
static void plem10Kick(cPlayer* pl);
static void plem10Kick2(cPlayer* pl);
static void em10FSAction(cEm10* em);
static void plem10FS(cPlayer* pl);
static void plem10KneeKick(cPlayer* pl);
static void plem10NeckBreak(cPlayer* pl);
static void plem10Showtay(cPlayer* pl);
static void em10TradeAction(cEm10* em);
extern "C" int em10PlRunCk(cEm10* em);
extern "C" void em10SackSet(cEm10* em);
extern "C" int em10SearchParasite(cEm10* em);

// em10ScytheAtkCk / em10SukiAtkCk share one body apart from the weapon kind and the attack routine.
#define EM10_WEP_ATK_CK(em, w, kind, rtn)                                                          \
    {                                                                                              \
        Vec a;                                                                                     \
        Vec b;                                                                                     \
        int hit;                                                                                   \
        u8 r;                                                                                      \
        if (w->wepType != kind) {                                                                  \
            return 0;                                                                              \
        }                                                                                          \
        if (w->pWep == 0) {                                                                        \
            return 0;                                                                              \
        }                                                                                          \
        if (w->x67C != 0) {                                                                        \
            return 0;                                                                              \
        }                                                                                          \
        if (w->x58C != 0) {                                                                        \
            return 0;                                                                              \
        }                                                                                          \
        if (w->pParasite != 0) {                                                                   \
            return 0;                                                                              \
        }                                                                                          \
        if (w->flags & 0x80) {                                                                     \
            return 0;                                                                              \
        }                                                                                          \
        if (Ctrl12Ck(w->pCtrl12, 6)) {                                                             \
            return 0;                                                                              \
        }                                                                                          \
        if (!(w->flags & 1)) {                                                                     \
            return 0;                                                                              \
        }                                                                                          \
        if (w->x508 > 0.7853982f) {                                                                \
            return 0;                                                                              \
        }                                                                                          \
        if (fabsf(em->pos.y - pPL->pos.y) > 1500.0f) {                                             \
            return 0;                                                                              \
        }                                                                                          \
        if (em->plDist2 > 4000000.0f) {                                                            \
            if (!em10PlRunCk(em)) {                                                                \
                return 0;                                                                          \
            }                                                                                      \
            if (em->plDist2 > 20250000.0f) {                                                       \
                return 0;                                                                          \
            }                                                                                      \
        }                                                                                          \
        if (pG->x4F88 <= 3) {                                                                      \
            if (!em10ScreenInCk(em)) {                                                             \
                return 0;                                                                          \
            }                                                                                      \
        }                                                                                          \
        a = em->pos;                                                                               \
        b = pPLS->pos;                                                                             \
        a.y += 1500.0f;                                                                            \
        b.y += 1500.0f;                                                                            \
        hit = EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x4000);                                            \
        if (hit) {                                                                                 \
            return 0;                                                                              \
        }                                                                                          \
        if (pG->x4F88 <= 1 && !EM_RTN(em, 1, 0x1B) && (r = Rnd() % 10, r > 4)) {                   \
            w->x67C = 30;                                                                          \
            EmRoutineSet(em, 1, 0x1B, hit, hit);                                                   \
            return 1;                                                                              \
        }                                                                                          \
        EmRoutineSet(em, 1, rtn, 0, 0);                                                            \
        if (pG->x4F88 <= 3) {                                                                      \
            Ctrl12Set(w->pCtrl12, 6, 60);                                                          \
            Ctrl12Set(w->pCtrl12, 8, 120);                                                         \
        } else if (pG->stage_no <= 2 && pG->x4F88 <= 9) {                                          \
            Ctrl12Set(w->pCtrl12, 6, 30);                                                          \
            Ctrl12Set(w->pCtrl12, 8, 120);                                                         \
        }                                                                                          \
        return 1;                                                                                  \
    }
// Parts (cModel-shaped) fields model.h does not name: the rotation offset Vec at 0x128 and the flag word at 0x1C0.
#define PARTS_ROT_OFS(p) (*(Vec*) ((u8*) (p) + 0x128))
#define PARTS_FLAGS(p) (*(u32*) ((u8*) (p) + 0x1C0))
#define EMI_DATA ((EmiData*) pG->pRoomEmi)

// EstSet with the enemy as owner argument (esp.h declares the int form).
void EstSetEm(cModel* em, int b, Vec* pos, Vec* rot, int c, int d, int e, int f, cModel* g, void* h) asm("EstSet");

// COMPILER-DIFF: narrow-argument truncation (docs/matching.md item 4). The original passes -1 to the u16
// count of Ctrl12CntAdd as `li r5, -1`; an int-view declaration reproduces it.
void Ctrl12CntAddI(cCtrl* c, int idx, int add) asm("Ctrl12CntAdd__FP5cCtrliUs");
// COMPILER-DIFF: narrow-argument extension (docs/matching.md item 2): the s16 wait time is sign-extended.
void Ctrl12SetS(cCtrl* c, int idx, s16 val) asm("Ctrl12Set__FP5cCtrliUs");
// COMPILER-DIFF: narrow-argument truncation (docs/matching.md item 4): int-view of the u16 se number / block.
u32 Ctrl11SetSe2I(cCtrl* c, cModel* m, s16 time, int no, int idx, int blk) asm("Ctrl11SetSe2__FP5cCtrlP6cModelsUsiUs");
// COMPILER-DIFF: narrow-argument truncation (docs/matching.md item 4): int-view of em10CallVoiceSe's u16 se number (em10SetDamageVoice).
extern "C" void em10CallVoiceSeI(cEm10* em, int no) asm("em10CallVoiceSe");
// COMPILER-DIFF: argument-move order (docs/matching.md item 1): em10_R1_R10FGondola's setThrow issues the
// `addi r5, Em10AtkTbl` before `fmr f1, t`; the GPR-args-first redeclaration is ABI-identical.
void cEmWepSetThrowF(cEmWep* wep, Vec* spd, EmAtkInfo* atk, f32 grav) asm("setThrow__6cEmWepP3VecfP9EmAtkInfo");

// Helpers of this unit used before their definition.
int em10CrashCk(cEm10* em);
int em10LostHead(cEm10* em, int a, int b);
void em10BloodSet(cEm10* em, int a);
int em10SetDmVal(cEm10* em);
void em10CoreBreak(cEm10* em, int a);
extern "C" void em10ParasiteGoOut(cEm10* em);
int em10FindCk2(cEm10* em);
void em10KickHitMark(cEm10* em);
int em10RoofDmCk(cEm10* em);
void em10SetPoint(cEm10* em);
void em1cBloodSet(cEm10* em, int near);
int em10ArmorCk(cEm10* em, int parts);
int em10ChgParasiteCk(cEm10* em);
int em10LostHeadCk(cEm10* em);
int em10ModelInit(cEm10* em);
void em10InitRtnSet(cEm10* em);
void Em1fClothSet(cModel* m, PlCloth* c);
extern "C" int EspDataLoad(u32 addr, u32 owner, int flag);
extern "C" cObj* SetObj16(void* bin, void* tpl, cModel* target, cModel* body, int partsNo, u8 type, Vec* pos, Vec* rot);
void em10SetWaitMotion(cEm10* em, int a);
void em10SetWalkMotion(cEm10* em, int a);
void em10BeltSet(cEm10* em);
void em10ChainSet(cEm10* em);
int em10GotoCk(cEm10* em);
int em10FindCk(cEm10* em, int a);
void em10FindNotify(cEm10* em);
void em10WalkRtnSet(cEm10* em);
void em10HandSet(cEm10* em, int a);
void em10ActEvtSetTrade(cEm10* em);
extern "C" void em10ActEvtSetKick(cEm10* em);
extern "C" void em10ActEvtSetFS(cEm10* em);
int em10AtkRtnCk(cEm10* em, int a);
void em10SetDashMotion(cEm10* em);
void em10HeadSet(cEm10* em, int a);
void em10DragonFireCk(cEm10* em);
int em10JumpDownCk(cEm10* em);
cObjGondola* em10GetGondola(cEm10* em);
void em10CallVoiceSe2(cEm10* em, int no, int a);
cEmWep* em10MakeWeapon(cEm10* em, int type);
void em10WeaponSet(cEm10* em);
int em10ClimbOverCk(cEm10* em);
void em10SetDamageVoice(cEm10* em, int a, int b);
extern "C" void OSReport(const char* fmt, ...);
void em10RouteCk(cEm10* em);
void em10ClawMove(cEm10* em);
void em10NeckMove(cEm10* em);
void em10WaistMove(cEm10* em);
void em10SlopeMove(cEm10* em);
void em10ScaleCompress(cEm10* em);
void em10BombNeckMove(cEm10* em);
void em10ChainSawMove(cEm10* em);
void Em1fClothMove(cModel* m, PlCloth* c);
void em10BowgunMove(cEm10* em);
void em10SetParasite(cEm10* em);
void em10SetWaterEff(cEm10* em);
void em10FootSe(cEm10* em);
void em10GatlingRollMove(cEm10* em);
int em10CsawHitCk(cEm10* em);
int em10DoorOpenCk(cEm10* em, int a);
int em10RackBreakCk(cEm10* em);
int em10LadderClimbCk(cEm10* em);
int em10VLadderClimbCk(cEm10* em);
int em10LadderResetCk(cEm10* em);
int em10JumpCk(cEm10* em);
int em10WindowCk(cEm10* em);
int em10CatchCk(cEm10* em);
int em10SomebodyNearCk(cEm10* em);
u32 em10GetWanderRoute(cEm10* em);
int em10CatchSubCk(cEm10* em);
void em10ReturnStartPosCk(cEm10* em);
void em10BreathSe(cEm10* em);
void em10SetCrash(cEm10* em, f32 r);
void em10MouthPartsReset(cEm10* em);
void em10SetDmWaterEff(cEm10* em, int a);
void em10BombThrow(cEm10* em);
int em10ThrowBombCk(cEm10* em);
int em10SetDamageDoor(cEm10* em, int kind);
void em10SetDamageRack(cEm10* em, int a);
int em10AtkCk(cEm10* em, Vec* a, Vec* b, int c, int d);
int em10IgnitionCk(cEm10* em);
int em10ClawStickCK(cEm10* em);
int em10FindLostCk(cEm10* em);
int em10GotoPosCk(cEm10* em);
extern "C" int em10ReturnPosCk(cEm10* em);
extern "C" void em10ClothPartsSet(cEm10* em, int on);
extern "C" void em10GoodsPartsSet(cEm10* em, int on);
extern "C" int em10AtkDoorCk(cEm10* em);
extern "C" int em10AtkRackCk(cEm10* em);
extern "C" void Em10SetSeTbl(cEm10* em, int type);
extern "C" void em10WeaponSet2(cEm10* em);
extern "C" int em10CatchSubRtnCk(cEm10* em);
extern "C" void em10SetRtnFind(cEm10* em);
extern "C" int em10BullJumpCk(cEm10* em);
extern "C" cModel* em10SearchTruck(cEm10* em);
extern "C" void em10CallVoiceSe(cEm10* em, u16 no);
extern "C" int em10RouteTargetSet(cEm10* em);
extern "C" int em10SomebodyDamageNowCk(cEm10* em);
extern "C" int em10TorchFrameAtkCk(cEm10* em);
extern "C" int em10TorchFrameAtkCkSub(cEm10* em);
extern "C" void em10PlHeadLost();
extern "C" f32 em10GetPower(cEm10* em);
extern "C" void em10WepSeEffSet(cEm10* em, cEmWep* wep, int type);
extern "C" int em10ThrowScaCk(cEm10* em);
extern "C" int em10ThrowNearCk(cEm10* em);
extern "C" int em10WindowCk2(cEm10* em);
extern "C" int em10ClimbOverCk2(cEm10* em);
extern "C" void cModel_swapModelInfo(cModel* m, ModelData* old, cModelInfo* info) asm("swapModelInfo__6cModelP9ModelDataP10cModelInfo");
extern "C" void plem10KickCamMove(cPlayer* pl, int a);
int em10HideRtnCk2(cEm10* em);
extern "C" void em10SetAccesory(cEm10* em);
extern "C" void em10WeaponInit(cEm10* em);
extern "C" void em10ShieldSet(cEm10* em);
extern "C" int em10DootAtkCk(cEm10* em);
extern "C" int em10ScreenInCk(cEm10* em);
extern "C" int em10SetWanderRoute(cEm10* em);
extern "C" void em10CamMoveTakeaway(cEm10* em);
extern "C" void em10CamMoveCri(cEm10* em, u32 no, int shake);
extern "C" void em10CamMove(cEm10* em, int no, f32 rate, int shake);
extern "C" void em10SetCampos2(cEm10* em);
void em10CamMove2(cEm10* em);
extern "C" void em10SetAtkWait(cEm10* em, int set);
extern "C" void em10CamMoveAshley(cEm10* em, u32 no);
extern "C" void em10SetTakeawayPos(cEm10* em);
extern "C" int em10JumpDownCk2(cEm10* em);
extern FootShadowTbl Em10_fs_tbl;
extern "C" void em10BellAtkCk(cEm10* em, Vec* pos, u32 no);
extern "C" int em10ShotGatlingCk(cEm10* em);
extern "C" int em10GoSubStayCk(cEm10* em);
extern "C" int em10ReturnCk(cEm10* em);
extern "C" int em10BombThrowScaCk(cEm10* em);
extern "C" void em10CsawSignSe(cEm10* em);
extern "C" int em10ShotBowgunCk(cEm10* em);
extern "C" int em10ThreatCk(cEm10* em);
extern "C" int em10ClawCriAtkCk(cEm10* em);
extern "C" void em10SetTakeawayPosUpdate(cEm10* em);
int em10HideToStepCk(cEm10* em, int a);
extern "C" int em10ParasiteAtkCk(cEm10* em);
extern "C" int em10ShieldAtkCk(cEm10* em);
extern "C" int em10AxeAtkCk(cEm10* em);
extern "C" int em10SukiAtkCk(cEm10* em);
extern "C" int em10ScytheAtkCk(cEm10* em);
extern "C" int em10ClawAtkCk(cEm10* em);
extern "C" int em10ShotRocketCk(cEm10* em);
extern "C" int em10ThrowAxeCk(cEm10* em);
extern "C" int em10CsawAtkCk(cEm10* em);
extern "C" int em10CatchPLRtnCk(cEm10* em);
extern "C" int em10BackCk(cEm10* em);
extern "C" int em10StayCk(cEm10* em);
extern "C" int em10DashCk(cEm10* em);
extern "C" int em10HeadLockCk(cEm10* em);
extern "C" void em10BehindSeCk(cEm10* em);
extern "C" void em10FallWaterCk(cEm10* em);
extern "C" void em10BlendMotSet(cEm10* em, void* m0, void* m1, void* m2, int a, int b, int c, int d);
extern "C" int em10HideRtnCk(cEm10* em);
extern "C" int em10GatlingHitCk(cEm10* em);

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em10DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

// Reference store (same mechanism as FSet): keeps the following global load after the store.
static inline void U16Set(u16& d, u16 v) { d = v; }
// Same for an int work field (Dm_Roof: `w->x20 = 1` before the pG load of the water-effect room check).
static inline void IntSet(int& d, int v) { d = v; }

static inline int em10DmgDeadCk(cDmgInfo* d)
{
    return (*(u32*) d & 0xFFFF0000) ? 1 : 0;
}

#define EM10_WINDOW(w) ((w)->pWindow)

// Routine test on the cModel status word (xFC / xFD as the upper half of `stat`).
#define EM_RTN(em, fc, fd) (((em)->stat & 0xFFFF0000) == (u32) (((fc) << 24) | ((fd) << 16)))

// The bell / rung point (emwep.cpp): the byte-pointer copy keeps the pG reload before the next store.
#define SET_BELL_POS(pos) memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), pos, sizeof(Vec))

// Routine bytes written through an int inline (player.cpp PlRoutineSet): the stores come out
// in the target's order.
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

#define G_ROOM_ID32 (*(u32*) &pG->stage_no)

// Struct-member view of pSys (global.h pGS): its load stays below a preceding store (em10_R1_C_SawHit).
struct SystemWorkPtr {
    SystemWork* p;
};
#define pSysS (((SystemWorkPtr*) &pSys)->p)
// Same for pSUB: its load stays below the preceding member stores and is redone after the flag store (em10_R1_TakeAway).
struct SubCharPtr {
    cSubChar* p;
};
#define pSUBS (((SubCharPtr*) &pSUB)->p)
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)
#define EM_RTN_SET(em, fc, fd) ((em)->stat = (u32) (((fc) << 24) | ((fd) << 16)))

Em10Func Em10SetFunc = 0;

static Em10Func Em10_R0_move_tbl[5] = {
    em10_R0_Init,
    em10_R0_Move,
    em10_R0_Damage,
    em10_R0_Die,
    (Em10Func) Em_R0_Scenario,
};

// Routine 1 handlers: the branch check and the move of each sub-routine (cModel::xFD).
static Em10Func Em10_R1_move_tbl[220] = {
    em10_R1_br_Wait, em10_R1_Wait,  // 0x00
    em10_R1_br_Dummy, em10_R1_Keeper,  // 0x01
    em10_R1_br_Dummy, em10_R1_Hide,  // 0x02
    em10_R1_br_Dummy, em10_R1_HideFall,  // 0x03
    em10_R1_br_Dummy, em10_R1_HideJump,  // 0x04
    em10_R1_br_Dummy, em10_R1_R100TurnWalk,  // 0x05
    em10_R1_br_Dummy, em10_R1_R100Cliff,  // 0x06
    em10_R1_br_Dummy, em10_R1_R101Bucket,  // 0x07
    em10_R1_br_Dummy, em10_R1_R101Suki,  // 0x08
    em10_R1_br_Dummy, em10_R1_R101Cart,  // 0x09
    em10_R1_br_EvtDash, em10_R1_EvtDash,  // 0x0A
    em10_R1_br_EvtWalk, em10_R1_EvtWalk,  // 0x0B
    em10_R1_br_Dummy, em10_R1_Pickup,  // 0x0C
    em10_R1_br_Dummy, em10_R1_Find,  // 0x0D
    em10_R1_br_Dummy, em10_R1_C_SawStart,  // 0x0E
    em10_R1_br_Dummy, em10_R1_BombIgnition,  // 0x0F
    em10_R1_br_Walk, em10_R1_Walk,  // 0x10
    em10_R1_br_Dash, em10_R1_Dash,  // 0x11
    em10_R1_br_Back, em10_R1_Back,  // 0x12
    em10_R1_br_Goto, em10_R1_Goto,  // 0x13
    em10_R1_br_Dummy, em10_R1_GuardWalk,  // 0x14
    em10_R1_br_Dummy, em10_R1_Turn180,  // 0x15
    em10_R1_br_Dummy, em10_R1_Threat,  // 0x16
    em10_R1_br_Dummy, em10_R1_SideStep,  // 0x17
    em10_R1_br_Dummy, em10_R1_HideSide,  // 0x18
    em10_R1_br_Dummy, em10_R1_AppearSide,  // 0x19
    em10_R1_br_Dummy, em10_R1_SitDown,  // 0x1A
    em10_R1_br_Dummy, em10_R1_Stay,  // 0x1B
    em10_R1_br_Dummy, em10_R1_RoofWait,  // 0x1C
    em10_R1_br_Dummy, em10_R1_Guard,  // 0x1D
    em10_R1_br_Dummy, em10_R1_DownWakeWait,  // 0x1E
    em10_R1_br_Dummy, em10_R1_DownWake,  // 0x1F
    em10_R1_br_Dummy, em10_R1_ParasiteAtk,  // 0x20
    em10_R1_br_Dummy, em10_R1_ShotBowgun,  // 0x21
    em10_R1_br_Dummy, em10_R1_ShotRocket,  // 0x22
    em10_R1_br_Dummy, em10_R1_ShotGatling,  // 0x23
    em10_R1_br_Dummy, em10_R1_ThrowAxe,  // 0x24
    em10_R1_br_Dummy, em10_R1_ThrowBomb,  // 0x25
    em10_R1_br_Dummy, em10_R1_AxeAtk,  // 0x26
    em10_R1_br_Dummy, em10_R1_ShieldAtk,  // 0x27
    em10_R1_br_Dummy, em10_R1_TorchFrame,  // 0x28
    em10_R1_br_Dummy, em10_R1_SukiAtk,  // 0x29
    em10_R1_br_Dummy, em10_R1_ScytheAtk,  // 0x2A
    em10_R1_br_Dummy, em10_R1_ClawAtk,  // 0x2B
    em10_R1_br_Dummy, em10_R1_ClawWalkAtk,  // 0x2C
    em10_R1_br_ClawCriAtk, em10_R1_ClawCriAtk,  // 0x2D
    em10_R1_br_Dummy, em10_R1_ClawCriHit,  // 0x2E
    em10_R1_br_C_SawAtk, em10_R1_C_SawAtk,  // 0x2F
    em10_R1_br_Dummy, em10_R1_C_SawHit,  // 0x30
    em10_R1_br_C_SawCriAtk, em10_R1_C_SawCriAtk,  // 0x31
    em10_R1_br_Dummy, em10_R1_C_SawCriHit,  // 0x32
    em10_R1_br_Catch, em10_R1_Catch,  // 0x33
    em10_R1_br_Dummy, em10_R1_NeckHang,  // 0x34
    em10_R1_br_Dummy, em10_R1_NeckHang_Luis,  // 0x35
    em10_R1_br_Dummy, em10_R1_NeckHang_Ashley,  // 0x36
    em10_R1_br_Dummy, em10_R1_Backhold,  // 0x37
    em10_R1_br_Dummy, em10_R1_Bombhold,  // 0x38
    em10_R1_br_DashCatch, em10_R1_DashCatch,  // 0x39
    em10_R1_br_Dummy, em10_R1_TakeAway,  // 0x3A
    em10_R1_br_Dummy, em10_R1_Crash,  // 0x3B
    em10_R1_br_Dummy, em10_R1_ClimbOver,  // 0x3C
    em10_R1_br_Dummy, em10_R1_DoorAtk,  // 0x3D
    em10_R1_br_Dummy, em10_R1_RackAtk,  // 0x3E
    em10_R1_br_Dummy, em10_R1_WindowAtk,  // 0x3F
    em10_R1_br_Dummy, em10_R1_LadderClimb,  // 0x40
    em10_R1_br_Dummy, em10_R1_VLadderClimb,  // 0x41
    em10_R1_br_Dummy, em10_R1_LadderReset,  // 0x42
    em10_R1_br_Dummy, em10_R1_JumpDown,  // 0x43
    em10_R1_br_Dummy, em10_R1_Jump,  // 0x44
    em10_R1_br_Dummy, em10_R1_JumpUp,  // 0x45
    em10_R1_br_Dummy, em10_R1_Trade,  // 0x46
    em10_R1_br_Dummy, em10_R1_Drive,  // 0x47
    em10_R1_br_Dummy, em10_R1_Catapult,  // 0x48
    em10_R1_br_Dummy, em10_R1_RockPush,  // 0x49
    em10_R1_br_Dummy, em10_R1_R10CParasite,  // 0x4A
    em10_R1_br_Dummy, em10_R1_R10CPCancel,  // 0x4B
    em10_R1_br_Dummy, em10_R1_R100WalkStay,  // 0x4C
    em10_R1_br_Dummy, em10_R1_R202Finger,  // 0x4D
    em10_R1_br_Dummy, em10_R1_StayWalk,  // 0x4E
    em10_R1_br_Dummy, em10_R1_AttackWait,  // 0x4F
    em10_R1_br_Dummy, em10_R1_FixBomber,  // 0x50
    em10_R1_br_Dummy, em10_R1_R204Prayer,  // 0x51
    em10_R1_br_Dummy, em10_R1_R222DragonA,  // 0x52
    em10_R1_br_Dummy, em10_R1_R222DragonB,  // 0x53
    em10_R1_br_Dummy, em10_R1_R222DragonC,  // 0x54
    em10_R1_br_Dummy, em10_R1_R227Barrel,  // 0x55
    em10_R1_br_Dummy, em10_R1_R10FGJump,  // 0x56
    em10_R1_br_Dummy, em10_R1_R10FGondola,  // 0x57
    em10_R1_br_Dummy, em10_R1_R209DashSit,  // 0x58
    em10_R1_br_Dummy, em10_R1_StickClaw,  // 0x59
    em10_R1_br_Dummy, em10_R1_R11DAppear1,  // 0x5A
    em10_R1_br_Dummy, em10_R1_R11DAppear2,  // 0x5B
    em10_R1_br_Dummy, em10_R1_R212Drill,  // 0x5C
    em10_R1_br_Dummy, em10_R1_FindLost,  // 0x5D
    em10_R1_br_Dummy, em10_R1_R201EventWait,  // 0x5E
    em10_R1_br_Dummy, em10_R1_R209Gatling,  // 0x5F
    em10_R1_br_Dummy, em10_R1_RocketWait,  // 0x60
    em10_R1_br_Dummy, em10_R1_R21BTrolleyJump,  // 0x61
    em10_R1_br_Dummy, em10_R1_R21BTrolleyJump2,  // 0x62
    em10_R1_br_Dummy, em10_R1_R303FireDash,  // 0x63
    em10_R1_br_Dummy, em10_R1_Work,  // 0x64
    em10_R1_br_Dummy, em10_R1_UFOCatch,  // 0x65
    em10_R1_br_Dummy, em10_R1_R300TakeAshley,  // 0x66
    em10_R1_br_Dummy, em10_R1_R30FBullJump,  // 0x67
    em10_R1_br_Dummy, em10_R1_R320Gatling,  // 0x68
    em10_R1_br_Dummy, em10_R1_R300Gatling,  // 0x69
    em10_R1_br_Dummy, em10_R1_R305Bomber,  // 0x6A
    em10_R1_br_Dummy, em10_R1_R321DeadBody,  // 0x6B
    em10_R1_br_Dummy, em10_R1_R408Bomber,  // 0x6C
    em10_R1_br_CSawWalkAtk, em10_R1_CSawWalkAtk,  // 0x6D
};

static Em10Func Em10_R1_dmg_tbl[22] = {
    em10_R1_Dm_Small,
    em10_R1_Dm_Head,
    em10_R1_Dm_DashUp,
    em10_R1_Dm_DashDown,
    em10_R1_Dm_Blow,
    em10_R1_Dm_Fence,
    em10_R1_Dm_Ladder,
    em10_R1_Dm_Roof,
    em10_R1_Dm_KneeDown,
    em10_R1_Dm_KnockOut,
    em10_R1_Dm_Down,
    em10_R1_Dm_Frame,
    em10_R1_Dm_TakeAway,
    em10_R1_Dm_Flash,
    em10_R1_Dm_Claw,
    em10_R1_Dm_Claw_Big,
    em10_R1_Dm_FS,
    em10_R1_Dm_Gatling,
    em10_R1_Dm_Showtay,
    em10_R1_Dm_Heel,
    em10_R1_Dm_KneeKick,
    em10_R1_Dm_NeckBreak,
};

static Em10Func Em10_R1_die_tbl[6] = {
    em10_R1_Die_Cramp,
    em10_R1_Die_Down,
    em10_R1_Die_Normal,
    em10_R1_Die_Lost,
    em10_R1_Die_RunDown,
    em10_R1_Die_Bomb,
};

// Damage reaction per weapon id (cEm::dmWep).
static Em10Func Em10DmSetWep_tbl[46] = {
    em10DmSetWep00, em10DmSetWep02, em10DmSetWep02, em10DmSetWep02,
    em10DmSetWep02, em10DmSetWep09, em10DmSetWep09, em10DmSetWep03,
    em10DmSetWep03, em10DmSetWep02, em10DmSetWep02, em10DmSetWep02,
    em10DmSetWep02, em10DmSetWep09, em10DmSetWep02, em10DmSetWep09,
    em10DmSetWep02, em10DmSetWep02, em10DmSetWep09, em10DmSetWep09,
    em10DmSetWep00, em10DmSetWep02, em10DmSetWep02, em10DmSetWep23,
    em10DmSetWep02, em10DmSetWep02, em10DmSetWep02, em10DmSetWep02,
    em10DmSetWep09, em10DmSetWep02, em10DmSetWep02, em10DmSetWep02,
    em10DmSetWep02, em10DmSetWep03, em10DmSetWep00, em10DmSetWep00,
    em10DmSetWep00, em10DmSetWep00, em10DmSetWep02, em10DmSetWep02,
    em10DmSetWep02, em10DmSetWep09, em10DmSetWep23, em10DmSetWep02,
    em10DmSetWep09, em10DmSetWep09,
};

// ---------------------------------------------------------------------------------------------------

cEm10::~cEm10()
{
    Em10Work* w = EM10_WK(this);

    if (w->pWep) {
        if (w->pWep->isAlive()) {
            EmMgr.destroy(w->pWep);
        }
        w->pWep = 0;
        w->wepType = 0;
    }
    if (w->pWep2) {
        if (w->pWep2->isAlive()) {
            EmMgr.destroy(w->pWep2);
        }
        w->pWep2 = 0;
        w->wep2Type = 0;
    }
    if (w->x178) {
        if (w->x178->isAlive()) {
            ObjMgr.destroy(w->x178);
        }
        w->x178 = 0;
    }
    if (w->x17C) {
        if (w->x17C->isAlive()) {
            ObjMgr.destroy(w->x17C);
        }
        w->x17C = 0;
    }
    if (w->pHead && w->mot[51]) {
        MotionSetCore(w->pHead, MOTION(w->pHead), w->mot[51], 0, 0, 0, 0);
        atariInitF(&w->pHead->atari, 0.0f, 150.0f, -300.0f, 300.0f, 300.0f, 300.0f, 150.0f, 1, 0x2000, 10);
        w->pHead->atari.flags &= ~0x100;
        w->pHead->atari.flags |= 0x200;
        w->pHead->atari.flags |= 0x10;
        w->pHead = 0;
    }
    if (w->wepType == 4) {
        SndStop(w->sndId, 0);
    }
    if (w->x590) {
        if (w->x590->isAlive()) {
            ObjMgr.destroy((cObj*) w->x590);
        }
        w->x590 = 0;
    }
    if (w->x594) {
        if (w->x594->isAlive()) {
            ObjMgr.destroy((cObj*) w->x594);
        }
        w->x594 = 0;
    }
    if (w->pParasite) {
        if (w->pParasite->isAlive()) {
            ObjMgr.destroy(w->pParasite);
        }
        w->pParasite = 0;
    }
    if (w->x58C) {
        if (w->x58C->isAlive()) {
            w->x58C->setReset();
        }
        w->x58C = 0;
    }
}

void cEm10::setNoSuspend(int on)
{
    Em10Work* w = EM10_WK(this);
    u32 i;

    if (on) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
    if (w->pWep) {
        if (w->pWep->isAlive()) {
            w->pWep->setNoSuspend(on);
        } else {
            w->pWep = 0;
        }
    }
    if (w->pWep2) {
        if (w->pWep2->isAlive()) {
            w->pWep2->setNoSuspend(on);
        } else {
            w->pWep2 = 0;
        }
    }
    if (w->x178) {
        if (w->x178->isAlive()) {
            w->x178->setNoSuspend(on);
        } else {
            w->x178 = 0;
        }
    }
    if (w->x17C) {
        if (w->x17C->isAlive()) {
            w->x17C->setNoSuspend(on);
        } else {
            w->x17C = 0;
        }
    }
    if (w->pHead) {
        if (w->pHead->isAlive()) {
            w->pHead->setNoSuspend(on);
        } else {
            w->pHead = 0;
        }
    }
    if (w->pShield) {
        if (w->pShield->isAlive()) {
            w->pShield->setNoSuspend(on);
        } else {
            w->pShield = 0;
        }
    }
    if (w->x590) {
        if (w->x590->isAlive()) {
            w->x590->setNoSuspend(on);
        } else {
            w->x590 = 0;
        }
    }
    if (w->x594) {
        if (w->x594->isAlive()) {
            w->x594->setNoSuspend(on);
        } else {
            w->x594 = 0;
        }
    }
    if (w->x58C) {
        if (w->x58C->isAlive()) {
            w->x58C->setNoSuspend(on);
        } else {
            w->x58C = 0;
        }
    }
    if (w->pParasite) {
        if (w->pParasite->isAlive()) {
            w->pParasite->setNoSuspend(on);
        } else {
            w->pParasite = 0;
        }
    }
    for (i = 0; i < 5; i++) {
        if (w->x578[i]) {
            if (w->x578[i]->isAlive()) {
                w->x578[i]->setNoSuspend(on);
            } else {
                w->x578[i] = 0;
            }
        }
    }
}





void em10DmCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int dmg;

    if (em10CrashCk(em)) {
        return;
    }
    if ((em->be_flag & 2) && !em10DeadCk(em) && em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            if (w->x68C == 0) {
                if (w->pGatling) {
                    w->x68C = 120;
                    EstSetEm(em, -1, 0, 0, 0x10, 0x1E, 0, 0, em, 0);
                    LifeDownSet(em, 1200, 0);
                    if (em->hp > 0) {
                        if (w->x6C3 == 0) {
                            em->xFE = 4;
                        }
                    } else {
                        em->xFE = 6;
                    }
                    return;
                }
                if (EM_RTN(em, 1, 0x5E) && (w->flags & 8)) {
                    w->x68C = 120;
                    EstSetEm(em, -1, 0, 0, 0x10, 0x1E, 0, 0, em, 0);
                    return;
                }
                w->x68C = 120;
                EstSetEm(em, -1, 0, 0, 0x10, 0x1E, 0, 0, em, 0);
                if (w->flags & 0x4000) {
                    LifeDownSet(em, 500, 0);
                    EmRoutineSet(em, 2, 0xC, 0, 0);
                } else if (w->flags & 0x20000) {
                    LifeDownSet(em, 500, 0);
                    EmRoutineSet(em, 2, 5, 0, 0);
                } else if (w->flags & 0x10000) {
                    LifeDownSet(em, 500, 0);
                    EmRoutineSet(em, 2, 6, 0, 0);
                } else if (w->flags & 0x10000000) {
                    LifeDownSet(em, 500, 0);
                    if (em->hp > 0) {
                        return;
                    }
                    EmRoutineSet(em, 2, 4, 0, 0);
                } else if (w->flags & 0x100000) {
                    LifeDownSet(em, 500, 0);
                    EmRoutineSet(em, 2, 4, 0, 0);
                } else if (em->type == 0xA || em->type == 0xD) {
                    LifeDownSet(em, 200, 0);
                    if (em->hp <= 0) {
                        EmRoutineSet(em, 2, 0xB, 0, 1);
                    } else {
                        em->x328 = pPL->pos;
                        EmRoutineSet(em, 2, 0xE, 0, 0);
                    }
                } else if (em->type == 2) {
                    LifeDownSet(em, 200, 0);
                    if (em->hp <= 0) {
                        EmRoutineSet(em, 2, 0xB, 0, 1);
                    } else {
                        EmRoutineSet(em, 2, 0x11, 0, 0);
                    }
                } else if (w->flags & 0x10) {
                    LifeDownSet(em, 500, 0);
                    if (em->hp <= 0) {
                        EmRoutineSet(em, 3, 1, 0, 0);
                    } else {
                        w->x67A = 0;
                        EmRoutineSet(em, 2, 0xA, 0, 0);
                    }
                } else {
                    EmRoutineSet(em, 2, 0xB, 0, 1);
                }
                return;
            }
            break;
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    if ((w->flags & 0x200000) && w->pLadder) {
        w->pLadder->setDown2();
        w->pLadder = 0;
    }
    if (w->pTruck) {
        em->dmHit = 0;
        if (em->hp <= 0) {
            return;
        }
        em10LostHead(em, 0, 0);
        em->hp = 0;
        GameAddPoint(9);
        return;
    }
    if (w->x564) {
        em->dmHit = 0;
        if (em->hp <= 0) {
            return;
        }
        if (em->dmPart->partsNo == 5) {
            em10LostHead(em, 0, 0);
        } else {
            em10BloodSet(em, 0);
        }
        em->hp = 0;
        GameAddPoint(9);
        return;
    }
    if (EM_RTN(em, 1, 0x5E) && (w->flags & 8)) {
        em10BloodSet(em, 0);
        if (!(pG->flags_5010 & 0x20000000)) {
            BitOn(pG->flags_5010, 0x20000000);
            SET_BELL_POS(&em->pos);
            pG->bell_stat = 0;
        }
        em->dmHit = 0;
        return;
    }
    dmg = em10SetDmVal(em);
    LifeDownSet2(em, dmg, 0, 0);
    w->x686 -= dmg;
    if (w->x686 & 0x8000) {
        w->x686 = 0;
    }
    if (EM_RTN(em, 1, 0x5F)) {
        em->dmHit = 0;
        em10BloodSet(em, 0);
        if (em->hp > 0) {
            if (w->x6C3 == 0) {
                em->xFE = 4;
            }
        } else {
            em->xFE = 6;
        }
        return;
    }
    if (em->type == 6) {
        if (em->dmWep == 0x14) {
            return;
        }
        if (em->dmWep == 0x16) {
            return;
        }
        if (em->dmWep == 0x17) {
            return;
        }
        if (em->dmWep == 0x2A) {
            return;
        }
        if (em->dmWep == 0xE) {
            return;
        }
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em10CoreBreak(em, 0);
    }
    if (em->dmWep <= 0x2D) {
        u32 no = em->dmWep;
        if ((em->dmWep == 9 || em->dmWep == 10) && w->wepType == 4) {
            no = 5;
        }
        Em10DmSetWep_tbl[no](em);
    } else {
        pLog->err(0, 0, "em10DmCk() -> Wep no over max!!!");
    }
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
    }
    if (em10FindCk2(em)) {
        w->flags &= ~0x20000000;
        em->setFindPL();
    }
    if ((s16) w->x660 < 300) {
        w->x660 = 300;
    }
    switch (w->x5EC) {
    case 6:
    case 7:
    case 8:
    case 10:
    case 11:
    case 12:
    case 13:
        if (em->type == 0xA || em->type == 0xD) {
            if (em10FindCk2(em)) {
                w->flags &= ~0x20000000;
                em->setFindPL();
            }
        } else {
            em->setFindPL();
            w->x5EC = 0;
        }
        break;
    }
    if (em->x3D0 == 0 || em->x3D0 == 2) {
        w->x6B8 = 1;
    }
    if (w->wepType == 9) {
        int parts = 9;
        EmHitInfo* part = em->dmPart;
        if (em->flags_3C8 & 0x1000000) {
            parts = 0xF;
        }
        if (part->partsNo == parts) {
            w->x640 = 3;
            GameAddPoint(9);
        }
    }
    if (!(pG->flags_5010 & 0x20000000)) {
        BitOn(pG->flags_5010, 0x20000000);
        SET_BELL_POS(&em->pos);
        pG->bell_stat = 0;
    }
    em->dmHit = 0;
}

static void em10DmSetWep00(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    EmHitInfo* part = em->dmPart;

    em->dmg.set(0, 8);
    if ((part->partsNo == 5 || part->partsNo == 0x25) && w->pParasite) {
        SndCall(8, 0x83, &em->pos, em->id, 0, em);
    } else {
        SndCall(8, 0xC, &em->pos, em->id, 0, em);
    }
    if (em->dmWep == 0x25) {
        EmRoutineSet(em, 2, 0x12, 0, 0);
        return;
    }
    em10KickHitMark(em);
    if ((s16) w->x682 != 0) {
        w->x68C = 120;
        EmRoutineSet(em, 2, 0xB, 0, 0);
    } else if (w->flags & 0x4000) {
        EmRoutineSet(em, 2, 0xC, 0, 0);
    } else if (w->flags & 0x20000) {
        if (em->hp > 0 && em->type == 0x16) {
            return;
        }
        EmRoutineSet(em, 2, 5, 0, 0);
    } else if (w->flags & 0x10000) {
        if (em->hp > 0 && em->type == 0x16) {
            return;
        }
        EmRoutineSet(em, 2, 6, 0, 0);
    } else if (w->flags & 0x10000000) {
        if (em->hp > 0) {
            return;
        }
        EmRoutineSet(em, 2, 4, 0, 0);
    } else if (w->flags & 0x100000) {
        EmRoutineSet(em, 2, 4, 0, 0);
    } else if (w->flags & 0x40000000) {
        if (pG->x4FB8 == 5) {
            EmRoutineSet(em, 2, 0x13, 0, 0);
        } else {
            EmRoutineSet(em, 2, 4, 0, 1);
        }
    } else if (em->hp <= 0) {
        em10LostHead(em, 0, 0);
        if ((w->flags & 0x40) && (part->partsNo == 0x13 || part->partsNo == 0x17 || part->partsNo == 0x14 || part->partsNo == 0x18)) {
            EmRoutineSet(em, 2, 3, 0, 0);
        } else {
            EmRoutineSet(em, 2, 4, 0, 1);
        }
    } else if (w->flags & 8) {
        w->x67A = 0;
    } else if (w->flags & 0x10) {
        w->x67A = 0;
        EmRoutineSet(em, 2, 0xA, 0, 0);
    } else if (em->dmWep == 0x24) {
        EmRoutineSet(em, 2, 0, 0, 0);
    } else if (pG->x4FB8 == 5) {
        EmRoutineSet(em, 2, 0x13, 0, 0);
    } else {
        EmRoutineSet(em, 2, 4, 0, 1);
    }
}

// Chainsaw / parasite / partner Ganados take a few hits before reacting: count the guard down,
// then re-arm it with 2..4 hits.
#define EM10_GUARD_CK(w)                                                                            \
    if ((w)->x6BB) {                                                                                \
        (w)->x6BB--;                                                                                \
        return;                                                                                     \
    }                                                                                               \
    (w)->x6BB = Rnd() % 3 + 2

static void em10DmSetWep02(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    EmHitInfo* part = em->dmPart;
    int react;
    int ret;
    int type;

    em->dmType = 1;
    if ((part->partsNo == 5 || part->partsNo == 0x25) && w->pParasite) {
        SndCall(8, 0x83, &em->pos, em->id, 0, em);
    } else if (em10ArmorCk(em, part->partsNo)) {
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
    } else {
        SndCall(8, 0xC, &em->pos, em->id, 0, em);
    }
    react = em10ArmorCk(em, part->partsNo) == 0;
    if (em->type == 0xA || em->type == 0xD) {
        if (Rnd() & 3) {
            react = 0;
        }
    }
    if ((s16) w->x682 != 0) {
        w->x68C = 120;
        EmRoutineSet(em, 2, 0xB, 0, 0);
    } else if (w->flags & 0x4000) {
        if (em->hp <= 0 && part->partsNo == 5 && em->dmWep != 0x10) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, 0);
            if (w->wepType == 4 || w->pParasite || w->x58C) {
                EM10_GUARD_CK(w);
            }
        }
        if (react) {
            EmRoutineSet(em, 2, 0xC, 0, 0);
        }
    } else if (w->flags & 0x20000) {
        if (em->hp <= 0 && part->partsNo == 5 && em->dmWep != 0x10) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, 0);
            if (em->type == 0x16) {
                return;
            }
            if (w->wepType == 4 || w->pParasite || w->x58C) {
                EM10_GUARD_CK(w);
            }
        }
        if (react) {
            EmRoutineSet(em, 2, 5, 0, 0);
        }
    } else if (w->flags & 0x10000) {
        if (em->hp <= 0 && part->partsNo == 5 && em->dmWep != 0x10) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, 0);
            if (em->type == 0x16) {
                return;
            }
            if (w->wepType == 4 || w->pParasite || w->x58C) {
                EM10_GUARD_CK(w);
            }
        }
        if (react) {
            EmRoutineSet(em, 2, 6, 0, 0);
        }
    } else if (w->flags & 0x10000000) {
        em10BloodSet(em, 0);
        if (em->hp > 0) {
            return;
        }
        EmRoutineSet(em, 2, 4, 0, 0);
    } else if (w->flags & 0x100000) {
        if (em->hp <= 0 && part->partsNo == 5 && em->dmWep != 0x10) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, 0);
        }
        if (react) {
            EmRoutineSet(em, 2, 4, 0, 0);
        }
    } else if (w->flags & 0x40000000) {
        if (em->hp <= 0 && part->partsNo == 5 && em->dmWep != 0x10) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, 0);
            if (em->type == 0x16) {
                return;
            }
            if (w->wepType == 4 || w->pParasite || w->x58C) {
                EM10_GUARD_CK(w);
            }
        }
        if (react) {
            EmRoutineSet(em, 2, 8, 0, 0);
        }
    } else {
        if (em10ChgParasiteCk(em)) {
            return;
        }
        if (em->hp <= 0) {
            EmSetDie(em);
            EmReserveDropItem(em);
            em10SetPoint(em);
            if (em->type == 0xA || em->type == 0xD || em->type == 2 || em->type == 0x16) {
                em10BloodSet(em, 0);
                EmRoutineSet(em, 3, 2, 0, 0);
                return;
            }
            if (w->flags & 0x10) {
                if (part->partsNo == 5 && em->dmWep != 0x10) {
                    em10LostHead(em, 1, 0);
                    GameAddPoint(9);
                } else {
                    em10BloodSet(em, 0);
                }
                EmRoutineSet(em, 3, 1, 0, 0);
                return;
            }
            if ((w->flags & 0x40) && (part->partsNo == 0x13 || part->partsNo == 0x17 || part->partsNo == 0x14 || part->partsNo == 0x18)) {
                EmRoutineSet(em, 2, 3, 0, 0);
                return;
            }
            if (em10RoofDmCk(em)) {
                em10BloodSet(em, 0);
                return;
            }
            if (part->partsNo == 5 && em->dmWep != 0x10) {
                u8 r;
                em10LostHead(em, 0, 0);
                GameAddPoint(9);
                r = Rnd() % 3;
                if (r == 0 || !em10LostHeadCk(em)) {
                    EmRoutineSet(em, 2, 4, 0, 0);
                    return;
                }
                if (em->xFC == 1 && (em->xFD == 0x10 || em->xFD == 0x39 || em->xFD == 0x33) && em10LostHeadCk(em) && w->wepType != 4) {
                    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01000000) {
                        em->xFC = 2;
                        em->xFD = 9;
                        em->xFF = em->xFE = 0;
                    }
                    if ((Rnd() & 0xF) == 5) {
                        w->x6C1 = 0x5A;
                        em->hp = 1;
                    }
                    return;
                }
                EmRoutineSet(em, 2, 9, 0, 0);
                return;
            }
            if (part->partsNo == 8 || part->partsNo == 0xE) {
                u8 r = Rnd() % 3;
                if (r != 0) {
                    EmRoutineSet(em, 2, 4, 0, 0);
                    BitOn(pG->flags_5010, 0x20000);
                    return;
                }
            }
            em->xFC = 3;
            em->xFD = 2;
            em->xFF = em->xFE = 0;
            em10BloodSet(em, 0);
            return;
        }
        if (w->flags & 8) {
            w->x67A = 0;
            em10BloodSet(em, 0);
            return;
        }
        if (w->flags & 0x10) {
            w->x67A = 0;
            em10BloodSet(em, 0);
            if (react) {
                EmRoutineSet(em, 2, 0xA, 0, 0);
            }
            return;
        }
        em10BloodSet(em, 0);
        ret = em10RoofDmCk(em);
        if (ret) {
            return;
        }
        type = em->type;
        if (type == 6) {
            em->xFC = 2;
            em->xFD = 0;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
        if (type == 2) {
            switch (em->dmWep) {
            default: {
                u8 r;
                if (part->partsNo != 5) {
                    return;
                }
                r = Rnd() % 10;
                if (r != 0) {
                    return;
                }
                em->xFC = 2;
                em->xFD = 1;
                em->xFE = 0;
                em->xFF = 0;
                return;
            }
            case 9:
            case 10:
                if (part->partsNo == 5) {
                    em->xFC = 2;
                    em->xFD = 1;
                    em->xFE = 0;
                    em->xFF = 0;
                    return;
                } else {
                    u8 r = Rnd() % 10;
                    if (r < 5) {
                        return;
                    }
                    em->xFC = 2;
                    em->xFD = 0x11;
                    em->xFE = 0;
                    em->xFF = 0;
                    return;
                }
            }
        }
        if (part->partsNo == 5 && w->pShield == 0) {
            if (w->wepType == 4) {
                EM10_GUARD_CK(w);
            }
            if (!react) {
                return;
            }
            if (w->pParasite && w->x6C5 == 1) {
                if (Rnd() & 3) {
                    EmRoutineSet(em, 2, 0, 0, 0);
                }
                return;
            }
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
        }
        if (part->partsNo == 0x25) {
            EmRoutineSet(em, 2, 0xF, 0, 0);
            return;
        }
        if (w->flags & 0x40) {
            if (w->wepType == 4 || w->pParasite || w->x58C) {
                EM10_GUARD_CK(w);
            }
            if (!react) {
                return;
            }
            if (part->partsNo == 0x13 || part->partsNo == 0x17 || part->partsNo == 0x14 || part->partsNo == 0x18) {
                EmRoutineSet(em, 2, 3, 0, 0);
            } else {
                EmRoutineSet(em, 2, 2, 0, 0);
            }
            return;
        }
        if (em->dmWep == 0x10 && pG->x4FB8 != 4) {
            u8 r = Rnd() % 3;
            if (r == 0) {
                return;
            }
            if (!react) {
                return;
            }
        }
        if (em->type == 0xA || em->type == 0xD) {
            return;
        }
        if (w->wepType == 4 || w->pParasite || w->x58C) {
            EM10_GUARD_CK(w);
        }
        if (em->type == 0x16) {
            return;
        }
        if (react) {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
    }
}

static void em10DmSetWep03(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    EmHitInfo* part = em->dmPart;
    int near = 0;

    em->dmType = 1;
    if (part->rad < 36000000.0f) {
        near = 1;
    }
    em10ArmorCk(em, part->partsNo);
    if (em->type == 0xA || em->type == 0xD || em->type == 2 || em->type == 0x16) {
        Rnd();
    }
    if ((part->partsNo == 5 || part->partsNo == 0x25) && w->pParasite) {
        SndCall(8, 0x83, &em->pos, em->id, 0, em);
    } else if (em10ArmorCk(em, part->partsNo)) {
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
    } else if (near) {
        SndCall(8, 0x7A, &em->pos, em->id, 0, em);
    } else {
        SndCall(8, 0xC, &em->pos, em->id, 0, em);
    }
    if ((s16) w->x682 != 0) {
        w->x68C = 120;
        EmRoutineSet(em, 2, 0xB, 0, 0);
    } else if (w->flags & 0x4000) {
        EmRoutineSet(em, 2, 0xC, 0, 0);
    } else if (w->flags & 0x20000) {
        if (em->hp <= 0 && part->partsNo == 5) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, near);
            if (em->type == 0x16) {
                return;
            }
        }
        EmRoutineSet(em, 2, 4, 0, 0);
    } else if (w->flags & 0x10000) {
        if (em->hp <= 0 && part->partsNo == 5) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, near);
            if (em->type == 0x16) {
                return;
            }
        }
        EmRoutineSet(em, 2, 6, 0, 0);
    } else if (w->flags & 0x10000000) {
        em10BloodSet(em, 0);
        if (em->hp > 0) {
            return;
        }
        EmRoutineSet(em, 2, 4, 0, 0);
    } else if (w->flags & 0x100000) {
        if (em->hp <= 0 && part->partsNo == 5) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, near);
        }
        EmRoutineSet(em, 2, 4, 0, 0);
    } else if (w->flags & 0x40000000) {
        if (em->hp <= 0 && part->partsNo == 5) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, near);
            if (em->type == 0x16) {
                return;
            }
        }
        EmRoutineSet(em, 2, 8, 0, 0);
    } else {
        if (em10ChgParasiteCk(em)) {
            return;
        }
        if (em->hp <= 0) {
            em10BloodSet(em, near);
            if (em->type == 0xA || em->type == 0xD || em->type == 2 || em->type == 0x16) {
                EmRoutineSet(em, 3, 2, 0, 0);
                return;
            }
            if (w->flags & 0x10) {
                if (part->partsNo == 5 && near) {
                    em10LostHead(em, 1, 0);
                    GameAddPoint(9);
                } else {
                    em10BloodSet(em, near);
                }
                EmRoutineSet(em, 3, 1, 0, 0);
                return;
            }
            if (part->partsNo == 5 && near) {
                em10LostHead(em, 0, 0);
                GameAddPoint(9);
                if (em->xFC == 1 && (em->xFD == 0x10 || em->xFD == 0x39 || em->xFD == 0x33) && em10LostHeadCk(em) && w->wepType != 4) {
                    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01000000) {
                        em->xFC = 2;
                        em->xFD = 4;
                        em->xFF = em->xFE = 0;
                    }
                    if ((Rnd() & 0xF) == 5) {
                        w->x6C1 = 0x5A;
                        em->hp = 1;
                    }
                    return;
                }
                EmRoutineSet(em, 2, 4, 0, 0);
                return;
            }
            em10BloodSet(em, near);
            {
                int ret = em10RoofDmCk(em);
                if (ret) {
                    return;
                }
                em->xFC = 2;
                em->xFD = 4;
                em->xFE = 0;
                em->xFF = 0;
                return;
            }
            em->xFC = 2;
            em->xFD = 4;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
        if (w->flags & 8) {
            w->x67A = 0;
            em10BloodSet(em, near);
            return;
        }
        if (w->flags & 0x10) {
            w->x67A = 0;
            em10BloodSet(em, near);
            EmRoutineSet(em, 2, 0xA, 0, 0);
            return;
        }
        if (w->pShield) {
            em10BloodSet(em, near);
            EmRoutineSet(em, 2, 0, 0, 0);
            return;
        }
        em10BloodSet(em, near);
        {
        int ret = em10RoofDmCk(em);
        if (ret) {
            return;
        }
        }
        if (part->partsNo == 0x25) {
            em->xFC = 2;
            em->xFD = 0xF;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
        if (near && em->type == 2) {
            int type = em->type;
            if (part->partsNo == 5) {
                em->xFC = type;
                em->xFD = 1;
                em->xFE = 0;
                em->xFF = 0;
                return;
            } else {
                u8 r = Rnd() % 10;
                if (r > 4) {
                    em->xFC = type;
                    em->xFD = 0x11;
                    em->xFE = 0;
                    em->xFF = 0;
                    return;
                }
            }
        }
        if (em->type == 0xA || em->type == 0xD || em->type == 2) {
            return;
        }
        if (near) {
            if (em->type == 0x16) {
                u8 r = Rnd() % 10;
                if (r <= 4 && part->partsNo != 5) {
                    return;
                }
                EmRoutineSet(em, 2, 0, 0, 0);
                return;
            }
            EmRoutineSet(em, 2, 4, 0, 0);
            return;
        }
        if (em->type == 0x16) {
            return;
        }
        if (w->flags & 0x40) {
            if (part->partsNo == 0x13 || part->partsNo == 0x17 || part->partsNo == 0x14 || part->partsNo == 0x18) {
                EmRoutineSet(em, 2, 3, 0, 0);
            } else {
                EmRoutineSet(em, 2, 2, 0, 0);
            }
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
    }
}

static void em10DmSetWep09(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    EmHitInfo* part = em->dmPart;
    cModel* parts;
    int mag;
    int type;

    switch (em->dmWep) {
    default:
        mag = 0;
        em->dmg.set(0, 5);
        break;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
        mag = 1;
        em->dmg.set(0, 0x1E);
        break;
    }
    if (em->dmWep == 0x2D) {
        em->hp = 0;
        EmRoutineSet(em, 3, 5, 0, 2);
        return;
    }
    parts = em->getPartsPtr(part->partsNo - 1);
    if ((part->partsNo == 5 || part->partsNo == 0x25) && w->pParasite) {
        SndCall(8, 0x83, &parts->worldPos, em->id, 0, em);
    } else if (em10ArmorCk(em, part->partsNo)) {
        SndCall(8, 0xD, &parts->worldPos, em->id, 0, em);
    } else if (em->dmWep == 0x1C) {
        SndCall(8, 0x81, &parts->worldPos, em->id, 0, em);
    } else {
        SndCall(8, 0xC, &parts->worldPos, em->id, 0, em);
    }
    if (w->wepType == 9) {
        switch (em->dmWep) {
        case 0xD:
        case 0x12:
        case 0x13:
        case 0x29:
            em->hp = 0;
            EmRoutineSet(em, 3, 5, 0, 0);
            return;
        }
    }
    if ((s16) w->x682 != 0) {
        w->x68C = 120;
        EmRoutineSet(em, 2, 0xB, 0, 0);
    } else if (w->flags & 0x4000) {
        EmRoutineSet(em, 2, 0xC, 0, 0);
    } else if (w->flags & 0x20000) {
        if (em->hp <= 0 && part->partsNo == 5 && mag == 0) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, 0);
            if (em->type == 0x16) {
                return;
            }
        }
        EmRoutineSet(em, 2, 4, 0, 0);
    } else if (w->flags & 0x10000) {
        if (em->hp <= 0 && part->partsNo == 5 && mag == 0) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, 0);
            if (em->type == 0x16) {
                return;
            }
        }
        EmRoutineSet(em, 2, 6, 0, 0);
    } else if (w->flags & 0x10000000) {
        em10BloodSet(em, 0);
        if (em->hp > 0) {
            return;
        }
        EmRoutineSet(em, 2, 4, 0, 0);
    } else if (w->flags & 0x100000) {
        if (em->hp <= 0 && part->partsNo == 5 && mag == 0) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, 0);
        }
        EmRoutineSet(em, 2, 4, 0, 0);
    } else if (w->flags & 0x40000000) {
        if (em->hp <= 0 && part->partsNo == 5 && mag == 0) {
            em10LostHead(em, 1, 0);
            GameAddPoint(9);
        } else {
            em10BloodSet(em, 0);
            if (em->type == 0x16) {
                return;
            }
        }
        EmRoutineSet(em, 2, 8, 0, 0);
    } else {
        if (mag == 0 && em10ChgParasiteCk(em)) {
            return;
        }
        if (em->hp <= 0) {
            if (em->type == 0xA || em->type == 0xD || em->type == 2) {
                em10BloodSet(em, 0);
                EmRoutineSet(em, 3, 2, 0, 0);
                return;
            }
            if (part->rad < 1000000.0f && w->wepType != 4 && em->type != 0xA && em->type != 0xD && G_ROOM_ID != 0x100 &&
                em->x38D != 0x39 && em->type != 2) {
                switch (em->dmWep) {
                case 0xD:
                case 0x13:
                case 0x29:
                    EmRoutineSet(em, 3, 5, 0, 1);
                    return;
                }
            }
            if (em->hp <= 0 && part->partsNo == 5 && mag == 0) {
                em10LostHead(em, 1, 0);
                GameAddPoint(9);
            } else {
                em10BloodSet(em, 0);
            }
            EmRoutineSet(em, 2, 4, 0, 0);
            return;
        }
        if (w->flags & 8) {
            w->x67A = 0;
            em10BloodSet(em, 0);
            return;
        }
        if (w->flags & 0x10) {
            w->x67A = 0;
            em10BloodSet(em, 0);
            EmRoutineSet(em, 2, 0xA, 0, 0);
            return;
        }
        if (part->partsNo == 0x25) {
            EmRoutineSet(em, 2, 0xF, 0, 0);
            return;
        }
        type = em->type;
        if (type == 0xA || type == 0xD) {
            EmRoutineSet(em, 2, 0xE, 0, 0);
            return;
        }
        if (type == 2) {
            int no;
            em10BloodSet(em, 0);
            no = part->partsNo;
            if (no == 5) {
                no = 1;
            } else {
                no = 0x11;
            }
            em->xFC = type;
            em->xFD = no;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
        if (type == 0x16) {
            u8 r = Rnd() % 10;
            if (r <= 4 && part->partsNo != 5) {
                return;
            }
            EmRoutineSet(em, 2, 0, 0, 0);
            return;
        }
        if (em->dmWep == 0x1C) {
            if (part->partsNo == 5 && w->pShield == 0) {
                if (w->pParasite && w->x6C5 == 1) {
                    if (!(Rnd() & 3)) {
                        return;
                    }
                    em10BloodSet(em, 0);
                    EmRoutineSet(em, 2, 4, 0, 0);
                    return;
                }
                EmRoutineSet(em, 2, 1, 0, 0);
                return;
            }
            if (part->partsNo == 0x13 || part->partsNo == 0x17 || part->partsNo == 0x14 || part->partsNo == 0x18) {
                EmRoutineSet(em, 2, 0, 0, 0);
                return;
            }
        }
        em10BloodSet(em, 0);
        EmRoutineSet(em, 2, 4, 0, 0);
    }
}

static void em10DmSetWep23(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int one = 1;

    em->dmType = one;
    if (em->type == 0xA || em->type == 0xD) {
        return;
    }
    if (em->x38D == 0x19) {
        return;
    }
    if (w->pGatling) {
        return;
    }
    if (w->pShield) {
        return;
    }
    if (w->pParasite || w->x58C) {
        em->hp = 0;
        EmSetDie(em);
        EmReserveDropItem(em);
        em10SetPoint(em);
        EstSetEm(em, -1, 0, 0, 0x10, 0x56, 0, 0, em, 0);
        em10CoreBreak(em, 0);
        if (EM10_WK(em)->flags & 0x10) {
            EmRoutineSet(em, 3, one, 0, 0);
            return;
        }
        if (em10RoofDmCk(em)) {
            return;
        }
        if ((s16) w->x682 != 0) {
            w->x68C = 120;
            EmRoutineSet(em, 2, 0xB, 0, 0);
        } else if (EM10_WK(em)->flags & 0x4000) {
            EmRoutineSet(em, 2, 0xC, 0, 0);
        } else if (EM10_WK(em)->flags & 0x20000) {
            EmRoutineSet(em, 2, 5, 0, 0);
        } else if (EM10_WK(em)->flags & 0x10000) {
            EmRoutineSet(em, 2, 6, 0, 0);
        } else if (EM10_WK(em)->flags & 0x10000000) {
            EmRoutineSet(em, 2, 4, 0, 0);
        } else if (EM10_WK(em)->flags & 0x100000) {
            EmRoutineSet(em, 2, 4, 0, 0);
        } else {
            EmRoutineSet(em, 3, 2, 0, 0);
        }
    } else {
        if ((s16) w->x682 != 0) {
            w->x68C = 120;
            EmRoutineSet(em, 2, 0xB, 0, 0);
        } else if (EM10_WK(em)->flags & 0x4000) {
            EmRoutineSet(em, 2, 0xC, 0, 0);
        } else if (EM10_WK(em)->flags & 0x20000) {
            EmRoutineSet(em, 2, 5, 0, 0);
        } else if (EM10_WK(em)->flags & 0x10000) {
            EmRoutineSet(em, 2, 6, 0, 0);
        } else if (EM10_WK(em)->flags & 0x10000000) {
            if (em->hp > 0) {
                return;
            }
            EmRoutineSet(em, 2, 4, 0, 0);
        } else if (EM10_WK(em)->flags & 0x100000) {
            EmRoutineSet(em, 2, 4, 0, 0);
        } else if (em->hp <= 0) {
            EmSetDie(em);
            EmReserveDropItem(em);
            em10SetPoint(em);
            if (EM10_WK(em)->flags & 0x10) {
                EmRoutineSet(em, 3, one, 0, 0);
            } else {
                if (em10RoofDmCk(em)) {
                    return;
                }
                EmRoutineSet(em, 3, 2, 0, 0);
            }
        } else if (EM10_WK(em)->flags & 8) {
            w->x67A = 0;
        } else if (w->flags & 0x10) {
            EmRoutineSet(em, 2, 0xA, 0, 0);
        } else {
            if (em10RoofDmCk(em)) {
                return;
            }
            EmRoutineSet(em, 2, 0xD, 0, 0);
        }
    }
}

// Damage blood / hit effect by weapon (near: the shot came from close range).
void em10BloodSet(cEm10* em, int near)
{
    Em10Work* w = EM10_WK(em);
    Camera* cam = &pG->Cam;
    EmHitInfo* part;
    cModel* parts;
    f32 dist;
    Vec pos;
    Vec dir;
    Vec dir2;
    Mtx m;
    cEsp* esp;
    u32 i;

    if (em->type == 0xA || em->type == 0xD) {
        em1cBloodSet(em, near);
        return;
    }
    parts = em->getPartsPtr(0);
    dist = (cam->param.pos.x - parts->worldPos.x) * (cam->param.pos.x - parts->worldPos.x) +
           (cam->param.pos.y - parts->worldPos.y) * (cam->param.pos.y - parts->worldPos.y) +
           (cam->param.pos.z - parts->worldPos.z) * (cam->param.pos.z - parts->worldPos.z);
    part = em->dmPart;
    if (part == 0) {
        return;
    }
    if (em10ArmorCk(em, part->partsNo)) {
        switch (em->dmWep) {
        case 0:
        case 0x14:
        case 0x17:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x2A:
            return;
        case 1:
        case 2:
        case 3:
        case 4:
        case 0x11:
        case 0x19:
        case 0x1C:
        case 0x1F:
        case 0x20:
        case 0x26:
        case 0x2B:
            EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
            return;
        case 0x10:
        case 0x1A:
            EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
            return;
        case 0xB:
        case 0xC:
        case 0x1B:
        case 0x1D:
        case 0x27:
            EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
            return;
        case 7:
        case 8:
        case 0x21:
            if (near) {
                EmDmBloodSet2(em, 0x10, 0x65, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
            }
            return;
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
            break;
        }
        EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
    } else {
        if (part->partsNo == 5 && (w->pParasite || w->x58C)) {
            EmDmBloodSet2(em, 0x10, 0x26, 0, 0, 0);
            return;
        }
        switch (em->dmWep) {
        case 0:
        case 0x14:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x2A:
            return;
        case 1:
        case 2:
        case 3:
        case 4:
        case 0x11:
        case 0x19:
        case 0x1C:
        case 0x1F:
        case 0x20:
        case 0x26:
        case 0x2B:
            if (ChkWaterEffectEnable(&em->pos)) {
                EmDmBloodSet2(em, 0x10, 0x3B, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 1, 0, 0, 0);
            }
            return;
        case 0x10:
        case 0x1A:
            EmDmBloodSet2(em, 0x10, 0x29, 0, 0, 0);
            return;
        case 0xB:
        case 0xC:
        case 0x1B:
        case 0x1D:
        case 0x27:
            if (ChkWaterEffectEnable(&em->pos)) {
                EmDmBloodSet2(em, 0x10, 0x3D, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 0xC, 0, 0, 0);
            }
            if ((Rnd() & 3) == 0) {
                if (EmGetDmPos(em, &pos, &dir)) {
                    EstSet(0, -1, &pos, 0, 0x10, 0xD, 0, 0, 0, 0);
                }
            }
            return;
        case 7:
        case 8:
        case 0x21:
            if (near) {
                if (Ctrl12Ck(w->pCtrl12, 0xB)) {
                    if (dist < 16000000.0f) {
                        if (ChkWaterEffectEnable(&em->pos)) {
                            EmDmBloodSet2(em, 0x10, 0x40, 0, 0, 0);
                        } else {
                            EmDmBloodSet2(em, 0x10, 0x37, 0, 0, 0);
                        }
                    } else {
                        if (ChkWaterEffectEnable(&em->pos)) {
                            EmDmBloodSet2(em, 0x10, 0x3E, 0, 0, 0);
                        } else {
                            EmDmBloodSet2(em, 0x10, 0x35, 0, 0, 0);
                        }
                    }
                } else {
                    if (dist < 16000000.0f) {
                        if (ChkWaterEffectEnable(&em->pos)) {
                            EmDmBloodSet2(em, 0x10, 0x3F, 0, 0, 0);
                        } else {
                            EmDmBloodSet2(em, 0x10, 0x36, 0, 0, 0);
                        }
                    } else {
                        if (ChkWaterEffectEnable(&em->pos)) {
                            EmDmBloodSet2(em, 0x10, 0x3C, 0, 0, 0);
                        } else {
                            EmDmBloodSet2(em, 0x10, 2, 0, 0, 0);
                        }
                    }
                    Ctrl12Set(w->pCtrl12, 0xB, 0x2D);
                }
            } else {
                if (ChkWaterEffectEnable(&em->pos)) {
                    EmDmBloodSet2(em, 0x10, 0x3B, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x10, 1, 0, 0, 0);
                }
            }
            return;
        case 9:
        case 0xA:
        case 0x28:
            EmDmBloodSet2(em, 0x10, 0x87, 0, 0, 0);
            if (EmGetDmPos(em, &pos, &dir2)) {
                EspSeqData* est = EspGetEstAddr(0x10, 0x88, 1);
                if (est) {
                    for (i = 0; i < est->num; i++) {
                        if (EspEstSetSelect(0x10, 0x88, i, &esp, 0)) {
                            esp->pos = pos;
                            esp->parent = em->getPartsPtr(part->partsNo - 1);
                            PSMTXInverse(((cModel*) esp->parent)->mat, m);
                            PSMTXMultVec(m, &esp->pos, &esp->pos);
                            esp->partsNo = ((u8*) &part->partsNo)[1] - 1;
                        }
                    }
                }
            }
            return;
        case 5:
        case 6:
        case 0xD:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x15:
        case 0x29:
        case 0x2C:
        case 0x2D:
            break;
        }
        {
            if (Ctrl12Ck(w->pCtrl12, 0xB)) {
                if (dist < 16000000.0f) {
                    if (ChkWaterEffectEnable(&em->pos)) {
                        EmDmBloodSet2(em, 0x10, 0x40, 0, 0, 0);
                    } else {
                        EmDmBloodSet2(em, 0x10, 0x37, 0, 0, 0);
                    }
                } else {
                    if (ChkWaterEffectEnable(&em->pos)) {
                        EmDmBloodSet2(em, 0x10, 0x3E, 0, 0, 0);
                    } else {
                        EmDmBloodSet2(em, 0x10, 0x35, 0, 0, 0);
                    }
                }
            } else {
                if (dist < 16000000.0f) {
                    if (ChkWaterEffectEnable(&em->pos)) {
                        EmDmBloodSet2(em, 0x10, 0x3F, 0, 0, 0);
                    } else {
                        EmDmBloodSet2(em, 0x10, 0x36, 0, 0, 0);
                    }
                } else {
                    if (ChkWaterEffectEnable(&em->pos)) {
                        EmDmBloodSet2(em, 0x10, 0x3C, 0, 0, 0);
                    } else {
                        EmDmBloodSet2(em, 0x10, 2, 0, 0, 0);
                    }
                }
                Ctrl12Set(w->pCtrl12, 0xB, 0x2D);
            }
        }
    }
}

// Blood effect for the parasite-headed Ganados (types 0xA / 0xD): parts 0x25 is the parasite itself.
void em1cBloodSet(cEm10* em, int near)
{
    Em10Work* w = EM10_WK(em);
    Camera* cam = &pG->Cam;
    EmHitInfo* part;
    cModel* parts;
    f32 dist;
    int armor;
    Vec pos;
    Vec dir;

    parts = em->getPartsPtr(0);
    dist = (cam->param.pos.x - parts->worldPos.x) * (cam->param.pos.x - parts->worldPos.x) +
           (cam->param.pos.y - parts->worldPos.y) * (cam->param.pos.y - parts->worldPos.y) +
           (cam->param.pos.z - parts->worldPos.z) * (cam->param.pos.z - parts->worldPos.z);
    part = em->dmPart;
    if (part->partsNo == 0x25) {
        switch (em->dmWep) {
        case 0:
        case 0x14:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
            return;
        case 1:
        case 2:
        case 3:
        case 4:
        case 9:
        case 0xA:
        case 0x11:
        case 0x19:
        case 0x1C:
        case 0x1F:
        case 0x20:
        case 0x26:
        case 0x28:
        case 0x2B:
            EmDmBloodSet2(em, 0x10, 0x26, 0, 0, 0);
            return;
        case 0x10:
        case 0x1A:
            EmDmBloodSet2(em, 0x10, 0x26, 0, 0, 0);
            return;
        case 0xB:
        case 0xC:
        case 0x1B:
        case 0x1D:
        case 0x27:
            EmDmBloodSet2(em, 0x10, 0x26, 0, 0, 0);
            return;
        case 7:
        case 8:
        case 0x21:
            if (near) {
                EmDmBloodSet2(em, 0x10, 0x27, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 0x26, 0, 0, 0);
            }
            return;
        case 5:
        case 6:
        case 0xD:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x15:
        case 0x29:
        case 0x2C:
        case 0x2D:
        default:
            EmDmBloodSet2(em, 0x10, 0x27, 0, 0, 0);
            return;
        }
    }
    armor = 0;
    if (em10ArmorCk(em, part->partsNo)) {
        armor = 1;
    }
    switch (em->dmWep) {
    case 0:
    case 0x14:
        return;
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0x11:
    case 0x19:
    case 0x1C:
    case 0x1F:
    case 0x20:
    case 0x26:
    case 0x28:
    case 0x2B:
        if (armor) {
            EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
        } else if (ChkWaterEffectEnable(&em->pos)) {
            EmDmBloodSet2(em, 0x10, 0x3B, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x10, 1, 0, 0, 0);
        }
        return;
    case 0x10:
    case 0x1A:
        if (armor) {
            EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x10, 0x29, 0, 0, 0);
        }
        return;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        if (armor) {
            EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
        } else {
            if (ChkWaterEffectEnable(&em->pos)) {
                EmDmBloodSet2(em, 0x10, 0x3D, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 0xC, 0, 0, 0);
            }
            if ((Rnd() & 3) == 0) {
                if (EmGetDmPos(em, &pos, &dir)) {
                    EstSet(0, -1, &pos, 0, 0x10, 0xD, 0, 0, 0, 0);
                }
            }
        }
        return;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (armor) {
                EmDmBloodSet2(em, 0x10, 0x65, 0, 0, 0);
            } else if (Ctrl12Ck(w->pCtrl12, 0xB)) {
                if (dist < 16000000.0f) {
                    if (ChkWaterEffectEnable(&em->pos)) {
                        EmDmBloodSet2(em, 0x10, 0x40, 0, 0, 0);
                    } else {
                        EmDmBloodSet2(em, 0x10, 0x37, 0, 0, 0);
                    }
                } else {
                    if (ChkWaterEffectEnable(&em->pos)) {
                        EmDmBloodSet2(em, 0x10, 0x3E, 0, 0, 0);
                    } else {
                        EmDmBloodSet2(em, 0x10, 0x35, 0, 0, 0);
                    }
                }
            } else {
                if (dist < 16000000.0f) {
                    if (ChkWaterEffectEnable(&em->pos)) {
                        EmDmBloodSet2(em, 0x10, 0x3F, 0, 0, 0);
                    } else {
                        EmDmBloodSet2(em, 0x10, 0x36, 0, 0, 0);
                    }
                } else {
                    if (ChkWaterEffectEnable(&em->pos)) {
                        EmDmBloodSet2(em, 0x10, 0x3C, 0, 0, 0);
                    } else {
                        EmDmBloodSet2(em, 0x10, 2, 0, 0, 0);
                    }
                }
                Ctrl12Set(w->pCtrl12, 0xB, 0x2D);
            }
        } else {
            if (armor) {
                EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
            } else if (ChkWaterEffectEnable(&em->pos)) {
                EmDmBloodSet2(em, 0x10, 0x3B, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 1, 0, 0, 0);
            }
        }
        return;
    case 5:
    case 6:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        break;
    }
    if (armor) {
        EmDmBloodSet2(em, 0x10, 0x64, 0, 0, 0);
    } else if (Ctrl12Ck(w->pCtrl12, 0xB)) {
        if (dist < 16000000.0f) {
            if (ChkWaterEffectEnable(&em->pos)) {
                EmDmBloodSet2(em, 0x10, 0x40, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 0x37, 0, 0, 0);
            }
        } else {
            if (ChkWaterEffectEnable(&em->pos)) {
                EmDmBloodSet2(em, 0x10, 0x3E, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 0x35, 0, 0, 0);
            }
        }
    } else {
        if (dist < 16000000.0f) {
            if (ChkWaterEffectEnable(&em->pos)) {
                EmDmBloodSet2(em, 0x10, 0x3F, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 0x36, 0, 0, 0);
            }
        } else {
            if (ChkWaterEffectEnable(&em->pos)) {
                EmDmBloodSet2(em, 0x10, 0x3C, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x10, 2, 0, 0, 0);
            }
        }
        Ctrl12Set(w->pCtrl12, 0xB, 0x2D);
    }
}

void em10KickHitMark(cEm10* em)
{
    cModel* parts;
    Vec rot;

    parts = em->getPartsPtr(4);
    rot.x = 0.0f;
    rot.y = GetXZAngle(&em->x328, &em->pos);
    rot.z = 0.0f;
    if (ChkWaterEffectEnable(&em->pos)) {
        EstSet(0, -1, &parts->worldPos, &rot, 0x10, 0x38, 0, 0, 0, 0);
    } else {
        EstSet(0, -1, &parts->worldPos, &rot, 0x10, 0x25, 0, 0, 0, 0);
    }
}

void cEm10::move()
{
    Em10Work* w = EM10_WK(this);
    f32 dist;
    Mtx m;
    Vec v;

    if (xFC) {
        em10DmCk(this);
    }
    w->flags &= 0xACC081A7;
    clearStatus(3);
    hitInfo.flags |= 1;
    if ((w->flags & 0x80) && w->pParasite == 0 && w->x58C == 0) {
        hitInfo.flags &= ~1;
    }
    em10RouteCk(this);
    {
        u32 lost = w->flags & 0x800000;
        w->flags &= ~0x04000004;
        if (lost) {
            w->x656++;
        } else {
            w->x656 = 0;
        }
    }
    if (w->x67C) {
        w->x67C--;
    }
    if (pPL->xFC == 1 || (pG->flags_5010 & 0x8000)) {
        if ((s16) w->x67C <= 4) {
            w->x67C = 5;
        }
    }
    if (pG->flags_5010 & 0x2000) {
        w->x67C = 0;
    }
    if (w->x674) {
        w->x674--;
    }
    if (pG->x4F88 > 6) {
        if (w->x674) {
            w->x674--;
        }
    }
    if (pG->x4F88 > 9) {
        if (w->x674) {
            w->x674--;
        }
    }
    if (w->x660) {
        w->x660--;
    }
    if (w->x68C) {
        w->x68C--;
    }
    if (w->flags & 1) {
        w->x638 = 0;
    } else {
        w->x638++;
    }
    if (w->x664) {
        w->x664--;
    }
    if (w->x670) {
        w->x670--;
    }
    if (w->x696) {
        w->x696--;
    }
    if (w->x682) {
        w->x682--;
    }
    if (w->x6A3) {
        w->x6A3--;
    }
    if (w->x644) {
        w->x644--;
    }
    if (w->x65E) {
        w->x65E--;
    }
    if (w->x6C3) {
        w->x6C3--;
    }
    if (w->x6BC) {
        w->x6BC--;
        if (w->x6BC == 0) {
            atari.flags &= ~8;
        }
    }
    if (w->x6C1) {
        w->x6C1--;
    }
    if ((w->flags & 1) && w->x646) {
        w->x646--;
    }
    if (w->x648) {
        w->x648--;
    }
    w->x6C2 = 0;
    Em10_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    if (seFlags28B & 0x80) {
        w->flags |= 0x10;
        w->flags |= 0x01000000;
        setStatus(3);
    }
    if (w->flags & 0x800) {
        scale.x = 1.0f;
        scale.y = 1.0f;
        scale.z = 1.0f;
    } else {
        scale.x = scale.x * 0.9f + w->scaleBase.x * 0.1f;
        scale.y = scale.y * 0.9f + w->scaleBase.y * 0.1f;
        scale.z = scale.z * 0.9f + w->scaleBase.z * 0.1f;
    }
    em10ClawMove(this);
    if (!(w->flags & 0x400000)) {
        em10NeckMove(this);
        em10WaistMove(this);
        em10SlopeMove(this);
        partsWorldCalc();
        em10ScaleCompress(this);
        em10BombNeckMove(this);
        partsFixAdjust();
        PartsWorldPosCalc(this);
        if (!(w->flags & 0x400000)) {
            u16 atFlags;
            f32 moved;
            dist = SQRTF((oldPos.x - pos.x) * (oldPos.x - pos.x) + (oldPos.z - pos.z) * (oldPos.z - pos.z));
            if ((seFlags28B & 0x40) || (w->flags & 0x10091000)) {
                atari.flags |= 0x10;
            } else {
                atari.flags &= ~0x10;
            }
            atFlags = atari.flags;
            if ((seFlags28B & 0x40) || (w->flags & 0x11000)) {
                atari.flags &= ~0x100;
            }
            EmAtCheck(this);
            atari.move();
            if (w->flags & 0x80000) {
                SatMgr.checkAir(this, 0x1C2810);
            } else {
                SatMgr.check(this, 0);
            }
            atari.flags = atFlags;
            moved = SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z));
            if (moved < dist * 0.5f) {
                w->x634++;
            } else {
                if (w->x634 > 60) {
                    w->x634 = 60;
                }
                if (w->x634) {
                    w->x634--;
                }
            }
        }
    }
    {
        Camera* cam = &pG->Cam;
        int hide = 0;
        Vec* nrm = pFloorNrm;
        if (nrm == 0 || nrm->y < 0.8f) {
            hide = 1;
        }
        if (cam->param.pos.y < pos.y) {
            hide = 1;
        }
        if ((w->flags & 0x101B0000) || hide) {
            if (shdCol <= 0xF6) {
                shdCol += 8;
            } else {
                shdCol = 0xFF;
            }
        } else {
            if (shdCol > 8) {
                shdCol -= 8;
            } else {
                shdCol = hide;
            }
        }
    }
    switch (x38D) {
    default:
        break;
    case 5:
    case 6:
    case 0x1A:
        if (!EM_RTN(this, 1, 7)) {
            EffectEspDelete(0, w->x69E, (u32) this, 0);
            EffectEspgenDelete(0, w->x69E, (int) this);
            EffectEfmDelete(0, w->x69E, (int) this);
        }
        break;
    case 7:
    case 8:
    case 9:
        if (!EM_RTN(this, 1, 8)) {
            EffectEspDelete(0, w->x69E, (u32) this, 0);
            EffectEspgenDelete(0, w->x69E, (int) this);
            EffectEfmDelete(0, w->x69E, (int) this);
        }
        break;
    case 0xA:
    case 0xB:
        break;
    }
    em10ChainSawMove(this);
    if (type == 6) {
        Em18ClothMove(this, (PlCloth*) &w->cloth);
    }
    if (type == 0x16) {
        Em1fClothMove(this, (PlCloth*) &w->cloth);
    }
    if (w->pWep && w->wepType == 4 && (w->flags & 0x80000000) && hp > 0 && (s16) pG->pl_life > 0) {
        if (w->x684) {
            w->x684--;
            if ((s16) w->x684 == 0) {
                w->x684 = 60;
                w->sndId = SndCall(6, 0x4D, &pos, 0, 0, this);
            }
        }
    } else if (w->sndId) {
        SndStop(w->sndId, 0);
        w->sndId = 0;
    }
    if (w->pHead && !EM_RTN(this, 1, 9)) {
        if (w->mot[51]) {
            MotionSetCore(w->pHead, MOTION(w->pHead), w->mot[51], 0, 0, 0, 0);
        }
        atariInitF(&w->pHead->atari, 0.0f, 150.0f, -300.0f, 300.0f, 300.0f, 300.0f, 150.0f, 1, 0x2000, 10);
        w->pHead->atari.flags &= ~0x100;
        w->pHead->atari.flags |= 0x200;
        w->pHead->atari.flags |= 0x10;
        EstSetEm(w->pHead, -1, 0, 0, 0x10, 0x16, 0, 0, w->pHead, 0);
        EstSetEm(w->pHead, -1, 0, 0, 0x10, 0x16, 0, 0, w->pHead, 0);
        EstSetEm(w->pHead, -1, 0, 0, 0x10, 0x16, 0, 0, w->pHead, 0);
        w->pHead = 0;
    }
    em10BowgunMove(this);
    if (w->x658) {
        w->x658--;
        if (w->x658 == 0) {
            em10SetParasite(this);
        }
    }
    em10SetWaterEff(this);
    if (w->x58C && hp > 0) {
        if (w->x694) {
            w->x694--;
        } else {
            w->x694 = 0x1D;
            EstSetEm(this, -1, 0, 0, 0x10, 0x33, 0, 0, this, 0);
        }
    }
    em10FootSe(this);
    if (w->wepType == 9 && w->x640 && w->pWep && hp > 0) {
        if (w->x640 <= 999) {
            w->x640--;
        }
        w->x6A4++;
        if (w->x6A4 % 6 == 0) {
            SndCall(8, 0x95, &pos, id, 0, this);
        }
        if (w->x640 == 0) {
            cModel* parts = w->pWep->getPartsPtr(0);
            w->pWep->setLost();
            w->pWep = 0;
            w->wepType = 0;
            if (hp > 0) {
                hp = 0;
                EmRoutineSet(this, 3, 5, 0, 0);
            } else {
                if (w->flags & 0x01400000) {
                    EstSet(0, -1, &parts->worldPos, 0, 0x10, 0x2A, 0, 0, 0, 0);
                } else {
                    EstSetEm(this, -1, 0, 0, 0x10, 0x30, 0, 0, this, 0);
                }
                SndCall(8, 0x96, &pos, id, 0, this);
                SndCall(8, 8, &pos, id, 0, this);
                be_flag &= ~2;
            }
            dmType = 0;
            PlWepHitCheck2(0, &pos, &pos, 0x13, 2, 6000.0f);
        }
    }
    if (type == 0xA || type == 0xD) {
        w->hit[9].flags |= 1;
    } else {
        w->hit[9].flags &= ~1;
        if (w->pParasite && w->pParasite->pParts && w->pParasite->isAlive()) {
            cModel* parts;
            PSMTXInverse(getPartsPtr(4)->mat, m);
            if (w->x6C5 == 1) {
                parts = w->pParasite->getPartsPtr(9);
            } else {
                parts = w->pParasite->getPartsPtr(0x15);
            }
            PSMTXMultVec(m, &parts->worldPos, &v);
            w->hit[9].ofs = v;
            w->hit[9].flags |= 1;
        }
        if (w->x58C && w->x58C->pParts && w->x58C->isAlive()) {
            cModel* parts;
            PSMTXInverse(getPartsPtr(4)->mat, m);
            parts = w->x58C->getPartsPtr(2);
            PSMTXMultVec(m, &parts->worldPos, &v);
            w->hit[9].ofs = v;
            w->hit[9].flags |= 1;
        }
    }
    if (w->pShield) {
        if (flags_3C8 & 0x1000000) {
            w->hit[3].flags &= ~1;
            w->hit[7].flags &= ~1;
        } else {
            w->hit[4].flags &= ~1;
            w->hit[8].flags &= ~1;
        }
    } else {
        w->hit[3].flags |= 1;
        w->hit[7].flags |= 1;
        w->hit[4].flags |= 1;
        w->hit[8].flags |= 1;
    }
    if (w->pShield && w->pShield->hp <= 0) {
        w->pShield = 0;
    }
    em10GatlingRollMove(this);
    if ((flags_3C8 & 0x40) && !(w->flags & 0x100) && G_ROOM_ID == 0x206) {
        setFindPL();
    }
}

// Initial routine from the enemy set number (cEm::x38D) and the work defaults.
void em10InitRtnSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;

    switch (em->type) {
    case 2:
    case 5:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xD:
        w->x69A = 2;
        break;
    default:
        w->x69A = Rnd() & 1;
        break;
    }
    w->x6AC = em->emsetNo % 7;
    if (em->x3CC <= 0.0f) {
        em->x3CC = 20000.0f;
    }
    w->flags = 0;
    w->x69B = Rnd() % 5 + 5;
    w->x67C = 0;
    w->x5B8 = 0;
    w->x5BC = 0;
    w->x5C4 = 0;
    w->x5C8 = 0.0f;
    w->x5CC = 0.0f;
    w->x5D0 = 0.0f;
    w->x674 = Rnd() % 450 + 150;
    w->x67E = Rnd() % 90 + 90;
    w->x680 = 0;
    w->x598.x = 0.0f;
    w->x598.y = 0.0f;
    w->x598.z = 0.0f;
    w->x66C = 0.0f;
    w->x634 = 0;
    w->x68C = 0;
    w->x668 = 0;
    w->x524 = 100000000.0f;
    w->x52C = 100000000.0f;
    w->x530 = 100000000.0f;
    w->x528 = 100000000.0f;
    w->x6AD = 0xFF;
    w->x664 = Rnd() % 150;
    w->x6B5 = 2;
    w->x5D8 = 1.0f;
    w->x660 = 0;
    w->sndId = 0;
    w->x670 = 0;
    w->x696 = 0;
    w->x5EC = 0;
    w->x682 = 0;
    w->pParasite = 0;
    w->x6B6 = 0;
    w->x58C = 0;
    for (i = 0; i < 5; i++) {
        w->x578[i] = 0;
    }
    w->pTruck = 0;
    w->x658 = 0;
    w->x6B7 = 0;
    w->x6AE = 0;
    w->x6B8 = 0;
    w->x6B9 = 0;
    w->x6BA = 0;
    w->x64C = 0.0f;
    w->x6BB = Rnd() % 3 + 2;
    w->x6A3 = 0;
    w->x640 = 0;
    w->x694 = 0;
    w->x65E = 0;
    w->x6BC = 0;
    w->pSwitch = 0;
    w->pDragon = 0;
    w->pGondola = 0;
    w->x6BE = 0;
    w->x6BF = 0;
    w->x646 = 450;
    w->x686 = Rnd() % 150 + 150;
    w->x6C0 = 0;
    w->x564 = 0;
    w->pGatling = 0;
    w->x654 = 0;
    w->x656 = 0;
    w->x6C1 = 0;
    w->x6C3 = 0;
    w->x6A6 = 0;
    w->x6A8 = 0;
    w->x590 = 0;
    w->x6C4 = 0;
    w->x594 = 0;
    for (i = 0; i < 3; i++) {
        void** mot0 = &w->evtMot[0];
        void** mot4 = &w->evtMot[4];
        mot0[i] = 0;
        mot4[i] = 0;
    }
    if (em->type == 0x17) {
        EstSetEm(em, -1, 0, 0, 0x10, 0x8E, 0x800, w->x69D, em, 0);
    } else if (em->type == 0x19) {
        EstSetEm(em, -1, 0, 0, 0x10, 0x8F, 0x800, w->x69D, em, 0);
    } else if (GetEm10EyeEffectEnable()) {
        EstSetEm(em, -1, 0, 0, 0, 0x35, 0x800, w->x69D, em, 0);
    }
    if (em->type == 0xA || em->type == 0xD) {
        Vec pos;
        Vec rot;
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pParasite = (cObj16*) SetObj16(PL_ARC_PTR(em->subArc, 0x227), PL_ARC_PTR(em->subArc, 0x228), em, em, 0x24, 4, &pos, &rot);
        if (w->pParasite) {
            PlArc* arc = em->subArc;
            w->pParasite->setMotData(PL_ARC_PTR(arc, 0x229), PL_ARC_PTR(arc, 0x229), PL_ARC_PTR(arc, 0x229), PL_ARC_PTR(arc, 0x229),
                                     PL_ARC_PTR(arc, 0x229), PL_ARC_PTR(arc, 0x229), PL_ARC_PTR(arc, 0x229), PL_ARC_PTR(arc, 0x22A),
                                     PL_ARC_PTR(arc, 0x22A), PL_ARC_PTR(arc, 0x229), PL_ARC_PTR(arc, 0x229));
        }
    }
    switch (em->x38D) {
    case 0x14:
    case 0x15:
        break;
    default:
        if (w->wepType == 7) {
            w->pWep->setEffAlways(0x10, 0x14);
            w->pWep->setEffFall(0x10, 0x1C);
        }
        if (w->wepType == 8 && w->pWep) {
            EstSetEm(w->pWep, -1, 0, 0, 0x10, 0x1F, 0, w->x69F, w->pWep, 0);
        }
        break;
    }
    if (em->x38D == 0x1D) {
        w->x6B9 = 1;
    }
    if (em->x38D == 0x21) {
        w->x6BA = 1;
    }
    if ((em->flags_3C8 & 0x40) && G_ROOM_ID == 0x206) {
        em->setFindPL();
    }
    switch (em->x38D) {
    case 0:
    default:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        if (em->type == 6 || (em->x3D0 != 1 && em->x3D0 != 3)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    case 0xE:
    case 0x1D:
    case 0x21:
        em10SetWalkMotion(em, 0);
        em->setStatus(5);
        em->setFindPL();
        w->x6AC = Rnd() % 11;
        EmRoutineSet(em, 1, 0x10, 0, 0);
        break;
    case 0x1B:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 5, 0, 0);
        break;
    case 2:
    case 3:
    case 4:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 6, 0, 0);
        break;
    case 5:
    case 6:
    case 0x1A:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 7, 0, 0);
        break;
    case 7:
    case 8:
    case 9:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 8, 0, 0);
        break;
    case 0xA:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 8, 0, 0);
        break;
    case 0xB:
        if (w->mot[48] && w->mot[49]) {
            w->pHead = (cEm*) SetObj12(w->mot[48], w->mot[49], &em->pos, &em->rot);
        }
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 9, 0, 0);
        break;
    case 0xC:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0xA, 0, 0);
        break;
    case 0x2F:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0xA, 2, 0);
        break;
    case 0xD:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0xB, 0, 0);
        break;
    case 0xF:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x18, 0, 0);
        break;
    case 0x10:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x18, 0, 1);
        break;
    case 0x11:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x1A, 0, 1);
        break;
    case 0x13:
        em10SetWaitMotion(em, 0);
        em->clearStatus(5);
        EmRoutineSet(em, 1, 2, 0, 0);
        break;
    case 0x14:
        em10SetWaitMotion(em, 0);
        em->clearStatus(5);
        EmRoutineSet(em, 1, 3, 0, 0);
        break;
    case 0x15:
        em10SetWaitMotion(em, 0);
        em->clearStatus(5);
        EmRoutineSet(em, 1, 4, 0, 0);
        break;
    case 0x16:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x19, 0, 0);
        break;
    case 0x17:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x19, 0, 1);
        break;
    case 0x18:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x47, 0, 0);
        break;
    case 0x19:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x48, 0, 0);
        break;
    case 0x1C:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x49, 0, 0);
        break;
    case 0x1E:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x4A, 0, 0);
        break;
    case 0x30:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x4B, 0, 0);
        break;
    case 0x1F:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x4C, 0, 0);
        break;
    case 0x20:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x4D, 0, 0);
        break;
    case 0x22:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x4E, 0, 0);
        break;
    case 0x23:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x4F, 0, 0);
        break;
    case 0x24:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x50, 0, 0);
        break;
    case 0x25:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x50, 0, 1);
        break;
    case 0x2B:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x50, 0, 2);
        break;
    case 0x26:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x51, 0, 0);
        break;
    case 0x27:
        w->pDragon = GetCtrlDragon(0);
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        em->atari.throughOn();
        EmRoutineSet(em, 1, 0x52, 0, 0);
        break;
    case 0x28:
        w->pDragon = GetCtrlDragon(1);
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        em->atari.throughOn();
        EmRoutineSet(em, 1, 0x53, 0, 0);
        break;
    case 0x29:
        w->pDragon = GetCtrlDragon(2);
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        em->atari.throughOn();
        EmRoutineSet(em, 1, 0x54, 0, 0);
        break;
    case 0x2A:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x55, 0, 0);
        break;
    case 0x2C:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x56, 0, 0);
        break;
    case 0x31:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x56, 0, 1);
        break;
    case 0x2D:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x57, 0, 0);
        break;
    case 0x2E:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x58, 0, 0);
        break;
    case 0x32:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x5A, 0, 0);
        break;
    case 0x33:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x5B, 0, 0);
        break;
    case 0x34:
        em10SetWaitMotion(em, 0);
        em->clearStatus(5);
        EmRoutineSet(em, 1, 0x5C, 0, 0);
        break;
    case 0x35:
        em10SetWaitMotion(em, 0);
        em->clearStatus(5);
        EmRoutineSet(em, 1, 0x5E, 0, 0);
        break;
    case 0x36:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x60, 0, 0);
        break;
    case 0x37:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x61, 0, 0);
        break;
    case 0x38:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x62, 0, 0);
        break;
    case 0x39:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x63, 0, 0);
        break;
    case 0x3A:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x64, 0, 0);
        break;
    case 0x3B:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        em->scale.x = 1.0f;
        em->scale.y = 1.0f;
        em->scale.z = 1.0f;
        w->scaleBase.x = 1.0f;
        w->scaleBase.y = 1.0f;
        w->scaleBase.z = 1.0f;
        EmRoutineSet(em, 1, 0x66, 0, 0);
        break;
    case 0x3C:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x68, 0, 0);
        break;
    case 0x3D:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x69, 0, 0);
        break;
    case 0x3E:
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x6A, 0, 0);
        break;
    case 0x3F: {
        MotionData* mot = (MotionData*) PL_ARC_PTR(em->subArc, 0x67);
        MotionSetCore(em, MOTION(em), mot, 0, 0, 1, (u16) ((mot->maxFrame & 0x3FFF) - 1));
        em->hp = 0;
        em->clearStatus(5);
        EmRoutineSet(em, 1, 0x6B, 0, 0);
        break;
    }
    case 0x40:
        w->flags |= 0x100;
        em10SetWaitMotion(em, 0);
        em->setStatus(5);
        EmRoutineSet(em, 1, 0x6C, 0, 0);
        break;
    }
    if (em->type == 6) {
        em->clearStatus(5);
    }
    switch (em->type) {
    case 2:
        em10BeltSet(em);
        break;
    case 0xA:
    case 0xD:
        em10ChainSet(em);
        break;
    }
}

// Motion parts flip table (MotionWork::flip): left / right parts swapped.
static u16 em10_xflip_tbl[80] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
};

static void em10_R0_Init(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    f32 sc;

    if (Em10SetFunc == 0) {
        em->xFC = 0xFF;
        return;
    }
    Em10SetFunc(em);
    EspDataLoad((u32) PL_ARC_PTR(em->subArc, 4), 0x10, 0);
    if (em10ModelInit(em) == 0) {
        em->xFC = 0xFF;
        return;
    }
    switch (em->emsetNo & 0xF) {
    default: {
        sc = fRand1_1() * 0.01f + 1.03f;
        const f32 k = 1.01f; // pool order: 1.01 before the 1.1 of case 7 (docs/matching.md const-local idiom)
        break;
    }
    case 7:
        sc = 1.1f;
        break;
    case 0:
    case 4:
    case 9:
    case 0xB:
        sc = fRand1_1() * 0.01f + 1.01f;
        break;
    }
    if (em->type == 6) {
        sc = 1.05f;
    }
    if (em->type == 0xA || em->type == 0xD) {
        sc = 1.25f;
    }
    if (em->type == 2) {
        sc = 1.25f;
    }
    if (em->type == 0x16) {
        sc = 1.3f;
    }
    if (em->type == 0x18) {
        sc = 1.2f;
    }
    switch (em->type) {
    case 0xB:
    case 0xC:
        sc = 1.0f;
        break;
    }
    em->scale.z = sc;
    em->scale.y = sc;
    em->scale.x = sc;
    w->scaleBase = em->scale;
    em->motFlip = em10_xflip_tbl;
    switch (em->type) {
    case 6:
        Em18ClothSet(em, (PlCloth*) &w->cloth, 0);
        break;
    case 0x16:
        Em1fClothSet(em, (PlCloth*) &w->cloth);
        break;
    case 3:
    case 5:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0x17:
        break;
    }
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    w->startPos = em->pos;
    w->startRotY = em->rot.y;
    w->x4D8 = w->startPos;
    w->x4C4 = em->x38D;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };
        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 400.0f, 250.0f, 250.0f, 800.0f, 1, 0x2000, 10);
    if (em->type == 6) {
        em->atari.flags |= 8;
        em->setStatus(0xB);
    }
    em->litArea.on(1);
    if (em->type == 6) {
        em->setStatus(1);
        if (em->type == 6) {
            em->hp = 1;
        }
    }
    if (em->type == 0xA || em->type == 0xD) {
        YarareInit(em, 0.0f, 300.0f, 50.0f, 150.0f, 110.0f, 3, 5);
    } else {
        YarareInit(em, 0.0f, 0.0f, 0.0f, 160.0f, 110.0f, 5, 1);
    }
    YarareAdd(em, &w->hit[0], 0.0f, -30.0f, 0.0f, 210.0f, 290.0f, 2, 1);
    switch (em->type) {
    case 7:
    case 8:
    case 9:
    case 0xB:
    case 0xC:
        YarareAdd(em, &w->hit[1], -20.0f, -400.0f, 0.0f, 180.0f, 400.0f, 0x14, 1);
        YarareAdd(em, &w->hit[2], 20.0f, -400.0f, 0.0f, 180.0f, 400.0f, 0x18, 1);
        break;
    default:
        YarareAdd(em, &w->hit[1], -20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x14, 1);
        YarareAdd(em, &w->hit[2], 20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x18, 1);
        break;
    }
    if (em->type == 0xA || em->type == 0xD) {
        YarareAdd(em, &w->hit[3], -300.0f, 0.0f, 0.0f, 100.0f, 250.0f, 9, 3);
        YarareAdd(em, &w->hit[4], 50.0f, 0.0f, 0.0f, 100.0f, 250.0f, 0xF, 3);
    } else {
        YarareAdd(em, &w->hit[3], -350.0f, 0.0f, 0.0f, 100.0f, 350.0f, 9, 3);
        YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 100.0f, 350.0f, 0xF, 3);
    }
    switch (em->type) {
    case 7:
    case 8:
    case 9:
    case 0xB:
    case 0xC:
        YarareAdd(em, &w->hit[5], -20.0f, -300.0f, 0.0f, 200.0f, 300.0f, 0x13, 1);
        YarareAdd(em, &w->hit[6], 20.0f, -300.0f, 0.0f, 200.0f, 300.0f, 0x17, 1);
        break;
    default:
        YarareAdd(em, &w->hit[5], -20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x13, 1);
        YarareAdd(em, &w->hit[6], 20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x17, 1);
        break;
    }
    YarareAdd(em, &w->hit[7], -300.0f, 0.0f, 0.0f, 120.0f, 300.0f, 8, 3);
    YarareAdd(em, &w->hit[8], 0.0f, 0.0f, 0.0f, 120.0f, 300.0f, 0xE, 3);
    if (em->type == 0xA || em->type == 0xD) {
        YarareAdd(em, &w->hit[9], 0.0f, 50.0f, 0.0f, 200.0f, 0.0f, 0x25, 1);
    } else {
        YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 5, 0);
    }
    w->x69C = 0x2C;
    w->x69D = 0x2D;
    w->x69E = 0x2E;
    w->x69F = 0x2F;
    w->x6A0 = 0x30;
    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01000000 && em->emsetNo == 0) {
        em->be_flag |= 0x10000;
    }
    switch (em->x38D) {
    case 2:
    case 3:
    case 4:
    case 0x18:
    case 0x19:
    case 0x1C:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    case 0x31:
    case 0x34:
        em->be_flag |= 0x10000;
        break;
    case 0x35:
        break;
    }
    em10InitRtnSet(em);
    if (em->type == 6) {
        em->be_flag |= 0x10000;
    }
    if (pG->flags_6C & 0x200) {
        em->flags_3C8 &= ~0x100000;
    }
    MotionMoveF(em, 0);
    em10_R0_Move(em);
    OSReport("em10 free size = 0x%x\n", sizeof(Em10Work));
}

static void em10_R0_Move(cEm10* em)
{
    Em10_R1_move_tbl[em->xFD * 2](em);
    Em10_R1_move_tbl[em->xFD * 2 + 1](em);
}

// ===== STUBS (development only, removed as functions are written) =====
static void em10_R1_br_Dummy(cEm10* em)
{
}

static void em10_R1_br_Wait(cEm10* em)
{
}

static void em10_R1_Wait(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int ret;

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0xA);
        if (w->pParasite || w->x58C) {
            em->setWeaponFall();
            if (w->pShield) {
                w->pShield->setFall(20.0f, 0);
                w->pShield = 0;
            }
        }
        em->xFE++;
    case 1:
        if (em->flags_3C8 & 8) {
            em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, PI);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        MotionMoveF(em, 0);
        int one = 1;
        if (pG->flags_64 & 0x02000000) {
            break;
        }
        ret = em10GotoCk(em);
        if (ret) {
            break;
        }
        if (em->type == 6) {
            em->setFindPL();
            w->flags |= 0x40000;
            break;
        }
        if ((s16) pG->pl_life <= 0 || (pSUB && (s16) pG->sub_life <= 0)) {
            do { // loop notes keep `one` at its declaration (update_equiv_regs moves single-use constants only outside loops)
                EmRoutineSet(em, one, 0x1B, 0, 0);
            } while (0);
            break;
        }
        if (em10FindCk(em, 0)) {
            break;
        }
        if (em->flags_3C8 & 1) {
            em->setFindPL();
        }
        if (w->flags & 0x100) {
            em10FindNotify(em);
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
    em10ActEvtSetTrade(em);
}

static void em10_R1_Keeper(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    f32 ang;

    if ((EM10_WK(em)->flags & 0x100) && em->xFE == 0 && w->x508 > 1.5707964f) {
        em->xFE = 2;
    }
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0xA);
        em->xFE++;
    case 1:
        if (w->flags & 0x100) {
            w->flags |= 0x40000;
        }
        if (w->flags & 0x100) {
            em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.024543693f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        } else if (em->flags_3C8 & 8) {
            em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, PI);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->plDist2 < 2500000000.0f || (em->flags_3C8 & 0x40) || G_ROOM_ID == 0x320) {
            w->flags &= ~0x400000;
            if (!(em->be_flag & 2)) {
                em->be_flag |= 2;
                em->dmType = 0;
            }
            em->atari.flags |= 0x300;
            em->alpha = 1.0f;
            MotionMoveF(em, 0);
            if (pG->flags_64 & 0x02000000) {
                break;
            }
            if (em10GotoCk(em)) {
                return;
            }
            if (w->flags & 0x100) {
                if (w->x52C < em->x3CC || (em->flags_3C8 & 0x40) || (w->flags & 0x08000000)) {
                    em10WalkRtnSet(em);
                    break;
                }
            } else if (em10FindCk(em, 0)) {
                break;
            }
            if (em->flags_3C8 & 1) {
                em->setFindPL();
            }
        } else {
            if (em->alpha > 0.0f) {
                em->alpha -= 0.1f;
            } else {
                w->flags |= 0x400000;
                em->alpha = 0.0f;
                em->be_flag &= ~2;
                em->atari.flags &= ~0x300;
                em->dmType = 0x80;
            }
        }
        if (em->type == 6) {
            em->setFindPL();
            w->flags |= 0x40000;
        }
        break;
    case 2: {
        void* m0 = PL_ARC_PTR(em->subArc, 0x18);
        void* m1 = PL_ARC_PTR(em->subArc, 0x19);
        int hokan = 1;
        if (w->x518 < 0.0f) {
            hokan = 0x41;
        }
        if (w->pShield) {
            m0 = PL_ARC_PTR(em->subArc, 0x16C);
            m1 = PL_ARC_PTR(em->subArc, 0x16D);
            hokan = 1;
            if (em->flags_3C8 & 0x01000000) {
                hokan = 0x41;
            }
        }
        if (em->type == 0xA || em->type == 0xD) {
            m0 = PL_ARC_PTR(em->subArc, 0x116);
            m1 = PL_ARC_PTR(em->subArc, 0x117);
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, hokan, 0);
        w->x18 = em->rot.y + PI;
        w->x4 = 60;
        em->xFE++;
    }
    case 3:
        if (em->seFlags28B & 8) {
            ang = Muku(&em->pos, &pPL->pos, w->x18, 0.09817477f);
            w->x18 += ang;
            w->x18 = LIMIT_ANGLE(w->x18);
            em->rot.y += ang;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        } else {
            em10AtkRtnCk(em, 0);
        }
        break;
    }
    em10HandSet(em, 0);
    em10ActEvtSetTrade(em);
}

// Hidden (invisible, no collision) and appearing states shared by the hide routines.
// Store order: the pool `lfs` of the 0.0f depends on every store issued before it in RTL (sched1
// true-dependence of a `mem/u` pool load on the `mem/s` stores), so the alpha store must be the FIRST
// statement for its load to be hoisted to the block top like the target; the rest is LUID order.
static inline void em10HideOn(cEm10* em, Em10Work* w)
{
    em->alpha = 0.0f;
    em->be_flag &= ~2;
    em->dmType = 0x80;
    EM10_WK(em)->flags |= 0x400000;
    em->atari.flags &= ~0x300;
    w->x6B6 = 1;
}

static inline void em10HideOff(cEm10* em, Em10Work* w)
{
    em->be_flag |= 2;
    em->atari.flags |= 0x300;
    MotionMoveF(em, 0);
    em->alpha = 1.0f;
    w->x6B6 = 0;
    em->setFindPL();
    em->setStatus(5);
    em->dmType = 0;
    em->x38D = 0;
    w->flags &= ~0x400000;
    if (w->wepType == 7) {
        w->pWep->setEffAlways(0x10, 0x14);
        w->pWep->setEffFall(0x10, 0x1C);
    }
    if (w->wepType == 8 && w->pWep) {
        EstSetEm(w->pWep, -1, 0, 0, 0x10, 0x1F, 0, w->x69F, w->pWep, 0);
    }
}

static void em10_R1_Hide(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWalkMotion(em, 0);
        MotionMoveF(em, 0);
        em10HideOn(em, w);
        em->xFE++;
    case 1:
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        w->flags |= 4;
        em10RouteCk(em);
        em->xFE++;
    case 2:
        em10SetWalkMotion(em, 0);
        MotionMoveF(em, 0);
        em10HideOff(em, w);
        if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01000000) {
            w->x674 = 150;
        }
        em10WalkRtnSet(em);
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_HideFall(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        MotionMoveF(em, 0);
        em10HideOn(em, w);
        em->xFE++;
    case 1:
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        em->xFE++;
    case 2:
        em10HideOff(em, w);
        {
            f32 y = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
            w->x4D8 = em->pos;
            w->x4D8.y = y;
        }
        w->x5E0 = em->pos;
        EmRoutineSet(em, 1, 0x43, 0, 0);
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_HideJump(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    f32 y;

    switch (em->xFE) {
    case 0:
        em10SetWalkMotion(em, 0);
        MotionMoveF(em, 0);
        em10HideOn(em, w);
        em->xFE++;
    case 1:
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        em->xFE++;
    case 2:
        em10HideOff(em, w);
        em->xFF = 0;
        em10SetDashMotion(em);
        w->x6AC = 0;
        RouteCkToPos(em, &pPL->pos, &w->x534, 0, 0);
        w->x504 = Muku(&em->pos, &w->x534, em->rot.y, PI);
        w->x508 = fabsf(w->x504);
        w->x54C = w->x534;
        w->x518 = w->x504;
        w->x51C = w->x508;
        w->x520 = em->plDist2;
        em->xFE++;
    case 3:
        em->dmType = 0xA;
        w->flags |= 0x1000;
        em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.15707964f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 200.0f;
        PSMTXMultVec(em->mat, &v, &v);
        y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
        if (y < em->pos.y - 350.0f) {
            w->x4D8 = em->pos;
            w->x4D8.y = y;
            em->dmType = 0xA;
            w->x5E0 = em->pos;
            EmRoutineSet(em, 1, 0x43, 0, 0);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R10CParasite(cEm10* em)
{
    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->xFE++;
        }
        break;
    case 2:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        } else if (em->frame > 200.7f && em->frame < 201.3f) {
            em->flags_3C8 |= 0x100000;
            em10LostHead(em, 2, 0);
        }
        break;
    case 3:
        em10SetWalkMotion(em, 7);
        em->xFE++;
    case 4:
        MotionMoveF(em, 0);
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R10CPCancel(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        w->flags |= 0x80;
        w->x6AE = 1;
        Ctrl12CntAddI(w->pCtrl12, 4, -1);
        Ctrl12CntAddI(w->pCtrl12, 4, 1);
        em10SetParasite(em);
        em10HeadSet(em, 1);
        EffectEspDelete(0, w->x69D, (u32) em, 0);
        EffectEspgenDelete(0, w->x69D, (int) em);
        EffectEfmDelete(0, w->x69D, (int) em);
        if (w->x1A0) {
            w->x1A0->be_flag &= ~8;
        }
        if (w->x1A4) {
            w->x1A4->be_flag &= ~8;
        }
        if (w->x1A8) {
            w->x1A8->be_flag &= ~8;
        }
        if (w->x1AC) {
            w->x1AC->be_flag &= ~8;
        }
        if (w->x1B4) {
            w->x1B4->be_flag &= ~8;
        }
        if (w->x1B8) {
            w->x1B8->be_flag &= ~8;
        }
        if (w->x1BC) {
            w->x1BC->be_flag &= ~8;
        }
        em->flags_3C8 |= 0x100000;
        em->setFindPL();
        em10WalkRtnSet(em);
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R204Prayer(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        w->x4 = 10;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em10FindCk(em, 1)) {
            break;
        }
        if ((w->flags & 0x100) || w->x5EC) {
            if (w->x4) {
                w->x4--;
            } else if (!em10GotoCk(em)) {
                em10WalkRtnSet(em);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

// Dragon statue control (game/ctrl14.cpp) as the Ganado riders call it (room 222).
class cCtrlDragon : public cCtrl {
public:
    virtual void setTarget(Vec* pos);           // 0x30
    virtual void getMtx(Mtx m, int flag);       // 0x38
    virtual f32 getAngle();                     // 0x40
    virtual f32 getFireAngle();                 // 0x48
    virtual void setAngleX(f32 x);              // 0x50
    virtual void setAngleY(f32 y);              // 0x58
    virtual void addAngle(f32 d);               // 0x60
    virtual void setFireAngle(f32 a);           // 0x68
    virtual void setHome();                     // 0x70
    virtual void setFire();                     // 0x78
    virtual int ckHitFire(Vec* p);              // 0x80
    virtual int ckHitFireBlocked();             // 0x88
};

#define EM10_DRAGON(w) ((cCtrlDragon*) (w)->pDragon)

static void em10_R1_R222DragonA(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx m;
    Mtx minv;
    Vec pl;
    Vec v;
    f32 x;
    f32 y;
    f32 ay;
    f32 ang;

    em->atari.throughOn();
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        EM10_DRAGON(w)->getMtx(m, 1);
        v.x = 0.0f;
        v.y = -1000.0f;
        v.z = -1000.0f;
        PSMTXMultVec(m, &v, &em->pos);
        em->dmType = 0x1E;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xAE), (int) PL_ARC_PTR(em->subArc, 0xAF), 3, 5, 0);
        w->x20 = 0;
        w->x4 = 90;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->pDragon == 0) {
            break;
        }
        EM10_DRAGON(w)->getMtx(m, 1);
        v.x = 0.0f;
        v.y = -1000.0f;
        v.z = 2000.0f;
        PSMTXMultVec(m, &v, &v);
        if (w->x20) {
            if (w->x4) {
                w->x4--;
                PosToPos(&em->pos, &v, &em->pos, 0.1f);
            } else {
                em->xFE++;
            }
        } else if ((em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.z - v.z) * (em->pos.z - v.z) < 10000.0f) {
            em10SetWaitMotion(em, 0xA);
            w->x20 = 1;
            if (w->pDragon) {
                EM10_DRAGON(w)->setFire();
            }
        }
        break;
    case 2:
        em10SetWaitMotion(em, 0xA);
        w->x4 = 90;
        em->xFE++;
    case 3:
        if (w->pDragon) {
            EM10_DRAGON(w)->getMtx(m, 1);
            v.x = 0.0f;
            v.y = -1000.0f;
            v.z = 2000.0f;
            PSMTXMultVec(m, &v, &em->pos);
            EM10_DRAGON(w)->getMtx(m, 0);
            PSMTXInverse(m, minv);
            PSMTXMultVec(minv, &pPL->pos, &pl);
            x = pl.x;
            if (x > 50.0f) {
                x = 50.0f;
            }
            if (x < -50.0f) {
                x = -50.0f;
            }
            EM10_DRAGON(w)->setAngleX(x);
            pl.y += 2000.0f;
            y = pl.y;
            ay = fabsf(y);
            if (y > 50.0f) {
                y = 50.0f;
            }
            if (y < -50.0f) {
                y = -50.0f;
            }
            EM10_DRAGON(w)->setAngleY(y);
            EM10_DRAGON(w)->setTarget(&pl);
            ang = EM10_DRAGON(w)->getAngle();
            ang = Muku(&pl, &pPL->pos, ang, 0.0015339808f);
            EM10_DRAGON(w)->addAngle(ang);
            if (w->x4 == 0) {
                if (ay < 150.0f && fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) < 0.5235988f &&
                    (em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.y - pPL->pos.y) * (em->pos.y - pPL->pos.y) +
                            (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z) <
                        400000000.0f) {
                    em->xFE++;
                    break;
                }
            } else {
                w->x4--;
            }
        }
        MotionMoveF(em, 0);
        break;
    case 4:
        em10SetWaitMotion(em, 0);
        w->x4 = 150;
        w->x20 = 0;
        EM10_DRAGON(w)->setFire();
        w->x18 = 0.0f;
        w->x1C = EM10_DRAGON(w)->getFireAngle();
        w->x8 = 30;
        w->xC = 15;
        em->xFE++;
    case 5:
        if (w->pDragon) {
            EM10_DRAGON(w)->getMtx(m, 1);
            v.x = 0.0f;
            v.y = -1000.0f;
            v.z = 2000.0f;
            PSMTXMultVec(m, &v, &em->pos);
            if (w->x4 > 30) {
                if (w->x8) {
                    w->x8--;
                } else {
                    ang = SINF(w->x18) * 0.19634955f + w->x1C;
                    EM10_DRAGON(w)->setFireAngle(ang);
                    w->x18 += 0.06981317f;
                    if (w->xC) {
                        w->xC--;
                    } else {
                        em10DragonFireCk(em);
                    }
                }
            }
        }
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
        } else {
            em->xFE = 2;
        }
        break;
    }
    if (em->hp <= 0) {
        w->x5DC = em->rot.y;
        if (Rnd() & 1) {
            w->x5DC += 1.5707964f;
        } else {
            w->x5DC -= 1.5707964f;
        }
        w->x5DC = LIMIT_ANGLE(w->x5DC);
        EmRoutineSet(em, 2, 7, 0, 1);
    } else {
        em10HandSet(em, 0);
    }
}
static void em10_R1_R222DragonB(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx m;
    Mtx minv;
    Vec pl;
    Vec v;
    f32 x;
    f32 y;
    f32 ay;
    f32 ang;

    em->atari.throughOn();
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        EM10_DRAGON(w)->getMtx(m, 1);
        v.x = 0.0f;
        v.y = -1000.0f;
        v.z = -1000.0f;
        PSMTXMultVec(m, &v, &em->pos);
        em->dmType = 0x1E;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xAE), (int) PL_ARC_PTR(em->subArc, 0xAF), 3, 5, 0);
        w->x20 = 0;
        w->x4 = 90;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->pDragon == 0) {
            break;
        }
        EM10_DRAGON(w)->getMtx(m, 1);
        v.x = 0.0f;
        v.y = -1000.0f;
        v.z = 2000.0f;
        PSMTXMultVec(m, &v, &v);
        if (w->x20) {
            if (w->x4) {
                w->x4--;
                PosToPos(&em->pos, &v, &em->pos, 0.1f);
            } else {
                em->xFE++;
            }
        } else if ((em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.z - v.z) * (em->pos.z - v.z) < 10000.0f) {
            em10SetWaitMotion(em, 0xA);
            w->x20 = 1;
            if (w->pDragon) {
                EM10_DRAGON(w)->setFire();
            }
        }
        break;
    case 2:
        em10SetWaitMotion(em, 0xA);
        w->x4 = 90;
        em->xFE++;
    case 3:
        if (w->pDragon) {
            EM10_DRAGON(w)->getMtx(m, 1);
            v.x = 0.0f;
            v.y = -1000.0f;
            v.z = 2000.0f;
            PSMTXMultVec(m, &v, &em->pos);
            EM10_DRAGON(w)->getMtx(m, 0);
            PSMTXInverse(m, minv);
            PSMTXMultVec(minv, &pPL->pos, &pl);
            x = pl.x;
            if (x > 50.0f) {
                x = 50.0f;
            }
            if (x < -50.0f) {
                x = -50.0f;
            }
            EM10_DRAGON(w)->setAngleX(x);
            pl.y += 2000.0f;
            y = pl.y;
            ay = fabsf(y);
            if (y > 50.0f) {
                y = 50.0f;
            }
            if (y < -50.0f) {
                y = -50.0f;
            }
            if (pPL->pos.x > -32000.0f) {
                y = 50.0f;
                if (w->x4 <= 29) {
                    w->x4 = 30;
                }
            }
            EM10_DRAGON(w)->setAngleY(y);
            EM10_DRAGON(w)->setTarget(&pl);
            ang = EM10_DRAGON(w)->getAngle();
            ang = Muku(&pl, &pPL->pos, ang, 0.0015339808f);
            EM10_DRAGON(w)->addAngle(ang);
            if (w->x4 == 0) {
                if (ay < 150.0f && fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) < 0.5235988f &&
                    (em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.y - pPL->pos.y) * (em->pos.y - pPL->pos.y) +
                            (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z) <
                        400000000.0f) {
                    em->xFE++;
                    break;
                }
            } else {
                w->x4--;
            }
        }
        MotionMoveF(em, 0);
        break;
    case 4:
        em10SetWaitMotion(em, 0);
        w->x4 = 150;
        w->x20 = 0;
        EM10_DRAGON(w)->setFire();
        w->x18 = 0.0f;
        w->x1C = EM10_DRAGON(w)->getFireAngle();
        w->x8 = 30;
        w->xC = 15;
        em->xFE++;
    case 5:
        if (w->pDragon) {
            EM10_DRAGON(w)->getMtx(m, 1);
            v.x = 0.0f;
            v.y = -1000.0f;
            v.z = 2000.0f;
            PSMTXMultVec(m, &v, &em->pos);
            if (w->x4 > 30) {
                if (w->x8) {
                    w->x8--;
                } else {
                    ang = SINF(w->x18) * 0.19634955f + w->x1C;
                    EM10_DRAGON(w)->setFireAngle(ang);
                    w->x18 += 0.06981317f;
                    if (w->xC) {
                        w->xC--;
                    } else {
                        em10DragonFireCk(em);
                    }
                }
            }
        }
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
        } else {
            em->xFE = 2;
        }
        break;
    }
    if (em->hp <= 0) {
        w->x5DC = em->rot.y;
        if (Rnd() & 1) {
            w->x5DC += 1.5707964f;
        } else {
            w->x5DC -= 1.5707964f;
        }
        w->x5DC = LIMIT_ANGLE(w->x5DC);
        EmRoutineSet(em, 2, 7, 0, 1);
    } else {
        em10HandSet(em, 0);
    }
}
static void em10_R1_R222DragonC(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx m;
    Mtx minv;
    Vec pl;
    Vec v;
    f32 y;
    f32 ay;
    f32 ang;

    em->atari.throughOn();
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        w->x4 = 90;
        em->xFE++;
    case 1:
        if (w->pDragon) {
            EM10_DRAGON(w)->getMtx(m, 1);
            v.x = 0.0f;
            v.y = -1000.0f;
            v.z = 2000.0f;
            PSMTXMultVec(m, &v, &em->pos);
            EM10_DRAGON(w)->getMtx(m, 0);
            PSMTXInverse(m, minv);
            PSMTXMultVec(minv, &pPL->pos, &pl);
            pl.y += 2000.0f;
            y = pl.y;
            ay = fabsf(y);
            if (y > 200.0f) {
                y = 200.0f;
            }
            if (y < -200.0f) {
                y = -200.0f;
            }
            if (pPL->pos.x > -53000.0f && (s32) pG->flags_174 >= 0) {
                y = 200.0f;
                if (w->x4 <= 99) {
                    w->x4 = 100;
                }
                EM10_DRAGON(w)->setHome();
            } else {
                EM10_DRAGON(w)->setTarget(&pl);
                ang = EM10_DRAGON(w)->getAngle();
                ang = Muku(&pl, &pPL->pos, ang, 0.0061359233f);
                EM10_DRAGON(w)->addAngle(ang);
            }
            EM10_DRAGON(w)->setAngleY(y);
            if (w->x4 == 0) {
                if (ay < 150.0f && fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) < 1.1170107f &&
                    (em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.y - pPL->pos.y) * (em->pos.y - pPL->pos.y) +
                            (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z) <
                        400000000.0f) {
                    em->xFE++;
                    break;
                }
            } else {
                w->x4--;
            }
        }
        MotionMoveF(em, 0);
        break;
    case 2:
        em10SetWaitMotion(em, 0);
        w->x4 = 150;
        w->x20 = 0;
        EM10_DRAGON(w)->setFire();
        w->x18 = 0.0f;
        w->x1C = EM10_DRAGON(w)->getFireAngle();
        w->x8 = 30;
        w->xC = 15;
        em->xFE++;
    case 3:
        if (w->pDragon) {
            EM10_DRAGON(w)->getMtx(m, 1);
            v.x = 0.0f;
            v.y = -1000.0f;
            v.z = 2000.0f;
            PSMTXMultVec(m, &v, &em->pos);
            if (w->x4 > 30) {
                if (w->x8) {
                    w->x8--;
                } else {
                    ang = SINF(w->x18) * 0.19634955f + w->x1C;
                    EM10_DRAGON(w)->setFireAngle(ang);
                    w->x18 += 0.06981317f;
                    if (w->xC) {
                        w->xC--;
                    } else {
                        em10DragonFireCk(em);
                    }
                }
            }
        }
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
        } else {
            em->xFE = 0;
        }
        break;
    }
    if (em->hp <= 0) {
        w->x5DC = em->rot.y;
        if (Rnd() & 1) {
            w->x5DC += 1.5707964f;
        } else {
            w->x5DC -= 1.5707964f;
        }
        w->x5DC = LIMIT_ANGLE(w->x5DC);
        EmRoutineSet(em, 2, 7, 0, 1);
    } else {
        em10HandSet(em, 0);
    }
}

static void em10_R1_R227Barrel(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        w->x4 = 150;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->pos.y < pPL->pos.y + 2000.0f) {
            em10WalkRtnSet(em);
        } else if (!(em->flags_3C8 & 1)) {
            w->x4 = 0;
        } else if (w->x4) {
            w->x4--;
        } else {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x9C), (int) PL_ARC_PTR(em->subArc, 0x9D), 30, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        } else if ((em->seFlags28B & 1) && w->pSwitch) {
            ((cEmSwitch*) w->pSwitch)->setClose();
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R21BTrolleyJump(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            w->x5E0 = em->pos;
            em->x38D = 0;
            w->flags |= 0x10080000;
            em->xFC = 1;
            em->xFD = 0x43;
            em->xFE = 0;
            em->xFF = 0;
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R21BTrolleyJump2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->xFE++;
        }
        break;
    case 2:
        if (w->wepType == 7) {
            w->pWep->setEffAlways(0x10, 0x14);
            w->pWep->setEffFall(0x10, 0x1C);
        }
        if (w->wepType == 8 && w->pWep) {
            EstSetEm(w->pWep, -1, 0, 0, 0x10, 0x1F, 0, w->x69F, w->pWep, 0);
        }
        em->xFF = 0;
        em10SetDashMotion(em);
        w->x6AC = 0;
        em->xFE++;
    case 3:
        em->dmType = 0xA;
        MotionMoveF(em, 0);
        if (em10JumpDownCk(em)) {
            w->x5E0 = em->pos;
            em->x38D = 0;
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R303FireDash(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0: {
        cModelInfo* info;
        em10SetWaitMotion(em, 0);
        em->be_flag &= ~2;
        em->hp = 0;
        for (info = em->pInfo; info; info = info->pNext) {
            info->color[0] = 0x80;
            info->color[1] = 0x30;
            info->color[2] = 0x30;
        }
        if (w->x178) {
            ((cObj12*) w->x178)->setBurn();
        }
        w->x8 = 10;
        em->xFE++;
    }
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->be_flag |= 2;
            em->hp = em->hpMax;
            SndCall(6, 3, &em->pos, 0, 0, em);
            if (w->x4) {
                w->x4--;
            } else {
                EmRoutineSet(em, 1, 0x39, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R10FGJump(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    Vec dir;
    Vec dir2;

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        w->x4 = 90;
        em->atari.throughOn();
        em->hp = 1;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->evtMot[0] && w->evtMot[1] && w->evtMot[2]) {
            cObjGondola* g = em10GetGondola(em);
            if (g) {
                f32 fr;
                if (em->x38D == 0x2C) {
                    fr = 682.0f;
                } else {
                    fr = 1286.0f;
                }
                if (g->motFrame == fr) {
                    w->pGondola = g;
                    em->xFE++;
                }
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), w->evtMot[0], 0, 3, 1, 0);
        w->x4 = 10;
        em->xFE++;
    case 3:
        if (w->x4) {
            w->x4--;
        } else {
            w->flags |= 0x100000;
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), w->evtMot[1], 0, 3, 1, 0);
        if (w->pGondola) {
            w->pGondola->setVib();
            if ((s16) pG->pl_life > 0) {
                SetPlDamage((int) em, plem10DmGondolaShake);
            }
            if (pSUB && pSUB->hp > 0) {
                SetSubDamage((int) em, (void*) subem10DmGondolaShake);
            }
        }
        SndCall(6, 0xC, &em->pos, 0, 0, em);
        em->xFE++;
    case 5:
        w->flags |= 0x100000;
        if (w->pGondola) {
            cModel* parts = w->pGondola->getPartsPtr(1);
            dir.x = 0.0f;
            dir.y = 0.0f;
            dir.z = 1.0f;
            PSMTXMultVecSR(parts->mat, &dir, &dir);
            em->rot.y = atan2f(dir.x, dir.z);
            v.x = 51.9f;
            v.y = -2757.56f;
            v.z = -1419.8f;
            PSMTXMultVec(parts->mat, &v, &em->pos);
            if (em->frame > 41.7f && em->frame < 42.3f) {
                SndCall(6, 0xD, &em->pos, 0, 0, em);
                EstSetEm(em, -1, 0, 0, 1, 2, 0, 0, em, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), w->evtMot[2], 0, 3, 5, 0);
        w->x4 = 5;
        em->xFE++;
    case 7:
        w->flags |= 0x100000;
        if (w->pGondola) {
            cModel* parts = w->pGondola->getPartsPtr(1);
            dir2.x = 0.0f;
            dir2.y = 0.0f;
            dir2.z = 1.0f;
            PSMTXMultVecSR(parts->mat, &dir2, &dir2);
            em->rot.y = atan2f(dir2.x, dir2.z);
            v.x = 51.9f;
            v.y = -2757.56f;
            v.z = -1419.8f;
            PSMTXMultVec(parts->mat, &v, &em->pos);
        }
        MotionMoveF(em, 0);
        if (em->frame > 32.7f && em->frame < 33.3f) {
            SndCall(6, 0xD, &em->pos, 0, 0, em);
            EstSetEm(em, -1, 0, 0, 1, 2, 0, 0, em, 0);
        }
        if (w->x4 && em->frame > 32.7f && em->frame < 33.3f && w->pGondola) {
            w->x4--;
            if (w->x4 > 0) {
                w->pGondola->setDamage();
                w->pGondola->setVib();
            } else {
                w->pGondola->setBreak();
                w->pGondola->setVib();
                if ((s16) pG->pl_life > 0) {
                    SetPlDamage((int) em, plem10DmGondolaShake);
                    pPL->xFF = 1;
                }
                if (pSUB && pSUB->hp > 0) {
                    SetSubDamage((int) em, (void*) subem10DmGondolaShake);
                    pSUB->xFF = 1;
                }
                em->dmType = 0x80;
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

// Thrown-weapon attack parameters (.data): axe / dynamite throw, scythe throw.
// Attack parameters by attack number (em10AtkCk / em10BellAtkCk index it; the axe / scythe throws
// hand entries 5 / 6 to cEmWep::setThrow).
static EmAtkInfo Em10AtkTbl[19] = {
    { 250.0f, 8, 380, 0, 10, 0 },
    { 250.0f, 8, 380, 0, 10, 0 },
    { 500.0f, 8, 480, 0, 10, 0 },
    { 350.0f, 8, 480, 0, 10, 0 },
    { 350.0f, 8, 480, 0, 10, 0 },
    { 250.0f, 8, 380, 0, 10, 0 },
    { 750.0f, 8, 700, 0, 10, 0 },
    { 250.0f, 8, 400, 0, 10, 0 },
    { 250.0f, 8, 380, 0, 10, 0 },
    { 250.0f, 8, 700, 0, 10, 0 },
    { 250.0f, 8, 700, 0, 10, 0 },
    { 500.0f, 8, 10, 0, 10, 0 },
    { 500.0f, 8, 9999, 8, 10, 0 },
    { 400.0f, 8, 640, 0, 10, 0 },
    { 400.0f, 8, 1400, 0, 10, 0 },
    { 400.0f, 8, 900, 0, 10, 0 },
    { 400.0f, 8, 800, 0, 10, 0 },
    { 400.0f, 8, 0, 0, 10, 0 },
    { 250.0f, 8, 570, 0, 10, 0 },
};

static void em10_R1_R10FGondola(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec tgt = { 26400.0f, 10279.0f, -32116.0f };
    Vec pos;
    Vec d;
    Mtx m;
    Vec wpos;
    Vec spd;
    Vec diff;
    Vec plPos;
    Vec wpos2;
    cModel* parts;
    f32 len;
    f32 t;
    f32 dist;

    parts = pPL->getPartsPtr(0);
    PSVECSubtract(&parts->worldPos, &parts->x88, &d);
    len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z);
    PSVECScale(&d, &d, len / 350.0f + 10.0f);
    PSVECAdd(&pPL->pos, &d, &pos);
    pos.y += 1300.0f;
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        em->motFlags &= ~1;
        em->atari.throughOn();
        w->x4 = 90;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &pos, em->rot.y, PI);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        dist = (em->pos.x - tgt.x) * (em->pos.x - tgt.x) + (em->pos.y - tgt.y) * (em->pos.y - tgt.y) + (em->pos.z - tgt.z) * (em->pos.z - tgt.z);
        if (dist < 64000000.0f) {
            EmRoutineSet(em, 3, 3, 0, 0);
            return;
        }
        if (w->pWep == 0) {
            em->xFE = 4;
        } else {
            // `dist` (the tgt distance above) reused for the limit: the multi-set pseudo is global and
            // takes f13 for both the `fmadds` result and the limit loads.
            switch (em->emsetNo % 3) {
            default:
                dist = 400000000.0f;
                break;
            case 1:
                dist = 324000000.0f;
                break;
            case 2:
                dist = 256000000.0f;
                break;
            }
            if (em->plDist2 < dist && em->pos.y < pPL->pos.y) {
                em->xFE = 2;
            }
        }
        break;
    case 2: {
        int hokan = 0;
        if (em->flags_3C8 & 0x01000000) {
            hokan = 0x40;
        }
        if (w->wepType != 6) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, hokan, 0x10);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x13D), (int) PL_ARC_PTR(em->subArc, 0x13F), 10, hokan, 0);
        }
        w->x4 = 10;
        em10CallVoiceSe2(em, w->se6D2, 8);
        em->xFE++;
    }
    case 3:
        em->rot.y += Muku(&em->pos, &pos, em->rot.y, PI);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
            break;
        }
        if (em->seFlags28B & 4) {
            w->x670 = 2;
        }
        if ((em->seFlags28B & 1) && w->pWep) {
            wpos.x = w->pWep->mat[0][3];
            wpos.y = w->pWep->mat[1][3];
            wpos.z = w->pWep->mat[2][3];
            PSVECSubtract(&pos, &wpos, &diff);
            len = SQRTF(diff.x * diff.x + diff.z * diff.z) * 0.0028571428f;
            t = diff.y / ((len + 1.0f) * 0.5f * len);
            spd.y = t * len;
            switch (Rnd() % 5) {
            case 0:
            default:
                spd.x = fRand1_1() * 10.0f;
                break;
            case 1:
                spd.x = 5.0f;
                break;
            case 2:
                spd.x = -5.0f;
                break;
            case 3:
                spd.x = 10.0f;
                break;
            case 4:
                spd.x = -10.0f;
                break;
            }
            spd.z = 350.0f;
            PSMTXRotRad(m, 'y', atan2f(diff.x, diff.z));
            PSMTXMultVecSR(m, &spd, &spd);
            if (w->wepType != 6) {
                cEmWepSetThrowF(w->pWep, &spd, &Em10AtkTbl[5], t);
            } else {
                plPos = pPL->pos;
                plPos.y += 1500.0f;
                wpos2.x = w->pWep->mat[0][3];
                wpos2.y = w->pWep->mat[1][3];
                wpos2.z = w->pWep->mat[2][3];
                PSVECSubtract(&plPos, &wpos2, &spd);
#line 6995 "D:/Bio4/Prog/em10.cpp"
                VECNormalize(&spd, &spd);
                PSVECScale(&spd, &spd, 250.0f);
                w->pWep->setThrowScythe(&spd, &Em10AtkTbl[6]);
            }
            w->pWep = 0;
            w->wepType = 0;
            w->x670 = 10;
        }
        break;
    case 4: {
        int hokan = 0;
        if (em->flags_3C8 & 0x01000000) {
            hokan = 0x40;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x76), (int) PL_ARC_PTR(em->subArc, 0x77), 10, hokan, 0);
        em->xFE++;
    }
    case 5:
        em->rot.y += Muku(&em->pos, &pos, em->rot.y, PI);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if ((em->seFlags28B & 4) && w->pWep2) {
            w->pWep = em10MakeWeapon(em, w->wep2Type);
            if (w->pWep) {
                w->wepType = w->wep2Type;
            } else {
                w->wepType = 0;
            }
            em10WeaponSet(em);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R209DashSit(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xAE), (int) PL_ARC_PTR(em->subArc, 0xAF), 0, 5, 0);
        w->x4 = 70;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
        } else {
            EmRoutineSet(em, 1, 0x1A, 0, 1);
        }
        break;
    }
    em10HandSet(em, 0);
}

// Claw walk / claw attack shared tail: back to the walk or the dash.
static inline void em10ClawAtkEnd(cEm10* em, Em10Work* w)
{
    if (!(w->flags & 0x08000000) &&
        (em->pos.x - w->x534.x) * (em->pos.x - w->x534.x) + (em->pos.z - w->x534.z) * (em->pos.z - w->x534.z) > 9000000.0f) {
        w->x6AC = 0;
        EmRoutineSet(em, 1, 0x11, 0, 0);
    } else {
        em10WalkRtnSet(em);
    }
}

static void em10_R1_StickClaw(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->xFE == 0 && fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 1.9198622f) {
        em->xFE = 2;
    }
    if (em10FindCk2(em)) {
        w->x654 = 150;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x115), 0, 3, 1, 0);
        EstSetEm(em, -1, 0, 0, 0x10, 0x76, 0, 0, em, 0);
        w->x4 = 25;
        w->x8 = 46;
        if (em->xFF == 0) {
            em10CallVoiceSe2(em, 0x71, 6);
        }
        if (w->x5EC == 0) {
            u8 r = Rnd() % 10;
            if (r > 4) {
                w->x5F0 = pPL->pos;
            }
            w->x654 = 150;
        }
        em->xFE++;
    case 1:
        if (em->frame > 9.7f && em->frame < 10.3f) {
            w->x6BE = 1;
            w->x6BF = 1;
        }
        if (MotionMoveF(em, 0) && !em10AtkRtnCk(em, 0)) {
            em10ClawAtkEnd(em, w);
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x116), (int) PL_ARC_PTR(em->subArc, 0x117), 10, 1, 0);
        em10CallVoiceSe2(em, 0x71, 6);
        em->xFF = 1;
        if (w->x5EC == 0) {
            u8 r = Rnd() % 10;
            if (r > 4) {
                w->x5F0 = pPL->pos;
            }
            w->x654 = 150;
        }
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0) || (em->seFlags28B & 1)) {
            if (!em10AtkRtnCk(em, 0)) {
                em10ClawAtkEnd(em, w);
            }
        } else if (em->frame > 9.7f && em->frame < 10.3f) {
            w->x6BE = 1;
            w->x6BF = 1;
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R11DAppear1(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        if (em->flags_3C8 & 0x01000000) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xE7), (int) PL_ARC_PTR(em->subArc, 0xE8), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xE7), (int) PL_ARC_PTR(em->subArc, 0xE8), 10, 1, 0);
        }
        EstSetEm(em, -1, 0, 0, 0x10, 0x2B, 0, 0, em, 0);
        w->x4 = 30;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 4) {
            if (em->type == 0x16) {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 0x96, 0, w->x69C, w->pWep, 0);
            } else {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 9, 0, w->x69C, w->pWep, 0);
            }
            w->flags |= 0x80000000;
            w->x65C = Rnd() % 150 + 150;
            SndCall(6, 0x4C, &em->pos, 0, 0, em);
            w->x684 = 60;
            em10FindNotify(em);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        em10SetDashMotion(em);
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        if (!em10JumpDownCk(em)) {
            em10ClimbOverCk(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R11DAppear2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        EM10_WK(em)->flags |= 0x80000000;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->evtMot[0] == 0) {
            break;
        }
        em->xFE++;
    case 2:
        MotionSetCore(em, MOTION(em), w->evtMot[0], 0, 0, 1, 0);
        SndStop(w->x5C4, 0);
        w->x5C4 = SndCall(6, 0x50, &em->pos, 0, 0, em);
        SndStop(w->x5B8, 0);
        SndStop(w->x5BC, 0);
        w->flags |= 0x80000000;
        EstSetEm(w->pWep, -1, 0, 0, 0x10, 9, 0, w->x69C, w->pWep, 0);
        w->flags |= 0x80000000;
        w->x65C = Rnd() % 150 + 150;
        w->x684 = 60;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        } else if (em->frame > 39.7f && em->frame < 40.3f) {
            SndStop(w->x5B8, 0);
            SndStop(w->x5BC, 0);
            w->x5B8 = SndCall(6, 0x3D, &em->pos, 0, 0, em);
        }
        break;
    }
    em10HandSet(em, 0);
}

// Ganado riding the R212 drill: follow its root part (em10_R1_R212Drill).
#define EM10_DRILL_FOLLOW                                                                              \
    if (w->x20) {                                                                                      \
        v.x = 465.71f;                                                                                 \
        v.y = 550.9f;                                                                                  \
        v.z = -1243.91f;                                                                               \
    } else {                                                                                           \
        v.x = -604.1f;                                                                                 \
        v.y = 550.9f;                                                                                  \
        v.z = -1243.91f;                                                                               \
    }                                                                                                  \
    PSMTXMultVec(((cModel*) w->x564)->getPartsPtr(0)->mat, &v, &em->pos);                              \
    em->rot.y = ((cModel*) w->x564)->rot.y;

static void em10_R1_R212Drill(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        AtariOff(&em->atari, 0xFCFF);
        w->scaleBase.x = 1.0f;
        w->scaleBase.y = 1.0f;
        w->scaleBase.z = 1.0f;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (!w->x564 || !w->evtMot[0] || !w->evtMot[1]) {
            break;
        }
        em->setStatus(5);
        em->xFE++;
    case 2:
        MotionSetCore(em, MOTION(em), w->evtMot[0], 0, 0, 5, 0);
        em->xFE++;
    case 3:
        EM10_DRILL_FOLLOW;
        MotionMoveF(em, 0);
        if (em->hp <= 0) {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), w->evtMot[1], 0, 3, 1, 0);
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em->xFE++;
    case 5:
        EM10_DRILL_FOLLOW;
        if (MotionMoveF(em, 0)) {
            EmSetDie(em);
            EmReserveDropItem(em);
            em10SetPoint(em);
            em->clearStatus(5);
        }
        break;
    }
    em10HandSet(em, 0);
}
#undef EM10_DRILL_FOLLOW

// Room 209: Ganado on the mounted gatling (evtMot[0..3] = fire / reload / hit / die, setGatling).
static void em10_R1_R209Gatling(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cObjGatling* g = w->pGatling;
    Vec ofs;

    if (g) {
        ofs.x = 8.46f;
        ofs.y = 63.6f;
        ofs.z = -778.04f;
        PSMTXMultVec(g->mat, &ofs, &em->pos);
        em->rot.y = g->rot.y;
    }
    switch (em->xFE) {
    case 0:
        if (w->evtMot[0]) {
            MotionSetCore(em, MOTION(em), w->evtMot[0], 0, 5, 5, 0);
        } else {
            em10SetWaitMotion(em, 0);
        }
        AtariOff(&em->atari, 0xFCFF);
        w->scaleBase.x = 1.0f;
        w->scaleBase.y = 1.0f;
        w->scaleBase.z = 1.0f;
        if (!(em->flags_3C8 & 1) || !w->gatlingMode) {
            MotionMoveF(em, 0);
            break;
        }
        if (w->pGatling) {
            w->pGatling->setFire();
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->pGatling) {
            if (w->pGatling->ckReload()) {
                em->xFE++;
            } else if (w->pGatling && w->pGatling->ckBreak()) {
                em->xFE = 6;
            }
        }
        break;
    case 2:
        if (w->evtMot[1]) {
            MotionSetCore(em, MOTION(em), w->evtMot[1], 0, 5, 1, 0);
        } else {
            em10SetWaitMotion(em, 0);
        }
        w->pGatling->stopFire();
        w->pGatling->setReload();
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->pGatling) {
                em->xFE = 0;
            }
        } else if (w->pGatling && w->pGatling->ckBreak()) {
            em->xFE = 6;
        }
        break;
    case 4:
        if (w->evtMot[2]) {
            MotionSetCore(em, MOTION(em), w->evtMot[2], 0, 5, 1, 0);
        } else {
            em10SetWaitMotion(em, 0);
        }
        w->pGatling->stopFire();
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        w->x6C3 = 90;
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        } else if (w->pGatling && w->pGatling->ckBreak()) {
            em->xFE = 6;
        }
        break;
    case 6:
        if (w->evtMot[3]) {
            MotionSetCore(em, MOTION(em), w->evtMot[3], 0, 5, 1, 0);
        } else {
            em10SetWaitMotion(em, 0);
        }
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        if (w->x6AE) {
            w->x6AE = 0;
            Ctrl12CntAddI(w->pCtrl12, 4, -1);
        }
        em10CoreBreak(em, 0);
        w->pGatling->stopFire();
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            EmSetDie(em);
            em10SetPoint(em);
            em->clearStatus(5);
            em->xFE++;
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R201EventWait(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x108), 0, 0, 5, 0);
        em->atari.clrFlag100();
        em->xFE++;
    case 1:
        em->atari.flags |= 8;
        w->x6BC = 2;
        w->flags |= 8;
        MotionMoveF(em, 0);
        if (!w->evtMot[0] || !w->evtMot[1]) {
            break;
        }
        MotionMoveF(em, 0);
        em->xFE++;
    case 2:
        em->pos.x = 34588.95f;
        em->pos.y = -2003.77f;
        em->pos.z = -41650.69f;
        em->rot.y = -1.5707964f;
        MotionSetCore(em, MOTION(em), w->evtMot[0], 0, 0, 5, 0);
        em->xFE++;
    case 3:
        em->atari.flags |= 8;
        w->x6BC = 2;
        w->flags |= 8;
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), w->evtMot[1], 0, 0, 5, 0);
        em->setStatus(5);
        em->setFindPL();
        w->x5F0 = pPL->pos;
        w->x4 = 30;
        em10CallVoiceSe2(em, w->se6CA, 8);
        em->xFE++;
    case 5:
        if (w->x4) {
            w->x4--;
            w->flags |= 8;
            if (w->x4 == 0) {
                em->atari.setFlag100();
            }
        }
        if (MotionMoveF(em, 0)) {
            em->atari.setFlag100();
            EmRoutineSet(em, 1, 0x59, 0, 0);
        } else if (MOTION(em)->seqFrame > 14.7f && MOTION(em)->seqFrame < 15.3f) {
            SndCall(6, 0x11, &em->pos, 0, 0, em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_FindLost(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->flags |= 0x800000;
            w->flags &= ~0x100;
            w->x63C = em10GetWanderRoute(em);
            w->x656 = 0;
            if (w->x6BE != 4) {
                w->x6BE = 3;
            }
            if (w->x6BF != 4) {
                w->x6BF = 3;
            }
            em10WalkRtnSet(em);
        }
        if (em10FindCk2(em)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R100WalkStay(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    em->dmg.x1 = 2;
    switch (em->xFE) {
    case 0:
        em10SetWalkMotion(em, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        em10SetWaitMotion(em, 10);
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            w->flags &= ~0x100;
            if (em->x3D0 != 1 && em->x3D0 != 3) {
                EmRoutineSet(em, 1, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R202Finger(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    int flag;

    w->flags |= 0x8000;
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        em->xFE++;
    case 1:
        em->dmg.x1 = 2;
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->xFE++;
        }
        break;
    case 2:
        m0 = PL_ARC_PTR(em->subArc, 0x73);
        m1 = PL_ARC_PTR(em->subArc, 0x74);
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        em->xFE++;
    case 3:
        em->dmg.x1 = 2;
        if (em->seFlags28B & 8) {
            w->flags |= 0x2000000;
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 4:
        em10SetWaitMotion(em, 10);
        em->xFE++;
    case 5:
        MotionMoveF(em, 0);
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_StayWalk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int r;

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        w->x4 = 600;
        if (w->wepType == 4) {
            if (em->type == 0x16) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x96, 0, w->x69C, (u32) w->pWep, 0);
            } else {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 9, 0, w->x69C, (u32) w->pWep, 0);
            }
            w->flags |= 0x80000000;
            w->x65C = (u8) (Rnd() % 150) + 150;
            w->x684 = 60;
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        r = em10GotoCk(em);
        if (r) {
            break;
        }
        if (w->flags & 1) {
            w->x4 = r;
        }
        if (em->flags_3C8 & 1) {
            w->x4 = r;
        }
        if (w->x4) {
            w->x4--;
        } else {
            em->setFindPL();
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}


// Waits for the player to come in front of the Ganado, then picks the weapon's attack routine.
static void em10_R1_AttackWait(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    Vec c;
    cModel* p;
    f32 d2;
    f32 dy;
    int no;

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        w->x4 = 600;
        if (w->wepType == 4) {
            if (em->type == 0x16) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x96, 0, w->x69C, (u32) w->pWep, 0);
            } else {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 9, 0, w->x69C, (u32) w->pWep, 0);
            }
            w->flags |= 0x80000000;
            w->x65C = (u8) (Rnd() % 150) + 150;
            w->x684 = 60;
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em10GotoCk(em)) {
            return;
        }
        if ((w->flags & 1) && w->x508 > 1.5707964f) {
            em->setFindPL();
            em10WalkRtnSet(em);
            break;
        }
        p = pPL->getPartsPtr(0);
        PSVECSubtract(&p->worldPos, &p->x88, &a);
        PSVECScale(&a, &a, 30.0f);
        PSVECAdd(&pPL->pos, &a, &b);
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 1000.0f;
        PSMTXMultVec(em->mat, &a, &c);
        d2 = (b.x - c.x) * (b.x - c.x) + (b.z - c.z) * (b.z - c.z);
        dy = b.y - c.y;
        dy = fabsf(dy);
        if (d2 < 1000000.0f && dy < 1000.0f) {
            no = 0;
            em->x38D = no;
            if (em->type == 0xA || em->type == 0xD) {
                EmRoutineSet(em, 1, 0x2B, 0, 0xA);
                return;
            }
            if (em->type == 2) {
                EmRoutineSet(em, 1, 0x23, 0, 0);
                return;
            }
            switch (w->wepType) {
            default:
            case 0:
                if (w->pShield) {
                    EmRoutineSet(em, 1, 0x27, 0, 0);
                    break;
                }
            case 9:
                EmRoutineSet(em, 1, 0x33, 0, 0);
                break;
            case 1:
                EmRoutineSet(em, 1, 0x29, 0, 0);
                break;
            case 2:
            case 3:
            case 7:
            case 0xA:
            case 0xB:
            case 0xF:
            case 0x10:
                EmRoutineSet(em, 1, 0x26, 0, 0);
                break;
            case 4:
                EmRoutineSet(em, 1, 0x2F, 0, 0);
                break;
            case 6:
                EmRoutineSet(em, 1, 0x2A, 0, 0xA);
                break;
            case 8:
                EmRoutineSet(em, 1, 0x21, 0, 0);
                break;
            case 0xC:
                EmRoutineSet(em, 1, 0x22, 0, 0);
                break;
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R100TurnWalk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    PlArc* arc;
    void* m0;
    void* m1;
    int flag;

    em->dmg.x1 = 0x80;
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        w->x4 = 90;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
            break;
        }
        em->xFE++;
    case 2:
        arc = em->subArc;
        m0 = PL_ARC_PTR(arc, 0x18);
        m1 = PL_ARC_PTR(arc, 0x19);
        flag = (w->x518 < 0.0f) ? 0x41 : 1;
        // direct `em->subArc` reads below: cse copies the `arc` load into a second pseudo (`mr r10, r11`)
        if (w->pShield) {
            m0 = PL_ARC_PTR(em->subArc, 0x16C);
            m1 = PL_ARC_PTR(em->subArc, 0x16D);
            flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        }
        if (em->type == 10 || em->type == 13) {
            m0 = PL_ARC_PTR(em->subArc, 0x116);
            m1 = PL_ARC_PTR(em->subArc, 0x117);
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 4:
        em10SetWalkMotion(em, 7);
        em->xFE++;
    case 5:
        MotionMoveF(em, 0);
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R100Cliff(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    u32 i;

    em->dmg.x1 = 0x80;
    switch (em->xFE) {
    case 0:
        if (w->evtMot[0]) {
            m0 = w->evtMot[0];
            m1 = w->evtMot[4];
        } else {
            m0 = PL_ARC_PTR(em->subArc, 5);
            m1 = 0;
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 0, 5, 0);
        MotionMoveF(em, 0);
        em->hp = 0;
        em->atari.flags &= ~0x200;
        em->xFE++;
        break;
    case 1:
        if (w->evtMot[0]) {
            m0 = w->evtMot[0];
            m1 = w->evtMot[4];
        } else {
            m0 = PL_ARC_PTR(em->subArc, 5);
            m1 = 0;
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 0, 5, 0);
        MotionMoveF(em, 0);
        w->x4 = (*(u16*) m0 & 0x3FFF) - 10;
        if (em->plDist2 < 784000000.0f || (em->flags_3C8 & 1)) {
            em->xFE++;
            em->flags_3C8 |= 1;
            if (em->x38D == 2) {
                SndCall(6, 4, &em->pos, 0, 0, em);
            }
            for (i = 0; i < EmMgr.nArray; i++) {
                cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
                if ((e->be_flag & 0x201) != 1) {
                    continue;
                }
                if (e->id <= 0xF) {
                    continue;
                }
                if (e->id > 0x20) {
                    continue;
                }
                if (e->hp <= 0) {
                    continue;
                }
                if (e == em) {
                    continue;
                }
                switch (e->x38D) {
                case 2:
                case 3:
                case 4:
                    e->flags_3C8 |= 1;
                    break;
                }
            }
        }
        break;
    case 2:
        EmSetDie(em);
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
        } else {
            em->alpha -= 0.1f;
            if (em->alpha <= 0.0f) {
                em->alpha = 0.0f;
                em->atari.flags &= 0xFCFF;
                em->be_flag &= ~2;
                em->clearStatus(5);
                em->xFE++;
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

// Room 101 bucket carrier (.data 0x680 / 0x6A4): where the Ganado walks to with / without the
// bucket, per bucket route (x38D 6 / 0x1A / other).
static Vec em10_r101_bucket_pos[3] = {
    { -7540.0f, 0.0f, 600.0f },
    { -6340.0f, 0.0f, -900.0f },
    { -40240.0f, 0.0f, -32290.0f },
};
static Vec em10_r101_bucket_pos2[3] = {
    { -4971.0f, 141.0f, 8452.0f },
    { 15502.0f, 1456.0f, 12082.0f },
    { -33010.0f, 0.0f, -22460.0f },
};

// Room 101: the Ganado carrying a bucket between the two positions of its route (x38D picks it).
static void em10_R1_R101Bucket(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModel* p;
    Vec* rp;
    Vec* rp2;
    int no;
    f32 d2;

    no = em->x38D == 6;
    if (em->x38D == 0x1A) {
        no = 2;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1AF), (int) PL_ARC_PTR(em->subArc, 0x1B0), 3, 1, 0);
        if (w->wepType == 5 && w->pWep && w->mot[0x2D]) {
            w->pWep->rot.x = 0.0f;
            w->pWep->rot.y = 0.0f;
            w->pWep->rot.z = 0.0f;
            MotionSetCore(w->pWep, MOTION(w->pWep), w->mot[0x2D], 0, 0, 0, 0);
        }
        EstSetEm(em, -1, 0, 0, 0x10, 0x12, 0, w->x69E, em, 0);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x20) {
            SndCall(6, 0xF, &em->pos, 0, 0, em);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1B1), (int) PL_ARC_PTR(em->subArc, 0x1B2), 3, 5, 0);
        if (w->wepType == 5 && w->pWep && w->mot[0x2E]) {
            MotionSetCore(w->pWep, MOTION(w->pWep), w->mot[0x2E], 0, 0, 4, 0);
        }
        w->x5E0 = em10_r101_bucket_pos2[no];
        em->xFE++;
    case 3:
        if ((pG->flags_51E4 & 7) == (em->emsetNo & 7)) {
            RouteCkToPos(em, &em10_r101_bucket_pos2[no], &w->x5E0, 0, 0);
        }
        em->rot.y += Muku(&em->pos, &w->x5E0, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        rp2 = &em10_r101_bucket_pos2[no];
        d2 = (em->pos.x - rp2->x) * (em->pos.x - rp2->x) + (em->pos.z - rp2->z) * (em->pos.z - rp2->z);
        if (d2 < 250000.0f) {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1B3), (int) PL_ARC_PTR(em->subArc, 0x1B4), 10, 1, 0);
        if (w->wepType == 5 && w->pWep && w->mot[0x2E]) {
            MotionSetCore(w->pWep, MOTION(w->pWep), w->mot[0x2F], 0, 0, 0, 0);
        }
        EffectEspDelete(0, w->x69E, (u32) em, 0);
        EffectEspgenDelete(0, w->x69E, (int) em);
        EffectEfmDelete(0, w->x69E, (int) em);
        EstSetEm(em, -1, 0, 0, 0x10, 0x13, 0, w->x69E, em, 0);
        em->xFE++;
    case 5:
        if (em->seFlags28B & 0x20) {
            SndCall(6, 0xF, &em->pos, 0, 0, em);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 6:
        EffectEspDelete(0, w->x69E, (u32) em, 0);
        EffectEspgenDelete(0, w->x69E, (int) em);
        EffectEfmDelete(0, w->x69E, (int) em);
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 8), (int) PL_ARC_PTR(em->subArc, 0xB), 5, 5, 0);
        w->x5E0 = em10_r101_bucket_pos[no];
        em->xFE++;
    case 7:
        if ((pG->flags_51E4 & 7) == (em->emsetNo & 7)) {
            RouteCkToPos(em, &em10_r101_bucket_pos[no], &w->x5E0, 0, 0);
        }
        em->rot.y += Muku(&em->pos, &w->x5E0, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        p = em->getPartsPtr(10);
        p->rot.x = 0.017601645f;
        p->rot.y = 0.05197416f;
        p->rot.z = 0.04245688f;
        RotMatrix(p->worldMat, &p->rot);
        TransMatrix(p->worldMat, &p->pos);
        ScaleMatrix(p->worldMat, &p->scale);
        rp = &em10_r101_bucket_pos[no];
        d2 = (em->pos.x - rp->x) * (em->pos.x - rp->x) + (em->pos.z - rp->z) * (em->pos.z - rp->z);
        if (d2 < 90000.0f) {
            em->xFE = 0;
        }
        break;
    }
    if (!em10FindCk(em, 0)) {
        if (w->flags & 0x100) {
            em10WalkRtnSet(em);
        } else {
            em10HandSet(em, 0);
        }
    }
}

static void em10_R1_R101Suki(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1B5), (int) PL_ARC_PTR(em->subArc, 0x1B6), 3, 5, 0);
        w->x4 = Rnd() % 5 + 5;
        EstSetEm(em, -1, 0, 0, 0x10, 0x15, 0, w->x69E, em, 0);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x20) {
            SndCall(6, 0x10, &em->pos, 0, 0, em);
        }
        if (em->seFlags28B & 1) {
            EstSetEm(em, -1, 0, 0, 0x10, 0xF, 0, 0, em, 0);
        }
        if (em->seFlags28B & 4) {
            EstSetEm(em, -1, 0, 0, 0x10, 0x11, 0, 0, em, 0);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x4) {
                w->x4--;
                EstSetEm(em, -1, 0, 0, 0x10, 0x15, 0, w->x69E, em, 0);
            } else {
                em->xFE++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1B7), 0, 3, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    if (!em10FindCk(em, 0)) {
        if (w->flags & 0x100) {
            em10WalkRtnSet(em);
        } else {
            em10HandSet(em, 0);
        }
    }
}

static void em10_R1_Work(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        break;
    }
    if (!em10FindCk(em, 0)) {
        if (w->flags & 0x100) {
            em10WalkRtnSet(em);
        } else {
            em10HandSet(em, 0);
        }
    }
}

static void em10_R1_UFOCatch(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    em->dmg.x1 = 2;
    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        if (w->evtMot[0]) {
            MotionSetCore(em, MOTION(em), w->evtMot[0], 0, 10, 5, 0);
        } else {
            em10SetWaitMotion(em, 0);
        }
        em->flags_3C8 &= ~1;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->xFE++;
        }
        break;
    case 2:
        if (w->evtMot[1]) {
            MotionSetCore(em, MOTION(em), w->evtMot[1], 0, 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x21), 0, 3, 1, 0);
        }
        em10SetDamageVoice(em, w->se6D4, w->se6D4);
        em->flags_3C8 &= ~1;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->setLost();
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R300TakeAshley(cEm10* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2A0), (int) PL_ARC_PTR(em->subArc, 0x2A6), 0, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R30FBullJump(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    int end;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR((PlArc*) pG->pRoomArc, 0x39), (int) PL_ARC_PTR((PlArc*) pG->pRoomArc, 0x3A), 3, 1, 0);
        em10CallVoiceSe2(em, w->se6D7, 8);
        em->xFE++;
    case 1:
        w->flags |= 0x10080000;
        em->setStatus(3);
        end = MotionMoveF(em, 0);
        if (em->seFlags28B & 4) {
            f32 y;
            v = em->pos;
            v.y = em->oldPos.y;
            y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
            // The landing tail is repeated in both arms (jump2 cross-jumps it): with the MotionSetCore call
            // in the SndCall's block, the SndCall arg `li r3, 8` is issued after `mr r8` like the target.
            if (em->pos.y < y) {
                em->pos.y = y;
                w->x5A4.y = 0.0f;
                SndCall(8, 5, &em->pos, em->id, 0, em);
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x25), 0, 3, 1, 0);
                MotionMoveF(em, 0);
                em->xFE = 2;
            } else if (end) {
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x25), 0, 3, 1, 0);
                MotionMoveF(em, 0);
                em->xFE = 2;
            }
        }
        break;
    case 2:
        if (ChkWaterEffectEnable(&em->pos)) {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x31, 0, 0, (u32) em, 0);
        } else {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x17, 0, 0, (u32) em, 0);
        }
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R320Gatling(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        w->flags |= 0x100;
        em->xFE++;
    case 1:
        int one = 1;
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, PI);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (!(pG->flags_64 & 0x2000000) && !em10GotoCk(em)) {
            if (em->type == 6) {
                em->setFindPL();
                w->flags |= 0x40000;
            } else if ((s16) pG->pl_life <= 0 || (pSUB && (s16) pG->sub_life <= 0)) {
                do { // loop notes keep `one` at its declaration (update_equiv_regs moves single-use constants only outside loops)
                    EmRoutineSet(em, one, 0x1B, 0, 0);
                } while (0);
            } else if (!em10FindCk(em, 0)) {
                if (em->plDist2 < 9000000.0f) {
                    em->x38D = 0;
                    em10WalkRtnSet(em);
                } else {
                    em10AtkRtnCk(em, 0);
                }
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R321DeadBody(cEm10* em)
{
    EmRoutineSet(em, 3, 0, 0, 0);
}

static void em10_R1_R300Gatling(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (!w->evtMot[0] || !w->evtMot[4]) {
            break;
        }
        em->xFE++;
    case 2:
        MotionSetCore(em, MOTION(em), w->evtMot[0], (int) w->evtMot[4], 0, 1, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        SndCall(8, 0xB3, &em->pos, em->id, 0, em);
        EstSetEm(em, -1, 0, 0, 1, 0x11, 0, 0, em, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        } else if (em->seFlags28B & 1) {
            SndCall(6, 9, &em->pos, 0, 0, 0);
        }
        break;
    }
    em10HandSet(em, 0);
}

// Cart route of room 101 (.data 0x6C8): the points the Ganado pushes the cart along.
static Vec em10_r101_cart_route[7] = {
    { -11340.0f, 0.0f, 2630.0f },
    { -4020.0f, 0.0f, -550.0f },
    { -3680.0f, 0.0f, -960.0f },
    { 1100.0f, 0.0f, -1590.0f },
    { 1240.0f, 0.0f, 2700.0f },
    { -8420.0f, 0.0f, 6720.0f },
    { -15600.0f, 0.0f, -6190.0f },
};

static void em10_R1_R101Cart(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1B8), (int) PL_ARC_PTR(em->subArc, 0x1B9), 3, 5, 0);
        if (w->pHead && w->mot[50]) {
            MotionSetCore(w->pHead, MOTION(w->pHead), w->mot[50], 0, 0, 4, 0);
        }
        w->x4 = 0;
        w->x8 = Rnd() % 15 + 5;
        em->xFE++;
    case 1:
        RouteCkToPos(em, &em10_r101_cart_route[w->x4], &w->x54C, 0, 0);
        w->x518 = Muku(&em->pos, &w->x54C, em->rot.y, PI);
        w->x51C = fabsf(w->x518);
        {
            Vec* rp = &em10_r101_cart_route[w->x4];
            f32 dx = em->pos.x - rp->x;
            f32 dz = em->pos.z - rp->z;
            w->x520 = dx * dx + dz * dz;
        }
        em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.012271847f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (w->x520 < 1000000.0f) {
            w->x4++;
            if (w->x4 > 6) {
                w->x4 = 0;
            }
        }
        MotionMoveF(em, 0);
        break;
    }
    if (!em10FindCk(em, 0)) {
        if (w->flags & 0x100) {
            em10WalkRtnSet(em);
        } else {
            em10HandSet(em, 0);
            if (w->pHead) {
                v.x = 0.0f;
                v.y = 0.0f;
                v.z = 1700.0f;
                PSMTXMultVec(em->mat, &v, &w->pHead->pos);
                w->pHead->rot = em->rot;
            }
            if (w->x8) {
                w->x8--;
            } else {
                w->x8 = Rnd() % 30 + 15;
                EstSetEm(w->pHead, -1, 0, 0, 0x10, 0x16, 0, 0, w->pHead, 0);
            }
        }
    }
}

static void em10_R1_br_EvtDash(cEm10* em)
{
    if (!em10DoorOpenCk(em, 0) && !em10RackBreakCk(em) && !em10LadderClimbCk(em) && !em10VLadderClimbCk(em) && !em10LadderResetCk(em) && !em10JumpDownCk(em) && !em10JumpCk(em) && !em10ClimbOverCk(em)) {
        em10WindowCk(em);
    }
}

static void em10_R1_EvtDash(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em->xFF = Rnd() % 256;
        em10SetDashMotion(em);
        if (w->wepType == 4 && (s32) w->flags < 0) {
            if (em->type == 0x16) {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 0x96, 0, w->x69C, w->pWep, 0);
            } else {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 9, 0, w->x69C, w->pWep, 0);
            }
            w->flags |= 0x80000000;
            w->x65C = Rnd() % 150 + 150;
            SndCall(6, 0x4C, &em->pos, 0, 0, em);
            w->x684 = 60;
        }
        w->x4 = 120;
        em->xFF = 0;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
        } else {
            em->xFC = 1;
            em->xFD = 0x11;
            em->xFE = 0;
            em->xFF = (u8) (MOTION(em)->seqFrame * 255.0f / (f32) MOTION(em)->seqMax);
            w->x6AC = Rnd() % 7;
        }
        break;
    case 2:
        if (em->flags_3C8 & 0x1000000) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xE7), (int) PL_ARC_PTR(em->subArc, 0xE8), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xE7), (int) PL_ARC_PTR(em->subArc, 0xE8), 10, 1, 0);
        }
        EstSetEm(em, -1, 0, 0, 0x10, 0x2B, 0, 0, em, 0);
        w->x4 = 30;
        em->xFE++;
    case 3:
        if (em->seFlags28B & 4) {
            if (em->type == 0x16) {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 0x96, 0, w->x69C, w->pWep, 0);
            } else {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 9, 0, w->x69C, w->pWep, 0);
            }
            w->flags |= 0x80000000;
            w->x65C = Rnd() % 150 + 150;
            SndCall(6, 0x4C, &em->pos, 0, 0, em);
            w->x684 = 60;
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_br_EvtWalk(cEm10* em)
{
    if (!em10DoorOpenCk(em, 0) && !em10RackBreakCk(em) && !em10LadderClimbCk(em) && !em10VLadderClimbCk(em) && !em10LadderResetCk(em) && !em10JumpDownCk(em) && !em10JumpCk(em) && !em10ClimbOverCk(em)) {
        em10WindowCk(em);
    }
}

static void em10_R1_EvtWalk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em->xFF = Rnd() % 256;
        em10SetWalkMotion(em, 7);
        if (w->wepType == 4 && (s32) w->flags < 0) {
            if (em->type == 0x16) {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 0x96, 0, w->x69C, w->pWep, 0);
            } else {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 9, 0, w->x69C, w->pWep, 0);
            }
            w->flags |= 0x80000000;
            w->x65C = Rnd() % 150 + 150;
            SndCall(6, 0x4C, &em->pos, 0, 0, em);
            w->x684 = 60;
        }
        w->x4 = 120;
        em->xFF = 0;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
        } else {
            em->xFC = 1;
            em->xFD = 0x10;
            em->xFE = 0;
            em->xFF = (u8) (MOTION(em)->seqFrame * 255.0f / (f32) MOTION(em)->seqMax);
            w->x6AC = Rnd() % 11;
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_Pickup(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    if ((w->flags & 0x100) && (em->flags_3C8 & 0x2000)) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x76), (int) PL_ARC_PTR(em->subArc, 0x77), 10, flag, 0);
        em->xFE++;
    case 1:
        if ((em->seFlags28B & 4) && w->pWep2) {
            if (em->flags_3C8 & 0x10000) {
                w->pWep = em10MakeWeapon(em, w->wep2Type);
                if (w->pWep) {
                    w->wepType = w->wep2Type;
                } else {
                    w->wepType = 0;
                }
            } else {
                w->pWep2->setTransMode(1);
                w->pWep = w->pWep2;
                w->wepType = w->wep2Type;
                w->pWep2 = 0;
                w->wep2Type = 0;
            }
            em10WeaponSet(em);
        }
        if (MotionMoveF(em, 0)) {
            if (w->pWep && w->wepType == 4 && !(w->flags & 0x80000000)) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
                break;
            }
            if (w->pWep && w->wepType == 9 && w->x640 == 0 && (w->x524 < 15000.0f || em->x3D0 == 2) && (w->flags & 1)) {
                if (em->flags_3C8 & 0x10000) {
                    EmRoutineSet(em, 1, 0xF, 0, 0);
                    break;
                }
                if (em->pos.y > pPL->pos.y - 500.0f) {
                    EmRoutineSet(em, 1, 0xF, 0, 0);
                    break;
                }
            }
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Find(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    int flag;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        m0 = PL_ARC_PTR(em->subArc, 0x73);
        m1 = PL_ARC_PTR(em->subArc, 0x74);
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        w->x4 = 30;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (em->seFlags28B & 0x20) {
            em10CallVoiceSe2(em, w->se6CA, 8);
        }
        if (em->seFlags28B & 8) {
            w->flags |= 0x2000000;
        }
        if (w->x4) {
            w->x4--;
            if (w->x4 == 0) {
                em10FindNotify(em);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (em->flags_3C8 & 0x2000000) {
                EmRoutineSet(em, 1, 0x11, 0, 0);
            } else {
                em10WalkRtnSet(em);
            }
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 2);
    em10GotoCk(em);
}

static void em10_R1_C_SawStart(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        if (em->flags_3C8 & 0x1000000) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xE7), (int) PL_ARC_PTR(em->subArc, 0xE8), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xE7), (int) PL_ARC_PTR(em->subArc, 0xE8), 10, 1, 0);
        }
        EstSetEm(em, -1, 0, 0, 0x10, 0x2B, 0, 0, em, 0);
        w->x4 = 30;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 4) {
            if (em->type == 0x16) {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 0x96, 0, w->x69C, w->pWep, 0);
            } else {
                EstSetEm(w->pWep, -1, 0, 0, 0x10, 9, 0, w->x69C, w->pWep, 0);
            }
            w->flags |= 0x80000000;
            w->x65C = Rnd() % 150 + 150;
            SndCall(6, 0x4C, &em->pos, 0, 0, em);
            w->x684 = 60;
            em10FindNotify(em);
        }
        if (MotionMoveF(em, 0)) {
            if (em->flags_3C8 & 0x2000000) {
                EmRoutineSet(em, 1, 0x11, 0, 0);
            } else {
                em10WalkRtnSet(em);
            }
        } else if ((em->seFlags28B & 1) && w->x51C > 1.9634955f) {
            EmRoutineSet(em, 1, 0x15, 0, 0);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_BombIgnition(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec ofs;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        if (em->flags_3C8 & 0x1000000) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x7E), (int) PL_ARC_PTR(em->subArc, 0x7F), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x7E), (int) PL_ARC_PTR(em->subArc, 0x7F), 10, 1, 0);
        }
        w->x4 = 30;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x11, 0, 0);
        } else if (em->seFlags28B & 1) {
            w->x640 = 9999;
            w->pWep->setEffAlways(0x10, 0x2D);
            ofs.x = 0.0f;
            ofs.y = 40.0f;
            ofs.z = 60.0f;
            w->pWep->setEffAlways2(0x10, 0x2F, 0, &ofs, 3);
            SndCall(8, 0x94, &em->pos, em->id, 0, em);
            em10ThrowBombCk(em);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_br_Walk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->hp > 0) {
        int one = 1; // kept in a callee-saved reg across the calls (docs/matching.md)
        if (pG->flags_64 & 0x2000000) {
            EmRoutineSet(em, one, 0, 0, 0);
        } else if (!em10GotoCk(em) && !em10DoorOpenCk(em, 0) && !em10RackBreakCk(em) && !em10LadderClimbCk(em) && !em10VLadderClimbCk(em) && !em10LadderResetCk(em) && !em10JumpDownCk(em) && !em10JumpCk(em)) {
            em10ReturnStartPosCk(em);
            if (!em10ClimbOverCk(em) && !em10WindowCk(em)) {
                if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && w->x634 > 30 && fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) < 0.5235988f) {
                    EmRoutineSet(em, one, 0x1C, 0, 0);
                } else if (!em10IgnitionCk(em) && !em10ClawStickCK(em) && !em10FindLostCk(em)) {
                    if (w->flags & 0x20000000) {
                        f32 dx = em->pos.x - w->x4D8.x;
                        f32 dz = em->pos.z - w->x4D8.z;
                        if (dx * dx + dz * dz < 4000000.0f) {
                            w->flags &= ~0x20000000;
                            EmRoutineSet(em, 1, 1, 0, 0);
                            return;
                        }
                        if (em->plDist2 < 4000000.0f && w->x530 < em->x3CC) {
                            w->flags &= ~0x20000000;
                        }
                    }
                    em10GotoPosCk(em);
                }
            }
        }
    }
}

static void em10_R1_Walk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int end;
    int r;
    f32 a;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        em10MouthPartsReset(em);
        em10SetWalkMotion(em, 7);
        w->x4 = Rnd() % 2;
        w->x8 = 0;
        w->xC = 15;
        w->x1C = 0.0f;
        if (w->flags & 0x100) {
            w->x6B8 = 1;
        }
        em->xFE++;
    case 1:
        if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && em->pos.y > pPL->pos.y + 500.0f) {
            a = Muku(&em->pos, &pPL->pos, em->rot.y, PI);
        } else {
            a = Muku(&em->pos, &w->x54C, em->rot.y, PI);
        }
        w->x1C = a * 0.3f;
        w->x1C = Muku2(0.0f, w->x1C, 0.15707964f);
        em->rot.y += w->x1C;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        end = MotionMoveF(em, 0);
        if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
            em->hp = 0;
        }
        if (em->pos.y < -99000.0f) {
            em->hp = 0;
        }
        if (em->hp <= 0) {
            if (end) {
                EmRoutineSet(em, 2, 9, 0, 0);
            }
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            break;
        }
        if (pSUB && (s16) pG->sub_life <= 0) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            break;
        }
        if (em10AtkRtnCk(em, 0)) {
            break;
        }
        if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && em->pos.y > pPL->pos.y + 500.0f) {
            if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 2.7488937f && em->plDist2 > 36000000.0f) {
                EmRoutineSet(em, 1, 0x15, 0, 0);
                break;
            }
        } else if (w->x51C > 2.7488937f && em->plDist2 > 36000000.0f) {
            EmRoutineSet(em, 1, 0x15, 0, 0);
            break;
        }
        if (end) {
            if (em10StayCk(em)) {
                break;
            }
            if (em10DashCk(em)) {
                break;
            }
            if ((u8) (Rnd() % 10) == 0 && !Ctrl12Ck(w->pCtrl12, 0xC)) {
                em10CallVoiceSe2(em, w->se6D5, 8);
            }
        }
        if (w->x4 == 0) {
            if (em10ThreatCk(em)) {
                break;
            }
        }
        if (w->x4) {
            if (em10HeadLockCk(em)) {
                if (++w->x8 > 10) {
                    if ((u8) (Rnd() % 3)) {
                        EmRoutineSet(em, 1, 0x1D, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0x14, 0, 0);
                    }
                    break;
                }
            } else {
                w->x8 = 0;
            }
        }
        r = em10FindCk(em, 2);
        if (!r && (w->flags & 0x800000) && end && (Rnd() & 1) && (s16) w->x656 > 150 && em->type == 0xA) {
            EmRoutineSet(em, 1, 0x5D, 0, 0);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
    em10CsawSignSe(em);
    em10BehindSeCk(em);
    if (em->type == 0x16) {
        EmRoutineSet(em, 1, 0x6D, 0, 0);
    }
}

static void em10_R1_br_Dash(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->hp > 0) {
        int one = 1; // kept in a callee-saved reg across the calls (docs/matching.md)
        if (pG->flags_64 & 0x2000000) {
            EmRoutineSet(em, one, 0, 0, 0);
        } else if (!em10GotoCk(em) && !em10DoorOpenCk(em, 0) && !em10RackBreakCk(em) && !em10LadderClimbCk(em) && !em10VLadderClimbCk(em) && !em10LadderResetCk(em) && !em10JumpDownCk(em) && !em10JumpCk(em)) {
            em10ReturnStartPosCk(em);
            if (!em10ClimbOverCk(em) && !em10WindowCk(em) && !em10IgnitionCk(em) && !em10ClawStickCK(em) && !em10FindLostCk(em)) {
                if (w->flags & 0x20000000) {
                    f32 dx = em->pos.x - w->x4D8.x;
                    f32 dz = em->pos.z - w->x4D8.z;
                    if (dx * dx + dz * dz < 4000000.0f) {
                        w->flags &= ~0x20000000;
                        EmRoutineSet(em, one, one, 0, 0);
                        return;
                    }
                    if (em->plDist2 < 4000000.0f && w->x530 < em->x3CC) {
                        w->flags &= ~0x20000000;
                    }
                }
                em10GotoPosCk(em);
            }
        }
    }
}

static void em10_R1_Dash(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int end;
    f32 lim;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    w->flags |= 0x40;
    if (w->wepType == 4 || w->pShield) {
        w->flags &= ~0x40;
    }
    switch (em->xFE) {
    case 0:
        em->flags_3C8 &= ~0x80;
        em10SetDashMotion(em);
        w->x4 = (u8) (Rnd() % 3) + 5;
        if (w->x6C5) {
            w->x674 = (int) Rnd() % 450 + 150;
        } else {
            w->x674 = (u8) (Rnd() % 150) + 150;
        }
        if (em->type == 0xA) {
            w->xC = 60;
        } else {
            w->xC = 15;
        }
        if (w->flags & 0x100) {
            w->x6B8 = 1;
        }
        em->xFE++;
    case 1:
        if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && em->pos.y > pPL->pos.y + 500.0f) {
            em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.19634955f);
        } else {
            em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.19634955f);
        }
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        end = MotionMoveF(em, 0);
        if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
            em->hp = 0;
        }
        if (em->pos.y < -99000.0f) {
            em->hp = 0;
        }
        if (em->hp <= 0) {
            if (end) {
                EmRoutineSet(em, 2, 9, 0, 0);
            }
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            break;
        }
        if (pSUB && (s16) pG->sub_life <= 0) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            break;
        }
        if (em10AtkRtnCk(em, 0)) {
            break;
        }
        if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && em->pos.y > pPL->pos.y + 500.0f) {
            if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 2.7488937f && em->plDist2 > 36000000.0f) {
                EmRoutineSet(em, 1, 0x15, 0, 0);
                break;
            }
        } else if (w->x51C > 1.9634955f) {
            EmRoutineSet(em, 1, 0x15, 0, 0);
            break;
        }
        if (w->x644) {
            break;
        }
        if (end || w->x634 > 60) {
            if (--w->x4 <= 0) {
                em->xFC = 1;
                em->xFD = 0x10;
                em->xFE = 0;
                em->xFF = (u8) (MOTION(em)->seqFrame * 255.0f / (f32) MOTION(em)->seqMax);
                w->x6AC = Rnd() % 11;
                break;
            }
        }
        lim = 2500.0f;
        if (pG->x4F88 <= 3) {
            lim = 5000.0f;
        }
        if (em->type == 0xA || em->type == 0xD) {
            lim = 0.0f;
        }
        if (em->type == 0x16 && w->x524 < 5000.0f) {
            EmRoutineSet(em, 1, 0x6D, 0, 0);
            break;
        }
        if (w->x524 < lim && w->x4) {
            em->xFC = 1;
            em->xFD = 0x10;
            em->xFE = 0;
            em->xFF = (u8) (MOTION(em)->seqFrame * 255.0f / (f32) MOTION(em)->seqMax);
            w->x6AC = Rnd() % 11;
            break;
        }
        em10FindCk(em, 2);
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
    em10CsawSignSe(em);
    em10BehindSeCk(em);
    if (em->type == 0xA || em->type == 0xD) {
        DmgMgr.set(3, 2, &em->pos, 1500.0f, 1000.0f);
    }
}

static void em10_R1_br_Back(cEm10* em)
{
    if (em->hp > 0) {
        em10GotoCk(em);
    }
}

// Back-step motion set with / without the 0x40 "keep the current frame" flag (em10_R1_Back).
#define EM10_BACK_MOT(a, b)                                                                            \
    if (em->flags_3C8 & 0x1000000) {                                                                   \
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, a), (int) PL_ARC_PTR(em->subArc, b), 5, 0x45, 0); \
    } else {                                                                                           \
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, a), (int) PL_ARC_PTR(em->subArc, b), 5, 5, 0); \
    }

static void em10_R1_Back(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int end;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        if (w->pShield) {
            EM10_BACK_MOT(0x16A, 0x16B);
        } else {
            switch (w->wepType) {
            case 0:
                EM10_BACK_MOT(0x15, 0x14);
                break;
            default:
                EM10_BACK_MOT(0xE4, 0xE5);
                break;
            case 1:
                EM10_BACK_MOT(0x157, 0x158);
                break;
            case 6:
                EM10_BACK_MOT(0x142, 0x143);
                break;
            }
        }
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        end = MotionMoveF(em, 0);
        if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
            em->hp = 0;
        }
        if (em->hp <= 0) {
            if (end) {
                EmRoutineSet(em, 2, 9, 0, 0);
            }
            break;
        }
        if ((s16) pG->pl_life <= 0 || (pSUB && (s16) pG->sub_life <= 0)) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            break;
        }
        if (em10AtkRtnCk(em, 1)) {
            break;
        }
        if (em->xFF) {
            if (end) {
                EmRoutineSet(em, 1, 0x1B, 0, 0);
            }
        } else if (end || em->plDist2 > 16000000.0f) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
    em10CsawSignSe(em);
    em10BehindSeCk(em);
}
#undef EM10_BACK_MOT

static void em10_R1_br_Goto(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->hp > 0 && !em10DoorOpenCk(em, 1) && !em10RackBreakCk(em) && !em10LadderClimbCk(em) && !em10VLadderClimbCk(em) && !em10LadderResetCk(em) && !em10JumpDownCk(em) && !em10JumpCk(em)) {
        em10ReturnStartPosCk(em);
        if (!em10ClimbOverCk(em) && !em10WindowCk(em) && em->xFE == 0 && w->x5EC != 8 && w->x51C > 1.9198622f) {
            EmRoutineSet(em, 1, 0x15, 0, 2);
        }
    }
}

// Routine set on arriving at the goto position: the special route routines by x38D, else `dflt`.
#define EM10_GOTO_ARRIVE_RTN(dflt)                                                                     \
    switch (em->x38D) {                                                                                \
    default:                                                                                           \
        dflt;                                                                                          \
        break;                                                                                         \
    case 0x24:                                                                                         \
        EmRoutineSet(em, 1, 0x50, 0, 0);                                                               \
        break;                                                                                         \
    case 0x25:                                                                                         \
        EmRoutineSet(em, 1, 0x50, 0, 1);                                                               \
        break;                                                                                         \
    case 0x2B:                                                                                         \
        EmRoutineSet(em, 1, 0x50, 0, 2);                                                               \
        break;                                                                                         \
    case 0x36:                                                                                         \
        EmRoutineSet(em, 1, 0x60, 0, 0);                                                               \
        break;                                                                                         \
    case 0x19:                                                                                         \
        EmRoutineSet(em, 1, 0x48, 0, 0);                                                               \
        break;                                                                                         \
    case 0x3E:                                                                                         \
        EmRoutineSet(em, 1, 0x6A, 0, 0);                                                               \
        break;                                                                                         \
    case 0x40:                                                                                         \
        EmRoutineSet(em, 1, 0x6C, 0, 0);                                                               \
        break;                                                                                         \
    }

// Default arm of EM10_GOTO_ARRIVE_RTN for the plain goto: idle (x3D0 1 / 3) or wait.
#define EM10_GOTO_ARRIVE_WAIT                                                                          \
    if (em->x3D0 != 1 && em->x3D0 != 3) {                                                              \
        EmRoutineSet(em, 1, 0, 0, 0);                                                                  \
    } else {                                                                                           \
        EmRoutineSet(em, 1, 1, 0, 0);                                                                  \
    }

static void em10_R1_Goto(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int end;
    f32 d2;
    f32 dy;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    w->flags |= 0x40;
    if (w->wepType == 4 || w->pShield) {
        w->flags &= ~0x40;
    }
    if (em->xFE == 0 && w->x5EC == 8) {
        em->xFE = 6;
    }
    switch (em->xFE) {
    case 0:
        switch (w->x5EC) {
        default:
            em->xFF = 0;
            em10SetDashMotion(em);
            break;
        case 6:
        case 9:
        case 0xA:
            em10SetWalkMotion(em, 7);
            break;
        }
        em->xFE++;
    case 1:
        if (w->x5EC == 6 || w->x5EC == 0xA) {
            em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.09817477f);
        } else {
            em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.2617994f);
        }
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
            em->hp = 0;
        }
        if (em->hp <= 0) {
            EmRoutineSet(em, 2, 9, 0, 0);
            break;
        }
        d2 = (em->pos.x - w->x5F0.x) * (em->pos.x - w->x5F0.x) + (em->pos.y - w->x5F0.y) * (em->pos.y - w->x5F0.y) + (em->pos.z - w->x5F0.z) * (em->pos.z - w->x5F0.z);
        if (d2 < 250000.0f) {
            switch (w->x5EC) {
            case 0:
                break;
            case 1:
            case 0xA:
            case 0xB:
            case 0xD:
                w->x5EC = 0;
                w->x4D8 = em->pos;
                EM10_GOTO_ARRIVE_RTN(EM10_GOTO_ARRIVE_WAIT);
                break;
            case 2:
                w->x5EC = 0;
                w->x4D8 = em->pos;
                EmRoutineSet(em, 1, 0xD, 0, 0);
                break;
            case 3:
            case 4:
                if (w->pSwitch) {
                    em->xFE++;
                } else {
                    w->x5EC = 0;
                    EM10_GOTO_ARRIVE_RTN(EM10_GOTO_ARRIVE_WAIT);
                }
                break;
            case 5:
                em->xFE = 4;
                break;
            case 9:
                w->x5EC = 0;
                if (em10LadderResetCk(em)) {
                    return;
                }
                em10WalkRtnSet(em);
                break;
            case 6:
            case 7:
                em->xFE = 8;
                break;
            case 0xC:
                em->setFindPL();
                w->x5EC = 0;
                em10WalkRtnSet(em);
                break;
            case 0xE:
                w->x5EC = 0;
                EmRoutineSet(em, 1, 0x1A, 0, 1);
                break;
            }
        } else {
            if (w->x51C > 1.9198622f) {
                EmRoutineSet(em, 1, 0x15, 0, 0);
                break;
            }
            switch (w->x5EC) {
            case 6:
            case 7:
            case 0xA:
            case 0xB:
                em10FindCk(em, 2);
                if (w->flags & 0x100) {
                    w->x5EC = 0;
                    w->x4D8 = em->pos;
                    w->x6AC = Rnd() % 11;
                    em10FindNotify(em);
                    EM10_GOTO_ARRIVE_RTN(EmRoutineSet(em, 1, 0x10, 0, 0));
                }
                break;
            case 0xC:
            case 0xD:
                dy = pPL->pos.y - em->pos.y;
                dy = fabsf(dy);
                if ((w->flags & 1) && dy < 1500.0f && em->plDist2 < 16000000.0f) {
                    em->setFindPL();
                    w->x5EC = 0;
                    em10WalkRtnSet(em);
                    break;
                }
                if (pSUB && (w->flags & 0x8000000)) {
                    dy = pSUB->pos.y - em->pos.y;
                    dy = fabsf(dy);
                    if ((w->flags & 2) && dy < 1500.0f && w->x514 < 16000000.0f) {
                        em->setFindPL();
                        w->x5EC = 0;
                        em10WalkRtnSet(em);
                    }
                }
                break;
            case 0xE: // default-grouped case after [C-D]: shapes the compare tree (`ble` into the arm, `b` to default)
                break;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x9C), (int) PL_ARC_PTR(em->subArc, 0x9D), 5, 1, 0);
        em->xFE++;
    case 3:
        em->rot.y += Muku(&em->pos, &w->pSwitch->pos, em->rot.y, 0.19634955f);
        if (MotionMoveF(em, 0)) {
            EM10_GOTO_ARRIVE_RTN(EM10_GOTO_ARRIVE_WAIT);
            w->pSwitch = 0;
        } else if (em->seFlags28B & 1) {
            switch (w->x5EC) {
            case 3:
                ((cEmSwitch*) w->pSwitch)->setOpen();
                break;
            case 4:
                ((cEmSwitch*) w->pSwitch)->setClose();
                break;
            }
            w->x5EC = 0;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x23), (int) PL_ARC_PTR(em->subArc, 0x24), 3, 1, 0);
        em->rot.y = GetXZAngle(&em->pos, &w->x4EC);
        w->x5EC = 0;
        em->hp = 0;
        em->atari.flags &= 0xFCFF;
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 3, 0, 0);
        }
        break;
    case 6: {
        void* m0 = PL_ARC_PTR(em->subArc, 0x73);
        void* m1 = PL_ARC_PTR(em->subArc, 0x74);
        int flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        em->xFE++;
    }
    case 7:
        w->x24 = w->x4EC;
        em->rot.y += Muku(&em->pos, &w->x24, em->rot.y, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (em->seFlags28B & 0x20) {
            em10CallVoiceSe2(em, w->se6CA, 8);
        }
        if (em->seFlags28B & 8) {
            w->flags |= 0x2000000;
        }
        if (MotionMoveF(em, 0)) {
            w->x5EC = 0;
            em10WalkRtnSet(em);
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2AA), (int) PL_ARC_PTR(em->subArc, 0x2AB), 10, 1, 0);
        em->xFE++;
    case 9:
        end = MotionMoveF(em, 0);
        if (end) {
            w->x5EC = 0;
            w->x4D8 = em->pos;
            EmRoutineSet(em, 1, 0, 0, 0);
            break;
        }
        em10FindCk(em, 2);
        if (w->flags & 0x100) {
            w->x5EC = 0;
            w->x4D8 = em->pos;
            w->x6AC = Rnd() % 11;
            EM10_GOTO_ARRIVE_RTN(EmRoutineSet(em, 1, 0x10, 0, 0));
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}
#undef EM10_GOTO_ARRIVE_RTN
#undef EM10_GOTO_ARRIVE_WAIT

static void em10_R1_GuardWalk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    PlArc* arc;
    int end;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    em->hitInfo.flags &= ~1;
    switch (em->xFE) {
    case 0:
        arc = em->subArc;
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(arc, 6), (int) PL_ARC_PTR(arc, 7), 30, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(arc, 6), (int) PL_ARC_PTR(arc, 7), 30, 0x45, 0);
        }
        em->xFF = Rnd() & 1;
        w->x4 = Rnd() % 120 + 120;
        em->xFE++;
    case 1:
        if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && em->pos.y > pPL->pos.y + 500.0f) {
            em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
        } else {
            em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.09817477f);
        }
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        end = MotionMoveF(em, 0);
        if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
            em->hp = 0;
        }
        if (em->hp <= 0) {
            if (end) {
                EmRoutineSet(em, 2, 9, 0, 0);
            }
        } else if ((s16) pG->pl_life <= 0 || (pSUB && (s16) pG->sub_life <= 0)) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
        } else if (!em10AtkRtnCk(em, 0)) {
            if (w->x4) {
                w->x4--;
            } else {
                u8 f = (u8) (MOTION(em)->seqFrame * 255.0f / (f32) MOTION(em)->seqMax);
                w->x6AC = Rnd() % 11;
                em->xFC = 1;
                em->xFD = 0x10;
                em->xFE = 0;
                em->xFF = f;
            }
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_Turn180(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    int flag;
    f32 a;

    switch (em->xFE) {
    case 0:
        if (em->xFF != 1) {
            m0 = PL_ARC_PTR(em->subArc, 0x18);
            m1 = PL_ARC_PTR(em->subArc, 0x19);
        } else {
            m0 = PL_ARC_PTR(em->subArc, 0x16);
            m1 = PL_ARC_PTR(em->subArc, 0x17);
        }
        flag = 1;
        if (w->x518 < 0.0f) {
            flag = 0x41;
        }
        if (w->pShield) {
            m0 = PL_ARC_PTR(em->subArc, 0x16C);
            m1 = PL_ARC_PTR(em->subArc, 0x16D);
            flag = 1;
            if (em->flags_3C8 & 0x1000000) {
                flag = 0x41;
            }
        }
        if (em->type == 0xA || em->type == 0xD) {
            m0 = PL_ARC_PTR(em->subArc, 0x116);
            m1 = PL_ARC_PTR(em->subArc, 0x117);
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        w->x18 = em->rot.y + PI;
        w->x18 = LIMIT_ANGLE(w->x18);
        w->x4 = 60;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && em->pos.y > pPL->pos.y + 500.0f) {
                a = Muku(&em->pos, &pPL->pos, w->x18, 0.09817477f);
            } else {
                a = Muku(&em->pos, &w->x54C, w->x18, 0.09817477f);
            }
            w->x18 += a;
            w->x18 = LIMIT_ANGLE(w->x18);
            em->rot.y += a;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 4) {
            if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && em->pos.y > pPL->pos.y + 500.0f) {
                em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
            } else {
                em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.09817477f);
            }
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if ((em->seFlags28B & 1) && (w->flags & 0x100)) {
            w->flags |= 0x40000;
        }
        if (w->x4) {
            w->x4--;
            if (w->x4 == 0) {
                em10FindNotify(em);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (em->xFF == 1) {
                EmRoutineSet(em, 1, 0, 0, 0);
                break;
            }
            if (em->xFF == 2 && w->x5EC) {
                EmRoutineSet(em, 1, 0x13, 0, 0);
                break;
            }
            if (em->flags_3C8 & 0x80) {
                EmRoutineSet(em, 1, 0x11, 0, 0);
                break;
            }
            if (em10AtkRtnCk(em, 0)) {
                break;
            }
            if (em10StayCk(em)) {
                break;
            }
            if (em10DashCk(em)) {
                break;
            }
            if (em->type == 0x16) {
                EmRoutineSet(em, 1, 0x6D, 0, 0);
                return;
            }
            w->x6AC = Rnd() % 11;
            EmRoutineSet(em, 1, 0x10, 0, 0);
        } else {
            if (em->xFF != 2 && em10GotoCk(em)) {
                break;
            }
            if ((s16) pG->pl_life <= 0 || (pSUB && (s16) pG->sub_life <= 0)) {
                EmRoutineSet(em, 1, 0x1B, 0, 0);
                break;
            }
            if (em->type != 0xA && em->type != 0xD) {
                em10AtkRtnCk(em, 0);
            }
        }
        break;
    }
    em10CsawSignSe(em);
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_Threat(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        if (Muku(&pPL->pos, &em->pos, pPL->rot.y, PI) > 0.0f) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x71), (int) PL_ARC_PTR(em->subArc, 0x72), 30, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x71), (int) PL_ARC_PTR(em->subArc, 0x72), 30, 0x41, 0);
        }
        w->x4 = 10;
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        } else {
            em10AtkRtnCk(em, 0);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_SideStep(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        switch (em->xFF) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2AC), (int) PL_ARC_PTR(em->subArc, 0x2AD), 5, 1, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2AC), (int) PL_ARC_PTR(em->subArc, 0x2AD), 5, 0x41, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x98), (int) PL_ARC_PTR(em->subArc, 0x99), 5, 1, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x98), (int) PL_ARC_PTR(em->subArc, 0x99), 5, 0x41, 0);
            break;
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->x696 = 1;
            if (!em10AtkRtnCk(em, 0) && !em10HideRtnCk2(em)) {
                em10WalkRtnSet(em);
            }
        } else if (em->seFlags28B & 1) {
            w->x696 = 1;
            em10AtkRtnCk(em, 0);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_HideSide(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        w->x4 = Rnd() % 150 + 90;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            em->setFindPL();
            em->xFC = 1;
            em->xFD = 0x17;
            em->xFE = 0;
        } else {
            if (w->x4 == 0) {
                if (em10HideToStepCk(em, em->xFF)) {
                    if (!(w->flags & 0x100)) {
                        em->setFindPL();
                        em10CallVoiceSe2(em, w->se6CA, 8);
                        if (w->x4) {
                            w->x4--;
                            if (w->x4 == 0) {
                                em10FindNotify(em);
                            }
                        }
                    }
                    break;
                }
            } else {
                w->x4--;
            }
            if (w->x524 < 4000.0f) {
                em10WalkRtnSet(em);
            }
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_AppearSide(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        w->x4 = Rnd() % 150 + 90;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->setFindPL();
            em->xFC = 1;
            em->xFD = 0x17;
            em->xFE = 0;
        } else if (w->x52C < em->x3CC) {
            em->setFindPL();
            em10WalkRtnSet(em);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
}

static void em10_R1_SitDown(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x9A), 0, 5, 1, 0);
        em->flags_3C8 &= ~1;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
            if (w->wepType == 8) {
                if (w->x6B5 == 0 && w->pWep) {
                    EstSetEm(w->pWep, -1, 0, 0, 0x10, 0x1F, 0, w->x69F, w->pWep, 0);
                }
                w->x6B5 = 2;
            }
        }
        break;
    case 2:
        w->x4 = (u8) (Rnd() % 90) + 120;
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (em->xFF) {
            if (w->x524 < 3000.0f) {
                em->setFindPL();
                em->xFE++;
            } else if (em->flags_3C8 & 1) {
                em->setFindPL();
                em->xFE++;
            } else if (w->x5EC) {
                em->setFindPL();
                em->xFE++;
            }
        } else if (w->x524 < 3000.0f) {
            em->setFindPL();
            em->xFE++;
        } else if (w->x4) {
            w->x4--;
        } else {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x9B), 0, 5, 1, 0);
        em->xFE++;
    case 5:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            w->x696 = 1;
            if (em10AtkRtnCk(em, 0)) {
                if (em->xFD == 0x21) {
                    em->xFE = 2;
                }
            } else {
                em10WalkRtnSet(em);
            }
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
    if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        EmRoutineSet(em, 2, 9, 0, 0);
    }
}

static void em10_R1_Stay(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    int flag;
    int one;
    f32 a;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    if (em->xFE == 0 && fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 2.3561945f) {
        em->xFE = 2;
    }
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        if (w->pParasite || w->x58C) {
            em->setWeaponFall();
            if (w->pShield) {
                w->pShield->setFall(20.0f, 0);
                w->pShield = 0;
            }
        }
        w->x8 = 0x23;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.024543693f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        if (pSUB && (s16) pG->sub_life <= 0) {
            break;
        }
        one = 1;  // shared SImode constant: the routine kind below and the xFE reset of the wait re-entry
        if (pG->flags_64 & 0x2000000) {
            EmRoutineSet(em, one, 0, 0, 0);
            return;
        }
        if (em10GotoCk(em)) {
            break;
        }
        if (w->x8 == 0 || --w->x8 == 0) {
            if (!Ctrl12Ck(w->pCtrl12, 9) && !Ctrl12Ck(w->pCtrl12, 6) && w->x67C == 0) {
                if (em10ReturnCk(em)) {
                    break;
                }
                if (em10AtkRtnCk(em, 0)) {
                    break;
                }
                if (em->x3D0 == 0 || em->x3D0 == 2) {
                    if (em10StayCk(em)) {
                        if (EM_RTN(em, 1, 0x1B)) {
                            em->xFE = one;
                            w->x8 = 0x23;
                        }
                        break;
                    }
                    em10WalkRtnSet(em);
                    break;
                }
                if (w->x52C < em->x3CC || (em->flags_3C8 & 0x40)) {
                    em10WalkRtnSet(em);
                    break;
                }
            }
        }
        if ((w->flags & 0x20000000) && (!(w->flags & 1) || em->plDist2 > 225000000.0f || w->x530 > em->x3CC + 10000.0f)) {
            w->x6AC = Rnd() % 3;
            EmRoutineSet(em, 1, 0x11, 0, 0);
        } else {
            em10GotoPosCk(em);
        }
        break;
    case 2:
        m0 = PL_ARC_PTR(em->subArc, 0x18);
        m1 = PL_ARC_PTR(em->subArc, 0x19);
        flag = 1;
        if (w->x518 < 0.0f) {
            flag = 0x41;
        }
        if (w->pShield) {
            m0 = PL_ARC_PTR(em->subArc, 0x16C);
            m1 = PL_ARC_PTR(em->subArc, 0x16D);
            flag = 1;
            if (em->flags_3C8 & 0x1000000) {
                flag = 0x41;
            }
        }
        if (em->type == 0xA || em->type == 0xD) {
            m0 = PL_ARC_PTR(em->subArc, 0x116);
            m1 = PL_ARC_PTR(em->subArc, 0x117);
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        w->x18 = em->rot.y + PI;
        w->x4 = 60;
        em->xFE++;
    case 3:
        if (em->seFlags28B & 8) {
            a = Muku(&em->pos, &pPL->pos, w->x18, 0.09817477f);
            w->x18 += a;
            w->x18 = LIMIT_ANGLE(w->x18);
            em->rot.y += a;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        } else {
            em10AtkRtnCk(em, 0);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
    if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        EmRoutineSet(em, 2, 9, 0, 0);
    } else {
        em10CsawSignSe(em);
    }
}

static void em10_R1_RoofWait(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        w->x4 = 10;
        w->x8 = 0x23;
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        MotionMoveF(em, 0);
        if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 0.7853982f) {
            if ((s16) pG->pl_life > 0 && (!pSUB || (s16) pG->sub_life > 0)) {
                em10WalkRtnSet(em);
            }
        } else {
            em10AtkRtnCk(em, 0);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
    if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        EmRoutineSet(em, 2, 9, 0, 0);
    }
}

static void em10_R1_Guard(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    u32 mode;

    if (em->xFE == 0 && (Rnd() & 1)) {
        em->xFE = 2;
    }
    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        mode = Rnd() % 2;
        if ((Rnd() & 1) == 0) {
            a.x = 0.0f;
            a.y = 250.0f;
            a.z = -1000.0f;
            PSMTXMultVec(em->mat, &a, &a);
            PSMTXMultVec(em->mat, &b, &b);
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
                mode = 2;
            } else {
                a.x = 0.0f;
                a.y = 250.0f;
                a.z = 1000.0f;
                PSMTXMultVec(em->mat, &b, &b);
                if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
                    mode = 3;
                }
            }
        }
        switch (mode) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x78), 0, 30, 1, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x78), 0, 30, 0x41, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2A8), (int) PL_ARC_PTR(em->subArc, 0x2A9), 30, 1, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2A8), (int) PL_ARC_PTR(em->subArc, 0x2A9), 30, 0x41, 0);
            break;
        }
        w->x4 = 15;
        w->x8 = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
            break;
        }
        if (w->x4) {
            w->x4--;
            break;
        }
        if (em10HeadLockCk(em)) {
            if (++w->x8 > 10) {
                em->xFE = 2;
            }
        } else {
            w->x8 = 0;
        }
        break;
    case 2:
        if (Muku(&pPL->pos, &em->pos, pPL->rot.y, PI) > 0.0f) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x71), (int) PL_ARC_PTR(em->subArc, 0x72), 30, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x71), (int) PL_ARC_PTR(em->subArc, 0x72), 30, 0x41, 0);
        }
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
    if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        EmRoutineSet(em, 2, 9, 0, 0);
    }
}

static void em10_R1_DownWakeWait(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        w->x67A = Rnd() % 60 + 60;
        em->xFE++;
    case 1:
        w->flags |= 0x10;
        w->flags |= 0x1000000;
        em->setStatus(3);
        MotionMoveF(em, 0);
        if (em->plDist2 > 6250000.0f && w->pParasite && w->x6C5 != 1 && w->pParasite->ckAtkEnable()) {
            w->pParasite->setAtk(1);
        }
        if ((s16) w->x67A != 0) {
            w->x67A--;
        } else {
            EmRoutineSet(em, 1, 0x1F, 0, 0);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_DownWake(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    switch (em->xFE) {
    case 0:
        flag = (MOTION(em)->flags & 0x40) ? 0x41 : 1;
        if (w->flags & 0x20) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x6D), (int) PL_ARC_PTR(em->subArc, 0x6E), 15, flag, 0);
        } else if (em->type == 6) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xAA), (int) PL_ARC_PTR(em->subArc, 0xAD), 15, flag, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x6F), (int) PL_ARC_PTR(em->subArc, 0x70), 15, flag, 0);
        }
        if (!(w->flags & 0x80)) {
            em10CallVoiceSe2(em, w->se6D1, 8);
        }
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x18;
            w->flags |= 0x1000000;
            em->setStatus(3);
        } else {
            w->flags &= ~0x10;
            w->flags &= ~0x1000000;
        }
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        } else {
            if (em->plDist2 > 6250000.0f && w->pParasite && w->x6C5 != 1 && w->pParasite->ckAtkEnable()) {
                w->pParasite->setAtk(1);
            }
            if (em->seFlags28B & 2) {
                em10SetDmWaterEff(em, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Crash(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    int flag;

    switch (em->xFE) {
    case 0:
        flag = (Rnd() & 1) ? 0x41 : 1;
        if (em->xFF) {
            m0 = PL_ARC_PTR(em->subArc, 0x1D);
            m1 = PL_ARC_PTR(em->subArc, 0x1E);
        } else {
            m0 = PL_ARC_PTR(em->subArc, 0x1B);
            m1 = PL_ARC_PTR(em->subArc, 0x1C);
        }
        {
            // Rnd() before the call statement: the `addi r4, em, 0x1d8` is then computed after the
            // call and regmove ties it to r4 (issued after `mr r3`); m0 declared before m1 for r30/r29.
            int r = Rnd() % 5;
            MotionSetCore(em, MOTION(em), m0, (int) m1, 6, flag, r);
        }
        em->xFE++;
    case 1:
        w->flags |= 0x2000;
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
    w->flags |= 0x2000;
    em10SetCrash(em, 500.0f);
}

static void em10_R1_ClimbOver(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx m;
    Vec v;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1F), (int) PL_ARC_PTR(em->subArc, 0x20), 10, 1, 0);
        PSVECSubtract(&w->x5E0, &em->pos, &w->x5E0);
        PSMTXRotRad(m, 'y', em->rot.y);
        PSMTXInverse(m, m);
        PSMTXMultVecSR(m, &w->x5E0, &w->x5E0);
        w->x5E0.y = 0.0f;
        em->xFE++;
    case 1:
        PSVECScale(&w->x5E0, &v, 0.2f);
        PSVECSubtract(&w->x5E0, &v, &w->x5E0);
        PSMTXRotRad(m, 'y', em->rot.y);
        PSMTXMultVecSR(m, &v, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        if (em->seFlags28B & 4) {
            w->flags |= 0x20000;
            em->setStatus(3);
        }
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
            w->x6B9 = 0;
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_DoorAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    int flag;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        m0 = PL_ARC_PTR(em->subArc, 0x79);
        m1 = PL_ARC_PTR(em->subArc, 0x7A);
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        if (w->pWep && w->wepType == 4) {
            m0 = PL_ARC_PTR(em->subArc, 0xE9);
            m1 = PL_ARC_PTR(em->subArc, 0xEA);
            SndCall(6, 0x50, &em->pos, 0, 0, em);
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x697 = 0;
        em->xFE++;
    case 1:
        em->atari.flags |= 8;
        w->x6BC = 2;
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        } else if ((em->seFlags28B & 0x20) || (em->seFlags28B & 1)) {
            if (em->seFlags28B & 1) {
                if (w->wepType == 4) {
                    em10SetDamageDoor(em, 2);
                    em10SetDamageRack(em, 2);
                    break;
                }
                if (em10SetDamageDoor(em, 1) != 1) {
                    em10SetDamageRack(em, 2);
                    break;
                }
                em10SetDamageRack(em, 0);
            } else {
                em10SetDamageDoor(em, 0);
                em10SetDamageRack(em, 0);
            }
        } else if (em->seFlags28B & 4) {
            int ok = em10AtkDoorCk(em) == 0;
            if (em10AtkRackCk(em)) {
                ok = 0;
            }
            if (ok) {
                em->xFE++;
            }
        }
        break;
    case 2:
        m0 = PL_ARC_PTR(em->subArc, 0x79);
        m1 = PL_ARC_PTR(em->subArc, 0x7B);
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_RackAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    int flag;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        m0 = PL_ARC_PTR(em->subArc, 0x7C);
        m1 = PL_ARC_PTR(em->subArc, 0x7D);
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        em10CallVoiceSe2(em, w->se6D2, 8);
        if (w->pWep && w->wepType == 4) {
            m0 = PL_ARC_PTR(em->subArc, 0xE9);
            m1 = PL_ARC_PTR(em->subArc, 0xEA);
            SndCall(6, 0x50, &em->pos, 0, 0, em);
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        w->x697 = 0;
        em->xFE++;
    case 1:
        em->atari.flags |= 8;
        w->x6BC = 2;
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        } else if (em->seFlags28B & 1) {
            em10SetDamageDoor(em, 2);
            em10SetDamageRack(em, 2);
        }
        break;
    case 2:
        m0 = PL_ARC_PTR(em->subArc, 0x79);
        m1 = PL_ARC_PTR(em->subArc, 0x7B);
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_WindowAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    int flag;
    int ok;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        m0 = PL_ARC_PTR(em->subArc, 0x79);
        m1 = PL_ARC_PTR(em->subArc, 0x7A);
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        if (w->pWep && w->wepType == 4) {
            m0 = PL_ARC_PTR(em->subArc, 0xE9);
            m1 = PL_ARC_PTR(em->subArc, 0xEA);
            SndCall(6, 0x50, &em->pos, 0, 0, em);
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x697 = 0;
        em->xFE++;
    case 1:
        em->atari.flags |= 8;
        w->x6BC = 2;
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        } else if ((em->seFlags28B & 0x20) || (em->seFlags28B & 1)) {
            if (em->seFlags28B & 1) {
                ok = 0;
                if (EM10_WINDOW(w) && EM10_WINDOW(w)->hp > 0) {
                    if (EM10_WINDOW(w)->hp <= 1 || w->wepType == 4) {
                        EM10_WINDOW(w)->SetBreakAll(&em->pos, 0, 0);
                        w->pWindow = 0;
                        ok = 1;
                    } else {
                        EM10_WINDOW(w)->SetShake();
                    }
                }
                if (w->wepType == 4 || ok) {
                    em10SetDamageDoor(em, 2);
                    em10SetDamageRack(em, 2);
                } else {
                    em10SetDamageRack(em, 0);
                }
            } else {
                if (EM10_WINDOW(w) && EM10_WINDOW(w)->hp > 0) {
                    EM10_WINDOW(w)->SetShake();
                }
                em10SetDamageDoor(em, 0);
                em10SetDamageRack(em, 0);
            }
        } else if (em->seFlags28B & 4) {
            ok = 1;
            if (EM10_WINDOW(w) && EM10_WINDOW(w)->hp > 0) {
                ok = 0;
            }
            if (em10AtkDoorCk(em)) {
                ok = 0;
            }
            if (em10AtkRackCk(em)) {
                ok = 0;
            }
            if (ok) {
                em->xFE++;
            }
        }
        break;
    case 2:
        m0 = PL_ARC_PTR(em->subArc, 0x79);
        m1 = PL_ARC_PTR(em->subArc, 0x7B);
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_LadderClimb(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec tmp;
    Vec spd;
    Vec rot;
    Mtx mat;
    Vec v;
    cModel* p;
    int flag;
    int st;
    f32 d2;
    f32 fl;

    switch (em->xFE) {
    case 0:
        PSMTXRotRad(mat, 'y', w->pLadder->rot.y);
        TransMatrix(mat, &w->pLadder->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 300.0f;
        PSMTXMultVec(mat, &v, &v);
        PSVECSubtract(&v, &em->pos, &w->x5E0);
        em->rot.y = w->pLadder->rot.y + PI;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em->xFF = Rnd() % 2;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xBC), (int) PL_ARC_PTR(em->subArc, 0xBD), 10, em->xFF ? 0 : 0x40, 0);
        w->x4 = w->pLadder->getLadderNum();
        w->x24 = em->pos;
        em->xFE++;
    case 1:
        w->flags |= 0x10000;
        em->setStatus(3);
        em->pos.x = w->x24.x;
        em->pos.z = w->x24.z;
        PSVECScale(&w->x5E0, &tmp, 0.3f);
        PSVECAdd(&em->pos, &tmp, &em->pos);
        PSVECSubtract(&w->x5E0, &tmp, &w->x5E0);
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        PSVECScale(&spd, &spd, 1.0f / em->scale.x);
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        if (MotionMoveF(em, 0)) {
            w->x4 -= 4;
            if (w->x4 <= 0) {
                em->xFE = 4;
            } else {
                em->xFE++;
            }
        } else {
            w->x24 = em->pos;
        }
        break;
    case 2:
        flag = 0x44;
        if (em->xFF) {
            flag = 4;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xBE), (int) PL_ARC_PTR(em->subArc, 0xBF), 10, flag, 0);
        em->xFE++;
    case 3:
        w->flags |= 0x10000;
        em->setStatus(3);
        em->pos.x = w->x24.x;
        em->pos.z = w->x24.z;
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        PSVECScale(&spd, &spd, 1.0f / em->scale.x);
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        if (MotionMoveF(em, 0)) {
            w->x4 -= 2;
            if (w->x4 <= 0) {
                em->xFE = 4;
            }
        } else {
            if (pG->flags_5010 & 0x40000) {
                p = pPL->getPartsPtr(4);
                v = p->worldPos;
                d2 = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.y - v.y) * (em->pos.y - v.y) + (em->pos.z - v.z) * (em->pos.z - v.z);
                if (d2 < 90000.0f) {
                    em->x328 = pPL->pos;
                    EmRoutineSet(em, 2, 6, 0, 0);
                    break;
                }
            }
            w->x24 = em->pos;
        }
        break;
    case 4:
        flag = em->xFF ? 0 : 0x40;
        if (w->pLadder->getType() == 1) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xC2), (int) PL_ARC_PTR(em->subArc, 0xC3), 10, flag, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xC0), (int) PL_ARC_PTR(em->subArc, 0xC1), 10, flag, 0);
        }
        em->xFE++;
    case 5:
        if (!(em->seFlags28B & 4)) {
            w->flags |= 0x10000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 8) {
            fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
            }
        }
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        PSVECScale(&spd, &spd, 1.0f / em->scale.x);
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        } else if ((pG->flags_5010 & 0x40000) && !(em->seFlags28B & 4)) {
            p = pPL->getPartsPtr(4);
            v = p->worldPos;
            d2 = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.y - v.y) * (em->pos.y - v.y) + (em->pos.z - v.z) * (em->pos.z - v.z);
            if (d2 < 90000.0f) {
                em->x328 = pPL->pos;
                EmRoutineSet(em, 2, 6, 0, 0);
            }
        }
        break;
    }
    st = w->pLadder->getStatus();
    if ((w->flags & 0x10000) && st == 3) {
        em->x328 = pPL->pos;
        EmRoutineSet(em, 2, 6, 0, 0);
        return;
    }
    em10HandSet(em, 0);
    if (em->xFF) {
        if (em->seFlags28B & 1) {
            SndCall(6, 0x46, &em->pos, 0, 0, em);
        }
        if (em->seFlags28B & 2) {
            SndCall(6, 0x45, &em->pos, 0, 0, em);
        }
    } else {
        if (em->seFlags28B & 1) {
            SndCall(6, 0x45, &em->pos, 0, 0, em);
        }
        if (em->seFlags28B & 2) {
            SndCall(6, 0x46, &em->pos, 0, 0, em);
        }
    }
}

static void em10_R1_VLadderClimb(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec tmp;
    Vec spd;
    Vec rot;
    Mtx mat;
    Vec v;
    cModel* p;
    int flag;
    f32 d2;

    switch (em->xFE) {
    case 0:
        PSMTXRotRad(mat, 'y', w->x5DC + PI);
        TransMatrix(mat, &w->x5E0);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 100.0f;
        PSMTXMultVec(mat, &v, &v);
        PSVECSubtract(&v, &em->pos, &w->x5E0);
        w->x4 = em->xFF;
        em->xFF = Rnd() % 2;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xC4), (int) PL_ARC_PTR(em->subArc, 0xC5), 10, em->xFF ? 0 : 0x40, 0);
        em->xFE++;
    case 1:
        w->flags |= 0x10000;
        em->setStatus(3);
        PSVECScale(&w->x5E0, &tmp, 0.3f);
        PSVECAdd(&em->pos, &tmp, &em->pos);
        PSVECSubtract(&w->x5E0, &tmp, &w->x5E0);
        em->rot.y += Muku2(em->rot.y, w->x5DC, 0.3926991f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        PSVECScale(&spd, &spd, 1.0f / em->scale.x);
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        if (MotionMoveF(em, 0)) {
            w->x4 -= 2;
            if (w->x4 <= 0) {
                em->xFE = 4;
            } else {
                em->xFE++;
            }
        }
        break;
    case 2:
        flag = 0x44;
        if (em->xFF) {
            flag = 4;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xC6), (int) PL_ARC_PTR(em->subArc, 0xC7), 10, flag, 0);
        em->xFE++;
    case 3:
        w->flags |= 0x10000;
        em->setStatus(3);
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        PSVECScale(&spd, &spd, 1.0f / em->scale.x);
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        if (MotionMoveF(em, 0)) {
            w->x4 -= 1;
            if (w->x4 <= 0) {
                em->xFE = 4;
            }
        } else if (pG->flags_5010 & 0x40000) {
            p = pPL->getPartsPtr(4);
            v = p->worldPos;
            d2 = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.y - v.y) * (em->pos.y - v.y) + (em->pos.z - v.z) * (em->pos.z - v.z);
            if (d2 < 90000.0f) {
                em->x328 = pPL->pos;
                EmRoutineSet(em, 2, 6, 0, 0);
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xC8), (int) PL_ARC_PTR(em->subArc, 0xC9), 10, em->xFF ? 0 : 0x40, 0);
        em->xFE++;
    case 5:
        if (!(em->seFlags28B & 4)) {
            w->flags |= 0x10000;
            em->setStatus(3);
        }
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        PSVECScale(&spd, &spd, 1.0f / em->scale.x);
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        } else if ((pG->flags_5010 & 0x40000) && !(em->seFlags28B & 4)) {
            p = pPL->getPartsPtr(4);
            v = p->worldPos;
            d2 = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.y - v.y) * (em->pos.y - v.y) + (em->pos.z - v.z) * (em->pos.z - v.z);
            if (d2 < 90000.0f) {
                em->x328 = pPL->pos;
                EmRoutineSet(em, 2, 6, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    if (em->xFF) {
        if (em->seFlags28B & 1) {
            SndCall(6, 0x63, &em->pos, 0, 0, em);
        }
        if (em->seFlags28B & 2) {
            SndCall(6, 0x62, &em->pos, 0, 0, em);
        }
    } else {
        if (em->seFlags28B & 1) {
            SndCall(6, 0x62, &em->pos, 0, 0, em);
        }
        if (em->seFlags28B & 2) {
            SndCall(6, 0x63, &em->pos, 0, 0, em);
        }
    }
}

static void em10_R1_LadderReset(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    Mtx m;
    Vec ofs;
    int flag;

    switch (em->xFE) {
    case 0:
        PSMTXRotRad(m, 'y', w->pLadder->rot.y);
        TransMatrix(m, &w->pLadder->pos);
        if (Muku2(w->pLadder->rot.y, em->rot.y, PI) > 0.0f) {
            ofs.x = -615.27f;
            ofs.y = 0.0f;
            ofs.z = 1432.8f;
            flag = 1;
            w->x5DC = w->pLadder->rot.y + 1.5707964f;
        } else {
            ofs.x = 615.27f;
            ofs.y = 0.0f;
            ofs.z = 1432.8f;
            flag = 0x41;
            w->x5DC = w->pLadder->rot.y - 1.5707964f;
        }
        w->x5DC = Muku2(em->rot.y, w->x5DC, PI);
        PSMTXMultVec(m, &ofs, &ofs);
        PSVECSubtract(&ofs, &em->pos, &w->x5E0);
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xBA), 0, 10, flag, 0);
        w->x4 = 0x1F;
        w->x8 = 0x71;
        em->xFE++;
    case 1:
        PSVECScale(&w->x5E0, &v, 0.2f);
        PSVECAdd(&em->pos, &v, &em->pos);
        PSVECSubtract(&w->x5E0, &v, &w->x5E0);
        {
            f32 d = w->x5DC * 0.2f;
            em->rot.y += d;
            w->x5DC -= d;
        }
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (w->x4) {
            w->x4--;
            if (w->x4 == 0) {
                w->pLadder->setReset(1);
            }
        }
        if (w->x8) {
            w->x8--;
            if (w->x4 == 0) {
                w->flags |= 0x200000;
            }
        } else if (w->pLadder) {
            w->pLadder = 0;
        }
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

// Water entry effect of a falling Ganado (once per fall, x20 flags it; em10_R1_JumpDown).
#define EM10_FALL_WATER_EFFECT                                                                         \
    w->x20 = 1;                                                                                        \
    if (pG->room_id == 0x311) {                                                                        \
        EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);                                                  \
        SndCall(6, 0xA, &em->pos, 0, 0, em);                                                           \
    } else {                                                                                           \
        EstSetEm10WaterFall((Vec*) em);                                                                \
        SndCall(6, 0x16, &em->pos, 0, 0, em);                                                          \
    }

// Landing of the jump down: snap to the floor, landing sound, landing motion (em10_R1_JumpDown).
#define EM10_JUMP_DOWN_LAND                                                                            \
    em->pos.y = fl;                                                                                    \
    w->x5A4.y = 0.0f;                                                                                  \
    SndCall(8, 5, &em->pos, em->id, 0, em);                                                            \
    MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x25), 0, 3, 1, 0);                           \
    MotionMoveF(em, 0);                                                                                \
    em->xFE = 4;

static void em10_R1_JumpDown(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec tmp;
    int end;
    f32 fl;

    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x23), (int) PL_ARC_PTR(em->subArc, 0x24), 3, 1, 0);
            PSVECSubtract(&w->x5E0, &em->pos, &w->x5E0);
            w->x5E0.y = 0.0f;
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x21), (int) PL_ARC_PTR(em->subArc, 0x22), 3, 1, 0);
            w->x5E0.x = 0.0f;
            w->x5E0.y = 0.0f;
            w->x5E0.z = 0.0f;
        }
        w->x20 = 0;
        em10CallVoiceSe2(em, w->se6D8, 8);
        em->xFE++;
    case 1:
        PSVECScale(&w->x5E0, &tmp, 0.2f);
        PSVECAdd(&em->pos, &tmp, &em->pos);
        PSVECSubtract(&w->x5E0, &tmp, &w->x5E0);
        w->flags |= 0x10080000;
        em->setStatus(3);
        end = MotionMoveF(em, 0);
        if (w->x20 == 0) {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                EM10_FALL_WATER_EFFECT;
            }
        }
        if (em->seFlags28B & 0x40) {
            break;
        }
        tmp = em->pos;
        tmp.y = em->oldPos.y;
        fl = SatMgr.getFloor(&tmp, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            EM10_JUMP_DOWN_LAND;
        } else if (end) {
            em->xFE++;
        }
        break;
    case 2:
        w->x5A4.x = 0.0f;
        w->x5A4.y = -400.0f;
        w->x5A4.z = 0.0f;
        em->xFE++;
    case 3:
        w->flags |= 0x10080000;
        em->setStatus(3);
        PSVECAdd(&em->pos, &w->x5A4, &em->pos);
        w->x5A4.y -= 20.0f;
        if (w->x20 == 0) {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                EM10_FALL_WATER_EFFECT;
            }
        }
        MotionMoveF(em, 0);
        tmp = em->pos;
        tmp.y = em->oldPos.y;
        fl = SatMgr.getFloor(&tmp, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            EM10_JUMP_DOWN_LAND;
        }
        break;
    case 4:
        if (CheckInWater(em, 0)) {
            em10FallWaterCk(em);
            if (w->x20 == 0) {
                EM10_FALL_WATER_EFFECT;
            }
        } else if (ChkWaterEffectEnable(&em->pos)) {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x31, 0, 0, (u32) em, 0);
        } else {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x17, 0, 0, (u32) em, 0);
        }
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 800.0f);
}
#undef EM10_FALL_WATER_EFFECT
#undef EM10_JUMP_DOWN_LAND

// Water entry effect of a falling Ganado (once per fall, x20 flags it; em10_R1_JumpDown / Jump).
#define EM10_FALL_WATER_EFFECT                                                                         \
    w->x20 = 1;                                                                                        \
    if (pG->room_id == 0x311) {                                                                        \
        EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);                                                  \
        SndCall(6, 0xA, &em->pos, 0, 0, em);                                                           \
    } else {                                                                                           \
        EstSetEm10WaterFall((Vec*) em);                                                                \
        SndCall(6, 0x16, &em->pos, 0, 0, em);                                                          \
    }

// Landing of the jump: snap to the floor, landing sound, landing motion (em10_R1_JumpDown / Jump).
#define EM10_JUMP_DOWN_LAND                                                                            \
    em->pos.y = fl;                                                                                    \
    w->x5A4.y = 0.0f;                                                                                  \
    SndCall(8, 5, &em->pos, em->id, 0, em);                                                            \
    MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x25), 0, 3, 1, 0);                           \
    MotionMoveF(em, 0);                                                                                \
    em->xFE = 4;

static void em10_R1_Jump(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx mat;
    Vec tmp;
    Vec v;
    Vec spd;
    Vec rot;
    f32 fl;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x8C), (int) PL_ARC_PTR(em->subArc, 0x8D), 10, 0, 0);
        PSMTXRotRad(mat, 'y', em->rot.y);
        TransMatrix(mat, &em->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 4000.0f;
        PSMTXMultVec(mat, &v, &v);
        fl = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
        w->x18 = fl - em->pos.y;
        if (fl > 1000.0f) {
            w->x18 = 0.0f;
        }
        if (fl < 0.0f) {
            w->x18 = 0.0f;
        }
        w->x20 = 0;
        em10CallVoiceSe2(em, w->se6D8, 8);
        em->xFE++;
    case 1:
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        if (em->seFlags28B & 4) {
            w->flags |= 0x00181000;
            fl = w->x18 * 0.1f;
            em->pos.y += fl;
            w->x18 -= fl;
            PSVECScale(&spd, &spd, 1.0f / em->scale.x);
        }
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        em->setStatus(3);
        if (w->x20 == 0) {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                EM10_FALL_WATER_EFFECT;
            }
        }
        if (em->seFlags28B & 1) {
            tmp = em->pos;
            tmp.y = em->oldPos.y;
            fl = SatMgr.getFloor(&tmp, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                EM10_JUMP_DOWN_LAND;
                break;
            }
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        w->x5A4.x = 0.0f;
        w->x5A4.y = -400.0f;
        w->x5A4.z = 0.0f;
        em->xFE++;
    case 3:
        w->flags |= 0x10080000;
        em->setStatus(3);
        PSVECAdd(&em->pos, &w->x5A4, &em->pos);
        w->x5A4.y -= 20.0f;
        if (w->x20 == 0) {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                EM10_FALL_WATER_EFFECT;
            }
        }
        MotionMoveF(em, 0);
        tmp = em->pos;
        tmp.y = em->oldPos.y;
        fl = SatMgr.getFloor(&tmp, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            EM10_JUMP_DOWN_LAND;
        }
        break;
    case 4:
        if (CheckInWater(em, 0)) {
            em10FallWaterCk(em);
            if (w->x20 == 0) {
                EM10_FALL_WATER_EFFECT;
            }
        } else if (ChkWaterEffectEnable(&em->pos)) {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x31, 0, 0, (u32) em, 0);
        } else {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x17, 0, 0, (u32) em, 0);
        }
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 800.0f);
}
#undef EM10_FALL_WATER_EFFECT
#undef EM10_JUMP_DOWN_LAND

static void em10_R1_JumpUp(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx mat;
    Vec a;
    Vec b;
    Vec tmp;
    f32 f;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1A0), (int) PL_ARC_PTR(em->subArc, 0x1A1), 10, 1, 0);
        f = w->x5E0.y - em->pos.y + 500.0f;
        w->x18 = f;
        w->x18 *= 0.010989011f;
        w->x5A4.x = 0.0f;
        w->x5A4.y = w->x18 * 13.0f;
        w->x5A4.z = 0.0f;
        em->rot.y = w->x5DC;
        PSMTXRotRad(mat, 'y', w->x5DC);
        TransMatrix(mat, &w->x5E0);
        ScaleMatrix(mat, &em->scale);
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 1448.59f;
        PSMTXMultVec(mat, &a, &a);
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        b.x = 0.0f;
        b.y = 0.0f;
        b.z = 1448.59f;
        PSMTXMultVec(em->mat, &b, &b);
        PSVECSubtract(&a, &b, &w->x24);
        w->x24.y = 0.0f;
        em10CallVoiceSe2(em, w->se6D7, 8);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x40) {
            w->flags |= 0x00180000;
            if (em->seFlags28B & 4) {
                PSVECAdd(&em->pos, &w->x5A4, &em->pos);
                w->x5A4.y -= 33.333332f;
            } else {
                em->dmType = 2;
                PSVECAdd(&em->pos, &w->x5A4, &em->pos);
                w->x5A4.y -= w->x18;
            }
            PSVECScale(&w->x24, &tmp, 0.1f);
            PSVECAdd(&em->pos, &tmp, &em->pos);
            PSVECSubtract(&w->x24, &tmp, &w->x24);
        }
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 800.0f);
}

static void em10_R1_Trade(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xA5), 0, 10, 1, 0);
        KeyStop(0xEFCF0000);
        w->x5B8 = SndCall(8, 0x84, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        em->dmg.x1 = 2;
        if (MOTION(em)->seqFrame > 35.7f && MOTION(em)->seqFrame < 36.3f) {
            em10ClothPartsSet(em, 1);
            SndCall(8, 0x86, &em->pos, em->id, 0, em);
        }
        if (MOTION(em)->seqFrame > 40.7f && MOTION(em)->seqFrame < 41.3f) {
            em10GoodsPartsSet(em, 1);
        }
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        if (SubScreenOpen(0x10, 0)) {
            em->xFE++;
        }
        break;
    case 3:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xA7), 0, 10, 1, 0);
        SndCall(8, 0x86, &em->pos, em->id, 0, em);
        w->x5B8 = SndCall(8, 0x9A, &em->pos, em->id, 0, em);
        pGS->flags_170 &= 0x7FFFFFFF; // load stays below the x5B8 store
        em->xFE++;
    case 4:
        if (MOTION(em)->seqFrame > 33.7f && MOTION(em)->seqFrame < 34.3f) {
            em10ClothPartsSet(em, 0);
        }
        if (MOTION(em)->seqFrame > 26.7f && MOTION(em)->seqFrame < 27.3f) {
            em10GoodsPartsSet(em, 0);
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Drive(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModel* truck;

    em->atari.flags &= 0xFCFF;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1BA), 0, 0, 5, 0);
        w->pTruck = 0;
        w->scaleBase.x = 1.0f;
        w->scaleBase.y = 1.0f;
        w->scaleBase.z = 1.0f;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em10SearchTruck(em)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1BA), 0, 0, 5, 0);
        em->xFE++;
    case 3:
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = 0.0f;
        em->rot.x = 0.0f;
        em->rot.y = 0.0f;
        em->rot.z = 0.0f;
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        PSMTXConcat(w->pTruck->getPartsPtr(0)->mat, em->mat, em->mat);
        MOTION(em)->flags2 |= 0x40000000;
        MotionMoveF(em, 0);
        if (em->hp <= 0 || (w->pTruck->flags_3C8 & 2)) {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1BB), 0, 3, 1, 0);
        em->hp = 0;
        em->clearStatus(5);
        em->xFE++;
    case 5:
        em->pos.x = 0.0f;
        em->pos.y = -100.0f;
        em->pos.z = 0.0f;
        em->rot.x = 0.0f;
        em->rot.y = 0.0f;
        em->rot.z = 0.0f;
        truck = w->pTruck->getPartsPtr(0);
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        PSMTXConcat(truck->mat, em->mat, em->mat);
        MOTION(em)->flags2 |= 0x40000000;
        MotionMoveF(em, 0);
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Catapult(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 n;
    u32 r;

    w->flags |= 0x8000;
    switch (em->xFE) {
    case 0:
        n = *(u16*) PL_ARC_PTR(em->subArc, 5) & 0x3FFF;
        r = Rnd() % n;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 5), 0, 10, 5, (u16) r);
        em->hp = 1;
        em->xFE++;
    case 1:
        w->flags |= 0x40000;
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            em->xFE++;
        }
        break;
    case 2:
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    if (!em10GotoCk(em)) {
        em10HandSet(em, 0);
    }
}

static void em10_R1_RockPush(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->flags |= 0x8000;
    em->atari.throughOn();
    em->dmg.x1 = 2;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 5), 0, 0, 5, 0);
        em->be_flag &= ~2;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            em->be_flag |= 2;
            em->xFE++;
        }
        break;
    case 2:
        w->x4 = 150;
        em->xFE++;
    case 3:
        if (w->x4) {
            w->x4--;
        } else {
            em->alpha = 0.0f;
            em->be_flag &= ~2;
            w->flags |= 0x400000;
        }
        if (MotionMoveF(em, 0)) {
            em->hp = 0;
            em->alpha = 0.0f;
            em->be_flag &= ~2;
            w->flags |= 0x400000;
            em->xFE++;
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_ParasiteAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int r;
    int flag;

    switch (em->xFE) {
    case 0:
        flag = 0;
        if (w->x58C) {
            f32 d;
            flag = 0x1E;
            if (w->x58C->vC8() && pSUB) {
                d = w->x514;
            } else {
                d = em->plDist2;
            }
            if (d < 2560000.0f) {
                w->x58C->v68();
            } else {
                w->x58C->v78();
                r = Rnd();
                w->x648 = r % 300 + 300;
            }
        }
        if (w->pParasite) {
            if (w->x6C5 != 1) {
                flag = 0x1E;
                if (w->flags & 0x8000000) {
                    w->pParasite->setAtk(1);
                } else {
                    w->pParasite->setAtk(0);
                }
            } else {
                w->pParasite->setCritical();
            }
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x28E), 0, 10, 1, flag);
        w->x697 = 0;
        w->x4 = 120;
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        em->xFE++;
    case 1:
        if (w->pParasite && w->pParasite->ckAtkHit()) {
            w->x697 = 1;
        }
        if (w->x58C && w->x58C->v70()) {
            w->x697 = 1;
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697) {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
            }
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

// Bowgun aim: vertical blend rate from the angle between the bowgun hand and the target, in
// [-255, 255] (em10_R1_ShotBowgun).
#define EM10_BOWGUN_AIM_RATE                                                                           \
    PSVECSubtract(&tgt, &em->getPartsPtr(3)->worldPos, &d);                                            \
    len = SQRTF(d.x * d.x + d.z * d.z);                                                                \
    rate = -atan2f(d.y, len) * 325.9493f;                                                              \
    if (rate > 255.0f) {                                                                               \
        rate = 255.0f;                                                                                 \
    }                                                                                                  \
    if (rate < -255.0f) {                                                                              \
        rate = -255.0f;                                                                                \
    }

static void em10_R1_ShotBowgun(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec d;
    Vec tgt;
    Vec ofs;
    Vec spd;
    cEmWep* wep;
    f32 rate;
    f32 len;
    int flag;

    w->flags |= 0x200;
    if (w->flags & 1) {
        tgt = pPL->getPartsPtr(2)->worldPos;
    } else {
        tgt = pPL->pos;
        tgt.y += 1300.0f;
    }
    switch (em->xFE) {
    case 0:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x12C), (int) PL_ARC_PTR(em->subArc, 0x12D), 10, flag, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.3926991f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0) || (em->seFlags28B & 4)) {
            if (w->x5EC != 0 || !w->pWep) {
                em->xFE = 8;
            } else if (w->x6B5) {
                em->xFE++;
            } else {
                em->xFE = 6;
            }
        }
        break;
    case 2:
        EM10_BOWGUN_AIM_RATE;
        w->x740 = 10;
        w->x744 = 0;
        w->blendRate = rate;
        {
            u8 r = Rnd() % 30;
            w->x4 = r + 30;
        }
        em->xFE++;
    case 3:
        U16Set(w->x670, 2);
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        EM10_BOWGUN_AIM_RATE;
        w->blendRate = w->blendRate * 0.9f + rate * 0.1f;
        flag = (em->flags_3C8 & 0x1000000) ? 0x45 : 5;
        em10BlendMotSet(em, PL_ARC_PTR(em->subArc, 0x12E), PL_ARC_PTR(em->subArc, 0x133), PL_ARC_PTR(em->subArc, 0x136), 0, 0, 0, flag);
        MotionMoveF(em, 0);
        if (w->x5EC != 0) {
            em->xFE = 8;
            break;
        }
        if (em10ThrowNearCk(em)) {
            em->xFE = 0;
            em->xFC = 1;
            em->xFD = 0x17;
            em->xFF = Rnd() & 1;
            break;
        }
        if (!em10ThrowScaCk(em) || !w->pWep) {
            em->xFE = 8;
            break;
        }
        if (w->x4) {
            w->x4--;
            break;
        }
        if (!(em->flags_3C8 & 1)) {
            if (Ctrl12Ck(w->pCtrl12, 8)) {
                break;
            }
            if (em10DeadCk(pPL)) {
                break;
            }
            if (pG->x4F88 <= 4) {
                if (!em10ScreenInCk(em)) {
                    break;
                }
            }
            if (pG->x4F88 <= 3) {
                u8 r = Rnd() % 10;
                if (r > 4) {
                    u8 r2 = Rnd() % 30;
                    w->x4 = r2 + 30;
                    break;
                }
            }
        }
        if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) < 0.19634955f) {
            em->flags_3C8 &= ~1;
            em->xFE++;
        } else {
            em->xFE = 8;
        }
        break;
    case 4:
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x4 = 1;
        if (pG->x4F88 <= 3) {
            w->x4 = 0;
        }
        w->x740 = 0;
        w->x744 = 0;
        em->xFE++;
    case 5:
        EM10_BOWGUN_AIM_RATE;
        w->blendRate = w->blendRate * 0.9f + rate * 0.1f;
        flag = (em->flags_3C8 & 0x1000000) ? 0x45 : 5;
        em10BlendMotSet(em, PL_ARC_PTR(em->subArc, 0x12F), PL_ARC_PTR(em->subArc, 0x134), PL_ARC_PTR(em->subArc, 0x137),
                        (int) PL_ARC_PTR(em->subArc, 0x130), (int) PL_ARC_PTR(em->subArc, 0x135), (int) PL_ARC_PTR(em->subArc, 0x138), flag);
        if (em->seFlags28B & 4) {
            w->x670 = 2;
        }
        if (em->seFlags28B & 1) {
            if (em->type != 6) {
                w->x6B5--;
            }
            w->x670 = 10;
            w->x6C2 = 1;
            if (w->pWep && w->mot[75] && w->mot[76]) {
                ofs.x = 0.0f;
                ofs.y = 57.4f;
                ofs.z = 232.98f;
                PSMTXMultVec(w->pWep->mat, &ofs, &ofs);
                wep = SetWeapon(w->mot[75], w->mot[76], &ofs, &em->rot, 0);
                if (wep) {
                    spd.x = fRand1_1() * 50.0f;
                    spd.y = fRand0_1() * 50.0f;
                    spd.z = fRand1_1() * 50.0f + 1000.0f;
                    wep->be_flag |= 0x4000;
                    PSMTXMultVecSR(w->pWep->mat, &spd, &spd);
                    wep->setShot(&spd, &Em10AtkTbl[7]);
                    wep->setSeDamage(8, 0x3F, em->id);
                    wep->setSeHit(8, 0x81, em->id);
                    wep->setSeHitWall(8, 0x82, em->id);
                    wep->setSeThrow(8, 0x80, em->id, 0xFF);
                    wep->setEffDamage(0, 0x18);
                    wep->setEffHit(0x10, 0xB);
                    wep->setEffWater(1, 0x37);
                    wep->setEffAlways(0x10, 0x20);
                    SndCall(8, 0x7F, &em->pos, em->id, 0, em);
                }
                if (w->x6B5 == 0) {
                    EffectEspDelete(0, w->x69F, (u32) w->pWep, 0);
                    EffectEspgenDelete(0, w->x69F, (int) w->pWep);
                    EffectEfmDelete(0, w->x69F, (int) w->pWep);
                }
            }
        }
        if (MotionMoveF(em, 0)) {
            if (!em10ThrowScaCk(em) || !w->pWep) {
                em->xFE = 8;
            } else if (w->x6B5 == 0) {
                if ((Rnd() & 3) && em10HideRtnCk(em)) {
                    break;
                }
                if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) < 0.19634955f) {
                    em->xFE++;
                } else {
                    em->xFE = 8;
                }
            } else if (w->x4 == 0) {
                em->xFE = 2;
            } else {
                w->x4--;
                if (Ctrl12Ck(w->pCtrl12, 8) || em10DeadCk(pPL)) {
                    em->xFE = 2;
                }
            }
        }
        break;
    case 6:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x131), (int) PL_ARC_PTR(em->subArc, 0x132), 10, flag, 0);
        em->xFE++;
    case 7:
        if (em->seFlags28B & 1) {
            w->x6B5 = 2;
            if (w->wepType == 8 && w->pWep) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x1F, 0, w->x69F, (u32) w->pWep, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) < 0.19634955f) {
                em->xFE = 2;
            } else if (!em10HideRtnCk(em)) {
                em->xFE++;
            }
        }
        break;
    case 8:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x139), 0, 10, flag, 0);
        em->xFE++;
    case 9:
        if (MotionMoveF(em, 0)) {
            if (w->x5EC) {
                em10WalkRtnSet(em);
            } else if (!em10HideRtnCk(em)) {
                em10WalkRtnSet(em);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        EmRoutineSet(em, 2, 9, 0, 0);
    }
}
#undef EM10_BOWGUN_AIM_RATE

// Rocket launcher aim: same blend rate as the bowgun (em10_R1_ShotRocket).
#define EM10_ROCKET_AIM_RATE                                                                           \
    PSVECSubtract(&tgt, &em->getPartsPtr(3)->worldPos, &d);                                            \
    len = SQRTF(d.x * d.x + d.z * d.z);                                                                \
    rate = -atan2f(d.y, len) * 325.9493f;                                                              \
    if (rate > 255.0f) {                                                                               \
        rate = 255.0f;                                                                                 \
    }                                                                                                  \
    if (rate < -255.0f) {                                                                              \
        rate = -255.0f;                                                                                \
    }

static void em10_R1_ShotRocket(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec d;
    Vec tgt;
    Vec ofs;
    Vec spd;
    cEmWep* wep;
    cModel* p;
    f32 rate;
    f32 len;

    w->flags |= 0x200;
    if (w->flags & 1) {
        tgt = pPL->getPartsPtr(2)->worldPos;
    } else {
        tgt = pPL->pos;
        tgt.y += 1300.0f;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x181), (int) PL_ARC_PTR(em->subArc, 0x182), 10, 1, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 4) {
            w->flags |= 0x40000000;
        }
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->x5EC != 0) {
                em10WalkRtnSet(em);
            } else if (!w->pWep) {
                em->xFE = 6;
            } else {
                em->xFE++;
            }
        }
        break;
    case 2:
        EM10_ROCKET_AIM_RATE;
        w->x740 = 10;
        w->x744 = 0;
        w->blendRate = rate;
        {
            u8 r = Rnd() % 30;
            w->x4 = r + 30;
        }
        em->xFE++;
    case 3:
        w->x670 = 2;
        BitOn(w->flags, 0x40000000);
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        EM10_ROCKET_AIM_RATE;
        w->blendRate = w->blendRate * 0.9f + rate * 0.1f;
        em10BlendMotSet(em, PL_ARC_PTR(em->subArc, 0x183), PL_ARC_PTR(em->subArc, 0x186), PL_ARC_PTR(em->subArc, 0x188), 0, 0, 0, 5);
        MotionMoveF(em, 0);
        if (w->x5EC != 0) {
            em10WalkRtnSet(em);
            break;
        }
        if (!w->pWep) {
            em->xFE = 6;
            break;
        }
        if (!(em->flags_3C8 & 1)) {
            if (em10ThrowNearCk(em)) {
                em->xFE = 0;
                em->xFC = 1;
                em->xFD = 0x17;
                em->xFF = Rnd() & 1;
                break;
            }
            if (!em10ThrowScaCk(em)) {
                em10WalkRtnSet(em);
                break;
            }
        }
        if (w->x4) {
            w->x4--;
            break;
        }
        if (!(em->flags_3C8 & 1)) {
            if (Ctrl12Ck(w->pCtrl12, 8)) {
                break;
            }
            if (em10DeadCk(pPL)) {
                break;
            }
        }
        if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) < 0.19634955f) {
            em->flags_3C8 &= ~1;
            em->xFE++;
        } else {
            em10WalkRtnSet(em);
        }
        break;
    case 4:
        w->flags |= 0x40000000;
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x4 = 1;
        if (pGS->x4F88 <= 3) {
            w->x4 = 0;
        }
        w->x740 = 0;
        w->x744 = 0;
        ofs.x = 0.0f;
        ofs.y = 57.4f;
        ofs.z = 232.98f;
        PSMTXMultVec(w->pWep->mat, &ofs, &ofs);
        wep = SetWeapon(PL_ARC_PTR(pG->pPlArc, 0x70), PL_ARC_PTR(pG->pPlArc, 0x71), &ofs, &em->rot, 0);
        if (wep) {
            spd.x = -500.0f;
            spd.y = 0.0f;
            spd.z = 0.0f;
            wep->be_flag |= 0x4000;
            PSMTXMultVecSR(w->pWep->mat, &spd, &spd);
            wep->setRocket(em, &spd, &Em10AtkTbl[7]);
            wep->setSeDamage(8, 0x3F, em->id);
            wep->setSeHit(8, 0x81, em->id);
            wep->setSeHitWall(8, 0x82, em->id);
            wep->setSeThrow(8, 0xB2, em->id, 0xFF);
            wep->setEffDamage(0, 0x18);
            wep->setEffHit(0x10, 0xB);
            wep->setEffWater(1, 0x37);
            wep->setEffAlways(0x10, 0x9F);
            SndCall(8, 0xB1, &em->pos, em->id, 0, em);
            p = w->pWep->getPartsPtr(2);
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
        em->xFE++;
    case 5:
        EM10_ROCKET_AIM_RATE;
        w->blendRate = w->blendRate * 0.9f + rate * 0.1f;
        em10BlendMotSet(em, PL_ARC_PTR(em->subArc, 0x184), PL_ARC_PTR(em->subArc, 0x187), PL_ARC_PTR(em->subArc, 0x189),
                        (int) PL_ARC_PTR(em->subArc, 0x185), 0, 0, 1);
        if (MotionMoveF(em, 0)) {
            em->xFE = 6;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x18A), (int) PL_ARC_PTR(em->subArc, 0x18B), 10, 1, 0);
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        } else if (em->seFlags28B & 1) {
            em->setWeaponFall();
        }
        break;
    }
    em10HandSet(em, 0);
    if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        EmRoutineSet(em, 2, 9, 0, 0);
    }
}
#undef EM10_ROCKET_AIM_RATE

// Gatling aim limit per frame from the player distance (em10_R1_ShotGatling).
#define EM10_GATLING_TURN_LIMIT(k)                                                                     \
    len = SQRTF(em->plDist2);                                                                          \
    if (len < 5000.0f) {                                                                               \
        len = 5000.0f;                                                                                 \
    }                                                                                                  \
    rate = len * 0.0002f;                                                                              \
    lim = 1.0f / rate * k;

static void em10_R1_ShotGatling(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec d;
    Vec tgt;
    cModel* p;
    f32 ang;
    f32 lim;
    f32 rate;
    f32 len;
    f32 sv;

    tgt = pPL->pos;
    tgt.y += 1200.0f;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x198), (int) PL_ARC_PTR(em->subArc, 0x199), 10, 1, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        SndCall(8, 0xB3, &em->pos, em->id, 0, em);
        w->x4 = 15;
        w->blendRate = 0.0f;
        w->xC = 0;
        w->x10 = 0;
        em->xFE++;
    case 1:
        p = em->getPartsPtr(10);
        ang = em->rot.y;
        ang = LIMIT_ANGLE(ang + (Muku(&p->worldPos, &pPL->pos, ang, 3.1415927f) + -0.34906584f));
        em->rot.y += Muku2(em->rot.y, ang, 0.05235988f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->x5EC != 0) {
                em->xFE = 6;
            } else {
                em->xFE++;
            }
        }
        break;
    case 2:
        w->x740 = 10;
        w->x744 = 0;
        w->x8 = 50;
        em->xFE++;
    case 3:
        w->x6A6 = 1;
        w->x670 = 2;
        EM10_GATLING_TURN_LIMIT(0.034906585f);
        em->getPartsPtr(10);
        p = em->getPartsPtr(10);
        ang = em->rot.y;
        ang = LIMIT_ANGLE(ang + (Muku(&p->worldPos, &pPL->pos, ang, 3.1415927f) + -0.34906584f));
        em->rot.y += Muku2(em->rot.y, ang, lim);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        w->blendRate = w->blendRate * 0.95f + 6.4f;
        em->partsFixMemory(0x15);
        em10BlendMotSet(em, PL_ARC_PTR(em->subArc, 0x19A), PL_ARC_PTR(em->subArc, 0x19C), PL_ARC_PTR(em->subArc, 0x19E), 0, 0, 0, 1);
        MotionMoveF(em, 0);
        if (w->x5EC != 0) {
            em->xFE = 6;
            break;
        }
        if (w->x4) {
            w->x4--;
        } else {
            if (!(em->flags_3C8 & 1)) {
                if (em10DeadCk(pPL)) {
                    break;
                }
                if (pG->x4F88 <= 4) {
                    if (!em10ScreenInCk(em)) {
                        break;
                    }
                }
                if (pG->x4F88 <= 3) {
                    u8 r = Rnd() % 10;
                    if (r > 4) {
                        u8 r2 = Rnd() % 30;
                        w->x4 = r2 + 30;
                        break;
                    }
                }
            }
            em->flags_3C8 &= ~1;
            em->xFE++;
            break;
        }
        em10AxeAtkCk(em);
        break;
    case 4:
        w->x4 = 1;
        if (pGS->x4F88 <= 3) {
            w->x4 = 0;
        }
        w->x740 = 0;
        w->x744 = 0;
        w->x4 = 2;
        em->xFE++;
    case 5:
        w->x6A6 = 1;
        EM10_GATLING_TURN_LIMIT(0.02268928f);
        em->rot.y += Muku(&em->getPartsPtr(10)->worldPos, &pPL->pos, em->rot.y, lim);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        PSVECSubtract(&tgt, &em->getPartsPtr(0)->worldPos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        lim = -atan2f(d.y, len);
        rate = lim * 488.92398f;
        if (rate > 255.0f) {
            rate = 255.0f;
        }
        if (rate < -255.0f) {
            rate = -255.0f;
        }
        w->blendRate = w->blendRate * 0.95f + rate * 0.05f;
        em->partsFixMemory(0x15);
        em10BlendMotSet(em, PL_ARC_PTR(em->subArc, 0x19B), PL_ARC_PTR(em->subArc, 0x19D), PL_ARC_PTR(em->subArc, 0x19F), 0, 0, 0, 1);
        if (MotionMoveF(em, 0)) {
            if (!em10ThrowScaCk(em)) {
                em->xFE = 6;
                break;
            }
            if (EatMgr.hitCheck(&em->getPartsPtr(10)->worldPos, &tgt, 0, 0, 0, 0x404000)) {
                em->xFE = 6;
                break;
            }
            if (w->x508 > 0.7853982f) {
                em->xFE = 6;
                break;
            }
            PSVECSubtract(&pPL->pos, &em->pos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
            lim = -atan2f(d.y, len);
            lim = fabsf(lim);
            if (lim > 0.43633232f) {
                em->xFE = 6;
            } else {
                w->x4 = 60;
                em->xFE = 2;
            }
        } else {
            switch (w->x4) {
            case 2:
                sv = w->x5D4;
                em10WaistMove(em);
                em->partsWorldCalc();
                if (em10GatlingHitCk(em)) {
                    if (w->x8 > 8) {
                        w->x8 = 8;
                    }
                }
                w->x5D4 = sv;
            default:
                w->x4--;
                break;
            case 0:
                if (w->x508 > 2.0943952f) {
                    if (w->x8 > 5) {
                        w->x8 = 5;
                    }
                }
                if (w->x8) {
                    w->x8--;
                    em->xFE = 4;
                }
                break;
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x139), 0, 10, 1, 0);
        SndCall(8, 0xB4, &em->pos, em->id, 0, em);
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            if (w->x5EC) {
                em10WalkRtnSet(em);
            } else if (!em10HideRtnCk(em)) {
                em10WalkRtnSet(em);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        EmRoutineSet(em, 2, 9, 0, 0);
    }
}
#undef EM10_GATLING_TURN_LIMIT

static void em10_R1_ThrowAxe(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec spd;
    Vec d;
    Vec plPos;
    Vec wpos;
    f32 len;
    f32 t;
    f32 v;
    int flag;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        if (w->wepType != 6) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, flag, 0x10);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x13D), (int) PL_ARC_PTR(em->subArc, 0x13F), 10, flag, 0);
        }
        w->x4 = 10;
        w->x8 = 0;
        em10CallVoiceSe2(em, w->se6D2, 8);
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 4) {
            w->x670 = 2;
        }
        if (em->seFlags28B & 1) {
            if (w->pWep) {
                spd.x = fRand1_1() * 50.0f + 20.0f;
                spd.y = fRand1_1() * 10.0f + 75.0f;
                spd.z = fRand1_1() * 10.0f + 350.0f;
                if (pG->x4F88 <= 3) {
                    if ((u8) (Rnd() % 10) > 4) {
                        if ((u8) (Rnd() % 10) > 4) {
                            spd.x = 50.0f;
                        } else {
                            spd.x = -10.0f;
                        }
                    }
                }
                if (pG->x4F88 <= 1) {
                    if ((u8) (Rnd() % 10) > 1) {
                        if ((u8) (Rnd() % 10) > 4) {
                            spd.x = 50.0f;
                        } else {
                            spd.x = -10.0f;
                        }
                    }
                }
                PSVECSubtract(&pPL->pos, &em->pos, &d);
                len = SQRTF(d.x * d.x + d.z * d.z);
                t = -atan2(d.y, len);
                if (len < 3000.0f) {
                    if (len < 1500.0f) {
                        v = sinf(t) * -700.0f;
                    } else {
                        v = sinf(t) * -500.0f;
                    }
                } else {
                    v = sinf(t) * -400.0f;
                }
                spd.y += v;
                PSMTXMultVecSR(em->mat, &spd, &spd);
                if (w->wepType != 6) {
                    w->pWep->setThrow(&spd, 15.0f, &Em10AtkTbl[5]);
                } else {
                    plPos = pPL->pos;
                    plPos.y += 1500.0f;
                    wpos.x = w->pWep->mat[0][3];
                    wpos.y = w->pWep->mat[1][3];
                    wpos.z = w->pWep->mat[2][3];
                    PSVECSubtract(&plPos, &wpos, &spd);
#line 15148 "D:/Bio4/Prog/em10.cpp"
                    VECNormalize(&spd, &spd);
                    PSVECScale(&spd, &spd, 250.0f);
                    w->pWep->setThrowScythe(&spd, &Em10AtkTbl[6]);
                }
                w->pWep = 0;
                w->wepType = 0;
                w->x670 = 10;
            }
        }
        if (w->pWep) {
            if (GetEm10EyeEffectEnable()) {
                if (w->x8) {
                    w->x8--;
                } else {
                    w->x8 = 14;
                    switch (w->wepType) {
                    case 2:
                        EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x89, 0, 0, (u32) w->pWep, 0);
                        break;
                    case 3:
                        EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x8A, 0, 0, (u32) w->pWep, 0);
                        break;
                    }
                }
            }
        }
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_ThrowBomb(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        if (em->flags_3C8 & 0x1000000) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, 0x41, 0x10);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, 1, 0x10);
        }
        w->x4 = 10;
        em10CallVoiceSe2(em, w->se6D2, 8);
        if (w->x640 > 90) {
            w->x640 = 90;
        }
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4 -= 1;
            em->rot.y = em->rot.y + Muku(&em->pos, &pPLS->pos, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 4) {
            w->x670 = 2;
        }
        if ((em->seFlags28B & 1) && w->pWep) {
            em10BombThrow(em);
        }
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

// Bomb fuse lit: fuse effect + smoke on the held bomb, ignition sound (em10_R1_FixBomber).
#define EM10_BOMB_FIRE_EFFECT()                                                                    \
    w->x640 = 9999;                                                                                \
    w->pWep->setEffAlways(0x10, 0x2D);                                                             \
    ofs.x = 0.0f;                                                                                  \
    ofs.y = 40.0f;                                                                                 \
    ofs.z = 60.0f;                                                                                 \
    w->pWep->setEffAlways2(0x10, 0x2F, 0, &ofs, 3);                                                \
    SndCall(8, 0x94, &em->pos, em->id, 0, em)

// Take the spare weapon in hand (em10_R1_FixBomber; same block as em10_R1_WeaponChange).
#define EM10_WEP2_TAKE()                                                                           \
    if (em->flags_3C8 & 0x10000) {                                                                 \
        w->pWep = em10MakeWeapon(em, w->wep2Type);                                                 \
        if (w->pWep) {                                                                             \
            w->wepType = w->wep2Type;                                                              \
        } else {                                                                                   \
            w->wepType = 0;                                                                        \
        }                                                                                          \
    } else {                                                                                       \
        w->pWep2->setTransMode(1);                                                                 \
        w->pWep = w->pWep2;                                                                        \
        w->wepType = w->wep2Type;                                                                  \
        w->pWep2 = 0;                                                                              \
        w->wep2Type = 0;                                                                           \
    }                                                                                              \
    em10WeaponSet(em)

static void em10_R1_FixBomber(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec ofs;
    Mtx inv;
    f32 f;
    f32 ang;
    int flag;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        w->x18 = em->rot.y;
        em->xFE++;
    case 1:
        if (em->xFF) {
            f = Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
        } else {
            ang = Muku(&em->pos, &pPL->pos, w->x18, 3.1415927f);
            if (ang > 0.17453292f) {
                ang = 0.17453292f;
            }
            if (ang < -0.17453292f) {
                ang = -0.17453292f;
            }
            ang = LIMIT_ANGLE(ang + w->x18);
            f = Muku2(em->rot.y, ang, 0.09817477f);
        }
        em->rot.y += f;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (em10GotoCk(em)) {
            break;
        }
        if (w->pWep == 0) {
            em->xFE = 6;
            break;
        }
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            EM10_BOMB_FIRE_EFFECT();
            em->xFE = 4;
            break;
        }
        if (w->x52C < em->x3CC) {
            em->x38D = 0;
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            break;
        }
        if (em->xFF && !(w->flags & 1)) {
            break;
        }
        if (em10DeadCk(pPL)) {
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &ofs);
        if (ofs.x > -2000.0f && ofs.x < 2000.0f && ofs.y < 1000.0f && ofs.z > 1000.0f && ofs.z < 20000.0f) {
            if (em10BombThrowScaCk(em)) {
                if (w->x640) {
                    em->xFE = 4;
                } else {
                    em->xFE = 2;
                }
            }
        }
        break;
    case 2:
        if (em->flags_3C8 & 0x1000000) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x7E), (int) PL_ARC_PTR(em->subArc, 0x7F), 10, 0x40, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x7E), (int) PL_ARC_PTR(em->subArc, 0x7F), 10, 0, 0);
        }
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        if (em->seFlags28B & 1) {
            EM10_BOMB_FIRE_EFFECT();
            if (em10BombThrowScaCk(em)) {
                em->xFE++;
            } else {
                em->xFE = 0;
            }
        }
        break;
    case 4:
        if (em->flags_3C8 & 0x1000000) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, 0x40, 0x12);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, 0, 0x12);
        }
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x4 = 10;
        em->xFE++;
    case 5:
        if (w->x4) {
            w->x4--;
        } else if (em->xFF) {
            em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 4) {
            w->x670 = 2;
        }
        if ((em->seFlags28B & 1) && w->pWep) {
            em10BombThrow(em);
        }
        if (MotionMoveF(em, 0)) {
            if (em->xFF == 2) {
                em->xFE = 8;
            } else {
                em->xFE = 0;
            }
        } else if ((em->seFlags28B & 2) && em->xFF == 2) {
            em->xFE = 8;
        }
        break;
    case 6:
        flag = (em->flags_3C8 & 0x1000000) ? 0x40 : 0;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x76), (int) PL_ARC_PTR(em->subArc, 0x77), 10, flag, 0);
        em->xFE++;
    case 7:
        if ((em->seFlags28B & 4) && w->pWep2) {
            EM10_WEP2_TAKE();
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x9A), 0, 5, 1, 0);
        em->xFE++;
    case 9:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
            if (w->pWep2) {
                EM10_WEP2_TAKE();
            }
        }
        break;
    case 10:
        w->x4 = (u8) (Rnd() % 90) + 90;
        em->xFE++;
    case 11:
        MotionMoveF(em, 0);
        if (w->x524 < 3000.0f) {
            em->setFindPL();
            em->xFE++;
        } else if (w->flags & 1) {
            if (w->x4) {
                w->x4--;
            } else {
                em->xFE++;
            }
        }
        break;
    case 12:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x9B), 0, 5, 1, 0);
        EM10_BOMB_FIRE_EFFECT();
        em->xFE++;
    case 13:
        if (MotionMoveF(em, 0)) {
            em->xFE = 4;
        }
        break;
    }
    em10HandSet(em, 0);
}
static void em10_R1_R305Bomber(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec ofs;
    int flag;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (em10GotoCk(em)) {
            break;
        }
        if (w->pWep == 0) {
            em->xFE = 4;
            break;
        }
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            EM10_BOMB_FIRE_EFFECT();
            em->xFE = 2;
        }
        break;
    case 2:
        if (em->flags_3C8 & 0x1000000) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, 0x40, 0x12);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, 0, 0x12);
        }
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x4 = 10;
        em->xFE++;
    case 3:
        if (w->x4) {
            w->x4--;
        } else if (em->xFF) {
            em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 4) {
            w->x670 = 2;
        }
        if ((em->seFlags28B & 1) && w->pWep) {
            em10BombThrow(em);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 4;
        }
        break;
    case 4:
        flag = (em->flags_3C8 & 0x1000000) ? 0x40 : 0;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x76), (int) PL_ARC_PTR(em->subArc, 0x77), 10, flag, 0);
        em->xFE++;
    case 5:
        if ((em->seFlags28B & 4) && w->pWep2) {
            EM10_WEP2_TAKE();
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_R408Bomber(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec ofs;
    int flag;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        w->x4 = (u8) (Rnd() % 30) + 60;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em10GotoCk(em)) {
            break;
        }
        if (w->pWep == 0) {
            em->xFE = 4;
            break;
        }
        if (w->x52C < em->x3CC) {
            em->x38D = 0;
            EmRoutineSet(em, 1, 0x10, 0, 0);
            break;
        }
        if (w->x4) {
            w->x4--;
            break;
        }
        EM10_BOMB_FIRE_EFFECT();
        em->xFE = 2;
        break;
    case 2:
        if (em->flags_3C8 & 0x1000000) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, 0x40, 0x12);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x82), (int) PL_ARC_PTR(em->subArc, 0x83), 10, 0, 0x12);
        }
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x4 = 10;
        em->xFE++;
    case 3:
        if (w->x4) {
            w->x4--;
        } else if (em->xFF) {
            em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 4) {
            w->x670 = 2;
        }
        if ((em->seFlags28B & 1) && w->pWep) {
            em10BombThrow(em);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 4;
        }
        break;
    case 4:
        flag = (em->flags_3C8 & 0x1000000) ? 0x40 : 0;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x76), (int) PL_ARC_PTR(em->subArc, 0x77), 10, flag, 0);
        em->xFE++;
    case 5:
        if ((em->seFlags28B & 4) && w->pWep2) {
            EM10_WEP2_TAKE();
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    em10HandSet(em, 0);
}
#undef EM10_BOMB_FIRE_EFFECT
#undef EM10_WEP2_TAKE

static void em10_R1_RocketWait(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        em10SetWaitMotion(em, 10);
        w->x18 = em->rot.y;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (!em10GotoCk(em) && (em->flags_3C8 & 1)) {
            em->x38D = 0;
            EmRoutineSet(em, 1, 0x22, 0, 0);
        }
        break;
    }
    em10HandSet(em, 0);
}

// One melee sweep segment: a point offset in the swinging part's frame, checked against the sweep base (em10_R1_AxeAtk).
#define EM10_AXE_SWEEP_CK(m, px, py, pz, base)                                                     \
    v2.x = px;                                                                                     \
    v2.y = py;                                                                                     \
    v2.z = pz;                                                                                     \
    PSMTXMultVec(m, &v2, &v2);                                                                     \
    em10AtkCk(em, &v2, base, atk, 0)

static void em10_R1_AxeAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v2;
    Vec v;
    cModel* p;
    void* m0;
    int m1;
    int flag;
    int atk;

    switch (em->xFE) {
    case 0:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        m0 = PL_ARC_PTR(em->subArc, 0x80);
        m1 = (int) PL_ARC_PTR(em->subArc, 0x81);
        if (w->wepType == 0xB) {
            m0 = PL_ARC_PTR(em->subArc, 0x178);
            m1 = (int) PL_ARC_PTR(em->subArc, 0x179);
        }
        if (w->wepType == 0xF) {
            m0 = PL_ARC_PTR(em->subArc, 0x1A5);
            m1 = (int) PL_ARC_PTR(em->subArc, 0x1A6);
        }
        if (w->pShield) {
            m0 = PL_ARC_PTR(em->subArc, 0x170);
            m1 = (int) PL_ARC_PTR(em->subArc, 0x171);
        }
        if (em->type == 2) {
            m0 = PL_ARC_PTR(em->subArc, 0x1A2);
            m1 = (int) PL_ARC_PTR(em->subArc, 0x1A3);
        }
        if (em->type == 0x18) {
            EstSet((int) em, -1, 0, 0, 0x10, 0x98, 0, 0, (u32) em, 0);
        }
        MotionSetCore(em, MOTION(em), m0, m1, 10, flag, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x4 = 20;
        if (pG->x4F88 <= 3) {
            w->x4 = 5;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 30;
        }
        w->x697 = 0;
        em->xFE++;
    case 1:
        if ((w->x4 && --w->x4) || (em->seFlags28B & 8)) {
            f32 ang = (em->seFlags28B & 8) ? 0.049087387f : 0.19634955f;
            if ((w->flags & 0x8000000) && pSUB) {
                em->rot.y += Muku(&em->pos, &pSUB->pos, em->rot.y, ang);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            } else {
                em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, ang);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            }
        }
        if (em->seFlags28B & 0x20) {
            switch (w->wepType) {
            case 7:
                SndCall(8, 0x42, &em->pos, em->id, 0, em);
                break;
            case 0xF:
                SndCall(8, 0x42, &em->pos, em->id, 0, em);
                break;
            default:
                SndCall(8, 0x3D, &em->pos, em->id, 0, em);
                break;
            }
            if (em->type == 2) {
                SndCall(8, 0xB3, &em->pos, em->id, 0, em);
            }
        }
        if (em->seFlags28B & 1) {
            atk = (w->wepType == 0xA) ? 1 : 0;
            if (w->wepType == 0xB) {
                atk = 2;
            }
            if (w->wepType == 0xF) {
                atk = 3;
            }
            if (em->type == 2) {
                atk = 0x10;
            }
            if (em->type == 0x18) {
                atk = 0x12;
            }
            if (em->type == 2) {
                p = em->getPartsPtr(0x22);
                em10AtkCk(em, &p->worldPos, &p->x88, 0x10, 0);
                p = em->getPartsPtr(0x23);
                em10AtkCk(em, &p->worldPos, &p->x88, 0x10, 0);
                p = em->getPartsPtr(0x24);
                em10AtkCk(em, &p->worldPos, &p->x88, 0x10, 0);
                p = em->getPartsPtr(0x10);
                em10AtkCk(em, &p->worldPos, &p->x88, 0x10, 0);
            }
            if (em->type == 0x18) {
                p = em->getPartsPtr(10);
                v.x = 1000.0f;
                v.y = 0.0f;
                v.z = -1000.0f;
                PSMTXMultVec(p->mat, &v, &v);
                EM10_AXE_SWEEP_CK(p->mat, 200.0f, 0.0f, -200.0f, &v);
                EM10_AXE_SWEEP_CK(p->mat, -50.0f, 0.0f, 50.0f, &v);
                EM10_AXE_SWEEP_CK(p->mat, -300.0f, 0.0f, 300.0f, &v);
                EM10_AXE_SWEEP_CK(p->mat, -550.0f, 0.0f, 550.0f, &v);
            }
            if (w->pWep) {
                if (w->wepType == 0xB) {
                    cModel* q = w->pWep->getPartsPtr(10);
                    em10AtkCk(em, &q->worldPos, &q->x88, atk, 0);
                    EM10_AXE_SWEEP_CK(w->pWep->mat, 0.0f, 0.0f, 150.0f, &em->pos);
                } else {
                    v.x = 0.0f;
                    v.y = 0.0f;
                    v.z = -1000.0f;
                    PSMTXMultVec(w->pWep->mat, &v, &v);
                    EM10_AXE_SWEEP_CK(w->pWep->mat, 0.0f, 0.0f, -200.0f, &v);
                    EM10_AXE_SWEEP_CK(w->pWep->mat, 0.0f, 0.0f, 50.0f, &v);
                    if (w->wepType == 0x10) {
                        EM10_AXE_SWEEP_CK(w->pWep->mat, 0.0f, 0.0f, 300.0f, &v);
                        EM10_AXE_SWEEP_CK(w->pWep->mat, 0.0f, 0.0f, 550.0f, &v);
                    }
                }
            }
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            if (w->x697) {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
                if (w->wepType == 0xF) {
                    w->x67C = 90;
                }
                if (em->type == 2) {
                    w->x67C = 90;
                }
            }
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}
#undef EM10_AXE_SWEEP_CK

static void em10_R1_ShieldAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    cModel* parts;
    int flag;

    switch (em->xFE) {
    case 0:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x16E), (int) PL_ARC_PTR(em->subArc, 0x16F), 10, flag, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x4 = 20;
        if (pG->x4F88 <= 3) {
            w->x4 = 5;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 30;
        }
        w->x697 = 0;
        em->xFE++;
    case 1:
        if ((w->x4 && --w->x4) || (em->seFlags28B & 8)) {
            f32 ang = (em->seFlags28B & 8) ? 0.049087387f : 0.09817477f;
            // LIMIT_ANGLE in both arms (cross-jumped): its argument is the summed register, not a reload.
            if ((w->flags & 0x8000000) && pSUB) {
                em->rot.y += Muku(&em->pos, &pSUB->pos, em->rot.y, ang);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            } else {
                em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, ang);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            }
        }
        if (em->seFlags28B & 0x20) {
            if (w->wepType == 7) {
                SndCall(8, 0x42, &em->pos, em->id, 0, em);
            } else {
                SndCall(8, 0x3D, &em->pos, em->id, 0, em);
            }
        }
        if (em->seFlags28B & 1) {
            if (em->flags_3C8 & 0x1000000) {
                parts = em->getPartsPtr(10);
            } else {
                parts = em->getPartsPtr(16);
            }
            v = parts->worldPos;
            em10AtkCk(em, &v, &parts->x88, 4, 0);
            v.y -= 500.0f;
            em10AtkCk(em, &v, &parts->x88, 4, 0);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            if (w->x697) {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
            }
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_TorchFrame(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    switch (em->xFE) {
    case 0:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        w->x8 = 0;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x84), (int) PL_ARC_PTR(em->subArc, 0x85), 10, flag, 0);
        w->x682 = 60;
        w->x8 = 1;
        SndCall(8, 0x8C, &em->pos, em->id, 0, em);
        w->x4 = 10;
        w->x697 = 0;
        w->x698 = 0;
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            if ((w->flags & 0x8000000) && pSUB) {
                em->rot.y += Muku(&em->pos, &pSUB->pos, em->rot.y, 0.09817477f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            } else {
                em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.09817477f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            }
        }
        if ((em->seFlags28B & 1) && w->pWep) {
            em10TorchFrameAtkCk(em);
            em10TorchFrameAtkCkSub(em);
        }
        if (em->seFlags28B & 2) {
            EstSetEm(em, -1, 0, 0, 0x10, 0x23, 0, w->x69E, em, 0);
        }
        if (em->seFlags28B & 0x20) {
            w->x5C0 = SndCall(8, 0x8D, &em->pos, em->id, 0, em);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            if (w->x697) {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
            }
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

// One melee sweep segment: a point offset in the weapon's frame, checked against the sweep base (em10_R1_SukiAtk).
#define EM10_SUKI_SWEEP_CK(pz) \
    v2.x = 0.0f; \
    v2.y = 0.0f; \
    v2.z = pz; \
    PSMTXMultVec(w->pWep->mat, &v2, &v2); \
    em10AtkCk(em, &v2, &v, 8, 0)

static void em10_R1_SukiAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v2;
    Vec v;
    f32 dy;
    u32 sel;
    int flag;
    int hit;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        sel = 0;
        dy = pPL->pos.y - em->pos.y;
        if (dy > -100.0f) {
            sel = 1;
        }
        if (dy < -800.0f) {
            sel = 2;
        }
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        switch (sel) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x15D), (int) PL_ARC_PTR(em->subArc, 0x15E), 10, flag, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x15F), (int) PL_ARC_PTR(em->subArc, 0x160), 10, flag, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x161), (int) PL_ARC_PTR(em->subArc, 0x162), 10, flag, 0);
            break;
        }
        w->x4 = 20;
        if (pG->x4F88 <= 3) {
            w->x4 = 5;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 30;
        }
        w->x697 = 0;
        em10CallVoiceSe2(em, w->se6D2, 8);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x20) {
            SndCall(8, 0x3D, &em->pos, em->id, 0, em);
        }
        if (w->x4) {
            w->x4--;
            if ((w->flags & 0x8000000) && pSUB) {
                em->rot.y += Muku(&em->pos, &pSUB->pos, em->rot.y, 0.19634955f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            } else {
                em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.19634955f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            }
        }
        if ((em->seFlags28B & 1) && w->pWep) {
            hit = w->x697;
            if (hit == 0) {
                v.x = 0.0f;
                v.y = 0.0f;
                v.z = -2000.0f;
                PSMTXMultVec(w->pWep->mat, &v, &v);
                EM10_SUKI_SWEEP_CK(-1000.0f);
                EM10_SUKI_SWEEP_CK(-700.0f);
                EM10_SUKI_SWEEP_CK(-400.0f);
                EM10_SUKI_SWEEP_CK(-100.0f);
                EM10_SUKI_SWEEP_CK(300.0f);
                EM10_SUKI_SWEEP_CK(600.0f);
                if (w->x697) {
                    EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x44, 0, 0, (u32) w->pWep, (void*) hit);
                }
            }
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            if (w->x697) {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
            }
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}
#undef EM10_SUKI_SWEEP_CK

// One melee sweep segment: a point offset in the weapon's frame, checked against the sweep base (em10_R1_ScytheAtk).
#define EM10_SCYTHE_SWEEP_CK(pz) \
    v2.x = 0.0f; \
    v2.y = 0.0f; \
    v2.z = pz; \
    PSMTXMultVec(w->pWep->mat, &v2, &v2); \
    em10AtkCk(em, &v2, &v, atk, 0)

static void em10_R1_ScytheAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v2;
    Vec v;
    cModel* p;
    int flag;
    int atk;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        p = pPL->getPartsPtr(4);
        if (p->worldPos.y < em->pos.y + 1300.0f) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x148), (int) PL_ARC_PTR(em->subArc, 0x149), 10, flag, em->xFF);
            em->xFF = 1;
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x13D), (int) PL_ARC_PTR(em->subArc, 0x13E), 10, flag, em->xFF);
            em->xFF = 0;
        }
        w->x4 = 20;
        if (pG->x4F88 <= 3) {
            w->x4 = 5;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 30;
        }
        w->x697 = 0;
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x8 = 31;
        em->xFE++;
    case 1:
        if ((w->x4 && --w->x4) || (em->seFlags28B & 8)) {
            f32 ang = (em->seFlags28B & 8) ? 0.049087387f : 0.19634955f;
            if ((w->flags & 0x8000000) && pSUB) {
                em->rot.y += Muku(&em->pos, &pSUB->pos, em->rot.y, ang);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            } else {
                em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, ang);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            }
        }
        if (w->x8 && --w->x8 == 0) {
            EstSet((int) em, -1, 0, 0, 0x10, 0x43, 0, 0, (u32) em, 0);
        }
        if ((em->seFlags28B & 1) && w->pWep) {
            atk = em->xFF ? 10 : 9;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -1000.0f;
            PSMTXMultVec(w->pWep->mat, &v, &v);
            EM10_SCYTHE_SWEEP_CK(200.0f);
            EM10_SCYTHE_SWEEP_CK(400.0f);
            EM10_SCYTHE_SWEEP_CK(600.0f);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            if (w->x697) {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
            }
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}
#undef EM10_SCYTHE_SWEEP_CK

// One claw sweep segment: a point offset along the claw part's x axis, checked against the sweep base (em10_R1_ClawAtk).
#define EM10_CLAW_SWEEP_CK(px, part) \
    v2.x = px; \
    v2.y = 0.0f; \
    v2.z = 0.0f; \
    PSMTXMultVec(*m, &v2, &v2); \
    em10AtkCk(em, &v2, &v, 0xD, part)

static void em10_R1_ClawAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v2;
    Vec v;
    int r;

    if (em10FindCk2(em)) {
        w->x654 = 150;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x11C), (int) PL_ARC_PTR(em->subArc, 0x11D), 10, 1, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x654 = 0;
        w->x4 = 20;
        if (pGS->x4F88 <= 3) {
            w->x4 = 5;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 30;
        }
        w->x20 = 0;
        w->x697 = 0;
        em->xFE++;
    case 1:
        if ((w->x4 && --w->x4) || (em->seFlags28B & 8)) {
            f32 ang = (em->seFlags28B & 8) ? 0.049087387f : 0.19634955f;
            em->rot.y += Muku(&em->pos, &w->x534, em->rot.y, ang);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 0x20) {
            SndCall(8, 0x3D, &em->pos, em->id, 0, em);
        }
        if (em->seFlags28B & 3) {
            w->x697 = 0;
            if (em->seFlags28B & 2) {
                v.x = -500.0f;
                v.y = 0.0f;
                v.z = 0.0f;
                Mtx* m = &em->getPartsPtr(0x10)->mat;
                PSMTXMultVec(*m, &v, &v);
                EM10_CLAW_SWEEP_CK(-300.0f, 0x10);
                EM10_CLAW_SWEEP_CK(0.0f, 0x10);
                EM10_CLAW_SWEEP_CK(300.0f, 0x10);
                EM10_CLAW_SWEEP_CK(600.0f, 0x10);
            } else {
                v.x = 500.0f;
                v.y = 0.0f;
                v.z = 0.0f;
                Mtx* m = &em->getPartsPtr(10)->mat;
                PSMTXMultVec(*m, &v, &v);
                EM10_CLAW_SWEEP_CK(300.0f, 10);
                EM10_CLAW_SWEEP_CK(0.0f, 10);
                EM10_CLAW_SWEEP_CK(-300.0f, 10);
                EM10_CLAW_SWEEP_CK(-600.0f, 10);
            }
            if (w->x697) {
                w->x20++;
            }
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            if (w->x697) {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
            }
            if ((s16) w->x654 == 0) {
                EmRoutineSet(em, 1, 0x5D, 0, 0);
            } else {
                em10WalkRtnSet(em);
            }
        } else if ((em->seFlags28B & 4) && w->x697 == 0) {
            u8 rnd;
            if ((s16) w->x654 != 0 && (rnd = Rnd() % 10, rnd > 4)) {
                r = em10ClawCriAtkCk(em);
                if (r == 0) {
                    if ((em->pos.x - w->x534.x) * (em->pos.x - w->x534.x) + (em->pos.z - w->x534.z) * (em->pos.z - w->x534.z) > 16000000.0f &&
                        !EM_RTN(em, 1, 0x11)) {
                        em10CallVoiceSe2(em, 0x71, 6);
                        w->x6AC = 0;
                        EmRoutineSet(em, 1, 0x11, 0, 0);
                    }
                }
            } else {
                w->x654 = 0;
            }
        }
        break;
    }
    em10HandSet(em, 0);
}
#undef EM10_CLAW_SWEEP_CK

static void em10_R1_br_CSawWalkAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->hp > 0) {
        int one = 1; // kept in a callee-saved reg across the calls (docs/matching.md)
        if (pG->flags_64 & 0x2000000) {
            EmRoutineSet(em, one, 0, 0, 0);
        } else if (!em10GotoCk(em) && !em10DoorOpenCk(em, 0) && !em10RackBreakCk(em) && !em10LadderClimbCk(em) && !em10VLadderClimbCk(em) && !em10LadderResetCk(em) && !em10JumpDownCk(em) && !em10JumpCk(em)) {
            em10ReturnStartPosCk(em);
            if (!em10ClimbOverCk(em) && !em10WindowCk(em)) {
                if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && w->x634 > 30 && fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) < 0.5235988f) {
                    EmRoutineSet(em, one, 0x1C, 0, 0);
                } else if (!em10IgnitionCk(em) && !em10ClawStickCK(em) && !em10FindLostCk(em)) {
                    if (w->flags & 0x20000000) {
                        f32 dx = em->pos.x - w->x4D8.x;
                        f32 dz = em->pos.z - w->x4D8.z;
                        if (dx * dx + dz * dz < 4000000.0f) {
                            w->flags &= ~0x20000000;
                            EmRoutineSet(em, 1, 1, 0, 0);
                            return;
                        }
                        if (em->plDist2 < 4000000.0f && w->x530 < em->x3CC) {
                            w->flags &= ~0x20000000;
                        }
                    }
                    em10GotoPosCk(em);
                }
            }
        }
    }
}

// One chainsaw sweep segment: a point offset along the saw part's x axis, checked against the sweep base (em10_R1_CSawWalkAtk).
#define EM10_CSAW_SWEEP_CK(px) \
    v2.x = px; \
    v2.y = 0.0f; \
    v2.z = 0.0f; \
    PSMTXMultVec(*m, &v2, &v2); \
    em10AtkCk(em, &v2, &v, 0xC, 10)

static void em10_R1_CSawWalkAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v2;
    Vec v;
    int end;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2BC), (int) PL_ARC_PTR(em->subArc, 0x2BD), 5, 5, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x4 = 20;
        if (pG->x4F88 <= 3) {
            w->x4 = 5;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 30;
        }
        w->x20 = 0;
        w->x697 = 0;
        em->xFE++;
    case 1:
        w->x1C = Muku(&em->pos, &w->x54C, em->rot.y, PI) * 0.3f;
        w->x1C = Muku2(0.0f, w->x1C, 0.15707964f);
        em->rot.y += w->x1C;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        end = MotionMoveF(em, 0);
        if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
            em->hp = 0;
        }
        if (em->hp <= 0) {
            if (end) {
                EmRoutineSet(em, 2, 9, 0, 0);
            }
            break;
        }
        if (end || (em->seFlags28B & 4)) {
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 0x1B, 0, 0);
                break;
            }
            if (pSUB && (s16) pG->sub_life <= 0) {
                EmRoutineSet(em, 1, 0x1B, 0, 0);
                break;
            }
            if ((em->flags_3C8 & 0x400) && w->x5EC == 0 && em->pos.y > pPL->pos.y + 500.0f) {
                if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 2.7488937f && em->plDist2 > 36000000.0f) {
                    EmRoutineSet(em, 1, 0x15, 0, 0);
                    break;
                }
            } else if (w->x51C > 2.7488937f && em->plDist2 > 36000000.0f) {
                EmRoutineSet(em, 1, 0x15, 0, 0);
                break;
            }
            if (w->x697) {
                EmRoutineSet(em, 1, 0x1B, 0, 0);
                break;
            }
        }
        if (end) {
            if ((u8) (Rnd() % 10) == 0 && !Ctrl12Ck(w->pCtrl12, 0xC)) {
                em10CallVoiceSe2(em, w->se6D5, 8);
            }
        }
        if (em->seFlags28B & 0x20) {
            SndCall(8, 0x3D, &em->pos, em->id, 0, em);
        }
        if (em->seFlags28B & 1) {
            Mtx* m;
            w->x697 = 0;
            v.x = 500.0f;
            v.y = 0.0f;
            v.z = 0.0f;
            m = &em->getPartsPtr(10)->mat;
            PSMTXMultVec(*m, &v, &v);
            EM10_CSAW_SWEEP_CK(300.0f);
            EM10_CSAW_SWEEP_CK(0.0f);
            EM10_CSAW_SWEEP_CK(-300.0f);
            EM10_CSAW_SWEEP_CK(-600.0f);
        }
        break;
    }
    em10BreathSe(em);
    em10HandSet(em, 0);
    em10CsawSignSe(em);
    em10BehindSeCk(em);
}
#undef EM10_CSAW_SWEEP_CK

// One claw sweep segment: a point offset along the claw part's x axis, checked against the sweep base (em10_R1_ClawWalkAtk).
#define EM10_CLAW_SWEEP_CK(px) \
    v2.x = px; \
    v2.y = 0.0f; \
    v2.z = 0.0f; \
    PSMTXMultVec(*m, &v2, &v2); \
    em10AtkCk(em, &v2, &v, 0xD, 0)

static void em10_R1_ClawWalkAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v2;
    Vec v;
    int end;

    if (em->xFE == 0 && (w->x6BE == 4 || w->x6BF == 4)) {
        em->xFE = 2;
    }
    if (em10FindCk2(em)) {
        w->x654 = 150;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x121), (int) PL_ARC_PTR(em->subArc, 0x122), 10, 5, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x654 = 0;
        w->x4 = (u8) (Rnd() % 5) + 10;
        w->x20 = 0;
        w->x697 = 0;
        em->xFE++;
    case 1:
        if ((em->pos.x - w->x534.x) * (em->pos.x - w->x534.x) + (em->pos.y - w->x534.y) * (em->pos.y - w->x534.y) +
                (em->pos.z - w->x534.z) * (em->pos.z - w->x534.z) <
            1000000.0f) {
            w->x4 = 0;
        } else {
            w->x1C = Muku(&em->pos, &w->x54C, em->rot.y, PI) * 0.3f;
            w->x1C = Muku2(0.0f, w->x1C, 0.19634955f);
            em->rot.y += w->x1C;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        end = MotionMoveF(em, 0);
        if (em->seFlags28B & 0x20) {
            SndCall(8, 0x3D, &em->pos, em->id, 0, em);
        }
        if (em->seFlags28B & 3) {
            if (em->seFlags28B & 2) {
                Mtx* m;
                v.x = -500.0f;
                v.y = 0.0f;
                v.z = 0.0f;
                m = &em->getPartsPtr(0x10)->mat;
                PSMTXMultVec(*m, &v, &v);
                EM10_CLAW_SWEEP_CK(-300.0f);
                EM10_CLAW_SWEEP_CK(0.0f);
                EM10_CLAW_SWEEP_CK(300.0f);
                EM10_CLAW_SWEEP_CK(600.0f);
            } else {
                Mtx* m;
                v.x = 500.0f;
                v.y = 0.0f;
                v.z = 0.0f;
                m = &em->getPartsPtr(10)->mat;
                PSMTXMultVec(*m, &v, &v);
                EM10_CLAW_SWEEP_CK(300.0f);
                EM10_CLAW_SWEEP_CK(0.0f);
                EM10_CLAW_SWEEP_CK(-300.0f);
                EM10_CLAW_SWEEP_CK(-600.0f);
            }
        }
        if (end || (em->seFlags28B & 4)) {
            if (w->x697) {
                w->x4 = 0;
            }
            if (w->x4) {
                w->x4--;
            } else {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
                if ((s16) w->x654 == 0) {
                    EmRoutineSet(em, 1, 0x5D, 0, 0);
                } else {
                    em10WalkRtnSet(em);
                }
                break;
            }
            if (w->x51C > 1.9634955f) {
                EmRoutineSet(em, 1, 0x15, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x115), 0, 3, 1, 0);
        w->x4 = 25;
        w->x8 = 46;
        SndCall(6, 0x71, &em->getPartsPtr(0)->worldPos, 0, 0, em);
        em->xFE++;
    case 3:
        if (em->frame > 9.7f && em->frame < 10.3f) {
            w->x6BE = 1;
            w->x6BF = 1;
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    em10HandSet(em, 0);
}
#undef EM10_CLAW_SWEEP_CK

static void em10_R1_br_ClawCriAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    Vec p;
    Mtx* m;

    if (em->hp <= 0) {
        return;
    }
    if (em->xFE != 5) {
        return;
    }
    if (!(em->seFlags28B & 1)) {
        return;
    }
    {
        p.x = 500.0f;
        p.y = 0.0f;
        p.z = 0.0f;
        m = &em->getPartsPtr(10)->mat;
        PSMTXMultVec(*m, &p, &p);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(*m, &v, &v);
        em10AtkCk(em, &v, &p, 0xE, 0);
        v.x = -300.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(*m, &v, &v);
        em10AtkCk(em, &v, &p, 0xE, 0);
        v.x = -600.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(*m, &v, &v);
        em10AtkCk(em, &v, &p, 0xE, 0);
        switch (pG->x4FB8) {
        case 0:
        case 3:
        case 5:
            if (pSys->region && w->x697 && (s16) pG->pl_life <= 0) {
                pG->pl_life = 1;
                EmRoutineSet(em, 1, 0x2E, 0, 0);
            }
            break;
        }
    }
}

static void em10_R1_ClawCriAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    Mtx inv;
    Vec lp;
    Vec hit;
    Vec nrm;
    Vec rot;

    if (em10FindCk2(em)) {
        w->x654 = 150;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x118), (int) PL_ARC_PTR(em->subArc, 0x119), 10, 1, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x697 = 0;
        w->x646 = 0x1C2;
        w->x654 = 0;
        w->x4 = 80;
        w->x8 = 15;
        em->xFE++;
    case 1:
        if (em->frame > 14.7f && em->frame < 15.3f) {
            if (w->x6BE != 2) {
                w->x6BE = 1;
            }
            if (w->x6BF != 2) {
                w->x6BF = 1;
            }
        }
        em->rot.y += Muku(&em->pos, &w->x534, em->rot.y, 0.3926991f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (w->x4 == 0) {
            PSMTXInverse(em->mat, inv);
            PSMTXMultVec(inv, &pPL->pos, &lp);
            if (lp.x > -300.0f && lp.x < 300.0f && lp.y > -500.0f && lp.y < 500.0f && lp.z > 0.0f && lp.z < 2500.0f) {
                MotionMoveF(em, 0);
                em->xFE = 4;
                break;
            }
        } else {
            w->x4--;
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x10B), (int) PL_ARC_PTR(em->subArc, 0x10C), 3, 5, 3);
        w->x20 = 1;
        em->xFE++;
    case 3:
        if ((w->x534.x - em->pos.x) * (w->x534.x - em->pos.x) + (w->x534.y - em->pos.y) * (w->x534.y - em->pos.y) +
                    (w->x534.z - em->pos.z) * (w->x534.z - em->pos.z) >
                49000000.0f &&
            w->x20 && w->x508 < 0.5235988f) {
            em->rot.y += Muku(&em->pos, &w->x534, em->rot.y, 0.049087387f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        } else {
            w->x20 = 0;
        }
        MotionMoveF(em, 0);
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 0.0f;
        b.y = 500.0f;
        b.z = 2800.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            em->xFE = 4;
            break;
        }
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &lp);
        if (lp.x > -300.0f && lp.x < 300.0f && lp.y > -500.0f && lp.y < 500.0f && lp.z > 0.0f && lp.z < 2500.0f) {
            em->xFE = 4;
            break;
        }
        if (w->x634 > 5) {
            em->xFE = 4;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x11E), (int) PL_ARC_PTR(em->subArc, 0x11F), 3, 1, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        w->x20 = 0;
        em->xFE++;
    case 5:
        if (em->seFlags28B & 0x20) {
            SndCall(8, 0x3D, &em->pos, em->id, 0, em);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            if (w->x697) {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
            }
            if ((s16) w->x654 == 0) {
                EmRoutineSet(em, 1, 0x5D, 0, 0);
            } else {
                em10WalkRtnSet(em);
            }
        } else {
            if ((em->seFlags28B & 2) && w->x20 == 0) {
                Mtx* m;
                a.x = 0.0f;
                a.y = 0.0f;
                a.z = -100.0f;
                b.x = -1800.0f;
                b.y = 0.0f;
                b.z = -100.0f;
                m = &em->getPartsPtr(10)->mat;
                PSMTXMultVec(*m, &a, &a);
                PSMTXMultVec(*m, &b, &b);
                rot = em->rot;
                rot.y += PI;
                rot.y = LIMIT_ANGLE(rot.y);
                if (EatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0x4000)) {
                    b = hit;
                    rot.y = atan2f(nrm.x, nrm.z);
                    EstSet(0, -1, &b, &rot, 0x10, 0x7C, 0, 0, 0, 0);
                    w->x20 = 1;
                    SndCall(6, 0x72, &em->getPartsPtr(0)->worldPos, 0, 0, em);
                }
            }
            if ((em->seFlags28B & 4) && w->x20) {
                em->xFE = 6;
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x11A), (int) PL_ARC_PTR(em->subArc, 0x11B), 3, 1, 0);
        em10CallVoiceSe2(em, w->se6D2, 8);
        EstSet((int) em, -1, 0, 0, 0x10, 0x7E, 0, 0, (u32) em, 0);
        w->x4 = 120;
        em->xFE++;
    case 7:
        if (w->x4) {
            w->x4--;
            em->atari.flags |= 8;
            w->x6BC = 2;
        }
        if (em->seFlags28B & 1) {
            SndCall(6, 0x73, &em->getPartsPtr(0)->worldPos, 0, 0, em);
        }
        if (em->seFlags28B & 4) {
            SndCall(6, 0x74, &em->getPartsPtr(0)->worldPos, 0, 0, em);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            if (w->x697) {
                w->x67C = 15;
                if (pG->x4F88 <= 3) {
                    w->x67C = 45;
                }
                if (pG->x4F88 <= 1) {
                    w->x67C = 90;
                }
            }
            if (em->plDist2 > 6250000.0f) {
                EmRoutineSet(em, 1, 0x5D, 0, 0);
            } else {
                em10WalkRtnSet(em);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_ClawCriHit(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    em->dmg.x1 = 2;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x120), 0, 10, 1, 0);
        EstSetEm(em, -1, 0, 0, 0x10, 0x8C, 0, 0, em, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem10_ClawCriHit, 241.15f, 0.0f, 975.16f);
        w->x4 = 10;
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        em->xFE++;
    case 1:
        if (MOTION(em)->seqFrame > 123.7f && MOTION(em)->seqFrame < 124.3f) {
            w->x6BE = 1;
            pG->pl_life = 0;
            DiedemoExec(2, 0);
        }
        if (w->x4) {
            w->x4--;
            EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            MotionMoveF(em, 0);
        }
        break;
    }
    em->x3A8 = em->pos;
    em10HandSet(em, 0);
}

static void plem10_ClawCriHit(cPlayer* pl)
{
    cEm* em;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    em = (cEm*) pl->dmgType;
    pl->subArc = em->subArc;
    switch (pl->xFE) {
    case 0:
        pl->atari.throughOn();
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x123), 0, 5, 1, 0);
        EstSetEm(pl, -1, 0, 0, 0x10, 0x8B, 0, 0, pl, 0);
        pl->atari.set(10, 400.0f, 700.0f);
        pl->x3E8 = SndCall(1, 0xC, &pPL->pos, 0, 0, pPL);
        pl->x3E0 = 10;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
            EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            MotionMoveF(pl, 0);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em10_R1_br_C_SawAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->hp > 0 && (em->seFlags28B & 1) && w->pWep && em10CsawHitCk(em)) {
        EM_RTN_SET(em, 1, 0x30);
    }
}

static void em10_R1_C_SawAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xE9), (int) PL_ARC_PTR(em->subArc, 0xEA), 10, flag, 0x1E);
        w->x697 = 0;
        w->x6BB = 0;
        w->x4 = 5;
        SndStop(w->x5C4, 0);
        w->x5C4 = SndCall(6, 0x50, &em->pos, 0, 0, em);
        SndStop(w->x5B8, 0);
        SndStop(w->x5BC, 0);
        w->x5B8 = SndCall(6, 0x3D, &em->pos, 0, 0, em);
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            em10WalkRtnSet(em);
        } else if (em->seFlags28B & 1) {
            em10SetDamageDoor(em, 2);
            em10SetDamageRack(em, 2);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_C_SawHit(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    int flag;

    w->flags |= 0x800;
    Ctrl12Set(w->pCtrl12, 6, 30);
    Ctrl12Set(w->pCtrl12, 8, 120);
    em->dmg.set(0, 2);
    switch (em->xFE) {
    case 0:
        if (pG->x4FB8 != 2) {
            EmCatchPLSet(em, 0.0f, 2, (int) plem10_C_SawHit, -15.17f, 0.0f, 853.48f);
        } else {
            EmCatchPLSet(em, 0.0f, 2, (int) plem10_C_SawHit, 8.56f, 0.0f, 520.49f);
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xFB), 0, 5, 1, 0);
        w->x8 = 0;
        PlSetDamageSe(0);
        SndStop(w->x5C4, 0);
        w->x20 = SndCall(6, 0x4F, &em->pos, 0, 0, em);
        if (pSysS->region == 0 && (u8) (Rnd() % 10) > 4) {
            em->xFF = 1;
        } else {
            em->xFF = 3;
        }
        PlGachaInit();
        w->x4 = 19;
        w->x8 = 41;
        GameAddPoint(2);
        w->xC = 0;
        VibSetData((VibDataTbl*) (pGS->pArc->ofs_1C + (u32) pGS->pArc), 0xD, 1);
        em->xFE++;
    case 1:
        Ctrl12Set(w->pCtrl12, 9, 5);
        if (w->pWep) {
            if (w->xC) {
                w->xC--;
            } else {
                w->xC = 2;
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x50, 0, 0, (u32) w->pWep, 0);
            }
        }
        if (w->x4) {
            w->x4--;
        } else if (w->x8) {
            w->x8--;
        } else {
            pG->pl_life = 0;
        }
        if ((s16) pG->pl_life > 0) {
            if (w->x8) {
                w->x8--;
            } else {
                w->x8 = 10;
                if (w->pWep) {
                    v.x = 0.0f;
                    v.y = 0.0f;
                    v.z = 250.0f;
                    PSMTXMultVec(w->pWep->mat, &v, &v);
                    EstSet(0, -1, &v, &em->rot, 0x10, 0x10, 0, 0, 0, 0);
                }
            }
        } else {
            em->dmType = 2;
            em->xFE = 4;
            SndStop(w->x20, 0);
            SndCall(6, 0x54, &em->pos, 0, 0, em);
            break;
        }
        if (EmCatchMotionMove(em, 0.3f, 0.2f) || (u32) PlGachaGet() > 30) {
            em->dmType = 2;
            em->xFE++;
            SndStop(w->x20, 0);
            SndCall(6, 0x54, &em->pos, 0, 0, em);
        }
        break;
    case 2:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        em->atari.flags &= ~8;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xFE), (int) PL_ARC_PTR(em->subArc, 0xFF), 5, flag, 0);
        em->xFE++;
    case 3:
        em->dmType = 2;
        w->flags &= ~0x800;
        if (MotionMoveF(em, 0)) {
            w->x67C = 15;
            if (pG->x4F88 <= 3) {
                w->x67C = 45;
            }
            if (pG->x4F88 <= 1) {
                w->x67C = 90;
            }
            em10WalkRtnSet(em);
        }
        break;
    case 4:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        em->atari.flags &= ~8;
        w->x4 = 30;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xFD), 0, 5, flag, 0);
        em->xFE++;
    case 5:
        em->dmType = 2;
        if (w->x4) {
            w->x4--;
        } else {
            w->flags &= ~0x800;
        }
        MotionMoveF(em, 0);
        break;
    }
    em->x3A8 = em->pos;
    if (w->flags & 0x800) {
        em10CamMoveCri(em, em->xFF, 1);
    }
    w->flags |= 0x2000;
    em10SetCrash(em, 800.0f);
    em10HandSet(em, 0);
}

static void plem10_C_SawHit(cPlayer* pl)
{
    cEm* em;
    int end;
    int flag;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    pl->dmg.set(0, 2);
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x101), 0, 5, 1, 0);
        pl->atari.set(10, 400.0f, 700.0f);
        pl->x3E8 = SndCall(1, 0xC, &pPL->pos, 0, 0, pPL);
        if (pSysS->region == 0) {
            SndCall(6, 0x5C, &pPL->pos, 0, 0, pPL);
        }
        pl->xFE++;
    case 1:
        end = EmCatchMotionMove(pl, 0.3f, 0.2f);
        if ((s16) pG->pl_life <= 0) {
            pl->xFE = 4;
            break;
        }
        em = (cEm*) pPL->dmgType;
        if (!EM_RTN(em, 1, 0x30)) {
            EndPlDamage();
            SndStop(pl->x3E8, 0);
            pl->dmg.set(0, 30);
            break;
        }
        if (end || em->xFE == 2) {
            pl->xFE = 2;
        }
        break;
    case 2:
        flag = (((cEm*) pPL->dmgType)->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x102), 0, 5, flag, 0);
        SndStop(pl->x3E8, 0);
        pl->xFE++;
    case 3:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x4C), (int) PL_ARC_PTR(pG->pPlArc, 0x4D), 5, 1, 0);
        if (pSys->region == 0) {
            PlSetDamageSe(0xD);
            EstSet((int) pPL, -1, 0, 0, 0x10, 0x57, 0, 0, (u32) pPL, 0);
        }
        pl->x3E0 = 15;
        pl->xFE++;
    case 5:
        if (pl->frame > 71.7f && pl->frame < 72.3f) {
            EstSet((int) pl, -1, 0, 0, 0x10, 0x4D, 0, 0, (u32) pl, 0);
        }
        if (pl->x3E0 && --pl->x3E0 == 0 && pSys->region) {
            SndStop(pl->x3E8, 0);
            em10PlHeadLost();
        }
        MotionMoveF(pl, 0);
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em10_R1_br_C_SawCriAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->hp > 0 && (em->seFlags28B & 1) && w->pWep && em10CsawHitCk(em)) {
        if (fabsf(pPL->pos.y - em->pos.y) > 50.0f) {
            EM_RTN_SET(em, 1, 0x30);
        } else {
            EM_RTN_SET(em, 1, 0x32);
        }
    }
}

static void em10_R1_C_SawCriAtk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    if (w->flags & 0x100) {
        w->flags |= 0x40000;
    }
    switch (em->xFE) {
    case 0:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xEB), (int) PL_ARC_PTR(em->subArc, 0xEC), 10, flag, 0);
        w->x697 = 0;
        w->x6BB = 0;
        w->x4 = 5;
        SndStop(w->x5C4, 0);
        w->x5C4 = SndCall(6, 0x50, &em->pos, 0, 0, em);
        SndStop(w->x5B8, 0);
        SndStop(w->x5BC, 0);
        w->x5B8 = SndCall(6, 0x3D, &em->pos, 0, 0, em);
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x697 == 0) {
                GameAddPoint(0xB);
            }
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_C_SawCriHit(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->flags |= 0x800;
    Ctrl12Set(w->pCtrl12, 6, 30);
    Ctrl12Set(w->pCtrl12, 8, 120);
    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x100), 0, 5, 1, 0);
        if (pG->x4FB8 != 2) {
            EmCatchPLSet(em, 0.0f, 2, (int) plem10_C_SawCriHit, 24.27f, 0.0f, 805.87f);
        } else {
            EmCatchPLSet(em, 0.0f, 2, (int) plem10_C_SawCriHit, 24.27f, 0.0f, 805.87f);
        }
        w->x8 = 0;
        SndStop(w->x5C4, 0);
        w->x20 = SndCall(6, 0x4F, &em->pos, 0, 0, em);
        em->xFF = 1;
        w->xC = 0;
        w->x4 = 18;
        GameAddPoint(2);
        if (pSys->region == 0) {
            SndCall(6, 0x5C, &pPL->pos, 0, 0, pPL);
        }
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xD, 1);
        em->xFE++;
    case 1:
        Ctrl12Set(w->pCtrl12, 9, 5);
        if (w->x4) {
            w->x4--;
            if (w->pWep) {
                if (w->xC) {
                    w->xC--;
                } else {
                    w->xC = 2;
                    EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x4F, 0, 0, (u32) w->pWep, 0);
                }
            }
        }
        if (em->frame > 17.7f && em->frame < 18.3f) {
            SndStop(w->x20, 0);
            SndCall(6, 0x54, &em->pos, 0, 0, em);
        }
        if (EmCatchMotionMove(em, 0.3f, 0.2f)) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
        }
        break;
    }
    em->x3A8 = em->pos;
    if (w->flags & 0x800) {
        if (w->x4) {
            em10CamMoveCri(em, em->xFF, 1);
        } else {
            em10CamMoveCri(em, em->xFF, 0);
        }
    }
    em10HandSet(em, 0);
}

static void plem10_C_SawCriHit(cPlayer* pl)
{
    cEm* em;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    em = (cEm*) pl->dmgType;
    pl->subArc = em->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x103), 0, 5, 1, 0);
        pl->atari.set(10, 400.0f, 700.0f);
        pl->x3E8 = SndCall(1, 0xC, &pPL->pos, 0, 0, pPL);
        pl->xFE++;
    case 1:
        if (MOTION(pl)->seqFrame > 17.7f && MOTION(pl)->seqFrame < 18.3f) {
            pG->pl_life = 0;
            SndStop(pl->x3E8, 0);
            em10PlHeadLost();
        }
        if (MOTION(pl)->seqFrame > 76.7f && MOTION(pl)->seqFrame < 77.3f) {
            EstSetEm(pl, -1, 0, 0, 0x10, 0x4D, 0, 0, pl, 0);
        }
        if (MOTION(pl)->seqFrame > 39.7f && MOTION(pl)->seqFrame < 40.3f) {
            SndCall(5, 4, &pl->pos, 0, 0, pl);
        }
        if (MOTION(pl)->seqFrame > 70.7f && MOTION(pl)->seqFrame < 71.3f) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (EmCatchMotionMove(pl, 0.3f, 0.2f)) {
            pl->xFE++;
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
    if ((s16) pGS->pl_life <= 0) {
        SndStop(pl->x3E8, 0);
    }
}

static void em10_R1_br_Catch(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->hp > 0 && (em->seFlags28B & 2)) {
        if (em10CatchCk(em)) {
            if (pG->x4FB8 == 1) {
                EM_RTN_SET(em, 1, 0x36);
            } else if ((fabsf(Muku2(em->rot.y, pPL->rot.y, PI)) < 1.5707964f && (em10SomebodyNearCk(em) || pSUB)) || w->wepType == 9) {
                if (w->wepType == 9 && w->x640) {
                    EM_RTN_SET(em, 1, 0x38);
                } else {
                    EM_RTN_SET(em, 1, 0x37);
                }
            } else {
                EM_RTN_SET(em, 1, 0x34);
            }
        } else {
            em10CatchSubCk(em);
        }
    }
}

static void em10_R1_Catch(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    f32 a;

    switch (em->xFE) {
    case 0: {
        f32 ang;
        if (em->xFF && pSUB) {
            ang = Muku(&em->pos, &pSUB->pos, em->rot.y, PI);
        } else {
            ang = Muku(&em->pos, &pPL->pos, em->rot.y, PI);
        }
        a = fabsf(ang);
        if (a < 1.3089969f) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x86), (int) PL_ARC_PTR(em->subArc, 0x87), 10, 1, 0);
            w->x20 = 0;
        } else if (a < 1.9634955f) {
            if (ang < 0.0f) {
                w->x20 = 1;
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x88), (int) PL_ARC_PTR(em->subArc, 0x89), 10, 0x41, 0);
            } else {
                w->x20 = 2;
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x88), (int) PL_ARC_PTR(em->subArc, 0x89), 10, 1, 0);
            }
        } else {
            w->x20 = 3;
            if (ang < 0.0f) {
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x8A), (int) PL_ARC_PTR(em->subArc, 0x8B), 10, 0x41, 0);
            } else {
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x8A), (int) PL_ARC_PTR(em->subArc, 0x8B), 10, 1, 0);
            }
        }
        w->x18 = em->rot.y;
        switch ((u32) w->x20) {
        case 0:
        default:
            w->x18 = em->rot.y;
            break;
        case 1:
            w->x18 = em->rot.y - 1.5707964f;
            break;
        case 2:
            w->x18 = em->rot.y + 1.5707964f;
            break;
        case 3:
            w->x18 = em->rot.y + PI;
            break;
        }
        w->x18 = LIMIT_ANGLE(w->x18);
        w->x1C = 0.31415927f;
        if (pGS->x4F88 <= 3) {
            w->x1C = 0.10471976f;
        }
        em10CallVoiceSe2(em, w->se6CB, 8);
        em->xFE++;
    }
    case 1:
        if (em->seFlags28B & 8) {
            if (em->xFF && pSUB) {
                a = Muku(&em->pos, &pSUB->pos, w->x18, w->x1C);
            } else {
                a = Muku(&em->pos, &pPL->pos, w->x18, w->x1C);
            }
            w->x18 += a;
            w->x18 = LIMIT_ANGLE(w->x18);
            em->rot.y += a;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
                em->hp = 0;
            }
            if (em->hp <= 0) {
                EmRoutineSet(em, 2, 9, 0, 0);
            } else {
                GameAddPoint(0xB);
                em10WalkRtnSet(em);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_NeckHang(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int dmg;

    w->flags |= 0x800;
    em10SetAtkWait(em, 1);
    switch (em->xFE) {
    case 0:
        w->x24 = em->scale;
        em->scale.x = 1.0f;
        em->scale.y = 1.0f;
        em->scale.z = 1.0f;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x28F), 0, 5, 1, 0);
        PlSetDamageSe(0);
        if (pG->x4FB8 != 2) {
            EmCatchPLSet(em, 0.0f, 2, (int) plem10_NeckHang, -180.0f, 0.0f, 470.18f);
        } else {
            EmCatchPLSet(em, 0.0f, 2, (int) plem10_NeckHang, -172.81f, 0.0f, 428.66f);
        }
        em->dmg.set(0, 0);
        w->x4 = 0xF;
        w->x8 = 0x28;
        w->xC = (s16) pGS->pl_life;
        SndCall(8, 0x85, &em->pos, em->id, 0, em);
        w->x20 = SndCall(8, 0x85, &em->pos, em->id, 0, em);
        em->xFF = 0;
        PlGachaInit();
        GameAddPoint(2);
        em->xFE++;
    case 1:
        Ctrl12Set(w->pCtrl12, 9, 5);
        em10CamMove(em, em->xFF, 0.1f, 0);
        PlGachaMove();
        if (em->frame > 43.7f && em->frame < 44.3f) {
            SndStop(w->x20, 0);
            w->x20 = SndCall(8, 0x85, &em->pos, em->id, 0, em);
        }
        dmg = 10;
        dmg *= em10GetPower(em);
        if ((em->flags_3C8 & 4) && !(pG->flags_54 & 0x20)) {
            dmg = dmg / 2 + 1;
        }
        LifeDownSet2(pPL, dmg, 0, 1);
        if (EmCatchMotionMove(em, 0.3f, 0.2f) || (em->frame > 34.7f && em->frame < 35.3f)) {
            em->dmType = 2;
            if ((u32) PlGachaGet() <= 9 || ((s16) pG->pl_life <= 1 && w->xC <= 0xC7)) {
                if ((s16) pG->pl_life <= 1) {
                    pG->pl_life = 0;
                }
                em->xFE = 2;
            } else {
                em->xFE = 4;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x290), (int) PL_ARC_PTR(em->subArc, 0x291), 5, 1, 0);
        w->x4 = 0x30;
        SndStop(w->x20, 0);
        em10CallVoiceSe2(em, w->se6D3, 8);
        if (pG->room_id != 0x21B) {
            em10SetCampos2(em);
        }
        em->xFE++;
    case 3: {
        int r;
        em->dmType = 2;
        Ctrl12Set(w->pCtrl12, 9, 5);
        if (w->x4) {
            w->x4--;
            r = EmCatchMotionMove(em, 0.3f, 0.2f);
            if (w->x4 == 0) {
                QuakeExec(0, 0, 10, 8.0f, 2);
            }
        } else {
            r = MotionMoveF(em, 0);
        }
        if (r) {
            w->flags &= ~0x800;
            em->atari.flags &= ~8;
            if ((w->flags & 0x80) && w->x6C1 == 0 && em->hp == 1) {
                em->hp = 0;
            }
            if (em->hp <= 0) {
                EmRoutineSet(em, 2, 9, 0, 0);
            } else {
                em10WalkRtnSet(em);
            }
        } else if (pG->room_id != 0x21B) {
            em10CamMove2(em);
        } else {
            em10CamMove(em, em->xFF, 0.1f, 0);
        }
        break;
    }
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x292), (int) PL_ARC_PTR(em->subArc, 0x293), 5, 1, 0);
        w->x4 = 0xE;
        if (pG->x4FB8 == 2) {
            EmCatchPLSet(em, 0.0f, 2, (int) plem10_NeckHang, -280.09f, 0.0f, 364.05f);
            pPL->xFE = 4;
        }
        SndStop(w->x20, 0);
        dmg = 100;
        em->xFF = 1;
        w->x4 = 0x37;
        if (!(w->flags & 0x80)) {
            if ((u32) PlGachaGet() > 0x1E) {
                dmg = 9999;
            }
            if ((u32) PlGachaGet() > 0x14 && (Rnd() & 1)) {
                dmg = 9999;
            }
            if (pG->x4) {
                dmg = 9999;
            }
            if (w->flags & 0x80) {
                dmg = 0;
            }
            if (dmg == 9999) {
                GameAddPoint(9);
            }
        }
        LifeDownSet2(em, dmg, 0, 0);
        if (em->hp <= 0) {
            SndCall(1, 0x35, &pPL->pos, 0, 0, pPL);
        } else {
            SndCall(1, 0x3D, &pPL->pos, 0, 0, pPL);
            EstSet((int) em, -1, 0, 0, 0x10, 0x4B, 0, 0, (u32) em, 0);
        }
        EstSet((int) pPL, -1, 0, 0, 0x10, 0x4C, 0, 0, (u32) pPL, 0);
        pPL->dmType = 2;
        w->flags |= 0x20;
        em->xFE++;
    case 5:
        em->dmType = 2;
        if (w->x4) {
            w->x4--;
            em10CamMove(em, em->xFF, 1.0f, 0);
        } else {
            w->flags &= ~0x800;
            w->flags |= 0x2000;
        }
        if (MotionMoveF(em, 0)) {
            w->flags &= ~0x800;
            em->atari.flags &= ~8;
            em10SetAtkWait(em, 1);
            w->flags |= 0x20;
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        } else {
            if (em->seFlags28B & 2) {
                em10SetDmWaterEff(em, 1);
            }
            if (em->seFlags28B & 1) {
                if (!(w->flags & 0x80) && em->hp <= 0 && !em10ChgParasiteCk(em)) {
                    em10LostHead(em, 0, 1);
                }
                SndCall(1, 0x3A, &pPL->pos, 0, 0, pPL);
                SndCall(1, 0x3B, &pPL->pos, 0, 0, pPL);
                w->x6B1 = w->se6CE;
                em10SetDamageVoice(em, w->x6B1, w->se6C6);
            }
        }
        break;
    }
    em->x3A8 = em->pos;
    em10HandSet(em, 1);
    if (em->seFlags28B & 0x10) {
        em10SetCrash(em, 800.0f);
    }
}

static void plem10_NeckHang(cPlayer* pl)
{
    cEm* em;
    PlArc* arc;
    int end;
    int r;
    int dmg;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    em = (cEm*) pl->dmgType;
    arc = em->subArc;
    pl->subArc = arc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x297), 0, 5, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->pWep->setTrans(0, 0);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xC, 1);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 0.3f, 0.2f);
        em = (cEm*) pPL->dmgType;
        if (!EM_RTN(em, 1, 0x34)) {
            VibSetClearType(1);
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        } else {
            pl->xFE = em->xFE;
        }
        break;
    case 2:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x298), 0, 5, 1, 0);
        pl->x3E0 = 0x28;
        VibSetClearType(1);
        r = CheckInWater(pl, 0);
        if (r) {
            EstSet((int) pl, -1, 0, 0, 0x10, 0x1B, 0, 0, (u32) pl, 0);
            SndCall(6, 0x17, &pl->pos, 0, 0, pl);
        } else if (ChkWaterEffectEnable(&pl->pos)) {
            EstSet((int) pl, -1, 0, 0, 0x10, 0x2C, 0, 0, (u32) pl, 0);
        } else {
            EstSet((int) pl, -1, 0, 0, 0x10, 0x21, 0, 0, (u32) pl, 0);
        }
        if (pl->frame > 34.7f && pl->frame < 35.3f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xB, 1);
        }
        pl->xFE++;
    case 3:
        end = MotionMoveF(pl, 0);
        em = (cEm*) pPL->dmgType;
        if (!EM_RTN(em, 1, 0x34)) {
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
            break;
        }
        if (pl->frame > 4.7f && pl->frame < 5.3f) {
            SndCall(1, 9, &pl->pos, pl->id, 0, pl);
        }
        if (pl->frame > 36.7f && pl->frame < 37.3f) {
            dmg = 0xB4;
            dmg *= em10GetPower((cEm10*) pl);
            if ((pl->flags_3C8 & 4) && !(pG->flags_54 & 0x20)) {
                dmg = dmg / 2 + 1;
            }
            if ((s16) pG->pl_life > 0x32) {
                LifeDownSet2(pPL, dmg, 0, 1);
            } else {
                LifeDownSet2(pPL, dmg, 0, 0);
            }
            if (!CheckInWater(pl, 0)) {
                SndCall(5, 5, &pl->pos, pl->id, 0, pl);
                SndCall(1, 0x12, &pl->pos, pl->id, 0, pl);
                if ((s16) pG->pl_life <= 0) {
                    PlSetDamageSe(0xD);
                }
            }
        }
        if (pl->frame > 33.7f && pl->frame < 34.3f && CheckInWater(pl, 0)) {
            SndCall(6, 0x18, &pl->pos, 0, 0, pl);
        }
        if (end) {
            if ((s16) pG->pl_life > 0) {
                pl->pWep->setTrans(1, 0);
                EmRoutineSet(pPL, 1, 0, 10, 0);
            } else {
                pl->xFE = 6;
            }
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x299), 0, 5, 1, 0);
        pl->x3E0 = 0xE;
        VibSetClearType(1);
        pl->xFE++;
    case 5:
        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
            em = (cEm*) pPL->dmgType;
            if (!EM_RTN(em, 1, 0x34)) {
                pl->pWep->setTrans(1, 0);
                EndPlDamage();
                pl->dmg.set(0, 0x1E);
                break;
            }
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (end) {
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        } else {
            if (pl->frame > 29.7f && pl->frame < 30.3f && CheckInWater(pl, 0)) {
                EstSet((int) pl, -1, 0, 0, 1, 0x24, 0, 0, (u32) pl, 0);
            }
            if (((pl->frame > 34.7f && pl->frame < 35.3f) || (pl->frame > 39.7f && pl->frame < 40.3f)) && CheckInWater(pl, 0)) {
                EstSet((int) pl, -1, 0, 0, 1, 0x23, 0, 0, (u32) pl, 0);
            }
            if (pl->frame > 18.7f && pl->frame < 19.3f) {
                SndCall(1, 0x4F, &pPL->pos, 0, 0, pPL);
            }
            if (pl->frame > 37.7f && pl->frame < 38.3f) {
                SndCall(5, 0x14, &pPL->pos, 0, 0, pPL);
            }
        }
        break;
    case 6:
        MotionMoveF(pl, 0);
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em10_R1_NeckHang_Luis(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->flags |= 0x800;
    em10SetAtkWait(em, 1);
    switch (em->xFE) {
    case 0:
        w->x24 = em->scale;
        em->scale.x = 1.0f;
        em->scale.y = 1.0f;
        em->scale.z = 1.0f;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x28F), 0, 5, 1, 0);
        EmCatchSubSet(em, pSUB, 2, (int) subem10_NeckHang_Luis, 0.0f, -180.0f, 0.0f, 470.18f);
        em->dmg.set(0, 0);
        w->x4 = 0xF;
        w->x8 = 0x28;
        SndCall(8, 0x85, &em->pos, em->id, 0, em);
        w->x20 = SndCall(8, 0x85, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        Ctrl12Set(w->pCtrl12, 9, 5);
        if (em->frame > 43.7f && em->frame < 44.3f) {
            SndStop(w->x20, 0);
            w->x20 = SndCall(8, 0x85, &em->pos, em->id, 0, em);
        }
        if (EmCatchMotionMove(em, 0.3f, 0.2f)) {
            em->xFE = 2;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x292), (int) PL_ARC_PTR(em->subArc, 0x293), 5, 1, 0);
        w->x4 = 0xE;
        SndStop(w->x20, 0);
        w->flags |= 0x20;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            w->flags &= ~0x800;
            em->atari.flags &= ~8;
            em10SetAtkWait(em, 1);
            w->flags |= 0x20;
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        } else if (em->seFlags28B & 1) {
            SndCall(1, 0x3A, &pPL->pos, 0, 0, pPL);
            SndCall(1, 0x3B, &pPL->pos, 0, 0, pPL);
            w->x6B1 = w->se6CE;
            em10SetDamageVoice(em, w->x6B1, w->se6C6);
        }
        break;
    }
    em->x3A8 = em->pos;
    em10HandSet(em, 1);
    if (em->seFlags28B & 0x10) {
        em10SetCrash(em, 800.0f);
    }
}

static void subem10_NeckHang_Luis(cSubChar* sub)
{
    cSubChar* s = pSUB;
    PlArc* arc;

    BitOn(pG->flags_5010, 0x10000);
    BitOn(pG->flags_5014, 0x20000000);
    s->dmg.set(0, 10);
    arc = ((cEm*) s->dmgType)->subArc;
    s->subArc = arc;
    switch (s->xFE) {
    case 0:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(arc, 0x297), 0, 5, 1, 0);
        SubCharSetFace(1);
        s->atari.set(10, 480.00003f, 400.0f);
        SndCall(8, 9, &s->pos, s->id, 0, s);
        s->xFE++;
    case 1:
        EmCatchMotionMove(s, 0.3f, 0.2f);
        if (((cEm*) s->dmgType)->xFC != 1 && ((cEm*) s->dmgType)->xFD != 0x34) {
            EndSubDamage();
            s->dmg.set(0, 0x1E);
        } else {
            s->xFE = ((cEm*) s->dmgType)->xFE;
        }
        break;
    case 2:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(arc, 0x299), 0, 5, 1, 0);
        SndCall(8, 0x11, &s->pos, s->id, 0, s);
        s->subHideMode = 0xF;
        s->xFE++;
    case 3:
        if (MotionMoveF(s, 0)) {
            EndSubDamage();
            s->dmg.set(0, 0x1E);
        } else if (s->subHideMode) {
            s->subHideMode--;
            if (((cEm*) s->dmgType)->xFC != 1 && ((cEm*) s->dmgType)->xFD != 0x34) {
                EndSubDamage();
                s->dmg.set(0, 0x1E);
            }
        }
        break;
    }
    s->x3A8 = s->pos;
    s->subArc = s->subArc2;
}

static void em10_R1_NeckHang_Ashley(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int dmg;
    int r;

    w->flags |= 0x800;
    em10SetAtkWait(em, 1);
    switch (em->xFE) {
    case 0:
        w->x24 = em->scale;
        em->scale.x = 1.0f;
        em->scale.y = 1.0f;
        em->scale.z = 1.0f;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1A8), 0, 5, 1, 0);
        PlSetFace(1);
        EmCatchPLSet(em, 0.0f, 2, (int) subem10_NeckHang_Ashley, -150.33f, 0.0f, 415.26f);
        em->dmg.set(0, 0);
        w->x8 = 0x28;
        w->x4 = 0xF;
        w->xC = (s16) pGS->pl_life;
        SndCall(8, 0x85, &em->pos, em->id, 0, em);
        w->x20 = SndCall(8, 0x85, &em->pos, em->id, 0, em);
        SndCall(1, 7, &em->pos, 0, 0, em);
        em->xFF = 0;
        PlGachaInit();
        GameAddPoint(2);
        w->x4 = 0x28;
        em->xFE++;
    case 1:
        Ctrl12Set(w->pCtrl12, 9, 5);
        em10CamMove(em, em->xFF, 0.1f, 0);
        if (em->frame > 39.7f && em->frame < 40.3f) {
            SndStop(w->x20, 0);
            w->x20 = SndCall(8, 0x85, &em->pos, em->id, 0, em);
        }
        if (w->x4) {
            w->x4--;
        } else {
            PlGachaMove();
            dmg = 10;
            if ((em->flags_3C8 & 4) && !(pG->flags_54 & 0x20)) {
                dmg = 6;
            }
            LifeDownSet2(pPL, dmg, 0, 0);
        }
        if (EmCatchMotionMove(em, 0.3f, 0.2f) || (u32) PlGachaGet() > 10 || (s16) pG->pl_life <= 0) {
            em->dmType = 2;
            if ((s16) pG->pl_life <= 0) {
                em->xFE = 2;
            } else {
                em->xFE = 4;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1AB), 0, 5, 1, 0);
        SndStop(w->x20, 0);
        em10CallVoiceSe2(em, w->se6D3, 8);
        SndCall(1, 0xD, &em->pos, 0, 0, em);
        em->xFF = Rnd() & 1;
        em->xFE++;
    case 3:
        em10CamMoveAshley(em, em->xFF);
        em->dmType = 2;
        MotionMoveF(em, 0);
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x1A9), (int) PL_ARC_PTR(em->subArc, 0x1AA), 5, 1, 0);
        w->x4 = 0xE;
        SndStop(w->x20, 0);
        em->xFF = (Rnd() & 1) + 1;
        w->x4 = 0x37;
        SndCall(1, 0x35, &em->pos, 0, 0, em);
        pPL->dmType = 2;
        em->xFE++;
    case 5:
        em->dmType = 2;
        if (w->x4) {
            w->x4--;
            em10CamMoveAshley(em, em->xFF);
        }
        if (w->x4) {
            w->x4--;
            r = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            r = MotionMoveF(em, 0);
        }
        if (r) {
            w->flags &= ~0x800;
            em->atari.flags &= ~8;
            em10SetAtkWait(em, 1);
            w->flags |= 0x20;
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        } else {
            if (em->seFlags28B & 2) {
                em10SetDmWaterEff(em, 1);
            }
            if (em->seFlags28B & 1) {
                SndCall(1, 0x3A, &em->pos, 0, 0, em);
            }
        }
        break;
    }
    em->x3A8 = em->pos;
    em10HandSet(em, 1);
    if (em->seFlags28B & 0x10) {
        em10SetCrash(em, 800.0f);
    }
}

// Ashley as the player: the routine takes the sub-char slot but runs on the player fields.
static void subem10_NeckHang_Ashley(cSubChar* sub)
{
    cPlayer* pl = (cPlayer*) sub;
    cEm* em;
    PlArc* arc;
    int end;
    int r;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    arc = ((cEm*) pl->dmgType)->subArc;
    pl->subArc = arc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x1AC), 0, 5, 1, 0);
        pl->atari.set(10, 480.00003f, 400.0f);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xC, 1);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 0.3f, 0.2f);
        em = (cEm*) pPL->dmgType;
        if (!EM_RTN(em, 1, 0x36)) {
            VibSetClearType(1);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        } else {
            pl->xFE = em->xFE;
        }
        break;
    case 2:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x1AE), 0, 5, 1, 0);
        pl->x3E0 = 0x28;
        r = CheckInWater(pl, 0);
        if (r) {
            EstSet((int) pl, -1, 0, 0, 0x10, 0x1B, 0, 0, (u32) pl, 0);
            SndCall(6, 0x17, &pl->pos, 0, 0, pl);
        } else if (ChkWaterEffectEnable(&pl->pos)) {
            EstSet((int) pl, -1, 0, 0, 0x10, 0x2C, 0, 0, (u32) pl, 0);
        } else {
            EstSet((int) pl, -1, 0, 0, 0x10, 0x21, 0, 0, (u32) pl, 0);
        }
        VibSetClearType(1);
        pl->xFE++;
    case 3:
        MotionMoveF(pl, 0);
        if (pl->frame > 29.7f && pl->frame < 30.3f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xB, 1);
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x1AD), 0, 5, 1, 0);
        pl->x3E0 = 0xE;
        VibSetClearType(1);
        pl->xFE++;
    case 5:
        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
            em = (cEm*) pPL->dmgType;
            if (!EM_RTN(em, 1, 0x36)) {
                EndPlDamage();
                pl->dmg.set(0, 0x1E);
                break;
            }
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (end) {
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em10_R1_Backhold(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModel* p = pPL->getPartsPtr(4);
    int dmg;

    w->flags |= 0x800;
    em10SetAtkWait(em, 0);
    switch (em->xFE) {
    case 0:
        Ctrl12Set(w->pCtrl12, 6, 0);
        Ctrl12Set(w->pCtrl12, 8, 0);
        Ctrl12Set(w->pCtrl12, 9, 0);
        w->x24 = em->scale;
        em->scale.x = 1.0f;
        em->scale.y = 1.0f;
        em->scale.z = 1.0f;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x294), 0, 5, 1, 0);
        PlSetDamageSe(0);
        if (pG->x4FB8 != 2) {
            EmCatchPLSet(em, PI, 2, (int) plem10_Backhold, 178.63f, 0.0f, -190.03f);
        } else {
            EmCatchPLSet(em, PI, 2, (int) plem10_Backhold, 178.53f, 0.0f, -190.03f);
        }
        pG->flags_5010 |= 0x8000;
        em->dmg.set(0, 0);
        w->x4 = 0xF;
        if (w->x640) {
            w->x640 = 0x1E;
        }
        SndCall(8, 0x85, &p->worldPos, em->id, 0, pPL);
        w->x20 = SndCall(8, 0x85, &p->worldPos, em->id, 0, pPL);
        em->xFF = 0;
        PlGachaInit();
        pG->flags_5010 |= 0x2000;
        w->x8 = 0x2D;
        em->xFE++;
    case 1:
        em10CamMove(em, em->xFF, 0.1f, 0);
        PlGachaMove();
        if (w->x8) {
            w->x8--;
        }
        if (EmCatchMotionMove(em, 0.3f, 0.2f) || w->x8 == 0) {
            em->dmType = 2;
            em->xFE = 2;
        } else if (!(pG->flags_5010 & 0x2000)) {
            em->xFE = 4;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x295), (int) PL_ARC_PTR(em->subArc, 0x296), 5, 1, 0);
        dmg = 100;
        SndStop(w->x20, 0);
        if (!(w->flags & 0x80)) {
            if ((u32) PlGachaGet() > 0x28) {
                dmg = 9999;
            }
            if ((u32) PlGachaGet() > 0x1E && (Rnd() & 1)) {
                dmg = 9999;
            }
            if (pG->x4) {
                dmg = 9999;
            }
            if (w->flags & 0x80) {
                dmg = 0;
            }
            if (dmg == 9999) {
                GameAddPoint(9);
            }
        }
        LifeDownSet2(em, dmg, 0, 0);
        if (em->hp <= 0) {
            SndCall(1, 0x35, &p->worldPos, 0, 0, pPL);
        } else {
            SndCall(1, 0x3D, &p->worldPos, 0, 0, pPL);
            EstSet((int) em, -1, 0, 0, 0x10, 0x4A, 0, 0, (u32) em, 0);
        }
        EstSet((int) pPL, -1, 0, 0, 0x10, 0x4C, 0, 0, (u32) pPL, 0);
        pPL->dmType = 2;
        em->xFE++;
    case 3:
        em->dmType = 2;
        if (MotionMoveF(em, 0)) {
            w->flags &= ~0x800;
            em->atari.flags &= ~8;
            em10SetAtkWait(em, 1);
            w->flags &= ~0x20;
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        } else {
            if (em->seFlags28B & 2) {
                em10SetDmWaterEff(em, 1);
            }
            if (em->seFlags28B & 1) {
                if (!(w->flags & 0x80) && em->hp <= 0 && !em10ChgParasiteCk(em)) {
                    em10LostHead(em, 0, 1);
                }
                SndCall(1, 0x3A, &p->worldPos, 0, 0, pPL);
                SndCall(1, 0x3B, &p->worldPos, 0, 0, pPL);
                w->x6B1 = w->se6CE;
                em10SetDamageVoice(em, w->x6B1, w->se6C6);
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x57), (int) PL_ARC_PTR(em->subArc, 0x58), 5, 1, 0);
        SndStop(w->x20, 0);
        w->flags &= ~0x800;
        em->atari.flags &= ~8;
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em10SetAtkWait(em, 0);
            em10WalkRtnSet(em);
        }
        break;
    }
    em->x3A8 = em->pos;
    em10HandSet(em, 1);
    if (em->seFlags28B & 0x10) {
        em10SetCrash(em, 800.0f);
    }
}

static void plem10_Backhold(cPlayer* pl)
{
    cEm* em;
    PlArc* arc;

    BitOn(pG->flags_5010, 0x8000);
    BitOn(pG->flags_5010, 0x2000);
    em = (cEm*) pl->dmgType;
    arc = em->subArc;
    pl->subArc = arc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x29A), 0, 5, 1, 0);
        PlSetFace(1);
        pl->pWep->setTrans(0, 0);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->dmg.set(0, 10);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 0.3f, 0.2f);
        em = (cEm*) pPL->dmgType;
        if (!EM_RTN(em, 1, 0x37)) {
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        } else {
            pl->xFE = em->xFE;
        }
        break;
    case 2:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x29B), 0, 5, 1, 0);
        pl->xFE++;
    case 3:
        pl->dmg.x1 = 2;
        if (MotionMoveF(pl, 0)) {
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em10_R1_Bombhold(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModel* p;
    Camera* cam;
    Vec rot;

    w->flags |= 0x800;
    em10SetAtkWait(em, 1);
    switch (em->xFE) {
    case 0:
        w->x24 = em->scale;
        em->scale.x = 1.0f;
        em->scale.y = 1.0f;
        em->scale.z = 1.0f;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x294), 0, 5, 1, 0);
        PlSetDamageSe(0);
        if (pG->x4FB8 != 2) {
            EmCatchPLSet(em, PI, 2, (int) plem10_Bombhold, 178.63f, 0.0f, -190.03f);
        } else {
            EmCatchPLSet(em, PI, 2, (int) plem10_Bombhold, 178.53f, 0.0f, -190.03f);
        }
        pG->flags_5010 |= 0x8000;
        em->dmg.set(0, 0);
        w->x4 = 0xF;
        if (w->x640) {
            w->x640 = 0x78;
        }
        SndCall(8, 0x85, &em->pos, em->id, 0, em);
        w->x20 = SndCall(8, 0x85, &em->pos, em->id, 0, em);
        em->xFF = 0;
        PlGachaInit();
        pG->flags_5010 |= 0x2000;
        w->x8 = 0x50;
        em->xFE++;
    case 1:
        em10CamMove(em, em->xFF, 0.1f, 0);
        PlGachaMove();
        if (w->x8) {
            w->x8--;
            if (w->x8 == 0) {
                if (w->x640) {
                    w->x640 = 0;
                }
                if (w->pWep) {
                    w->pWep->setLost();
                    w->pWep = 0;
                    w->wepType = 0;
                }
                em10CoreBreak(em, 1);
                if (w->x58C) {
                    w->x58C->setReset();
                    w->x58C = 0;
                }
                em->hp = 0;
                em->atari.flags = (em->atari.flags & ~0x300) | 0x10;
                EmSetDie(em);
                EmReserveDropItem(em);
                em10SetPoint(em);
                em->clearStatus(5);
                em->setStatus(8);
                SndStop(w->x20, 0);
                SndCall(8, 0x96, &em->pos, em->id, 0, em);
                cam = &pG->Cam;
                p = em->getPartsPtr(0);
                if ((cam->param.pos.x - p->worldPos.x) * (cam->param.pos.x - p->worldPos.x) +
                        (cam->param.pos.y - p->worldPos.y) * (cam->param.pos.y - p->worldPos.y) +
                        (cam->param.pos.z - p->worldPos.z) * (cam->param.pos.z - p->worldPos.z) <
                    4000000.0f) {
                    rot.x = 0.0f;
                    rot.y = GetXZAngle(&p->worldPos, &cam->param.pos);
                    rot.z = 0.0f;
                    EstSet(0, -1, &em->pos, &rot, 0x10, 0x47, 0, 0, 0, 0);
                } else {
                    EstSet((int) em, -1, 0, 0, 0x10, 0x30, 0, 0, (u32) em, 0);
                }
                w->x4 = 3;
                MotionMoveF(em, 0);
                em->xFE = 2;
                break;
            }
        }
        EmCatchMotionMove(em, 0.3f, 0.2f);
        if ((u32) PlGachaGet() > 0xF) {
            em->xFE = 4;
        }
        break;
    case 2:
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
            if (w->x4 == 0) {
                w->x6B7 = 1;
                w->flags |= 0x400000;
                em->be_flag &= ~2;
            }
        }
        em10CamMove(em, em->xFF, 0.1f, 0);
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x295), (int) PL_ARC_PTR(em->subArc, 0x296), 5, 1, 0);
        SndStop(w->x20, 0);
        SndCall(1, 0x3D, &pPL->pos, 0, 0, pPL);
        EstSet((int) em, -1, 0, 0, 0x10, 0x4A, 0, 0, (u32) em, 0);
        EstSet((int) pPL, -1, 0, 0, 0x10, 0x4C, 0, 0, (u32) pPL, 0);
        pPL->dmType = 2;
        em->xFE++;
    case 5:
        em->dmType = 2;
        if (MotionMoveF(em, 0)) {
            w->flags &= ~0x800;
            em->atari.flags &= ~8;
            em10SetAtkWait(em, 1);
            w->flags &= ~0x20;
            EmRoutineSet(em, 1, 0x1E, 0, 0);
        } else {
            if (em->seFlags28B & 1) {
                cModel* q = pPL->getPartsPtr(4);
                SndCall(1, 0x3A, &q->worldPos, 0, 0, pPL);
                SndCall(1, 0x3B, &q->worldPos, 0, 0, pPL);
                w->x6B1 = w->se6CE;
                em10SetDamageVoice(em, w->x6B1, w->se6C6);
            }
            if (em->seFlags28B & 2) {
                em10SetDmWaterEff(em, 1);
            }
        }
        break;
    }
    em->x3A8 = em->pos;
    em10HandSet(em, 1);
    if (em->seFlags28B & 0x10) {
        em10SetCrash(em, 800.0f);
    }
}

static void plem10_Bombhold(cPlayer* pl)
{
    cEm* em;
    PlArc* arc;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    em = (cEm*) pl->dmgType;
    arc = em->subArc;
    pl->subArc = arc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x29A), 0, 5, 1, 0);
        PlSetFace(1);
        pl->pWep->setTrans(0, 0);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 0.3f, 0.2f);
        if (!EM_RTN((cEm*) pPL->dmgType, 1, 0x38)) {
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        } else {
            pl->xFE = ((cEm*) pl->dmgType)->xFE;
        }
        break;
    case 2:
        pG->pl_life = 0;
        EstSetEm(pl, -1, 0, 0, 0x10, 0x4E, 0, 0, pl, 0);
        pl->x3E0 = 3;
        pl->xFE++;
    case 3:
        EmCatchMotionMove(pl, 0.3f, 0.2f);
        if (pl->x3E0) {
            pl->x3E0--;
            if (pl->x3E0 == 0) {
                pl->be_flag &= ~2;
            }
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(arc, 0x29B), 0, 5, 1, 0);
        pl->xFE++;
    case 5:
        if (MotionMoveF(pl, 0)) {
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em10_R1_br_DashCatch(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->hp > 0 && (em->seFlags28B & 2) && em10CatchCk(em)) {
        if (fabsf(Muku2(em->rot.y, pPL->rot.y, PI)) < 1.5707964f && (em10SomebodyNearCk(em) || pSUB || w->wepType == 9)) {
            if (w->wepType == 9 && w->x640) {
                EM_RTN_SET(em, 1, 0x38);
            } else {
                EM_RTN_SET(em, 1, 0x37);
            }
        } else {
            EM_RTN_SET(em, 1, 0x34);
        }
    }
}

static void em10_R1_DashCatch(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int end;
    int r;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x8E), (int) PL_ARC_PTR(em->subArc, 0x8F), 10, 1, 0);
        em->xFF = 0;
        em10CallVoiceSe2(em, w->se6CB, 8);
        w->x4 = 15;
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        end = MotionMoveF(em, 0);
        if (end) {
            r = Rnd();
            w->x664 = r % 300 + 300;
            GameAddPoint(0xB);
            em10WalkRtnSet(em);
        } else if ((em->seFlags28B & 1) && CheckInWater(em, 0)) {
            EstSetEm(em, -1, 0, 0, 1, 0x33, 0, 0, em, 0);
            SndCall(6, 0x16, &em->pos, 0, 0, em);
        } else if ((em->seFlags28B & 0x20) && CheckInWater(em, 0)) {
            EstSetEm(em, -1, 0, 0, 1, 0x34, 0, 0, em, 0);
            SndCall(6, 0x11, &em->pos, 0, 0, em);
        }
        break;
    }
    em10HandSet(em, 0);
}

// Water entry effect of the taken-away Ganado (once per fall, x20 flags it; em10_R1_TakeAway).
#define EM10_FALL_WATER_EFFECT                                                                         \
    w->x20 = 1;                                                                                        \
    if (pG->room_id == 0x311) {                                                                        \
        EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);                                                  \
        SndCall(6, 0xA, &em->pos, 0, 0, em);                                                           \
    } else {                                                                                           \
        EstSetEm10WaterFall((Vec*) em);                                                                \
        SndCall(6, 0x16, &em->pos, 0, 0, em);                                                          \
    }

static void em10_R1_TakeAway(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int r;
    u32 i;
    Vec v;

    w->flags |= 0x4800;
    switch (em->xFE) {
    case 0:
        w->x24 = em->scale;
        em->scale.x = 1.0f;
        em->scale.y = 1.0f;
        em->scale.z = 1.0f;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x29F), 0, 5, 1, 0);
        EmCatchSubSet(em, pSUB, 2, (int) subem10_TakeAway, 0.0f, -194.22f, 0.0f, 582.05f);
        em->dmg.set(0, 0);
        em->flags_3C8 &= ~0x400;
        w->x8 = 0x28;
        em10SetTakeawayPos(em);
        em10SetTakeawayPosUpdate(em);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        if (em->x3D0 == 5) {
            em->x3D0 = 0;
        }
        w->x4 = 10;
        em->xFE++;
    case 1: {
        int end;
        w->flags |= 0x4000000;
        if (w->x4) {
            w->x4--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (end) {
            em->xFE++;
        }
        break;
    }
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2A0), 0, 5, 5, 0);
        em->xFE++;
    case 3:
        w->flags |= 0x4000000;
        em10SetTakeawayPosUpdate(em);
        em->rot.y += Muku(&em->pos, &w->x54C, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (!(pG->flags_5010 & 0x10000)) {
            em10WalkRtnSet(em);
            break;
        }
        switch ((u32) em10WindowCk2(em)) {
        case 0:
        case 3:
        case 4:
        default:
            break;
        case 1:
            em->xFF = 1;
            em->xFE = 4;
            return;
        case 2:
            em->xFE = 6;
            return;
        }
        em10DoorOpenCk(em, 0);
        em10RackBreakCk(em);
        switch ((u32) em10ClimbOverCk2(em)) {
        case 0:
        default:
            break;
        case 1:
            em->xFE = 4;
            return;
        case 2:
            em->xFE = 6;
            em->xFF = 1;
            return;
        }
        if (em10JumpDownCk2(em)) {
            em->xFE = 6;
            em->xFF = 0;
            return;
        }
        if ((em->pos.x - w->x4EC.x) * (em->pos.x - w->x4EC.x) + (em->pos.z - w->x4EC.z) * (em->pos.z - w->x4EC.z) <
            640000.0f) {
            em->xFE = 0xE;
            break;
        }
        if (!(pG->flags_5010 & 0x10000)) {
            em10WalkRtnSet(em);
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x92), (int) PL_ARC_PTR(em->subArc, 0x93), 10, 1, 0);
        PSVECSubtract(&w->x5E0, &em->pos, &w->x5E0);
        w->x5E0.y = 0.0f;
        em->xFE++;
    case 5:
        em->setStatus(3);
        PSVECScale(&w->x5E0, &v, 0.2f);
        PSVECAdd(&em->pos, &v, &em->pos);
        PSVECSubtract(&w->x5E0, &v, &w->x5E0);
        if (em->seFlags28B & 4) {
            w->flags |= 0x20000;
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 2;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x94), (int) PL_ARC_PTR(em->subArc, 0x95), 3, 1, 0);
        w->x20 = 0;
        em->xFE++;
    case 7:
        em->dmType = 2;
        w->flags |= 0x10080000;
        em->setStatus(3);
        MotionMoveF(em, 0);
        if (w->x20 == 0) {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                EM10_FALL_WATER_EFFECT
            }
        }
        if (!(em->seFlags28B & 0x40)) {
            f32 y;
            v = em->pos;
            v.y = em->oldPos.y;
            y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
            if (!(em->pos.y > y)) {
                em->pos.y = y;
                w->x5A4.y = 0.0f;
                em->xFE++;
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x96), 0, 3, 1, 0);
                MotionMoveF(em, 0);
            }
        }
        break;
    case 8:
        SndCall(8, 0x77, &em->pos, em->id, 0, em);
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x96), 0, 3, 1, 0);
        r = CheckInWater(em, 0);
        if (r) {
            em10FallWaterCk(em);
            if (w->x20 == 0) {
                EM10_FALL_WATER_EFFECT
            }
        } else if (ChkWaterEffectEnable(&em->pos)) {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x31, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x17, 0, 0, 0, 0);
        }
        em->xFE++;
    case 9:
        if (MotionMoveF(em, 0)) {
            em->xFE = 2;
        }
        break;
    case 0xE:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x97), 0, 3, 5, 0);
        w->xC = 0x1E;
        em->xFE++;
    case 0xF:
        MotionMoveF(em, 0);
        if (!(pG->flags_5010 & 0x10000)) {
            em10WalkRtnSet(em);
            break;
        }
        if (w->xC) {
            w->xC--;
            if (w->xC == 0) {
                SceEventStart(0);
                pPL->dmg.set(0, 0x80);
                pPL->setNoSuspend(1);
                em->setNoSuspend(1);
                em->dmType = 0x80;
                pG->sub_life = 0;
                em->atari.throughOn();
                if (pSUBS) {
                    pSUBS->atari.throughOn();
                    pSUBS->setNoSuspend(1);
                }
                em->xFE++;
            }
        }
        break;
    case 0x10: {
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2A0), 0, 5, 5, 0);
        EffectDeleteAll();
        {
            Vec v2 = {0.0f, 0.0f, 2000.0f};
            Vec rot = em->rot;
            PSMTXMultVec(em->mat, &v2, &v2);
            EstSet(0, -1, &v2, &rot, 4, 0, 1, 0, 0, 0);
        }
        BitOn(pG->flags_58, 0x8000000);
        pPL->setNoSuspend(0);
        BitOff(pG->flags_6C, 0x2000);
        BitOn(pG->flags_5010, 0x40);
        bio4_GXSetCopyClear(GXColor(), 0xFFFFFF);
        for (i = 0; i < EmMgr.nArray; i++) {
            cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
            if (e && e != em && pSUB && e != pSUB && e->isAlive()) {
                e->setNoSuspend(0);
            }
        }
        for (i = 0; i < ObjMgr.nArray; i++) {
            cObj* o = (cObj*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);
            if (o && o->isAlive()) {
                o->setNoSuspend(0);
            }
        }
        w->xC = 0x32;
        em->xFE++;
    }
    case 0x11:
        if (w->xC) {
            w->xC--;
        } else {
            em->alpha -= 0.04f;
            if (em->alpha < 0.0f) {
                em->alpha = 0.0f;
            }
            if (pSUB) {
                FSet(pSUB->alpha, pSUB->alpha - 0.04f);
                if (pSUB->alpha < 0.0f) {
                    pSUB->alpha = 0.0f;
                }
            }
        }
        MotionMoveF(em, 0);
        em10CamMoveTakeaway(em);
        break;
    }
    em->x3A8 = em->pos;
    em10HandSet(em, 0);
}
#undef EM10_FALL_WATER_EFFECT

// Ashley carried off: the sub follows the Ganado's hold position, retrying the scream timer (x534/x538).
#define SUB_TAKEAWAY_POS(X, Z)                                                                         \
    {                                                                                                  \
        Vec v;                                                                                         \
        v.x = X;                                                                                       \
        v.y = 0.0f;                                                                                    \
        v.z = Z;                                                                                       \
        PSMTXMultVec(((cEm*) s->dmgType)->mat, &v, &s->pos);                                           \
    }                                                                                                  \
    s->rot.y = ((cEm*) s->dmgType)->rot.y + PI;                                                        \
    s->rot.y = LIMIT_ANGLE(s->rot.y);
#define SUB_TAKEAWAY_HOLD_CK ((u32) (((cEm*) s->dmgType)->xFC - 2) <= 1)
#define SUB_TAKEAWAY_SCREAM                                                                            \
    {                                                                                                  \
        int t = s->subX534;                                                                            \
        if (t) {                                                                                       \
            s->subX534 = t - 1;                                                                        \
        } else {                                                                                       \
            s->subX534 = (u8) (Rnd() % 30) + 60;                                                       \
            if (s->sub538) {                                                                           \
                s->sub538 = t;                                                                         \
                SndCall(8, 1, &s->pos, s->id, 0, s);                                                 \
            } else {                                                                                   \
                s->sub538 = 1;                                                                         \
                SndCall(8, 2, &s->pos, s->id, 0, s);                                                 \
            }                                                                                          \
        }                                                                                              \
    }                                                                                                  \
    s->xFE = ((cEm*) s->dmgType)->xFE;

static void subem10_TakeAway(cSubChar* sub)
{
    cSubChar* s = pSUB;
    int r;

    s->subArc = ((cEm*) s->dmgType)->subArc;
    BitOn(pGS->flags_5010, 0x10000);
    BitOn(pG->flags_5014, 0x20000000);
    switch (s->xFE) {
    case 0:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x2A2), 0, 5, 1, 0);
        s->atari.setFlag100();
        s->atari.clrFlag200();
        {
            int no;
            if (Rnd() & 1) {
                no = 0;
            } else {
                no = 3;
            }
            SndCall(8, no, &s->pos, s->id, 0, s);
        }
        s->subHideMode = 10;
        s->xFE++;
    case 1:
        s->atari.setFlag100();
        s->atari.clrFlag200();
        if (s->subHideMode) {
            s->subHideMode--;
            r = EmCatchMotionMove(s, 0.3f, 0.2f);
        } else {
            r = MotionMoveF(s, 0);
        }
        if (em10DeadCk((cEm*) s->dmgType)) {
            EndSubDamage();
        }
        if (r) {
            s->xFE++;
        }
        break;
    case 2:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x2A3), 0, 5, 4, 0);
        s->subHideMode = 0x28;
        s->subX534 = 0x3C;
        s->sub538 = 0;
        s->xFE++;
    case 3:
        s->atari.setFlag100();
        s->atari.clrFlag200();
        SUB_TAKEAWAY_POS(-176.17f, 40.95f)
        MotionMoveF(s, 0);
        if (SUB_TAKEAWAY_HOLD_CK) {
            s->xFE = 0xA;
        } else {
            SUB_TAKEAWAY_SCREAM
        }
        break;
    case 4:
        SUB_TAKEAWAY_POS(-176.17f, -79.17f)
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x9E), 0, 5, 1, 0);
        s->subHideMode = 0x28;
        s->subX534 = 0x3C;
        s->sub538 = 0;
        s->xFE++;
    case 5:
        s->atari.throughOn();
        MotionMoveF(s, 0);
        if (SUB_TAKEAWAY_HOLD_CK) {
            s->xFE = 0xA;
        } else {
            SUB_TAKEAWAY_SCREAM
        }
        break;
    case 6:
        SUB_TAKEAWAY_POS(-184.08f, 40.78f)
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x9F), 0, 5, 1, 0);
        s->subHideMode = 0x28;
        s->subX534 = 0x3C;
        s->sub538 = 0;
        s->xFE++;
    case 7:
        s->atari.throughOn();
        s->dmType = 2;
        MotionMoveF(s, 0);
        if (SUB_TAKEAWAY_HOLD_CK) {
            s->xFE = 0xA;
        } else {
            SUB_TAKEAWAY_SCREAM
        }
        break;
    case 8:
        SUB_TAKEAWAY_POS(-147.03f, 219.32f)
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0xA0), 0, 5, 1, 0);
        s->subHideMode = 0x28;
        s->subX534 = 0x3C;
        s->sub538 = 0;
        s->xFE++;
    case 9:
        s->atari.throughOn();
        MotionMoveF(s, 0);
        if (SUB_TAKEAWAY_HOLD_CK) {
            s->xFE = 0xA;
        } else {
            int t = s->subX534;
            if (t) {
                s->subX534 = t - 1;
            } else {
                s->subX534 = (u8) (Rnd() % 30) + 60;
                if ((s16) pGS->sub_life > 0) {
                    if (s->sub538) {
                        s->sub538 = t;
                        SndCall(8, 1, &s->pos, s->id, 0, s);
                    } else {
                        s->sub538 = 1;
                        SndCall(8, 2, &s->pos, s->id, 0, s);
                    }
                }
            }
            s->xFE = ((cEm*) s->dmgType)->xFE;
        }
        break;
    case 0xA:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x2A4), 0, 5, 1, 0);
        s->atari.throughOff();
        if (ChkWaterEffectEnable(&s->pos)) {
            EstSet((int) s, -1, 0, 0, 4, 0xC, 0, 0, (u32) s, 0);
        } else {
            EstSet((int) s, -1, 0, 0, 4, 0xB, 0, 0, (u32) s, 0);
        }
        s->xFE++;
    case 0xB:
        if (MotionMoveF(s, 0)) {
            s->xFE++;
        }
        break;
    case 0xC:
        s->subArc = s->subArc2;
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x33), 0, 0, 1, 0);
        s->xFE++;
    case 0xD:
        if (MotionMoveF(s, 0)) {
            EndSubDamage();
        }
        break;
    case 0xE:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x2A3), 0, 3, 1, 0);
        s->xFE++;
    case 0xF:
        s->atari.throughOn();
        SUB_TAKEAWAY_POS(-116.87f, 40.67f)
        MotionMoveF(s, 0);
        if (SUB_TAKEAWAY_HOLD_CK) {
            s->xFE = 0xA;
        } else {
            s->xFE = ((cEm*) s->dmgType)->xFE;
        }
        break;
    case 0x10:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x2A3), 0, 5, 4, 0);
        s->xFE++;
    case 0x11:
        SUB_TAKEAWAY_POS(-176.17f, 40.95f)
        MotionMoveF(s, 0);
        if (SUB_TAKEAWAY_HOLD_CK) {
            s->xFE = 0xA;
        } else {
            SUB_TAKEAWAY_SCREAM
        }
        break;
    }
    s->x3A8 = s->pos;
    if ((((cEm*) s->dmgType)->be_flag & 0x201) != 1) {
        s->pos.y = SatMgr.getFloor(&s->pos, 600.0f, 100000.0f, 0, 0);
        EndSubDamage();
    }
    s->subArc = s->subArc2;
}
#undef SUB_TAKEAWAY_POS
#undef SUB_TAKEAWAY_HOLD_CK
#undef SUB_TAKEAWAY_SCREAM

extern "C" void em10CamMoveTakeaway(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    GlobalWork* g = pG;
    Vec a;
    Vec b;

    a.x = 250.0f;
    a.y = 579.0f;
    a.z = -2530.0f;
    b.x = 0.0f;
    b.y = 1075.0f;
    b.z = 0.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    PosToPos(&g->Cam.param.pos, &a, &w->cam.param.pos, 1.0f);
    PosToPos(&g->Cam.param.at, &b, &w->cam.param.at, 1.0f);
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        w->cam.dist = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    w->cam.param.fovy = 55.0f;
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

static void em10_R0_Damage(cEm10* em)
{
    EM10_WK(em)->flags |= 8;
    Em10_R1_dmg_tbl[em->xFD](em);
}

// Flinch motion pair by weapon in hand, `flag` 0x41 when the arm parts are broken (flags_3C8 bit 24).
#define DM_SMALL_WEP_MOT(a, b)                                                                     \
    m0 = PL_ARC_PTR(em->subArc, a);                                                                \
    m1 = PL_ARC_PTR(em->subArc, b);                                                                \
    flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
#define DM_SMALL_WEP_MOT_SE(a, b)                                                                  \
    DM_SMALL_WEP_MOT(a, b)                                                                         \
    w->x6B1 = w->se6CE;
// Random 0/1 forced to 0 while a partner / parasite rides the Ganado.
#define DM_SMALL_RND2()                                                                            \
    r = Rnd() & 1;                                                                                 \
    if ((w->x58C || w->pParasite) && r == 1) {                                                     \
        r = 0;                                                                                     \
    }
#define DM_SMALL_RND3()                                                                            \
    r3 = Rnd() % 3;                                                                                \
    if ((w->x58C || w->pParasite) && r3 == 1) {                                                    \
        r3 = 0;                                                                                    \
    }

static void em10_R1_Dm_Small(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    void* m0;
    void* m1;
    int flag;
    u32 type;
    int r;
    u32 r3;

    switch (em->xFE) {
    case 0: {
        EmHitInfo* hit = em->dmPart;
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        if (fabsf(Muku(&em->pos, &em->dmg.pos, em->rot.y, PI)) < 1.5707964f) {
            type = hit->partsNo == 8 ? 1 : 0;
            if (hit->partsNo == 9) {
                type = 2;
            }
            if (hit->partsNo == 0xE) {
                type = 3;
            }
            if (hit->partsNo == 0xF) {
                type = 4;
            }
            if (hit->partsNo == 0x13) {
                type = 5;
            }
            if (hit->partsNo == 0x17) {
                type = 6;
            }
            if (hit->partsNo == 0x14) {
                type = 8;
            }
            if (hit->partsNo == 0x18) {
                type = 9;
            }
        } else {
            type = 7;
            if (hit->partsNo == 0x14) {
                type = 8;
            }
            if (hit->partsNo == 0x18) {
                type = 9;
            }
            if (hit->partsNo == 9) {
                type = 0xA;
            }
            if (hit->partsNo == 0xF) {
                type = 0xB;
            }
        }
        if (w->wepType == 0xC) {
            type = 0xC;
        }
        w->x4 = 0;
        w->x8 = 0;
        w->xC = 2;
        w->x14 = 10;
        w->x20 = 0;
        if (em->type == 6 || em->type == 0x16) {
            if (type == 8) {
                type = 5;
            }
            if (type == 9) {
                type = 6;
            }
        }
        w->x6B1 = w->se6CE;
        flag = 1;
        m1 = 0;
        switch (type) {
        case 0:
        default:
            switch (Rnd() % 3) {
            case 0:
            default:
                m0 = PL_ARC_PTR(em->subArc, 0x26);
                m1 = PL_ARC_PTR(em->subArc, 0x27);
                break;
            case 1:
                m0 = PL_ARC_PTR(em->subArc, 0x28);
                m1 = PL_ARC_PTR(em->subArc, 0x29);
                break;
            case 2:
                m0 = PL_ARC_PTR(em->subArc, 0x28);
                m1 = PL_ARC_PTR(em->subArc, 0x29);
                break;
            }
            flag = (em->motFlags & 0x40) ? 1 : 0x41;
            if (w->wepType == 1) {
                DM_SMALL_WEP_MOT(0x159, 0x15A)
            }
            if (w->wepType == 6) {
                DM_SMALL_WEP_MOT(0x144, 0x145)
            }
            if (w->wepType == 4) {
                DM_SMALL_WEP_MOT(0xF7, 0xF8)
            }
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x176, 0x177)
            }
            break;
        case 1:
            DM_SMALL_RND2()
            switch (r) {
            case 0:
            default:
                m0 = PL_ARC_PTR(em->subArc, 0x28);
                m1 = PL_ARC_PTR(em->subArc, 0x29);
                break;
            case 1:
                m0 = PL_ARC_PTR(em->subArc, 0x33);
                m1 = PL_ARC_PTR(em->subArc, 0x34);
                w->x4 = 10;
                w->x8 = 0x3C;
                w->x6B1 = w->se6C7;
                break;
            }
            em->setWeaponFall();
            flag = 1;
            if (w->wepType == 1) {
                DM_SMALL_WEP_MOT_SE(0x159, 0x15A)
            }
            if (w->wepType == 6) {
                DM_SMALL_WEP_MOT_SE(0x144, 0x145)
            }
            if (w->wepType == 4) {
                DM_SMALL_WEP_MOT_SE(0xF7, 0xF8)
            }
            if (w->pShield) {
                DM_SMALL_WEP_MOT_SE(0x176, 0x177)
            }
            break;
        case 2:
            DM_SMALL_RND2()
            switch (r) {
            case 0:
            default:
                m0 = PL_ARC_PTR(em->subArc, 0x32);
                break;
            case 1:
                m0 = PL_ARC_PTR(em->subArc, 0x33);
                m1 = PL_ARC_PTR(em->subArc, 0x34);
                w->x4 = 10;
                w->x8 = 0x3C;
                w->x6B1 = w->se6C7;
                break;
            }
            if (w->pWep && !(em->flags_3C8 & 0x1000000)) {
                m0 = PL_ARC_PTR(em->subArc, 0x55);
                m1 = PL_ARC_PTR(em->subArc, 0x56);
                w->x6B1 = w->se6CE;
                em->setWeaponFall();
            }
            flag = 1;
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x176, 0x177)
            }
            break;
        case 3:
            DM_SMALL_RND2()
            switch (r) {
            case 0:
            default:
                m0 = PL_ARC_PTR(em->subArc, 0x2A);
                m1 = PL_ARC_PTR(em->subArc, 0x2B);
                flag = 1;
                break;
            case 1:
                m0 = PL_ARC_PTR(em->subArc, 0x33);
                m1 = PL_ARC_PTR(em->subArc, 0x34);
                w->x4 = 10;
                w->x8 = 0x3C;
                w->x6B1 = w->se6C7;
                flag = 0x41;
                break;
            }
            em->setWeaponFall();
            if (w->wepType == 1) {
                DM_SMALL_WEP_MOT_SE(0x159, 0x15A)
            }
            if (w->wepType == 6) {
                DM_SMALL_WEP_MOT_SE(0x144, 0x145)
            }
            if (w->wepType == 4) {
                DM_SMALL_WEP_MOT_SE(0xF7, 0xF8)
            }
            if (w->pShield) {
                DM_SMALL_WEP_MOT_SE(0x176, 0x177)
            }
            break;
        case 4:
            DM_SMALL_RND2()
            switch (r) {
            case 0:
            default:
                m0 = PL_ARC_PTR(em->subArc, 0x32);
                break;
            case 1:
                m0 = PL_ARC_PTR(em->subArc, 0x33);
                m1 = PL_ARC_PTR(em->subArc, 0x34);
                w->x4 = 10;
                w->x8 = 0x3C;
                w->x6B1 = w->se6C7;
                break;
            }
            if (w->pWep && (em->flags_3C8 & 0x1000000)) {
                m0 = PL_ARC_PTR(em->subArc, 0x55);
                m1 = PL_ARC_PTR(em->subArc, 0x56);
                w->x6B1 = w->se6CE;
                em->setWeaponFall();
            }
            flag = 0x41;
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x176, 0x177)
            }
            break;
        case 5:
            DM_SMALL_RND3()
            switch (r3) {
            case 0:
            default:
                m0 = PL_ARC_PTR(em->subArc, 0x35);
                m1 = PL_ARC_PTR(em->subArc, 0x36);
                break;
            case 1:
                m0 = PL_ARC_PTR(em->subArc, 0x37);
                m1 = PL_ARC_PTR(em->subArc, 0x38);
                w->x6B1 = w->se6C9;
                break;
            case 2:
                m0 = PL_ARC_PTR(em->subArc, 0x51);
                m1 = PL_ARC_PTR(em->subArc, 0x52);
                w->flags |= 0x20;
                w->x4 = 999;
                break;
            }
            w->x14 = 0;
            flag = 1;
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x174, 0x175)
                w->x4 = 0;
            }
            break;
        case 6:
            DM_SMALL_RND3()
            switch (r3) {
            case 0:
            default:
                m0 = PL_ARC_PTR(em->subArc, 0x35);
                m1 = PL_ARC_PTR(em->subArc, 0x36);
                break;
            case 1:
                m0 = PL_ARC_PTR(em->subArc, 0x37);
                m1 = PL_ARC_PTR(em->subArc, 0x38);
                w->x6B1 = w->se6C9;
                break;
            case 2:
                m0 = PL_ARC_PTR(em->subArc, 0x51);
                m1 = PL_ARC_PTR(em->subArc, 0x52);
                w->flags |= 0x20;
                w->x4 = 999;
                break;
            }
            w->x14 = 0;
            flag = 0x41;
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x174, 0x175)
                w->x4 = 0;
            }
            break;
        case 7:
            m0 = PL_ARC_PTR(em->subArc, 0x45);
            m1 = PL_ARC_PTR(em->subArc, 0x46);
            flag = (em->motFlags & 0x40) ? 1 : 0x41;
            if (w->wepType == 1) {
                DM_SMALL_WEP_MOT(0x15B, 0x15C)
            }
            if (w->wepType == 6) {
                DM_SMALL_WEP_MOT(0x146, 0x147)
            }
            if (w->wepType == 4) {
                DM_SMALL_WEP_MOT(0xF9, 0xFA)
            }
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x172, 0x173)
            }
            w->x14 = 0;
            w->x4 = 10;
            break;
        case 8:
            switch (Rnd() % 3) {
            case 0:
            case 1:
            default:
                m0 = PL_ARC_PTR(em->subArc, 0x39);
                m1 = PL_ARC_PTR(em->subArc, 0x3A);
                w->x4 = 10;
                w->x14 = 0;
                w->x20 = 1;
                break;
            case 2:
                m0 = PL_ARC_PTR(em->subArc, 0x51);
                m1 = PL_ARC_PTR(em->subArc, 0x52);
                w->flags |= 0x20;
                w->x4 = 999;
                break;
            }
            flag = 1;
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x174, 0x175)
                w->x4 = 0;
                w->x20 = 2;
            }
            w->x14 = 0;
            break;
        case 9:
            switch (Rnd() % 3) {
            case 0:
            case 1:
            default:
                m0 = PL_ARC_PTR(em->subArc, 0x39);
                m1 = PL_ARC_PTR(em->subArc, 0x3A);
                w->x4 = 10;
                w->x14 = 0;
                w->x20 = 1;
                break;
            case 2:
                m0 = PL_ARC_PTR(em->subArc, 0x51);
                m1 = PL_ARC_PTR(em->subArc, 0x52);
                w->flags |= 0x20;
                w->x4 = 999;
                break;
            }
            flag = 0x41;
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x174, 0x175)
                w->x4 = 0;
                w->x20 = 2;
            }
            w->x14 = 0;
            break;
        case 0xA:
            m0 = PL_ARC_PTR(em->subArc, 0x47);
            m1 = PL_ARC_PTR(em->subArc, 0x48);
            if (w->pWep && !(em->flags_3C8 & 0x1000000)) {
                m0 = PL_ARC_PTR(em->subArc, 0x55);
                m1 = PL_ARC_PTR(em->subArc, 0x56);
                em->setWeaponFall();
            }
            w->x14 = 0;
            flag = 1;
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x176, 0x177)
            }
            break;
        case 0xB:
            m0 = PL_ARC_PTR(em->subArc, 0x47);
            m1 = PL_ARC_PTR(em->subArc, 0x48);
            if (w->pWep && (em->flags_3C8 & 0x1000000)) {
                m0 = PL_ARC_PTR(em->subArc, 0x55);
                m1 = PL_ARC_PTR(em->subArc, 0x56);
                em->setWeaponFall();
            }
            w->x14 = 0;
            flag = 0x41;
            if (w->pShield) {
                DM_SMALL_WEP_MOT(0x176, 0x177)
            }
            break;
        case 0xC:
            m0 = PL_ARC_PTR(em->subArc, 0x51);
            m1 = PL_ARC_PTR(em->subArc, 0x52);
            w->flags |= 0x20;
            w->x4 = 999;
            w->x14 = 0;
            break;
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 6, flag, 0);
        w->x10 = 10;
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        if (w->pShield) {
            w->x4 = 0;
        }
        em->xFE++;
    }
    case 1:
        if (w->xC) {
            w->xC--;
            if (w->xC == 0) {
                em10SetDamageVoice(em, w->x6B1, w->se6C6);
            }
        }
        if (w->x4) {
            w->x4--;
        } else {
            w->flags &= ~8;
        }
        if (w->x8) {
            w->x8--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.09817477f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 2) {
            w->flags |= 0x40000000;
        }
        if (em->seFlags28B & 1) {
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSetEm(em, -1, 0, 0, 0x10, 0x32, 0, 0, em, 0);
            } else {
                EstSetEm(em, -1, 0, 0, 0x10, 0x1A, 0, 0, em, 0);
            }
        }
        if (MotionMoveF(em, 0) || (em->seFlags28B & 4)) {
            if (em->seFlags28B & 0x80) {
                w->flags |= 0x10;
                w->flags |= 0x1000000;
                em->setStatus(3);
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            } else {
                em10WalkRtnSet(em);
            }
        } else if (!(em->seFlags28B & 0x80) && w->x14) {
            w->x14--;
            if (w->x14 == 0) {
                em10HideRtnCk(em);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    switch ((u32) w->x20) {
    case 0:
    default:
        break;
    case 1:
        switch (pG->x4FB8) {
        case 2:
            em10ActEvtSetKick(em);
            break;
        case 3:
            em10ActEvtSetKick(em);
            break;
        case 4:
            em10ActEvtSetFS(em);
            break;
        case 5:
            em10ActEvtSetKick(em);
            break;
        default:
            if (w->x6C5) {
                em10ActEvtSetFS(em);
            } else {
                em10ActEvtSetKick(em);
            }
            break;
        }
        break;
    case 2:
        switch (pG->x4FB8) {
        case 2:
            em10ActEvtSetKick(em);
            break;
        case 4:
            em10ActEvtSetKick(em);
            break;
        case 5:
            em10ActEvtSetKick(em);
            break;
        default:
            em10ActEvtSetKick(em);
            break;
        case 3:
            w->flags |= 0x40000000;
            em10ActEvtSetKick(em);
            w->flags &= ~0x40000000;
            break;
        }
        break;
    }
}
#undef DM_SMALL_WEP_MOT
#undef DM_SMALL_WEP_MOT_SE
#undef DM_SMALL_RND2
#undef DM_SMALL_RND3

static void em10_R1_Dm_Head(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;
    Mtx m;
    Vec spd;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        if (fabsf(Muku(&em->pos, &em->dmg.pos, em->rot.y, PI)) < 1.5707964f) {
            switch ((u8) (Rnd() % 3)) {
            case 1:
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2E), (int) PL_ARC_PTR(em->subArc, 0x2F), 6, flag, 0);
                break;
            case 2:
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x30), (int) PL_ARC_PTR(em->subArc, 0x31), 6, flag, 0);
                break;
            case 0:
            default:
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2C), (int) PL_ARC_PTR(em->subArc, 0x2D), 6, flag, 0);
                break;
            }
        } else {
            switch (Rnd() & 1) {
            case 0:
            default:
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2C), (int) PL_ARC_PTR(em->subArc, 0x2D), 6, flag, 0);
                break;
            case 1:
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2E), (int) PL_ARC_PTR(em->subArc, 0x2F), 6, flag, 0);
                break;
            }
        }
        w->x4 = 15;
        w->xC = 2;
        if (w->x178) {
            if ((u8) (Rnd() % 10) > 4 || em->type == 2) {
                switch (w->x6B4) {
                case 0:
                    break;
                default:
                    PSMTXRotRad(m, 'y', GetXZAngle(&pPL->pos, &em->pos));
                    spd.x = 0.0f;
                    spd.y = 40.0f;
                    spd.z = -50.0f;
                    PSMTXMultVecSR(em->mat, &spd, &spd);
                    ((cObj12*) w->x178)->setFall(&spd, 2);
                    w->x178 = 0;
                    w->x6B4 = 0;
                    break;
                case 3:
                case 4:
                    ObjMgr.destroy(w->x178);
                    w->x178 = 0;
                    w->x6B4 = 0;
                    break;
                }
            }
        }
        if (w->x17C) {
            PSMTXRotRad(m, 'y', GetXZAngle(&pPL->pos, &em->pos));
            spd.x = 0.0f;
            spd.y = 40.0f;
            spd.z = -50.0f;
            PSMTXMultVecSR(em->mat, &spd, &spd);
            ((cObj12*) w->x17C)->setFall(&spd, 3);
            w->x17C = 0;
        }
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em->xFE++;
    case 1:
        if (w->xC) {
            w->xC--;
            if (w->xC == 0) {
                em10SetDamageVoice(em, w->se6C8, w->se6C6);
            }
        }
        if (w->x4) {
            w->x4--;
        } else {
            w->flags &= ~8;
        }
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
    em10ActEvtSetKick(em);
}

static void em10_R1_Dm_Flash(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2AE), (int) PL_ARC_PTR(em->subArc, 0x2AF), 6, (u16) flag,
                      (u8) (Rnd() % 5));
        w->x4 = 15;
        w->xC = 2;
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em->xFE++;
    case 1:
        if (w->xC) {
            w->xC--;
            if (w->xC == 0) {
                em10SetDamageVoice(em, w->se6C8, w->se6C6);
            }
        }
        if (w->x4) {
            w->x4--;
        } else {
            w->flags &= ~8;
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        flag = (em->flags_3C8 & 0x1000000) ? 0x45 : 5;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2B0), (int) PL_ARC_PTR(em->subArc, 0x2B1), 6, flag, 0);
        w->x4 = (u8) (Rnd() % 5) + 5;
        em->xFE++;
    case 3:
        w->flags &= ~8;
        if (MotionMoveF(em, 0)) {
            if (w->x4) {
                w->x4--;
            } else {
                em->xFE++;
            }
        }
        break;
    case 4:
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2B2), (int) PL_ARC_PTR(em->subArc, 0x2B3), 6, flag, 0);
        em->xFE++;
    case 5:
        w->flags &= ~8;
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
    em10ActEvtSetKick(em);
}

static void em10_R1_Dm_Claw(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->x686 = Rnd() % 150 + 150;
    w->flags &= ~8;
    em10FindCk2(em);
    w->flags |= 0x100;
    w->x654 = 0x1C2;
    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        if (fabsf(Muku(&em->pos, &em->dmg.pos, em->rot.y, PI)) < 1.5707964f) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x10F), (int) PL_ARC_PTR(em->subArc, 0x110), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x111), (int) PL_ARC_PTR(em->subArc, 0x112), 3, 1, 0);
        }
        em10SetDamageVoice(em, w->se6C7, w->se6C6);
        w->x5F0 = pG->bell_pos;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        } else if (em->seFlags28B & 1) {
            em10CallVoiceSe2(em, w->se6D1, 8);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_Claw_Big(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    f32 a;

    w->x686 = Rnd() % 150 + 150;
    em10FindCk2(em);
    w->flags |= 0x100;
    w->x654 = 0x1C2;
    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x113), (int) PL_ARC_PTR(em->subArc, 0x114), 3, 1, 0);
        if (w->pParasite) {
            if (w->pParasite->ckAtkEnable()) {
                w->pParasite->setDamage();
            }
            EstSetEm(w->pParasite, -1, 0, 0, 0x10, 0x77, 0, 0, w->pParasite, 0);
        }
        em10CallVoiceSe(em, w->se6C7);
        w->x5F0 = pG->bell_pos;
        w->x18 = em->rot.y + PI;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            a = Muku(&em->pos, &w->x54C, w->x18, 0.09817477f);
            w->x18 += a;
            w->x18 = LIMIT_ANGLE(w->x18);
            em->rot.y += a;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_Gatling(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->x686 = Rnd() % 150 + 150;
    em10FindCk2(em);
    w->flags |= 0x100;
    w->x654 = 0x1C2;
    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        if (fabsf(Muku(&em->pos, &em->dmg.pos, em->rot.y, PI)) < 1.5707964f) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x194), (int) PL_ARC_PTR(em->subArc, 0x195), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x196), (int) PL_ARC_PTR(em->subArc, 0x197), 3, 1, 0);
        }
        em10SetDamageVoice(em, w->se6C7, w->se6C6);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_FS(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        em->xFF = 0;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xD4), (int) PL_ARC_PTR(em->subArc, 0xD5), 0, 1, 0);
        em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, PI);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        pPLS->rot.y = em->rot.y + PI;
        pPLS->rot.y = LIMIT_ANGLE(pPLS->rot.y);
        v.x = -2.01f;
        v.y = 0.0f;
        v.z = 628.03f;
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        PSMTXMultVec(em->mat, &v, &pPLS->pos);
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        em10SetDmWaterEff(em, 1);
        SndStop(w->x5C4, 0);
        w->x20 = 0;
        w->x6BD = 0;
        w->xC = 0;
        if ((u8) (Rnd() % 100) < 40) {
            LifeDownSet(em, 9999, 0);
        } else {
            LifeDownSet(em, 300, 0);
        }
        if (em->hp > 0) {
            EstSetEm(em, -1, 0, 0, 0x10, 0x84, 0, 0, em, 0);
        }
        w->flags |= 0x20;
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em->xFE++;
    case 1:
        if (w->x6BD == 0) {
            em->atari.flags |= 8;
            w->x6BC = 2;
        }
        em->dmg.set(0, 2);
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 1) {
            em10FallWaterCk(em);
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSetEm(em, -1, 0, 0, 0x10, 0x32, 0, 0, em, 0);
            } else {
                EstSetEm(em, -1, 0, 0, 0x10, 0x1A, 0, 0, em, 0);
            }
            if (em->hp <= 0) {
                em->hp = 0;
                em10LostHead(em, 3, 1);
                SndCall(1, 0x12, &em->pos, 0, 0, em);
            } else {
                EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x83, 0, 0, 0, 0);
                SndCall(8, 0xB0, &em->pos, em->id, 0, em);
            }
        }
        if (em->seFlags28B & 2) {
            em10SetDmWaterEff(em, 1);
        }
        if (w->x20 == 0 && CheckInWater(em, 0)) {
            w->x20 = 1;
            if (pG->room_id == 0x311) {
                EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);
                SndCall(6, 0xA, &em->pos, 0, 0, em);
            } else {
                EstSetEm10WaterFall((Vec*) em);
                SndCall(6, 0x16, &em->pos, 0, 0, em);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 1200.0f);
}

static void em10_R1_Dm_KneeKick(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;

    em->dmg.x1 = 2;
    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        em->xFF = 0;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2B7), (int) PL_ARC_PTR(em->subArc, 0x2B8), 0, 1, 0);
        em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, PI);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        pPLS->rot.y = em->rot.y + PI;
        pPLS->rot.y = LIMIT_ANGLE(pPLS->rot.y);
        v.x = -28.49f;
        v.y = 0.0f;
        v.z = 1382.54f;
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        PSMTXMultVec(em->mat, &v, &pPLS->pos);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        em10SetDmWaterEff(em, 1);
        SndStop(w->x5C4, 0);
        w->x20 = 0;
        w->x6BD = 0;
        w->xC = 0;
        if ((u8) (Rnd() % 100) < 40) {
            LifeDownSet(em, 9999, 0);
        } else {
            LifeDownSet(em, 1000, 0);
        }
        w->flags |= 0x20;
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em->xFE++;
    case 1:
        em->dmg.set(0, 2);
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 1) {
            em10FallWaterCk(em);
            if (em->hp <= 0) {
                em->hp = 0;
                em10LostHead(em, 0, 1);
            } else {
                SndCall(8, 0xB0, &em->pos, em->id, 0, em);
            }
        }
        if (em->seFlags28B & 2) {
            em10SetDmWaterEff(em, 1);
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 800.0f);
}

static void em10_R1_Dm_NeckBreak(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int end;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2BA), (int) PL_ARC_PTR(em->subArc, 0x2BB), 0, 1, 0);
        EstSetEm(em, -1, 0, 0, 0x10, 0x90, 0, 0, em, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem10NeckBreak, -75.21f, 0.0f, 1076.1f);
        em10SetDmWaterEff(em, 1);
        SndStop(w->x5C4, 0);
        em->hp = 0;
        w->flags &= ~0x20;
        w->x4 = 30;
        em->xFE++;
    case 1:
        em->dmg.set(0, 2);
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        } else {
            em->pos.y = pPL->pos.y;
            w->flags |= 0x80000;
        }
        if (em->seFlags28B & 2) {
            em10SetDmWaterEff(em, 1);
        }
        if (w->x4) {
            w->x4--;
            end = EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (end) {
            em->pos.y = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
            EmRoutineSet(em, 3, 0, 0, 1);
        } else if (em->seFlags28B & 1) {
            SndCall(1, 0x4C, &em->getPartsPtr(4)->worldPos, 0, 0, em);
            em10SetDamageVoice(em, w->se6CE, w->se6C6);
            em->setWeaponFall();
            if (w->pShield) {
                w->pShield->setFall(20.0f, 0);
                w->pShield = 0;
            }
            em10SetPoint(em);
        }
        break;
    }
    em->x3A8 = em->pos;
    em10HandSet(em, 0);
    em10SetCrash(em, 1500.0f);
}

static void em10_R1_Dm_Showtay(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    f32 y;
    int dmg;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2B5), 0, 3, 1, 2);
        em->rot.y = pPL->rot.y + PI;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        em10SetDmWaterEff(em, 1);
        w->flags |= 0x20;
        SndStop(w->x5C4, 0);
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        w->x4 = 12;
        w->x20 = 0;
        w->x6BD = 0;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        em->atari.flags |= 8;
        w->x6BC = 2;
        w->xC++;
        em->dmg.set(0, 2);
        w->flags |= 0x80000;
        em->setStatus(3);
        if (w->x20 == 0) {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                w->x20 = 1;
                if (pG->room_id == 0x311) {
                    EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);
                    SndCall(6, 0xA, &em->pos, 0, 0, em);
                } else {
                    EstSetEm10WaterFall((Vec*) em);
                    SndCall(6, 0x16, &em->pos, 0, 0, em);
                }
            }
        }
        if (w->x4) {
            w->x4--;
            v = em->pos;
            v.y = em->oldPos.y;
            y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < y + 50.0f) {
                em->pos.y = y;
            }
        } else {
            v = em->pos;
            v.y = em->oldPos.y;
            y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
            v.x += 10.0f;
            if (em->pos.y < y) {
                em->pos.y = y;
                w->x5A4.y = 0.0f;
                em->xFE++;
                MotionMoveF(em, 0);
                if (w->xC > 30) {
                    em->hp = 0;
                } else {
                    SndCall(8, 0x77, &em->pos, em->id, 0, em);
                    if (ChkWaterEffectEnable(&em->pos)) {
                        EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x31, 0, 0, 0, 0);
                    } else {
                        EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x17, 0, 0, 0, 0);
                    }
                }
                w->flags &= ~0x80000;
                break;
            }
        }
        DmgMgr.set(3, 2, &em->pos, 1500.0f, 800.0f);
        break;
    case 2:
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2B6), 0, 3, 1, 0);
        dmg = w->xC * 50;
        if (em->type == 6) {
            dmg = 0;
        }
        LifeDownSet2(em, dmg, 0, 0);
        if (em->pos.y < -99000.0f) {
            em->hp = 0;
        }
        em->xFE++;
    case 3:
        w->flags |= 0x10;
        w->flags |= 0x1000000;
        em->setStatus(3);
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 800.0f);
}

static void em10_R1_Dm_Heel(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x2B7), (int) PL_ARC_PTR(em->subArc, 0x2B8), 3, 1, 0);
        w->flags &= ~0x20;
        if (em->hp <= 0) {
            em10LostHead(em, 1, 0);
            em->setWeaponFall();
            if (w->pShield) {
                w->pShield->setFall(20.0f, 0);
                w->pShield = 0;
            }
        }
        if (!(w->flags & 0x80)) {
            em10CallVoiceSe2(em, w->se6D0, 8);
        }
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        GameAddPoint(9);
        w->x10 = 10;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_DashUp(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        flag = (Rnd() & 1) ? 1 : 0x41;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x57), (int) PL_ARC_PTR(em->subArc, 0x58), 6, flag, 0);
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em10WalkRtnSet(em);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_DashDown(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        flag = 1;
        if (em->dmPart->partsNo == 0x13) {
            flag = 0x41;
        }
        if (em->dmPart->partsNo == 0x14) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x59), (int) PL_ARC_PTR(em->subArc, 0x5A), 3, flag, 0);
        w->flags &= ~0x20;
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 1) {
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSetEm(em, -1, 0, 0, 0x10, 0x32, 0, 0, em, 0);
            } else {
                EstSetEm(em, -1, 0, 0, 0x10, 0x1A, 0, 0, em, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_Blow(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;
    f32 ang;
    Vec v;
    Vec spd;
    Vec rot;
    f32 y;
    cModel* p;
    int dmg;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        w->x4 = 0;
        if (em->xFF) {
            w->x4 = 1;
        }
        em->xFF = Rnd() & 1;
        flag = em->xFF ? 1 : 0x41;
        ang = fabsf(Muku(&em->pos, &em->dmg.pos, em->rot.y, PI));
        w->x18 = 1.0f;
        if ((u8) (Rnd() % 10) > 6 || em->hp > 0) {
            if (ang < 1.5707964f) {
                if ((Rnd() & 1) || (w->flags & 0x80)) {
                    MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x43), (int) PL_ARC_PTR(em->subArc, 0x44), 3, flag, 0);
                    em->rot.y += Muku(&em->pos, &em->dmg.pos, em->rot.y, PI);
                    w->flags &= ~0x20;
                    w->x5A4.x = 0.0f;
                    w->x5A4.y = -100.0f;
                    w->x5A4.z = -100.0f;
                } else {
                    MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x3F), (int) PL_ARC_PTR(em->subArc, 0x40), 3, flag, 0);
                    em->rot.y += Muku(&em->pos, &em->dmg.pos, em->rot.y, PI);
                    w->flags |= 0x20;
                    w->x5A4.x = 0.0f;
                    w->x5A4.y = -100.0f;
                    w->x5A4.z = -100.0f;
                }
            } else {
                if (Rnd() & 1) {
                    MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x4B), (int) PL_ARC_PTR(em->subArc, 0x4C), 3, flag, 0);
                    em->rot.y += Muku(&em->dmg.pos, &em->pos, em->rot.y, PI);
                    w->flags |= 0x20;
                    w->x5A4.x = 0.0f;
                    w->x5A4.y = -100.0f;
                    w->x5A4.z = 100.0f;
                } else {
                    MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x4D), (int) PL_ARC_PTR(em->subArc, 0x4E), 3, flag, 0);
                    em->rot.y += Muku(&em->dmg.pos, &em->pos, em->rot.y, PI);
                    w->flags &= ~0x20;
                    w->x5A4.x = 0.0f;
                    w->x5A4.y = -100.0f;
                    w->x5A4.z = 100.0f;
                }
            }
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        } else {
            if (ang < 1.5707964f && (u8) (Rnd() % 10) > 6) {
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x41), (int) PL_ARC_PTR(em->subArc, 0x42), 3, flag, 0);
                w->flags &= ~0x20;
                w->x5A4.x = 0.0f;
                w->x5A4.y = -200.0f;
                w->x5A4.z = -100.0f;
            } else {
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x3B), (int) PL_ARC_PTR(em->subArc, 0x3C), 3, flag, 0);
                w->flags |= 0x20;
                if (em->xFF == 1) {
                    w->x18 = 2.0f;
                }
                w->x5A4.x = 0.0f;
                w->x5A4.y = -200.0f;
                w->x5A4.z = -100.0f;
            }
            em->rot.y += Muku(&em->pos, &em->dmg.pos, em->rot.y, PI);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em10SetDmWaterEff(em, 1);
        SndStop(w->x5C4, 0);
        w->x20 = 0;
        w->x6BD = 0;
        w->xC = 0;
        em->xFE++;
    case 1:
        if (w->x6BD == 0) {
            em->atari.flags |= 8;
            w->x6BC = 2;
            em->dmg.set(0, 2);
        }
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 0x10) {
            w->flags |= 0x80000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 1) {
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSetEm(em, -1, 0, 0, 0x10, 0x32, 0, 0, em, 0);
            } else {
                EstSetEm(em, -1, 0, 0, 0x10, 0x1A, 0, 0, em, 0);
            }
        }
        if (em->seFlags28B & 2) {
            em10SetDmWaterEff(em, 1);
        }
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        PSVECScale(&spd, &spd, w->x18);
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        if (w->x20 == 0) {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                w->x20 = 1;
                if (pG->room_id == 0x311) {
                    EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);
                    SndCall(6, 0xA, &em->pos, 0, 0, em);
                } else {
                    EstSetEm10WaterFall((Vec*) em);
                    SndCall(6, 0x16, &em->pos, 0, 0, em);
                }
            }
        }
        v = em->pos;
        v.y = em->oldPos.y;
        y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
        if (w->flags & 0x80000) {
            if (w->x6BD == 0) {
                if (em->pos.y < y + 50.0f) {
                    em->pos.y = y;
                }
            }
        }
        if (w->x6BD) {
            w->flags &= ~0x80000;
        }
        if (MotionMoveF(em, 0)) {
            if (em->pos.y < -99000.0f) {
                em->hp = 0;
            }
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        } else {
            if (w->x4) {
                p = em->getPartsPtr(0);
                if (!(em->seFlags28B & 0x80)) {
                    p->worldMat[1][3] *= 1.5f;
                }
            }
            if (em->seFlags28B & 4) {
                if (em->pos.y > y + 300.0f) {
                    em->xFE++;
                } else {
                    v.x = 0.0f;
                    v.y = 0.0f;
                    v.z = -100.0f;
                    PSMTXMultVec(em->mat, &v, &v);
                    v.y = em->oldPos.y;
                    y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
                    if (em->pos.y > y + 300.0f) {
                        em->xFE++;
                    } else {
                        v.x = 50.0f;
                        v.y = 0.0f;
                        v.z = -50.0f;
                        PSMTXMultVec(em->mat, &v, &v);
                        v.y = em->oldPos.y;
                        y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
                        if (em->pos.y > y + 300.0f) {
                            em->xFE++;
                        } else {
                            w->x6BD = 1;
                            w->flags &= ~0x80000;
                        }
                    }
                }
            }
        }
        break;
    case 2:
        flag = em->xFF ? 1 : 0x41;
        if (w->flags & 0x20) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x5B), (int) PL_ARC_PTR(em->subArc, 0x5E), 3, flag, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x53), 0, 3, flag, 0);
        }
        em->xFE++;
    case 3:
        em->atari.flags |= 8;
        w->x6BC = 2;
        w->xC++;
        em->dmg.set(0, 2);
        if (em->seFlags28B & 0x10) {
            w->flags |= 0x80000;
            em->setStatus(3);
        }
        w->x5A4.y -= 20.0f;
        PSMTXMultVecSR(em->mat, &w->x5A4, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        if (w->x20 == 0) {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                w->x20 = 1;
                if (pG->room_id == 0x311) {
                    EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);
                    SndCall(6, 0xA, &em->pos, 0, 0, em);
                } else {
                    EstSetEm10WaterFall((Vec*) em);
                    SndCall(6, 0x16, &em->pos, 0, 0, em);
                }
            }
        }
        v = em->pos;
        v.y = em->oldPos.y;
        y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
        v.x += 10.0f;
        if (em->pos.y < y) {
            em->pos.y = y;
            w->x5A4.y = 0.0f;
            em->xFE++;
            MotionMoveF(em, 0);
            if (w->xC > 60) {
                em->hp = 0;
            } else {
                SndCall(8, 0x77, &em->pos, em->id, 0, em);
                if (ChkWaterEffectEnable(&em->pos)) {
                    EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x31, 0, 0, 0, 0);
                } else {
                    EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x17, 0, 0, 0, 0);
                }
            }
            w->flags &= ~0x80000;
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 4:
        flag = em->xFF ? 1 : 0x41;
        if (w->flags & 0x20) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x5B), (int) PL_ARC_PTR(em->subArc, 0x5D), 3, flag, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x54), 0, 3, flag, 0);
        }
        dmg = w->xC * 50;
        if (em->type == 6) {
            dmg = 0;
        }
        LifeDownSet2(em, dmg, 0, 0);
        if (em->pos.y < -99000.0f) {
            em->hp = 0;
        }
        em->xFE++;
    case 5:
        w->flags |= 0x10;
        w->flags |= 0x1000000;
        em->setStatus(3);
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        } else if (em->xFF == 2) {
            p = em->getPartsPtr(0);
            if (!(em->seFlags28B & 0x80)) {
                p->worldMat[1][3] *= 1.5f;
            }
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 800.0f);
}

static void em10_R1_Dm_Fence(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        em->xFF = Rnd() & 1;
        flag = em->xFF ? 1 : 0x41;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x5B), (int) PL_ARC_PTR(em->subArc, 0x5C), 3, flag, 0);
        w->flags |= 0x20;
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em10SetDmWaterEff(em, 1);
        SndStop(w->x5C4, 0);
        em->xFE++;
    case 1:
        em->dmg.set(0, 2);
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 1) {
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSetEm(em, -1, 0, 0, 0x10, 0x32, 0, 0, em, 0);
            } else {
                EstSetEm(em, -1, 0, 0, 0x10, 0x1A, 0, 0, em, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 800.0f);
}

static void em10_R1_Dm_Ladder(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;
    Vec v;
    Vec a;
    Vec b;
    f32 y;
    int dmg;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        em->xFF = Rnd() & 1;
        flag = em->xFF ? 1 : 0x41;
        a.x = 0.0f;
        a.y = -1000.0f;
        a.z = -1500.0f;
        b.x = 2000.0f;
        b.y = -1000.0f;
        b.z = -1500.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            flag = 1;
        }
        a.x = 0.0f;
        a.y = -1000.0f;
        a.z = -1500.0f;
        b.x = -2000.0f;
        b.y = -1000.0f;
        b.z = -1500.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0xCC), (int) PL_ARC_PTR(em->subArc, 0xCD), 3, flag, 0);
        w->flags |= 0x20;
        em10SetDamageVoice(em, w->se6D4, w->se6D4);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        w->x4 = 0;
        em10SetDmWaterEff(em, 1);
        SndStop(w->x5C4, 0);
        GameAddPoint(9);
        w->x20 = 0;
        em->xFE++;
    case 1:
        em->dmg.set(0, 2);
        w->flags |= 0x80000;
        em->setStatus(3);
        if (MotionMoveF(em, 0)) {
            w->x20 = 1;
        }
        if (!(em->seFlags28B & 4)) {
            break;
        }
        if (w->x20) {
            em->pos.y -= 1000.0f;
        }
        w->x4++;
        v = em->pos;
        v.y = em->oldPos.y;
        y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y > y) {
            break;
        }
        em->pos.y = y;
        w->x5A4.y = 0.0f;
        em->xFE++;
    case 2:
        flag = em->xFF ? 1 : 0x41;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x5B), (int) PL_ARC_PTR(em->subArc, 0x5D), 3, flag, 0);
        dmg = w->x4 * 50;
        if (em->type == 6) {
            dmg = 0;
        }
        LifeDownSet2(em, dmg, 0, 0);
        if (em->pos.y < -99000.0f) {
            em->hp = 0;
        }
        SndCall(8, 0x77, &em->pos, em->id, 0, em);
        if (ChkWaterEffectEnable(&em->pos)) {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x31, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x17, 0, 0, 0, 0);
        }
        em->xFE++;
    case 3:
        w->flags |= 0x10;
        w->flags |= 0x1000000;
        em->setStatus(3);
        if (em->seFlags28B & 1) {
            em10FallWaterCk(em);
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSetEm(em, -1, 0, 0, 0x10, 0x32, 0, 0, em, 0);
            } else {
                EstSetEm(em, -1, 0, 0, 0x10, 0x1A, 0, 0, em, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 800.0f);
}

static void em10_R1_Dm_Roof(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;
    Vec v;
    Vec d;
    f32 y;
    f32 a;
    int mv;
    cModel* p;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        if (em->xFF) {
            em->xFF = Rnd() & 1;
            flag = em->xFF ? 1 : 0x41;
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x61), (int) PL_ARC_PTR(em->subArc, 0x62), 3, flag, 0);
        } else {
            em->xFF = Rnd() & 1;
            flag = em->xFF ? 1 : 0x41;
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x5F), (int) PL_ARC_PTR(em->subArc, 0x60), 3, flag, 0);
        }
        w->flags &= ~0x20;
        em10SetDamageVoice(em, w->se6D4, w->se6D4);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em->flags_3C8 &= ~0x400;
        w->x8 = 15;
        w->x4 = 0;
        w->x20 = 0;
        em10SetDmWaterEff(em, 1);
        SndStop(w->x5C4, 0);
        w->x18 = Muku2(em->rot.y, w->x5DC, PI);
        w->xC = 20;
        em->xFE++;
    case 1:
        em->dmg.set(0, 2);
        em->flags_3C8 &= ~0x400;
        a = w->x18 * 0.1f;
        em->rot.y += a;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        w->x18 -= a;
        if (w->x8) {
            w->x8--;
            if (w->x8 == 0) {
                BitOn(pG->flags_5010, 0x20000);
            }
        }
        if (w->xC) {
            w->xC--;
            if (em->xFF == 2 && w->pGondola) {
                p = w->pGondola->getPartsPtr(0);
                PSVECSubtract(&p->worldPos, &p->x88, &d);
                d.y = 0.0f;
                PSVECAdd(&em->pos, &d, &em->pos);
            }
        } else if (w->pGondola) {
            w->pGondola->setGetOffEm(em);
            w->pGondola = 0;
        }
        w->flags |= 0x80000;
        w->flags |= 0x1000;
        em->setStatus(3);
        mv = MotionMoveF(em, 0);
        if (em->seFlags28B & 4) {
            w->x4++;
            v = em->pos;
            v.y = em->oldPos.y;
            y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
            if (w->x20 == 0) {
                em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                w->x20 = 1;
                if (pG->room_id == 0x311) {
                    EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);
                    SndCall(6, 0xA, &em->pos, 0, 0, em);
                } else {
                    EstSetEm10WaterFall((Vec*) em);
                    SndCall(6, 0x16, &em->pos, 0, 0, em);
                }
            }
            }
            if (em->pos.y < y) {
                em->pos.y = y;
                w->x5A4.y = 0.0f;
                flag = em->xFF ? 1 : 0x41;
                MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x63), (int) PL_ARC_PTR(em->subArc, 0x64), 3, flag, 0);
                MotionMoveF(em, 0);
                em->xFE = 4;
            } else if (mv) {
                em->xFE++;
            }
        }
        break;
    case 2:
        w->x5A4.x = 0.0f;
        w->x5A4.y = -300.0f;
        w->x5A4.z = 0.0f;
        if (w->pGondola) {
            w->pGondola->setGetOffEm(em);
            w->pGondola = 0;
        }
        em->xFE++;
    case 3:
        w->x4++;
        em->dmg.set(0, 2);
        w->flags |= 0x80000;
        w->flags |= 0x1000;
        em->setStatus(3);
        PSVECAdd(&em->pos, &w->x5A4, &em->pos);
        w->x5A4.y -= 20.0f;
        if (w->x20 == 0) {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                IntSet(w->x20, 1);
                if (pG->room_id == 0x311) {
                    EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);
                    SndCall(6, 0xA, &em->pos, 0, 0, em);
                } else {
                    EstSetEm10WaterFall((Vec*) em);
                    SndCall(6, 0x16, &em->pos, 0, 0, em);
                }
            }
            if (pG->room_id == 0x222 && em->pos.y <= -9900.0f) {
                d = em->pos;
                d.y = -9900.0f;
                EstSet(0, -1, &d, 0, 1, 6, 0, 0, 0, 0);
                w->x20 = 1;
            }
        }
        MotionMoveF(em, 0);
        v = em->pos;
        v.y = em->oldPos.y;
        y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < y) {
            em->pos.y = y;
            w->x5A4.y = 0.0f;
            flag = em->xFF ? 1 : 0x41;
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x63), (int) PL_ARC_PTR(em->subArc, 0x64), 3, flag, 0);
            MotionMoveF(em, 0);
            em->xFE = 4;
        }
        break;
    case 4:
        LifeDownSet2(em, w->x4 * 50, 0, 0);
        if (em->pos.y < -99000.0f) {
            em->hp = 0;
        }
        if (w->pGondola) {
            w->pGondola->setGetOffEm(em);
            w->pGondola = 0;
        }
        if (w->x4 > 60) {
            em->hp = 0;
        } else {
            em10FallWaterCk(em);
            if (CheckInWater(em, 0)) {
                if (w->x20 == 0) {
                    w->x20 = 1;
                    if (pG->room_id == 0x311) {
                        EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);
                        SndCall(6, 0xA, &em->pos, 0, 0, em);
                    } else {
                        EstSetEm10WaterFall((Vec*) em);
                        SndCall(6, 0x16, &em->pos, 0, 0, em);
                    }
                }
            } else {
                SndCall(8, 0x77, &em->pos, em->id, 0, em);
                if (ChkWaterEffectEnable(&em->pos)) {
                    EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x31, 0, 0, 0, 0);
                } else {
                    EstSet(0, -1, &em->pos, &em->rot, 0x10, 0x17, 0, 0, 0, 0);
                }
            }
        }
        em->xFE++;
    case 5:
        w->flags |= 0x10;
        w->flags |= 0x1000000;
        em->setStatus(3);
        if (em->seFlags28B & 1) {
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSetEm(em, -1, 0, 0, 0x10, 0x32, 0, 0, em, 0);
            } else {
                EstSetEm(em, -1, 0, 0, 0x10, 0x1A, 0, 0, em, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
    em10SetCrash(em, 800.0f);
}

static void em10_R1_Dm_KneeDown(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        if (fabsf(Muku(&em->pos, &em->dmg.pos, em->rot.y, PI)) < 1.5707964f) {
            em->xFF = Rnd() & 1;
            flag = (MOTION(em)->flags & 0x40) ? 0x41 : 1;
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x3D), (int) PL_ARC_PTR(em->subArc, 0x3E), 15, flag, 0);
            w->flags |= 0x20;
        } else {
            em->xFF = Rnd() & 1;
            flag = (MOTION(em)->flags & 0x40) ? 0x41 : 1;
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x49), (int) PL_ARC_PTR(em->subArc, 0x4A), 15, flag, 0);
            w->flags &= ~0x20;
        }
        if (em->hp <= 0) {
            em->setWeaponFall();
            if (w->pShield) {
                w->pShield->setFall(20.0f, 0);
                w->pShield = 0;
            }
        }
        if (!(w->flags & 0x80)) {
            em10CallVoiceSe2(em, w->se6D0, 8);
        }
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        GameAddPoint(9);
        w->x10 = 10;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_KnockOut(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        em->xFF = Rnd() & 1;
        flag = em->xFF ? 1 : 0x41;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x69), (int) PL_ARC_PTR(em->subArc, 0x6A), 15, flag, 0);
        w->flags &= ~0x20;
        if (em->hp <= 0) {
            em->setWeaponFall();
            if (w->pShield) {
                w->pShield->setFall(20.0f, 0);
                w->pShield = 0;
            }
        }
        if (!(w->flags & 0x80)) {
            em10CallVoiceSe2(em, w->se6D0, 8);
        }
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        w->x10 = 10;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 1) {
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSetEm(em, -1, 0, 0, 0x10, 0x32, 0, 0, em, 0);
            } else {
                EstSetEm(em, -1, 0, 0, 0x10, 0x1A, 0, 0, em, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_Down(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        flag = (MOTION(em)->flags & 0x40) ? 0x41 : 1;
        if (w->flags & 0x20) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x4F), 0, 5, flag, 0);
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x50), 0, 5, flag, 0);
        }
        if (w->pParasite && w->pParasite->ckAtkEnable()) {
            w->pParasite->setDamage();
        }
        if (w->x58C && w->x58C->vB8()) {
            w->x58C->vC0();
        }
        em->xFE++;
    case 1:
        w->flags |= 0x10;
        w->flags |= 0x1000000;
        em->setStatus(3);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x1F, 0, 0);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_Frame(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag;
    cModelInfo* info;
    u32 i;

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        flag = (em->flags_3C8 & 0x1000000) ? 0x41 : 1;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x65), (int) PL_ARC_PTR(em->subArc, 0x66), 3, (u16) flag,
                      (u8) (Rnd() % 5));
        LifeDownSet(em, 500, 0);
        if (em->xFF == 0) {
            EffectEspDelete(0, w->x69E, (u32) em, 0);
            EffectEspgenDelete(0, w->x69E, (int) em);
            EffectEfmDelete(0, w->x69E, (int) em);
            EstSetEm(em, -1, 0, 0, 0x10, 0x24, 0, 0, em, 0);
        }
        SndStop(w->x5C0, 0);
        SndCall(8, 0x8E, &em->pos, em->id, 0, em);
        SndCall(8, 0x90, &em->pos, em->id, 0, em);
        em10CallVoiceSe2(em, w->se6C8, 8);
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        w->x4 = 50;
        w->flags &= ~0x20;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        } else {
            w->flags &= ~8;
        }
        if (w->x4) {
            w->x4--;
            LifeDownSet2(em, 10, 0, 0);
        }
        if (em->hp <= 0) {
            for (info = em->pInfo; info; info = info->pNext) {
                if (info->color[0] > 0x20) {
                    info->color[0] -= 0x20;
                }
                info->color[2] = info->color[1] = info->color[0];
            }
            if (w->x178) {
                ((cObj12*) w->x178)->setBurn();
            }
            if (w->pParasite) {
                w->pParasite->setBurn();
            }
            for (i = 0; i < 5; i++) {
                if (w->x578[i]) {
                    ((cObj16*) w->x578[i])->setBurn();
                }
            }
        }
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Dm_TakeAway(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        w->flags &= ~0x8000000;
        em10MouthPartsReset(em);
        if (w->x6BE != 4) {
            w->x6BE = 3;
        }
        if (w->x6BF != 4) {
            w->x6BF = 3;
        }
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x59), (int) PL_ARC_PTR(em->subArc, 0x5A), 3, 1, 0);
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        w->flags &= ~0x20;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            }
        } else if ((em->seFlags28B & 4) && em->hp <= 0) {
            EmRoutineSet(em, 3, 2, 0, 0);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R0_Die(cEm10* em)
{
    EM10_WK(em)->flags |= 8;
    Em10_R1_die_tbl[em->xFD](em);
}

static void em10_R1_Die_Cramp(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int f;

    w->flags |= 0x10;
    w->flags |= 0x1000000;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        em10MouthPartsReset(em);
        em10SetPoint(em);
        EmSetDie(em);
        if (em->type != 6) {
            EmSetDieCntE(em);
            EmReserveDropItem(em);
        }
        em->clearStatus(5);
        em->setStatus(8);
        EmSetDropItem(em);
        em->atari.flags &= ~0x200;
        w->x4 = 0;
        if (w->x58C) {
            w->x4 = 60;
        }
        if (w->x6AE) {
            w->x6AE = 0;
            Ctrl12CntAddI(w->pCtrl12, 4, -1);
        }
        if (em->type == 0xA || em->type == 0xD) {
            if (w->pParasite) {
                EffectEspDelete(0, w->x6A0, (u32) w->pParasite, 0);
                EffectEspgenDelete(0, w->x6A0, (int) w->pParasite);
                EffectEfmDelete(0, w->x6A0, (int) w->pParasite);
                w->pParasite->setLostWait(0);
                w->pParasite = 0;
            }
        }
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        em10ParasiteGoOut(em);
        em10CoreBreak(em, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->x640) {
            EmRoutineSet(em, 3, 5, 0, 0);
            return;
        }
        if (w->x4) {
            w->x4--;
        } else if (!Ctrl12Ck(w->pCtrl12, 7)) {
            f = 0;
            if ((pG->room_id32 & 0xFFFF0000) == 0x1000000) {
                f = 1;
            }
            if (em->flags_3C8 & 0x10000000) {
                f = 1;
            }
            if (w->x6C5 == 1 && (em->flags_3C8 & 0x100)) {
                f = 1;
            }
            if (em->type == 6) {
                f = 1;
            }
            if (em->type == 2) {
                f = 1;
            }
            if (em->type == 0xA) {
                f = 1;
            }
            if (em->type == 0xD) {
                f = 1;
            }
            if (em->x38D == 0x39) {
                f = 1;
            }
            if (pG->stage_no > 3) {
                f = 0;
            }
            if (f) {
                em->xFE++;
            } else {
                EmRoutineSet(em, 3, 3, 0, 0);
                Ctrl12Set(w->pCtrl12, 7, (u8) (Rnd() % 10) + 15);
            }
        }
        break;
    case 2:
        MotionMoveF(em, 0);
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Die_Lost(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->flags |= 0x400;
    switch (em->xFE) {
    case 0:
        em->atari.flags &= ~0x300;
        em->atari.flags |= 0x10;
        em10SetPoint(em);
        EmSetDie(em);
        EmReserveDropItem(em);
        em->clearStatus(5);
        if (CheckInWater(em, 0)) {
            EstSet((int) em, -1, 0, 0, 1, 0x35, 0, 0, (u32) em, 0);
        } else if (w->flags & 0x20) {
            switch (em->type) {
            case 0:
            case 1:
            case 3:
            case 4:
            case 0xA:
            case 0xD:
            case 0xE:
            case 0xF:
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x13:
            case 0x14:
            case 0x15:
            case 0x17:
            case 0x18:
            case 0x19:
                EstSet((int) em, -1, 0, 0, 0x10, 0, 0, 0, (u32) em, 0);
                break;
            case 2:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 0xB:
            case 0xC:
                EstSet((int) em, -1, 0, 0, 0x10, 0x58, 0, 0, (u32) em, 0);
                break;
            }
        } else {
            switch (em->type) {
            case 0:
            case 1:
            case 3:
            case 4:
            case 0xA:
            case 0xD:
            case 0xE:
            case 0xF:
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x13:
            case 0x14:
            case 0x15:
            case 0x17:
            case 0x18:
            case 0x19:
                EstSet((int) em, -1, 0, 0, 0x10, 0x19, 0, 0, (u32) em, 0);
                break;
            case 2:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 0xB:
            case 0xC:
                EstSet((int) em, -1, 0, 0, 0x10, 0x59, 0, 0, (u32) em, 0);
                break;
            }
        }
        em10CoreBreak(em, 1);
        if (w->x58C) {
            w->x58C->setReset();
            w->x58C = 0;
        }
        w->x4 = 15;
        w->x8 = 0x35;
        w->xC = 1;
        if (w->wepType == 4) {
            w->xC = 60;
        }
        SndCall(8, 0x44, &em->pos, em->id, 0, em);
        w->x5D8 = 1.0f;
        em->xFE++;
    case 1:
        if (w->xC) {
            w->xC--;
            if (w->xC == 0) {
                em->setStatus(8);
            }
        }
        if (w->x4) {
            w->x4--;
        } else {
            w->x5D8 -= 0.028f;
            if (w->x5D8 < 0.1f) {
                w->x5D8 = 0.1f;
            }
            em->pos.y -= 6.0f;
        }
        TransMatrix(em->mat, &em->pos);
        if (w->x8) {
            w->x8--;
        } else {
            em->alpha -= 0.1f;
            if (em->alpha <= 0.0f) {
                em->alpha = 0.0f;
                if (w->pWep) {
                    w->pWep->setLost();
                    w->pWep = 0;
                    w->wepType = 0;
                }
                if (w->pWep2) {
                    w->pWep2->setLost();
                    w->pWep2 = 0;
                    w->wep2Type = 0;
                }
                em10CoreBreak(em, 1);
                if (w->x58C) {
                    w->x58C->setReset();
                    w->x58C = 0;
                }
                em->be_flag &= ~2;
                w->x6B7 = 1;
                w->flags |= 0x400000;
                em->xFE++;
            }
        }
        break;
    }
}

static void em10_R1_Die_Down(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->flags |= 0x10;
    w->flags |= 0x1000000;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        em10MouthPartsReset(em);
        em10SetPoint(em);
        if (w->flags & 0x20) {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x6B), 0, 6, 1, 0);
            w->x4 = 0x46;
        } else {
            MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x6C), 0, 6, 1, 0);
            w->x4 = 0x4A;
        }
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            if (w->x4 == 0) {
                SndCall(8, 4, &em->pos, em->id, 0, em);
            }
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 0, 0, 1);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Die_Normal(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;

    switch (em->xFE) {
    case 0:
        em10MouthPartsReset(em);
        em10SetPoint(em);
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x67), (int) PL_ARC_PTR(em->subArc, 0x68), 15, 1, 0);
        w->flags &= ~0x20;
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        if (em->type != 6) {
            EmReserveDropItem(em);
        }
        w->x4 = 0x49;
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        if (em->type == 0xA || em->type == 0xD) {
            EstSet((int) em, -1, 0, 0, 0x10, 0x85, 0, 0, (u32) em, 0);
            SndCall(8, 0x44, &em->pos, em->id, 0, em);
            if (w->pParasite) {
                EffectEspDelete(0, w->x6A0, (u32) w->pParasite, 0);
                EffectEspgenDelete(0, w->x6A0, (int) w->pParasite);
                EffectEfmDelete(0, w->x6A0, (int) w->pParasite);
                w->pParasite->setLostWait(100);
                w->pParasite = 0;
            }
        }
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 1) {
            v = em->rot;
            v.y += fRand1_1() * 3.1415927f;
            v.y = LIMIT_ANGLE(v.y);
            EstSet(0, -1, &em->pos, &v, 0x10, 0x18, 0, 0, 0, 0);
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSet((int) em, -1, 0, 0, 0x10, 0x32, 0, 0, (u32) em, 0);
            } else {
                EstSet((int) em, -1, 0, 0, 0x10, 0x1A, 0, 0, (u32) em, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 0, 0, 1);
        } else if (w->x4) {
            w->x4--;
            if (w->x4 == 0) {
                em->clearStatus(5);
                em->setStatus(8);
                EmSetDropItem(em);
                em->be_flag |= 0x10000;
            }
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Die_RunDown(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->xFE) {
    case 0:
        em10MouthPartsReset(em);
        em10SetPoint(em);
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(em->subArc, 0x59), (int) PL_ARC_PTR(em->subArc, 0x5A), 3, 1, 0);
        em->hp = 0;
        w->flags &= ~0x20;
        em10SetDamageVoice(em, w->se6CE, w->se6C6);
        em10SetDmWaterEff(em, 0);
        SndStop(w->x5C4, 0);
        em->setWeaponFall();
        if (w->pShield) {
            w->pShield->setFall(20.0f, 0);
            w->pShield = 0;
        }
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x10;
            w->flags |= 0x1000000;
            em->setStatus(3);
        }
        if (em->seFlags28B & 1) {
            if (ChkWaterEffectEnable(&em->pos)) {
                EstSetEm(em, -1, 0, 0, 0x10, 0x32, 0, 0, em, 0);
            } else {
                EstSetEm(em, -1, 0, 0, 0x10, 0x1A, 0, 0, em, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 0, 0, 1);
        }
        break;
    }
    em10HandSet(em, 0);
}

static void em10_R1_Die_Bomb(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModel* p;
    Camera* cam;
    f32 dx, dy;
    Vec rot;

    switch (em->xFE) {
    case 0:
        em10SetPoint(em);
        if (w->x640) {
            w->x640 = 0;
        }
        if (w->pWep) {
            w->pWep->setLost();
            w->pWep = 0;
            w->wepType = 0;
        }
        if (w->x58C) {
            w->x58C->setReset();
            w->x58C = 0;
        }
        em->hp = 0;
        SndStop(w->x5C4, 0);
        em->atari.flags &= ~0x300;
        em->atari.flags |= 0x10;
        EmSetDie(em);
        EmReserveDropItem(em);
        em10SetPoint(em);
        if (em->type != 6) {
            EmSetDieCntE(em);
        }
        em->clearStatus(5);
        em->setStatus(8);
        EmSetDropItem(em);
        w->x4 = 1;
        w->x8 = 3;
        SndCall(8, 0x96, &em->pos, em->id, 0, em);
        SndCall(8, 8, &em->pos, em->id, 0, em);
        MotionMoveF(em, 0);
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            if (w->x4 == 0) {
                if (em->xFF == 2) {
                    EstSet((int) em, -1, 0, 0, 0x10, 0x9B, 0, 0, (u32) em, 0);
                } else {
                    p = em->getPartsPtr(0);
                    if (w->flags & 0x1400000) {
                        EstSet(0, -1, &p->worldPos, 0, 0x10, 0x2A, 0, 0, 0, 0);
                    } else {
                        cam = &pG->Cam;
                        dx = cam->param.pos.x - p->worldPos.x;
                        dy = cam->param.pos.y - p->worldPos.y;
                        if (dx * dx + dy * dy + (cam->param.pos.z - p->worldPos.z) * (cam->param.pos.z - p->worldPos.z) < 4000000.0f) {
                            rot.x = 0.0f;
                            rot.y = GetXZAngle(&p->worldPos, &cam->param.pos);
                            rot.z = 0.0f;
                            EstSet(0, -1, &em->pos, &rot, 0x10, 0x47, 0, 0, 0, 0);
                        } else {
                            EstSet((int) em, -1, 0, 0, 0x10, 0x30, 0, 0, (u32) em, 0);
                        }
                    }
                }
                if (em->xFF == 0) {
                    PlWepHitCheck2(0, &em->pos, &em->pos, 0x13, 2, 6000.0f);
                }
            }
        }
        if (w->x8) {
            w->x8--;
        } else {
            em10CoreBreak(em, 2);
            if (w->x58C) {
                w->x58C->setReset();
                w->x58C = 0;
            }
            if (w->x590) {
                if (w->x590->isAlive()) {
                    ObjMgr.destroy((cObj*) w->x590);
                }
                w->x590 = 0;
            }
            if (w->x594) {
                if (w->x594->isAlive()) {
                    ObjMgr.destroy((cObj*) w->x594);
                }
                w->x594 = 0;
            }
            w->flags |= 0x400000;
            em->be_flag &= ~2;
            em->xFE++;
        }
        break;
    case 2:
        w->x4 = 90;
        em->xFE++;
    case 3:
        if (w->x4) {
            w->x4--;
        } else {
            w->x6B7 = 1;
        }
        break;
    }
}
#undef EM10_ROOF_PROBE

// Once the enemy is locked on (be_flag 0x20000000) the route target is the player / partner itself when
// on the same floor.
#define EM10_ROUTE_LOCKON()                                                                        \
    if (em->be_flag & 0x20000000) {                                                                \
        if ((w->flags & 0x08000000) && pSUB) {                                                     \
            dy = pSUB->pos.y - em->pos.y;                                                          \
            dy = fabsf(dy);                                                                        \
            if (dy < 1000.0f) {                                                                    \
                w->x54C = pSUB->pos;                                                               \
                w->x518 = Muku(&em->pos, &w->x54C, em->rot.y, 3.1415927f);                         \
                w->x51C = fabsf(w->x518);                                                          \
            }                                                                                      \
        } else {                                                                                   \
            dy = pPL->pos.y - em->pos.y;                                                           \
            dy = fabsf(dy);                                                                        \
            if (dy < 1000.0f) {                                                                    \
            w->x534 = pPL->pos;                                                                    \
            w->x504 = Muku(&em->pos, &w->x534, em->rot.y, 3.1415927f);                             \
            w->x508 = fabsf(w->x504);                                                              \
            w->x54C = w->x534;                                                                     \
            w->x518 = w->x504;                                                                     \
            w->x51C = w->x508;                                                                     \
            w->x520 = em->plDist2;                                                                 \
            }                                                                                      \
        }                                                                                          \
    }

// Line-of-sight probe from a point beside the enemy (alternating sides) to the player's head.
#define EM10_ROUTE_SIGHT_CK()                                                                      \
    if (w->x6A5 & 1) {                                                                             \
        b.x = 200.0f;                                                                              \
        b.y = 1500.0f;                                                                             \
        b.z = 0.0f;                                                                                \
    } else {                                                                                       \
        b.x = -200.0f;                                                                             \
        b.y = 1500.0f;                                                                             \
        b.z = 0.0f;                                                                                \
    }                                                                                              \
    PSMTXMultVec(em->mat, &b, &b);                                                                 \
    w->x6A5++;                                                                                     \
    c.x = pPL->pos.x;                                                                              \
    c.y = pPL->pos.y + 1500.0f;                                                                    \
    c.z = pPL->pos.z;                                                                              \
    if (!EatMgr.hitCheck(&b, &c, 0, 0, 0, 0x4000)) {                                               \
        w->flags |= 1;                                                                             \
    }

// Route bookkeeping run every frame: distances / angles to the player, partner and goto point.
void em10RouteCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec p;
    Vec q;
    Vec b;
    Vec c;
    Vec r;
    Vec nrm;
    Vec dbg;
    f32 d;
    f32 ang;
    f32 aa;
    f32 dy;
    int fl;
    int t;

    if (em->hp <= 0) {
        return;
    }
    if (em->type == 0xA || em->type == 0xD) {
        w->flags |= 4;
    }
    if (em->xFC != 0 && !(w->flags & 4) && (pG->flags_51E4 & 7) != (em->emsetNo & 7)) {
        em10RouteTargetSet(em);
        EM10_ROUTE_LOCKON();
        return;
    }
    w->x6AF = 0;
    if (w->flags & 0x8000) {
        BitOff(w->flags, 0x08000000);
        w->x534 = pPL->pos;
        w->x504 = Muku(&em->pos, &w->x534, em->rot.y, 3.1415927f);
        w->x508 = fabsf(w->x504);
        EM10_ROUTE_SIGHT_CK();
        w->x54C = w->x534;
        w->x518 = w->x504;
        w->x51C = w->x508;
        w->x520 = em->plDist2;
        em10RouteTargetSet(em);
        return;
    }
    d = SQRTF(em->plDist2);
    if (d > 4000.0f) {
        d = 4000.0f;
    }
    d *= 0.00025f;
    if (w->pShield || em->type == 0xA || em->type == 0xD) {
        switch ((u8) (em->emsetNo % 3)) {
        case 0:
        default:
            w->x6AC = 0;
            break;
        case 1:
            w->x6AC = 7;
            break;
        case 2:
            w->x6AC = 8;
            break;
        }
    }
    switch (w->x6AC) {
    case 0:
    default:
        p.x = 0.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 1:
        p.x = d * 2000.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 2:
        p.x = d * -2000.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 3:
        p.x = d * 3000.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 4:
        p.x = d * -3000.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 5:
        p.x = d * 4000.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 6:
        p.x = d * -4000.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 7:
        p.x = d * 1000.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 8:
        p.x = d * -1000.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 9:
        p.x = d * 2500.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    case 10:
        p.x = d * -2500.0f;
        p.y = 500.0f;
        p.z = 0.0f;
        break;
    }
    PSMTXMultVec(pPL->mat, &p, &p);
    r = pPL->pos;
    r.y += 500.0f;
    if (SatMgr.hitCheck(&r, &p, &q, 0, 0, 0)) {
        PSVECSubtract(&r, &q, &nrm);
#line 25754 "D:/Bio4/Prog/em10.cpp"
        VECNormalize(&nrm, &nrm);
        PSVECScale(&nrm, &nrm, 350.0f);
        PSVECAdd(&q, &nrm, &p);
        if (pG->flags_60 & 0x4000) {
            Draw_line3d(&r, &q, 0xFFFFFFFF, 0);
            Draw_line3d(&r, &p, 0xFF00FF00, 0);
        }
    }
    r = p;
    if (em->plDist2 < 12250000.0f && (s16) w->x660 == 0) {
        if ((s16) w->x680 <= 0x12B) {
            ang = Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f);
            if (ang > 0.0f) {
                if (ang > 0.7853982f) {
                    ang -= 0.7853982f;
                } else {
                    ang = 0.0f;
                }
            } else {
                if (ang < -0.7853982f) {
                    ang += 0.7853982f;
                } else {
                    ang = 0.0f;
                }
            }
            aa = fabsf(ang);
            r.x = ang * 3500.0f;
            r.y = 500.0f;
            r.z = aa * 3000.0f;
            PSMTXMultVec(pPL->mat, &r, &r);
            p = pPL->pos;
            p.y += 500.0f;
            if (SatMgr.hitCheck(&p, &r, &q, 0, 0, 0)) {
                PSVECSubtract(&p, &q, &nrm);
#line 25789 "D:/Bio4/Prog/em10.cpp"
                VECNormalize(&nrm, &nrm);
                PSVECScale(&nrm, &nrm, 350.0f);
                PSVECAdd(&q, &nrm, &r);
            }
            if (aa < 0.3926991f) {
                w->x680 = 0;
            } else {
                w->x680++;
            }
        }
    } else {
        w->x680 = 0;
    }
    FSet(w->x524, RouteCkPosToPosDis(&em->pos, &pPL->pos));
    w->x52C = RouteCkPosToPosDis(&w->x4D8, &pPL->pos);
    w->x530 = RouteCkPosToPosDis(&w->x4D8, &em->pos);
    if ((em->type == 0xA || em->type == 0xD) && w->x5EC == 0 && (w->flags & 0x100)) {
        r = w->x5F0;
    }
    BitOff(w->flags, 3);
    fl = 0;
    if (pPL->pos.y > em->pos.y + 1000.0f) {
        fl = 1;
    }
    RouteCkToPos(em, &r, &w->x534, fl, &w->x650);
    w->x504 = Muku(&em->pos, &w->x534, em->rot.y, 3.1415927f);
    w->x508 = fabsf(w->x504);
    if (em->xFC == 0) {
        w->x504 = 0.0f;
        w->x508 = 0.0f;
        em->plDist2 = 100000000.0f;
    }
    EM10_ROUTE_SIGHT_CK();
    if (em->x3D0 == 5 && (w->flags & 1) && w->x524 < 5000.0f) {
        em->x3D0 = 0;
    }
    if (pSUB) {
        RouteCkToEm(em, pSUB, &w->x540, 0);
        FSet(w->x528, RouteCkPosToPosDis(&em->pos, &pSUB->pos));
        w->x514 = (em->pos.x - pSUB->pos.x) * (em->pos.x - pSUB->pos.x) + (em->pos.z - pSUB->pos.z) * (em->pos.z - pSUB->pos.z);
        w->x50C = Muku(&em->pos, &w->x540, em->rot.y, 3.1415927f);
        w->x510 = fabsf(w->x50C);
        if (em->xFC == 0) {
            w->x50C = 0.0f;
            w->x510 = 0.0f;
            w->x514 = 100000000.0f;
        }
        b.x = em->pos.x;
        b.y = em->pos.y + 1500.0f;
        b.z = em->pos.z;
        c.x = pSUB->pos.x;
        c.y = pSUB->pos.y + 1500.0f;
        c.z = pSUB->pos.z;
        if (!EatMgr.hitCheck(&b, &c, 0, 0, 0, 0)) {
            w->flags |= 2;
        }
    } else {
        w->flags &= ~0x08000000;
        w->x540 = w->x534;
        w->x528 = 100000000.0f;
        w->x514 = 10000000000000000.0f;
        w->x50C = 0.0f;
        w->x510 = 0.0f;
    }
    if (w->flags & 0x20000000) {
        w->x4EC = w->x4D8;
        w->flags |= 0x04000000;
    }
    if (w->x5EC != 0) {
        w->x4EC = w->x5F0;
        w->flags |= 0x04000000;
    }
    if (!(w->flags & 0x04000000)) {
        if (w->flags & 0x00800000) {
            if (em10SetWanderRoute(em)) {
                em10RouteTargetSet(em);
                return;
            }
            w->x638 = 0;
            w->flags &= ~0x00800000;
        }
    }
    t = em10RouteTargetSet(em);
    switch (t) {
    case 0:
    default:
        w->flags &= ~0x08000000;
        w->x54C = w->x534;
        w->x518 = w->x504;
        w->x51C = w->x508;
        w->x520 = em->plDist2;
        if (w->x644 != 0) {
            if (em->plDist2 > 36000000.0f) {
                w->x644 = 0;
            }
            RouteCkEscEm(em, pPL, &w->x54C);
        }
        break;
    case 1:
        w->flags |= 0x08000000;
        w->x54C = w->x540;
        w->x518 = w->x50C;
        w->x51C = w->x510;
        w->x520 = w->x514;
        break;
    }
    if (w->x6B9) {
        w->flags |= 0x04000000;
        w->x4EC.x = 109672.0f;
        w->x4EC.y = 500.0f;
        w->x4EC.z = -48196.0f;
        d = (em->pos.x - w->x4EC.x) * (em->pos.x - w->x4EC.x) + (em->pos.z - w->x4EC.z) * (em->pos.z - w->x4EC.z);
        if (d < 4000000.0f) {
            w->x6B9 = 0;
        }
        p = em->pos;
        p.y += 500.0f;
        if (SatMgr.hitCheck(&p, &w->x4EC, 0, 0, 0, 0) == 0) {
            w->x6B9 = 0;
        }
    }
    if (w->x6BA) {
        w->flags |= 0x04000000;
        w->x4EC.x = 113323.0f;
        w->x4EC.y = 500.0f;
        w->x4EC.z = -51441.0f;
        d = (em->pos.x - w->x4EC.x) * (em->pos.x - w->x4EC.x) + (em->pos.z - w->x4EC.z) * (em->pos.z - w->x4EC.z);
        if (d < 4000000.0f) {
            w->x6BA = 0;
        }
        p = em->pos;
        p.y += 500.0f;
        if (SatMgr.hitCheck(&p, &w->x4EC, 0, 0, 0, 0) == 0) {
            w->x6BA = 0;
        }
    }
    if (w->flags & 0x04000000) {
        RouteCkToPos(em, &w->x4EC, &w->x54C, 1, &w->x650);
        w->x518 = Muku(&em->pos, &w->x54C, em->rot.y, 3.1415927f);
        w->x51C = fabsf(w->x518);
        w->x520 = (em->pos.x - w->x4EC.x) * (em->pos.x - w->x4EC.x) + (em->pos.z - w->x4EC.z) * (em->pos.z - w->x4EC.z);
        w->x6AF = 1;
        if (pGS->flags_60 & 0x4000) {
            dbg = em->pos;
            dbg.y += 250.0f;
            Draw_line3d(&dbg, &w->x4EC, 0xFFFF0000, 0);
        }
    }
    EM10_ROUTE_LOCKON();
    if (pG->flags_60 & 0x4000) {
        dbg = em->pos;
        dbg.y += 250.0f;
        Draw_line3d(&dbg, &w->x54C, 0xFFFFFF40, 0);
        Draw_line3d(&dbg, &r, 0xFF0000FF, 0);
    }
}

extern "C" int em10GetGoSub(cEm10* em)
{
    int n = 0;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        if (!(EM10_WK(e)->flags & 0x08000000)) {
            continue;
        }
        n++;
    }
    return n;
}

extern "C" int em10RouteTargetSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (!pSUB) {
        return 0;
    }
    if (w->flags & 0x8000) {
        return 0;
    }
    if (w->flags & 0x04000000) {
        return 0;
    }
    if (pG->room_id != 0x30F && em->xFC == 1) {
        if (em->xFD == 0 || em->xFD == 0x1B || em->xFD == 1) {
            return 0;
        }
    }
    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01010000 || (G_ROOM_ID32 & 0xFFFF0000) == 0x01110000 ||
        (G_ROOM_ID32 & 0xFFFF0000) == 0x04000000) {
        if (pSUB->pos.y > 6000.0f) {
            return 0;
        }
    }
    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x030F0000 && w->wepType == 0) {
        return 0;
    }
    if (w->wepType == 8) {
        return 0;
    }
    if (em->type == 6) {
        return 0;
    }
    if (pG->flags_500C & 0x800) {
        return 0;
    }
    if (pG->flags_5010 & 0x10000) {
        return 0;
    }
    if (pG->flags_5010 & 8) {
        return 0;
    }
    if (em->flags_3C8 & 0x40) {
        return 1;
    }
    if (em->type == 10) {
        return 0;
    }
    if (em->type == 13) {
        return 0;
    }
    if ((u32) em10GetGoSub(em) > 1) {
        return 0;
    }
    if (w->flags & 0x08000000) {
        return 1;
    }
    if (w->x524 < w->x528 + 2000.0f) {
        return 0;
    }
    return 1;
}

extern "C" int em10GetWanderRouteEmi(cEm10* em)
{
    EmiData* emi = (EmiData*) pG->pRoomEmi;
    int cnt;
    int i;
    int r;

    if (!emi) {
        return -1;
    }
    cnt = 0;
    // Byte-offset entry address: the `i*64 + 8` DEST_REG giv has a constant addend, so loop.c makes the
    // later `.sub` address giv (+9) the base (`addi r9,emi,9`, type at -1(r9)); `&emi->entry[i]`
    // gives a zero-addend giv that wins the combine (base +0).
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = (EmiEntry*) ((u8*) emi + 8 + i * 0x40);
        if (e->type != 1) {
            continue;
        }
        if (e->sub != 3) {
            continue;
        }
        cnt++;
    }
    if (cnt == 0) {
        return -1;
    }
    r = Rnd() % cnt;
    cnt = 0;
    // pG->pRoomEmi re-read here: the loop bound is then a gcse PRE copy (`mr r8, r9`) of the entry test's load.
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + 8 + i * 0x40);
        if (e->type != 1) {
            continue;
        }
        if (e->sub != 3) {
            continue;
        }
        if (r == cnt) {
            return i;
        }
        cnt++;
    }
    return -1;
}

extern "C" int em10GetWanderRouteEmi(cEm10* em);

u32 em10GetWanderRoute(cEm10* em)
{
    int n = em10GetWanderRouteEmi(em);
    if (n >= 0) {
        return n;
    }
    n = RouteCkGetPointNumber();
    if (n <= 0) {
        return -1;
    }
    n = Rnd() % n;
    if ((Rnd() & 3) == 0) {
        n = RouteCkGetNearPoint(&pPL->pos);
    }
    return n;
}

extern "C" void em10GetWanderRoutePos(cEm10* em, Vec* pos)
{
    Em10Work* w = EM10_WK(em);
    EmiData* emi = (EmiData*) pG->pRoomEmi;

    if (emi && (int) w->x63C >= 0 && (int) w->x63C < emi->n) {
        EmiEntry* e = &emi->entry[w->x63C];
        if ((*(u32*) e & 0xFFFF0000) == 0x01030000) {
            *pos = e->pos;
            return;
        }
    }
    RouteCkGetPoint(w->x63C, pos);
}

extern "C" void em10GetWanderRoutePos(cEm10* em, Vec* pos);

extern "C" u32 em10WanderRouteUpdate(cEm10* em, int no)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;

    // Early return: the label in front of the main path keeps its `mr r3, r31` (see docs/matching.md
    // "early return merged with the final return").
    if (no <= 0) {
        return em10GetWanderRoute(em);
    }
    em10GetWanderRoutePos(em, &pos);
    f32 d = (em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.z - pos.z) * (em->pos.z - pos.z);
    if (!(d < 2250000.0f)) {
        if (w->x634 <= 30) {
            return no;
        }
    }
    return em10GetWanderRoute(em);
}

extern "C" int em10SetWanderRoute(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;
    Vec p2;
    int n;

    n = em10WanderRouteUpdate(em, w->x63C);
    w->x63C = n;
    if (n < 0) {
        return 0;
    }
    em10GetWanderRoutePos(em, &pos);
    RouteCkToPos(em, &pos, &w->x54C, 0, 0);
    w->x518 = Muku(&em->pos, &w->x54C, em->rot.y, 3.1415927f);
    w->x51C = fabsf(w->x518);
    w->x520 = (em->pos.x - w->x54C.x) * (em->pos.x - w->x54C.x) + (em->pos.z - w->x54C.z) * (em->pos.z - w->x54C.z);
    if (pGS->flags_60 & 0x4000) {
        p2 = em->pos;
        p2.y += 250.0f;
        Draw_line3d(&p2, &pos, 0xFFFF0000, 0);
        Draw_line3d(&p2, &w->x54C, 0xFFFFFF40, 0);
    }
    return 1;
}

int em10AtkRtnCk(cEm10* em, int a)
{
    Em10Work* w = EM10_WK(em);

    if (pG->flags_64 & 0x02000000) {
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
        em->xFC = 1;
        return 1;
    }
    if (a == 0) {
        if (em10BackCk(em)) {
            return 1;
        }
    }
    if (w->x644 != 0) {
        return 0;
    }
    if (w->x696 == 0) {
        if (Ctrl12Ck(w->pCtrl12, 6)) {
            return 0;
        }
    }
    if (pG->flags_5010 & 0x00200000) {
        return 0;
    }
    if (em10ParasiteAtkCk(em)) {
        return 1;
    }
    if (w->x58C) {
        return 0;
    }
    if (em->type != 10 && em->type != 13) {
        if (w->pParasite) {
            return 0;
        }
    }
    if (em->type == 0x16) {
        return 0;
    }
    if (em10ShieldAtkCk(em)) {
        return 1;
    }
    if (em10AxeAtkCk(em)) {
        return 1;
    }
    if (em10SukiAtkCk(em)) {
        return 1;
    }
    if (em10ScytheAtkCk(em)) {
        return 1;
    }
    if (em10ClawAtkCk(em)) {
        return 1;
    }
    if (em10ClawCriAtkCk(em)) {
        return 1;
    }
    if (em10ShotBowgunCk(em)) {
        return 1;
    }
    if (em10ShotRocketCk(em)) {
        return 1;
    }
    if (em10ShotGatlingCk(em)) {
        return 1;
    }
    if (em10ThrowAxeCk(em)) {
        return 1;
    }
    if (em10ThrowBombCk(em)) {
        return 1;
    }
    if (pG->room_id == 0x21B) {
        if (pG->flags_5014 & 0x08000000) {
            return 0;
        }
    }
    if (em10CsawAtkCk(em)) {
        return 1;
    }
    if (em10CatchPLRtnCk(em)) {
        return 1;
    }
    if (em10CatchSubRtnCk(em)) {
        return 1;
    }
    return 0;
}

extern "C" int em10CatchPLRtnCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    int hit;
    u8 r;
    int rr;

    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if (w->x67C != 0) {
        return 0;
    }
    if (pG->flags_5010 & 0x8000) {
        return 0;
    }
    if (w->pShield != 0) {
        return 0;
    }
    // Four separate ifs, not an `||` chain: cse follows at most 9 conditional jumps per extended
    // block (PATHLENGTH 10), so the x3E0 test below is the 10th and the pWep block starts a fresh
    // ebb -- `w->flags` is reloaded there (not merged with em->x3E0) exactly like the target.
    if (em->type == 0xA) {
        return 0;
    }
    if (em->type == 0xD) {
        return 0;
    }
    if (em->type == 2) {
        return 0;
    }
    if (em->type == 0x18) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (!(em->x3E0 & 1)) {
        return 0;
    }
    if (w->pWep != 0 && !(w->flags & 0x08000000)) {
        if (w->wepType != 9) {
            return 0;
        }
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 300.0f) {
        return 0;
    }
    if (!(em->flags_3C8 & 0x20) && w->x664 == 0 && !(w->flags & 0x08000000) && !Ctrl12Ck(w->pCtrl12, 6) && pG->x4F88 > 3 && em->plDist2 < 12250000.0f && em->plDist2 > 4000000.0f && w->x508 < 0.7853982f && fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f)) < 0.5235988f) {
        if ((Rnd() & 1) == 0) {
            EmRoutineSet(em, 1, 0x39, 0, 0);
            return 1;
        }
        rr = Rnd();
        w->x664 = rr % 300 + 300;
    }
    if (em->plDist2 > 1210000.0f) {
        if (!em10PlRunCk(em)) {
            return 0;
        }
        if (em->plDist2 > 4000000.0f) {
            return 0;
        }
    }
    a = em->pos;
    b = pPLS->pos;
    a.y += 1500.0f;
    b.y += 1500.0f;
    hit = SatMgr.hitCheck(&a, &b, 0, 0, 0, 0);
    if (hit) {
        return 0;
    }
    if (pG->x4F88 <= 1 && !EM_RTN(em, 1, 0x1B) && (r = Rnd() % 10, r > 4)) {
        w->x67C = 30;
        EmRoutineSet(em, 1, 0x1B, hit, hit);
        return 1;
    }
    EmRoutineSet(em, 1, 0x33, 0, 0);
    return 1;
}

extern "C" int em10CatchSubRtnCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    int hit;
    int rtn;

    if (!pSUB) {
        return 0;
    }
    if (w->x58C) {
        return 0;
    }
    if (w->wepType == 4) {
        return 0;
    }
    if (pG->flags_500C & 0x800) {
        return 0;
    }
    if (pG->flags_5010 & 8) {
        return 0;
    }
    if (w->pShield) {
        return 0;
    }
    if (em->type == 10 || em->type == 13 || em->type == 2 || em->type == 0x18) {
        return 0;
    }
    rtn = 1;
    if (pG->flags_5010 & 0x10000) {
        return 0;
    }
    if (pG->flags_5014 & 0x00800000) {
        return 0;
    }
    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if (w->x67C != 0) {
        return 0;
    }
    if (pSUB->hp <= 0) {
        return 0;
    }
    if (!(w->flags & 2)) {
        return 0;
    }
    if (fabsf(em->pos.y - pSUB->pos.y) > 300.0f) {
        return 0;
    }
    if (w->x514 > 1210000.0f) {
        return 0;
    }
    a = em->pos;
    b = pSUB->pos;
    a.y += 1500.0f;
    b.y += 1500.0f;
    hit = SatMgr.hitCheck(&a, &b, 0, 0, 0, 0);
    if (hit) {
        return 0;
    }
    em->xFD = 0x33;
    em->xFE = hit;
    em->xFC = rtn;
    em->xFF = rtn;
    return 1;
}

int em10CatchCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx inv;
    Vec v;
    Vec a;
    Vec b;
    Mtx m;

    if (em10DeadCk(pPL)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (!(em->seFlags28B & 2)) {
        return 0;
    }
    if (!(w->flags & 1)) {
        return 0;
    }
    if (w->x6C1 > 0x2D) {
        return 0;
    }
    if (pG->flags_5010 & 0x8000) {
        return 0;
    }
    if ((w->flags & 0x80) && (em->flags_3C8 & 0x100000)) {
        return 0;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.y < -500.0f || v.y > 500.0f) {
        return 0;
    }
    if (v.z < 0.0f || v.z > 900.0f) {
        return 0;
    }
    if (!(v.x > -400.0f)) {
        return 0;
    }
    if (!(v.x < 400.0f)) {
        return 0;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos));
    TransMatrix(m, &em->pos);
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    pPL->dmg.set(0, 2);
    em->dmg.set(0, 2);
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    Ctrl12Set(w->pCtrl12, 6, 30);
    Ctrl12Set(w->pCtrl12, 8, 120);
    return 1;
}

int em10CatchSubCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx inv;
    Vec v;
    Vec a;
    Vec b;
    Mtx m;

    if (pSUB == 0) {
        return 0;
    }
    if (em10DeadCk(pSUB)) {
        return 0;
    }
    if (pSUB->hp <= 0) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (!(em->seFlags28B & 2)) {
        return 0;
    }
    if (!(w->flags & 2)) {
        return 0;
    }
    if (pG->flags_5010 & 0x00010000) {
        return 0;
    }
    if (pG->flags_5014 & 0x00800000) {
        return 0;
    }
    if (pG->flags_500C & 0x800) {
        return 0;
    }
    if (pG->flags_5010 & 8) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pSUB->pos, &v);
    if (v.y < -500.0f || v.y > 500.0f) {
        return 0;
    }
    if (v.z < 0.0f || v.z > 900.0f) {
        return 0;
    }
    if (!(v.x > -400.0f && v.x < 400.0f)) {
        return 0;
    }
    pSUB->dmg.set(0, 2);
    em->dmg.set(0, 2);
    a = em->pos;
    b = pSUB->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos));
    TransMatrix(m, &em->pos);
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    if (pSUB->id == 4) {
        BitOn(pG->flags_5010, 0x00010000);
        BitOn(pG->flags_5014, 0x20000000);
        EM_RTN_SET(em, 1, 0x35);
    } else {
        BitOn(pG->flags_5010, 0x00010000);
        BitOn(pG->flags_5014, 0x20000000);
        EM_RTN_SET(em, 1, 0x3A);
    }
    return 1;
}

int em10CsawHitCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    EmAtkInfo info;
    Vec a;
    Vec b;
    int hit;

    if (!(em->seFlags28B & 1)) {
        return 0;
    }
    if (w->pWep == 0) {
        return 0;
    }
    info = Em10AtkTbl[12];
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 500.0f;
    b.x = 0.0f;
    b.y = 0.0f;
    b.z = -1000.0f;
    PSMTXMultVec(w->pWep->mat, &a, &a);
    PSMTXMultVec(w->pWep->mat, &b, &b);
    hit = EmAtkHitCk(&info, &a, &b, 0);
    if (hit & 1) {
        U16Set(pG->pl_life, 1);
        pPL->dmg.set(0, 0x80);
        em->dmg.set(0, 0x80);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        return 1;
    } else if (hit & 2) {
        LifeDownSet(pSUB, 9999, 0);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        return 0;
    } else {
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 0.0f;
        b.x = 0.0f;
        b.y = 0.0f;
        b.z = -1000.0f;
        PSMTXMultVec(w->pWep->mat, &a, &a);
        PSMTXMultVec(w->pWep->mat, &b, &b);
        hit = EmAtkHitCk(&info, &a, &b, 0);
        if (hit & 1) {
            U16Set(pG->pl_life, 1);
            pPL->dmg.set(0, 0x80);
            em->dmg.set(0, 0x80);
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            return 1;
        } else if (hit & 2) {
            LifeDownSet(pSUB, 9999, 0);
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            return 0;
        } else {
            a.x = 0.0f;
            a.y = 0.0f;
            a.z = -200.0f;
            b.x = 0.0f;
            b.y = 0.0f;
            b.z = -1000.0f;
            PSMTXMultVec(w->pWep->mat, &a, &a);
            PSMTXMultVec(w->pWep->mat, &b, &b);
            a.y = em->pos.y + 500.0f;
            hit = EmAtkHitCk(&info, &a, &b, 0);
            if (hit & 1) {
                U16Set(pG->pl_life, 1);
                pPL->dmg.set(0, 0x80);
                em->dmg.set(0, 0x80);
                VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xD, 1);
                return 1;
            } else if (hit & 2) {
                LifeDownSet(pSUB, 9999, 0);
                VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xD, 1);
                return 0;
            }
        }
    }
    return 0;
}

int em10LostHeadCk(cEm10* em)
{
    if (pSys->region != 0) {
        return 1;
    }
    switch (em->type) {
    case 0:
    case 1:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
        if (!GetEm10EyeEffectEnable()) {
            return 0;
        }
        return 1;
    case 2:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    default:
        return 1;
    }
}

// Head loss: a = 0/1 shot off (checked), 2 parasite emerges, 3 kick; b = 1 suppresses the blood burst
// when the head cannot be lost. Returns 1 when the head is gone.
int em10LostHead(cEm10* em, int a, int b)
{
    Em10Work* w = EM10_WK(em);
    int hit;
    int paras;
    int no;
    Mtx m;
    Vec spd;

    if ((a == 0 || a == 1) && !(hit = em10LostHeadCk(em))) {
        SndStop(w->x5B8, 0);
        SndStop(w->x5BC, 0);
        SndCall(8, 7, &em->pos, em->id, 0, em);
        if (a == 3) {
            EstSet((int) em, -1, 0, 0, 0x10, 0x78, 0, 0, (u32) em, (void*) hit);
        } else {
            EstSet((int) em, -1, 0, 0, 0x10, 0xA0, 0, 0, (u32) em, (void*) hit);
        }
        return 0;
    }
    no = em->type == 6;
    if (em->type == 0xA) {
        no = 1;
    }
    if (em->type == 0xD) {
        no = 1;
    }
    if (em->type == 2) {
        no = 1;
    }
    if ((pG->room_id32 & 0xFFFF0000) == 0x01000000) {
        no = 1;
    }
    if (w->wepType == 4) {
        no = 1;
    }
    if (em->flags_3C8 & 0x200) {
        if (w->x6C5 == 1) {
            no = 1;
        }
        if (w->x6C5 == 2) {
            no = 1;
        }
    }
    if ((w->flags & 0x80) && !w->pParasite) {
        no = 1;
    }
    if (no) {
        if (b == 0) {
            em10BloodSet(em, 0);
        }
        return 0;
    }
    paras = 0;
    if (w->pParasite) {
        paras = 1;
    }
    switch ((u32) a) {
    case 0:
    default:
        // Dead test falling through into case 1 (the store is deleted by flow, the compare stays
        // live because gcse PRE reuses it for the `a == 3` after em10HeadSet): its block gives the
        // case-1 label a second predecessor where the compare is already computed, so PRE inserts
        // `cmpwi cr4, a, 3` at the end of the dispatch block and of the case-2 else arm and deletes
        // the join's compare. Any dead local store works here; the tree still needs case 0 and
        // case 1 as separate nodes (`cmpwi 1; beq; cmplwi 1; blt`).
        if (a == 3) {
            paras = 0;
        }
    case 1:
        SndStop(w->x5B8, 0);
        SndStop(w->x5BC, 0);
        SndCall(8, 7, &em->pos, em->id, 0, em);
        w->flags |= 0x80;
        EmSetDie(em);
        EmReserveDropItem(em);
        em10SetPoint(em);
        break;
    case 2:
        SndStop(w->x5B8, 0);
        SndStop(w->x5BC, 0);
        SndCall(8, 7, &em->pos, em->id, 0, em);
        w->flags |= 0x80;
        if ((em->flags_3C8 & 0x00100000) && !paras && !Ctrl12CntCk(w->pCtrl12, 4, 2)) {
            u8 r;
            w->x6AE = 1;
            Ctrl12CntAdd(w->pCtrl12, 4, 1);
            r = Rnd() % 3;
            w->x658 = r * 30 + 1;
            em->hp = 3000;
        } else {
            EmSetDie(em);
            EmReserveDropItem(em);
            em10SetPoint(em);
        }
        break;
    }
    em10HeadSet(em, 1);
    if (a == 3) {
        EstSet((int) em, -1, 0, 0, 0x10, 0x78, 0, 0, (u32) em, 0);
    } else {
        hit = Ctrl12Ck(w->pCtrl12, 0xB);
        if (hit) {
            EstSet((int) em, -1, 0, 0, 0x10, 0x34, 0, 0, (u32) em, 0);
        } else {
            Ctrl12Set(w->pCtrl12, 0xB, 0x2D);
            EstSet((int) em, -1, 0, 0, 0x10, 3, 0, 0, (u32) em, (void*) hit);
        }
    }
    EstSet((int) em, -1, 0, 0, 0x10, 6, 0, 0, (u32) em, 0);
    EffectEspDelete(0, w->x69D, (u32) em, 0);
    EffectEspgenDelete(0, w->x69D, (int) em);
    EffectEfmDelete(0, w->x69D, (int) em);
    if (w->x1A0) {
        w->x1A0->be_flag &= ~8;
    }
    if (w->x1A4) {
        w->x1A4->be_flag &= ~8;
    }
    if (w->x1A8) {
        w->x1A8->be_flag &= ~8;
    }
    if (w->x1AC) {
        w->x1AC->be_flag &= ~8;
    }
    if (w->x1B4) {
        w->x1B4->be_flag &= ~8;
    }
    if (w->x1B8) {
        w->x1B8->be_flag &= ~8;
    }
    if (w->x1BC) {
        w->x1BC->be_flag &= ~8;
    }
    if (w->x178) {
        switch (w->x6B4) {
        case 0:
            break;
        default:
            PSMTXRotRad(m, 'y', GetXZAngle(&pPL->pos, &em->pos));
            spd.x = 0.0f;
            spd.y = 40.0f;
            spd.z = -50.0f;
            PSMTXMultVecSR(em->mat, &spd, &spd);
            ((cObj12*) w->x178)->setFall(&spd, 2);
            w->x178 = 0;
            w->x6B4 = 0;
            break;
        case 3:
        case 4:
            ObjMgr.destroy(w->x178);
            w->x178 = 0;
            w->x6B4 = 0;
            break;
        }
    }
    if (w->x17C) {
        PSMTXRotRad(m, 'y', GetXZAngle(&pPL->pos, &em->pos));
        spd.x = 0.0f;
        spd.y = 40.0f;
        spd.z = -50.0f;
        PSMTXMultVecSR(em->mat, &spd, &spd);
        ((cObj12*) w->x17C)->setFall(&spd, 3);
        w->x17C = 0;
    }
    em->setWeaponFall();
    if (w->pShield) {
        w->pShield->setFall(20.0f, 0);
        w->pShield = 0;
    }
    return 1;
}

extern "C" void em10CallVoiceSe(cEm10* em, u16 no)
{
    Em10Work* w = EM10_WK(em);
    int idx;

    SndStop(w->x5B8, 0);
    SndStop(w->x5BC, 0);
    if (w->flags & 0x80) {
        return;
    }
    switch (em->type) {
    case 0:
        idx = 0;
        break;
    default:
        idx = 0;
        break;
    case 1:
        idx = 1;
        break;
    case 2:
        idx = 2;
        break;
    case 3:
        idx = 3;
        break;
    case 4:
        idx = 4;
        break;
    case 5:
        idx = 2;
        break;
    case 6:
        idx = 5;
        break;
    case 7:
        idx = 2;
        break;
    case 8:
        idx = 2;
        break;
    case 9:
        idx = 2;
        break;
    case 10:
        idx = 2;
        break;
    case 13:
        idx = 2;
        break;
    case 14:
        idx = 2;
        break;
    case 15:
        idx = 2;
        break;
    case 16:
        idx = 2;
        break;
    case 17:
        idx = 2;
        break;
    case 18:
        idx = 2;
        break;
    case 19:
        idx = 2;
        break;
    case 20:
        idx = 2;
        break;
    case 21:
        idx = 2;
        break;
    case 23:
        idx = 2;
        break;
    case 24:
        idx = 2;
        break;
    case 25:
        idx = 2;
        break;
    }
    w->x5B8 = Ctrl11StopAndSetSe(w->pCtrl11, em, Rnd() % 20 + 20, no, idx);
    w->x67E = Rnd() % 120 + 120;
}

void em10CallVoiceSe2(cEm10* em, int no, int a)
{
    Em10Work* w = EM10_WK(em);

    SndStop(w->x5B8, 0);
    SndStop(w->x5BC, 0);
    if (!(w->flags & 0x80)) {
        w->x5B8 = Ctrl11SetSe2I(w->pCtrl11, em, Rnd() % 20 + 20, no, 6, a);
        w->x67E = Rnd() % 120 + 120;
    }
}

void em10BreathSe(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->x67E) {
        w->x67E--;
    } else {
        w->x67E = Rnd() % 120 + 120;
        if (!(w->flags & 0x80)) {
            w->x5BC = Ctrl11SetSe(w->pCtrl11, em, Rnd() % 20 + 20, w->se6CD, 6);
        }
    }
}

extern "C" void em10CsawSignSe(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if ((int) w->flags >= 0) {
        return;
    }
    if (w->wepType != 4) {
        return;
    }
    if ((s16) pG->pl_life <= 0) {
        return;
    }
    if (em->plDist2 > 25000000.0f) {
        if (w->x65C) {
            w->x65C--;
            return;
        }
        w->x65C = (u8) (Rnd() % 150) + 150;
        w->x65E = 150;
        w->x5C4 = SndCall(6, 0x4B, &em->pos, 0, 0, em);
    } else {
        if (w->x65E != 0) {
            return;
        }
        SndStop(w->x5C4, 0);
        w->x5C4 = SndCall(6, 0x3E, &em->pos, 0, 0, em);
        w->x65E = 150;
        w->x65C = (u8) (Rnd() % 150) + 150;
    }
}

int em10ModelInit(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (!em->modelInit(w->mot[1], w->mot[0])) {
        pLog->err(0, 0, "EM10 pEm->modelInit() failed.");
        return 0;
    }
    w->x190 = 0;
    w->x194 = 0;
    w->x198 = 0;
    if (em->type == 6) {
        if (w->mot[16] && w->mot[17]) {
            w->x190 = ModInfoMgr.create(w->mot[16], w->mot[17]);
            if (w->x190) {
                em->addModel(w->x190);
            }
        }
        em10ClothPartsSet(em, 0);
        em10GoodsPartsSet(em, 0);
    }
    w->x18C = 0;
    em10HeadSet(em, 0);
    w->x184 = 0;
    w->x188 = 0;
    em10HandSet(em, 0);
    em->pFootShadowTbl = &Em10_fs_tbl;
    w->pHead = 0;
    em10SetAccesory(em);
    em10WeaponInit(em);
    em10ShieldSet(em);
    em10SackSet(em);
    return 1;
}
extern "C" void Em10SetSeTbl(cEm10* em, int type)
{
    Em10Work* w = EM10_WK(em);

    switch ((u32) type) {
    case 0:
    default:
        w->se6C6 = 0x16;
        w->se6C7 = 0x17;
        w->se6C8 = 0x18;
        w->se6C9 = 0x19;
        w->se6CA = 0x1A;
        w->se6CB = 0x1B;
        w->se6CC = 0x1C;
        w->se6CD = 0x25;
        w->se6CE = 0x47;
        w->se6CF = 0x48;
        w->se6D0 = 0x49;
        w->se6D1 = 0x4A;
        w->se6D2 = 0x4B;
        w->se6D3 = 0x4C;
        w->se6D4 = 0x4D;
        w->se6D5 = 0x4E;
        w->se6D6 = 0x5B;
        w->se6D7 = 0xB5;
        w->se6D8 = 0x15;
        break;
    case 1:
        w->se6C6 = 0x1E;
        w->se6C7 = 0x1F;
        w->se6C8 = 0x20;
        w->se6C9 = 0x21;
        w->se6CA = 0x22;
        w->se6CB = 0x23;
        w->se6CC = 0x24;
        w->se6CD = 0x27;
        w->se6CE = 0x51;
        w->se6CF = 0x52;
        w->se6D0 = 0x53;
        w->se6D1 = 0x54;
        w->se6D2 = 0x55;
        w->se6D3 = 0x56;
        w->se6D4 = 0x57;
        w->se6D5 = 0x58;
        w->se6D6 = 0x5C;
        w->se6D7 = 0xB6;
        w->se6D8 = 0x1D;
        break;
    case 2:
        w->se6C6 = 0x2E;
        w->se6C7 = 0x2F;
        w->se6C8 = 0x30;
        w->se6C9 = 0x31;
        w->se6CA = 0x32;
        w->se6CB = 0x33;
        w->se6CC = 0x34;
        w->se6CD = 0x29;
        w->se6CE = 0x65;
        w->se6CF = 0x66;
        w->se6D0 = 0x67;
        w->se6D1 = 0x68;
        w->se6D2 = 0x69;
        w->se6D3 = 0x6A;
        w->se6D4 = 0x6B;
        w->se6D5 = 0x6C;
        w->se6D6 = 0x5D;
        w->se6D7 = 0xB7;
        w->se6D8 = 0x2D;
        break;
    case 3:
        w->se6C6 = 0x36;
        w->se6C7 = 0x37;
        w->se6C8 = 0x38;
        w->se6C9 = 0x39;
        w->se6CA = 0x3A;
        w->se6CB = 0x3B;
        w->se6CC = 0x3C;
        w->se6CD = 0x2B;
        w->se6CE = 0x6F;
        w->se6CF = 0x70;
        w->se6D0 = 0x71;
        w->se6D1 = 0x72;
        w->se6D2 = 0x73;
        w->se6D3 = 0x74;
        w->se6D4 = 0x75;
        w->se6D5 = 0x76;
        w->se6D6 = 0x5E;
        w->se6D7 = 0xB8;
        w->se6D8 = 0x35;
        break;
    }
    if (em->type == 6) {
        w->se6C6 = 0xE;
    }
}

extern "C" void em10WeaponInit(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    w->wepType = 0;
    w->wep2Type = 0;
    w->pWep = 0;
    w->pWep2 = 0;
    if (em->type == 0xA || em->type == 0xD || em->type == 2) {
        return;
    }
    if (em->flags_3C8 & 0x40000000) {
        if (w->x6C5 == 0) {
            if (w->mot[43] && w->mot[44]) {
                w->wepType = 5;
            }
        } else {
            w->wepType = 0xC;
        }
    }
    if (w->mot[69] && w->mot[70] && (em->flags_3C8 & 0x04000000) && w->pWep == 0) {
        if (w->x6C5 == 2) {
            w->wepType = 0xF;
        } else {
            w->wepType = 6;
        }
    }
    if (w->mot[41] && w->mot[42] && w->x6C5 == 0 && (s32) em->flags_3C8 < 0 && w->pWep == 0) {
        w->wepType = 1;
    }
    if (w->mot[65] && w->mot[66] && (em->flags_3C8 & 0x20000000) && w->pWep == 0) {
        if (w->x6C5 == 0) {
            if (!(em->flags_3C8 & 0x2000)) {
                w->wepType = 2;
            }
        } else {
            em->flags_3C8 &= ~0x2000;
            w->wepType = 0xB;
        }
    }
    if (w->mot[63] && w->mot[64] && (em->flags_3C8 & 0x08000000) && w->pWep == 0 && !(em->flags_3C8 & 0x2000)) {
        if (w->x6C5 == 2) {
            w->wepType = 2;
        } else {
            w->wepType = 3;
        }
    }
    if (w->mot[77] && w->mot[78] && (em->flags_3C8 & 0x4000) && w->pWep == 0 && !(em->flags_3C8 & 0x2000)) {
        w->wepType = 0xA;
    }
    if (w->mot[67] && w->mot[68] && (em->flags_3C8 & 0x10000000) && w->pWep == 0) {
        w->wepType = 4;
    }
    if (w->mot[71] && w->mot[72] && (em->flags_3C8 & 0x800) && w->pWep == 0) {
        if (w->x6C5 == 2) {
            w->wepType = 0x10;
        } else {
            w->wepType = 7;
        }
    }
    if (w->mot[73] && w->mot[74] && em->type == 6) {
        em->flags_3C8 |= 0x0001A000;
    }
    if ((em->flags_3C8 & 0x8000) && w->pWep == 0) {
        w->x6B5 = 2;
        if (!(em->flags_3C8 & 0x2000)) {
            w->wepType = 8;
        }
    }
    if ((em->flags_3C8 & 0x00020000) && w->pWep == 0) {
        if (!(em->flags_3C8 & 0x2000)) {
            w->wepType = 9;
        }
        em->flags_3C8 &= ~0x00100000;
    }
    w->pWep = em10MakeWeapon(em, w->wepType);
    if (w->pWep == 0) {
        w->wepType = 0;
    }
    em10WeaponSet(em);
    em10WeaponSet2(em);
}

// One hanging accessory (cObj12) on the body: sack / lantern / bucket etc.
#define EM10_ACC_OBJ12(bin, tpl, r, py, pz, kind)                                                  \
    pos.x = 0.0f;                                                                                  \
    pos.y = py;                                                                                    \
    pos.z = pz;                                                                                    \
    r.x = 0.0f;                                                                                    \
    r.y = 0.0f;                                                                                    \
    r.z = 0.0f;                                                                                    \
    w->x178 = SetObj12(bin, tpl, &pos, &r);                                                        \
    if (w->x178) {                                                                                 \
        ((cObj12*) w->x178)->setParent(em, 4, 1);                                                  \
        w->x6B4 = kind;                                                                            \
    }

// One extra model part (hat, belt, goods ...) added to the body.
#define EM10_ACC_PARTS(bin, tpl, dst)                                                              \
    info = ModInfoMgr.create(bin, tpl);                                                            \
    em->addModel(info);                                                                            \
    dst = (cModel*) info;

// Sets up the accessory objects / parts selected by the flags_3C8 bits (per enemy type).
extern "C" void em10SetAccesory(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;
    Vec rot;
    Vec rot2;
    cModelInfo* info;

    w->x178 = 0;
    w->x6B4 = 0;
    w->x17C = 0;
    w->x1A0 = 0;
    w->x1A4 = 0;
    w->x1A8 = 0;
    w->x1AC = 0;
    w->x1B0 = 0;
    w->x1B4 = 0;
    w->x1B8 = 0;
    w->x1BC = 0;
    w->x1C0 = 0;
    if (em->type == 0xA || em->type == 0xD) {
        return;
    }
    switch (em->type) {
    case 0:
    case 1:
    case 3:
    case 4:
    case 6:
    default:
        if (w->mot[52] && w->mot[53] && (em->flags_3C8 & 0x00800000) && !w->x178) {
            EM10_ACC_OBJ12(w->mot[52], w->mot[53], rot, 125.7f, 20.0f, 1);
        }
        if (w->mot[52] && w->mot[54] && (em->flags_3C8 & 0x00080000) && !w->x178) {
            EM10_ACC_OBJ12(w->mot[52], w->mot[54], rot2, 125.7f, 20.0f, 2);
        }
        if (w->mot[55] && w->mot[56] && (em->flags_3C8 & 0x00400000) && !w->x178) {
            EM10_ACC_OBJ12(w->mot[55], w->mot[56], rot, 5.0f, 0.0f, 3);
        }
        if (w->mot[55] && w->mot[57] && (em->flags_3C8 & 0x00040000) && !w->x178) {
            EM10_ACC_OBJ12(w->mot[55], w->mot[57], rot, 5.0f, 0.0f, 4);
        }
        if (w->mot[58] && w->mot[59] && (em->flags_3C8 & 0x200) && !w->x178) {
            EM10_ACC_OBJ12(w->mot[58], w->mot[59], rot, 87.0f, 11.0f, 5);
        }
        if (w->mot[58] && w->mot[60] && (em->flags_3C8 & 0x100) && !w->x178) {
            EM10_ACC_OBJ12(w->mot[58], w->mot[60], rot, 87.0f, 11.0f, 6);
        }
        if (w->mot[61] && w->mot[62] && (em->flags_3C8 & 0x00200000)) {
            pos.x = 0.0f;
            pos.y = 54.71f;
            pos.z = 72.73f;
            rot.x = 0.0f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            w->x17C = SetObj12(w->mot[61], w->mot[62], &pos, &rot);
            if (w->x17C) {
                ((cObj12*) w->x17C)->setParent(em, 4, 1);
            }
        }
        if (w->mot[23] && w->mot[24] && (em->flags_3C8 & 0x00400000)) {
            EM10_ACC_PARTS(w->mot[23], w->mot[24], w->x1A4);
        }
        if (w->mot[23] && w->mot[25] && (em->flags_3C8 & 0x00040000)) {
            EM10_ACC_PARTS(w->mot[23], w->mot[25], w->x1A4);
        }
        break;
    case 5:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xD:
        if ((em->flags_3C8 & 0x00040002) && !(em->flags_3C8 & 0x00081000)) {
            em->flags_3C8 |= 0x00080000;
        }
        if (em->flags_3C8 & 0x300) {
            em->flags_3C8 &= ~0x00080000;
            em->flags_3C8 |= 0x1000;
        }
        if ((em->flags_3C8 & 0x00080000) && w->mot[26] && w->mot[0]) {
            EM10_ACC_PARTS(w->mot[26], w->mot[0], w->x1A0);
        }
        if ((em->flags_3C8 & 0x1000) && w->mot[27] && w->mot[0]) {
            EM10_ACC_PARTS(w->mot[27], w->mot[0], w->x1A0);
            if (w->x18C) {
                w->x18C->be_flag &= ~8;
            }
            em->flags_3C8 &= ~0x00204000;
        }
        if ((em->flags_3C8 & 0x00800000) && w->mot[28] && w->mot[29]) {
            EM10_ACC_PARTS(w->mot[28], w->mot[29], w->x1A8);
        }
        if ((em->flags_3C8 & 0x00400000) && w->mot[30] && w->mot[31]) {
            EM10_ACC_PARTS(w->mot[30], w->mot[31], w->x1AC);
        }
        if ((em->flags_3C8 & 0x00040000) && w->mot[32] && w->mot[33]) {
            EM10_ACC_PARTS(w->mot[32], w->mot[33], w->x1B0);
        }
        if ((em->flags_3C8 & 2) && !w->x1B0 && w->mot[32]) {
            EM10_ACC_PARTS(w->mot[32], PL_ARC_PTR(em->subArc, 0x220), w->x1B0);
        }
        if ((em->flags_3C8 & 0x200) && w->mot[34] && w->mot[35]) {
            EM10_ACC_PARTS(w->mot[34], w->mot[35], w->x1B4);
            if (w->x18C) {
                w->x18C->be_flag &= ~8;
            }
        }
        if ((em->flags_3C8 & 0x100) && w->mot[36] && w->mot[37]) {
            EM10_ACC_PARTS(w->mot[36], w->mot[37], w->x1B8);
        }
        if ((em->flags_3C8 & 0x00200000) && w->mot[38] && w->mot[39]) {
            EM10_ACC_PARTS(w->mot[38], w->mot[39], w->x1BC);
        }
        if ((em->flags_3C8 & 0x4000) && !w->x1BC && w->mot[38] && w->mot[40]) {
            EM10_ACC_PARTS(w->mot[38], w->mot[40], w->x1BC);
        }
        break;
    case 0xE:
    case 0xF:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x17:
    case 0x18:
        if ((em->flags_3C8 & 0x00800000) && !w->x178) {
            EM10_ACC_OBJ12(PL_ARC_PTR(em->subArc, 0x265), PL_ARC_PTR(em->subArc, 0x266), rot, 80.0f, 0.0f, 9);
        }
        if ((em->flags_3C8 & 0x00200000) && !w->x178) {
            EM10_ACC_OBJ12(PL_ARC_PTR(em->subArc, 0x262), PL_ARC_PTR(em->subArc, 0x263), rot, 80.0f, 0.0f, 9);
        }
        if ((em->flags_3C8 & 0x00080000) && !w->x178) {
            EM10_ACC_OBJ12(PL_ARC_PTR(em->subArc, 0x262), PL_ARC_PTR(em->subArc, 0x263), rot, 80.0f, 0.0f, 9);
        }
        if (em->flags_3C8 & 0x200) {
            EM10_ACC_PARTS(PL_ARC_PTR(em->subArc, 0x24F), PL_ARC_PTR(em->subArc, 0x250), w->x1B4);
            if (w->x18C) {
                w->x18C->be_flag &= ~8;
            }
        }
        if (em->flags_3C8 & 0x100) {
            EM10_ACC_PARTS(PL_ARC_PTR(em->subArc, 0x243), PL_ARC_PTR(em->subArc, 0x244), w->x1B0);
        }
        if ((em->flags_3C8 & 2) && !w->x1B0) {
            EM10_ACC_PARTS(PL_ARC_PTR(em->subArc, 0x245), w->mot[0], w->x1B0);
        }
        if ((em->flags_3C8 & 0x00040000) && !w->x1A0) {
            EM10_ACC_PARTS(PL_ARC_PTR(em->subArc, 0x23D), PL_ARC_PTR(em->subArc, 0x23E), w->x1A0);
        }
        if ((em->flags_3C8 & 0x4000) && !w->x1A0) {
            EM10_ACC_PARTS(PL_ARC_PTR(em->subArc, 0x23F), PL_ARC_PTR(em->subArc, 0x240), w->x1A0);
        }
        if ((em->flags_3C8 & 0x1000) && !w->x1A0) {
            EM10_ACC_PARTS(PL_ARC_PTR(em->subArc, 0x241), PL_ARC_PTR(em->subArc, 0x242), w->x1A0);
        }
        break;
    case 2:
        EM10_ACC_OBJ12(PL_ARC_PTR(em->subArc, 0x25F), PL_ARC_PTR(em->subArc, 0x260), rot, 155.38f, 23.4f, 0xA);
        break;
    }
}

cEmWep* em10MakeWeapon(cEm10* em, int type)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;
    Vec rot;
    cEmWep* wep = 0;

    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    switch (type) {
    case 0:
    default:
        break;
    case 1:
        if (w->mot[41] && w->mot[42]) {
            wep = SetWeapon(w->mot[41], w->mot[42], &pos, &rot, 0);
        }
        break;
    case 2:
        if (w->x6C5 == 2) {
            if (w->mot[63] && w->mot[64]) {
                wep = SetWeapon(w->mot[63], w->mot[64], &pos, &rot, 0);
            }
        } else {
            if (w->mot[65] && w->mot[66]) {
                wep = SetWeapon(w->mot[65], w->mot[66], &pos, &rot, 0);
            }
        }
        break;
    case 0xB:
        if (w->mot[65] && w->mot[66]) {
            wep = SetWeapon(w->mot[65], w->mot[66], &pos, &rot, 0);
            if (wep) {
                wep->setCloth(em);
            }
        }
        break;
    case 3:
        if (w->mot[63] && w->mot[64]) {
            wep = SetWeapon(w->mot[63], w->mot[64], &pos, &rot, 0);
        }
        break;
    case 0xA:
        if (w->mot[77] && w->mot[78]) {
            wep = SetWeapon(w->mot[77], w->mot[78], &pos, &rot, 0);
        }
        break;
    case 4:
        if (w->mot[67] && w->mot[68]) {
            wep = SetWeapon(w->mot[67], w->mot[68], &pos, &rot, 0);
        }
        break;
    case 5:
        if (w->mot[43] && w->mot[44]) {
            wep = SetWeapon(w->mot[43], w->mot[44], &pos, &rot, 0);
        }
        break;
    case 6:
        if (w->mot[69] && w->mot[70]) {
            wep = SetWeapon(w->mot[69], w->mot[70], &pos, &rot, 0);
        }
        break;
    case 0xF:
        if (w->mot[69] && w->mot[70]) {
            wep = SetWeapon(w->mot[69], w->mot[70], &pos, &rot, 0);
            if (wep) {
                wep->setEffAlways(0xCD, 0);
                wep->setSeAlways(8, 0x60, em->id, 0x13);
            }
        }
        break;
    case 7:
        if (w->mot[71] && w->mot[72]) {
            wep = SetWeapon(w->mot[71], w->mot[72], &pos, &rot, 0);
        }
        break;
    case 0x10:
        if (w->mot[71] && w->mot[72]) {
            wep = SetWeapon(w->mot[71], w->mot[72], &pos, &rot, 0);
        }
        break;
    case 8:
        if (w->mot[73] && w->mot[74]) {
            wep = SetWeapon(w->mot[73], w->mot[74], &pos, &rot, 0);
        }
        break;
    case 0xC:
        wep = SetWeapon(PL_ARC_PTR(em->subArc, 0x18C), PL_ARC_PTR(em->subArc, 0x18D), &pos, &rot, 0);
        break;
    case 9:
        wep = SetWeapon(PL_ARC_PTR(em->subArc, 0xA2), PL_ARC_PTR(em->subArc, 0xA3), &pos, &rot, 0);
        break;
    }
    if (wep) {
        em10WepSeEffSet(em, wep, type);
        switch (type) {
        case 4:
        case 5:
        case 8:
        case 0xB:
        case 0xC:
            break;
        default:
            wep->be_flag |= 0x4000;
            break;
        }
    }
    return wep;
}

void em10WeaponSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;
    Vec rot;
    u32 f;

    if (!w->pWep) {
        return;
    }
    switch (w->wepType) {
    case 0:
    default:
        return;
    case 1:
        pos.x = -843.58f;
        pos.y = 5.81f;
        pos.z = 866.87f;
        rot.x = 0.0f;
        rot.y = -0.7166758f;
        rot.z = 0.0f;
        break;
    case 6:
        pos.x = -403.0f;
        pos.y = -120.94f;
        pos.z = 296.24f;
        rot.x = 0.0f;
        rot.y = -0.73521996f;
        rot.z = 0.0f;
        w->pWep->setEffAlways(0x10, 0x49);
        break;
    case 5:
        pos.x = -80.0f;
        pos.y = -35.0f;
        pos.z = -10.0f;
        rot.x = 0.0f;
        rot.y = -2.0943952f;
        rot.z = 0.0f;
        break;
    case 2:
    case 3:
    case 7:
    case 9:
    case 0xA:
    case 0xF:
    case 0x10:
        pos.x = -313.85f;
        pos.y = -21.2f;
        pos.z = 102.21f;
        rot.x = 0.0f;
        rot.y = -1.0402162f;
        rot.z = 0.0f;
        break;
    case 0xB:
        pos.x = -86.0f;
        pos.y = -28.0f;
        pos.z = -6.0f;
        rot.x = 0.0f;
        rot.y = -0.7853982f;
        rot.z = 0.0f;
        break;
    case 4:
        pos.x = -304.24f;
        pos.y = -17.86f;
        pos.z = -57.43f;
        rot.x = 1.4835298f;
        rot.y = 0.0f;
        rot.z = -1.5707964f;
        break;
    case 8:
        pos.x = -384.25f;
        pos.y = -24.01f;
        pos.z = 17.94f;
        rot.x = 1.5707964f;
        rot.y = 0.0f;
        rot.z = -1.5707964f;
        w->x6B5 = 2;
        break;
    case 0xC:
        pos.x = 220.0f;
        pos.y = 120.0f;
        pos.z = 25.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->x6B5 = 2;
        break;
    case 0xD:
        pos.x = -540.0f;
        pos.y = -25.0f;
        pos.z = 5.0f;
        rot.x = 0.0f;
        rot.y = -1.5707964f;
        rot.z = 0.0f;
        break;
    case 0xE:
        pos.x = -199.0f;
        pos.y = -27.0f;
        pos.z = 459.0f;
        rot.x = 0.0f;
        rot.y = -0.2617994f;
        rot.z = 0.0f;
        break;
    }
    f = em->flags_3C8;
    if (f & 0x01000000) {
        switch (w->wepType) {
        case 4:
        case 6:
            em->flags_3C8 = f & ~0x01000000;
            break;
        case 1:
        case 8:
        case 0xC:
            pos.x = -pos.x;
            rot.y = -rot.y;
            rot.z = -rot.z;
            break;
        default:
            pos.x = -pos.x;
            rot.z = -rot.z + 3.1415927f;
            break;
        }
    }
    w->pWep->pos = pos;
    w->pWep->rot = rot;
    if (em->flags_3C8 & 0x01000000) {
        w->pWep->setParent(em, 0x10, 0);
    } else {
        w->pWep->setParent(em, 0xA, 0);
    }
}

extern "C" void em10WeaponSet2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;
    Vec rot;

    if (w->pWep2) {
        return;
    }
    if (!(em->flags_3C8 & 0x2000)) {
        return;
    }
    if (em->flags_3C8 & 0x20000000) {
        if (w->x6C5 == 0) {
            w->wep2Type = 2;
        } else {
            w->wep2Type = 0xB;
        }
    }
    if (em->flags_3C8 & 0x08000000) {
        int t = w->x6C5;
        if (t != 2) {
            t = 3;
        }
        w->wep2Type = t;
    }
    if (em->flags_3C8 & 0x4000) {
        w->wep2Type = 0xA;
    }
    if (em->flags_3C8 & 0x8000) {
        w->wep2Type = 8;
    }
    if (em->flags_3C8 & 0x20000) {
        w->wep2Type = 9;
    }
    w->pWep2 = em10MakeWeapon(em, w->wep2Type);
    if (!w->pWep2) {
        w->wep2Type = 0;
        return;
    }
    if (em->type == 6) {
        w->pWep2->setTransMode(0);
    }
    if (em->flags_3C8 & 0x01000000) {
        pos.x = -140.0f;
        pos.y = -30.0f;
        pos.z = -150.0f;
        rot.x = -1.5707964f;
        rot.y = 0.0f;
        rot.z = 1.2566371f;
    } else {
        pos.x = 140.0f;
        pos.y = -30.0f;
        pos.z = -150.0f;
        rot.x = 1.5707964f;
        rot.y = 0.0f;
        rot.z = 1.8849555f;
    }
    w->pWep2->pos = pos;
    w->pWep2->rot = rot;
    w->pWep2->setParent(em, 0x11, 0);
}
extern "C" void em10WepSeEffSet(cEm10* em, cEmWep* wep, int type)
{
    if (!wep) {
        return;
    }
    switch (type) {
    case 0:
    case 9:
        return;
    case 2:
    case 3:
    case 5:
    case 10:
    case 11:
    case 13:
    default:
        wep->setSeDamage(8, 0x3F, em->id);
        wep->setSeHit(8, 0xB, em->id);
        wep->setSeFall(8, 0x40, em->id);
        wep->setSeThrow(8, 0x45, em->id, 4);
        wep->setEffDamage(0, 0x18);
        wep->setEffHit(0x10, 0xB);
        wep->setEffWater(1, 0x37);
        break;
    case 7:
        wep->setSeDamage(8, 0x3F, em->id);
        wep->setSeHit(8, 0x46, em->id);
        wep->setSeFall(8, 0x43, em->id);
        wep->setSeThrow(8, 0x42, em->id, 4);
        wep->setEffDamage(0, 0x18);
        wep->setEffHit(0x10, 0xB);
        wep->setEffWater(1, 0x37);
        break;
    case 1:
    case 6:
    case 8:
        wep->setSeDamage(8, 0x3F, em->id);
        wep->setSeHit(8, 0xB, em->id);
        wep->setSeFall(8, 0x41, em->id);
        wep->setSeThrow(8, 0x45, em->id, 4);
        wep->setEffDamage(0, 0x18);
        wep->setEffHit(0x10, 0xB);
        wep->setEffWater(1, 0x37);
        break;
    case 12:
        wep->setSeDamage(8, 0x3F, em->id);
        wep->setSeHit(8, 0xB, em->id);
        wep->setSeFall(8, 0x41, em->id);
        wep->setSeThrow(8, 0x45, em->id, 4);
        wep->setEffDamage(0, 0x18);
        wep->setEffHit(0x10, 0xB);
        wep->setEffWater(1, 0x37);
        break;
    }
}

extern "C" void em10ShieldSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cEmShield* s;
    Vec pos;
    Vec rot;

    w->pShield = 0;
    if (w->x6C5 == 0) {
        return;
    }
    if ((int) em->flags_3C8 >= 0) {
        return;
    }
    if (em->type == 10 || em->type == 13 || em->type == 2 || em->type == 22 || em->type == 24) {
        return;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    s = SetShield(PL_ARC_PTR(em->subArc, 0x163), PL_ARC_PTR(em->subArc, 0x164), &pos, &rot);
    if (!s) {
        return;
    }
    pos.x = -237.0f;
    pos.y = 0.02f;
    pos.z = 14.0f;
    rot.x = 0.0f;
    rot.y = -1.5707964f;
    rot.z = 0.0f;
    if (em->flags_3C8 & 0x01000000) {
        pos.x = pos.x * -1.0f;
        rot.y = 1.5707964f;
    }
    s->pos = pos;
    s->rot = rot;
    if (em->flags_3C8 & 0x01000000) {
        s->setParent(em, 0xA, 0);
    } else {
        s->setParent(em, 0x10, 0);
    }
    w->pShield = s;
}

void em10HandSet(cEm10* em, int type)
{
    Em10Work* w = EM10_WK(em);
    cEmWep* wep;
    void* bin;
    void* tpl;
    cModelInfo* info;

    if (em->type == 10 || em->type == 13 || em->type == 2 || em->type == 22 || em->type == 24) {
        return;
    }
    if (w->pWep) {
        type = 3;
    }
    if (w->x184 && w->x188 && w->x6AD == type) {
        if (type != 0) {
            return;
        }
        if (!w->pWep) {
            return;
        }
    }
    wep = w->pWep;
    switch ((u32) type) {
    case 0:
    default:
        bin = w->mot[6];
        tpl = w->mot[11];
        break;
    case 1:
        bin = w->mot[7];
        tpl = w->mot[12];
        break;
    case 2:
        if (em->flags_3C8 & 0x01000000) {
            bin = w->mot[6];
            tpl = w->mot[13];
        } else {
            bin = w->mot[8];
            tpl = w->mot[11];
        }
        break;
    }
    if (wep) {
        if (em->flags_3C8 & 0x01000000) {
            tpl = w->mot[14];
        } else {
            bin = w->mot[9];
        }
        if (w->wepType == 1 || w->wepType == 4) {
            bin = w->mot[9];
            tpl = w->mot[14];
        }
        if (w->wepType == 6) {
            tpl = w->mot[15];
        }
    }
    if (w->wepType == 12) {
        bin = PL_ARC_PTR(em->subArc, 0x18E);
    }
    info = ModInfoMgr.create(bin, w->mot[0]);
    if (info) {
        if (w->x184) {
            cModel_swapModelInfo(em, w->x184->pData, info);
        } else {
            em->addModel(info);
        }
        w->x184 = info;
    }
    info = ModInfoMgr.create(tpl, w->mot[0]);
    if (info) {
        if (w->x188) {
            cModel_swapModelInfo(em, w->x188->pData, info);
        } else {
            em->addModel(info);
        }
        w->x188 = info;
    }
    w->x6AD = type;
}

void cEm10::setHand(int no, int type)
{
    Em10Work* w = EM10_WK(this);
    void* tpl;
    void* bin;
    cModelInfo* info;

    if (type == 10 || type == 13 || type == 2 || type == 22 || type == 24) {
        return;
    }
    if (w->x184 && w->x188 && w->x6AD == type) {
        return;
    }
    switch ((u32) type) {
    case 0:
    default:
        bin = w->mot[6];
        tpl = w->mot[11];
        break;
    case 1:
        bin = w->mot[7];
        tpl = w->mot[12];
        break;
    case 2:
        bin = w->mot[8];
        tpl = w->mot[13];
        break;
    case 3:
        // Target block: `lbz wepType; lwz tpl; lwz bin; cmpwi; bne`. tpl first = LUID order of the two
        // loads; the barrier keeps the byte compare after `lwz bin` (rank_for_schedule prefers the
        // compare: equal priority, weight 0 vs +1, then more dependents -- no plain form ranks the
        // load first). tpl loaded first would cost it r4 (global-alloc priority 2*5/16 vs bin's
        // 2*6/19), so the tpl create below is weighted with a do{}while(0). `no` is r10 because of
        // the dead x184 test below.
        {
            u8 wt = w->wepType;
            tpl = w->mot[14];
            bin = w->mot[9];
            asm volatile(""); // COMPILER-DIFF: candidate #14 (sched1 rank of `lwz bin` vs the byte compare)
            if (wt == 6) {
                bin = w->mot[10];
            }
        }
        break;
    }
    // Dead test (`type` is dead here; flow deletes the store, jump2 deletes the jump-to-next and,
    // through delete_computation, the compare and the load). Its load temp holds r11 through
    // global alloc, so `no` takes r10 (alloc order r0, r9, r11, r10). Only this load reproduces it.
    if (w->x184 == 0) {
        type = 0;
    }
    if (no) {
        info = ModInfoMgr.create(bin, w->mot[0]);
        if (!info) {
            return;
        }
        if (w->x184) {
            cModel_swapModelInfo(this, w->x184->pData, info);
        } else {
            addModel(info);
        }
        w->x184 = info;
    } else {
        do { info = ModInfoMgr.create(tpl, w->mot[0]); } while (0); // tpl's 6th weighted ref: r4
        if (!info) {
            return;
        }
        if (w->x188) {
            cModel_swapModelInfo(this, w->x188->pData, info);
        } else {
            addModel(info);
        }
        w->x188 = info;
    }
}

void em10HeadSet(cEm10* em, int no)
{
    Em10Work* w = EM10_WK(em);
    cModelInfo* info;
    void* bin;

    if (em->type == 10 || em->type == 13 || em->type == 2 || em->type == 22) {
        return;
    }
    // Separate case 0 / default arms (cross-jumped after allocation): the extra w reference decides r30 for w.
    switch (no) {
    case 0:
        bin = w->mot[2];
        break;
    default:
        bin = w->mot[2];
        break;
    case 1:
        if (w->x6C5 == 1 && em->hp > 0 && (Rnd() & 3) && !(em->flags_3C8 & 0x204000)) {
            em->getPartsPtr(0x24)->rot.x = -0.6981317f;
            bin = w->mot[4];
        } else {
            bin = w->mot[3];
        }
        break;
    }
    info = ModInfoMgr.create(bin, w->mot[5]);
    if (info) {
        if (w->x18C) {
            cModel_swapModelInfo(em, w->x18C->pData, info);
        } else {
            em->addModel(info);
        }
        w->x18C = info;
        w->x18C->be_flag |= 8;
    }
}

extern "C" void em10ClothPartsSet(cEm10* em, int no)
{
    Em10Work* w = EM10_WK(em);
    cModelInfo* info;
    void* bin;

    if (em->type != 6) {
        return;
    }
    switch (no) {
    case 0:
    default:
        bin = w->mot[19];
        break;
    case 1:
        bin = w->mot[18];
        break;
    }
    info = ModInfoMgr.create(bin, w->mot[17]);
    if (info) {
        if (w->x194) {
            cModel_swapModelInfo(em, w->x194->pData, info);
        } else {
            em->addModel(info);
        }
        w->x194 = info;
    }
}

extern "C" void em10GoodsPartsSet(cEm10* em, int on)
{
    Em10Work* w = EM10_WK(em);

    if (em->type != 6) {
        return;
    }
    if (!w->x198) {
        cModelInfo* info = ModInfoMgr.create(w->mot[20], w->mot[0]);
        if (info) {
            em->addModel(info);
        }
        w->x198 = info;
    }
    if (!on) {
        w->x198->be_flag &= ~8;
    } else {
        w->x198->be_flag |= 8;
    }
}

extern "C" void em10SackSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModelInfo* info;

    if (w->wepType != 4) {
        return;
    }
    if (!w->mot[21] || !w->mot[22]) {
        return;
    }
    info = ModInfoMgr.create(w->mot[21], w->mot[22]);
    if (info) {
        if (w->x19C) {
            cModel_swapModelInfo(em, w->x19C->pData, info);
        } else {
            em->addModel(info);
        }
        w->x19C = info;
    }
    if ((u8) (em->type - 11) <= 1) {
        if (w->x18C) {
            w->x18C->be_flag &= ~8;
        }
        EstSetEm(em, -1, 0, 0, 0x10, 0x55, 0, 0, em, 0);
    }
}

extern "C" int em10ClimbOverCk2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    Vec c;
    Vec n;
    Mtx m;
    Vec d;
    Vec e1;
    Vec e2;
    int side;
    f32 ang;
    f32 y;

    if (pG->flags_5014 & 0x08000000) {
        return 0;
    }
    if (w->x634 % 15 != 11) {
        return 0;
    }
    if (w->x5EC == 0 && em->x3D0 == 5) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 800.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (!(SatMgr.hitCheck(&a, &b, &c, &n, 0, 0) & 0x20)) {
        return 0;
    }
    side = 0;
    ang = atan2f(-n.x, -n.z);
    em->rot.y = ang;
    w->x5E0 = em->pos;
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &em->pos);
    e1.x = 300.0f;
    e1.y = 1200.0f;
    e1.z = 0.0f;
    e2.x = 300.0f;
    e2.y = 1200.0f;
    e2.z = 800.0f;
    PSMTXMultVec(m, &e1, &e1);
    PSMTXMultVec(m, &e2, &e2);
    if (SatMgr.hitCheck(&e1, &e2, 0, 0, 0, 0x20)) {
        side = 1;
    }
    e1.x = -300.0f;
    e1.y = 1200.0f;
    e1.z = 0.0f;
    e2.x = -300.0f;
    e2.y = 1200.0f;
    e2.z = 800.0f;
    PSMTXMultVec(m, &e1, &e1);
    PSMTXMultVec(m, &e2, &e2);
    if (SatMgr.hitCheck(&e1, &e2, 0, 0, 0, 0x20)) {
        side |= 2;
    }
    if (side == 3) {
        return 0;
    }
    if (side & 1) {
        d.x = -300.0f;
        d.y = 0.0f;
        d.z = 0.0f;
        PSMTXMultVec(m, &d, &w->x5E0);
    }
    if (side & 2) {
        d.x = 300.0f;
        d.y = 0.0f;
        d.z = 0.0f;
        PSMTXMultVec(m, &d, &w->x5E0);
    }
    d.x = 0.0f;
    d.y = 0.0f;
    d.z = 1000.0f;
    PSMTXMultVec(m, &d, &d);
    y = SatMgr.getFloor(&d, 600.0f, 100000.0f, 0, 0);
    if (y < em->pos.y - 500.0f) {
        return 2;
    }
    return 1;
}

int em10ClimbOverCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    // `case 0: default:` first and no trailing `return 0`: case 2's inline `return 0` block is the
    // one the other return-0 paths jump into (with a trailing return the inline copy is deleted).
    switch ((u32) em10ClimbOverCk2(em)) {
    case 0:
    default:
        return 0;
    case 1:
        EmRoutineSet(em, 1, 0x3C, 0, 0);
        return 1;
    case 2:
        if ((em->flags_3C8 & 0x400) && w->x5EC == 0) {
            return 0;
        }
        EmRoutineSet(em, 1, 0x43, 0, 1);
        return 1;
    }
}

extern "C" int em10WindowCk2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    Vec dir;
    Vec pos;
    u16 status;
    f32 ang;
    f32 y;

    if (w->x634 % 15 != 10) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 400.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 400.0f;
    b.z = 1000.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (!ChkWindow(em, &a, &b, 1, &status, &dir, &pos, &w->pWindow)) {
        return 0;
    }
    if (!w->pWindow->ChkEnableDamage()) {
        return 0;
    }
    Mtx m;
    Vec tmp;
    Vec v;
    ang = atan2f(-dir.x, -dir.z);
    em->rot.y = ang;
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &pos);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = -400.0f;
    PSMTXMultVec(m, &a, &w->x5E0);
    w->x5E0.y = em->pos.y;
    tmp = em->pos;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1000.0f;
    PSMTXMultVec(m, &v, &v);
    y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
    if (y < em->pos.y - 500.0f) {
        return 2;
    }
    if (!(status & 1)) {
        if ((w->flags & 0x4000) && w->pWindow) {
            w->pWindow->SetBreakAll(&em->pos, 0, 0);
            w->pWindow = 0;
            return 0;
        }
        if (em10DootAtkCk(em) == 0) {
            return 0;
        }
        return 3;
    }
    if (EmRackCk(em, &em->pos, ang)) {
        return 1;
    }
    if (em10DootAtkCk(em) == 0) {
        return 0;
    }
    return 4;
}

int em10WindowCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01010000 && (w->flags & 0x00800000)) {
        return 0;
    }
    switch ((u32) em10WindowCk2(em)) {
    case 0:
    default:
        return 0;
    case 1:
        EmRoutineSet(em, 1, 0x3C, 0, 0);
        return 1;
    case 2:
        EmRoutineSet(em, 1, 0x43, 0, 1);
        return 1;
    case 3:
        EmRoutineSet(em, 1, 0x3F, 0, 0);
        return 1;
    case 4:
        EmRoutineSet(em, 1, 0x3D, 0, 0);
        return 1;
    }
}

int em10DoorOpenCk(cEm10* em, int kick)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    u32 i;
    f32 ang;

    if (w->x634 % 15 != 9) {
        return 0;
    }
    if (w->x644 != 0) {
        kick = 1;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* e = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmDoorWork* dw;
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x41) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) > 6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(e);
        ang = fabsf(Muku2(dw->rotY, em->rot.y, 3.1415927f));
        if (ang > 0.7853982f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->inv, &em->pos, &v);
        if (ang < 1.5707964f) {
            if (v.z > 0.0f || v.z < -800.0f) {
                continue;
            }
        } else {
            if (v.z < 0.0f || v.z > 800.0f) {
                continue;
            }
        }
        if (v.x > dw->width || v.x < -dw->width) {
            continue;
        }
        if (v.y > 500.0f || v.y < -500.0f) {
            continue;
        }
        switch (e->ckOpen()) {
        case 0:
        default:
            if (w->flags & 0x4000) {
                e->setOpen(&em->pos, 0, 0, 0);
                return 1;
            }
            if (em->plDist2 < 25000000.0f && kick == 0) {
                if (!em10DootAtkCk(em)) {
                    return 0;
                }
                em->xFC = 1;
                em->xFD = 0x3D;
                em->xFE = 0;
                em->xFF = 0;
                return 1;
            }
            e->setOpen(&em->pos, 0, 0, 0);
            return 0;
        case 2:
            if (w->flags & 0x4000) {
                e->setOpen(&em->pos, 0, 0, 0);
                return 1;
            }
            if (em10DootAtkCk(em)) {
                EmRoutineSet(em, 1, 0x3D, 0, 0);
                return 1;
            }
            return 0;
        case 1:
        case 3:
            return 0;
        }
    }
    return 0;
}

extern "C" int em10AtkDoorCk(cEm10* em)
{
    u32 i;
    Mtx m;
    Vec v;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* d = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmDoorWork* dw;
        f32 hw;
        if (!(d->be_flag & 1)) {
            continue;
        }
        if (!(d->be_flag & 0x20)) {
            continue;
        }
        if (d->id != 0x41) {
            continue;
        }
        if (d->hp <= 0) {
            continue;
        }
        {
            f32 dx = em->pos.x - d->pos.x;
            f32 dy = em->pos.y - d->pos.y;
            f32 dz = em->pos.z - d->pos.z;
            if (dx * dx + dy * dy + dz * dz > 4000000.0f) {
                continue;
            }
        }
        dw = EMDOOR_WK(d);
        PSMTXRotRad(m, 'y', dw->rotY);
        TransMatrix(m, &d->pos);
        v.x = -dw->width * 0.5f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(m, &v, &v);
        TransMatrix(m, &v);
        if (fabsf(Muku(&em->pos, &v, em->rot.y, 3.1415927f)) > 0.7853982f) {
            continue;
        }
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &em->pos, &v);
        hw = dw->width * 0.5f + 300.0f;
        if (v.x > hw) {
            continue;
        }
        if (v.x < -hw) {
            continue;
        }
        if (v.y > 500.0f || v.y < -500.0f) {
            continue;
        }
        if (v.z > 800.0f || v.z < -800.0f) {
            continue;
        }
        if (d->ckOpen() == 1) {
            continue;
        }
        return 1;
    }
    return 0;
}

// The do { } while (0) around the store adds a loop-note level, so flow counts the three `in = 1`
// sets at loop depth 3 (REG_N_REFS 13 instead of 10): `in` then outranks `kind` in global alloc
// (in r25, kind r24) with the loop's real insn count unchanged.
#define EM10_DOOR_IN_CK(v, in)                                                                     \
    if (v.x < 500.0f && v.x > -500.0f && v.y < 500.0f && v.y > -500.0f && v.z < 1500.0f &&        \
        v.z > 0.0f) {                                                                              \
        do {                                                                                       \
            in = 1;                                                                                \
        } while (0);                                                                               \
    }

int em10SetDamageDoor(cEm10* em, int kind)
{
    Mtx inv;
    Vec v;
    int ret = 0;
    u32 i;
    int in;
    cEmDoor* d;
    EmDoorWork* dw;

    PSMTXInverse(em->mat, inv);
    for (i = 0; i < EmMgr.nArray; i++) {
        d = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if (!(d->be_flag & 1)) {
            continue;
        }
        if (!(d->be_flag & 0x20)) {
            continue;
        }
        if (d->id != 0x41) {
            continue;
        }
        if (d->hp <= 0) {
            continue;
        }
        if ((em->pos.x - d->pos.x) * (em->pos.x - d->pos.x) + (em->pos.y - d->pos.y) * (em->pos.y - d->pos.y) +
                (em->pos.z - d->pos.z) * (em->pos.z - d->pos.z) >
            4000000.0f) {
            continue;
        }
        dw = EMDOOR_WK(d);
        in = 0;
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(dw->mat, &v, &v);
        PSMTXMultVec(inv, &v, &v);
        EM10_DOOR_IN_CK(v, in);
        v.x = dw->width * 0.5f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(dw->mat, &v, &v);
        PSMTXMultVec(inv, &v, &v);
        EM10_DOOR_IN_CK(v, in);
        v.x = -dw->width * 0.5f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(dw->mat, &v, &v);
        PSMTXMultVec(inv, &v, &v);
        EM10_DOOR_IN_CK(v, in);
        if (!in) {
            continue;
        }
        switch (d->ckOpen()) {
        case 1:
        case 3:
            break;
        case 0:
        default:
            switch ((u32) kind) {
            case 1:
                d->setOpen(&em->pos, 0, 0, 0);
                if (ret == 0) {
                    ret = 1;
                }
                break;
            case 2:
            door_break:
                if (d->type == 0) {
                    d->setBreak(&em->pos);
                } else {
                    d->setOpen(&em->pos, 0, 0, 0);
                }
                ret = 2;
                break;
            case 0:
                d->setShock(0, &em->pos, 0);
                if (ret == 0) {
                    ret = 1;
                }
                break;
            }
            break;
        case 2:
            switch ((u32) kind) {
            case 0:
                d->setShock(0, &em->pos, 0);
                if (ret == 0) {
                    ret = 1;
                }
                break;
            case 1:
                if (d->hp > 1) {
                    d->setShock(0, &em->pos, 0);
                    if (ret == 0) {
                        ret = 1;
                    }
                    break;
                }
                goto door_break; // the hp <= 1 path falls into the FIRST switch's type-check body (`ble` target)
            case 2:
                if (d->type == 0) {
                    d->setBreak(&em->pos);
                } else {
                    d->setOpen(&em->pos, 0, 0, 0);
                }
                ret = 2;
                break;
            }
            break;
        }
    }
    return ret;
}

int em10RackBreakCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;
    Mtx inv;
    Vec v;
    Vec a;
    Vec b;

    if (w->x634 % 15 != 8) {
        return 0;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEmRack* e = (cEmRack*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x45) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) > 4000000.0f) {
            continue;
        }
        if (fabsf(Muku(&em->pos, &e->pos, em->rot.y, 3.1415927f)) > 0.7853982f) {
            continue;
        }
        PSMTXInverse(e->mat, inv);
        PSMTXMultVec(inv, &em->pos, &v);
        if (v.x > 1000.0f || v.x < -1000.0f) {
            continue;
        }
        if (v.y > 500.0f || v.y < -500.0f) {
            continue;
        }
        if (v.z > 1200.0f || v.z < -1200.0f) {
            continue;
        }
        if (w->flags & 0x4000) {
            e->setDown(&em->pos);
            return 1;
        }
        if (!em10DootAtkCk(em)) {
            return 0;
        }
        a = em->pos;
        b = e->pos;
        b.y = a.y = em->pos.y + 1500.0f;
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            continue;
        }
        {
            // `type` loaded before the loop notes: the LOOP_BEG barrier keeps the ternary's hoisted
            // `li 0x3D` behind the compare, so the temp shares r0 with the loaded byte (and does not
            // inherit e's r3/r11 preferences). Two do { } while (0) levels put the four routine
            // stores at loop depth 4: em then has 34 weighted refs and outranks e (em r31, e r30);
            // the loop notes also keep `li r3, 1` below the stores so the hitCheck result (known 0)
            // stays in r3 for the xFE/xFF zeros.
            int type = e->type;
            do {
                do {
                    EmRoutineSet(em, 1, type == 0 ? 0x3E : 0x3D, 0, 0);
                } while (0);
            } while (0);
        }
        return 1;
    }
    return 0;
}

extern "C" int em10AtkRackCk(cEm10* em)
{
    u32 i;
    Mtx inv;
    Vec v;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x45) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        {
            f32 dx = em->pos.x - e->pos.x;
            f32 dy = em->pos.y - e->pos.y;
            f32 dz = em->pos.z - e->pos.z;
            if (dx * dx + dy * dy + dz * dz > 4000000.0f) {
                continue;
            }
        }
        if (fabsf(Muku(&em->pos, &e->pos, em->rot.y, 3.1415927f)) > 0.7853982f) {
            continue;
        }
        PSMTXInverse(e->mat, inv);
        PSMTXMultVec(inv, &em->pos, &v);
        if (v.x > 1000.0f || v.x < -1000.0f) {
            continue;
        }
        if (v.y > 500.0f || v.y < -500.0f) {
            continue;
        }
        if (v.z > 1800.0f || v.z < -1800.0f) {
            continue;
        }
        return 1;
    }
    return 0;
}

void em10SetDamageRack(cEm10* em, int a)
{
    u32 i;
    Mtx inv;
    Vec v;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEmRack* e = (cEmRack*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x45) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        {
            f32 dx = em->pos.x - e->pos.x;
            f32 dy = em->pos.y - e->pos.y;
            f32 dz = em->pos.z - e->pos.z;
            if (dx * dx + dy * dy + dz * dz > 4000000.0f) {
                continue;
            }
        }
        if (fabsf(Muku(&em->pos, &e->pos, em->rot.y, 3.1415927f)) > 0.7853982f) {
            continue;
        }
        PSMTXInverse(e->mat, inv);
        PSMTXMultVec(inv, &em->pos, &v);
        if (v.x > 1000.0f || v.x < -1000.0f) {
            continue;
        }
        if (v.y > 500.0f || v.y < -500.0f) {
            continue;
        }
        if (v.z > 1800.0f || v.z < -1800.0f) {
            continue;
        }
        switch ((u32) a) {
        case 0:
        case 1:
        default:
            e->setShock();
            break;
        case 2:
            e->setDown(&em->pos);
            break;
        }
        return;
    }
}

int em10LadderClimbCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cObjLadder* o;
    Mtx m;
    Vec v;

    if (w->x634 % 15 != 4) {
        return 0;
    }
    if (w->x5EC == 0 && em->x3D0 == 5) {
        return 0;
    }
    if (w->x54C.y - em->pos.y < 1000.0f) {
        return 0;
    }
    // A while loop with the `o = o->next` step repeated before every `continue` (jump2 cross-jumps
    // the copies into one): the copies keep the loop at 82 real insns in loop pass 2, above the
    // 71-insn invariant threshold, so the Muku PI `lis` stays inside the loop like the target.
    o = (cObjLadder*) ObjMgr.pAlive;
    while (o) {
        if (o->id != 0x13) {
            o = (cObjLadder*) o->next;
            continue;
        }
        if (!o->ckClimb()) {
            o = (cObjLadder*) o->next;
            continue;
        }
        {
            f32 dx = em->oldPos.x - o->pos.x;
            f32 dy = em->oldPos.y - o->pos.y;
            f32 dz = em->oldPos.z - o->pos.z;
            if (dx * dx + dy * dy + dz * dz > 4000000.0f) {
                o = (cObjLadder*) o->next;
                continue;
            }
        }
        if (fabsf(Muku(&em->oldPos, &o->pos, em->rot.y, 3.1415927f)) > 1.5707964f) {
            o = (cObjLadder*) o->next;
            continue;
        }
        PSMTXRotRad(m, 'y', o->rot.y);
        TransMatrix(m, &o->pos);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &em->pos, &v);
        if (v.z > 1000.0f || v.z < -500.0f) {
            o = (cObjLadder*) o->next;
            continue;
        }
        if (v.x > 800.0f || v.x < -800.0f) {
            o = (cObjLadder*) o->next;
            continue;
        }
        if (!(fabsf(v.y) > 500.0f)) {
            w->pLadder = o;
            o->setClimb();
            EmRoutineSet(em, 1, 0x40, 0, 0);
            return 1;
        }
        o = (cObjLadder*) o->next;
    }
    return 0;
}

int em10VLadderClimbCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;
    s8 level;
    f32 ang;
    u32 i;

    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01010000 || (G_ROOM_ID32 & 0xFFFF0000) == 0x01110000 || (G_ROOM_ID32 & 0xFFFF0000) == 0x04000000) {
        return 0;
    }
    switch (em->id) {
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x16:
    case 0x17:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x20:
        break;
    default:
        return 0;
    }
    if (w->x634 % 15 != 5) {
        return 0;
    }
    if (w->x5EC == 0 && em->x3D0 == 5) {
        return 0;
    }
    if (w->x54C.y - em->pos.y < 1000.0f) {
        return 0;
    }
    if (!SceAtSearchLadder(em, &pos, &ang, (u8*) &level)) {
        return 0;
    }
    if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.y - pos.y) * (em->pos.y - pos.y) + (em->pos.z - pos.z) * (em->pos.z - pos.z) > 1000000.0f) {
        return 0;
    }
    if (fabsf(Muku2(em->rot.y, ang, 3.1415927f)) > 1.5707964f) {
        return 0;
    }
    if ((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.y - pPL->pos.y) * (em->pos.y - pPL->pos.y) + (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z) < 4000000.0f) {
        return 0;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* o = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((o->be_flag & 0x201) != 1) {
            continue;
        }
        if (o->id <= 0xF) {
            continue;
        }
        if (o->id > 0x20) {
            continue;
        }
        if (o->hp <= 0) {
            continue;
        }
        if (o == em) {
            continue;
        }
        if (!o->checkStatus(5)) {
            continue;
        }
        if (o->xFC != 1) {
            continue;
        }
        if (o->xFD != 0x41) {
            continue;
        }
        if (!((em->pos.x - o->pos.x) * (em->pos.x - o->pos.x) + (em->pos.y - o->pos.y) * (em->pos.y - o->pos.y) + (em->pos.z - o->pos.z) * (em->pos.z - o->pos.z) > 4000000.0f)) {
            return 0;
        }
    }
    w->x5DC = ang;
    w->x5E0 = pos;
    if (em->type == 2 || em->type == 0x16) {
        w->x5DC = ang;
        w->x5E0 = pos;
        w->x5E0.y += (f32) level * 1000.0f;
        EmRoutineSet(em, 1, 0x45, 0, 0);
        return 1;
    }
    EmRoutineSet(em, 1, 0x41, 0, level);
    return 1;
}

int em10LadderResetCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cObjLadder* o;
    Vec v;

    if (w->x54C.y - em->pos.y < 1000.0f) {
        return 0;
    }
    for (o = (cObjLadder*) ObjMgr.pAlive; o; o = (cObjLadder*) o->next) {
        f32 d;
        if (o->id != 0x13) {
            continue;
        }
        if (!o->ckReset()) {
            continue;
        }
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1432.0f;
        PSMTXMultVec(o->mat, &v, &v);
        d = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.z - v.z) * (em->pos.z - v.z);
        if (d > 1210000.0f) {
            if (d < 9000000.0f) {
                em->setGoto(&v, 9);
                return 1;
            }
        } else {
            int zero = 0;
            w->pLadder = o;
            o->setResetReserve();
            EmRoutineSet(em, 1, 0x42, zero, zero);
            return 1;
        }
    }
    return 0;
}

extern "C" int em10JumpDownCk2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec n;
    Vec a;
    Vec b;
    f32 ang;
    int hit;

    if (w->x634 % 15 != 6) {
        return 0;
    }
    if (w->x6AF == 0) {
        if ((w->flags & 0x08000000) && pSUB != 0) {
            if (w->x514 < 25000000.0f) {
                if (pSUB->pos.y > em->pos.y - 500.0f) {
                    return 0;
                }
            }
        } else {
            if (em->plDist2 < 25000000.0f && pPL->pos.y > em->pos.y - 500.0f) {
                return 0;
            }
        }
    }
    a.x = 100.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    if (w->pShield) {
        b.x = 100.0f;
        b.y = 500.0f;
        b.z = 700.0f;
    } else {
        b.x = 100.0f;
        b.y = 500.0f;
        b.z = 500.0f;
    }
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    hit = SatMgr.hitCheck(&a, &b, 0, &n, 0, 0);
    if (hit & 0x142010) {
        ang = atan2f(-n.x, -n.z);
        if (fabsf(Muku2(em->rot.y, ang, 3.1415927f)) < 0.5235988f) {
            goto found;
        }
    }
    a.x = -100.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    if (w->pShield) {
        b.x = -100.0f;
        b.y = 500.0f;
        b.z = 700.0f;
    } else {
        b.x = -100.0f;
        b.y = 500.0f;
        b.z = 500.0f;
    }
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    hit = SatMgr.hitCheck(&a, &b, 0, &n, 0, 0);
    if (hit & 0x142010) {
        ang = atan2f(-n.x, -n.z);
        if (fabsf(Muku2(em->rot.y, ang, 3.1415927f)) < 0.5235988f) {
found:
            em->rot.y = ang;
            if (hit & 0x40000) {
                return 2;
            }
            return 1;
        }
    }
    return 0;
}

extern "C" int em10JumpDownCk2(cEm10* em);

int em10JumpDownCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int r;

    if ((em->flags_3C8 & 0x400) && w->x5EC == 0) {
        return 0;
    }
    if ((pG->flags_51E4 & 3) == (em->emsetNo & 3)) {
        f32 y = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (y < em->pos.y - 350.0f) {
            w->x5E0 = em->pos;
            EmRoutineSet(em, 1, 0x43, 0, 0);
            return 1;
        }
    }
    if (w->x650 < 600.0f) {
        return 0;
    }
    r = em10JumpDownCk2(em);
    switch ((u32) r) {
    case 0:
    default:
        break;
    case 1:
        w->x5E0 = em->pos;
        EmRoutineSet(em, r, 0x43, 0, 0);
        return 1;
    case 2:
        w->x5E0 = em->pos;
        em->xFD = 0x43;
        em->xFE = 0;
        em->xFC = 1;
        em->xFF = 1;
        return 1;
    }
    return 0;
}

int em10JumpCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec n;
    Vec a;
    Vec b;
    Mtx m;
    f32 ang;
    f32 y;

    if (em10BullJumpCk(em)) {
        return 1;
    }
    if (w->x634 % 15 != 7) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    if (w->pShield) {
        b.x = 0.0f;
        b.y = 500.0f;
        b.z = 700.0f;
    } else {
        b.x = 0.0f;
        b.y = 500.0f;
        b.z = 500.0f;
    }
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (!(SatMgr.hitCheck(&a, &b, 0, &n, 0, 0) & 0x80000)) {
        return 0;
    }
    ang = atan2f(-n.x, -n.z);
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &em->pos);
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 4500.0f;
    PSMTXMultVec(m, &a, &a);
    y = SatMgr.getFloor(&a, 600.0f, 100000.0f, 0, 0);
    if (fabsf(em->pos.y - y) > 1000.0f) {
        return 0;
    }
    em->rot.y = ang;
    EmRoutineSet(em, 1, 0x44, 0, 0);
    return 1;
}

extern "C" int em10BullJumpCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cObjBull* o;
    cModel* p;
    Mtx m;
    Vec v;
    Mtx inv;
    Vec lv;
    f32 ang;

    if (pG->room_id != 0x30F) {
        return 0;
    }
    for (o = (cObjBull*) ObjMgr.pAlive; o; o = (cObjBull*) o->next) {
        if (o->id != 0x3E) {
            continue;
        }
        p = o->getPartsPtr(2);
        // one `ang` for the facing test and the goal angle (fabs result and GetXZAngle share f31)
        ang = fabsf(Muku(&em->pos, &p->worldPos, em->rot.y, 3.1415927f));
        if (ang > 0.5235988f) {
            continue;
        }
        ang = GetXZAngle(&em->pos, &p->worldPos);
        PSMTXRotRad(m, 'y', ang);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1500.0f;
        PSMTXMultVec(em->mat, &v, &v);
        v.y = p->worldPos.y + 500.0f;
        if (!o->ckBullRide(&v, 0, 0)) {
            continue;
        }
        PSMTXInverse(p->mat, inv);
        PSMTXMultVec(inv, &em->pos, &lv);
        if (lv.x < -1500.0f || lv.x > 1500.0f) {
            continue;
        }
        if (!(lv.z > -4000.0f)) {
            em->rot.y = ang;
            w->x5EC = 0;
            EmRoutineSet(em, 1, 0x67, 0, 0);
            return 1;
        }
    }
    return 0;
}

void em10ReturnStartPosCk(cEm10* em)
{
    em10ReturnPosCk(em);
}

extern "C" int em10ParasiteAtkCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    Vec c;
    f32 dist;

    if (em->type == 0xA || em->type == 0xD) {
        return 0;
    }
    if (w->x58C == 0 && w->pParasite == 0) {
        return 0;
    }
    if (w->x67C != 0) {
        return 0;
    }
    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if (w->pParasite && !w->pParasite->ckAtkEnable()) {
        return 0;
    }
    if (w->x58C && !w->x58C->vB8()) {
        return 0;
    }
    if (w->x6C5 == 1) {
        dist = 4000000.0f;
    } else {
        dist = 9000000.0f;
    }
    if (w->x58C) {
        if ((s16) w->x648 != 0) {
            dist = 1690000.0f;
        } else {
            dist = 12250000.0f;
        }
    }
    if (!(w->flags & 0x08000000)) {
        if (!(w->flags & 1)) {
            return 0;
        }
        if (em->plDist2 > dist) {
            return 0;
        }
        if (fabsf(em->pos.y - pPL->pos.y) > 1500.0f) {
            return 0;
        }
        if (w->x508 > 0.7853982f) {
            return 0;
        }
        a = em->pos;
        b = pPL->pos;
        a.y += 1500.0f;
        b.y += 1500.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            return 0;
        }
    } else {
        if (!(w->flags & 2)) {
            return 0;
        }
        if (w->x514 > 2250000.0f) {
            return 0;
        }
        if (fabsf(em->pos.y - pSUB->pos.y) > 1500.0f) {
            return 0;
        }
        if (w->x510 > 0.7853982f) {
            return 0;
        }
        a = em->pos;
        c = pSUB->pos;
        a.y += 1500.0f;
        c.y += 1500.0f;
        if (EatMgr.hitCheck(&a, &c, 0, 0, 0, 0x4000)) {
            return 0;
        }
    }
    EmRoutineSet(em, 1, 0x20, 0, 0);
    if (pG->x4F88 <= 3) {
        Ctrl12Set(w->pCtrl12, 6, 60);
        Ctrl12Set(w->pCtrl12, 8, 120);
    } else if (pG->stage_no <= 2 && pG->x4F88 <= 9) {
        Ctrl12Set(w->pCtrl12, 6, 30);
        Ctrl12Set(w->pCtrl12, 8, 120);
    }
    return 1;
}

extern "C" int em10ShotBowgunCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int near;

    if (w->wepType != 8) {
        return 0;
    }
    if (!w->pWep) {
        return 0;
    }
    if (em->flags_3C8 & 0x40) {
        em->setWeaponFall();
        return 0;
    }
    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01000000) {
        return 0;
    }
    if (w->x58C) {
        return 0;
    }
    if (w->pParasite) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    if (!em10ThrowScaCk(em)) {
        return 0;
    }
    if (w->x696 == 0 && !(w->flags & 0x100)) {
        return 0;
    }
    near = em10ThrowNearCk(em);
    if (near) {
        em->xFC = 1;
        em->xFD = 0x17;
        em->xFE = 0;
        em->xFF = Rnd() & 1;
        return 1;
    }
    if (em->plDist2 > 900000000.0f) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x21, 0, 0);
    return 1;
}

extern "C" int em10ShotRocketCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int near;

    if (w->wepType != 0xC) {
        return 0;
    }
    if (!w->pWep) {
        return 0;
    }
    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01000000) {
        return 0;
    }
    if (w->x58C) {
        return 0;
    }
    if (w->pParasite) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    if (!em10ThrowScaCk(em)) {
        return 0;
    }
    if (w->x696 == 0 && !(w->flags & 0x100)) {
        return 0;
    }
    near = em10ThrowNearCk(em);
    if (near) {
        em->xFE = 0;
        em->xFC = 1;
        em->xFD = 0x17;
        em->xFF = Rnd() & 1;
        return 1;
    }
    if (em->plDist2 > 900000000.0f) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x22, 0, 0);
    return 1;
}

extern "C" int em10ShotGatlingCk(cEm10* em)
{
    Vec pos;
    Vec d;
    f32 ang;
    f32 len;
    int hit;

    if (em->type != 2) {
        return 0;
    }
    if (!em10ThrowScaCk(em)) {
        return 0;
    }
    if (em->plDist2 > 900000000.0f) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) > 0.7853982f) {
        return 0;
    }
    PSVECSubtract(&pPL->pos, &em->pos, &d);
    len = SQRTF(d.x * d.x + d.z * d.z); // separate statement: no precomputed d.y across the call
    ang = -atan2f(d.y, len);
    if (fabsf(ang) > 0.43633232f) {
        return 0;
    }
    pos = pPL->pos;
    pos.y += 1200.0f;
    hit = EatMgr.hitCheck(&em->getPartsPtr(10)->worldPos, &pos, 0, 0, 0, 0x404000);
    if (hit) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x23, 0, 0);
    return 1;
}

extern "C" int em10ThrowAxeCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    f32 ang;
    int hit;

    if (w->x58C != 0) {
        return 0;
    }
    if (w->pParasite != 0) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    switch (w->wepType) {
    case 2:
    case 3:
    case 5:
    case 6:
        break;
    default:
        return 0;
    }
    if (w->pWep == 0) {
        return 0;
    }
    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01000000) {
        return 0;
    }
    if (w->x696 == 0) {
        if ((pG->flags_51E4 & 0xF) != (em->emsetNo & 0xF)) {
            return 0;
        }
    }
    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if (Ctrl12Ck(w->pCtrl12, 8)) {
        return 0;
    }
    if (pG->x4F88 <= 3) {
        if (!em10ThrowNearCk(em)) {
            return 0;
        }
        if (!em10ScreenInCk(em)) {
            return 0;
        }
    }
    ang = fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f));
    if (ang > 0.7853982f) {
        return 0;
    }
    if (em->flags_3C8 & 0x00010000) {
        if (em->plDist2 < 9000000.0f) {
            return 0;
        }
        if (em->plDist2 > 100000000.0f) {
            return 0;
        }
        ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f));
        if (ang > 1.0471976f) {
            return 0;
        }
    } else {
        if (em->plDist2 < 20250000.0f) {
            return 0;
        }
        if (em->plDist2 > 49000000.0f) {
            return 0;
        }
        ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f));
        if (ang > 0.3926991f) {
            return 0;
        }
        if (Rnd() & 1) {
            return 0;
        }
    }
    a = em->pos;
    b = pPLS->pos;
    a.y += 1500.0f;
    b.y += 1500.0f;
    hit = EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x4000);
    if (hit) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x24, hit, hit);
    if (pG->x4F88 <= 3) {
        Ctrl12Set(w->pCtrl12, 6, 60);
        Ctrl12Set(w->pCtrl12, 8, 120);
    } else if (pG->stage_no <= 2 && pG->x4F88 <= 9) {
        Ctrl12Set(w->pCtrl12, 6, 30);
        Ctrl12Set(w->pCtrl12, 8, 120);
    }
    return 1;
}

int em10ThrowBombCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    f32 ang;
    int hit;

    if (w->x58C != 0) {
        return 0;
    }
    if (w->pParasite != 0) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    if (w->wepType != 9) {
        return 0;
    }
    if (w->pWep == 0) {
        return 0;
    }
    if (w->x640 == 0) {
        return 0;
    }
    if (!(em->flags_3C8 & 0x00010000)) {
        return 0;
    }
    if (w->x696 == 0) {
        if ((pG->flags_51E4 & 0xF) != (em->emsetNo & 0xF)) {
            return 0;
        }
    }
    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if (Ctrl12Ck(w->pCtrl12, 8)) {
        return 0;
    }
    if (pG->x4F88 <= 3) {
        if (!em10ThrowNearCk(em)) {
            return 0;
        }
        if (!em10ScreenInCk(em)) {
            return 0;
        }
    }
    ang = fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f));
    if (ang > 0.7853982f) {
        return 0;
    }
    if (em->plDist2 < 12250000.0f) {
        return 0;
    }
    if (em->plDist2 > 225000000.0f) {
        return 0;
    }
    ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f));
    if (ang > 1.0471976f) {
        return 0;
    }
    a = em->pos;
    b = pPLS->pos;
    a.y += 1500.0f;
    b.y += 1500.0f;
    hit = EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x4000);
    if (hit) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x25, hit, hit);
    if (pG->x4F88 <= 3) {
        Ctrl12Set(w->pCtrl12, 6, 60);
        Ctrl12Set(w->pCtrl12, 8, 120);
    } else if (pG->stage_no <= 2 && pG->x4F88 <= 9) {
        Ctrl12Set(w->pCtrl12, 6, 30);
        Ctrl12Set(w->pCtrl12, 8, 120);
    }
    return 1;
}

// Melee weapon swing start check (axe / sickle / pitchfork ...): routine 1B (running swing) or 26/28.
extern "C" int em10AxeAtkCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    Vec c;
    f32 lim;

    if (w->x58C) {
        return 0;
    }
    if (w->pParasite) {
        return 0;
    }
    if (em->x3E0 & 0x80) {
        return 0;
    }
    switch (w->wepType) {
    default:
        if (em->type != 2 && em->type != 0x18) {
            return 0;
        }
        break;
    case 2:
    case 3:
    case 5:
    case 7:
    case 0xA:
    case 0xB:
    case 0xF:
    case 0x10:
        if (w->pWep == 0) {
            return 0;
        }
        break;
    }
    if (w->x67C != 0) {
        return 0;
    }
    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if ((w->flags & 0x08000000) && pSUB && ((pG->flags_5014 & 0x00800000) || em->type == 0x18 || em->type == 2)) {
        if (!(w->flags & 2)) {
            return 0;
        }
        if (w->x510 > 0.7853982f) {
            return 0;
        }
        if (fabsf(em->pos.y - pSUB->pos.y) > 1500.0f) {
            return 0;
        }
        if (w->x514 > 2250000.0f) {
            return 0;
        }
        a = em->pos;
        b = pSUB->pos;
        a.y += 1500.0f;
        b.y += 1500.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x4000)) {
            return 0;
        }
    } else {
        if (!(w->flags & 1)) {
            return 0;
        }
        if (w->x508 > 0.7853982f) {
            return 0;
        }
        if (fabsf(em->pos.y - pPL->pos.y) > 1500.0f) {
            return 0;
        }
        lim = 2890000.0f;
        if (w->wepType == 7 && !(em->flags_3C8 & 0x00081300) && !w->x58C && !w->pParasite && (pG->room_id32 & 0xFFFF0000) != 0x011C0000) {
            lim = 6250000.0f;
        }
        if (em->plDist2 > lim) {
            if (!em10PlRunCk(em)) {
                return 0;
            }
            if (em->plDist2 > 16000000.0f) {
                return 0;
            }
        }
        if (pG->x4F88 <= 3) {
            if (!em10ScreenInCk(em)) {
                return 0;
            }
        }
        a = em->pos;
        c = pPLS->pos;
        a.y += 1500.0f;
        c.y += 1500.0f;
        if (EatMgr.hitCheck(&a, &c, 0, 0, 0, 0x4000)) {
            return 0;
        }
    }
    if (pG->x4F88 <= 1 && !EM_RTN(em, 1, 0x1B)) {
        u8 r = Rnd() % 10;
        if (r > 4) {
            w->x67C = 0x1E;
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            return 1;
        }
    }
    if (w->wepType == 7 && !(em->flags_3C8 & 0x00081300) && !w->x58C && !w->pParasite && (pG->room_id32 & 0xFFFF0000) != 0x011C0000) {
        EmRoutineSet(em, 1, 0x28, 0, 0);
    } else {
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
    if (pG->x4F88 <= 3) {
        Ctrl12Set(w->pCtrl12, 6, 0x3C);
        Ctrl12Set(w->pCtrl12, 8, 0x78);
    } else if (pG->stage_no <= 2 && pG->x4F88 <= 9) {
        Ctrl12Set(w->pCtrl12, 6, 0x1E);
        Ctrl12Set(w->pCtrl12, 8, 0x78);
    }
    return 1;
}

extern "C" int em10ShieldAtkCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    f32 d;
    Vec c;
    u8 r;

    if (w->x58C != 0) {
        return 0;
    }
    if (w->pParasite != 0) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    if (w->pShield == 0) {
        return 0;
    }
    if (w->x67C != 0) {
        return 0;
    }
    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if ((w->flags & 0x08000000) && pSUB) {
        if (!(w->flags & 2)) {
            return 0;
        }
        if (w->x510 > 0.7853982f) {
            return 0;
        }
        {
            // Multi-set `d` is not local-allocated, so the fsubs result cannot tie to the dying `t`
            // (em->pos.y stays f0) and lands in d's register f13: `fsubs f13, f0, f13; fabs f13`.
            f32 t = em->pos.y;
            d = pSUB->pos.y;
            d = t - d;
            d = fabsf(d);
        }
        if (d > 1500.0f) {
            return 0;
        }
        if (w->x514 > 4000000.0f) {
            return 0;
        }
        a = em->pos;
        b = pSUB->pos;
        a.y += 1500.0f;
        b.y += 1500.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x4000)) {
            return 0;
        }
    } else {
        if (!(w->flags & 1)) {
            return 0;
        }
        if (w->x508 > 0.7853982f) {
            return 0;
        }
        if (fabsf(em->pos.y - pPL->pos.y) > 1500.0f) {
            return 0;
        }
        if (em->plDist2 > 4000000.0f) {
            if (!em10PlRunCk(em)) {
                return 0;
            }
            if (em->plDist2 > 18490000.0f) {
                return 0;
            }
        }
        if (pG->x4F88 <= 3) {
            if (!em10ScreenInCk(em)) {
                return 0;
            }
        }
        a = em->pos;
        c = pPLS->pos;
        a.y += 1500.0f;
        c.y += 1500.0f;
        if (EatMgr.hitCheck(&a, &c, 0, 0, 0, 0x4000)) {
            return 0;
        }
    }
    if (pG->x4F88 <= 1 && !EM_RTN(em, 1, 0x1B) && (r = Rnd() % 10, r > 4)) {
        w->x67C = 30;
        EmRoutineSet(em, 1, 0x1B, 0, 0);
        return 1;
    }
    if (w->wepType == 0xB && (Rnd() & 1)) {
        EmRoutineSet(em, 1, 0x26, 0, 0);
    } else {
        EmRoutineSet(em, 1, 0x27, 0, 0);
    }
    if (pG->x4F88 <= 3) {
        Ctrl12Set(w->pCtrl12, 6, 60);
        Ctrl12Set(w->pCtrl12, 8, 120);
    } else if (pG->stage_no <= 2 && pG->x4F88 <= 9) {
        Ctrl12Set(w->pCtrl12, 6, 30);
        Ctrl12Set(w->pCtrl12, 8, 120);
    }
    return 1;
}

extern "C" int em10SukiAtkCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    EM10_WEP_ATK_CK(em, w, 1, 0x29);
}

extern "C" int em10ScytheAtkCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    EM10_WEP_ATK_CK(em, w, 6, 0x2A);
}

extern "C" int em10ClawAtkCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    int hit;

    if (em->type != 10 && em->type != 13) {
        return 0;
    }
    if (!(w->flags & 0x100)) {
        return 0;
    }
    if (w->x67C != 0) {
        return 0;
    }
    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if (!(w->flags & 1)) {
        return 0;
    }
    if (w->x508 > 0.7853982f) {
        return 0;
    }
    if (fabsf(em->pos.y - w->x534.y) > 1500.0f) {
        return 0;
    }
    {
        f32 dx = em->pos.x - w->x534.x;
        f32 dz = em->pos.z - w->x534.z;
        if (dx * dx + dz * dz > 1960000.0f) {
            return 0;
        }
    }
    a = em->pos;
    b = w->x534;
    a.y += 1500.0f;
    b.y += 1500.0f;
    hit = EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x4000);
    if (hit) {
        return 0;
    }
    if (w->x6BE == 4 || w->x6BF == 4) {
        EmRoutineSet(em, 1, 0x59, hit, hit);
        return 1;
    }
    EmRoutineSet(em, 1, 0x2B, hit, hit);
    if (pG->x4F88 <= 3) {
        Ctrl12Set(w->pCtrl12, 6, 0x3C);
        Ctrl12Set(w->pCtrl12, 8, 0x78);
    } else if (pG->stage_no <= 2 && pG->x4F88 <= 9) {
        Ctrl12Set(w->pCtrl12, 6, 0x1E);
        Ctrl12Set(w->pCtrl12, 8, 0x78);
    }
    return 1;
}

extern "C" int em10ClawCriAtkCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    u8 r;

    if (em->type != 0xA && em->type != 0xD) {
        return 0;
    }
    if (!(w->flags & 0x100)) {
        return 0;
    }
    if (w->x67C != 0) {
        return 0;
    }
    if ((s16) w->x646 != 0) {
        return 0;
    }
    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if ((w->flags & 0x08000000) && pSUB != 0) {
        return 0;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 1500.0f) {
        return 0;
    }
    if (em->plDist2 < 12250000.0f || em->plDist2 > 225000000.0f) {
        return 0;
    }
    if (!(Rnd() & 3) && !(pG->flags_5010 & 0x20000000)) {
        w->x646 = 150;
        return 0;
    }
    a = em->pos;
    b = pPLS->pos;
    a.y += 1500.0f;
    b.y += 1500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    if (em->plDist2 > 25000000.0f) {
        EmRoutineSet(em, 1, 0x2D, 0, 0);
    } else if (em->plDist2 > 9000000.0f && (r = Rnd() % 10, r > 4)) {
        EmRoutineSet(em, 1, 0x2D, 0, 0);
    } else {
        EmRoutineSet(em, 1, 0x2C, 0, 0);
    }
    if (pG->x4F88 <= 3) {
        Ctrl12Set(w->pCtrl12, 6, 60);
        Ctrl12Set(w->pCtrl12, 8, 120);
    } else if (pG->stage_no <= 2 && pG->x4F88 <= 9) {
        Ctrl12Set(w->pCtrl12, 6, 30);
        Ctrl12Set(w->pCtrl12, 8, 120);
    }
    return 1;
}

extern "C" int em10CsawAtkCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    Vec c;
    int hit;
    u8 r;

    if (w->wepType != 4) {
        return 0;
    }
    if (w->pWep == 0) {
        return 0;
    }
    if (w->x67C != 0) {
        return 0;
    }
    if (w->x58C != 0) {
        return 0;
    }
    if (w->pParasite != 0) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    if (Ctrl12Ck(w->pCtrl12, 6)) {
        return 0;
    }
    if (!(w->flags & 0x08000000)) {
        if (!(w->flags & 1)) {
            return 0;
        }
        if (w->x508 > 0.7853982f) {
            return 0;
        }
        if (fabsf(em->pos.y - pPL->pos.y) > 500.0f) {
            return 0;
        }
        if (em->plDist2 > 2250000.0f) {
            if (!em10PlRunCk(em)) {
                return 0;
            }
            if (em->plDist2 > 16000000.0f) {
                return 0;
            }
        }
        if (pG->x4F88 <= 3) {
            if (!em10ScreenInCk(em)) {
                return 0;
            }
        }
        a = em->pos;
        b = pPLS->pos;
        a.y += 1500.0f;
        b.y += 1500.0f;
        hit = EatMgr.hitCheck(&a, &b, 0, 0, 0, 0);
        if (hit) {
            return 0;
        }
        r = Rnd() % 100;
        if (r <= 29 || pSys->region == 0) {
            EmRoutineSet(em, 1, 0x2F, hit, hit);
        } else {
            EmRoutineSet(em, 1, 0x31, hit, hit);
        }
        if (pG->x4F88 <= 3) {
            Ctrl12Set(w->pCtrl12, 6, 0x3C);
            Ctrl12Set(w->pCtrl12, 8, 0x78);
        } else if (pG->stage_no <= 2 && pG->x4F88 <= 9) {
            Ctrl12Set(w->pCtrl12, 6, 0x1E);
            Ctrl12Set(w->pCtrl12, 8, 0x78);
        }
        return 1;
    } else {
        if (!(w->flags & 2)) {
            return 0;
        }
        if (w->x510 > 0.7853982f) {
            return 0;
        }
        if (fabsf(em->pos.y - pSUB->pos.y) > 500.0f) {
            return 0;
        }
        if (w->x514 > 2250000.0f) {
            return 0;
        }
        a = em->pos;
        c = pSUBS->pos;
        a.y += 1500.0f;
        c.y += 1500.0f;
        if (EatMgr.hitCheck(&a, &c, 0, 0, 0, 0)) {
            return 0;
        }
        EmRoutineSet(em, 1, 0x2F, 0, 0);
        return 1;
    }
}

extern "C" int em10ThreatCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx m;
    Vec v;

    if (pPL->xFC != 0) {
        return 0;
    }
    if (pPL->xFD != 3) {
        return 0;
    }
    if (w->pShield) {
        return 0;
    }
    if (w->pParasite) {
        return 0;
    }
    if (w->x58C) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) > 0.7853982f) {
        return 0;
    }
    PSMTXInverse(pPL->mat, m);
    PSMTXMultVec(m, &em->pos, &v);
    if (v.x > 3000.0f || v.x < -3000.0f) {
        return 0;
    }
    if (v.x > -500.0f && v.x < 500.0f) {
        return 0;
    }
    if (!(v.z < 1500.0f) && !(v.z > 5000.0f)) {
        EmRoutineSet(em, 1, 0x16, 0, 0);
        return 1;
    }
    return 0;
}

extern "C" int em10PlRunCk(cEm10* em)
{
    Mtx m;
    Vec v;

    if (pPL->xFC != 0) {
        return 0;
    }
    if (pPL->xFD != 3) {
        return 0;
    }
    if (pG->x4F88 > 3) {
        if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) > 0.7853982f) {
            return 0;
        }
        PSMTXInverse(pPL->mat, m);
        PSMTXMultVec(m, &em->pos, &v);
        if (v.x > 1500.0f) {
            return 0;
        }
        if (v.x < -1500.0f) {
            return 0;
        }
        return 1;
    }
    return 0;
}

extern "C" int em10HeadLockCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx inv;
    Vec v;
    cModel* p;
    int r;

    if (pPL->xFC != 0) {
        return 0;
    }
    if (pPL->xFD != 6) {
        return 0;
    }
    if (em->plDist2 > 64000000.0f) {
        return 0;
    }
    if (!(w->flags & 0x100)) {
        return 0;
    }
    if (w->x58C) {
        return 0;
    }
    if (w->pParasite) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    if (pG->x4F88 <= 3) {
        return 0;
    }
    if (w->pShield) {
        return 0;
    }
    if (em->type == 10 || em->type == 13 || em->type == 2) {
        return 0;
    }
    if (em->flags_3C8 & 0x200) {
        if (w->x6C5 == 1) {
            return 0;
        }
        if (w->x6C5 == 2) {
            return 0;
        }
    }
    if (pG->wep_no == 0x10) {
        return 0;
    }
    if (!ItemMgr.bulletNumCurrent()) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) > 0.7853982f) {
        return 0;
    }
    PSMTXInverse(pPL->getPartsPtr(10)->mat, inv);
    p = em->getPartsPtr(4);
    v.x = 0.0f;
    v.y = 150.0f;
    v.z = 0.0f;
    PSMTXMultVec(p->mat, &v, &v);
    PSMTXMultVec(inv, &v, &v);
    if (v.x > 0.0f) {
        return 0;
    }
    if (v.z > 300.0f || v.z < -300.0f) {
        return 0;
    }
    if (v.y > 300.0f) {
        return 0;
    }
    r = 0;
    if (v.y < -300.0f) {
        return r;
    }
    return 1;
}

void em10NeckMove(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModel* p;
    Mtx m;
    Vec v;
    Vec d;
    f32 a;
    f32 l;

    if (w->flags & 0x00400000) {
        return;
    }
    if (w->x58C != 0 || w->pParasite != 0) {
        w->flags &= ~0x00040000;
    }
    p = em->getPartsPtr(4);
    if (!(w->flags & 0x08000000)) {
        cModel* h = pPL->getPartsPtr(4);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(h->mat, &v, &v);
    } else {
        cModel* h = pSUB->getPartsPtr(4);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(h->mat, &v, &v);
    }
    if (w->flags & 0x00040000) {
        w->x5CC = w->x5CC * 0.9f + Muku(&em->pos, &v, em->rot.y, 1.0471976f) * 0.1f;
        PSVECSubtract(&v, &p->worldPos, &d);
        l = SQRTF(d.x * d.x + d.z * d.z);
        a = -atan2f(d.y, l);
        if (a > 0.7853982f) {
            a = 0.7853982f;
        }
        if (a < -0.7853982f) {
            a = -0.7853982f;
        }
        w->x5C8 = w->x5C8 * 0.9f + a * 0.1f;
    } else {
        w->x5C8 = w->x5C8 * 0.9f;
        w->x5CC = w->x5CC * 0.9f;
    }
    p = em->getPartsPtr(3);
    PARTS_FLAGS(p) |= 0x40000000;
    PARTS_ROT_OFS(p).x = w->x5C8;
    PARTS_ROT_OFS(p).y = w->x5CC;
    PARTS_ROT_OFS(p).z = 0.0f;
    if (w->flags & 0x02000000) {
        w->x5D0 = w->x5D0 * 0.9f + -w->x5C8 * 0.1f;
    } else {
        w->x5D0 = w->x5D0 * 0.9f;
    }
    a = fabsf(w->x5D0); // reuses the atan2 local: one global pseudo, allocated f1 after the locals
    if (!(a < 0.01f)) {
        p = em->getPartsPtr(13);
        PSMTXRotRad(m, 'z', w->x5D0);
        PSMTXConcat(p->worldMat, m, p->worldMat);
    }
}

void em10WaistMove(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    f32 r;
    cModel* p;

    if (w->flags & 0x00400000) {
        return;
    }
    if (w->flags & 0x200) {
        if (em->flags_3C8 & 0x01000000) {
            v.x = 300.0f;
            v.y = 0.0f;
            v.z = 0.0f;
        } else {
            v.x = -300.0f;
            v.y = 0.0f;
            v.z = 0.0f;
        }
        PSMTXMultVec(em->mat, &v, &v);
        w->x5D4 = w->x5D4 * 0.9f + Muku(&v, &pPL->pos, em->rot.y, 0.7853982f) * 0.1f;
    } else {
        w->x5D4 = w->x5D4 * 0.9f;
    }
    r = w->x5D4 * 0.5f;
    p = em->getPartsPtr(1);
    PARTS_FLAGS(p) |= 0x40000000;
    PARTS_ROT_OFS(p).x = 0.0f;
    PARTS_ROT_OFS(p).y = r;
    PARTS_ROT_OFS(p).z = 0.0f;
    p = em->getPartsPtr(2);
    PARTS_FLAGS(p) |= 0x40000000;
    PARTS_ROT_OFS(p).x = 0.0f;
    PARTS_ROT_OFS(p).y = r;
    PARTS_ROT_OFS(p).z = 0.0f;
}

void em10ScaleCompress(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx m;
    Vec s;
    cModel* p;

    // Two ifs, not `&&`: fold_truthop would merge the adjacent u8 compares into one u16 compare.
    if (em->xFC == 3) {
        if (em->xFD == 3) {
            PSMTXIdentity(m);
            s.x = 1.0f;
            s.y = w->x5D8;
            s.z = 1.0f;
            ScaleMatrix(m, &s);
            for (p = em->pParts; p; p = p->pParts) {
                PSMTXConcat(m, p->mat, p->mat);
                p->mat[0][3] = p->worldPos.x;
                p->mat[1][3] = p->worldPos.y;
                p->mat[2][3] = p->worldPos.z;
            }
        }
    }
}

int em10FindCk(cEm10* em, int a)
{
    Em10Work* w = EM10_WK(em);
    int find = 0;
    int dead;

    if (w->flags & 0x100) {
        return 0;
    }
    if (em->type == 10 || em->type == 13) {
        if (a == 2) {
            return 0;
        }
    }
    if (w->flags & 1) {
        f32 r;
        switch (w->x5EC) {
        case 0:
        case 6:
        case 7:
        case 10:
        case 11:
        case 12:
        case 13:
            r = 225000000.0f;
            break;
        default:
            r = 36000000.0f;
            break;
        }
        if (em->plDist2 < r) {
            if (w->x508 < 1.0471976f) {
                find = 1;
            }
        }
        if ((s32) pG->flags_5010 < 0) {
            if (em->plDist2 < 25000000.0f) {
                find = 1;
            }
        }
        if (em->plDist2 < 12250000.0f) {
            find = 1;
        }
        if (a != 0) {
            if (fabsf(em->pos.y - pPL->pos.y) > 2000.0f) {
                find = 0;
            }
        }
    }
    if (em10SomebodyDamageNowCk(em)) {
        find = 1;
    }
    if (!(em->flags_3C8 & 0x10)) {
        switch (w->x5EC) {
        case 0:
        case 6:
        case 7:
        case 10:
        case 11:
        case 12:
        case 13:
            if (pG->flags_5010 & 0x20000000) {
                // em3c bell idiom: three identical arms assigning `r` keep the dispatch compares; the
                // override in the distance block makes the arm sets dead (flow deletes them, the
                // compares stay) and `r` a block-local pseudo loaded at the use (local-alloc gives
                // it the next free FPR, f9, and the high r10 because r9/r11 hold pG).
                f32 r;
                switch (pG->bell_stat) {
                case 0:
                    r = 25000.0f;
                    break;
                case 1:
                    r = 25000.0f;
                    break;
                default:
                    r = 25000.0f;
                    break;
                }
                {
                    f32 dx = em->pos.x - pGS->bell_pos.x;
                    f32 dy = em->pos.y - pGS->bell_pos.y;
                    f32 dz = em->pos.z - pGS->bell_pos.z;
                    r = 25000.0f;
                    if (dx * dx + dy * dy + dz * dz < r * r) {
                        if ((w->flags & 1) && w->x524 < r) {
                            find = 1;
                        }
                    }
                }
            }
            if (pG->flags_500C & 0x00800000) {
                if (w->x524 < 25000.0f) {
                    find = 1;
                }
            }
            break;
        }
    }
    dead = em10DeadCk(em);
    if (dead) {
        find = 1;
    }
    if (em->flags_3C8 & 0x80) {
        find = 1;
    }
    switch (find) {
    case 0:
        return 0;
    case 1:
        em10SetRtnFind(em);
        return 1;
    default:
        return 0;
    }
}
int em10FindCk2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->x5EC) {
        return 0;
    }
    if (pG->flags_5010 & 0x20000000) {
        if (pG->bell_stat == 2) {
            f32 dx = em->pos.x - pG->bell_pos.x;
            f32 dy = em->pos.y - pG->bell_pos.y;
            f32 dz = em->pos.z - pG->bell_pos.z;
            if (dx * dx + dy * dy + dz * dz < 400000000.0f) {
                w->x5F0 = pG->bell_pos;
                w->x646 = 0;
                return 1;
            }
        }
        if (pG->bell_stat == 1) {
            f32 dx = em->pos.x - pG->bell_pos.x;
            f32 dy = em->pos.y - pG->bell_pos.y;
            f32 dz = em->pos.z - pG->bell_pos.z;
            if (dx * dx + dy * dy + dz * dz < 400000000.0f) {
                w->x5F0 = pG->bell_pos;
                return 1;
            }
        }
    }
    if ((s32) pG->flags_5010 < 0) {
        f32 r = 12250000.0f;
        if (EM_RTN(pPL, 0, 3)) {
            r = 64000000.0f;
        }
        if (em->plDist2 < r) {
            w->x5F0 = pPL->pos;
            return 1;
        }
    }
    if (em->plDist2 < 1000000.0f) {
        w->x5F0 = pPL->pos;
        return 1;
    }
    if ((pG->flags_500C & 0x00800000) && w->x524 < 25000.0f) {
        w->x5F0 = pPL->pos;
        return 1;
    }
    return 0;
}

extern "C" int em10SomebodyFindNowCk(cEm10* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        if (EM_RTN(e, 1, 0xD)) {
            return 0;
        }
    }
    return 1;
}

// Dead-stripped by the original REL link (body gone, constant pool kept at .rodata 0x13D4:
// 25000, 600, 100000, -100000). Never called; only the pool matters (modules.py STRIP_UNUSED).
static int em10FindFloorCk(cEm10* em)
{
    Vec v = pPL->pos;
    f32 y;

    if (em->plDist2 > 25000.0f) {
        return 0;
    }
    y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
    if (y < -100000.0f) {
        return 0;
    }
    return 1;
}

extern "C" int em10SomebodyDamageNowCk(cEm10* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        int dm;
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        dm = e->xFC == 2;
        if (e->hp <= 0) {
            if (e->xFC != 3) {
                continue;
            }
            if (e->xFD == 3) {
                continue;
            }
            dm = 1;
        }
        if (!dm) {
            continue;
        }
        if ((G_ROOM_ID32 & 0xFFFF0000) != 0x01010000) {
            f32 dx = em->pos.x - e->pos.x;
            f32 dy = em->pos.y - e->pos.y;
            f32 dz = em->pos.z - e->pos.z;
            f32 d = dx * dx + dy * dy + dz * dz;
            if (!(d < 9000000.0f)) {
                if (!(fabsf(Muku(&em->pos, &e->pos, em->rot.y, 3.1415927f)) < 1.0471976f)) {
                    continue;
                }
                if (!(d < 100000000.0f)) {
                    continue;
                }
            }
        }
        return 1;
    }
    return 0;
}

int em10SomebodyNearCk(cEm10* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        {
            f32 dx = em->pos.x - e->pos.x;
            f32 dy = em->pos.y - e->pos.y;
            f32 dz = em->pos.z - e->pos.z;
            if (dx * dx + dy * dy + dz * dz < 9000000.0f) {
                return 1;
            }
        }
    }
    return 0;
}

void em10FindNotify(cEm10* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        {
            f32 dx = em->pos.x - e->pos.x;
            f32 dz = em->pos.z - e->pos.z;
            f32 d = dx * dx + dz * dz;
            if (em->flags_3C8 & 0x10) {
                if (d > 100000000.0f) {
                    continue;
                }
            } else {
                if (d > 625000000.0f) {
                    continue;
                }
            }
        }
        em->setFindPL();
    }
}

void em10WalkRtnSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int r;

    if (w->wepType == 0xC) {
        if (w->pWep->getPartsPtr(2)->scale.x == 0.0f) {
            em->setWeaponFall();
        }
    }
    if (em10GotoCk(em)) {
        return;
    }
    switch (em->x38D) {
    case 0x24:
        EmRoutineSet(em, 1, 0x50, 0, 0);
        return;
    case 0x25:
        EmRoutineSet(em, 1, 0x50, 0, 1);
        return;
    case 0x2B:
        EmRoutineSet(em, 1, 0x50, 0, 2);
        return;
    case 0x36:
        EmRoutineSet(em, 1, 0x60, 0, 0);
        return;
    case 0x19:
        EmRoutineSet(em, 1, 0x48, 0, 0);
        return;
    case 0x3E:
        EmRoutineSet(em, 1, 0x6A, 0, 0);
        return;
    case 0x40:
        EmRoutineSet(em, 1, 0x6C, 0, 0);
        return;
    case 0x23:
        if (!(w->x52C < em->x3CC)) {
            EmRoutineSet(em, 1, 0x4F, 0, 0);
            return;
        }
        em->x38D = 0;
        break;
    case 0x3C:
        if (!(w->x52C < em->x3CC)) {
            EmRoutineSet(em, 1, 0x68, 0, 0);
            return;
        }
        em->x38D = 0;
        break;
    }
    if (w->pWep2 != 0 && w->pParasite == 0 && w->x58C == 0 && (w->pWep == 0 || w->wepType == 5)) {
        em->setWeaponFall();
        EmRoutineSet(em, 1, 0xC, 0, 0);
        return;
    }
    if (em10IgnitionCk(em)) {
        return;
    }
    if (em10ClawStickCK(em)) {
        return;
    }
    if ((s16) pG->pl_life <= 0) {
        EmRoutineSet(em, 1, 0x1B, 0, 0);
        return;
    }
    if (em->flags_3C8 & 0x80) {
        EmRoutineSet(em, 1, 0x11, 0, 0);
        return;
    }
    if (em->x3D0 == 3) {
        if (EM_RTN(em, 1, 0x1B)) {
            return;
        }
        EmRoutineSet(em, 1, 0x1B, 0, 0);
        return;
    }
    if (w->x51C > 2.7488937f) {
        EmRoutineSet(em, 1, 0x15, 0, 0);
        return;
    }
    if (w->wepType == 8 || w->wepType == 0xC) {
        if (em->plDist2 < 9000000.0f && pG->x4F88 <= 9 && !(em->be_flag & 0x20000000)) {
            w->x644 = 120;
            EmRoutineSet(em, 1, 0x11, 0, 0);
            return;
        }
        if (w->flags & 1) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            return;
        }
    }
    if (em10BackCk(em)) {
        return;
    }
    if (em10StayCk(em)) {
        return;
    }
    if (em->type == 0x16) {
        EmRoutineSet(em, 1, 0x6D, 0, 0);
        return;
    }
    r = em10DashCk(em);
    if (r) {
        return;
    }
    w->x6AC = Rnd() % 11;
    EmRoutineSet(em, 1, 0x10, 0, 0);
}

int em10GotoCk(cEm10* em)
{
    if (EM10_WK(em)->x5EC) {
        EmRoutineSet(em, 1, 0x13, 0, 0);
        return 1;
    }
    return 0;
}

extern "C" int em10BackCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int back = 0;

    if ((s16) w->x67C != 0) {
        back = 1;
    }
    if (Ctrl12Ck(w->pCtrl12, 9)) {
        back = 1;
    }
    if (!back) {
        return 0;
    }
    if (em->x3D0 == 3) {
        if (EM_RTN(em, 1, 0x1B)) {
            return 1;
        }
    } else if (em->plDist2 > 4000000.0f) {
        if (em->x3D0 == 2) {
            return 0;
        }
        if (EM_RTN(em, 1, 0x1B)) {
            return 0;
        }
    } else if (w->wepType != 4) {
        EmRoutineSet(em, 1, 0x12, 0, 0);
        return 1;
    }
    EmRoutineSet(em, 1, 0x1B, 0, 0);
    return 1;
}

void em10SlopeMove(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    Vec c;
    Mtx m;
    f32 ang;
    f32 t;
    f32 fa;
    f32 fb;

    if (pG->flags_500C & 0x1000) {
        return;
    }
    if (w->flags & 0x00400000) {
        return;
    }
    if (w->flags & 0x400) {
        return;
    }
    if (em->motFlags2 & 0x40000000) {
        return;
    }
    if (em->hp <= 0) {
        em->atari.rectZ = 0.0f;
    } else if (w->pShield != 0 || em->type == 0xA || em->type == 0xD) {
        em->atari.rectZ = em->atari.rectZ * 0.8f + 160.0f;
    } else {
        em->atari.rectZ = em->atari.rectZ * 0.8f + 50.0f;
    }
    em->atari.rectZ2 = em->atari.rectZ;
    if (w->flags & 0x01000000) {
        em->atari.rectX = em->atari.rectX * 0.7f + 180.0f;
    } else if (w->pShield != 0) {
        em->atari.rectX = em->atari.rectX * 0.7f + 150.0f;
    } else {
        em->atari.rectX = em->atari.rectX * 0.7f + 120.00001f;
    }
    em->atari.rectZ = em->atari.rectZ * 0.7f + 75.0f;
    fb = em->atari.rectX;
    em->atari.rectX2 = fb;
    if (w->flags & 0x01000000) {
        t = fb * em->scale.z - 100.0f;
        a.x = 0.0f;
        a.y = 1000.0f;
        a.z = t;
        b.x = 0.0f;
        b.y = 1000.0f;
        b.z = -t;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        fa = SatMgr.getFloor(&a, 600.0f, 100000.0f, 0, 0);
        fb = SatMgr.getFloor(&b, 600.0f, 100000.0f, 0, 0);
        if (fa == -100000.0f) {
            fa = em->pos.y;
        }
        if (fb == -100000.0f) {
            fb = em->pos.y;
        }
        fa -= fb;
        if (fa > 1500.0f) {
            fa = 0.0f;
        }
        if (fa < -1500.0f) {
            fa = 0.0f;
        }
        t = SQRTF((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
        ang = -atan2f(fa, t);
        if (w->x668 != 0) {
            w->x668--;
        }
    } else {
        ang = 0.0f;
        w->x668 = 30;
    }
    w->x598.x = w->x598.x * 0.95f + ang * 0.05f;
    RotMatrix(m, &w->x598);
    t = w->x598.x;
    if (t > 0.0f) {
        t -= 0.1f;
        if (t < 0.0f) {
            t = 0.0f;
        }
    } else {
        t += 0.1f;
        if (t > 0.0f) {
            t = 0.0f;
        }
    }
    if ((s16) w->x668 == 0 || (s16) w->x668 == 30) {
        t = 0.0f;
    }
    t *= 150.0f;
    w->x66C = w->x66C * 0.9f + t * 0.1f;
    c.x = 0.0f;
    c.y = 0.0f;
    c.z = w->x66C;
    PSMTXMultVecSR(em->mat, &c, &c);
    PSVECAdd(&em->pos, &c, &em->pos);
    PSMTXConcat(em->mat, m, em->mat);
    TransMatrix(em->mat, &em->pos);
}

extern "C" void em10CamMove(cEm10* em, int no, f32 rate, int shake)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    Vec hit;
    Vec d;
    Camera* c = &pG->Cam;
    cModel* p;
    cModel* q;

    switch (no) {
    case 0:
    default:
        v.x = 500.0f;
        v.y = 1600.0f;
        v.z = -2000.0f;
        PSMTXMultVec(pPL->mat, &v, &w->x608);
        break;
    case 1:
        v.x = 1500.0f;
        v.y = 1600.0f;
        v.z = 500.0f;
        PSMTXMultVec(pPL->mat, &v, &w->x608);
        break;
    }
    p = pPL->getPartsPtr(4);
    q = em->getPartsPtr(4);
    PSVECAdd(&p->worldPos, &q->worldPos, &v);
    PSVECScale(&v, &v, 0.5f);
    if (shake) {
        PosToPos(&c->param.at, &v, &w->cam.param.at, rate);
        PosToPos(&c->param.pos, &w->x608, &w->cam.param.pos, rate);
        v.x = fRand1_1() * 10.0f;
        v.y = fRand1_1() * 10.0f;
        v.z = fRand1_1() * 10.0f;
        PSVECAdd(&w->cam.param.pos, &v, &w->cam.param.pos);
        PSVECAdd(&w->cam.param.at, &v, &w->cam.param.at);
    } else {
        PosToPos(&c->param.at, &v, &w->cam.param.at, rate);
        PosToPos(&c->param.pos, &w->x608, &w->cam.param.pos, rate);
    }
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        if (dx * dx + dy * dy + dz * dz > 100.0f) {
            PSVECSubtract(&w->cam.param.pos, &w->cam.param.at, &d);
#line 32709 "D:/Bio4/Prog/em10.cpp"
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, 250.0f);
            PSVECAdd(&w->cam.param.pos, &d, &w->cam.param.pos);
            if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
                w->cam.param.pos = hit;
            }
            PSVECSubtract(&w->cam.param.pos, &d, &w->cam.param.pos);
        }
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        w->cam.dist = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    w->cam.param.fovy = 55.0f;
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

static Vec em10_campos2_r = { 1300.0f, 500.0f, 0.0f };
static Vec em10_campos2_l = { -1300.0f, 500.0f, 0.0f };
// 0xF8 explicitly zero-initialised bytes follow in .data (GCC 2.95 keeps `= {0}` aggregates out of .bss); nothing references them.
static Camera em10_campos2_cam = { 0 };

extern "C" void em10SetCampos2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;
    Vec out;
    Vec d;
    f32 h;
    f32 len;

    if (Rnd() & 1) {
        pos = em10_campos2_r;
    } else {
        pos = em10_campos2_l;
    }
    PSMTXMultVec(em->mat, &pos, &w->x608);
    pos = pPL->getPartsPtr(4)->worldPos;
    if (GetWaterHeight(&em->pos, &h) && w->x608.y < h) {
        w->x608.y = em->pos.y + 1500.0f;
    }
    if (EatMgr.hitCheck(&pos, &w->x608, &out, 0, 0, 0)) {
        PSVECSubtract(&out, &pos, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 32773 "D:/Bio4/Prog/em10.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&pos, &d, &w->x608);
    }
    w->cam.param.at = pos;
    w->cam.param.pos = w->x608;
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        w->cam.dist = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    w->cam.param.fovy = 55.0f;
    CameraSetOrientationUp(&w->cam);
}

void em10CamMove2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec pl;
    Vec d;
    Vec hit;
    Vec d2;
    Camera* c = &pG->Cam;

    pl = pPL->getPartsPtr(4)->worldPos;
    PSVECSubtract(&w->x608, &pl, &d);
    if (d.x * d.x + d.z * d.z > 2890000.0f) {
        d.y = 0.0f;
#line 32818 "D:/Bio4/Prog/em10.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, 1700.0f);
        PSVECAdd(&pl, &d, &d);
        d.y = w->x608.y;
        PosToPos(&w->x608, &d, &w->x608, 0.2f);
    }
    PosToPos(&c->param.at, &pl, &w->cam.param.at, 1.0f);
    PosToPos(&c->param.pos, &w->x608, &w->cam.param.pos, 1.0f);
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        if (dx * dx + dy * dy + dz * dz > 100.0f) {
            PSVECSubtract(&w->cam.param.pos, &w->cam.param.at, &d2);
#line 32836 "D:/Bio4/Prog/em10.cpp"
            VECNormalize(&d2, &d2);
            PSVECScale(&d2, &d2, 250.0f);
            PSVECAdd(&w->cam.param.pos, &d2, &w->cam.param.pos);
            if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
                w->cam.param.pos = hit;
            }
            PSVECSubtract(&w->cam.param.pos, &d2, &w->cam.param.pos);
        }
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        w->cam.dist = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    w->cam.param.fovy = 55.0f;
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

// Critical-hit (head burst / kick) cut-in camera: fixed offsets from the player matrix, optional shake.
extern "C" void em10CamMoveCri(cEm10* em, u32 no, int shake)
{
    Em10Work* w = EM10_WK(em);
    Camera* c = &pG->Cam;
    Vec a;
    Vec b;
    Vec hit;
    Vec d;

    switch (no) {
    case 0:
    default:
        a.x = 1716.6f;
        a.y = 3074.7f;
        a.z = 266.8f;
        b.x = -124.6f;
        b.y = 748.4f;
        b.z = -23.5f;
        w->cam.param.fovy = 50.0f;
        break;
    case 1:
        if (pSys->region == 0) {
            a.x = 762.0f;
            a.y = 1953.0f;
            a.z = 263.0f;
            b.x = -110.0f;
            b.y = 1083.0f;
            b.z = 212.0f;
        } else {
            a.x = -651.2f;
            a.y = 2688.1f;
            a.z = 147.3f;
            b.x = 174.0f;
            b.y = 887.6f;
            b.z = -17.1f;
        }
        w->cam.param.fovy = 50.0f;
        break;
    case 2:
        a.x = 120.0f;
        a.y = 1689.9f;
        a.z = 1118.5f;
        b.x = 9.2f;
        b.y = 1518.8f;
        b.z = -149.1f;
        w->cam.param.fovy = 30.0f;
        break;
    case 3:
        if (pSys->region == 0) {
            a.x = -1120.0f;
            a.y = 1269.0f;
            a.z = -494.1f;
            b.x = -16.0f;
            b.y = 1288.0f;
            b.z = 90.0f;
        } else {
            a.x = -87.3f;
            a.y = 1451.2f;
            a.z = -543.1f;
            b.x = 15.6f;
            b.y = 1250.6f;
            b.z = 293.6f;
        }
        w->cam.param.fovy = 50.0f;
        break;
    case 4:
        a.x = -957.6f;
        a.y = 1741.7f;
        a.z = -87.0f;
        b.x = 104.5f;
        b.y = 1234.5f;
        b.z = 120.06f;
        w->cam.param.fovy = 50.0f;
        break;
    }
    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    if (shake) {
        PosToPos(&c->param.at, &b, &w->cam.param.at, 1.0f);
        PosToPos(&c->param.pos, &a, &w->cam.param.pos, 1.0f);
        a.x = fRand1_1() * 10.0f;
        a.y = fRand1_1() * 10.0f;
        a.z = fRand1_1() * 10.0f;
        PSVECAdd(&w->cam.param.pos, &a, &w->cam.param.pos);
        PSVECAdd(&w->cam.param.at, &a, &w->cam.param.at);
    } else {
        PosToPos(&c->param.at, &b, &w->cam.param.at, 1.0f);
        PosToPos(&c->param.pos, &a, &w->cam.param.pos, 1.0f);
    }
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        if (dx * dx + dy * dy + dz * dz > 100.0f) {
            PSVECSubtract(&w->cam.param.pos, &w->cam.param.at, &d);
#line 32943 "D:/Bio4/Prog/em10.cpp"
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, 250.0f);
            PSVECAdd(&w->cam.param.pos, &d, &w->cam.param.pos);
            if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
                w->cam.param.pos = hit;
            }
            PSVECSubtract(&w->cam.param.pos, &d, &w->cam.param.pos);
        }
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        w->cam.dist = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

extern "C" void em10CamMoveAshley(cEm10* em, u32 no)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    Vec hit;
    Vec d;
    Camera* c = &pG->Cam;

    switch (no) {
    case 0:
    default:
        a.x = 957.0f;
        a.y = 1872.0f;
        a.z = 676.0f;
        b.x = -122.0f;
        b.y = 1622.0f;
        b.z = 49.0f;
        break;
    case 1:
        a.x = -1105.0f;
        a.y = 1590.1884f;
        a.z = -686.0f;
        b.x = -114.0f;
        b.y = 1589.0f;
        b.z = 54.0f;
        break;
    case 2:
        a.x = -1665.0f;
        a.y = 1750.1f;
        a.z = 1387.4f;
        b.x = -39.4f;
        b.y = 1339.7f;
        b.z = 376.8f;
        break;
    case 3:
        a.x = 1563.0f;
        a.y = 976.6f;
        a.z = -693.4f;
        b.x = -71.6f;
        b.y = 1339.7f;
        b.z = 318.0f;
        break;
    }
    FSet(w->cam.param.fovy, 50.0f);
    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    PosToPos(&c->param.at, &b, &w->cam.param.at, 0.2f);
    PosToPos(&c->param.pos, &a, &w->cam.param.pos, 0.2f);
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        if (dx * dx + dy * dy + dz * dz > 100.0f) {
            PSVECSubtract(&w->cam.param.pos, &w->cam.param.at, &d);
#line 33013 "D:/Bio4/Prog/em10.cpp"
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, 250.0f);
            PSVECAdd(&w->cam.param.pos, &d, &w->cam.param.pos);
            if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
                w->cam.param.pos = hit;
            }
            PSVECSubtract(&w->cam.param.pos, &d, &w->cam.param.pos);
        }
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        w->cam.dist = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

// Grow the parasite (Plaga) out of the neck: the body object and the four head/tentacle objects.
void em10SetParasite(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    PlArc* arc;
    Vec pos;
    Vec rot;
    cObj* o;
    u32 n;
    void* bin;
    void* tpl;
    void* m0;
    void* m1;
    void* m2;
    void* m3;
    void* m4;
    void* m5;
    void* m6;
    void* m7;
    void* m8;
    void* m9;
    void* m10;
    int type;
    int heads;

    if (em->hp <= 0) {
        return;
    }
    em->flags_3C8 |= 0x20;
    if (w->x6C5 == 1) {
        EstSet((int) em, -1, 0, 0, 0x10, 0x5B, 0, 0, (u32) em, 0);
    } else {
        EstSet((int) em, -1, 0, 0, 0x10, 8, 0, 0, (u32) em, 0);
    }
    if (em10SearchParasite(em)) {
        return;
    }
    arc = em->subArc;
    n = (((MotionData*) PL_ARC_PTR(arc, 0x253))->maxFrame & 0x3FFF) / 4;
    if (w->x6C5 != 1) {
        m5 = PL_ARC_PTR(arc, 0x283);
        type = 2;
        heads = 1;
        bin = PL_ARC_PTR(arc, 0x279);
        tpl = PL_ARC_PTR(arc, 0x27A);
        m0 = PL_ARC_PTR(arc, 0x27B);
        m1 = PL_ARC_PTR(arc, 0x27C);
        m2 = PL_ARC_PTR(arc, 0x280);
        m3 = PL_ARC_PTR(arc, 0x282);
        m7 = PL_ARC_PTR(arc, 0x27D);
        m8 = PL_ARC_PTR(arc, 0x281);
        m9 = PL_ARC_PTR(arc, 0x27E);
        m10 = PL_ARC_PTR(arc, 0x27F);
        m4 = m5;
        m6 = m5;
    } else {
        m1 = PL_ARC_PTR(arc, 0x286);
        m8 = PL_ARC_PTR(arc, 0x289);
        m10 = PL_ARC_PTR(arc, 0x28B);
        type = 3;
        heads = 0;
        bin = PL_ARC_PTR(arc, 0x284);
        tpl = PL_ARC_PTR(arc, 0x285);
        m2 = PL_ARC_PTR(arc, 0x287);
        m3 = PL_ARC_PTR(arc, 0x288);
        m4 = PL_ARC_PTR(arc, 0x28C);
        m5 = PL_ARC_PTR(arc, 0x28A);
        m6 = PL_ARC_PTR(arc, 0x28D);
        m0 = m1;
        m7 = m8;
        m9 = m10;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    w->pParasite = (cObj16*) SetObj16(bin, tpl, em, em, 3, type, &pos, &rot);
    if (w->pParasite) {
        w->pParasite->setDieEff();
        w->pParasite->setMotData(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
        EstSet((int) w->pParasite, -1, 0, 0, 0x10, 0xE, 0, w->x6A0, (u32) w->pParasite, 0);
        w->pParasite->setPlDmgMot(PL_ARC_PTR(em->subArc, 0x17A), (int) PL_ARC_PTR(em->subArc, 0x17B));
    }
    if (heads) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = -0.6632251f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        o = SetObj16(PL_ARC_PTR(em->subArc, 0x251), PL_ARC_PTR(em->subArc, 0x252), em, w->pParasite, 0x16, 1, &pos, &rot);
        if (o) {
            MotSetObj16(o, PL_ARC_PTR(em->subArc, 0x253), 4, 0);
            w->x578[0] = (cEm*) o;
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.61086524f;
        o = SetObj16(PL_ARC_PTR(em->subArc, 0x251), PL_ARC_PTR(em->subArc, 0x252), em, w->pParasite, 0x17, 1, &pos, &rot);
        if (o) {
            MotSetObj16(o, PL_ARC_PTR(em->subArc, 0x253), 4, n);
            w->x578[1] = (cEm*) o;
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = -0.5235988f;
        o = SetObj16(PL_ARC_PTR(em->subArc, 0x251), PL_ARC_PTR(em->subArc, 0x252), em, w->pParasite, 0x18, 1, &pos, &rot);
        if (o) {
            MotSetObj16(o, PL_ARC_PTR(em->subArc, 0x253), 4, n * 2);
            w->x578[2] = (cEm*) o;
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        o = SetObj16(PL_ARC_PTR(em->subArc, 0x251), PL_ARC_PTR(em->subArc, 0x252), em, w->pParasite, 0x19, 1, &pos, &rot);
        if (o) {
            MotSetObj16(o, PL_ARC_PTR(em->subArc, 0x253), 4, n * 3);
            w->x578[3] = (cEm*) o;
        }
    }
    SndCall(8, 0x8A, &em->pos, em->id, 0, em);
    w->x67C = 0x2D;
}
void em10SetWaitMotion(cEm10* em, int a)
{
    Em10Work* w = EM10_WK(em);
    int flag = (em->flags_3C8 & 0x01000000) ? 0x45 : 5;
    void* mot = PL_ARC_PTR(em->subArc, 5);
    u32 n;
    int r;

    if (w->pWep) {
        if (w->wepType == 1) {
            mot = PL_ARC_PTR(em->subArc, 0x14A);
        }
        if (w->wepType == 4) {
            mot = PL_ARC_PTR(em->subArc, 0xE6);
        }
        if (w->wepType == 11) {
            mot = PL_ARC_PTR(em->subArc, 0xD7);
        }
        if (w->wepType == 7) {
            mot = PL_ARC_PTR(em->subArc, 0xD7);
        }
        if (w->wepType == 9) {
            mot = PL_ARC_PTR(em->subArc, 0xD7);
        }
        if (w->wepType == 6) {
            mot = PL_ARC_PTR(em->subArc, 0x13A);
        }
        if (w->wepType == 12) {
            mot = PL_ARC_PTR(em->subArc, 0x17C);
        }
    }
    if (w->pShield) {
        mot = PL_ARC_PTR(em->subArc, 0x165);
    }
    if (em->type == 6) {
        mot = PL_ARC_PTR(em->subArc, 0xA4);
    }
    if (em->type == 10) {
        mot = PL_ARC_PTR(em->subArc, 0x108);
    }
    if (em->type == 13) {
        mot = PL_ARC_PTR(em->subArc, 0x108);
    }
    if (em->type == 2) {
        mot = PL_ARC_PTR(em->subArc, 0x18F);
    }
    n = ((MotionData*) mot)->maxFrame & 0x3FFF;
    r = Rnd();
    MotionSetCore(em, MOTION(em), mot, 0, (u8) a, flag, (u16) (r % n));
}

// Walk motion by weapon kind and set-number variant (5 walk styles, water / event overrides).
void em10SetWalkMotion(cEm10* em, int a)
{
    Em10Work* w = EM10_WK(em);
    u32 kind;
    int flag;
    u32 v;
    MotionData* m0;
    void* m1;

    kind = (w->x6C5 == 1) ? 8 : 0;
    if (em->type == 6) {
        kind = 7;
    }
    if (w->pWep) {
        if ((em->x3E0 & 0x20000100) == 0x100) {
            kind = 3;
        }
        if (w->wepType == 1) {
            kind = 4;
        }
        if (w->wepType == 4) {
            kind = 5;
        }
        if (w->wepType == 8 || w->wepType == 0xC) {
            kind = 6;
        }
        if (w->wepType == 6) {
            kind = 9;
        }
        if (w->wepType == 0xC) {
            kind = 0xC;
        }
    }
    if (w->pShield) {
        kind = 0xA;
    }
    if (em->type == 0xA) {
        kind = 0xB;
    }
    if (em->type == 0xD) {
        kind = 0xB;
    }
    if (em->type == 2) {
        kind = 0xD;
    }
    flag = 5;
    if (em->flags_3C8 & 0x01000000) {
        flag = 0x45;
    }
    v = em->emsetNo % 5;
    if (CheckInWater(em, 0)) {
        v = 1;
    }
    if ((s32) pG->flags_54 < 0 || (pG->flags_54 & 0x40000000)) {
        if (v == 2) {
            v = 1;
        }
        if (v == 4) {
            v = 3;
        }
    }
#define EM10_WALK_MOT(base)                                                                        \
    m0 = (MotionData*) PL_ARC_PTR(em->subArc, base);                                               \
    switch (v) {                                                                                   \
    case 0:                                                                                        \
    default:                                                                                       \
        m1 = PL_ARC_PTR(em->subArc, base + 1);                                                     \
        break;                                                                                     \
    case 1:                                                                                        \
        m1 = PL_ARC_PTR(em->subArc, base + 2);                                                     \
        break;                                                                                     \
    case 2:                                                                                        \
        m1 = PL_ARC_PTR(em->subArc, base + 3);                                                     \
        break;                                                                                     \
    case 3:                                                                                        \
        m1 = PL_ARC_PTR(em->subArc, base + 4);                                                     \
        break;                                                                                     \
    case 4:                                                                                        \
        m1 = PL_ARC_PTR(em->subArc, base + 5);                                                     \
        break;                                                                                     \
    }
    switch (kind) {
    case 0:
    case 1:
    case 2:
    default:
        switch (w->x69A) {
        case 0:
        default:
            EM10_WALK_MOT(8);
            break;
        case 1:
            EM10_WALK_MOT(0xE);
            break;
        }
        break;
    case 3:
        EM10_WALK_MOT(0xD8);
        break;
    case 4:
        EM10_WALK_MOT(0x14B);
        break;
    case 5:
        if (w->flags & 0x100) {
            m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0xEF);
            m1 = PL_ARC_PTR(em->subArc, 0xF0);
        } else {
            m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0xED);
            m1 = PL_ARC_PTR(em->subArc, 0xEE);
        }
        break;
    case 6:
        EM10_WALK_MOT(0x126);
        if (!(w->flags & 0x100)) {
            m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x124);
            m1 = PL_ARC_PTR(em->subArc, 0x125);
        }
        break;
    case 7:
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0xA8);
        m1 = PL_ARC_PTR(em->subArc, 0xAB);
        break;
    case 8:
        EM10_WALK_MOT(0xCE);
        break;
    case 9:
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x13B);
        m1 = PL_ARC_PTR(em->subArc, 0x13C);
        break;
    case 0xA:
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x166);
        m1 = PL_ARC_PTR(em->subArc, 0x167);
        break;
    case 0xB:
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x109);
        m1 = PL_ARC_PTR(em->subArc, 0x10A);
        break;
    case 0xC:
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x17D);
        m1 = PL_ARC_PTR(em->subArc, 0x17E);
        break;
    case 0xD:
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x190);
        m1 = PL_ARC_PTR(em->subArc, 0x191);
        break;
    }
#undef EM10_WALK_MOT
    {
        u16 fr = (u32) ((f32) (m0->maxFrame & 0x3FFF) * (f32) em->xFF / 256.0f);
        MotionSetCore(em, MOTION(em), m0, (int) m1, (u8) a, flag, fr);
    }
}

void em10SetDashMotion(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int flag = 5;
    u32 v;
    MotionData* m0;
    void* m1;

    if (em->flags_3C8 & 0x01000000) {
        flag = 0x45;
    }
    v = em->emsetNo % 5;
    if (CheckInWater(em, 0)) {
        v = 1;
    }
    if ((s32) pG->flags_54 < 0 || (pG->flags_54 & 0x40000000)) {
        if (v == 2) {
            v = 1;
        }
        if (v == 4) {
            v = 3;
        }
    }
    m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0xAE);
    switch (v) {
    default:
        m1 = PL_ARC_PTR(em->subArc, 0xAF);
        break;
    case 1:
        m1 = PL_ARC_PTR(em->subArc, 0xB0);
        break;
    case 2:
        m1 = PL_ARC_PTR(em->subArc, 0xB1);
        break;
    case 3:
        m1 = PL_ARC_PTR(em->subArc, 0xB2);
        break;
    case 4:
        m1 = PL_ARC_PTR(em->subArc, 0xB3);
        break;
    }
    if (w->pWep && w->wepType == 4) {
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0xF1);
        switch (v) {
        default:
            m1 = PL_ARC_PTR(em->subArc, 0xF2);
            break;
        case 1:
            m1 = PL_ARC_PTR(em->subArc, 0xF3);
            break;
        case 2:
            m1 = PL_ARC_PTR(em->subArc, 0xF4);
            break;
        case 3:
            m1 = PL_ARC_PTR(em->subArc, 0xF5);
            break;
        case 4:
            m1 = PL_ARC_PTR(em->subArc, 0xF6);
            break;
        }
    }
    if (w->pWep && w->wepType == 1) {
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x151);
        switch (v) {
        default:
            m1 = PL_ARC_PTR(em->subArc, 0x152);
            break;
        case 1:
            m1 = PL_ARC_PTR(em->subArc, 0x153);
            break;
        case 2:
            m1 = PL_ARC_PTR(em->subArc, 0x154);
            break;
        case 3:
            m1 = PL_ARC_PTR(em->subArc, 0x155);
            break;
        case 4:
            m1 = PL_ARC_PTR(em->subArc, 0x156);
            break;
        }
    }
    if (w->pWep && w->wepType == 6) {
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x140);
        m1 = PL_ARC_PTR(em->subArc, 0x141);
    }
    if (w->pWep && (w->wepType == 7 || w->wepType == 0xB || w->wepType == 9)) {
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0xDE);
        switch (v) {
        default:
            m1 = PL_ARC_PTR(em->subArc, 0xDF);
            break;
        case 1:
            m1 = PL_ARC_PTR(em->subArc, 0xE0);
            break;
        case 2:
            m1 = PL_ARC_PTR(em->subArc, 0xE1);
            break;
        case 3:
            m1 = PL_ARC_PTR(em->subArc, 0xE2);
            break;
        case 4:
            m1 = PL_ARC_PTR(em->subArc, 0xE3);
            break;
        }
    }
    if (em->type == 6) {
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0xA9);
        m1 = PL_ARC_PTR(em->subArc, 0xAC);
    }
    if (w->pShield) {
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x168);
        m1 = PL_ARC_PTR(em->subArc, 0x169);
    }
    if (em->type == 0xA || em->type == 0xD) {
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x10D);
        m1 = PL_ARC_PTR(em->subArc, 0x10E);
    }
    if (w->pWep && w->wepType == 0xC) {
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x17F);
        m1 = PL_ARC_PTR(em->subArc, 0x180);
    }
    if (em->type == 2) {
        m0 = (MotionData*) PL_ARC_PTR(em->subArc, 0x192);
        m1 = PL_ARC_PTR(em->subArc, 0x193);
    }
    {
        u16 fr = (u32) ((f32) (m0->maxFrame & 0x3FFF) * (f32) em->xFF / 256.0f);
        MotionSetCore(em, MOTION(em), m0, (int) m1, 5, flag, fr);
    }
}

extern "C" int em10DashCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;
    u32 cnt;
    u32 lim;
    f32 d;
    f32 ang;

    if (w->wepType == 9 && w->x640 != 0) {
        EmRoutineSet(em, 1, 0x11, 0, 0);
        return 1;
    }
    if (em->type == 0xA || em->type == 0xD) {
        return 0;
    }
    if (w->x58C != 0) {
        return 0;
    }
    if (w->pParasite != 0) {
        return 0;
    }
    if ((em->flags_3C8 & 0x400) && w->x5EC == 0) {
        return 0;
    }
    if (em->flags_3C8 & 0x40) {
        return 0;
    }
    if (pG->x4F88 <= 1) {
        return 0;
    }
    if (em->flags_3C8 & 0x80) {
        w->x6AC = Rnd() % 3;
        EmRoutineSet(em, 1, 0x11, 0, 0);
        return 1;
    }
    if (em->x3D0 == 3) {
        if (EM_RTN(em, 1, 0x1B)) {
            return 1;
        }
        EmRoutineSet(em, 1, 0x1B, 0, 0);
        return 1;
    }
    if (w->flags & 0x20000000) {
        w->x6AC = Rnd() % 3;
        EmRoutineSet(em, 1, 0x11, 0, 0);
        return 1;
    }
    d = 12250000.0f;
    if (pG->x4F88 > 6) {
        d = 4000000.0f;
    }
    if (em->plDist2 < d) {
        return 0;
    }
    if (w->x674 != 0) {
        return 0;
    }
    if (!(w->flags & 1)) {
        return 0;
    }
    ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f));
    if (pG->x4F88 <= 6 && ang > 0.3926991f) {
        return 0;
    }
    cnt = 0;
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* o = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((o->be_flag & 0x201) != 1) {
            continue;
        }
        if (o->id <= 0xF) {
            continue;
        }
        if (o->id > 0x20) {
            continue;
        }
        if (o->hp <= 0) {
            continue;
        }
        if (o == em) {
            continue;
        }
        if (!o->checkStatus(5)) {
            continue;
        }
        if (o->plDist2 < 12250000.0f) {
            cnt++;
        }
    }
    lim = 0;
    if (pG->x4F88 > 3) {
        lim = 1;
    }
    if (w->x6C5 != 0) {
        lim = 2;
    }
    if (pG->x4F88 > 6) {
        lim = 3;
    }
    if (pG->x4F88 > 9) {
        lim = 6;
    }
    if (cnt > lim) {
        return 0;
    }
    {
        int zero = 0; // shared zero pseudo: lands in r7 ahead of the 1 / 0x11 constants
        w->x6AC = Rnd() % 3;
        EmRoutineSet(em, 1, 0x11, zero, zero);
    }
    return 1;
}

extern "C" int em10StayCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;
    u32 n;

    if (em10GotoCk(em)) {
        return 1;
    }
    if (em->flags_3C8 & 0x40) {
        return 0;
    }
    if (w->flags & 0x08000000) {
        return 0;
    }
    if (em->type == 0x16) {
        return 0;
    }
    if (em->flags_3C8 & 0x80) {
        w->x6AC = Rnd() % 3;
        EmRoutineSet(em, 1, 0x11, 0, 0);
        return 1;
    }
    if (w->wepType == 9 && w->x640 != 0) {
        EmRoutineSet(em, 1, 0x11, 0, 0);
        return 1;
    }
    if (((G_ROOM_ID32 & 0xFFFF0000) == 0x01010000 || (G_ROOM_ID32 & 0xFFFF0000) == 0x01110000 ||
         (G_ROOM_ID32 & 0xFFFF0000) == 0x04000000) &&
        pPL->pos.y > 6000.0f && em->plDist2 < 144000000.0f) {
        if (em->plDist2 < 36000000.0f) {
            Vec tbl[4] = {
                { 13913.0f, 215.0f, 322.0f },
                { 10172.0f, 215.0f, 1311.0f },
                { 14208.0f, 215.0f, -11076.0f },
                { 23407.0f, 1215.0f, -1110.0f },
            };
            em->setGoto(&tbl[Rnd() & 3], 12);
            return 0;
        }
        EmRoutineSet(em, 1, 0x1B, 0, 0);
        return 1;
    }
    if (em->x3D0 == 3) {
        if (EM_RTN(em, 1, 0x1B)) {
            return 1;
        }
        EmRoutineSet(em, 1, 0x1B, 0, 0);
        return 1;
    }
    if (em10ReturnCk(em)) {
        return 1;
    }
    if (em->x3D0 == 0 && w->x6B8 == 0 && w->x52C > em->x3CC + 2000.0f) {
        EmRoutineSet(em, 1, 0x1B, 0, 0);
        return 1;
    }
    if (w->flags & 0x08000000) {
        if (em10GoSubStayCk(em)) {
            return 1;
        }
        return 0;
    }
    if (em->type == 0xA || em->type == 0xD) {
        return 0;
    }
    n = 0;
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) == 1 && e->id > 0xF && e->id <= 0x20 && e->hp > 0 && e != em &&
            e->checkStatus(5) && EM10_WK(e)->x524 < w->x524) {
            n++;
        }
    }
    if (pG->x4F88 <= 1 && n == 0) {
        return 0;
    }
    if (pG->x4F88 <= 3 && n <= 1) {
        return 0;
    }
    if (n <= 3) {
        return 0;
    }
    if (em->x3D0 == 2) {
        if (pG->x4F88 <= 3) {
            if (w->x524 > 6000.0f) {
                return 0;
            }
        } else {
            if (w->x524 > 4000.0f) {
                return 0;
            }
        }
    }
    if (n <= 7) {
        if (w->x524 > 8000.0f) {
            return 0;
        }
    }
    if (w->x524 > 12000.0f) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x1B, 0, 0);
    return 1;
}

extern "C" int em10GoSubStayCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 cnt;
    u32 i;
    f32 d;

    if (!(w->flags & 0x08000000)) {
        return 0;
    }
    cnt = 0;
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        Em10Work* ew;
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        ew = EM10_WK(e);
        if (!(ew->flags & 0x08000000)) {
            continue;
        }
        if (ew->x528 < w->x528) {
            cnt++;
        }
    }
    if (pG->x4F88 <= 3) {
        if (cnt <= 1) {
            return 0;
        }
    } else if (cnt <= 3) {
        return 0;
    }
    d = w->x528;
    if (em->x3D0 == 2) {
        if (pG->x4F88 <= 3) {
            if (d > 6000.0f) {
                return 0;
            }
        } else {
            if (d > 4000.0f) {
                return 0;
            }
        }
    }
    if (cnt <= 7) {
        if (d > 8000.0f) {
            return 0;
        }
    }
    if (d > 12000.0f) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x1B, 0, 0);
    return 1;
}

void em10ChainSawMove(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->wepType == 4 && w->pWep) {
        cModel* p = w->pWep->getPartsPtr(1);
        p->pos.z = (pG->flags_51E4 & 1) ? 0.0f : 10.0f;
    }
}
#undef EM10_ROUTE_LOCKON
#undef EM10_ROUTE_SIGHT_CK

// Attack hit check for attack `no` between `a` and `b` (Em10AtkTbl[no] gives range / damage).
// Returns 1 when the player or the partner was hit; `parts` is the model part used for the 0xD
// (chainsaw) hit effect direction.
int em10AtkCk(cEm10* em, Vec* a, Vec* b, int no, int parts)
{
    Em10Work* w = EM10_WK(em);
    EmAtkInfo info;
    Vec pos;
    Vec rot;
    Vec d;
    cModel* part;
    int hit;
    f32 pw;
    f32 len;

    em10BellAtkCk(em, a, no);
    if (w->x697) {
        return 0;
    }
    pw = em10GetPower(em);
    info = Em10AtkTbl[no];
    if (no != 0xD) {
        info.dmg = (f32) info.dmg * pw;
    } else {
        switch (w->x20) {
        case 0:
            info.dmg = 0x280;
            break;
        case 1:
            info.dmg = 0x140;
            break;
        default:
            info.dmg = 0xA0;
            break;
        }
    }
    if ((em->flags_3C8 & 4) && !(pG->flags_54 & 0x20)) {
        info.dmg = info.dmg / 2 + 1;
    }
    if ((pG->flags_5010 & 0x2000) && pG->x4F88 > 3) {
        info.dmg = 9999;
        info.x0A = 4;
    }
    hit = EmAtkHitCk(&info, a, b, 0);
    if (hit) {
    if (pG->x4F88 <= 3) {
        w->x67C = 0x3C;
        Ctrl12Set(w->pCtrl12, 6, 0x3C);
        Ctrl12Set(w->pCtrl12, 8, 0x78);
    } else if (pG->x4F88 > 6) {
        w->x67C = 0x1E;
        Ctrl12Set(w->pCtrl12, 6, 0x1E);
        Ctrl12Set(w->pCtrl12, 8, 0x5A);
    } else {
        w->x67C = 0x2D;
        Ctrl12Set(w->pCtrl12, 6, 0x2D);
        Ctrl12Set(w->pCtrl12, 8, 0x78);
    }
    if (hit & 1) {
        switch ((u32) no) {
        case 0:
        default:
            EmPlBloodSet2(em, a, 1, 0x10, 0xB);
            break;
        case 9:
            EmPlBloodSet2(em, a, 1, 0x10, 0x39);
            if (w->pWep && w->wepType == 6) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x6A, 0, 0, (u32) w->pWep, 0);
            }
            break;
        case 0xA:
            EmPlBloodSet2(em, a, 1, 0x10, 0x6B);
            if (w->pWep && w->wepType == 6) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x6A, 0, 0, (u32) w->pWep, 0);
            }
            break;
        case 8:
            EmPlBloodSet2(em, a, 1, 0x10, 7);
            break;
        case 1:
            EmPlBloodSet2(em, a, 1, 0x10, 0x54);
            break;
        case 4:
            EmPlBloodSet2(em, a, 1, 0x10, 0x5A);
            break;
        case 2:
            if (em->motFlags & 0x40) {
                pPL->x328 = em->pos;
                EmPlBloodSet2(em, a, 1, 0x10, 0x66);
            } else {
                pPL->x328 = em->pos;
                EmPlBloodSet2(em, a, 1, 0x10, 0x67);
            }
            EstSet((int) pPL, -1, 0, 0, 0x10, 0x68, 0, 0, (u32) pPL, 0);
            if (w->pWep && w->wepType == 0xB) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x69, 0, 0, (u32) w->pWep, 0);
            }
            break;
        case 0xD:
            if (EmGetDmPos(pPL, &pos, &rot)) {
                part = em->getPartsPtr(parts);
                PSVECSubtract(&part->worldPos, &part->x88, &d);
                len = SQRTF(d.x * d.x + d.z * d.z);
                rot.x = -atan2f(d.y, len);
                rot.y = atan2f(d.x, d.z);
                rot.z = 0.0f;
            }
            EstSet(0, -1, &pos, &rot, 0x10, 0x81, 0, 0, 0, 0);
            rot.x = 0.0f;
            rot.z = 0.0f;
            EstSet(0, -1, &pos, &rot, 0x10, 0x82, 0, 0, 0, 0);
            EstSet((int) pPL, -1, 0, 0, 0x10, 0x68, 0, 0, (u32) pPL, 0);
            EstSet((int) em, -1, 0, 0, 0x10, 0x79, 0, 0, (u32) em, 0);
            break;
        case 3:
            if (w->pWep) {
                EstSet((int) w->pWep, -1, 0, 0, 0xCD, 1, 0, 0, (u32) w->pWep, 0);
            }
            w->x67C = 0x5A;
            Ctrl12Set(w->pCtrl12, 6, 0x5A);
            Ctrl12Set(w->pCtrl12, 8, 0x78);
            break;
        case 0xC:
            EmPlBloodSet2(em, a, 1, 0x10, 0x97);
            if (w->pWep) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x9A, 0, 0, (u32) w->pWep, 0);
            }
            break;
        case 0x12:
            EmPlBloodSet2(em, a, 1, 0x10, 0x99);
            break;
        }
        switch ((u32) no) {
        case 2:
            if ((s16) pG->pl_life > 0) {
                SetPlDamage((int) em, plemDmMStar);
                if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f)) < 1.5707964f) {
                    FSet(pPL->rot.y, pPL->rot.y + Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f));
                    pPL->xFF = 0;
                } else {
                    FSet(pPL->rot.y, pPL->rot.y + Muku(&em->pos, &pPL->pos, pPL->rot.y, 3.1415927f));
                    pPL->xFF = 1;
                }
                if (em->flags_3C8 & 0x01000000) {
                    if (pPL->xFF) {
                        pPL->xFF = 0;
                    } else {
                        pPL->xFF = 1;
                    }
                }
            }
            break;
        case 3:
            SetPlDamage((int) em, plemDmStun);
            break;
        case 4:
            pPL->rot.y += Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f);
            PlSetDamage(8, 0, 0);
            break;
        case 0xD:
            if ((s16) pG->pl_life <= 0) {
                em10PlHeadLost();
            } else {
                SetPlDamage((int) em, plemDmMStar);
                pPL->xFF = 1;
                if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f)) < 1.5707964f) {
                    FSet(pPL->rot.y, pPL->rot.y + Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f));
                    pPL->xFF = 0;
                } else {
                    FSet(pPL->rot.y, pPL->rot.y + Muku(&em->pos, &pPL->pos, pPL->rot.y, 3.1415927f));
                    pPL->xFF = 1;
                }
                if (em->flags_3C8 & 0x01000000) {
                    if (pPL->xFF) {
                        pPL->xFF = 0;
                    } else {
                        pPL->xFF = 1;
                    }
                }
            }
            break;
        case 0xE:
            pPL->rot.y += Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f);
            PlSetDamage(8, 0, 0);
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xD, 1);
            if ((s16) pG->pl_life > 0) {
                EstSet((int) em, -1, 0, 0, 0x10, 0x79, 0, 0, (u32) em, 0);
            }
            break;
        case 0x10:
            pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
            PlSetDamage(8, 0, 0);
            break;
        case 9:
        case 0xC:
            if ((s16) pG->pl_life <= 0) {
                pG->pl_life = 0;
                em10PlHeadLost();
            }
            break;
        case 0x12:
            pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
            PlSetDamage(8, 0, 0);
            break;
        }
        w->x697 = 1;
        Ctrl12Set(w->pCtrl12, 9, 0x1E);
        if ((s16) pG->pl_life <= 0) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xB, 1);
        } else {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
        QuakeExec(0, 0, 5, 22.0f, 2);
        if (w->wepType == 7) {
            SndCall(8, 0x46, &em->pos, em->id, 0, em);
        } else {
            switch ((u32) no) {
            case 0xE:
                SndCall(6, 0x6E, &em->getPartsPtr(0)->worldPos, 0, 0, em);
                break;
            case 3:
                SndCall(8, 0x46, &pPL->getPartsPtr(0)->worldPos, em->id, 0, pPL);
                break;
            default:
                SndCall(8, 0x3E, &em->pos, em->id, 0, em);
                break;
            case 4:
                if (w->x6C5 == 1) {
                    SndCall(8, 0x3E, &em->pos, em->id, 0, em);
                } else {
                    SndCall(8, 0xB0, &em->pos, em->id, 0, em);
                }
                break;
            }
        }
    }
    if ((hit & 2) && pSUB) {
        switch ((u32) no) {
        case 0:
        default:
            EmSubBloodSet(em, a, 1, 0x10, 0xB);
            break;
        case 9:
            EmSubBloodSet(em, a, 1, 0x10, 0x39);
            if (w->pWep && w->wepType == 6) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x6A, 0, 0, (u32) w->pWep, 0);
            }
            break;
        case 0xA:
            EmSubBloodSet(em, a, 1, 0x10, 0x6B);
            if (w->pWep && w->wepType == 6) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x6A, 0, 0, (u32) w->pWep, 0);
            }
            break;
        case 8:
            EmSubBloodSet(em, a, 1, 0x10, 7);
            break;
        case 1:
            EmSubBloodSet(em, a, 1, 0x10, 0x54);
            break;
        case 4:
            EmSubBloodSet(em, a, 1, 0x10, 0x5A);
            break;
        case 2:
            if (em->motFlags & 0x40) {
                pSUB->x328 = em->pos;
                EmSubBloodSet(em, a, 1, 0x10, 0x66);
            } else {
                pSUB->x328 = em->pos;
                EmSubBloodSet(em, a, 1, 0x10, 0x67);
            }
            EstSet((int) pSUB, -1, 0, 0, 0x10, 0x68, 0, 0, (u32) pSUB, 0);
            if (w->pWep && w->wepType == 0xB) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x69, 0, 0, (u32) w->pWep, 0);
            }
            break;
        case 0xD:
            if (EmGetDmPos(pSUB, &pos, &rot)) {
                part = em->getPartsPtr(parts);
                PSVECSubtract(&part->worldPos, &part->x88, &d);
                len = SQRTF(d.x * d.x + d.z * d.z);
                rot.x = -atan2f(d.y, len);
                rot.y = atan2f(d.x, d.z);
                rot.z = 0.0f;
            }
            EstSet(0, -1, &pos, &rot, 0x10, 0x81, 0, 0, 0, 0);
            rot.x = 0.0f;
            rot.z = 0.0f;
            EstSet(0, -1, &pos, &rot, 0x10, 0x82, 0, 0, 0, 0);
            EstSet((int) pSUB, -1, 0, 0, 0x10, 0x68, 0, 0, (u32) pSUB, 0);
            EstSet((int) em, -1, 0, 0, 0x10, 0x79, 0, 0, (u32) em, 0);
            break;
        case 3:
            if (w->pWep) {
                EstSet((int) w->pWep, -1, 0, 0, 0xCD, 1, 0, 0, (u32) w->pWep, 0);
            }
            w->x67C = 0x5A;
            Ctrl12Set(w->pCtrl12, 6, 0x5A);
            Ctrl12Set(w->pCtrl12, 8, 0x78);
            break;
        case 0xC:
            EmSubBloodSet(em, a, 1, 0x10, 0x97);
            if (w->pWep) {
                EstSet((int) w->pWep, -1, 0, 0, 0x10, 0x9A, 0, 0, (u32) w->pWep, 0);
            }
            break;
        case 0x12:
            EmSubBloodSet(em, a, 1, 0x10, 0x99);
            break;
        }
        w->x697 = 1;
        Ctrl12Set(w->pCtrl12, 9, 0x1E);
        if (w->wepType == 7) {
            SndCall(8, 0x46, &em->pos, em->id, 0, em);
        } else {
            switch (no) {
            default:
                SndCall(8, 0x3E, &em->pos, em->id, 0, em);
                break;
            case 0xE:
                SndCall(6, 0x6E, &em->getPartsPtr(0)->worldPos, 0, 0, em);
                break;
            case 3:
                SndCall(8, 0x46, &pSUB->getPartsPtr(0)->worldPos, em->id, 0, pPL);
                break;
            }
        }
    }
    return 1;
    }
    return 0;
}

extern "C" int em10GatlingHitCk(cEm10* em)
{
    Vec a;
    Vec b;
    EmAtkInfo info;
    Vec hit;
    Vec nrm;
    Vec dir;
    Vec rot;
    Vec s;
    u32 attr;
    cEm* e;
    cModel* p;
    f32 l;

    EstSet((int) em, -1, 0, 0, 0xCC, 0, 0, 0, (u32) em, 0);
    SndCall(6, 9, &em->pos, 0, 0, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = -80.0f;
    b.x = -50000.0f;
    b.y = fRand1_1() * 500.0f;
    b.z = fRand1_1() * 500.0f;
    p = em->getPartsPtr(10);
    PSMTXMultVec(p->mat, &a, &a);
    PSMTXMultVec(p->mat, &b, &b);
    em->dmType = 1;
    PlWepHitCheck2(0, &a, &b, 0xC, 3, 6000.0f);
    em->dmType = 0;
    e = EmAtkLineHitCk(&a, &b, &hit, &nrm, &attr);
    const f32 k = 30.0f; // pool order: the 30 of PSVECScale before the 22 of QuakeExec
    if (e != 0) {
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        SndCall(8, 0x81, &pPL->getPartsPtr(0)->worldPos, em->id, 0, pPL);
        QuakeExec(0, 0, 5, 22.0f, 2);
        EmPlBloodSet2(em, &a, 1, 0xCC, 2);
        info = Em10AtkTbl[15];
        EmAtkSetDamagePL(e, &info, &a, &b);
        return 1;
    }
    l = SQRTF(nrm.x * nrm.x + nrm.z * nrm.z);
    rot.x = -atan2f(nrm.y, l);
    rot.y = atan2f(nrm.x, nrm.z);
    rot.z = 0.0f;
    PSVECScale(&nrm, &dir, 30.0f);
    PSVECAdd(&hit, &dir, &hit);
    EstSet(0, -1, &hit, &rot, 0xCC, 1, 0, 0, 0, 0);
    PSVECSubtract(&hit, &a, &s);
    EspSetGatling(a, s);
    SndCall(6, 0xA, &hit, 0, 0, 0);
    return 0;
}

extern "C" void em10BellAtkCk(cEm10* em, Vec* pos, u32 no)
{
    EmAtkInfo info = Em10AtkTbl[no];
    cObjBell* o;
    Vec v;
    cModel* p;

    switch (no) {
    case 0xD:
    case 0xE:
        for (o = (cObjBell*) ObjMgr.pAlive; o; o = (cObjBell*) o->next) {
            if (o->id != 0x14) {
                continue;
            }
            if (!o->ckBreakEnable()) {
                continue;
            }
            p = o->getPartsPtr(1);
            v.x = 0.0f;
            v.y = -650.0f;
            v.z = 0.0f;
            PSMTXMultVec(p->mat, &v, &v);
            {
                f32 dx = v.x - pos->x;
                f32 dy = v.y - pos->y;
                f32 dz = v.z - pos->z;
                if (dx * dx + dy * dy + dz * dz < (info.range + 300.0f) * (info.range + 300.0f)) {
                    o->setBreak();
                    SndCall(6, 0xF, &em->pos, 0, 0, em);
                }
            }
        }
        break;
    }
}

extern "C" int em10TorchFrameAtkCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx m;
    Vec v;

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (em10DeadCk(pPL)) {
        return 0;
    }
    if (w->x697) {
        return 0;
    }
    PSMTXInverse(em->mat, m);
    PSMTXMultVec(m, &pPL->pos, &v);
    if (!(v.x > 500.0f) && !(v.x < -500.0f) && !(v.z > 3500.0f) && !(v.z < 0.0f) && !(v.y > 1500.0f) && !(v.y < -500.0f)) {
        w->x697 = 1;
        SndCall(8, 0x8F, &pPL->pos, em->id, 0, pPL);
        Ctrl12Set(w->pCtrl12, 9, 30);
        SetPlDamage((int) em, plemDmFrame);
        pPL->dmg.set(0, 30);
        return 1;
    }
    return 0;
}

extern "C" int em10TorchFrameAtkCkSub(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx inv;
    Vec v;
    EmHitInfo* hit;
    u8 old;

    if (!pSUB) {
        return 0;
    }
    if (pSUB->hp <= 0) {
        return 0;
    }
    if (em10DeadCk(pSUB)) {
        return 0;
    }
    old = w->x698;
    if (old) {
        return 0;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pSUB->pos, &v);
    if (v.x > 500.0f || v.x < -500.0f) {
        return 0;
    }
    if (v.z > 3500.0f || v.z < 0.0f) {
        return 0;
    }
    if (v.y > 1500.0f || v.y < -500.0f) {
        return 0;
    }
    w->x698 = 1;
    SndCall(8, 0x8F, &pSUB->pos, em->id, 0, pSUB);
    Ctrl12Set(w->pCtrl12, 9, 0x1E);
    EstSet((int) pSUB, -1, 0, 0, 0x10, 0x28, 0, 0, (u32) pSUB, (void*) old);
    v = pSUB->pos;
    cDmgInfo* dmg = &pSUB->dmg; // held across the call (r31)
    v.y += 1300.0f;
    hit = EmAtkHitSubCk2(&Em10AtkTbl[17], &v, &em->pos);
    if (hit) {
        dmg->set(0, 0xA, 0x18, &em->pos, hit->rad, hit);
    }
    return 1;
}

void em10DragonFireCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;

    if (!w->pDragon) {
        return;
    }
    if (!EM10_DRAGON(w)->ckHitFireBlocked()) {
        return;
    }
    if ((s16) pG->pl_life > 0) {
        int dead = em10DeadCk(pPL);
        if (!dead) {
            if (EM10_DRAGON(w)->ckHitFire(&pPL->pos)) {
                SndCall(8, 0x8F, &pPL->pos, em->id, 0, pPL);
                Ctrl12Set(w->pCtrl12, 9, 0x1E);
                pPL->rot.y += Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f);
                SetPlDamage((int) em, plemDmFrame);
                pPL->dmg.set(0, 0x1E);
            }
        }
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm10* e = (cEm10*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        int dead;
        cDmgInfo* d;
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        d = &e->dmg;
        dead = em10DmgDeadCk(d);
        if (dead) {
            continue;
        }
        if (e == em) {
            continue;
        }
        {
            Em10Work* ew = EM10_WK(e);
            if (!EM10_DRAGON(w)->ckHitFire(&e->pos)) {
                continue;
            }
            d->set(0, 0x1E);
            ew->x68C = 0x78;
            e->xFC = 2;
            e->xFD = 0xB;
            e->xFE = dead;
            e->xFF = dead;
        }
    }
}

static void plemDmFrame(cPlayer* pl)
{
    cEm* em = (cEm*) pl->dmgType;
    int end;
    int dmg;

    pl->subArc = em->subArc;
    pl->dmg.set(0, 0x1E);
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x29C), 0, 3, 1, 0);
        PlSetDamageSe(0);
        EstSetEm(pl, -1, 0, 0, 0x10, 0x28, 0, 0, pl, 0);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xB, 1);
        pl->x3E0 = 50;
        pl->xFE++;
    case 1:
        end = MotionMoveF(pl, 0);
        if (pl->x3E0) {
            dmg = 10;
            dmg = (int) (em10GetPower((cEm10*) pl) * (f32) dmg);
            if ((pl->flags_3C8 & 4) && !(pG->flags_54 & 0x20)) {
                dmg = (dmg + 1) / 2;
            }
            LifeDownSet(pPL, dmg, 0);
            pl->x3E0--;
            if (pl->x3E0 == 0 && (s16) pG->pl_life <= 0) {
                PlSetDamageSe(0xD);
                PlSetDamage(6, 0, 0);
                break;
            }
        }
        if (end) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    plem10KickCamMove(pl, pl->xFF);
    pl->subArc = pl->subArc2;
}

static void plem10DmGondolaShake(cPlayer* pl)
{
    Em10Work* w = EM10_WK((cEm10*) pPL->dmgType);
    cEm* em = (cEm*) pl->dmgType;

    pl->subArc = em->subArc;
    pl->dmg.set(0, 5);
    switch (pl->xFE) {
    case 0:
        if (pl->xFF) {
            MotionSetCore(pl, MOTION(pl), w->evtMot[3], 0, 3, 5, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), w->evtMot[3], 0, 3, 1, 0);
        }
        PlSetDamageSe(0);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0) && !pl->xFF) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}


static void subem10DmGondolaShake(cSubChar* sub)
{
    cSubChar* s = pSUB;

    s->dmg.x1 = 2;
    BitOn(pG->flags_5010, 0x10000);
    BitOn(pG->flags_5014, 0x20000000);
    switch (s->xFE) {
    case 0:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x43), 0, 3, 1, 0);
        s->xFE++;
    case 1:
        if (MotionMoveF(s, 0)) {
            s->xFE++;
        }
        break;
    case 2:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x44), 0, 3, 1, 0);
        s->xFE++;
    case 3:
        if (MotionMoveF(s, 0) && s->xFF == 0) {
            s->xFE++;
        }
        break;
    case 4:
        MotionSetCore(s, MOTION(s), PL_ARC_PTR(s->subArc, 0x45), 0, 3, 1, 0);
        s->xFE++;
    case 5:
        if (MotionMoveF(s, 0)) {
            EndSubDamage();
        }
        break;
    }
}

static void plemDmMStar(cPlayer* pl)
{
    cEm* em = (cEm*) pl->dmgType;
    int flag;

    pl->subArc = em->subArc;
    if (pl->xFF == 0) {
        pl->dmg.set(0, 2);
    }
    switch (pl->xFE) {
    case 0:
        flag = pl->xFF ? 0x41 : 1;
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x17A), 0, 3, flag, 0);
        PlSetFace(1);
        PlSetDamageSe(0);
        if (pl->xFF) {
            pl->dmg.set(0, 0xF);
        }
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            if (!pl->xFF) {
                pl->dmg.set(0, 0xF);
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void plemDmStun(cPlayer* pl)
{
    cEm* em = (cEm*) pl->dmgType;

    pl->subArc = em->subArc;
    pl->dmg.set(0, 2);
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x1A7), 0, 3, 1, 0);
        PlSetDamageSe(0);
        EstSetEm(pl, -1, 0, 0, 0xCD, 2, 0, 0, pl, 0);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            pl->dmg.set(0, 0xF);
            EndPlDamage();
        } else if (MOTION(pl)->seqFrame > 16.7f && MOTION(pl)->seqFrame < 17.3f && (s16) pG->pl_life <= 0) {
            EmRoutineSet(pPL, 2, 0, 0, 0);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

extern "C" void em10SetRtnFind(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int r;
    f32 ang;

    em->setFindPL();
    if (w->pShield) {
        return;
    }
    if (em->type == 10 || em->type == 13 || em->type == 2) {
        return;
    }
    if (w->wepType == 8) {
        return;
    }
    if (w->flags & 0x80) {
        return;
    }
    if (w->pWep2 && !w->pParasite && !w->x58C && (!w->pWep || w->wepType == 5)) {
        em->setWeaponFall();
        EmRoutineSet(em, 1, 0xC, 0, 0);
        return;
    }
    if (em10IgnitionCk(em)) {
        return;
    }
    r = em10ClawStickCK(em);
    if (r) {
        return;
    }
    if (em10SomebodyFindNowCk(em)) {
        if (w->x508 < 1.5707964f) {
            ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f));
            if (em->plDist2 < 9000000.0f || (em->plDist2 < 49000000.0f && ang < 0.7853982f)) {
                em10WalkRtnSet(em);
            } else {
                EmRoutineSet(em, 1, 0xD, 0, 0);
            }
        } else {
            em->xFD = 0x15;
            em->xFE = r;
            em->xFC = 1;
            em->xFF = 1;
        }
    } else {
        em10WalkRtnSet(em);
    }
}

void cEm10::setWeaponFall()
{
    Em10Work* w = EM10_WK(this);
    u8 wtype;

    if (!w->pWep) {
        return;
    }
    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x01000000 && hp > 0) {
        return;
    }
    if (type == 6) {
        return;
    }
    wtype = w->wepType;
    if (wtype == 9) {
        return;
    }
    if (!(w->flags & 0x80) && !(flags_3C8 & 0x40)) {
        if (wtype == 8) {
            return;
        }
        if (type == 0x18) {
            return;
        }
    }
    switch (w->wepType) {
    case 6:
    case 7:
    case 0xB:
    case 0xF:
    case 0x10:
        if (hp > 300) {
            return;
        }
        break;
    }
    EffectEspDelete(0, w->x69E, (u32) this, 0);
    EffectEspgenDelete(0, w->x69E, (int) this);
    EffectEfmDelete(0, w->x69E, (int) this);
    if (w->wepType == 4) {
        if (hp > 0) {
            return;
        }
        if ((int) w->flags >= 0) {
            return;
        }
        EffectEspDelete(0, w->x69C, (u32) w->pWep, 0);
        EffectEspgenDelete(0, w->x69C, (int) w->pWep);
        EffectEfmDelete(0, w->x69C, (int) w->pWep);
        w->flags &= 0x7FFFFFFF;
    } else {
        w->pWep->setFall(0, 0, 20.0f);
        w->pWep = 0;
        w->wepType = 0;
    }
}

int cEm10::ckFindPL()
{
    if (hp <= 0) {
        return 0;
    }
    if (checkStatus(5) && type != 6 && (EM10_WK(this)->flags & 0x100)) {
        return 1;
    }
    return 0;
}

void cEm10::setFindPL()
{
    Em10Work* w = EM10_WK(this);

    if (hp > 0 && checkStatus(5) && type != 6) {
        w->flags |= 0x100;
        w->flags &= ~0x800000;
        w->x638 = 0;
        w->x6B8 = 1;
    }
}

void cEm10::clearFindPL()
{
    Em10Work* w = EM10_WK(this);

    if (hp > 0 && checkStatus(5) && type != 6) {
        w->flags &= ~0x100;
        w->flags &= ~0x800000;
        w->x638 = 0;
    }
}

int cEm10::ckParasite()
{
    return EM10_WK(this)->pParasite != 0;
}

int cEm10::ckBombFire()
{
    return EM10_WK(this)->x640 != 0;
}

int cEm10::ckShiled()
{
    return EM10_WK(this)->pShield != 0;
}

int cEm10::ckBowgunFire()
{
    Em10Work* w = EM10_WK(this);

    if (w->wepType != 8) {
        return 0;
    } else {
        if (w->x6C2 == 0) {
            return 0;
        }
        return 1;
    }
}

// ===== cEm10 accessors and small helpers =====
u32 cEm10::ckGoto()
{
    return EM10_WK(this)->x5EC;
}

void cEm10::setGoto(Vec* pos, int range)
{
    Em10Work* w = EM10_WK(this);
    f32 y;

    if (w->flags & 0x4000) {
        return;
    }
    w->x5EC = range;
    w->x5F0 = *pos;
    y = SatMgr.getFloor(&w->x5F0, 600.0f, 100000.0f, 0, 0);
    if (y != -100000.0f) {
        w->x5F0.y = y + 50.0f;
    }
    w->x4EC = *pos;
    w->flags |= 0x04000004;
    if (!(flags_3C8 & 0x40)) {
        w->flags &= ~0x100;
    }
    w->flags &= ~0x800000;
}

void cEm10::setGotoSwitch(cModel* sw, int near, Vec* pos)
{
    Em10Work* w = EM10_WK(this);
    f32 y;

    if (w->flags & 0x4000) {
        return;
    }
    w->x5EC = near ? 3 : 4;
    if (pos) {
        w->x5F0 = *pos;
    } else {
        w->x5F0 = sw->pos;
    }
    y = SatMgr.getFloor(&w->x5F0, 600.0f, 100000.0f, 0, 0);
    if (y != -100000.0f) {
        w->x5F0.y = y + 50.0f;
    }
    w->pSwitch = sw;
    w->x4EC = w->x5F0;
    w->flags |= 0x04000004;
    if (!(flags_3C8 & 0x40)) {
        w->flags &= ~0x100;
    }
    w->flags &= ~0x800000;
}

void cEm10::setSwitch(cModel* sw)
{
    EM10_WK(this)->pSwitch = sw;
}

// Kick check line: enemy and player positions lifted by the character's height.
#define EM10_KICK_LINE(a, b, em)                                                                   \
    a = em->pos;                                                                                   \
    b = pPL->pos;                                                                                  \
    if (pG->x4FB8 == 3) {                                                                          \
        a.y += 500.0f;                                                                             \
        b.y += 500.0f;                                                                             \
    } else {                                                                                       \
        a.y += 1500.0f;                                                                            \
        b.y += 1500.0f;                                                                            \
    }

extern "C" void em10ActEvtSetKick(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;

    if (em->hp <= 0) {
        return;
    }
    switch (pG->x4FB8) {
    default:
        if (em->plDist2 > 2250000.0f) {
            return;
        }
        break;
    case 5:
        if (em->plDist2 > 2250000.0f) {
            return;
        }
        break;
    case 4:
        if (em->plDist2 > 4000000.0f) {
            return;
        }
        break;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f)) > 1.0471976f) {
        return;
    }
    if (em->type == 10 || em->type == 13 || em->type == 2 || em->type == 0x16) {
        return;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 700.0f) {
        return;
    }
    EM10_KICK_LINE(a, b, em);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    if (pG->x4FB8 == 3) {
        EM10_KICK_LINE(a, b, em);
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            return;
        }
    }
    switch (pG->x4FB8) {
    default:
        if (w->flags & 0x40000000) {
            ActBtn.set(7, 0xB, (int) em10KneeDownAction, (int) em, 1, 1, 0, 0);
        } else {
            ActBtn.set(7, 0xB, (int) em10KickAction, (int) em, 1, 1, 0, 0);
        }
        break;
    case 5:
        if (w->flags & 0x40000000) {
            ActBtn.set(0x3D, 0xB, (int) em10KneeDownAction, (int) em, 1, 1, 0, 0);
        } else {
            ActBtn.set(0x3C, 0xB, (int) em10KickAction, (int) em, 1, 1, 0, 0);
        }
        break;
    case 2:
        if (w->flags & 0x40000000) {
            ActBtn.set(0x39, 0xB, (int) em10KneeDownAction, (int) em, 1, 1, 0, 0);
        } else {
            ActBtn.set(0x38, 0xB, (int) em10KickAction, (int) em, 1, 1, 0, 0);
        }
        break;
    case 3:
        if (w->flags & 0x40000000) {
            ActBtn.set(7, 0xB, (int) em10KneeDownAction, (int) em, 1, 1, 0, 0);
        } else if (w->pParasite != 0 || w->x58C != 0 || (w->flags & 0x80)) {
            ActBtn.set(7, 0xB, (int) em10KneeDownAction, (int) em, 1, 1, 0, 0);
        } else {
            ActBtn.set(0x3B, 0xB, (int) em10KickAction, (int) em, 1, 1, 0, 0);
        }
        break;
    case 4:
        ActBtn.set(7, 0xB, (int) em10KickAction, (int) em, 1, 1, 0, 0);
        break;
    }
}

static void em10KickAction(cEm10* em)
{
    switch (pG->x4FB8) {
    default:
        SetPlDamage((int) em, plem10Kick);
        break;
    case 5:
        SetPlDamage((int) em, plem10Showtay);
        break;
    case 4:
        SetPlDamage((int) em, plem10Kick2);
        break;
    case 3:
        EmRoutineSet(em, 2, 0x15, 0, 0);
        em->dmg.set(0, 0x1E);
        pPL->dmg.set(0, 0x1E);
        break;
    }
    pPL->dmg.set(0, 0x1E);
    if (pSUB && !em10DmgDeadCk(&pSUB->dmg)) {
        pSUB->dmg.set(0, 0x1E);
    }
}

static void em10KneeDownAction(cEm10* em)
{
    switch (pG->x4FB8) {
    case 4:
        SetPlDamage((int) em, plem10Kick2);
        break;
    case 3:
    case 5:
        SetPlDamage((int) em, plem10Kick);
        pPL->xFF = 1;
        break;
    default:
        SetPlDamage((int) em, plem10Kick);
        pPL->xFF = 1;
        break;
    }
    pPL->dmg.set(0, 0x1E);
    if (pSUB && !em10DmgDeadCk(&pSUB->dmg)) {
        pSUB->dmg.set(0, 0x1E);
    }
}

static void plem10Kick(cPlayer* pl)
{
    cEm* em = (cEm*) pl->dmgType;
    Vec v;

    pl->subArc = em->subArc;
    pl->dmg.set(0, 0x1E);
    BitOn(pG->flags_5014, 0x40000000);
    switch (pl->xFE) {
    case 0:
        if (pl->xFF) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x25), 0, 6, 1, 0);
            pl->x3E0 = 0x11;
            pl->x3E4 = 12;
            pl->x3E8 = 0x23;
            if (pGS->x4FB8 == 5) {
                pl->x3E0 = 0x10;
                pl->x3E8 = 0x30;
                EstSetEm(pl, -1, 0, 0, 0x10, 0x93, 0, 0, pl, 0);
                EstSetEm(pl, -1, 0, 0, 3, 9, 0, 0, pl, 0);
                SndCall(1, 0x4D, &pl->pos, 0, 0, pPL);
            }
            if (pGS->x4FB8 == 2) {
                pl->x3E0 = 0xD;
                EstSetEm(pl, -1, 0, 0, 0x10, 0x95, 0, 0, pl, 0);
                EstSetEm(pl, -1, 0, 0, 3, 8, 0, 0, pl, 0);
                SndCall(1, 0x4D, &pl->pos, 0, 0, pPL);
            }
            if (pGS->x4FB8 == 3) {
                pl->x3E0 = 12;
                pl->x3E8 = 0x32;
                EstSetEm(pl, -1, 0, 0, 0x10, 0x92, 0, 0, pl, 0);
                EstSetEm(pl, -1, 0, 0, 3, 9, 0, 0, pl, 0);
                SndCall(1, 0x4D, &pl->pos, 0, 0, pPL);
            }
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x29D), 0, 6, 1, 0);
            pl->x3E0 = 0x11;
            pl->x3E4 = 0xA;
            pl->x3E8 = 0x21;
            if (pGS->x4FB8 == 2) {
                EstSetEm(pl, -1, 0, 0, 0x10, 0x94, 0, 0, pl, 0);
                EstSetEm(pl, -1, 0, 0, 3, 9, 0, 0, pl, 0);
                SndCall(1, 0x4D, &pl->pos, 0, 0, pPL);
            }
        }
        GameAddPoint(9);
        pl->xFF = Rnd() & 3;
        pl->xFE++;
    case 1:
        if (pl->x3E4) {
            pl->x3E4--;
            pl->rot.y += Muku(&pl->pos, &((cEm*) pl->dmgType)->pos, pl->rot.y, 0.19634955f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            if (pl->x3E4 == 0) {
                SndCall(1, 0x11, &pl->pos, 0, 0, pPL);
                SndCall(1, 0x10, &pl->pos, 0, 0, pPL);
            }
        }
        if (pl->x3E0) {
            pl->x3E0--;
            if (pl->x3E0 == 0) {
                v.x = 0.0f;
                v.y = 1500.0f;
                v.z = 300.0f;
                PSMTXMultVec(pPL->mat, &v, &v);
                if (PlWepHitCheck3(&v, 0x14, 0xA, 1200.0f)) {
                    SndCall(1, 0xF, &pl->pos, 0, 0, pPL);
                }
                v.x = 0.0f;
                v.y = 1000.0f;
                v.z = 300.0f;
                PSMTXMultVec(pPL->mat, &v, &v);
                if (PlWepHitCheck3(&v, 0x14, 0xA, 1200.0f)) {
                    SndCall(1, 0xF, &pl->pos, 0, 0, pPL);
                }
            }
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        } else if (pl->x3E8) {
            pl->x3E8--;
        } else if (joyKamae() || (Key.on & 0x10F)) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    plem10KickCamMove(pl, pl->xFF);
    pl->subArc = pl->subArc2;
}

static void plem10Kick2(cPlayer* pl)
{
    cEm* em = (cEm*) pl->dmgType;
    Vec v;

    pl->subArc = em->subArc;
    pl->dmg.set(0, 0x1E);
    pG->flags_5014 |= 0x40000000;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x29D), (int) PL_ARC_PTR(pl->subArc, 0x29E), 6, 1, 0);
        pl->x3E4 = 10;
        pl->x3E8 = 0x42;
        pl->x3EC = 0;
        EstSetEm(pl, -1, 0, 0, 3, 8, 0, 0, pl, 0);
        SndCall(1, 0x4D, &pl->pos, 0, 0, pPL);
        GameAddPoint(9);
        pl->xFE++;
    case 1:
        if (pl->x3E4) {
            pl->x3E4--;
            pl->rot.y += Muku(&pl->pos, &((cEm*) pl->dmgType)->pos, pl->rot.y, 0.19634955f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        if (pl->seFlags28B & 4) {
            if (pl->x3EC) {
                SndCall(1, 0x11, &pl->pos, 0, 0, pPL);
                SndCall(1, 0x43, &pl->pos, 0, 0, pPL);
            } else {
                SndCall(1, 0x11, &pl->pos, 0, 0, pPL);
                SndCall(1, 0x10, &pl->pos, 0, 0, pPL);
            }
            pl->x3EC = 1;
        }
        if (pl->seFlags28B & 1) {
            v.x = 0.0f;
            v.y = 1500.0f;
            v.z = 300.0f;
            PSMTXMultVec(pPL->mat, &v, &v);
            if (PlWepHitCheck3(&v, 0x24, 0xA, 1200.0f)) {
                SndCall(1, 0xF, &pl->pos, 0, 0, pPL);
            }
            v.x = 0.0f;
            v.y = 1000.0f;
            v.z = 300.0f;
            PSMTXMultVec(pPL->mat, &v, &v);
            if (PlWepHitCheck3(&v, 0x24, 0xA, 1200.0f)) {
                SndCall(1, 0xF, &pl->pos, 0, 0, pPL);
            }
        }
        if (pl->seFlags28B & 2) {
            v.x = 0.0f;
            v.y = 1500.0f;
            v.z = 300.0f;
            PSMTXMultVec(pPL->mat, &v, &v);
            if (PlWepHitCheck3(&v, 0x14, 0xA, 1200.0f)) {
                SndCall(1, 0xF, &pl->pos, 0, 0, pPL);
            }
            v.x = 0.0f;
            v.y = 1000.0f;
            v.z = 300.0f;
            PSMTXMultVec(pPL->mat, &v, &v);
            if (PlWepHitCheck3(&v, 0x14, 0xA, 1200.0f)) {
                SndCall(1, 0xF, &pl->pos, 0, 0, pPL);
            }
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        } else if (pl->x3E8) {
            pl->x3E8--;
        } else if (joyKamae() || (Key.on & 0x10F)) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

extern "C" void em10ActEvtSetFS(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec a;
    Vec b;
    int hit;

    if (em->hp <= 0) {
        return;
    }
    if (em->plDist2 > 1000000.0f) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f)) > 1.0471976f) {
        return;
    }
    if (em->type == 10 || em->type == 13 || em->type == 2 || em->type == 0x16) {
        return;
    }
    if (pG->x4FB8 != 4) {
        if (w->x6C5 == 0) {
            return;
        }
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 700.0f) {
        return;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 1500.0f;
    b.y += 1500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    a = em->pos;
    b = pPLS->pos; // struct-member view: keeps the pPL reload below the em->pos copy
    a.y += 1500.0f;
    b.y += 1500.0f;
    hit = EatMgr.hitCheck(&a, &b, 0, 0, 0, 0);
    if (hit) {
        return;
    }
    if (pG->x4FB8 == 4) {
        ActBtn.set(0x3A, 0xB, (int) em10FSAction, (int) em, 1, 1, 0, hit);
    } else {
        ActBtn.set(0x2C, 0xB, (int) em10FSAction, (int) em, 1, 1, 0, hit);
    }
}

static void em10FSAction(cEm10* em)
{
    if (!em10DmgDeadCk(&em->dmg)) {
        if (pG->x4FB8 == 4) {
            EmRoutineSet(em, 2, 0x14, 0, 0);
            SetPlDamage((int) em, plem10KneeKick);
            em->dmg.set(0, 0x1E);
            pPL->dmg.set(0, 0x1E);
            if (pSUB && !em10DmgDeadCk(&pSUB->dmg)) {
                pSUB->dmg.set(0, 0x1E);
            }
        } else {
            EmRoutineSet(em, 2, 0x10, 0, 0);
            SetPlDamage((int) em, plem10FS);
            em->dmg.set(0, 0x1E);
            pPL->dmg.set(0, 0x1E);
            if (pSUB && !em10DmgDeadCk(&pSUB->dmg)) {
                pSUB->dmg.set(0, 0x1E);
            }
        }
    }
}

static void plem10FS(cPlayer* pl)
{
    cEm* em = (cEm*) pl->dmgType;

    pl->subArc = em->subArc;
    pl->dmg.set(0, 0x1E);
    pG->flags_5014 |= 0x40000000;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0xD6), 0, 0, 1, 0);
        GameAddPoint(9);
        pl->atari.flags |= 8;
        pl->xFF = Rnd() & 1;
        pl->xFE++;
    case 1:
        if (MOTION(pl)->seqFrame > 9.7f && MOTION(pl)->seqFrame < 10.3f) {
            SndCall(1, 0x11, &pl->pos, 0, 0, pPL);
            SndCall(1, 0x10, &pl->pos, 0, 0, pPL);
        }
        if (MOTION(pl)->seqFrame > 60.7f && MOTION(pl)->seqFrame < 61.3f) {
            SndCall(1, 0x43, &pl->pos, 0, 0, pPL);
            SndCall(1, 0x44, &pl->pos, 0, 0, pPL);
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void plem10KneeKick(cPlayer* pl)
{
    cEm* em = (cEm*) pl->dmgType;

    pl->subArc = em->subArc;
    pl->dmg.set(0, 0x1E);
    pG->flags_5014 |= 0x40000000;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x2B4), 0, 0, 1, 0);
        GameAddPoint(9);
        EstSetEm(pl, -1, 0, 0, 3, 9, 0, 0, pl, 0);
        pl->atari.flags |= 8;
        pl->xFF = Rnd() & 1;
        pl->xFE++;
    case 1:
        if (MOTION(pl)->seqFrame > 17.7f && MOTION(pl)->seqFrame < 18.3f) {
            SndCall(1, 0x3B, &pl->pos, 0, 0, pPL);
            SndCall(1, 0x3D, &pl->pos, 0, 0, pPL);
        }
        if (MOTION(pl)->seqFrame > 35.7f && MOTION(pl)->seqFrame < 36.3f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if ((MOTION(pl)->seqFrame > 9.7f && MOTION(pl)->seqFrame < 10.3f) || (MOTION(pl)->seqFrame > 46.7f && MOTION(pl)->seqFrame < 47.3f)) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void plem10NeckBreak(cPlayer* pl)
{
    cEm* em = (cEm*) pl->dmgType;
    int end;

    pl->subArc = em->subArc;
    pl->dmg.set(0, 0x1E);
    pG->flags_5014 |= 0x40000000;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x2B9), 0, 0, 1, 0);
        GameAddPoint(9);
        EstSetEm(pl, -1, 0, 0, 3, 8, 0, 0, pl, 0);
        SndCall(1, 0x4D, &pl->pos, 0, 0, pPL);
        pl->pWep->setTrans(0, 0);
        pl->setRightHand(0);
        pl->setLeftHand(0);
        pl->x3E0 = 30;
        pl->x3E4 = 70;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 1.0f, 1.0f);
            DmgMgr.set(3, 2, &pl->pos, 1500.0f, 1500.0f);
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (pl->x3E4) {
            pl->x3E4--;
        } else if (joyKamae() || (Key.on & 0x10F)) {
            pl->pWep->setTrans(1, 0);
            pl->setRightHand(1);
            pl->setLeftHand(0x63);
            EndPlDamage();
            pl->dmg.set(0, 0xF);
            break;
        }
        if (end) {
            pl->pWep->setTrans(1, 0);
            pl->setRightHand(1);
            pl->setLeftHand(0x63);
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// Dead-stripped camera move of the neck break (pool kept at .rodata 0x17F8: -146 1317 312 1986
// 2053.5 -609 146 -1986 0.5 0.3 0 1 50). See em10FindFloorCk.
static void plem10NeckBreakCamMove(cPlayer* pl, int a)
{
    Vec pos;
    Vec rot;
    Vec pos2;
    f32 rate;
    f32 rate2;

    pos.x = -146.0f;
    pos.y = 1317.0f;
    pos.z = 312.0f;
    rot.x = 1986.0f;
    rot.y = 2053.5f;
    rot.z = -609.0f;
    pos2.x = 146.0f;
    pos2.y = -1986.0f;
    rate = 0.5f;
    rate2 = 0.3f;
    pos2.z = 0.0f;
    if (a) {
        rate = 1.0f;
        rate2 = 50.0f;
    }
    PSMTXMultVec(pl->mat, &pos, &pos);
    PSMTXMultVec(pl->mat, &rot, &rot);
    PSMTXMultVec(pl->mat, &pos2, &pos2);
    pl->x3A8.x = rate;
    pl->x3A8.y = rate2;
}

static void plem10Showtay(cPlayer* pl)
{
    cEm* em = (cEm*) pl->dmgType;
    f32 f;

    pl->subArc = em->subArc;
    pl->dmg.set(0, 0x1E);
    pG->flags_5014 |= 0x40000000;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x2B4), 0, 3, 1, 0);
        pl->x3E4 = 12;
        pl->x3E8 = 0x37;
        GameAddPoint(9);
        pl->x3E0 = 0x11;
        pl->xFF = Rnd() & 3;
        pl->pWep->setTrans(0, 0);
        pl->setRightHand(0);
        pl->setLeftHand(0);
        EstSetEm(pl, -1, 0, 0, 0x10, 0x91, 0, 0, pl, 0);
        EstSetEm(pl, -1, 0, 0, 3, 8, 0, 0, pl, 0);
        SndCall(1, 0x4D, &pl->pos, 0, 0, pPL);
        pl->xFE++;
    case 1:
        if (pl->x3E4) {
            pl->x3E4--;
            pl->rot.y += Muku(&pl->pos, &((cEm*) pl->dmgType)->pos, pl->rot.y, 0.19634955f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            if (pl->x3E4 == 0) {
                SndCall(1, 0x11, &pl->pos, 0, 0, pPL);
                SndCall(1, 0x10, &pl->pos, 0, 0, pPL);
            }
        }
        if (MOTION(pl)->seqFrame > 12.7f && MOTION(pl)->seqFrame < 13.3f) {
            SndCall(1, 0x50, &pl->pos, 0, 0, pl);
        }
        f = MOTION(pl)->seqFrame;
        if ((f > 12.7f && f < 13.3f) || (f > 13.7f && f < 14.3f) || (f > 14.7f && f < 15.3f) || (f > 15.7f && f < 16.3f) ||
            (f > 16.7f && f < 17.3f)) {
            if (PlWepHitCheck3(&pl->getPartsPtr(10)->worldPos, 0x25, 0xA, 800.0f)) {
                SndCall(1, 0x51, &pl->pos, 0, 0, pPL);
            }
        }
        if (MotionMoveF(pl, 0)) {
            pl->pWep->setTrans(1, 0);
            pl->setRightHand(1);
            pl->setLeftHand(0x63);
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        } else if (pl->x3E8) {
            pl->x3E8--;
        } else if (joyKamae() || (Key.on & 0x10F)) {
            pl->pWep->setTrans(1, 0);
            pl->setRightHand(1);
            pl->setLeftHand(0x63);
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}
void em10ActEvtSetTrade(cEm10* em)
{
    Mtx inv;
    Vec v;
    u32 i;
    f32 a;

    if (em->type != 6) {
        return;
    }
    if (em->hp <= 0) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f)) > 0.7853982f) {
        return;
    }
    PSMTXInverse(pPL->mat, inv);
    PSMTXMultVec(inv, &em->pos, &v);
    switch (pG->room_id) {
    default:
        if (v.z > 1500.0f) {
            return;
        }
        if (v.z < 0.0f) {
            return;
        }
        if (v.x > 700.0f) {
            return;
        }
        if (v.x < -700.0f) {
            return;
        }
        a = fabsf(v.y);
        if (a > 700.0f) {
            return;
        }
        break;
    case 0x20F:
    case 0x301:
    case 0x305:
        if (em->plDist2 > 2250000.0f) {
            if (v.z > 3000.0f) {
                return;
            }
            if (v.z < 0.0f) {
                return;
            }
            if (v.x > 700.0f) {
                return;
            }
            if (v.x < -700.0f) {
                return;
            }
        }
        a = fabsf(v.y);
        if (a > 700.0f) {
            return;
        }
        break;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        {
            f32 dx = em->pos.x - e->pos.x;
            f32 dy = em->pos.y - e->pos.y;
            f32 dz = em->pos.z - e->pos.z;
            if (dx * dx + dy * dy + dz * dz < 100000000.0f) {
                return;
            }
        }
    }
    ActBtn.set(0, 2, (int) em10TradeAction, (int) em, 0, 1, 0, 0);
}

static void em10TradeAction(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (pG->room_id != 0x20F) {
        if (w->x6B0) {
            SubScreenOpen(0x10, 0);
        } else {
            EmRoutineSet(em, 1, 0x46, 0, 0);
            w->x6B0 = 1;
        }
    } else {
        SubScreenOpen(0x10, 0);
    }
    pPL->dmg.set(0, 0x1E);
}

// Empty in the shipped build; the 0x28 frame is left by two Vec and two f32 locals of the
// removed camera move (aggregates get their stack slot at declaration in GCC 2.95).
extern "C" void plem10KickCamMove(cPlayer* pl, int a)
{
    Vec pos;
    Vec rot;
    f32 tmp[2];
}

void em10SetCrash(cEm10* em, f32 r)
{
    if (em->seFlags28B & 0x10) {
        DmgMgr.set(3, 2, &em->pos, 1500.0f, r);
    }
}

int em10CrashCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    int r;

    if (em->dmg.stat != 0) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (w->pShield && w->pShield->hp > 0) {
        return 0;
    }
    if (em->type == 10 || em->type == 13 || em->type == 2) {
        return 0;
    }
    if (em->x38D == 0x2D || em->x38D == 0x37 || em->x38D == 0x38) {
        return 0;
    }
    if (w->pGatling) {
        return 0;
    }
    if (em->xFC == 1) {
        if (em->xFD == 0x43 || em->xFD == 0x40 || em->xFD == 0x41 || em->xFD == 0x44 || em->xFD == 0x45 ||
            em->xFD == 0x48) {
            return 0;
        }
    }
    if (w->flags & 0x101B6818) {
        return 0;
    }
    if (em->type == 6) {
        return 0;
    }
    r = DmgMgr.hitCheck(&em->pos, &v) == 3;
    if (w->pShield && w->pShield->hp <= 0) {
        v.x = 0.0f;
        v.y = 1000.0f;
        v.z = 1000.0f;
        PSMTXMultVec(em->mat, &v, &v);
        r = 1;
    }
    if (!r) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &v, em->rot.y, 3.1415927f)) < 1.5707964f) {
        EmRoutineSet(em, 1, 0x3B, 0, 0);
    } else {
        em->xFD = 0x3B;
        em->xFE = 0;
        em->xFC = 1;
        em->xFF = 1;
    }
    w->x5E0 = v;
    if ((w->flags & 0x00200000) && w->pLadder) {
        w->pLadder->setDown2();
        w->pLadder = 0;
    }
    return 1;
}

void em10MouthPartsReset(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModel* p;

    if (em->hp > 0 && !(w->flags & 0x80)) {
        p = em->getPartsPtr(0x1B);
        p->rot.x = 0.0f;
        p->rot.y = 0.0f;
        p->rot.z = 0.0f;
        p = em->getPartsPtr(0x1C);
        p->rot.x = 0.0f;
        p->rot.y = 0.0f;
        p->rot.z = 0.0f;
        p = em->getPartsPtr(0x1D);
        p->rot.x = 0.0f;
        p->rot.y = 0.0f;
        p->rot.z = 0.0f;
        p = em->getPartsPtr(0x1E);
        p->rot.x = 0.0f;
        p->rot.y = 0.0f;
        p->rot.z = 0.0f;
        p = em->getPartsPtr(0x1F);
        p->rot.x = 0.0f;
        p->rot.y = 0.0f;
        p->rot.z = 0.0f;
        p = em->getPartsPtr(0x20);
        p->rot.x = 0.0f;
        p->rot.y = 0.0f;
        p->rot.z = 0.0f;
        p = em->getPartsPtr(0x21);
        p->rot.x = 0.0f;
        p->rot.y = 0.0f;
        p->rot.z = 0.0f;
    }
}

// One probe of the roof search: a point 800 out from the enemy in the given direction, then the same
// point after turning towards the wall that was hit. Both must hit roof / wall geometry.
#define EM10_ROOF_PROBE(px, py, pz)                                                                   \
    b.x = px;                                                                                      \
    b.y = py;                                                                                      \
    b.z = pz;                                                                                      \
    PSMTXMultVec(em->mat, &b, &b);                                                                 \
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0) & mask) {                                           \
        PSMTXRotRad(m, 'y', atan2f(-nrm.x, -nrm.z));                                               \
        TransMatrix(m, &em->pos);                                                                  \
        b.x = 0.0f;                                                                                \
        b.y = 500.0f;                                                                              \
        b.z = 800.0f;                                                                              \
        PSMTXMultVec(m, &b, &b);                                                                   \
        if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0) & mask) {                                       \
            w->x5DC = atan2f(-nrm.x, -nrm.z);                                                      \
            EmRoutineSet(em, 2, 7, 0, 1);                                                          \
            return 1;                                                                              \
        }                                                                                          \
    }

// Knocked against a roof / wall: pick the wall direction (x5DC) and start damage routine 2-7.
int em10RoofDmCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx m;
    Vec a;
    Vec b;
    Vec nrm;
    u32 mask = 0x00182810;

    switch (em->x38D) {
    case 0x37:
        w->x5DC = em->rot.y;
        em->be_flag |= 0x10000;
        em->hp = 0;
        EmRoutineSet(em, 2, 7, 0, 1);
        return 1;
    case 0x2D:
        w->x5DC = em->rot.y;
        if (Rnd() & 1) {
            w->x5DC += 1.5707964f;
        } else {
            w->x5DC -= 1.5707964f;
        }
        w->x5DC = LIMIT_ANGLE(w->x5DC);
        em->be_flag |= 0x10000;
        em->hp = 0;
        EmRoutineSet(em, 2, 7, 0, 1);
        return 1;
    case 0x2C:
    case 0x31:
        w->x5DC = em->rot.y;
        if (Rnd() & 1) {
            w->x5DC += 1.5707964f;
        } else {
            w->x5DC -= 1.5707964f;
        }
        w->x5DC = LIMIT_ANGLE(w->x5DC);
        em->hp = 0;
        em->be_flag |= 0x10000;
        if (em->xFE < 4) {
            EmRoutineSet(em, 2, 7, 0, 1);
        } else {
            EmRoutineSet(em, 2, 7, 0, 2);
        }
        return 1;
    case 0x27:
    case 0x28:
    case 0x29:
        w->x5DC = em->rot.y;
        if (Rnd() & 1) {
            w->x5DC += 1.5707964f;
        } else {
            w->x5DC -= 1.5707964f;
        }
        w->x5DC = LIMIT_ANGLE(w->x5DC);
        em->be_flag |= 0x10000;
        em->hp = 0;
        EmRoutineSet(em, 2, 7, 0, 1);
        return 1;
    default:
        break;
    }
    if (w->wepType == 4) {
        u8 r = Rnd() % 3;
        if (r != 0) {
            return 0;
        }
    }
    if (em->type == 0xA || em->type == 0xD || em->type == 2 || em->type == 0x16) {
        return 0;
    }
    if (pG->x4F88 > 6) {
        u8 r = Rnd() % 100;
        if (r > 0x4B) {
            return 0;
        }
    }
    if (pG->x4F88 == 10) {
        return 0;
    }
    a = em->pos;
    a.y += 500.0f;
    EM10_ROOF_PROBE(0.0f, 500.0f, 800.0f);
    EM10_ROOF_PROBE(800.0f, 500.0f, 0.0f);
    EM10_ROOF_PROBE(-800.0f, 500.0f, 0.0f);
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = -800.0f;
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0) & mask) {
        PSMTXRotRad(m, 'y', atan2f(-nrm.x, -nrm.z));
        TransMatrix(m, &em->pos);
        b.x = 0.0f;
        b.y = 500.0f;
        b.z = 800.0f;
        PSMTXMultVec(m, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0) & mask) {
            w->x5DC = atan2f(nrm.x, nrm.z);
            EmRoutineSet(em, 2, 7, 0, 0);
            return 1;
        }
    }
    return 0;
}

void em10FootSe(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec v;
    cModel* p;
    u32 se;
    int a;
    int b;

    if (em->type == 0xA || em->type == 0xD || w->wepType == 0xB) {
        if (w->x6C0 != 0) {
            w->x6C0--;
            if (w->x6C0 == 0) {
                SndCall(8, 0xA0, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
            }
        }
    }
    if (em->seNo == 0) {
        return;
    }
    se = em->seNo - 1;
    if (CheckInWater(em, 0)) {
        p = em->getPartsPtr(0);
        if (se <= 5 || se == 0x77) {
            em->seNo = 0;
            return;
        }
    }
    if (em->type != 0xA && em->type != 0xD) {
        switch (se) {
        case 0:
            a = 0x10;
            b = 0x15;
            if (w->wepType == 0xB) {
                w->x6C0 = 1;
            }
            break;
        case 1:
            a = 0x11;
            b = 0x19;
            if (w->wepType == 0xB) {
                w->x6C0 = se;
            }
            break;
        case 2:
            a = 0x12;
            b = 0x15;
            if (w->wepType == 0xB) {
                SndCall(8, 0xA4, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
            }
            break;
        case 3:
            a = 0x13;
            b = 0x19;
            if (w->wepType == 0xB) {
                SndCall(8, 0xA4, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
            }
            break;
        default:
            return;
        }
    } else {
        switch (se) {
        case 0:
            w->x6C0 = 1;
            a = 0x18;
            b = 0x15;
            break;
        case 1:
            w->x6C0 = se;
            a = 0x19;
            b = 0x19;
            break;
        case 2:
            a = 0x1A;
            b = 0x15;
            SndCall(8, 0xA4, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
            break;
        case 3:
            a = 0x1B;
            b = 0x19;
            SndCall(8, 0xA4, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
            break;
        case 4:
            em->seNo = 0;
            SndCall(6, 0x6F, &em->getPartsPtr(0)->worldPos, 0, 0, em);
            return;
        case 5:
            em->seNo = 0;
            SndCall(6, 0x70, &em->getPartsPtr(0)->worldPos, 0, 0, em);
            return;
        default:
            return;
        }
    }
    em->seNo = 0;
    p = em->getPartsPtr(b);
    SndCall(5, a, &p->worldPos, 0, 0, em);
    if (ChkWaterEffectEnable(&em->pos)) {
        v = p->worldPos;
        v.y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
        EstSet(0, -1, &v, 0, 0x10, 0x3A, 0, 0, 0, 0);
    }
}

extern "C" int em10DootAtkCk(cEm10* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        if (e->xFC != 1) {
            continue;
        }
        if (e->xFD != 0x3D && e->xFD != 0x3F) {
            continue;
        }
        {
            f32 dx = em->pos.x - e->pos.x;
            f32 dy = em->pos.y - e->pos.y;
            f32 dz = em->pos.z - e->pos.z;
            if (dx * dx + dy * dy + dz * dz > 9000000.0f) {
                continue;
            }
        }
        return 0;
    }
    return 1;
}

extern "C" int em10ThrowNearCk(cEm10* em)
{
    Mtx m;
    Vec v;
    u32 i;

    PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos));
    TransMatrix(m, &em->pos);
    PSMTXInverse(m, m);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(5)) {
            continue;
        }
        PSMTXMultVec(m, &e->pos, &v);
        if (em->plDist2 < v.z * v.z) {
            continue;
        }
        if (v.z < 0.0f) {
            continue;
        }
        if (v.y > 1500.0f) {
            continue;
        }
        if (v.y < -1500.0f) {
            continue;
        }
        if (v.x > 250.0f) {
            continue;
        }
        if (v.x < -250.0f) {
            continue;
        }
        return 1;
    }
    return 0;
}

int cEm10::checkThrow()
{
    if ((s16) EM10_WK(this)->x670 == 0) {
        return 0;
    }
    return 1;
}

int cEm10::ckResetEnable()
{
    if (EM10_WK(this)->x6B7 == 0) {
        return 0;
    }
    return 1;
}

void cEm10::chgSet(u8 no)
{
    EM10_WK(this)->x4C4 = no;
    x38D = no;
}

void cEm10::setReset()
{
    Em10Work* w = EM10_WK(this);
    f32 sc;
    cModel* p;
    cModelInfo* info;

    alpha = 1.0f;
    be_flag = (be_flag | 2) & ~0x10000;
    atari.flags |= 0x300;
    atari.flags &= ~0x10;
    em10HeadSet(this, 0);
    switch (emsetNo & 0xF) {
    default: {
        sc = fRand1_1() * 0.01f + 1.03f;
        const f32 k = 1.01f; // pool order (see em10_R0_Init)
        break;
    }
    case 7:
        sc = 1.1f;
        break;
    case 0:
    case 4:
    case 9:
    case 0xB:
        sc = fRand1_1() * 0.01f + 1.01f;
        break;
    }
    if (type == 6) {
        sc = 1.05f;
    }
    scale.z = sc;
    scale.y = sc;
    scale.x = sc;
    w->scaleBase = scale;
    p = getPartsPtr(0x1B);
    p->scale.x = 1.0f;
    p->scale.y = 1.0f;
    p->scale.z = 1.0f;
    pos = w->startPos;
    oldPos = pos;
    rot.x = 0.0f;
    rot.y = w->startRotY;
    rot.z = 0.0f;
    hp = hpMax;
    w->x4D8 = w->startPos;
    x38D = w->x4C4;
    if (w->x1A0) {
        w->x1A0->be_flag |= 8;
    }
    if (w->x1A4) {
        w->x1A4->be_flag |= 8;
    }
    if (w->x1A8) {
        w->x1A8->be_flag |= 8;
    }
    if (w->x1AC) {
        w->x1AC->be_flag |= 8;
    }
    if (w->x1B4) {
        w->x1B4->be_flag |= 8;
    }
    if (w->x1B8) {
        w->x1B8->be_flag |= 8;
    }
    if (w->x1BC) {
        w->x1BC->be_flag |= 8;
    }
    if (w->x1C0) {
        w->x1C0->be_flag |= 8;
    }
    em10WeaponInit(this);
    em10ShieldSet(this);
    for (info = pInfo; info; info = info->pNext) {
        info->color[0] = 0xFF;
        info->color[1] = 0xFF;
        info->color[2] = 0xFF;
    }
    em10InitRtnSet(this);
    hitInfo.flags = 1;
    MotionMoveF(this, 0);
    em10_R0_Move(this);
    partsWorldCalc();
    for (p = pParts; p; p = p->pParts) {
        p->oldWorldPos = p->worldPos;
        p->x88 = p->oldWorldPos;
    }
}

void cEm10::setEvtMotion(void* m0, void* m1, void* m2, void* m3)
{
    Em10Work* w = EM10_WK(this);

    w->evtMot[0] = m0;
    w->evtMot[4] = m1;
    w->evtMot[1] = m2;
    w->evtMot[5] = m3;
}

void cEm10::setGondolaMotion(void* m0, void* m1, void* m2, void* m3)
{
    Em10Work* w = EM10_WK(this);

    w->evtMot[0] = m0;
    w->evtMot[1] = m1;
    w->evtMot[2] = m2;
    w->evtMot[3] = m3;
}

void cEm10::setR11DMotion(void* m0)
{
    EM10_WK(this)->evtMot[0] = m0;
}

void cEm10::setDrill(void* m0, void* m1, void* m2, void* m3)
{
    Em10Work* w = EM10_WK(this);

    w->evtMot[0] = m0;
    w->evtMot[1] = m1;
    w->x564 = (u32) m2;
    w->x20 = (int) m3;
}

void cEm10::setGatling(cObjGatling* g, void* m0, void* m1, void* m2, void* m3)
{
    Em10Work* w = EM10_WK(this);

    if (g) {
        w->evtMot[0] = m0;
        w->evtMot[1] = m1;
        w->evtMot[2] = m2;
        w->evtMot[3] = m3;
        w->pGatling = g;
        w->gatlingMode = 1;
        // Direct u8 routine stores (not EmRoutineSet): the QImode constant 1 is shared with the
        // gatlingMode store, which keeps the pGatling store after the call-argument moves.
        xFC = 1;
        xFD = 0x5F;
        xFE = 0;
        xFF = 0;
        be_flag |= 0x10000;
        g->setRide(this);
    }
}

void cEm10::setGatlingMode(u8 no)
{
    EM10_WK(this)->gatlingMode = no;
}

void cEm10::setUFOCatch(void* m0, void* m1)
{
    Em10Work* w = EM10_WK(this);

    w->evtMot[0] = m0;
    w->evtMot[1] = m1;
    EmRoutineSet(this, 1, 0x65, 0, 0);
}

void cEm10::setLost()
{
    Em10Work* w = EM10_WK(this);

    atari.throughOn();
    EmSetDie(this);
    EmReserveDropItem(this);
    clearStatus(5);
    em10CoreBreak(this, 1);
    if (w->x58C) {
        w->x58C->setReset();
        w->x58C = 0;
    }
    w->x5D8 = 1.0f;
    if (w->pWep) {
        w->pWep->setLost();
        w->pWep = 0;
        w->wepType = 0;
    }
    if (w->pWep2) {
        w->pWep2->setLost();
        w->pWep2 = 0;
        w->wep2Type = 0;
    }
    be_flag &= ~2;
    w->flags |= 0x400000;
    w->x6B7 = 1;
}

extern "C" void em10BlendMotSet(cEm10* em, void* m0, void* m1, void* m2, int a, int b, int c, int d)
{
    Em10Work* w = EM10_WK(em);
    MotionWorkSub* bm;
    void* m;
    int seq;
    f32 rate = fabsf(w->blendRate);

    MotionSetCore(em, MOTION(em), m0, a, (u8) w->x740, (u16) d, (u16) w->x744);
    if (w->blendRate < 0.0f) {
        m = m1;
        seq = b;
    } else {
        m = m2;
        seq = c;
    }
    bm = &w->blendMot;
    MotionSetCore(em, bm, m, seq, (u8) w->x740, (u16) d, (u16) w->x744);
    em->blendMot = bm;
    bm->blendRate = rate * 0.00390625f;
    if (w->x740) {
        w->x740--;
    }
    w->x744++;
    if (w->x744 >= em->frameMax) {
        w->x744 = 0;
    }
}

static Vec em10_hide_ofs_r = { 2000.0f, 0.0f, 0.0f };
static Vec em10_hide_ofs_l = { -2000.0f, 0.0f, 0.0f };

extern "C" int em10HideRtnCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Mtx m;
    Vec v;
    u32 i;
    EmiEntry* e;
    f32 ang;
    f32 a;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    for (i = 0; i < EMI_DATA->n; i++) {
        e = &EMI_DATA->entry[i];
        if (e->type != 1) {
            continue;
        }
        switch (e->sub) {
        case 0:
            {
                f32 dx = e->pos.x - em->pos.x;
                f32 dy = e->pos.y - em->pos.y;
                f32 dz = e->pos.z - em->pos.z;
                f32 d = dx * dx + dy * dy + dz * dz;
                if (d > 6250000.0f) {
                    continue;
                }
                if (d < 2250000.0f) {
                    continue;
                }
            }
            ang = Muku(&e->pos, &pPL->pos, e->rotY, 3.1415927f);
            a = fabsf(ang);
            if (a > 1.0471976f) {
                continue;
            }
            {
                f32 dx = e->pos.x - pPL->pos.x;
                f32 dy = e->pos.y - pPL->pos.y;
                f32 dz = e->pos.z - pPL->pos.z;
                if (dx * dx + dy * dy + dz * dz < 25000000.0f) {
                    continue;
                }
            }
            PSMTXRotRad(m, 'y', e->rotY);
            TransMatrix(m, &e->pos);
            PSMTXMultVec(m, &em10_hide_ofs_r, &v);
            {
                f32 dx = v.x - em->pos.x;
                f32 dy = v.y - em->pos.y;
                f32 dz = v.z - em->pos.z;
                if (dx * dx + dy * dy + dz * dz > 1440000.0f) {
                    continue;
                }
            }
            em->rot.y = e->rotY;
            em->xFC = 1;
            em->xFD = 0x17;
            em->xFE = 0;
            em->xFF = 1;
            return 1;
        case 1:
            {
                f32 dx = e->pos.x - em->pos.x;
                f32 dy = e->pos.y - em->pos.y;
                f32 dz = e->pos.z - em->pos.z;
                f32 d = dx * dx + dy * dy + dz * dz;
                if (d > 6250000.0f) {
                    continue;
                }
                if (d < 2250000.0f) {
                    continue;
                }
            }
            ang = Muku(&e->pos, &pPL->pos, e->rotY, 3.1415927f);
            a = fabsf(ang);
            if (a > 1.0471976f) {
                continue;
            }
            {
                f32 dx = e->pos.x - pPL->pos.x;
                f32 dy = e->pos.y - pPL->pos.y;
                f32 dz = e->pos.z - pPL->pos.z;
                if (dx * dx + dy * dy + dz * dz < 25000000.0f) {
                    continue;
                }
            }
            PSMTXRotRad(m, 'y', e->rotY);
            TransMatrix(m, &e->pos);
            PSMTXMultVec(m, &em10_hide_ofs_l, &v);
            {
                f32 dx = v.x - em->pos.x;
                f32 dy = v.y - em->pos.y;
                f32 dz = v.z - em->pos.z;
                if (dx * dx + dy * dy + dz * dz > 1440000.0f) {
                    continue;
                }
            }
            em->rot.y = e->rotY;
            em->xFC = 1;
            em->xFD = 0x17;
            em->xFE = 0;
            em->xFF = 0;
            return 1;
        case 2:
            if (w->wepType != 8) {
                continue;
            }
            {
                f32 dx = e->pos.x - em->pos.x;
                f32 dy = e->pos.y - em->pos.y;
                f32 dz = e->pos.z - em->pos.z;
                if (dx * dx + dy * dy + dz * dz > 2250000.0f) {
                    continue;
                }
            }
            ang = Muku(&e->pos, &pPL->pos, e->rotY, 3.1415927f);
            a = fabsf(ang);
            if (a > 1.0471976f) {
                continue;
            }
            {
                f32 dx = e->pos.x - pPL->pos.x;
                f32 dy = e->pos.y - pPL->pos.y;
                f32 dz = e->pos.z - pPL->pos.z;
                if (dx * dx + dy * dy + dz * dz < 25000000.0f) {
                    continue;
                }
            }
            em->rot.y = e->rotY;
            em->xFC = 1;
            em->xFD = 0x1A;
            em->xFE = 0;
            em->xFF = 0;
            return 1;
        }
    }
    return 0;
}

int em10HideRtnCk2(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;
    EmiEntry* e;
    f32 ang;
    f32 a;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    for (i = 0; i < EMI_DATA->n; i++) {
        e = &EMI_DATA->entry[i];
        if (e->type != 1) {
            continue;
        }
        switch (e->sub) {
        case 0:
            {
                f32 dx = e->pos.x - em->pos.x;
                f32 dy = e->pos.y - em->pos.y;
                f32 dz = e->pos.z - em->pos.z;
                if (dx * dx + dy * dy + dz * dz > 1000000.0f) {
                    continue;
                }
            }
            ang = Muku(&e->pos, &pPL->pos, e->rotY, 3.1415927f);
            a = fabsf(ang);
            if (a > 1.0471976f) {
                continue;
            }
            {
                f32 dx = e->pos.x - pPL->pos.x;
                f32 dy = e->pos.y - pPL->pos.y;
                f32 dz = e->pos.z - pPL->pos.z;
                if (dx * dx + dy * dy + dz * dz < 25000000.0f) {
                    continue;
                }
            }
            em->rot.y = e->rotY;
            em->xFC = 1;
            em->xFD = 0x18;
            em->xFE = 0;
            em->xFF = 0;
            return 1;
        case 1:
            {
                f32 dx = e->pos.x - em->pos.x;
                f32 dy = e->pos.y - em->pos.y;
                f32 dz = e->pos.z - em->pos.z;
                if (dx * dx + dy * dy + dz * dz > 1000000.0f) {
                    continue;
                }
            }
            ang = Muku(&e->pos, &pPL->pos, e->rotY, 3.1415927f);
            a = fabsf(ang);
            if (a > 1.0471976f) {
                continue;
            }
            {
                f32 dx = e->pos.x - pPL->pos.x;
                f32 dy = e->pos.y - pPL->pos.y;
                f32 dz = e->pos.z - pPL->pos.z;
                if (dx * dx + dy * dy + dz * dz < 25000000.0f) {
                    continue;
                }
            }
            em->rot.y = e->rotY;
            em->xFC = 1;
            em->xFD = 0x18;
            em->xFE = 0;
            em->xFF = 1;
            return 1;
        case 2:
            if (w->wepType != 8) {
                continue;
            }
            {
                f32 dx = e->pos.x - em->pos.x;
                f32 dy = e->pos.y - em->pos.y;
                f32 dz = e->pos.z - em->pos.z;
                if (dx * dx + dy * dy + dz * dz > 1000000.0f) {
                    continue;
                }
            }
            ang = Muku(&e->pos, &pPL->pos, e->rotY, 3.1415927f);
            a = fabsf(ang);
            if (a > 1.0471976f) {
                continue;
            }
            {
                f32 dx = e->pos.x - pPL->pos.x;
                f32 dy = e->pos.y - pPL->pos.y;
                f32 dz = e->pos.z - pPL->pos.z;
                if (dx * dx + dy * dy + dz * dz < 25000000.0f) {
                    continue;
                }
            }
            em->rot.y = e->rotY;
            em->xFC = 1;
            em->xFD = 0x1A;
            em->xFE = 0;
            em->xFF = 0;
            return 1;
        }
    }
    return 0;
}

int em10HideToStepCk(cEm10* em, int a)
{
    Vec v;
    Vec p;
    int hit;

    if ((pG->flags_51E4 & 7) != (em->emsetNo & 7)) {
        return 0;
    }
    if (a) {
        v.x = -2000.0f;
        v.y = 2000.0f;
        v.z = 0.0f;
        PSMTXMultVec(em->mat, &v, &v);
    } else {
        v.x = -2000.0f;
        v.y = 2000.0f;
        v.z = 0.0f;
        PSMTXMultVec(em->mat, &v, &v);
    }
    if (fabsf(Muku(&v, &pPL->pos, em->rot.y, 3.1415927f)) > 1.5707964f) {
        return 0;
    }
    {
        f32 dz = v.z - pPL->pos.z;
        f32 dx = v.x - pPL->pos.x;
        if (dx * dx + dz * dz > 169000000.0f) {
            return 0;
        }
    }
    p = pPL->pos;
    p.y += 2000.0f;
    hit = EatMgr.hitCheck(&v, &p, 0, 0, 0, 0);
    if (hit) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x17, hit, a);
    return 1;
}

void em10BowgunMove(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModel* p;

    if (w->pWep && w->wepType == 8) {
        p = w->pWep->getPartsPtr(4);
        p->scale.x = 1.0f;
        p->scale.y = 1.0f;
        p->scale.z = 1.0f;
        p = w->pWep->getPartsPtr(1);
        if (w->x6B5 <= 2) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        } else {
            p->scale.x = 1.0f;
            p->scale.y = 1.0f;
            p->scale.z = 1.0f;
        }
        p = w->pWep->getPartsPtr(2);
        if (w->x6B5 <= 1) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        } else {
            p->scale.x = 1.0f;
            p->scale.y = 1.0f;
            p->scale.z = 1.0f;
        }
        p = w->pWep->getPartsPtr(3);
        if (w->x6B5 == 0) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        } else {
            p->scale.x = 1.0f;
            p->scale.y = 1.0f;
            p->scale.z = 1.0f;
        }
    }
}

extern "C" void em10BehindSeCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (Ctrl12Ck(w->pCtrl12, 0xC)) {
        return;
    }
    if ((s16) w->x67C != 0) {
        return;
    }
    if (em->plDist2 > 9000000.0f) {
        return;
    }
    if (w->x508 > 1.5707964f) {
        return;
    }
    if (w->wepType == 4) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f)) < 1.5707964f) {
        return;
    }
    if (!(w->flags & 1)) {
        return;
    }
    em10CallVoiceSe2(em, w->se6D6, 8);
    Ctrl12Set(w->pCtrl12, 0xC, 300);
    w->x67E = (u8) (Rnd() % 120) + 120;
}

// Damage value of the hit being processed: weapon table value scaled by the Ganado variant, armour and
// the hit part (5 = head: critical rate).
int em10SetDmVal(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    EmHitInfo* part = em->dmPart;
    int far = 0;
    int dmg;
    f32 rate;

    if (part->rad < 36000000.0f) {
        far = 1;
    }
    dmg = 100;
    if (em->dmWep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmWep, far);
    }
    if (w->x6C5 == 1) {
        dmg = (u32) ((f32) dmg * 0.5555556f) + 1;
    }
    if (w->x6C5 == 2) {
        dmg = (u32) ((f32) dmg * 0.45454544f) + 1;
    }
    if (em->type == 2) {
        dmg = (u32) ((f32) dmg * 0.6666667f) + 1;
    }
    if (em->type == 0xA || em->type == 0xD) {
        if (em->dmWep != 0xD && em->dmWep != 0x12) {
            if (em10ArmorCk(em, part->partsNo)) {
                dmg = dmg / 32 + 1;
            } else if (part->partsNo == 0x25) {
                dmg *= 2;
            } else {
                dmg = dmg / 8 + 1;
            }
        }
    } else if (em10ArmorCk(em, part->partsNo)) {
        if (em->dmWep != 0xD && em->dmWep != 0x12) {
            dmg = dmg / 4 + 1;
        }
    } else if (part->partsNo == 5) {
        rate = 1.2f;
        switch (em->dmWep) {
        case 9:
        case 0xA:
            rate = 5.0f;
            break;
        }
        switch (em->dmWep) {
        case 0:
        case 0xE:
        case 0x10:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1F:
        case 0x20:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x2A:
            break;
        default: {
            u8 r = Rnd() % 12;
            if (r == 6) {
                rate = 10.0f;
            }
            break;
        }
        }
        if (pG->wep_no == 2 && pG->wep_lv > 5) {
            u8 r = Rnd() % 10;
            if (r > 4) {
                rate = 10.0f;
            }
        }
        switch (em->dmWep) {
        case 7:
        case 8:
        case 0x21:
            if (far) {
                u8 r = Rnd() % 3;
                if (r == 1) {
                    rate = 10.0f;
                }
            }
            break;
        }
        if (w->pParasite || w->x58C) {
            rate *= 2.0f;
        }
        if (w->wepType == 4) {
            rate = 1.2f;
        }
        if (em->type == 2) {
            rate = 1.2f;
        }
        dmg = (int) ((f32) dmg * rate);
    } else if (w->pParasite || w->x58C) {
        dmg = (u32) ((f32) dmg * 0.6666f) + 1;
    }
    if (em->type == 6) {
        dmg = 0;
    }
    if (w->wepType == 4 && em->dmWep != 0xD && em->dmWep != 0x12 && dmg > 1000) {
        dmg = 1000;
    }
    if (dmg > 9999) {
        dmg = 9999;
    }
    return dmg;
}

extern "C" cModel* em10SearchTruck(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) == 1 && e->id == 0x3B) {
            *(cEm10**) ((u8*) e + 0x64C) = em;  // truck (em3b) work: driver
            w->pTruck = e;
            return (cModel*) 1;
        }
    }
    return 0;
}

extern "C" int em10SearchParasite(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;

    w->x58C = 0;
    for (i = 0; i < EmMgr.nArray; i++) {
        cEmPartner* e = (cEmPartner*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x25) {
            continue;
        }
        if (!e->v50()) {
            continue;
        }
        e->v58(em, 3, 0, 0);
        w->x58C = e;
        return 1;
    }
    return 0;
}

extern "C" void em10ParasiteGoOut(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->x58C) {
        w->x58C->hp = 1000;
        if (w->flags & 0x20) {
            w->x58C->v98(1);
        } else {
            w->x58C->v98(0);
        }
        w->x58C = 0;
    }
}

void em10SetDamageVoice(cEm10* em, int a, int b)
{
    Em10Work* w = EM10_WK(em);

    if (em->hp <= 0) {
        if (!(w->flags & 0x80)) {
            em10CallVoiceSe2(em, b, 8);
        }
        if (em->type != 10 && em->type != 13 && (w->x58C || w->pParasite)) {
            SndStop(w->x5B8, 0);
            SndStop(w->x5BC, 0);
            w->x5B8 = Ctrl11SetSe2(w->pCtrl11, em, Rnd() % 20 + 20, 0x89, 6, 8);
            w->x67E = Rnd() % 120 + 120;
        }
    } else if (!(w->flags & 0x80)) {
        em10CallVoiceSeI(em, a);
    }
}

int em10ChgParasiteCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (!(em->flags_3C8 & 0x100000) || em->hp > 0) {
        return 0;
    }
    if (w->flags & 0x80) {
        return 0;
    }
    if (Ctrl12CntCk(w->pCtrl12, 4, 2)) {
        return 0;
    }
    if (em->type != 8) {
        u8 r = Rnd() % 10;
        if (r > 4) {
            return 0;
        }
    }
    if (!em10LostHead(em, 2, 0)) {
        return 0;
    }
    return 1;
}

void em10SetDmWaterEff(cEm10* em, int a)
{
    Em10Work* w = EM10_WK(em);
    cModel* p;
    s8 wait;

    if (CheckInWater(em, 4)) {
        wait = w->x6A3;
        if (wait == 0) {
            p = em->getPartsPtr(0);
            switch (a) {
            case 0:
            default:
                EstSetEm(em, -1, 0, 0, 1, 0x34, 0, (int) wait, em, 0);
                SndCall(6, 0x11, &p->worldPos, 0, 0, em);
                w->x6A3 = 5;
                break;
            case 1:
                EstSetEm(em, -1, 0, 0, 1, 0x33, 0, (int) wait, em, 0);
                SndCall(6, 0x16, &p->worldPos, 0, 0, em);
                w->x6A3 = 10;
                break;
            }
        }
    }
}

extern "C" void em10SetTakeawayPos(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec best;
    Vec c;
    int found = -1;
    f32 bestAng = 0.0f;
    f32 bestD = 0.0f;
    f32 d;
    f32 ang;
    f32 a;
    u32 i;
    SceAtWork* p;

    // Both loops write the `bestAng < PI/2` and `d < bestD` cases as separate arms with their own
    // copy of the update (jump2 cross-jumps them into the `||` shape): at global-alloc time the extra
    // copy gives `&best` 17 weighted refs (> em's 29/241) and the loop-2 `&c` PRE copy 10, which
    // puts &best above em (r28/r27) and the copy above p (r29/r28).
    if (pG->pRoomEmi) {
        for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
            EmiEntry* e = &((EmiData*) pG->pRoomEmi)->entry[i];
            if (e->type != 5) {
                continue;
            }
            if (e->sub != 0) {
                continue;
            }
            if (e->pad_3 != 0) {
                continue;
            }
            if (!RouteCkConnectPosCk(&em->pos, &e->pos)) {
                continue;
            }
            if (found == -1) {
                found = 1;
                best = e->pos;
                bestD = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) +
                        (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                        (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
                a = GetXZAngle(&em->pos, &e->pos);
                bestAng = fabsf(Muku(&em->pos, &pPL->pos, a, 3.1415927f));
            } else {
                d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) +
                    (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                    (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
                a = GetXZAngle(&em->pos, &e->pos);
                ang = fabsf(Muku(&em->pos, &pPL->pos, a, 3.1415927f));
                if (ang < 1.5707964f && bestAng < ang) {
                    best = e->pos;
                    bestD = d;
                    bestAng = ang;
                } else if (bestAng < 1.5707964f) {
                    best = e->pos;
                    bestD = d;
                    bestAng = ang;
                } else if (d < bestD) {
                    best = e->pos;
                    bestD = d;
                    bestAng = ang;
                }
            }
        }
        if (found != -1) {
            w->x4EC = best;
            return;
        }
    }
    p = sceAtSetOtStart();
    found = -1;
    while ((p = sceAtGetOtAddr(p)) != 0) {
        if (!(p->flag & 1)) {
            continue;
        }
        if (p->x35 != 1) {
            continue;
        }
        AreaGetCenterPos(&c, &p->area);
        if (found == -1) {
            found = 1;
            best = c;
            bestD = (em->pos.x - c.x) * (em->pos.x - c.x) + (em->pos.y - c.y) * (em->pos.y - c.y) +
                    (em->pos.z - c.z) * (em->pos.z - c.z);
            a = GetXZAngle(&em->pos, &c);
            bestAng = fabsf(Muku(&em->pos, &pPL->pos, a, 3.1415927f));
        } else {
            d = (em->pos.x - c.x) * (em->pos.x - c.x) + (em->pos.y - c.y) * (em->pos.y - c.y) +
                (em->pos.z - c.z) * (em->pos.z - c.z);
            a = GetXZAngle(&em->pos, &c);
            ang = fabsf(Muku(&em->pos, &pPL->pos, a, 3.1415927f));
            if (ang < 1.5707964f && bestAng < ang) {
                best = c;
                bestD = d;
                bestAng = ang;
            } else if (bestAng < 1.5707964f) {
                best = c;
                bestD = d;
                bestAng = ang;
            } else if (d < bestD) {
                best = c;
                bestD = d;
                bestAng = ang;
            }
        }
    }
    if (found == -1) {
        w->x4EC.x = 0.0f;
        w->x4EC.y = 0.0f;
        w->x4EC.z = 0.0f;
    } else {
        best.y += 300.0f;
        w->x4EC = best;
    }
}

extern "C" void em10SetTakeawayPosUpdate(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;
    u32 j;
    f32 d;
    f32 dy;

    if (!pGS->pRoomEmi) {
        return;
    }
    for (i = 0; i < ((EmiData*) pGS->pRoomEmi)->n; i++) {
        EmiEntry* e = &((EmiData*) pGS->pRoomEmi)->entry[i];
        if (e->type != 5) {
            continue;
        }
        if (e->sub == 0) {
            continue;
        }
        if (e->sub != 1) {
            continue;
        }
        d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        dy = fabsf(em->pos.y - e->pos.y);
        if (!(d < 16000000.0f && dy < 1000.0f)) {
            continue;
        }
        for (j = 0; j < ((EmiData*) pGS->pRoomEmi)->n; j++) {
            EmiEntry* e2 = &((EmiData*) pGS->pRoomEmi)->entry[j];
            if (e2->type != 5) {
                continue;
            }
            if (e2->sub != 0) {
                continue;
            }
            if (e2->state != e->state) {
                continue;
            }
            w->x4EC = e2->pos;
            return;
        }
    }
}

extern "C" int em10ReturnPosCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    EmiData* emi;
    EmiEntry* e;
    u32 i;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (w->flags & 0x4000) {
        return 0;
    }
    if (em->x3D0 != 1 && em->x3D0 != 3) {
        return 0;
    }
    emi = (EmiData*) pG->pRoomEmi;
    for (i = 0; i < emi->n; i++) {
        f32 dx;
        f32 dy;
        f32 dz;
        e = &emi->entry[i];
        if (e->type != 0xC) {
            continue;
        }
        dx = em->pos.x - e->pos.x;
        dy = em->pos.y - e->pos.y;
        dz = em->pos.z - e->pos.z;
        if (dx * dx + dy * dy + dz * dz > 9000000.0f) {
            continue;
        }
        w->flags |= 0x20000000;
        w->x4F8 = em->pos;
        EmRoutineSet(em, 1, 0x1B, 0, 0);
        return 1;
    }
    return 0;
}

int em10GotoPosCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u32 i;
    u32 j;
    int found;
    u32 cnt;
    EmiEntry* e;
    EmiEntry* f;
    f32 d;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (w->x5EC != 0) {
        return 0;
    }
    for (i = 0; i < EMI_DATA->n; i++) {
        e = &EMI_DATA->entry[i];
        if (e->type != 0xF) {
            continue;
        }
        if (e->sub != 0) {
            continue;
        }
        {
            f32 dx = em->pos.x - e->pos.x;
            f32 dy = em->pos.y - e->pos.y;
            f32 dz = em->pos.z - e->pos.z;
            if (dx * dx + dy * dy + dz * dz > 9000000.0f) {
                continue;
            }
        }
        found = 0;
        for (j = 0; j < EMI_DATA->n; j++) {
            f = &EMI_DATA->entry[j];
            if (f->type != 0xF) {
                continue;
            }
            if (f->sub != 2) {
                continue;
            }
            if (f->pad_3 != e->pad_3) {
                continue;
            }
            {
                f32 dx = f->pos.x - pPL->pos.x;
                f32 dy = f->pos.y - pPL->pos.y;
                f32 dz = f->pos.z - pPL->pos.z;
                d = dx * dx + dy * dy + dz * dz;
                if (d < 9000000.0f) {
                    found = 1;
                    break;
                }
            }
        }
        if (!found) {
            continue;
        }
        cnt = 0;
        for (j = 0; j < EmMgr.nArray; j++) {
            cEm* o = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * j);
            if ((o->be_flag & 0x201) != 1) {
                continue;
            }
            if (o->id <= 0xF) {
                continue;
            }
            if (o->id > 0x20) {
                continue;
            }
            if (o->hp <= 0) {
                continue;
            }
            if (o == em) {
                continue;
            }
            {
                f32 dx = o->pos.x - e->pos.x;
                f32 dy = o->pos.y - e->pos.y;
                f32 dz = o->pos.z - e->pos.z;
                d = dx * dx + dy * dy + dz * dz;
                if (d > 25000000.0f) {
                    continue;
                }
            }
            if (EM_RTN(o, 1, 0x13)) {
                continue;
            }
            cnt++;
        }
        if (cnt < e->state) {
            return 0;
        }
        for (j = 0; j < EMI_DATA->n; j++) {
            f = &EMI_DATA->entry[j];
            if (f->type != 0xF) {
                continue;
            }
            if (f->sub != 1) {
                continue;
            }
            if (f->pad_3 != e->pad_3) {
                continue;
            }
            if (!(em->flags_3C8 & 0x40)) {
                w->flags &= ~0x100;
            }
            em->setGoto(&f->pos, 0xC);
            return 1;
        }
    }
    return 0;
}

void em10SetWaterEff(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x00400000) {
        return;
    }
    if (!CheckInWater(em, 4)) {
        return;
    }
    if (w->x6A1) {
        w->x6A1--;
    } else {
        w->x6A1 = 8;
        EstSet((int) em, -1, 0, 0, 1, 0x30, 0, 0, (u32) em, 0);
    }
    if ((em->pos.x - em->oldPos.x) * (em->pos.x - em->oldPos.x) + (em->pos.z - em->oldPos.z) * (em->pos.z - em->oldPos.z) > 225.0f) {
        if (w->x6A2) {
            w->x6A2--;
        } else {
            w->x6A2 = 5;
            EstSet((int) em, -1, 0, 0, 1, 0x31, 0, 0, (u32) em, 0);
            SndCall(6, 0x11, &em->pos, 0, 0, em);
        }
    }
}

extern "C" int em10ReturnCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (w->flags & 0x20000000) {
        return 0;
    }
    if (em->x3D0 != 1 && em->x3D0 != 3) {
        return 0;
    }
    if (em->hp < em->hpMax / 2) {
        return 0;
    }
    if (w->x524 < 3000.0f) {
        return 0;
    }
    if (w->x52C < em->x3CC) {
        return 0;
    }
    {
        f32 dx = em->pos.x - w->x4D8.x;
        f32 dy = em->pos.y - w->x4D8.y;
        f32 dz = em->pos.z - w->x4D8.z;
        if (dx * dx + dy * dy + dz * dz < 6250000.0f) {
            if (EM_RTN(em, 1, 0x1B)) {
                return 0;
            }
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            return 1;
        }
    }
    w->flags |= 0x20000000;
    w->x4F8 = em->pos;
    EmRoutineSet(em, 1, 0x1B, 0, 0);
    return 1;
}

void em10BombNeckMove(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    cModel* p;
    f32 d;
    f32 r;
    f32 amp;
    Mtx m;

    if (!(w->flags & 0x80)) {
        return;
    }
    if (w->x6C5 != 1) {
        return;
    }
    p = em->getPartsPtr(0x24);
    if ((w->flags & 0x01000020) == 0x01000000 && em->hp <= 0) {
        p->rot.x = p->rot.x * 0.9f + 0.17453294f;
    } else {
        p->rot.x = p->rot.x * 0.9f + -0.06981317f;
    }
    if (em->hp <= 0) {
        return;
    }
    d = sqrtf((p->worldPos.x - p->x88.x) * (p->worldPos.x - p->x88.x) +
              (p->worldPos.y - p->x88.y) * (p->worldPos.y - p->x88.y) +
              (p->worldPos.z - p->x88.z) * (p->worldPos.z - p->x88.z));
    if (d > 25.0f) {
        d = 25.0f;
    }
    r = d * 0.02f + 0.5f;
    amp = r * 0.5235988f;
    PSMTXRotRad(m, 'y', SINF(w->x64C) * amp);
    PSMTXConcat(p->mat, m, p->mat);
    w->x64C = r * 0.17453292f + w->x64C;
    p = em->getPartsPtr(0x1B);
    PSMTXConcat(p->mat, m, p->mat);
    if (w->pParasite) {
        p->scale.x *= 0.9f;
        if (p->scale.x < 0.1f) {
            p->scale.x = 0.1f;
        }
        p->scale.z = p->scale.y = p->scale.x;
    }
}

extern "C" int em10ScreenInCk(cEm10* em)
{
    Vec scr;
    Vec pos;

    pos = em->pos;
    if (GetScreenPos(&pos, &scr) && scr.x > 0.0f && scr.x < 512.0f && scr.y > 0.0f && scr.y < 448.0f) {
        return 1;
    }
    pos = em->getPartsPtr(4)->worldPos;
    if (GetScreenPos(&pos, &scr) && scr.x > 0.0f && scr.x < 512.0f && scr.y > 0.0f && scr.y < 448.0f) {
        return 1;
    }
    return 0;
}

extern "C" int em10ThrowScaCk(cEm10* em)
{
    Vec a;
    Vec b;
    Vec d;

    a.x = em->pos.x;
    a.y = em->pos.y + 1500.0f;
    a.z = em->pos.z;
    b.x = pPL->pos.x;
    b.y = pPL->pos.y + 1500.0f;
    b.z = pPL->pos.z;
    if ((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z) > 25000000.0f) {
        PSVECSubtract(&b, &a, &d);
#line 38900 "D:/Bio4/Prog/em10.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, 5000.0f);
        PSVECAdd(&a, &d, &b);
    }
    return EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x404000) == 0;
}

extern "C" int em10BombThrowScaCk(cEm10* em)
{
    Vec a;
    Vec b;

    a.x = 300.0f;
    a.y = 2000.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 2000.0f;
    b.z = 5000.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x404000)) {
        return 0;
    }
    a.x = -300.0f;
    a.y = 2000.0f;
    a.z = 0.0f;
    b.x = -300.0f;
    b.y = 2000.0f;
    b.z = 5000.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    return EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x404000) == 0;
}

extern "C" void em10SetAtkWait(cEm10* em, int set)
{
    Em10Work* w = EM10_WK(em);
    int t = 30;

    if (pG->x4F88 <= 3) {
        t = 45;
    }
    if (pG->x4F88 <= 1) {
        t = 75;
    }
    w->x67C = t;
    if (set) {
        Ctrl12SetS(w->pCtrl12, 6, (s16) t);
        Ctrl12SetS(w->pCtrl12, 8, (s16) t);
    }
}

int em10IgnitionCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (!(w->flags & 0x100)) {
        return 0;
    }
    if (w->pWep) {
        if (w->wepType == 4 && !(w->flags & 0x80000000)) {
            EmRoutineSet(em, 1, 0xE, 0, 0);
            return 1;
        }
        if (w->pWep && w->wepType == 9 && w->x640 == 0) {
            if (w->x524 < 15000.0f || em->x3D0 == 2) {
                if (w->flags & 1) {
                    if (em->flags_3C8 & 0x10000) {
                        EmRoutineSet(em, 1, 0xF, 0, 0);
                        return 1;
                    }
                    if (em->pos.y > pPL->pos.y - 500.0f) {
                        EmRoutineSet(em, 1, 0xF, 0, 0);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

int em10ClawStickCK(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int r;

    if (em->type != 10 && em->type != 13) {
        return 0;
    }
    // `r` holds the flag test (callee-saved r29, live across the calls) and is reused for the
    // attack-check result; cse writes the zeros of both paths through it.
    r = w->flags & 0x100;
    if (!r) {
        if (!em10FindCk2(em)) {
            return 0;
        }
        em->setFindPL();
        w->x674 = 0;
    }
    if (w->x6BE == 2 && w->x6BF == 2) {
        // `||`: the inner label blocks the `li r3, 0` hoist in front of `bne`
        if (!em10FindCk2(em) || (w->flags & 0x08000000)) {
            return 0;
        }
        r = em10ClawCriAtkCk(em);
        if (r) {
            return 1;
        }
        if ((em->pos.x - w->x534.x) * (em->pos.x - w->x534.x) + (em->pos.z - w->x534.z) * (em->pos.z - w->x534.z) > 16000000.0f &&
            !EM_RTN(em, 1, 0x11)) {
            em10CallVoiceSe2(em, 0x71, 6);
            w->x6AC = 0;
            EmRoutineSet(em, 1, 0x11, 0, 0);
            return 1;
        }
        return 0;
    }
    if (em10ClawCriAtkCk(em)) {
        return 1;
    }
    EmRoutineSet(em, 1, 0x59, 0, 0);
    return 1;
}

int em10FindLostCk(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    if (em->type != 10 && em->type != 13) {
        return 0;
    }
    if (em10FindCk2(em)) {
        w->x654 = 150;
    }
    if (w->x654 != 0) {
        w->x654--;
        if ((s16) w->x654 == 0) {
            EmRoutineSet(em, 1, 0x5D, 0, 0);
            return 1;
        }
    }
    return 0;
}
extern "C" void em10PlHeadLost()
{
    Vec v;
    Vec ofs;
    cObj* o;
    cModel* p;
    int region = pSys->region;

    if (region == 0) {
        PlSetDamageSe(0xD);
        EstSet((int) pPL, -1, 0, 0, 0x10, 0x57, 0, 0, (u32) pPL, 0);
        return;
    }
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
    EstSet((int) pPL, -1, 0, 0, 0x10, 0x45, 0, 0, (u32) pPL, 0);
    pPL->setHead(0);
    p = pPL->getPartsPtr(3);
    if (pG->x4FB8 == 2) {
        v.x = 0.0f;
        v.y = 84.0f;
        v.z = 0.0f;
    } else {
        v.x = 0.0f;
        v.y = 68.0f;
        v.z = 28.0f;
    }
    ofs.x = 0.0f;
    ofs.y = 50.0f;
    ofs.z = -25.0f;
    PSMTXMultVec(p->mat, &v, &v);
    PSMTXMultVecSR(pPL->mat, &ofs, &ofs);
    o = SetObj01(PL_ARC_PTR(pG->pPlArc, 0xC), PL_ARC_PTR(pG->pPlArc, 7), &v, &pPL->rot, &ofs, 10.0f, 150.0f, 1000,
                 0x11);
    if (o) {
        o->lightInfo.x50 = 1;
        Obj01SetEst(o, 0, -1, 4, 0, -1, 0, -1, 0, -1);
    }
    EstSet((int) o, -1, 0, 0, 0x10, 0x46, 0, 0, (u32) o, 0);
}

void em10BombThrow(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    f32 d;
    int fuse;
    Vec spd;

    if (!w->pWep) {
        return;
    }
    if (w->wepType != 9) {
        return;
    }
    d = SQRTF(em->plDist2);
    if (d < 4000.0f) {
        d = 4000.0f;
    }
    fuse = (u8) (Rnd() % 10) + 50;
    if (pPL->pos.y < em->pos.y - 1000.0f) {
        if (pPL->pos.y < em->pos.y - 3000.0f) {
            d *= 0.015384615f;
            fuse = (u8) (Rnd() % 15) + 70;
        } else {
            d *= 0.018181818f;
            fuse = (u8) (Rnd() % 15) + 60;
        }
    } else {
        d *= 0.022222223f;
    }
    spd.x = fRand1_1() * 5.0f + 20.0f;
    spd.y = fRand1_1() * 5.0f + 50.0f;
    spd.z = fRand1_1() * 5.0f + d;
    PSMTXMultVecSR(em->mat, &spd, &spd);
    w->pWep->setBombThrow(&spd, fuse);
    w->pWep = 0;
    w->x640 = 0;
    w->wepType = 0;
    w->x670 = 10;
}

cObjGondola* em10GetGondola(cEm10* em)
{
    cObj* o;

    for (o = ObjMgr.pAlive; o; o = (cObj*) o->next) {
        if (o->id == 0x35 && ((cObjGondola*) o)->ckRide()) {
            return (cObjGondola*) o;
        }
    }
    return 0;
}

static u8 em10_chain_parts[4] = { 1, 2, 3, 4 };
static u8 em10_chain_up[4] = { 0xFF, 1, 2, 3 };
static u8 em10_chain_down[4] = { 2, 3, 4, 0xFF };
PlClothAt em10_chain_at[5] = {
    { 0, 1, 1, 1.0f, 200.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 1, 2, 0.5f, 200.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 2, 2, 1.0f, 200.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 2, 3, 0.5f, 170.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 3, 3, 1.0f, 150.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};

void em10ChainSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;
    Vec rot;

    if (em->type != 10 && em->type != 13) {
        return;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    w->x594 = (cModel*) SetChain(PL_ARC_PTR(em->subArc, 0x225), PL_ARC_PTR(em->subArc, 0x226), &pos, &rot);
    if (!w->x594) {
        pLog->err(0, 0, "EM10 em10ChainSet failed.");
        return;
    }
    w->cloth.x58 = em;
    w->cloth.num = 4;
    w->cloth.pParts = em10_chain_parts;
    w->cloth.x08 = 0;
    w->cloth.x0C = 0;
    w->cloth.x10 = 0;
    w->cloth.x14 = 0;
    w->cloth.pUp = em10_chain_up;
    w->cloth.pDown = em10_chain_down;
    w->cloth.pMax = 0;
    w->cloth.x2C = 0;
    w->cloth.x30 = 0;
    w->cloth.x20 = 0;
    w->cloth.x24 = 0;
    w->cloth.x34 = em10_chain_at;
    w->cloth.x38 = 5;
    w->cloth.x3C = 25.0f;
    w->cloth.x40 = 0.9f;
    w->cloth.x44 = 10;
    w->cloth.x48 = 0.0f;
    w->cloth.x4C = 0.1f;
    w->cloth.x50 = 0.0f;
    w->cloth.flags = 0;
    w->cloth.x54 = 0;
    ((cObjChain*) w->x594)->setChain(&w->cloth);
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    ((cObjChain*) w->x594)->setParent(em, 0x25, &pos, 1);
}

static u8 em10_belt_parts[8] = { 2, 3, 4, 5, 6, 7, 8, 9 };
static u8 em10_belt_up[8] = { 0xFF, 2, 3, 4, 5, 6, 7, 8 };
static u8 em10_belt_down[8] = { 3, 4, 5, 6, 7, 8, 9, 0xFF };
static f32 em10_belt_max[8] = { 0.3f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f };

void em10BeltSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec pos;
    Vec rot;

    if (em->type != 2) {
        return;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    w->x590 = (cModel*) SetChain(PL_ARC_PTR(em->subArc, 0x22D), PL_ARC_PTR(em->subArc, 0x22E), &pos, &rot);
    if (!w->x590) {
        pLog->err(0, 0, "EM10 em10BeltSet failed.");
        return;
    }
    w->cloth.x58 = em;
    w->cloth.num = 8;
    w->cloth.pParts = em10_belt_parts;
    w->cloth.x08 = 0;
    w->cloth.x0C = 0;
    w->cloth.x10 = 0;
    w->cloth.x14 = 0;
    w->cloth.pUp = em10_belt_up;
    w->cloth.pDown = em10_belt_down;
    w->cloth.pMax = em10_belt_max;
    w->cloth.x2C = 0;
    w->cloth.x30 = 0;
    w->cloth.x34 = 0;
    w->cloth.x20 = 0;
    w->cloth.x24 = 0;
    w->cloth.x38 = 0;
    w->cloth.x3C = 25.0f;
    w->cloth.x40 = 0.9f;
    w->cloth.x44 = 10;
    w->cloth.x48 = 0.0f;
    w->cloth.x4C = 0.1f;
    w->cloth.x50 = 0.0f;
    w->cloth.flags = 0;
    w->cloth.x54 = 0;
    ((cObjChain*) w->x590)->setChain(&w->cloth);
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    ((cObjChain*) w->x590)->setParent(em, 0x23, &pos, 1);
}
#undef EM10_ACC_OBJ12
#undef EM10_ACC_PARTS

// Moves one claw part towards its target position / scale, at most `spd` per frame.
#define EM10_CLAW_PART_MOVE(dst, tgt, spd, line)                                                   \
    PSVECSubtract(&tgt, &dst, &d);                                                                 \
    if (d.x * d.x + d.y * d.y + d.z * d.z < spd * spd) {                                           \
        dst = tgt;                                                                                 \
    } else {                                                                                       \
        VECNormalize(&d, &d);                                                                      \
        PSVECScale(&d, &d, spd);                                                                   \
        PSVECAdd(&dst, &d, &dst);                                                                  \
    }

// Claw Ganado (types 0xA / 0xD): extends / retracts the claw parts 0x22 / 0x23 (x6BE / x6BF drive
// the right / left claw state: 0 retracted, 1..2 extending, 3..4 retracting).
void em10ClawMove(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    Vec lPos;
    Vec rPos;
    Vec lScl;
    Vec rScl;
    Vec d;
    cModel* part;
    cModel* part2;
    f32 spd = 0.0f;
    f32 spd2 = spd;
    int snd = 0;

    if (em->type != 0xA && em->type != 0xD) {
        return;
    }
    switch (w->x6BE) {
    case 0:
        rPos.x = 160.0f;
        rPos.y = 0.0f;
        rPos.z = -120.0f;
        rScl.x = 0.4f;
        rScl.y = 1.0f;
        rScl.z = 0.6f;
        spd = 999.0f;
        spd2 = 1.0f;
        w->x6BE = 4;
        break;
    case 1:
        SndCall(6, 0x6C, &em->getPartsPtr(0)->worldPos, 0, 0, em);
        snd = 1;
        EstSet((int) em, -1, 0, 0, 0x10, 0x75, 0, 0, (u32) em, 0);
        w->x6BE++;
    case 2:
        rPos.x = 359.22f;
        rPos.y = 0.0f;
        rPos.z = -106.02f;
        rScl.x = 1.0f;
        rScl.y = 1.0f;
        rScl.z = 1.0f;
        spd = 50.0f;
        spd2 = 0.1f;
        break;
    case 3:
        SndCall(6, 0x6D, &em->getPartsPtr(0)->worldPos, 0, 0, em);
        snd = 1;
        EstSet((int) em, -1, 0, 0, 0x10, 0x7B, 0, 0, (u32) em, 0);
        w->x6BE++;
    case 4:
        rPos.x = 160.0f;
        rPos.y = 0.0f;
        rPos.z = -120.0f;
        rScl.x = 0.4f;
        rScl.y = 1.0f;
        rScl.z = 0.6f;
        spd = 50.0f;
        spd2 = 0.1f;
        break;
    }
    switch (w->x6BF) {
    case 0:
        lPos.x = -160.0f;
        lPos.y = 0.0f;
        lPos.z = -120.0f;
        lScl.x = 0.4f;
        lScl.y = 1.0f;
        lScl.z = 0.6f;
        spd = 999.0f;
        spd2 = 1.0f;
        w->x6BF = 4;
        break;
    case 1:
        if (!snd) {
            SndCall(6, 0x6C, &em->getPartsPtr(0)->worldPos, 0, 0, em);
        }
        EstSet((int) em, -1, 0, 0, 0x10, 0x71, 0, 0, (u32) em, 0);
        w->x6BF++;
    case 2:
        lPos.x = -359.22f;
        lPos.y = 0.0f;
        lPos.z = -106.02f;
        lScl.x = 1.0f;
        lScl.y = 1.0f;
        lScl.z = 1.0f;
        spd = 50.0f;
        spd2 = 0.1f;
        break;
    case 3:
        if (!snd) {
            SndCall(6, 0x6D, &em->getPartsPtr(0)->worldPos, 0, 0, em);
        }
        EstSet((int) em, -1, 0, 0, 0x10, 0x7A, 0, 0, (u32) em, 0);
        w->x6BF++;
    case 4:
        lPos.x = -160.0f;
        lPos.y = 0.0f;
        lPos.z = -120.0f;
        lScl.x = 0.4f;
        lScl.y = 1.0f;
        lScl.z = 0.6f;
        spd = 50.0f;
        spd2 = 0.1f;
        break;
    }
    part = em->getPartsPtr(0x22);
#line 39585 "D:/Bio4/Prog/em10.cpp"
    EM10_CLAW_PART_MOVE(part->pos, lPos, spd, 0);
#line 39594 "D:/Bio4/Prog/em10.cpp"
    EM10_CLAW_PART_MOVE(part->scale, lScl, spd2, 0);
    part2 = em->getPartsPtr(0x23);
#line 39606 "D:/Bio4/Prog/em10.cpp"
    EM10_CLAW_PART_MOVE(part2->pos, rPos, spd, 0);
#line 39615 "D:/Bio4/Prog/em10.cpp"
    EM10_CLAW_PART_MOVE(part2->scale, rScl, spd2, 0);
}

int em10ArmorCk(cEm10* em, int parts)
{
    Em10Work* w = EM10_WK(em);

    if ((em->flags_3C8 & 0x200) && w->x6C5 == 1 && parts == 5 && !w->pParasite) {
        return 1;
    }
    if ((em->flags_3C8 & 0x200) && w->x6C5 == 2 && parts == 5 && !w->pParasite) {
        return 1;
    }
    // `default: return 0;` first: its block is laid out right after the type tree, so each inner
    // switch's `default: return 0;` is directly followed by its own `return 1` and jump.c turns
    // `beq L1; li r3,0; b RET; L1: li r3,1` into `li r3,0; bnelr; L1:` (jump2 then merges case 10's
    // `li r3,1` tail into case 13's).
    switch (em->type) {
    default:
        return 0;
    case 10:
        switch ((u32) parts) {
        case 3:
        case 9:
        case 15:
        case 20:
        case 24:
            break;
        default:
            return 0;
        }
        return 1;
    case 13:
        if (parts == 0x25) {
            return 0;
        }
        return 1;
    case 24:
        switch ((u32) parts) {
        case 2:
        case 8:
        case 14:
        case 20:
        case 24:
            break;
        default:
            return 0;
        }
        return 1;
    }
}

void em10CoreBreak(cEm10* em, int a)
{
    Em10Work* w = EM10_WK(em);
    u32 i;

    if (!w->pParasite) {
        return;
    }
    if (a != 2 && (em->type == 10 || em->type == 13)) {
        return;
    }
    EffectEspDelete(0, w->x6A0, (u32) w->pParasite, 0);
    EffectEspgenDelete(0, w->x6A0, (int) w->pParasite);
    EffectEfmDelete(0, w->x6A0, (int) w->pParasite);
    SndStop(w->x5B8, 0);
    SndStop(w->x5BC, 0);
    if (a) {
        w->pParasite->clearLostWait();
        w->pParasite = 0;
        for (i = 0; i <= 4; i++) {
            if (w->x578[i]) {
                ((cObj16*) w->x578[i])->clearLostWait();
                w->x578[i] = 0;
            }
        }
        return;
    }
    w->x5B8 = Ctrl11SetSe2(w->pCtrl11, em, Rnd() % 20 + 20, 0x89, 6, 8);
    w->x67E = Rnd() % 120 + 120;
    if (w->x6C5 == 1) {
        EstSet(0, -1, &w->pParasite->getPartsPtr(9)->worldPos, 0, 0x10, 0x80, 0, 0, a, (void*) a);
    } else {
        EstSet(0, -1, &em->getPartsPtr(3)->worldPos, 0, 0x10, 0x27, 0, 0, a, (void*) a);
    }
    w->pParasite->clearLostWait();
    w->pParasite = 0;
    for (i = 0; i <= 4; i++) {
        if (w->x578[i]) {
            ((cObj16*) w->x578[i])->clearLostWait();
            w->x578[i] = 0;
        }
    }
}

void cEm10::setWeapon(void* bin, void* tpl, int type)
{
    Em10Work* w = EM10_WK(this);
    Vec pos;
    Vec rot;

    if (!w->pWep) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pWep = SetWeapon(bin, tpl, &pos, &rot, 0);
        if (w->pWep) {
            w->wepType = type;
            em10WepSeEffSet(this, w->pWep, w->wepType);
            em10WeaponSet(this);
        }
    }
}

int cEm10::ckWeapon()
{
    return EM10_WK(this)->pWep != 0;
}

int cEm10::ckTakeAway()
{
    if (EM10_WK(this)->flags & 0x4000) {
        return 1;
    }
    return 0;
}

int cEm10::ckR305BomberEnable()
{
    if (xFC != 1) {
        return 0;
    }
    if (xFD != 0x6A) {
        return 0;
    }
    return xFE == 1;
}

extern "C" void em10FallWaterCk(cEm10* em)
{
    Vec hit;
    u32 attr;
    AtEffInfo* info;

    attr = EatMgr.hitCheck(&em->oldPos, &em->pos, &hit, 0, 0, 0);
    if (!attr) {
        return;
    }
    info = EatMgr.getEffInfo(EatGetEffectType(attr));
    if (!info) {
        return;
    }
    if (info->flags & 1) {
        if (pG->room_id == 0x311) {
            EstSet(0, -1, &em->pos, 0, 1, 3, 0, 0, 0, 0);
            SndCall(6, 0xA, &em->pos, 0, 0, em);
        } else {
            EstSetEm10WaterFall((Vec*) em);
            SndCall(6, 0x16, &em->pos, 0, 0, em);
        }
    }
}

extern "C" f32 em10GetPower(cEm10* em)
{
    f32 p = 1.0f;

    switch (em->type) {
    default:
        p = 1.0f;
        break;
    case 2:
        p = 1.0f;
        break;
    case 7:
        p = 1.1f;
        break;
    case 8:
        p = 1.5f;
        break;
    case 9:
        p = 1.3f;
        break;
    case 10:
        p = 1.0f;
        break;
    case 13:
        p = 1.0f;
        break;
    case 14:
        p = 1.6f;
        break;
    case 15:
        p = 1.6f;
        break;
    case 16:
        p = 1.6f;
        break;
    case 17:
        p = 1.6f;
        break;
    case 18:
        p = 1.6f;
        break;
    case 19:
        p = 1.6f;
        break;
    case 20:
        p = 1.6f;
        break;
    case 21:
        p = 1.6f;
        break;
    case 22:
        p = 1.0f;
        break;
    case 23:
        p = 1.6f;
        break;
    case 24:
        p = 1.6f;
        break;
    case 25:
        p = 1.6f;
        break;
    }
    return p;
}

void em10GatlingRollMove(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    u8 on;

    if (em->type != 2) {
        return;
    }
    on = w->x6A6;
    if (on) {
        if (w->x6A8 == 0) {
            w->x6A8 = SndCall(6, 0x24, &em->pos, 0, 0, em);
        }
        em->getPartsPtr(0x22)->rot.x += 0.34906584f;
    } else if (w->x6A8) {
        SndStop(w->x6A8, 0);
        SndCall(6, 0x25, &em->pos, 0, 0, em);
        w->x6A8 = on;
    }
    w->x6A6 = 0;
}

static u8 em1f_cloth_parts[6] = { 0x22, 0x23, 0x24, 0x25, 0x26, 0x27 };
static u8 em1f_cloth_up[6] = { 0xFF, 0x22, 0x23, 0x24, 0x25, 0x26 };
static u8 em1f_cloth_down[6] = { 0x23, 0x24, 0x25, 0x26, 0x27, 0xFF };
static f32 em1f_cloth_max[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
PlClothAt em1f_cloth_at[1] = {
    { 0, 1, 1, 1.0f, 250.0f, { 0.0f, 0.0f, 100.0f }, { 0.0f, 0.0f, 0.0f } },
};

void Em1fClothSet(cModel* m, PlCloth* c)
{
    c->num = 6;
    c->pParts = em1f_cloth_parts;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em1f_cloth_up;
    c->pDown = em1f_cloth_down;
    c->pWindS = 0;
    c->pWindR = 0;
    c->x20 = 0;
    c->pRate = 0;
    c->pMax = em1f_cloth_max;
    c->pAt = em1f_cloth_at;
    c->nAt = 1;
    c->x3C = 20.0f;
    c->x40 = 0.1f;
    c->x44 = 4;
    c->pModel = m; // between the two 0.0f stores: weight-0 stores (x48's 0.0 is not the constant's last use) go in LUID order
    c->x48 = 0.0f;
    c->x4C = 0.05f;
    c->x50 = 0.0f;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(m, (PenCloth*) c, 100.0f);
}

void Em1fClothMove(cModel* m, PlCloth* c)
{
    PenClothMove(m, (PenCloth*) c);
    m->be_flag &= ~0xE00000;
}

void em10SetPoint(cEm10* em)
{
    Em10Work* w = EM10_WK(em);
    int pt;

    if (w->x6C4) {
        return;
    }
    w->x6C4 = 1;
    switch (em->type) {
    case 0:
        pt = 0;
        break;
    case 1:
        pt = 0;
        break;
    case 2:
        pt = 8;
        break;
    case 3:
        pt = 0;
        break;
    case 4:
        pt = 0;
        break;
    default:
        pt = 0;
        break;
    case 5:
        pt = 4;
        break;
    case 6:
        pt = 4;
        break;
    case 7:
        pt = 3;
        break;
    case 8:
        pt = 4;
        break;
    case 9:
        pt = 3;
        break;
    case 0xa:
        pt = 5;
        break;
    case 0xb:
        pt = 1;
        break;
    case 0xc:
        pt = 1;
        break;
    case 0xd:
        pt = 5;
        break;
    case 0xe:
        pt = 6;
        break;
    case 0xf:
        pt = 6;
        break;
    case 0x10:
        pt = 6;
        break;
    case 0x11:
        pt = 6;
        break;
    case 0x12:
        pt = 6;
        break;
    case 0x13:
        pt = 6;
        break;
    case 0x14:
        pt = 6;
        break;
    case 0x15:
        pt = 6;
        break;
    case 0x16:
        pt = 2;
        break;
    case 0x17:
        pt = 6;
        break;
    case 0x18:
        pt = 6;
        break;
    case 0x19:
        pt = 6;
        break;
    }
    if (em->flags_3C8 & 0x10000000) {
        pt = 2;
    }
    MercSysSetPoint(pt, 0);
}
#undef EM10_CLAW_PART_MOVE

// The original em10.cpp object has an 8-aligned .data (the split object's sh_addralign); the size is already
// a multiple of 8, so this only raises the section alignment (the REL places .data at +0x460D0, not +0x460CC).
asm(".section .data\n\t.balign 8\n\t.text");
