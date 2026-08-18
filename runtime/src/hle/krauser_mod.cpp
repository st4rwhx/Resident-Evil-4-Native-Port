// Mod « Krauser jouable » : le Krauser joueur des Mercenaires (pl0a, x4FB8 = 4) recoit les coups du
// Krauser boss (em39). Recherches et plan : docs/MOD_KRAUSER_2026-10-19.md.
//
// Les deux Krauser ont le meme squelette (127 os, memes parents : mesure du 19/10), les animations
// du boss se posent donc telles quelles sur le joueur. On ne reprend aucun code du boss : on lit ses
// animations sur le disque du joueur et on les joue avec les fonctions du jeu (MotionSetCore,
// MotionMove), les coups touchent les ennemis par la frappe du joueur (PlWepHitCheck2), comme le
// bras mute des Mercenaires.
//
// Donnees : em/em39.drs (conteneur, corps = archive du boss, mot n = decalage de l'entree n). Les
// paires (animation, sequence) utilisees ci-dessous font 0xB5240 octets : copiees au debut de
// chaque partie dans un tampon du tas du jeu (MemAlloc, le tas de Game.pBuf), a la meme
// numerotation (ofs[n]) ; le reste de l'archive (3,7 Mo) n'est jamais charge.
//
// Accroches :
//   cPlayer::move (0x800579B4) : avant le jeu, lit la manette ; si un coup part, routine 0/0xA ;
//   pl_R1_Aux (0x80059D10)     : la routine 0/0xA saute dans pAuxFunc ; pendant un de nos coups on
//                                joue notre etat a la place (pAuxFunc reste celui du jeu).
// Rien ne tourne hors de Krauser en histoire (room_init) ni sans le tampon.

#include "krauser_mod.h"
#include "runtime/env.h"
#include "krauser_plate.inc"

#include <cstdlib>
#include "runtime/game_version.h"
#include "runtime/log.h"
#include "runtime/obf.h"
#include "runtime/virtual_disc.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using nj::runtime::GuestCpu;
using nj::runtime::pg;
using namespace nj::obf_litteraux;
using GuestFn = void (*)(GuestCpu&);

extern "C" {
void nj_dispatch_one(GuestCpu& cpu);   // main_output.cpp (recompilateur)
}

namespace {

constexpr nj::runtime::GameAddr kPG {0x80314BC8};        // Global* pG
constexpr nj::runtime::GameAddr kPPL {0x803159F4};       // cPlayer* pPL
constexpr nj::runtime::GameAddr kJoy {0x802E8B30};       // JOY Joy[0]
constexpr nj::runtime::GameAddr kActBtn {0x802D7B04};    // cActionButton ActBtn
constexpr nj::runtime::GameAddr kFnMemAlloc {0x80154A70};
constexpr nj::runtime::GameAddr kFnMotionSetCore {0x8015D088};
constexpr nj::runtime::GameAddr kFnMotionMove {0x8015DA98};
constexpr nj::runtime::GameAddr kFnGetParts {0x8015C878};
constexpr nj::runtime::GameAddr kFnHitCheck {0x80179024};   // PlWepHitCheck2
constexpr nj::runtime::GameAddr kFnSndCall {0x8005C698};
constexpr nj::runtime::GameAddr kTransOfs {0x8025F394};
constexpr nj::runtime::GameAddr kEmFileTbl {0x8026AFE0}, kEmFileTblKlauser {0x8026B5E0};   // read.cpp : fichiers d'ennemis (64 x 8 octets)
        // g_transOfs[7][2][3] (QFPS_OFFSET, 0x2C octets)
constexpr uint32_t kTransSet = 6 * 0x2C;                          // un tableau d'epaule (2 cotes x 3 hauteurs)
constexpr nj::runtime::GameAddr kModInfoMgr {0x802E982C};      // cModInfoMgr ModInfoMgr
constexpr nj::runtime::GameAddr kFnEspLoad {0x800EB7C8};        // EspDataLoad(data, owner, multipleOK)
constexpr nj::runtime::GameAddr kFnEstSet {0x8012BDA0};         // EstSet(model, -1, pos, rot, owner, no, 0, kind, parent, 0)
constexpr int kOwnerEm39 = 47;                                   // EFF_EM39 (esp.h)
constexpr nj::runtime::GameAddr kFnEvtSetBin {0x80139BD8};      // EventMgr::SetBin(this, name, data, dat2, flag)
constexpr nj::runtime::GameAddr kEvtMgr {0x802DFA1C};            // EventMgr EvtMgr
constexpr nj::runtime::GameAddr kFnDvdReadN {0x800E7884};       // DvdReadN(name, dst, aram, ofs, size, mode, file, line)
constexpr nj::runtime::GameAddr kFnDvdCheck {0x800E9E84};       // cDvd::ReadCheck(this, id, info)
constexpr nj::runtime::GameAddr kDvd {0x802DA1CC};              // cDvd Dvd
constexpr nj::runtime::GameAddr kIdSys {0x802E020C};            // IDSystem IdSys
constexpr nj::runtime::GameAddr kFnIdUnit {0x80149174};         // IDSystem::unitPtr(this, markNo, classNo)
constexpr nj::runtime::GameAddr kFnMotionMoveCore {0x8015E34C};  // MotionMoveCore(model, motion, 0)
constexpr nj::runtime::GameAddr kFnMotionSeqCtrl {0x8015F468};   // MotionSequenceCtrl(motion) : non nul en fin de sequence
constexpr nj::runtime::GameAddr kFnModCreate {0x8015B838};      // cModInfoMgr::create(this, bin, tpl)
constexpr nj::runtime::GameAddr kFnAddModel {0x8015A38C};       // cModel::addModel(this, info)
constexpr uint32_t kReturn = 0xFFFFFFF0;

// cModel / cPlayer (include/model.h, em.h), identiques dans les trois versions
constexpr uint32_t oMat = 0x0C, oWorldPos = 0x70, oOldWorldPos = 0x7C, oPos = 0x94, oRotY = 0xA4;
constexpr uint32_t oFC = 0xFC, oFD = 0xFD, oFE = 0xFE, oFF = 0xFF;
constexpr uint32_t oMot = 0x1D8, oSeNo = 0x28A, oSeFlags = 0x28B, oFrame = 0x290, oFrameMax = 0x294;
// JOY (include/joy.h)
constexpr uint32_t oSx = 0x00, oSy = 0x01, oCx = 0x02, oCy = 0x03, oOn = 0x10, oTrg = 0x14;
constexpr uint32_t kA = 0x100, kB = 0x200, kX = 0x400, kR = 0x20, kL = 0x40, kZ = 0x10, kY = 0x800;
// Global : flags_5018 bit 23 = Krauser mute (bras), decalage debug traduit par pg()
constexpr uint32_t kMutated = 0x00800000;

// Routines du joueur (player.cpp Pl_func_move_tbl) : xFC 0, xFD :
constexpr uint8_t kFootwork = 0, kWalk = 1, kBack = 2, kRun = 3, kAux = 0xA, kKnife = 0xB;

// --------------------------------------------------------------------------- coups
enum Hit : uint8_t { HitNone, HitArm, HitFoot, HitKnife, HitBody };

struct Stage {
    uint16_t mot, seq;   // entrees de l'archive em39 (seq 0 = sans sequence)
    uint8_t hokan;       // images de fondu avec la pose precedente
    uint8_t frame;       // image de depart
    uint8_t fx1, fx2;    // effets d'em39 (EFF_EM39) lances au debut de l'etape : le flou des esquives, les traces des coups
    uint8_t voice;       // voix d'em39 (banque de sons du boss) au debut de l'etape
};

struct Move {
    const char* name;
    Stage stage[5];
    uint8_t stages;
    Hit hit;
    uint8_t type;        // type d'arme de PlWepHitCheck2 (Dmg_tbl_em10 : 0x2D 2500, 0x15 500, 0x14 200, 0x10 100)
    float reach;         // rayon (types a sphere) ou longueur
    float dash;          // avance par image pendant la fenetre (bit 1 de la sequence)
    bool chain;          // A pendant le coup enchaine l'etape suivante (sinon : toutes les etapes a la suite)
    bool dodge = false;  // esquive : invulnerable jusqu'au repere de sequence 4 (Be_flg 0x100 chez le boss), sans braquage
    uint8_t follow = 0;  // coup qui suit (A pendant le coup, s'il a touche) : le coup final apres le balayage
};

enum MoveId : uint8_t {
    MvNone, MvSuperDash, MvBackjump, MvStepL, MvStepR, MvArm, MvBackKnuckle, MvLongArm, MvJumpArm,
    MvKick, MvLowKick, MvKnife, MvKnife4, MvThreat, MvGuard,
    MvStepF, MvSlantL, MvSlantR, MvSlant2L, MvSlant2R, MvRollL, MvRollR, MvSlam, MvGrab, MvCatch, MvCatchL, MvCatchR,
    MvTransform, MvRevert, MvCount
};

// Animations relevees dans le decompile (em39.cpp, une routine par coup) ; deroule a nous.
const Move kMoves[MvCount] = {
    {"", {}, 0, HitNone, 0, 0, 0, false},
    {"super ruee", {{0xD9, 0xDA, 3, 0, 0xC}}, 1, HitBody, 0x14, 1100.0f, 160.0f, false},
    {"saut arriere", {{0x42, 0x43, 3, 3, 0x14}}, 1, HitNone, 0, 0, 0, false, true},
    {"pas gauche", {{0x6D, 0x6E, 3, 0, 0xA}}, 1, HitNone, 0, 0, 0, false, true},
    {"pas droit", {{0x6F, 0x70, 3, 0, 0xB}}, 1, HitNone, 0, 0, 0, false, true},
    {"bras", {{0xDB, 0xDC, 3, 0, 0x30}}, 1, HitArm, 0x2D, 700.0f, 0, false},
    {"revers", {{0xDD, 0xDE, 3, 0, 0x31}}, 1, HitArm, 0x2D, 700.0f, 0, false},
    {"bras allonge", {{0xDF, 0xE0, 3, 0, 0x32}}, 1, HitArm, 0x2D, 800.0f, 0, false},
    {"saut bras", {{0xE5, 0xE6, 3, 0, 0x3B, 0x3C}}, 1, HitArm, 0x2D, 900.0f, 0, false},
    {"coup de pied", {{0xE1, 0xE2, 3, 0, 0x2F}}, 1, HitFoot, 0x15, 1600.0f, 0, false},
    {"balayage", {{0xE3, 0xE4, 3, 0, 0x35}}, 1, HitFoot, 0x15, 1600.0f, 0, false, false, MvSlam},
    {"couteau", {{0xA1, 0xA2, 3, 0, 0x17, 0, 0x19}, {0xA3, 0xA4, 3, 0, 0x18, 0, 0x19}}, 2, HitKnife, 0x10, 6000.0f, 0, true},
    {"couteau x4", {{0xBA, 0xBB, 5, 0, 0x1D}, {0xBC, 0xBD, 0, 0, 0x1E, 0x1F, 0x19}, {0xBE, 0xBF, 0, 0, 0x20, 0x21, 0x19},
                    {0xC0, 0xC1, 0, 0, 0x22, 0x23, 0x19}, {0xB4, 0xB5, 0, 0}},
     5, HitKnife, 0x10, 6000.0f, 0, false},
    {"provocation", {{0x6C, 0, 5, 0}}, 1, HitNone, 0, 0, 0, false},
    // Garde de la forme mutante : le bras couvre la tete (em39 Dm_T_Down, 90 images). Tant que Z est tenu
    // le geste reprend ; les coups recus de face sont annules (cPlayer::setDamage, nj_kr_PlDamage).
    {"garde", {{0xF7, 0xF8, 3, 0}}, 1, HitNone, 0, 0, 0, false},
    // Esquives rapides du boss (em39 Step, Slant, Slant2, Escape), chacune avec son flou.
    {"pas avant", {{0x71, 0x72, 3, 0, 0xC}}, 1, HitNone, 0, 0, 0, false, true},
    {"esquive diagonale gauche", {{0x66, 0x67, 3, 0, 0xA}}, 1, HitNone, 0, 0, 0, false, true},
    {"esquive diagonale droite", {{0x64, 0x65, 3, 0, 0xB}}, 1, HitNone, 0, 0, 0, false, true},
    {"esquive mutante gauche", {{0xFD, 0xFE, 3, 0, 0x41}}, 1, HitNone, 0, 0, 0, false, true},
    {"esquive mutante droite", {{0xFB, 0xFC, 3, 0, 0x40}}, 1, HitNone, 0, 0, 0, false, true},
    {"roulade gauche", {{0x46, 0x47, 3, 1, 0xE}}, 1, HitNone, 0, 0, 0, false, true},
    {"roulade droite", {{0x44, 0x45, 3, 1, 0xD}}, 1, HitNone, 0, 0, 0, false, true},
    // Coup final apres le balayage (em39 T_LowKickHit) : le bras retombe sur l'ennemi a terre.
    {"coup final", {{0xE9, 0xEA, 0, 0, 0x36, 0, 0x23}}, 1, HitArm, 0x2D, 1000.0f, 0, false},
    // Prise (em39 T_CliffAtk : le boss tient sa victime au-dessus du vide) : appliquee a l'ennemi de face.
    {"prise", {{0xED, 0xEE, 0, 0, 0x38, 0, 0x52}}, 1, HitArm, 0x2D, 1000.0f, 0, false},
    // Prise au couteau (em39 KnifeCatch) : la fente vers la victime, droit devant / a gauche / a droite (voix 0x19).
    // Chez le boss la suite tient la victime (EmCatchPLSet) : sur un ennemi, la fente est le coup.
    {"estoc", {{0xA9, 0xAA, 3, 0, 0, 0, 0x19}}, 1, HitKnife, 0x10, 6000.0f, 0, false},
    {"estoc gauche", {{0xA7, 0xA8, 3, 0, 0, 0, 0x19}}, 1, HitKnife, 0x10, 6000.0f, 0, false},
    {"estoc droite", {{0xA5, 0xA6, 3, 0, 0, 0, 0x19}}, 1, HitKnife, 0x10, 6000.0f, 0, false},
    // Passage d'une forme a l'autre (X) : le boss n'a pas d'animation de transformation (em39->type est fixe a la
    // creation, deux combats distincts) ; le geste est sa provocation, et la forme change a mi-course (step).
    {"transformation", {{0x6C, 0, 5, 0}}, 1, HitNone, 0, 0, 0, false},
    {"retour a la forme normale", {{0x6C, 0, 5, 0}}, 1, HitNone, 0, 0, 0, false},
};

constexpr uint32_t oPlayerArc = 0x50;   // Global::pPlayer (avant 0x4F90 : pas de decalage par version)

// Parties du modele (squelette de Krauser, 127 os) : main droite 0xA, main gauche 0x10, lame du
// bras mute 0x5F..0x62, pied 0x13 (em39 : em39AtkCk / em39LeftArmAtkCk / coups de pied).
constexpr int kArmParts[] = {0x10, 0x5F, 0x60, 0x61, 0x62};
constexpr int kFootPart = 0x13, kKnifePart = 0xA, kRootPart = 0;

// Modele du boss. Le squelette du boss est ~3 % plus grand que celui de pl0a (hanches 1174 contre 1140,
// tete 1699 contre 1650) : toute piece de pl0a posee sur un squelette du boss serait decalee (la tete
// de pl0a differe de celle du boss jusqu'a 55 unites). Donc TOUT vient d'em39 :
//  - emplacements de l'archive du joueur (pl0a setModel) qui prennent une piece du boss ;
//  - pieces ajoutees par nous (kPart), montrees ou cachees selon la forme (normale / mutante) ;
//  - ce qui reste de pl0a (bras normal, bras mute, lueur) est remplace par une piece vide.
struct ModelSwap { int bin_slot, tpl_slot, bin_no, tpl_no; };
constexpr int kDummy = 0x101;   // entree synthetique : piece sans triangle visible
constexpr ModelSwap kModel[] = {
    {0x4, 0x5, 0x05, 0x10},     // corps (bas du corps), palette 10
    {0x6, 0x7, 0x06, 0x18},     // tete du boss
    {0x8, 0, kDummy, 0},        // bras normal de pl0a : remplace par les pieces du boss
    {0xA, 0, kDummy, 0},        // bras mute de pl0a
    {0xE, 0, 0x26, 0},          // couteau tenu en main du boss ; la palette (emplacement 9) reste celle qui marchait (1d469d4 l'avait changee : couteau disparu)
    {0xF, 0x10, 0x09, 0x10},    // couteau au flanc (etui) du boss
    {0x12, 0, 0x0A, 0},         // main droite
    {0x14, 0, 0x0E, 0},         // main gauche ouverte
    {0x15, 0, 0x0F, 0},         // main gauche fermee
    {0x18, 0, kDummy, 0},       // lueur de pl0a (positionnee pour le squelette de pl0a)
};
// Reactions aux degats : le joueur lit ses animations de coup recu par numero d'emplacement de son archive
// (pl_dmg.cpp : 0x48 face, 0x49 dos, 0x4A+0x64 et 0x4B+0x65 coups forts, 0x4C+0x4D mort, 0x4E+0x66 et 0x51+0x67
// projection, 0x4F+0x50 relevement, 0x52 souffle). On y met celles du boss (Dm_Normal 056 / 058 / 05C, Dm_Blow
// 05A / 05E / 079, relevement 077, mort E7). no = 0 : pas de table d'evenements (pointeur nul, `ofs + archive`).
struct SlotSwap { int slot, no; };
constexpr SlotSwap kDmgSlots[] = {
    {0x48, 0x56}, {0x49, 0x5C}, {0x4A, 0x58}, {0x64, 0x59}, {0x4B, 0x5C}, {0x65, 0x5D},
    {0x4C, 0xE7}, {0x4D, 0},
    {0x4E, 0x5E}, {0x66, 0x5F}, {0x51, 0x5A}, {0x67, 0x5B}, {0x4F, 0x77}, {0x50, 0x78}, {0x52, 0x79},
};

// Pieces ajoutees au joueur (ModInfoMgr.create + addModel, comme em39 le fait pour ses pieces) :
// forme 1 = le boss du premier combat (beret, veste), forme 2 = le boss mute (bras, lame).
struct Part { int bin_no, tpl_no, form; };
constexpr Part kPart[] = {
    {0x15, 0x17, 1}, {0x07, 0x12, 1}, {0x16, 0x19, 1},   // torse, harnais, beret
    {0x1A, 0x1D, 2}, {0x1B, 0x1F, 2}, {0x1C, 0x20, 2},   // torse mute, bras mute, lame
};
constexpr int kNumParts = int(sizeof(kPart) / sizeof(kPart[0]));
uint32_t g_part_info[kNumParts] = {};
bool g_parts_done = false;
constexpr int kCapBin = 0x16, kCapTpl = 0x19;
// Cinematiques : les evenements de Leon lisent son corps / sa tete par nom (event.cpp ExeBeginEvt,
// seulement pour pl_type 0). Pour Krauser on enregistre les memes noms, sur les pieces de pl00.drs de
// Leon lues sur le disque du joueur (entrees synthetiques 0x110.. du paquet).
struct LeonBin { int pl00_no, slot; const char* name; };
constexpr LeonBin kLeon[] = {
    {4, 0x110, "em/pl00/pl000a.bin"}, {5, 0x111, "em/pl00/pl000a.tpl"}, {9, 0x112, "em/pl00/pl000d.bin"},
    {7, 0x113, "em/pl00/pl000b.tpl"}, {0xA, 0x114, "em/pl00/pl000e.bin"}, {0x10, 0x115, "em/pl00/pl000l.bin"},
};
std::vector<uint8_t> g_plate_leon;   // pixels (C4, 48 x 24) de la plaque « Leon » du HUD, lus dans core.das
uint32_t g_snd_ofs = 0;   // decalage de l'en-tete des sons dans em39.drs (enregistrement de type 4)
// Bras mute (forme 2) : em39ArmControl. Les 45 os 82..126 ne sont animes par aucun coup du corps, seulement par
// 12 animations « de controle » (0xFF..0x10A) jouees sur une seconde structure d'animation (Arm_mot). Etats :
// 0 ferme (0xFF), 1 lame ouverte (0x100), 2 pose normale (0x101), avec des transitions de 16 images (0x102..0x107).
uint32_t g_arm_mot = 0;      // MOTION_INFO du bras (0xD0 octets) dans le tas du jeu
int g_arm_rno = 8;           // etat demande (pair) / en cours (impair) : 0 ferme, 4 ouvert, 8 normal
int g_arm_type = 0;          // etat atteint : 0, 1, 2
int g_prev_form = 1;
int g_snd_req = -1;       // requete de lecture de la banque de sons (DvdReadN)
bool g_snd_asked = false;
bool g_nocut = NJ_ENV("NJ_KRAUSER_NOCUT") != nullptr;
// Deplacements du boss : paires (animation, sequence) de la table des animations du joueur
// (cPlayer::m_MotTbl, +0x424) : attente, marche, course, demi-tour. Reposees a chaque image, car le
// module d'arme les refait quand on change d'arme.
struct MotSwap { int slot, no; };
constexpr MotSwap kMotSwap[] = {
    {0x0, 0x73}, {0x1, 0x74},   // attente
    {0x2, 0x3C}, {0x3, 0x3D},   // marche
    {0x6, 0x3E}, {0x7, 0x3F},   // course
    {0xB, 0x48}, {0xC, 0x49},   // demi-tour
};
// Forme mutante (em39 type 2) : attente sans sequence, marche et course de cette forme.
constexpr MotSwap kMotSwap2[] = {
    {0x0, 0xD6}, {0x1, 0},
    {0x2, 0xD4}, {0x3, 0xD5},
    {0x6, 0xD7}, {0x7, 0xD8},
    {0xB, 0x48}, {0xC, 0x49},
};
// 1 normale, 2 mutante. C'est NOTRE etat : le drapeau « Krauser mute » du jeu (flags_5018 bit 23) n'est jamais leve,
// car il change aussi la camera (vue presque a la premiere personne), la main (visee) et cache l'arme : c'est la
// posture figee des Mercenaries. Le passage se fait par MvTransform / MvRevert (touche X).
int g_form = 1;
constexpr uint32_t oX894 = 0x894;   // cPlKlauser::x894 : 0 = la touche X des Mercenaries est armee ; -1 la verrouille
constexpr uint32_t oMotTbl = 0x424;
const bool g_nomot = NJ_ENV("NJ_KRAUSER_NOMOT") != nullptr;
// NJ_KRAUSER_NOMODEL=1 : garde le modele des Mercenaires (comparaison).
const bool g_nomodel = NJ_ENV("NJ_KRAUSER_NOMODEL") != nullptr;

// --------------------------------------------------------------------------- etat
std::vector<uint8_t> g_pack;     // tampon prepare (cote hote), copie a chaque partie
uint32_t g_pack_guest = 0;       // son adresse dans le jeu (0 : pas de coups du boss)
bool g_room_on = false;
// NJ_KRAUSER_TRACE=1 : etat du joueur a chaque appui (reglage des commandes).
const bool g_trace = NJ_ENV("NJ_KRAUSER_TRACE") != nullptr;
const bool g_trace_arm = NJ_ENV("NJ_KRAUSER_TRACE_ARM") != nullptr;   // une ligne par image et par appel : diagnostic du bras

struct Run {
    MoveId id = MvNone;
    int stage = 0;
    bool started = false, hit_done = false, queued = false, seen_flags = false, switched = false;
    uint8_t back_fd = kFootwork, back_fe = 0;   // routine a reprendre a la fin
    float start_pos[3] = {};
    int frames = 0;
} g_run;

bool heap_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x81800000u; }

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
void put32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v); }

void call(GuestCpu& cpu, uint32_t addr) {
    const uint32_t lr = cpu.lr;
    cpu.lr = kReturn;
    cpu.pc = addr;
    const uint32_t msr = cpu.msr;
    cpu.msr &= ~0x8000u;
    nj_dispatch_one(cpu);   // fonction a addr, trouvee par le repartiteur (P3 : pas de nom)
    for (int guard = 0; cpu.pc != kReturn && cpu.alive && guard < 2000000; ++guard)
        nj_dispatch_one(cpu);
    cpu.msr = (cpu.msr & ~0x8000u) | (msr & 0x8000u);
    cpu.lr = lr;
}

// Tampon : table ofs[kTblWords] (decalages depuis le debut, comme l'archive du jeu) puis les
// entrees utilisees, alignees sur 0x20.
constexpr uint32_t kTblWords = 0x120;

// Beret : modele a une seule piece et un seul poids -> le moteur ne le deforme pas (commonModelTrans,
// nParts == 1) : il le dessine avec la matrice de l'os `partsNo` (octet 0 de la table des os) et
// ses sommets tels quels, donc en repere LOCAL de l'os. Il suffit de mettre l'os 4 (la tete) dans
// la table et de decaler les sommets du (0, 146, 22.6) de SetObj12 d'em39.
void bind_cap(uint8_t* cap, uint32_t cap_n) {
    if (cap_n < 0x40 || cap[0x19] != 1)
        return;
    const float t[3] = {0.0f, 146.0f, 22.6f};
    const uint32_t pos_off = be32(cap + 0x30), bone_off = be32(cap);
    const uint32_t pos_n = (uint32_t(cap[0x38]) << 8) | cap[0x39];
    const int frac = cap[0x28];
    if (pos_off + 8u * pos_n > cap_n || bone_off + 16u > cap_n)
        return;
    for (uint32_t i = 0; i < pos_n; ++i) {
        uint8_t* p = cap + pos_off + 8 * i;
        for (int k = 0; k < 3; ++k) {
            int v = int16_t((p[2 * k] << 8) | p[2 * k + 1]) + int(std::lround(t[k] * float(1 << frac)));
            v = std::max(-32768, std::min(32767, v));
            p[2 * k] = uint8_t(v >> 8);
            p[2 * k + 1] = uint8_t(v);
        }
    }
    cap[bone_off] = 4;
}

bool build_pack() {
    if (!g_pack.empty())
        return true;
    std::vector<uint8_t> raw;
    if (!nj::runtime::DiscView::get().read_file("em/em39.drs"_g, raw) || raw.size() < 0x400) {
        NJ_PRINTF("[Krauser] em/em39.drs introuvable : pas de coups du boss\n");
        return false;
    }
    // Conteneur : enregistrements de 32 octets a 0x20 (type, taille, 0, decalage), type 0 = corps.
    uint32_t body = 0, body_size = 0;
    for (uint32_t o = 0x20; o + 32 <= 0x400; o += 32) {
        const uint32_t type = be32(&raw[o]);
        if (type == 0xFFFFFFFFu)
            break;
        if (type == 0) {
            body_size = be32(&raw[o + 4]);
            body = be32(&raw[o + 12]);
        }
        if (type == 4)
            g_snd_ofs = be32(&raw[o + 12]);
    }
    if (!body || (uint64_t)body + body_size > raw.size() || body_size < 0x10)
        return false;
    const uint8_t* b = &raw[body];
    const uint32_t count = be32(b), rel = be32(b + 4);
    const uint32_t end = rel ? std::min(rel, body_size) : body_size;
    if (count + 4 > kTblWords + 0x40 || 16 + 4 * count > body_size)
        return false;
    std::vector<uint32_t> offs;
    for (uint32_t i = 0; i < count; ++i)
        if (uint32_t o = be32(b + 16 + 4 * i))
            offs.push_back(o);
    std::sort(offs.begin(), offs.end());
    auto entry = [&](uint32_t no, uint32_t& o, uint32_t& n) {
        if (no < 4 || no >= count + 4)
            return false;
        o = be32(b + 4 * no);
        if (!o || o >= end)
            return false;
        auto it = std::upper_bound(offs.begin(), offs.end(), o);
        n = (it != offs.end() ? std::min(*it, end) : end) - o;
        return n > 0;
    };
    std::vector<uint32_t> want;
    for (const Move& mv : kMoves)
        for (int s = 0; s < mv.stages; ++s) {
            want.push_back(mv.stage[s].mot);
            if (mv.stage[s].seq)
                want.push_back(mv.stage[s].seq);
        }
    for (uint32_t n = 0xFF; n <= 0x10A; ++n)
        want.push_back(n);   // controle du bras mute (Arm_mot)
    want.push_back(4);   // donnees d'effets d'em39 (EFF_EM39) : flou des esquives, traces des coups
    for (const SlotSwap& sl : kDmgSlots)
        if (sl.no)
            want.push_back(sl.no);
    for (const MotSwap& ms : kMotSwap)
        want.push_back(ms.no);
    for (const MotSwap& ms : kMotSwap2)
        if (ms.no)
            want.push_back(ms.no);
    for (const Part& pt : kPart) {
        want.push_back(pt.bin_no);
        want.push_back(pt.tpl_no);
    }
    for (const ModelSwap& ms : kModel) {
        if (ms.bin_no != kDummy)
            want.push_back(ms.bin_no);
        if (ms.tpl_no)
            want.push_back(ms.tpl_no);
    }
    std::sort(want.begin(), want.end());
    want.erase(std::unique(want.begin(), want.end()), want.end());
    std::vector<uint8_t> pack(kTblWords * 4, 0);
    for (uint32_t no : want) {
        uint32_t o = 0, n = 0;
        if (no >= kTblWords || !entry(no, o, n)) {
            NJ_PRINTF("[Krauser] entree 0x%X absente de em39\n", no);
            return false;
        }
        pack.resize((pack.size() + 0x1F) & ~size_t(0x1F), 0);
        put32(&pack[4 * no], (uint32_t)pack.size());
        pack.insert(pack.end(), b + o, b + o + n);
    }
    if (const uint32_t cap = be32(&pack[4 * kCapBin])) {
        uint32_t co = 0, cn = 0;
        if (entry(kCapBin, co, cn)) {
            // Piece vide : le beret avec tous ses sommets a l'origine (triangles degeneres).
            std::vector<uint8_t> dummy(&pack[cap], &pack[cap] + cn);
            if (cn >= 0x40) {
                const uint32_t po = be32(&dummy[0x30]), pn = (uint32_t(dummy[0x38]) << 8) | dummy[0x39];
                if (po + 8u * pn <= cn)
                    for (uint32_t k = 0; k < pn; ++k)
                        std::memset(&dummy[po + 8 * k], 0, 6);
            }
            bind_cap(&pack[cap], cn);
            pack.resize((pack.size() + 0x1F) & ~size_t(0x1F), 0);
            put32(&pack[4 * kDummy], (uint32_t)pack.size());
            pack.insert(pack.end(), dummy.begin(), dummy.end());
        }
    }
    // HUD : pixels de la plaque de Leon (TPL a une image C4 de 48 x 24 dans etc/core.das), pour les retrouver en memoire.
    {
        std::vector<uint8_t> craw;
        if (nj::runtime::DiscView::get().read_file("etc/core.das"_g, craw)) {
            for (size_t i = 0; i + 64 < craw.size() && g_plate_leon.empty(); i += 0x20) {
                if (be32(&craw[i]) != 0x0020AF30u || be32(&craw[i + 4]) != 1 || be32(&craw[i + 8]) != 12)
                    continue;
                const uint32_t ih = be32(&craw[i + 12]);
                if (i + ih + 36 > craw.size())
                    continue;
                const uint8_t* h = &craw[i + ih];
                if (((h[0] << 8) | h[1]) != 24 || ((h[2] << 8) | h[3]) != 48 || be32(h + 4) != 8)
                    continue;
                const size_t d = i + be32(h + 8);
                if (d + 576 <= craw.size())
                    g_plate_leon.assign(craw.begin() + d, craw.begin() + d + 576);
            }
        }
    }
    // Pieces de Leon (pl00.drs, entrees 4 5 9 7 A 10) pour ses cinematiques.
    {
        std::vector<uint8_t> lraw;
        if (nj::runtime::DiscView::get().read_file("em/pl00.drs"_g, lraw) && lraw.size() > 0x400) {
            uint32_t lbody = 0, lsize = 0;
            for (uint32_t o = 0x20; o + 32 <= 0x400; o += 32) {
                const uint32_t type = be32(&lraw[o]);
                if (type == 0xFFFFFFFFu)
                    break;
                if (type == 0) {
                    lsize = be32(&lraw[o + 4]);
                    lbody = be32(&lraw[o + 12]);
                }
            }
            if (lbody && (uint64_t)lbody + lsize <= lraw.size() && lsize > 0x10) {
                const uint8_t* lb = &lraw[lbody];
                const uint32_t lcount = be32(lb), lrel = be32(lb + 4), lend = lrel ? std::min(lrel, lsize) : lsize;
                std::vector<uint32_t> loffs;
                for (uint32_t i = 0; i < lcount && 16 + 4 * i + 4 <= lsize; ++i)
                    if (uint32_t o = be32(lb + 16 + 4 * i))
                        loffs.push_back(o);
                std::sort(loffs.begin(), loffs.end());
                int n = 0;
                for (const LeonBin& lg : kLeon) {
                    if (lg.pl00_no < 4 || uint32_t(lg.pl00_no) >= lcount + 4)
                        continue;
                    const uint32_t o = be32(lb + 4 * lg.pl00_no);
                    if (!o || o >= lend)
                        continue;
                    auto it = std::upper_bound(loffs.begin(), loffs.end(), o);
                    const uint32_t sz = (it != loffs.end() ? std::min(*it, lend) : lend) - o;
                    if (!sz)
                        continue;
                    pack.resize((pack.size() + 0x1F) & ~size_t(0x1F), 0);
                    put32(&pack[4 * lg.slot], (uint32_t)pack.size());
                    pack.insert(pack.end(), lb + o, lb + o + sz);
                    ++n;
                }
                NJ_PRINTF("[Krauser] cinematiques : %d pieces de Leon lues dans pl00.drs\n", n);
            }
        } else {
            NJ_PRINTF("[Krauser] em/pl00.drs introuvable : pas de modele de Leon pour les cinematiques\n");
        }
    }
    pack.resize((pack.size() + 0x1F) & ~size_t(0x1F), 0);
    g_pack.swap(pack);
    NJ_PRINTF("[Krauser] animations du boss : %u entrees, %u octets\n", (unsigned)want.size(), (unsigned)g_pack.size());
    return true;
}

uint32_t pack_ptr(GuestCpu& cpu, uint16_t no) {
    if (!no || !g_pack_guest)
        return 0;
    const uint32_t o = cpu.mmu.read32(g_pack_guest + 4u * no);
    return o ? g_pack_guest + o : 0;
}

// --------------------------------------------------------------------------- appels au jeu
struct Regs {
    decltype(GuestCpu::gpr) gpr;
    decltype(GuestCpu::fpr) fpr;
    decltype(GuestCpu::ps1) ps1;
    decltype(GuestCpu::cr) cr;
    uint32_t lr;
};
Regs save(GuestCpu& cpu) { return {cpu.gpr, cpu.fpr, cpu.ps1, cpu.cr, cpu.lr}; }
void restore(GuestCpu& cpu, const Regs& r) {
    cpu.gpr = r.gpr;
    cpu.fpr = r.fpr;
    cpu.ps1 = r.ps1;
    cpu.cr = r.cr;
    cpu.lr = r.lr;
}

// EstSet(model, -1, 0, 0, owner, no, 0, kind, parent, 0) : dix arguments, les deux derniers sur la pile
// (ABI EABI : a sp+8 et sp+12 de l'appelant).
void est_set(GuestCpu& cpu, uint32_t pl, int no) {
    if (!no)
        return;
    auto& m = cpu.mmu;
    const Regs r = save(cpu);
    const uint32_t r1 = cpu.gpr[1];
    const uint32_t sp = (r1 - 0x100) & ~0xFu;
    m.write32(sp, r1);
    m.write32(sp + 8, pl);
    m.write32(sp + 12, 0);
    cpu.gpr[1] = sp;
    cpu.gpr[3] = pl;
    cpu.gpr[4] = 0xFFFFFFFFu;
    cpu.gpr[5] = 0;
    cpu.gpr[6] = 0;
    cpu.gpr[7] = kOwnerEm39;
    cpu.gpr[8] = (uint32_t)no;
    cpu.gpr[9] = 0;
    cpu.gpr[10] = 0;
    call(cpu, kFnEstSet);
    cpu.gpr[1] = r1;
    restore(cpu, r);
}

void esp_load(GuestCpu& cpu) {
    const uint32_t eff = pack_ptr(cpu, 4);
    if (!eff)
        return;
    const Regs r = save(cpu);
    cpu.gpr[3] = eff;
    cpu.gpr[4] = kOwnerEm39;
    cpu.gpr[5] = 0;
    call(cpu, kFnEspLoad);
    const uint32_t ok = cpu.gpr[3];
    restore(cpu, r);
    NJ_PRINTF("[Krauser] effets du boss charges : %u\n", (unsigned)ok);
    std::fflush(stdout);
}

void motion_set(GuestCpu& cpu, uint32_t pl, const Stage& st) {
    cpu.gpr[3] = pl;
    cpu.gpr[4] = pl + oMot;
    cpu.gpr[5] = pack_ptr(cpu, st.mot);
    cpu.gpr[6] = pack_ptr(cpu, st.seq);
    cpu.gpr[7] = st.hokan;
    cpu.gpr[8] = 1;            // bit 0 : le deplacement de l'animation fait avancer le modele
    cpu.gpr[9] = st.frame;
    call(cpu, kFnMotionSetCore);
}

bool motion_move(GuestCpu& cpu, uint32_t pl) {
    cpu.gpr[3] = pl;
    cpu.gpr[4] = 0;
    call(cpu, kFnMotionMove);
    return (cpu.gpr[3] & 0xFFFF) != 0;
}

bool part_pos(GuestCpu& cpu, uint32_t pl, int part, float* p, float* old) {
    cpu.gpr[3] = pl;
    cpu.gpr[4] = (uint32_t)part;
    call(cpu, kFnGetParts);
    const uint32_t q = cpu.gpr[3];
    if (!heap_ptr(q))
        return false;
    for (int i = 0; i < 3; ++i) {
        p[i] = cpu.mmu.read_f32(q + oWorldPos + 4 * i);
        if (old)
            old[i] = cpu.mmu.read_f32(q + oOldWorldPos + 4 * i);
    }
    return true;
}

// PlWepHitCheck2(0, p0, p1, type, 1, len) : vecteurs sous la pile du jeu. Rend le resultat du jeu.
uint32_t hit_check(GuestCpu& cpu, const float* p0, const float* p1, int type, float len) {
    auto& m = cpu.mmu;
    const uint32_t r1 = cpu.gpr[1];
    const uint32_t sp = (r1 - 0x100) & ~0xFu;
    const uint32_t a0 = sp + 0x40, a1 = sp + 0x50;
    for (int i = 0; i < 3; ++i) {
        m.write_f32(a0 + 4 * i, p0[i]);
        m.write_f32(a1 + 4 * i, p1[i]);
    }
    cpu.gpr[1] = sp;
    cpu.gpr[3] = 0;           // sans joueur : ni rayon de visee, ni impact de balle
    cpu.gpr[4] = a0;
    cpu.gpr[5] = a1;
    cpu.gpr[6] = (uint32_t)type;
    cpu.gpr[7] = 1;           // bit 0 : pas d'effet de decor
    cpu.fpr[1] = cpu.ps1[1] = len;
    call(cpu, kFnHitCheck);
    cpu.gpr[1] = r1;
    return cpu.gpr[3];
}

// SndCall(1, no, pos, 0, 0, 0) : banque du joueur (pl0a : 0x51-0x53 = bras).
const char* g_nosnd_env = NJ_ENV("NJ_KRAUSER_NOSND");          // diagnostic : 1 = aucun son du boss, 2 = pas le bruit du bras
const bool g_nosnd = g_nosnd_env && g_nosnd_env[0] == '1';
const bool g_noarmsnd = g_nosnd_env && g_nosnd_env[0] == '2';
// Adresse (dans le jeu) de la position monde d'une partie du corps : valable tant que le joueur existe, comme
// celle que passe le boss (&em->getPartsPtr(no)->world). Repli : la position du joueur.
uint32_t part_pos_ptr(GuestCpu& cpu, uint32_t pl, int part) {
    const Regs r = save(cpu);
    cpu.gpr[3] = pl;
    cpu.gpr[4] = (uint32_t)part;
    call(cpu, kFnGetParts);
    const uint32_t q = cpu.gpr[3];
    restore(cpu, r);
    return heap_ptr(q) ? q + oWorldPos : pl + oPos;
}

bool g_hud_done = false;     // plaque du HUD posee dans cette salle
bool g_snd_ready = false;   // banque de sons d'em39 (blocs d'ennemis, id 0x39) chargee dans cette salle

// SndCall(8, no, pos, 0x39, 0, pl) : voix et bruits du boss, a la tete du joueur. Silencieux tant que la
// banque n'est pas chargee.
void boss_snd(GuestCpu& cpu, uint32_t pl, int no) {
    if (!no || !g_snd_ready || g_nosnd)
        return;
    const uint32_t a = part_pos_ptr(cpu, pl, 4);   // la tete
    const uint32_t r1 = cpu.gpr[1];
    const Regs r = save(cpu);
    cpu.gpr[1] = (r1 - 0x100) & ~0xFu;
    cpu.gpr[3] = 8;
    cpu.gpr[4] = (uint32_t)no;
    cpu.gpr[5] = a;
    cpu.gpr[6] = 0x39;
    cpu.gpr[7] = 0;
    cpu.gpr[8] = pl;
    call(cpu, kFnSndCall);
    cpu.gpr[1] = r1;
    restore(cpu, r);
}

// Numeros d'evenement de sequence (octet Se - 1) que le boss joue comme voix (em39VoiceMove).
bool is_boss_voice(uint32_t v) {
    return v == 6 || v == 9 || v == 0x19 || (v >= 0x1E && v <= 0x34) || v == 0x3E || v == 0x3F;
}

void sound(GuestCpu& cpu, uint32_t pl, int no) {
    const uint32_t r1 = cpu.gpr[1];
    cpu.gpr[1] = (r1 - 0x100) & ~0xFu;
    cpu.gpr[3] = 1;
    cpu.gpr[4] = (uint32_t)no;
    cpu.gpr[5] = pl + oPos;   // la position du joueur, vivante (SndCall en garde le pointeur)
    cpu.gpr[6] = cpu.gpr[7] = cpu.gpr[8] = 0;
    call(cpu, kFnSndCall);
    cpu.gpr[1] = r1;
}

// --------------------------------------------------------------------------- deroule d'un coup
void set_routine(GuestCpu& cpu, uint32_t pl, uint8_t fc, uint8_t fd, uint8_t fe, uint8_t ff) {
    cpu.mmu.write8(pl + oFC, fc);
    cpu.mmu.write8(pl + oFD, fd);
    cpu.mmu.write8(pl + oFE, fe);
    cpu.mmu.write8(pl + oFF, ff);
}

void start(GuestCpu& cpu, uint32_t pl, MoveId id) {
    auto& m = cpu.mmu;
    g_run = Run{};
    g_run.id = id;
    g_run.back_fd = m.read8(pl + oFD);
    g_run.back_fe = m.read8(pl + oFE);
    for (int i = 0; i < 3; ++i)
        g_run.start_pos[i] = m.read_f32(pl + oPos + 4 * i);
    if (g_run.back_fd != kAux)
        set_routine(cpu, pl, 0, kAux, 0, 0);
    // Posture du bras du boss pendant le coup (Arm_rno des routines) : lame ouverte (4) pour les coups du bras,
    // fermee (0) pour les pieds, la garde et la prise, normale (8) sinon.
    switch (id) {
    case MvArm: case MvBackKnuckle: case MvLongArm: case MvJumpArm:
        g_arm_rno = 4;
        break;
    case MvKick: case MvLowKick: case MvSlam: case MvGuard: case MvGrab:
        g_arm_rno = 0;
        break;
    default:
        g_arm_rno = 8;
        break;
    }
    NJ_PRINTF("[Krauser] coup : %s\n", kMoves[id].name);
    std::fflush(stdout);
}

void finish(GuestCpu& cpu, uint32_t pl) {
    auto& m = cpu.mmu;
    float d[3];
    for (int i = 0; i < 3; ++i)
        d[i] = m.read_f32(pl + oPos + 4 * i) - g_run.start_pos[i];
    // Avance en repere du personnage (z devant, x a sa gauche) : sert a verifier les pas et sauts.
    const float ry = m.read_f32(pl + oRotY);
    const float fwd = d[0] * std::sin(ry) + d[2] * std::cos(ry);
    const float side = d[0] * std::cos(ry) - d[2] * std::sin(ry);
    NJ_PRINTF("[Krauser] fin : %s, %d images, devant %.0f, cote %.0f, haut %.0f%s\n", kMoves[g_run.id].name,
              g_run.frames, fwd, side, d[1], g_run.seen_flags ? "" : " (sans drapeaux de sequence)");
    std::fflush(stdout);
    if (g_run.back_fd == kAux)          // posture du bras mute (pl0a) : on y revient
        set_routine(cpu, pl, 0, kAux, 0xA, 0);
    else if (g_run.back_fd == kKnife)   // posture du couteau : elle se relance si L est encore tenu
        set_routine(cpu, pl, 0, kKnife, 0, 0);
    else
        set_routine(cpu, pl, 0, kFootwork, 0, 0);
    g_run.id = MvNone;
    g_arm_rno = 8;   // retour a la pose normale du bras
}

void do_hits(GuestCpu& cpu, uint32_t pl, const Move& mv) {
    float p[3], old[3], root[3];
    uint32_t res = 0;
    switch (mv.hit) {
    case HitArm:
        for (int part : kArmParts)
            if (part_pos(cpu, pl, part, p, nullptr))
                res |= hit_check(cpu, p, p, mv.type, mv.reach);
        break;
    case HitBody:
        if (part_pos(cpu, pl, kRootPart, p, nullptr))
            res |= hit_check(cpu, p, p, mv.type, mv.reach);
        break;
    case HitFoot:
        // Rayon du bassin au pied, prolonge : type 0x15 cherche le long d'un segment.
        if (part_pos(cpu, pl, kRootPart, root, nullptr) && part_pos(cpu, pl, kFootPart, p, nullptr)) {
            float q[3];
            for (int i = 0; i < 3; ++i)
                q[i] = root[i] + (p[i] - root[i]) * 1.6f;
            res |= hit_check(cpu, root, q, mv.type, mv.reach);
        }
        break;
    case HitKnife:
        if (part_pos(cpu, pl, kKnifePart, p, old))
            res |= hit_check(cpu, old, p, mv.type, mv.reach);
        break;
    case HitNone:
        break;
    }
    if (res) {
        g_run.hit_done = true;
        NJ_PRINTF("[Krauser] touche (%s)\n", mv.name);
        std::fflush(stdout);
        if (mv.hit == HitFoot)
            boss_snd(cpu, pl, 0x38);
        else if (mv.hit == HitArm)
            boss_snd(cpu, pl, 0x3D);
    }
}

void step(GuestCpu& cpu, uint32_t pl) {
    auto& m = cpu.mmu;
    const Move& mv = kMoves[g_run.id];
    const uint32_t trg = m.read32(kJoy + oTrg);
    if (!g_run.started) {
        motion_set(cpu, pl, mv.stage[g_run.stage]);
        est_set(cpu, pl, mv.stage[g_run.stage].fx1);
        est_set(cpu, pl, mv.stage[g_run.stage].fx2);
        boss_snd(cpu, pl, mv.stage[g_run.stage].voice);
        if (mv.hit == HitArm)
            sound(cpu, pl, 0x53);
        g_run.started = true;
        g_run.hit_done = false;
    }
    if (mv.chain && (trg & kA) && g_run.stage + 1 < mv.stages)
        g_run.queued = true;
    if (mv.follow && (trg & kA))
        g_run.queued = true;
    // Le stick tourne Krauser pendant le coup (commandes « tank » du jeu), un peu.
    const int sx = (int8_t)m.read8(kJoy + oSx);
    if (std::abs(sx) > 40 && mv.dash == 0.0f && !mv.dodge) {
        float ry = m.read_f32(pl + oRotY) - (float)sx / 127.0f * 0.04f;
        m.write_f32(pl + oRotY, ry);
    }
    const bool end = motion_move(cpu, pl);
    ++g_run.frames;
    // Transformation : la forme change a mi-geste (les pieces se montrent / se cachent a l'image suivante).
    if ((g_run.id == MvTransform || g_run.id == MvRevert) && !g_run.switched) {
        const float f = m.read_f32(pl + oFrame), n = (float)m.read16(pl + oFrameMax);
        if (n > 0.0f && f >= n * 0.45f) {
            g_run.switched = true;
            g_form = g_run.id == MvTransform ? 2 : 1;
            g_arm_rno = g_form == 2 ? 8 : 0;
            NJ_PRINTF("[Krauser] forme %d\n", g_form);
            std::fflush(stdout);
        }
    }
    // Evenements de sequence du boss : ses voix (em39VoiceMove) ; le reste (pas) reste au joueur.
    if (const uint8_t se = m.read8(pl + oSeNo)) {
        if (is_boss_voice(uint32_t(se) - 1)) {
            m.write8(pl + oSeNo, 0);
            boss_snd(cpu, pl, se - 1);
        }
    }
    if (g_run.id == MvGuard) {
        const bool held = (m.read32(kJoy + oOn) & kZ) != 0;
        if (trg & kA) {   // Z tenu + A : la prise
            finish(cpu, pl);
            start(cpu, pl, MvGrab);
            return;
        }
        if (!held && g_run.frames > 12) {
            finish(cpu, pl);
            return;
        }
        if (end && held) {
            g_run.started = false;   // Z encore tenu : le geste reprend
            return;
        }
    }
    const uint8_t fl = m.read8(pl + oSeFlags);
    if (fl)
        g_run.seen_flags = true;
    // Fenetre active : bit 0 de la sequence du boss ; sans sequence, le tiers central de l'animation.
    bool active = (fl & 1) != 0;
    if (!g_run.seen_flags) {
        const float f = m.read_f32(pl + oFrame), n = (float)m.read16(pl + oFrameMax);
        active = n > 0.0f && f >= n * 0.3f && f <= n * 0.6f;
    }
    if (active && mv.dash > 0.0f) {
        // Devant = 3e colonne de la matrice locale (comme PSMTXMultVecSR(mat, (0, 0, v))).
        for (int i = 0; i < 3; i += 2) {
            const float c = m.read_f32(pl + oMat + 4 * (4 * i + 2));
            m.write_f32(pl + oPos + 4 * i, m.read_f32(pl + oPos + 4 * i) + c * mv.dash);
        }
    }
    if (active && !g_run.hit_done && mv.hit != HitNone)
        do_hits(cpu, pl, mv);
    const bool next_ok = (fl & 4) || end;
    if (mv.follow && g_run.hit_done && g_run.queued && next_ok) {
        const uint8_t nx = mv.follow;
        NJ_PRINTF("[Krauser] suite : %s -> %s\n", mv.name, kMoves[nx].name);
        g_run.id = (MoveId)nx;
        g_run.stage = 0;
        g_run.started = false;
        g_run.hit_done = false;
        g_run.queued = false;
        g_run.frames = 0;
        g_run.seen_flags = false;
        return;
    }
    if (g_run.stage + 1 < mv.stages && next_ok && (!mv.chain || g_run.queued)) {
        if (g_trace)
            NJ_PRINTF("[Krauser] etape %d -> %d apres %d images (drapeaux %02X, fin %d)\n", g_run.stage,
                      g_run.stage + 1, g_run.frames, fl, end);
        ++g_run.stage;
        g_run.started = false;
        g_run.queued = false;
        return;
    }
    if (end)
        finish(cpu, pl);
}

// --------------------------------------------------------------------------- commandes
bool prompt_shown(GuestCpu& cpu) {
    return cpu.mmu.read8(kActBtn + 0x40) != 0 || cpu.mmu.read8(kActBtn + 0x42) != 0;
}

// Esquive rapide du boss (Z + direction du stick principal ; le stick C sert a la camera). Z sans
// direction : saut arriere en forme normale, garde en forme mutante. Gauche / droite : pas de cote ;
// avant : pas en avant ; arriere : saut arriere ; diagonales avant : esquive diagonale (celle de la forme
// mutante quand elle est active) ; diagonales arriere : roulade.
MoveId dodge_by_stick(int sx, int sy, bool mutated) {
    const int ax = std::abs(sx), ay = std::abs(sy);
    if (std::max(ax, ay) < 40)   // le stick va a +-72
        return mutated ? MvGuard : MvBackjump;
    const bool side = ax * 10 > ay * 4, vert = ay * 10 > ax * 4;
    if (side && !vert)
        return sx < 0 ? MvStepL : MvStepR;
    if (vert && !side)
        return sy > 0 ? MvStepF : MvBackjump;
    if (sy > 0)
        return sx < 0 ? (mutated ? MvSlant2L : MvSlantL) : (mutated ? MvSlant2R : MvSlantR);
    return sx < 0 ? MvRollL : MvRollR;
}

MoveId choose(GuestCpu& cpu, uint32_t pl, uint32_t g) {
    auto& m = cpu.mmu;
    const uint32_t on = m.read32(kJoy + oOn), trg = m.read32(kJoy + oTrg);
    if (!(trg & (kA | kB | kX | kZ | kY)))
        return MvNone;
    const int sx = (int8_t)m.read8(kJoy + oSx), sy = (int8_t)m.read8(kJoy + oSy);
    const uint8_t fc = m.read8(pl + oFC), fd = m.read8(pl + oFD), fe = m.read8(pl + oFE);
    (void)g;
    const bool mutated = g_form == 2;
    // Meme flux que le script de touches (std::cout), pour que les deux journaux restent dans l'ordre.
    if (g_trace)
        NJ_LOG << std::hex << "[Krauser] boutons " << trg << "/" << on << std::dec << " stick " << sx << ","
               << sy << " routine " << (int)fc << "/" << (int)fd << "/" << (int)fe << " mute " << mutated
               << " invite " << prompt_shown(cpu) << "\n";
    // Z : esquive rapide / garde ; Y : provocation (forme normale).
    if (fc == 0 && (fd <= kRun || fd == kAux) && !prompt_shown(cpu)) {
        if (trg & kZ)
            return dodge_by_stick(sx, sy, mutated);
        if ((trg & kY) && !mutated && fd <= kRun)
            return MvThreat;
        // X : on passe d'une forme a l'autre, a pied (la routine X des Mercenaries est verrouillee).
        if ((trg & kX) && fd <= kRun)
            return mutated ? MvRevert : MvTransform;
    }
    if (fc != 0 || !(trg & (kA | kB)))
        return MvNone;
    (void)fe;
    // Forme mutante : le joueur marche, court et esquive librement (aucune posture figee) ; A = coups du bras,
    // avec Maj (B) tenu = coups de pied et saut du bras ; la direction du stick choisit le coup.
    if (mutated) {
        if (!(trg & kA) || fd > kRun || prompt_shown(cpu))
            return MvNone;
        const bool fwd = sy > 60, back = sy < -60;
        if (on & kB)
            return fwd ? MvJumpArm : (back ? MvLowKick : MvKick);
        return fwd ? MvLongArm : (back ? MvBackKnuckle : MvArm);
    }
    // Posture du couteau (L) : A = combo du boss (A encore = 2e coup), R + A = les quatre coups.
    if (fd == kKnife && (trg & kA)) {
        if (on & kR)
            return MvKnife4;
        if (sy > 60)
            return MvCatch;                       // estoc droit devant
        if (sx < -60)
            return MvCatchL;
        if (sx > 60)
            return MvCatchR;
        return MvKnife;
    }
    if (fd > kRun || prompt_shown(cpu))
        return MvNone;
    if ((trg & kA) && (on & kB) && fd == kRun)
        return MvSuperDash;
    if ((trg & kA) && !(on & kB))
        return fd == kBack || sy < -60 ? MvLowKick : MvKick;
    if ((trg & kB) && sy < -60 && std::abs(sx) < 50)
        return MvBackjump;
    if ((trg & kB) && std::abs(sx) > 70 && std::abs(sy) < 40)
        return sx < 0 ? MvStepL : MvStepR;
    return MvNone;
}

bool enabled(GuestCpu& cpu, uint32_t pl) {
    return g_room_on && g_pack_guest && heap_ptr(pl) && pl == cpu.mmu.read32(kPPL);
}

}  // namespace

namespace nj::krauser {

void game_init(GuestCpu& cpu) {
    g_pack_guest = 0;
    g_run = Run{};
    g_form = 1;
    {   // Ennemis de l'histoire : avec x4FB8 == 4 le jeu lit EmFileTbl_Klauser (table des Mercenaires : beaucoup d'ennemis
        // y pointent des fichiers generiques 0x18/0x19). On y recopie la table de l'histoire, comme le fait l'Ultimate
        // Trainer de Raz0r pour ses personnages. Seulement si les deux adresses sont connues pour cette version.
        const uint32_t story = kEmFileTbl, merc = kEmFileTblKlauser;
        auto& m = cpu.mmu;
        if (heap_ptr(story) && heap_ptr(merc))
            if (const uint8_t* src = m.get_span(story, 0x200))
                if (uint8_t* dst = m.get_span(merc, 0x200)) {
                    std::memcpy(dst, src, 0x200);
                    NJ_PRINTF("[Krauser] table d'ennemis : celle de l'histoire\n");
                    std::fflush(stdout);
                }
    }
    if (NJ_ENV("NJ_KRAUSER_NOPACK"))   // diagnostic : aucun tampon du boss (memoire du tas)
        return;
    if (!build_pack() || !heap_ptr(kFnMemAlloc))
        return;
    // 21/10 : le tampon (~2,3 Mo) va dans les 8 Mo au-dessus du jeu (MEM1 de 32 Mo, cpu_context.h) : le tas du jeu restait
    // plein (village : « DVD: Memory allocate failed » puis plantage). Repli sur le tas si la zone n'existe pas.
    uint32_t a = 0;
    if (cpu.mmu.mem1.size() - 8 >= (32u << 20) && g_pack.size() <= (8u << 20))
        a = 0x81800000u;
    else {
        const Regs r = save(cpu);
        cpu.gpr[3] = (uint32_t)g_pack.size();
        cpu.gpr[4] = 1;
        call(cpu, kFnMemAlloc);
        a = cpu.gpr[3];
        restore(cpu, r);
        if (!heap_ptr(a)) a = 0;
    }
    uint8_t* dst = a ? cpu.mmu.get_span(a, g_pack.size()) : nullptr;
    if (!dst) {
        NJ_PRINTF("[Krauser] pas de place pour les animations du boss (%u octets)\n", (unsigned)g_pack.size());
        return;
    }
    std::memcpy(dst, g_pack.data(), g_pack.size());
    g_pack_guest = a;
    {   // structure d'animation du bras (0xD0 octets, mise a zero)
        const Regs r2 = save(cpu);
        cpu.gpr[3] = 0x100;
        cpu.gpr[4] = 1;
        call(cpu, kFnMemAlloc);
        const uint32_t am = cpu.gpr[3];
        restore(cpu, r2);
        if (heap_ptr(am) && cpu.mmu.get_span(am, 0x100)) {
            std::memset(cpu.mmu.get_span(am, 0x100), 0, 0x100);
            g_arm_mot = am;
        }
    }
    NJ_PRINTF("[Krauser] animations du boss en %08X\n", a);
    std::fflush(stdout);
}

void room_init(GuestCpu& cpu, bool on) {
    g_room_on = on;
    g_parts_done = false;
    g_snd_ready = false;
    g_snd_asked = false;
    g_hud_done = false;
    g_snd_req = -1;
    g_arm_rno = 8;
    g_arm_type = 0;
    g_prev_form = 1;
    if (g_arm_mot)
        if (uint8_t* am = cpu.mmu.get_span(g_arm_mot, 0x100))
            std::memset(am, 0, 0x100);
    std::memset(g_part_info, 0, sizeof g_part_info);
    g_run = Run{};
    if (on) {
        // Camera : celle de Krauser (g_transOfs[3], reglee par Capcom pour les Mercenaires), laissee telle quelle.
        // NJ_KRAUSER_CAMY=<n> remonte la camera de n unites (site du milieu, les deux epaules) pour
        // regler la hauteur sans toucher a la distance ; applique une seule fois par partie.
        static bool done = false;
        const char* e = NJ_ENV("NJ_KRAUSER_CAMY");
        auto& m = cpu.mmu;
        const uint32_t base = kTransOfs;
        if (e && !done && heap_ptr(base)) {
            done = true;
            const float dy = (float)std::atof(e);
            for (int side = 0; side < 2; ++side) {
                const uint32_t ent = base + kTransSet * 3 + (uint32_t)(side * 3 + 1) * 0x2C;   // milieu
                m.write_f32(ent + 4, m.read_f32(ent + 4) + dy);       // camera (hauteur)
            }
        }
    }
}

}  // namespace nj::krauser

// HUD : le jeu a une plaque de nom pour Leon (unite 0x40 du compteur de vie), Ashley, Ada ; rien pour Krauser
// (cockpit.cpp LifeMeter::roomInit). On montre l'unite de Leon et on remplace les pixels de sa plaque (C4,
// meme palette) par « Krauser » (tools/re4/gen_plaque_krauser.py).
bool hud_plate(GuestCpu& cpu) {
    if (g_plate_leon.size() != 576)
        return true;   // rien a faire : on ne reessaie pas
    auto& m = cpu.mmu;
    // Le compteur de vie (cockpit LifeMeter::roomInit) est cree apres le joueur : tant que l'unite 0x40 n'existe
    // pas, IDSystem::unitPtr rend l'unite factice (celle de l'echec) ; on reessaie a l'image suivante.
    {
        const Regs rq = save(cpu);
        cpu.gpr[3] = kIdSys;
        cpu.gpr[4] = 0xEE;
        cpu.gpr[5] = 33;
        call(cpu, kFnIdUnit);
        const uint32_t dummy = cpu.gpr[3];
        cpu.gpr[3] = kIdSys;
        cpu.gpr[4] = 0x40;
        cpu.gpr[5] = 33;
        call(cpu, kFnIdUnit);
        const uint32_t u40 = cpu.gpr[3];
        restore(cpu, rq);
        if (u40 == dummy)
            return false;
    }
    uint8_t* mem = m.get_span(0x80000000u, 0x01800000u);
    int n = 0;
    if (mem) {
        const uint8_t* end = mem + 0x01800000u - 576;
        for (uint8_t* q = mem; q && q < end;) {
            q = (uint8_t*)std::memchr(q, g_plate_leon[0], size_t(end - q));
            if (!q)
                break;
            if (std::memcmp(q, g_plate_leon.data(), 576) == 0) {
                std::memcpy(q, kPlateKrauser, 576);
                q += 576;
                ++n;
            } else {
                ++q;
            }
        }
    }
    const Regs r = save(cpu);
    cpu.gpr[3] = kIdSys;
    cpu.gpr[4] = 0x40;
    cpu.gpr[5] = 33;   // IDC_LIFE_METER
    call(cpu, kFnIdUnit);
    const uint32_t unit = cpu.gpr[3];
    restore(cpu, r);
    if (heap_ptr(unit))
        m.write8(unit, uint8_t(m.read8(unit) | 8));
    if (NJ_ENV("NJ_KRAUSER_HUDDIAG")) {   // diagnostic : toutes les unites du compteur de vie
        const Regs rd = save(cpu);
        cpu.gpr[3] = kIdSys;
        cpu.gpr[4] = 0xEE;
        cpu.gpr[5] = 33;
        call(cpu, kFnIdUnit);
        const uint32_t dummy = cpu.gpr[3];
        for (uint32_t mk = 0; mk < 0x100; ++mk) {
            cpu.gpr[3] = kIdSys;
            cpu.gpr[4] = mk;
            cpu.gpr[5] = 33;
            call(cpu, kFnIdUnit);
            const uint32_t u = cpu.gpr[3];
            if (u == dummy || !heap_ptr(u))
                continue;
            float w = m.read_f32(u + 0xD0), h = m.read_f32(u + 0xD4);
            NJ_PRINTF("[Krauser] HUD unite %02X : flag %02X type %02X parent %02X tex %02X/%02X no %02X taille %.0fx%.0f pos %.0f,%.0f\n", mk,
                      m.read8(u), m.read8(u + 4), m.read8(u + 6), m.read8(u + 0x6C), m.read8(u + 0x6D), m.read8(u + 0x6E), w, h,
                      m.read_f32(u + 0x88), m.read_f32(u + 0x8C));
        }
        restore(cpu, rd);
        std::fflush(stdout);
    }
    NJ_PRINTF("[Krauser] HUD : plaque Krauser (unite %08X, pixels remplaces : %d)\n", unit, n);
    std::fflush(stdout);
    return true;
}

void snd_start(GuestCpu& cpu) {
    if (!g_snd_ofs || g_snd_asked)
        return;
    g_snd_asked = true;
    auto& m = cpu.mmu;
    const Regs r = save(cpu);
    const uint32_t r1 = r.gpr[1];
    const uint32_t sp = (r1 - 0x200) & ~0xFu, nm = sp + 0x80, fl = sp + 0xC0;
    const char* name = "em/em39.drs";
    for (uint32_t i = 0;; ++i) {
        m.write8(nm + i, (uint8_t)name[i]);
        if (!name[i])
            break;
    }
    m.write8(fl, 'k');
    m.write8(fl + 1, 0);
    cpu.gpr[1] = sp;
    cpu.gpr[3] = nm;
    cpu.gpr[4] = 0;
    cpu.gpr[5] = 0;
    cpu.gpr[6] = g_snd_ofs;
    cpu.gpr[7] = 0;
    cpu.gpr[8] = 0x8100;
    cpu.gpr[9] = fl;
    cpu.gpr[10] = 0;
    call(cpu, kFnDvdReadN);
    g_snd_req = (int32_t)cpu.gpr[3];
    cpu.gpr[1] = r1;
    restore(cpu, r);
    NJ_PRINTF("[Krauser] banque de sons du boss : lecture demandee (%d)\n", g_snd_req);
    std::fflush(stdout);
}

void snd_poll(GuestCpu& cpu) {
    if (g_snd_req < 0 || g_snd_ready)
        return;
    const Regs r = save(cpu);
    cpu.gpr[3] = kDvd;
    cpu.gpr[4] = (uint32_t)g_snd_req;
    cpu.gpr[5] = 0;
    call(cpu, kFnDvdCheck);
    const int ret = (int32_t)cpu.gpr[3];
    restore(cpu, r);
    if (ret == 1) {
        g_snd_ready = true;
        g_snd_req = -1;
        NJ_PRINTF("[Krauser] banque de sons du boss chargee\n");
        std::fflush(stdout);
    } else if (ret < 0) {
        g_snd_req = -1;
        NJ_PRINTF("[Krauser] banque de sons du boss : erreur %d\n", ret);
        std::fflush(stdout);
    }
}

void spawn_parts(GuestCpu& cpu, uint32_t pl) {
    const Regs r = save(cpu);
    int n = 0;
    for (int i = 0; i < kNumParts; ++i) {
        const uint32_t bin = pack_ptr(cpu, kPart[i].bin_no), tpl = pack_ptr(cpu, kPart[i].tpl_no);
        if (!bin || !tpl)
            continue;
        cpu.gpr[3] = kModInfoMgr;
        cpu.gpr[4] = bin;
        cpu.gpr[5] = tpl;
        call(cpu, kFnModCreate);
        const uint32_t info = cpu.gpr[3];
        if (!heap_ptr(info))
            continue;
        cpu.gpr[3] = pl;
        cpu.gpr[4] = info;
        call(cpu, kFnAddModel);
        g_part_info[i] = info;
        ++n;
    }
    restore(cpu, r);
    NJ_PRINTF("[Krauser] %d pieces du boss ajoutees (forme normale + forme mutante)\n", n);
    std::fflush(stdout);
}

// Forme du boss : mutante quand le jeu a leve le drapeau « Krauser mute » (X, pl0a), sinon normale.
// be_flag (octet 0 de l'info de modele) bit 8 = dessine.
// Bras mute (em39ArmControl, simplifie) : g_arm_rno demande une posture (0 ferme, 4 lame ouverte, 8 normale) ;
// le bras y va par l'animation de transition de 16 images (0x102..0x107, bruit de chair 0x53 au poignet),
// puis garde la derniere pose. Les animations en boucle de 134 images (0xFF, 0x100, 0x101) ne sont pas
// jouees : leur premier MotionMoveCore ne rend jamais la main dans le moteur (essai du 19/10 : le jeu se
// fige), alors que les transitions passent. On perd la pulsation de repos, pas les postures.
bool g_arm_busy = false;   // une transition est en cours

void arm_control(GuestCpu& cpu, uint32_t pl) {
    if (!g_arm_mot)
        return;
    auto& m = cpu.mmu;
    const int want = g_arm_rno == 0 ? 0 : (g_arm_rno == 4 ? 1 : 2);
    if (!g_arm_busy && want == g_arm_type)
        return;                                     // deja dans la posture demandee : on garde la pose
    m.write_f32(g_arm_mot + 0xC0, 1.0f);                                     // Seq_speed
    m.write32(g_arm_mot + 0x44, m.read32(g_arm_mot + 0x44) | 0x10000000);    // Mot_flag
    const Regs r = save(cpu);
    auto trace = [&](const char* what, int a, uint32_t b) {
        if (g_trace_arm) {
            NJ_PRINTF("[Krauser] bras : %s %d %08X (posture %d -> %d)" "\n", what, a, b, g_arm_type, want);
            std::fflush(stdout);
        }
    };
    bool se = false;
    if (!g_arm_busy) {
        // transitions : de la posture actuelle vers la demandee
        static const int tr[3][3] = {{0, 0x102, 0x104}, {0x103, 0, 0x106}, {0x105, 0x107, 0}};
        const int no = tr[g_arm_type < 3 ? g_arm_type : 0][want];
        if (no && pack_ptr(cpu, (uint16_t)no)) {
            trace("MotionSetCore", no, pack_ptr(cpu, (uint16_t)no));
            cpu.gpr[3] = pl;
            cpu.gpr[4] = g_arm_mot;
            cpu.gpr[5] = pack_ptr(cpu, (uint16_t)no);
            cpu.gpr[6] = 0;
            cpu.gpr[7] = 0;
            cpu.gpr[8] = 4;
            cpu.gpr[9] = 0;
            call(cpu, kFnMotionSetCore);
            g_arm_busy = true;
            se = true;
        }
        g_arm_type = want;
    }
    if (g_arm_busy && m.read32(g_arm_mot) != 0) {
        trace("MotionMoveCore", 0, pl);
        cpu.gpr[3] = pl;
        cpu.gpr[4] = g_arm_mot;
        cpu.gpr[5] = 0;
        call(cpu, kFnMotionMoveCore);
        trace("MotionSequenceCtrl", 0, g_arm_mot);
        cpu.gpr[3] = g_arm_mot;
        call(cpu, kFnMotionSeqCtrl);
        if (cpu.gpr[3])
            g_arm_busy = false;                     // transition finie : la pose reste
    } else {
        g_arm_busy = false;
    }
    restore(cpu, r);
    if (se && g_snd_ready && !g_nosnd && !g_noarmsnd) {
        const uint32_t a = part_pos_ptr(cpu, pl, 0xE);   // le poignet
        const uint32_t r1 = cpu.gpr[1];
        const Regs r2 = save(cpu);
        cpu.gpr[1] = (r1 - 0x100) & ~0xFu;
        cpu.gpr[3] = 8;
        cpu.gpr[4] = 0x53;
        cpu.gpr[5] = a;
        cpu.gpr[6] = 0x39;
        cpu.gpr[7] = 0;
        cpu.gpr[8] = pl;
        call(cpu, kFnSndCall);
        cpu.gpr[1] = r1;
        restore(cpu, r2);
    }
}

void update_forms(GuestCpu& cpu, uint32_t g) {
    auto& m = cpu.mmu;
    (void)g;
    const int form = g_form;
    if (form == 2 && g_prev_form != 2) {   // le bras se met en pose normale (etat 2 depuis l'etat 0)
        g_arm_rno = 8;
        g_arm_type = 0;
        g_arm_busy = false;
        if (g_arm_mot)
            if (uint8_t* am = m.get_span(g_arm_mot, 0x100))
                std::memset(am, 0, 0x100);
    }
    g_prev_form = form;
    for (int i = 0; i < kNumParts; ++i) {
        const uint32_t info = g_part_info[i];
        if (!heap_ptr(info))
            continue;
        const uint32_t f = m.read32(info);
        const uint32_t want = kPart[i].form == form ? (f | 8u) : (f & ~8u);
        if (want != f)
            m.write32(info, want);
    }
}

void nj_kr_PlayerMove(GuestCpu& cpu, GuestFn orig) {
    const uint32_t pl = cpu.gpr[3];
    if (enabled(cpu, pl)) {
        auto& m = cpu.mmu;
        const uint32_t g = m.read32(kPG);
        // Un coup interrompu par le jeu (degats, evenement) : on le lache.
        if (g_run.id != MvNone && (m.read8(pl + oFC) != 0 || m.read8(pl + oFD) != kAux))
            g_run.id = MvNone;
        if (heap_ptr(g)) {
            // Ni le drapeau « mute » du jeu (camera, main, arme cachee) ni la touche X des Mercenaires (posture
            // figee, aura de pl0a) : la forme est la notre.
            const uint32_t fl = m.read32(g + pg(0x5018));
            if (fl & kMutated)
                m.write32(g + pg(0x5018), fl & ~kMutated);
            m.write32(pl + oX894, 0xFFFFFFFFu);
        }
        if (!g_nomodel && heap_ptr(g)) {
            if (!g_parts_done) {
                g_parts_done = true;
                spawn_parts(cpu, pl);
                esp_load(cpu);
                snd_start(cpu);
            }
            if (!g_hud_done)
                g_hud_done = hud_plate(cpu);
            snd_poll(cpu);
            update_forms(cpu, g);
            if (g_form == 2 && g_parts_done)
                arm_control(cpu, pl);
        }
        if (!g_nomot) {
            const uint32_t tbl = m.read32(pl + oMotTbl);
            if (heap_ptr(tbl))
                for (const MotSwap& ms : (g_form == 2 ? kMotSwap2 : kMotSwap)) {
                    const uint32_t o = ms.no ? be32(&g_pack[4 * ms.no]) : 0;
                    m.write32(tbl + 4 * ms.slot, o ? g_pack_guest + o : 0);
                }
        }
        if (g_run.id == MvNone && heap_ptr(g)) {
            const Regs r = save(cpu);
            const MoveId id = choose(cpu, pl, g);
            if (id != MvNone)
                start(cpu, pl, id);
            restore(cpu, r);
        }
    }
    cpu.pc = nj::runtime::ga(0x800579B4);
    orig(cpu);
}

// cPlayer::setDamage(this, kind, arg, a, b, ang) : pendant la garde, un coup normal venu de face est
// annule. `ang` est l'orientation de l'attaquant (123 = inconnue) : s'il regarde du meme cote que le
// joueur (ecart < 90 degres) il est derriere lui, comme le teste le jeu (kind 1 : coup de dos).
void nj_kr_PlDamage(GuestCpu& cpu, GuestFn orig) {
    const uint32_t pl = cpu.gpr[3];
    // Esquive : le boss est invulnerable (Be_flg 0x100) tant que le repere 4 de la sequence n'est pas passe.
    if (g_run.id != MvNone && kMoves[g_run.id].dodge && enabled(cpu, pl) &&
        !(cpu.mmu.read8(pl + oSeFlags) & 4)) {
        NJ_PRINTF("[Krauser] esquive : coup evite\n");
        std::fflush(stdout);
        cpu.pc = cpu.lr;
        return;
    }
    if (g_run.id == MvGuard && enabled(cpu, pl) && (cpu.gpr[4] & 0xFF) <= 5) {
        const float ang = (float)cpu.fpr[1];
        float d = ang - cpu.mmu.read_f32(pl + oRotY);
        while (d > 3.14159265f) d -= 6.2831853f;
        while (d < -3.14159265f) d += 6.2831853f;
        if (ang == 123.0f || std::fabs(d) >= 1.5707964f) {
            NJ_PRINTF("[Krauser] garde : coup bloque\n");
            std::fflush(stdout);
            cpu.pc = cpu.lr;
            return;
        }
    }
    cpu.pc = nj::runtime::ga(0x80173A98);
    orig(cpu);
}

void nj_kr_EmConstruct(GuestCpu& cpu, GuestFn orig) {
    // cEmMgr::construct(this, cEm*, id) : id 0 = le joueur ; ReadPlayerData a deja charge pl0a.
    if (cpu.gpr[5] == 0 && g_room_on && g_pack_guest && !g_nomodel) {
        auto& m = cpu.mmu;
        const uint32_t g = m.read32(kPG);
        const uint32_t arc = heap_ptr(g) ? m.read32(g + oPlayerArc) : 0;
        if (heap_ptr(arc)) {
            int n = 0;
            for (const ModelSwap& ms : kModel) {
                const uint32_t bin = be32(&g_pack[4 * ms.bin_no]);
                if (!bin)
                    continue;
                m.write32(arc + 4 * ms.bin_slot, g_pack_guest + bin - arc);
                if (ms.tpl_slot && ms.tpl_no) {
                    const uint32_t tpl = be32(&g_pack[4 * ms.tpl_no]);
                    if (tpl)
                        m.write32(arc + 4 * ms.tpl_slot, g_pack_guest + tpl - arc);
                }
                ++n;
            }
            for (const SlotSwap& sl : kDmgSlots) {
                const uint32_t o = sl.no ? be32(&g_pack[4 * sl.no]) : 0;
                if (sl.no && !o)
                    continue;
                m.write32(arc + 4 * sl.slot, sl.no ? g_pack_guest + o - arc : 0u - arc);
                ++n;
            }
            NJ_PRINTF("[Krauser] modele du boss : %d pieces posees dans l'archive du joueur\n", n);
            std::fflush(stdout);
        }
    }
    cpu.pc = nj::runtime::ga(0x800ED258);
    orig(cpu);
}

// Event::ExeBeginEvt (0x80137520) : pour Leon le jeu enregistre ses modeles d'evenement ; pour Krauser
// (pl_type 4) il ne le fait pas, et l'evenement ne trouve ni le corps ni la tete de Leon. On enregistre
// les memes noms sur les pieces de Leon lues dans pl00.drs.
void nj_kr_BeginEvt(GuestCpu& cpu, GuestFn orig) {
    if (g_room_on && g_pack_guest && !g_nocut) {
        auto& m = cpu.mmu;
        const uint32_t g = m.read32(kPG);
        const Regs r = save(cpu);
        const uint32_t r1 = r.gpr[1];
        const uint32_t sp = (r1 - 0x200) & ~0xFu, nm = sp + 0x80;
        cpu.gpr[1] = sp;
        int n = 0;
        auto reg = [&](const char* name, uint32_t data) {
            if (!heap_ptr(data))
                return;
            for (uint32_t i = 0;; ++i) {
                m.write8(nm + i, (uint8_t)name[i]);
                if (!name[i])
                    break;
            }
            cpu.gpr[3] = kEvtMgr;
            cpu.gpr[4] = nm;
            cpu.gpr[5] = data;
            cpu.gpr[6] = 0;
            cpu.gpr[7] = 2;
            call(cpu, kFnEvtSetBin);
            ++n;
        };
        for (const LeonBin& lg : kLeon)
            reg(lg.name, pack_ptr(cpu, (uint16_t)lg.slot));
        const uint32_t core = heap_ptr(g) ? m.read32(g + 0x48) : 0;   // Global::pCore : CORE_ARC(CORE_DUMMY_BIN / TPL)
        if (heap_ptr(core)) {
            reg("etc/core/dummy.bin", m.read32(core + 4 * 8) + core);
            reg("etc/core/dummy.tpl", m.read32(core + 4 * 9) + core);
        }
        cpu.gpr[1] = r1;
        restore(cpu, r);
        NJ_PRINTF("[Krauser] cinematique : %d modeles de Leon enregistres\n", n);
        std::fflush(stdout);
    }
    cpu.pc = nj::runtime::ga(0x80137520);
    orig(cpu);
}

// Banque de sons du boss : em39.drs contient un en-tete imbrique (decalage g_snd_ofs) avec un bloc de
// sons (type 1) et ses echantillons (type 2), id 0x39. Le chargeur du jeu sait le lire seul : on le lance
// une fois par salle (les blocs d'ennemis sont remis a zero a chaque salle) avec les memes parametres que
// ReadPlayerData (0x8100 : fichier a en-tete, par une tache), puis on interroge la fin.
void nj_kr_PlAux(GuestCpu& cpu, GuestFn orig) {
    const uint32_t pl = cpu.gpr[3];
    if (g_run.id != MvNone && enabled(cpu, pl)) {
        const Regs r = save(cpu);
        step(cpu, pl);
        restore(cpu, r);
        cpu.pc = cpu.lr;
        return;
    }
    cpu.pc = nj::runtime::ga(0x80059D10);
    orig(cpu);
}
