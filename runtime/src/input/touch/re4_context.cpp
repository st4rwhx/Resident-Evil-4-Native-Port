#include "input/re4_context.h"
#include "runtime/log.h"
#include "runtime/game_version.h"

#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/native/native_core.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace nj::runtime {
extern GuestCpu *g_guest;
}

namespace nj::runtime::input {

namespace {

// Adresses de G4BE08 (re4-decomp-upstream). Chaque lecture cite sa source.
constexpr nj::runtime::GameAddr kPG {0x80314BC8};          // Global* pG (instance Global 0x802E0478)
constexpr nj::runtime::GameAddr kPPL {0x803159F4};         // cPlayer* pPL
constexpr nj::runtime::GameAddr kPSUB {0x803159F8};        // cSubChar* pSUB (Ashley)
constexpr nj::runtime::GameAddr kTask0Func {0x802EB044};   // Task[0].func
constexpr nj::runtime::GameAddr kTitleTask {0x801A07F0};   //   = ecran titre
constexpr nj::runtime::GameAddr kSofdecFlag {0x802EE858};  // cSofdec.flag, bit0 = video en cours
constexpr nj::runtime::GameAddr kActBtn {0x802D7B04};      // cActionButton ActBtn (include/act_btn.h)
constexpr nj::runtime::GameAddr kCckptActionNo {0x802D90F0};  // Cckpt.action.no = bouton de l'invite (disp())
constexpr nj::runtime::GameAddr kSubScreenWk {0x80277F10};  // SUB_SCREEN SubScreenWk (include/sscrn.h)
constexpr nj::runtime::GameAddr kMes {0x80271F38};         // MessageControl cMes (0x80271F34) + 4 = mes[0]
constexpr uint32_t kMesSize = 0xEC;
// IDSystem IdSys (0x802E020C) : ck[8] a +0x0C, un bit par type de table d'unites posee
// (IdBitOn, bit 0x80000000 >> (type & 31) du mot type >> 5). ID_MENU = 0x29 (title.cpp) : le menu
// START / LOAD / OPTIONS existe. Efface par IdSys.kill (options, sortie du titre).
constexpr nj::runtime::GameAddr kIdSysCkMenu {0x802E020C, (0x0C + (0x29 >> 5) * 4)};
constexpr uint32_t kIdMenuBit = 0x80000000u >> (0x29 & 31);
constexpr uint32_t kIdTitleBit = 0x80000000u >> (0x28 & 31);  // ID_TITLE, meme mot
constexpr nj::runtime::GameAddr kCinesco {0x8031BEE0, 0u - (0x629B)};         // opacite des bandes (r13 - 0x629B)
// FadeWork Fade[4] (0x802DFDD8, 0x24 octets) : cur (GXColor) a +0x08, flags (u16) a +0x18,
// bit 0 = en cours, bit 1 = reste dessine (game/fade.cpp).
constexpr nj::runtime::GameAddr kFade {0x802DFDD8};
// EventMgr EvtMgr (cManager<Event>) : pArray +0x00, nArray +0x04, size +0x08 ; Event : be_flag +0x00
// (bit 0 vivant), status +0x44 (include/event.h ; +0x44 verifie dans EventMgr::Run USA / Europe).
constexpr nj::runtime::GameAddr kEvtMgr {0x802DFA1C};

bool heap_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x81800000u; }

struct Mem {
    MMU& m;
    uint8_t u8(uint32_t a) { return m.read8(a); }
    uint32_t u32(uint32_t a) { return m.read32(a); }
    uint16_t u16(uint32_t a) { return m.read16(a); }
    float f32(uint32_t a) { return m.read_f32(a); }
};

// Mtx 3x4 a l'adresse a (lignes de 4 f32).
void load_mtx(Mem& mem, uint32_t a, float out[3][4]) {
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            out[r][c] = mem.f32(a + (r * 4 + c) * 4);
}

bool inverse(const float m[3][4], float o[3][4]) {
    const float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                      m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                      m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (std::fabs(det) < 1e-9f)
        return false;
    const float id = 1.0f / det;
    o[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * id;
    o[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * id;
    o[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * id;
    o[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * id;
    o[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * id;
    o[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * id;
    o[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * id;
    o[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * id;
    o[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * id;
    for (int r = 0; r < 3; ++r)
        o[r][3] = -(o[r][0] * m[0][3] + o[r][1] * m[1][3] + o[r][2] * m[2][3]);
    return true;
}

void mult(const float m[3][4], const float v[3], float out[3]) {
    for (int r = 0; r < 3; ++r)
        out[r] = m[r][0] * v[0] + m[r][1] * v[1] + m[r][2] * v[2] + m[r][3];
}

// Centre d'une case de la mallette, en pixels 640x480 : puzzlePos2screenPos (Sscrn/ss_pzzl.cpp)
// applique a (100 (x + 0.5), -100 (y + 0.5), 0) dans le repere de la grille (pzlBoard.mat).
bool cell_screen(const float board[3][4], const float inv_cam[3][4], float fovy, float x, float y,
                 float out[2]) {
    const float local[3] = {100.0f * (x + 0.5f), -100.0f * (y + 0.5f), 0.0f};
    float world[3], cam[3];
    mult(board, local, world);
    mult(inv_cam, world, cam);
    if (cam[2] > -1.0f)
        return false;
    const float h = std::fabs(cam[2]) * std::tan(fovy * 0.5f * 0.017453292f);
    const float w = h * 1.3333334f;
    out[0] = 320.0f + cam[0] * (320.0f / w);
    out[1] = 240.0f - cam[1] * (240.0f / h);
    return true;
}

bool read_board(Mem& mem, uint32_t g, uint32_t b, Re4Context::Grid& out);
void read_grid(Mem& mem, uint32_t g, Re4Context& c) {
    // pzlPlayer* SubScreenWk.x2B0 : caseBoard +0, spaceBoard +4, hand +0x10, cur +0x30 (include/puzzle.h)
    const uint32_t pl2 = mem.u32(kSubScreenWk + 0x2B0);
    if (heap_ptr(pl2)) {
        const uint32_t b0 = mem.u32(pl2 + 0x00), b1 = mem.u32(pl2 + 0x04), cur = mem.u32(pl2 + 0x30), hand = mem.u32(pl2 + 0x10);
        read_board(mem, g, b0, c.grids[0]);
        read_board(mem, g, b1, c.grids[1]);
        c.cur_board = cur == b1 && b1 ? 1 : 0;
        if (heap_ptr(hand)) {
            c.hand = true;
            c.hand_x = mem.f32(hand + 0x14);
            c.hand_y = mem.f32(hand + 0x18);
        }
    }

    // pzlPlayer* SubScreenWk.x2B0 ; caseBoard en +0 (include/puzzle.h)
    const uint32_t pl = mem.u32(kSubScreenWk + 0x2B0);
    if (!heap_ptr(pl))
        return;
    const uint32_t b = mem.u32(pl + 0x00);
    if (!heap_ptr(b))
        return;
    const int w = (int8_t)mem.u8(b + 0x04), h = (int8_t)mem.u8(b + 0x05);
    if (w <= 0 || h <= 0 || w > 32 || h > 32)
        return;
    float board[3][4], cam[3][4], inv[3][4];
    load_mtx(mem, b + 0x0C, board);
    load_mtx(mem, g + 0x74, cam);  // pG->Cam.mat
    const float fovy = mem.f32(g + 0x74 + 0xC0);  // pG->Cam.param.fovy
    if (!(fovy > 1.0f && fovy < 170.0f) || !inverse(cam, inv))
        return;
    float o[2], px[2], py[2];
    if (!cell_screen(board, inv, fovy, 0, 0, o) || !cell_screen(board, inv, fovy, 1, 0, px) ||
        !cell_screen(board, inv, fovy, 0, 1, py))
        return;
    c.grid = true;
    c.grid_w = w;
    c.grid_h = h;
    c.grid_cx = (int8_t)mem.u8(b + 0x3C);
    c.grid_cy = (int8_t)mem.u8(b + 0x3D);
    c.grid_origin[0] = o[0];
    c.grid_origin[1] = o[1];
    c.grid_ax[0] = px[0] - o[0];
    c.grid_ax[1] = px[1] - o[1];
    c.grid_ay[0] = py[0] - o[0];
    c.grid_ay[1] = py[1] - o[1];
}

// Grille d'une planche de la mallette (pzlBoard : w +4, h +5, matrice +0x0C, curseur +0x3C/+0x3D), projetee dans l'image 640x480.
bool read_board(Mem& mem, uint32_t g, uint32_t b, Re4Context::Grid& out) {
    if (!heap_ptr(b))
        return false;
    const int w = (int8_t)mem.u8(b + 0x04), h = (int8_t)mem.u8(b + 0x05);
    if (w <= 0 || h <= 0 || w > 32 || h > 32)
        return false;
    float board[3][4], cam[3][4], inv[3][4];
    load_mtx(mem, b + 0x0C, board);
    load_mtx(mem, g + 0x74, cam);  // pG->Cam.mat
    const float fovy = mem.f32(g + 0x74 + 0xC0);  // pG->Cam.param.fovy
    if (!(fovy > 1.0f && fovy < 170.0f) || !inverse(cam, inv))
        return false;
    float o[2], px[2], py[2];
    if (!cell_screen(board, inv, fovy, 0, 0, o) || !cell_screen(board, inv, fovy, 1, 0, px) ||
        !cell_screen(board, inv, fovy, 0, 1, py))
        return false;
    out.valid = true;
    out.w = w;
    out.h = h;
    out.cx = (int8_t)mem.u8(b + 0x3C);
    out.cy = (int8_t)mem.u8(b + 0x3D);
    out.origin[0] = o[0];
    out.origin[1] = o[1];
    out.ax[0] = px[0] - o[0];
    out.ax[1] = px[1] - o[1];
    out.ay[0] = py[0] - o[0];
    out.ay[1] = py[1] - o[1];
    return true;
}

// Message avec choix en attente (Message::code08) : les glyphes curseurs selCur[] donnent la
// position de chaque choix dans l'espace des messages, 512x384 (messageCamera, game/mes.cpp).
bool read_choices(Mem& mem, Re4Context& c) {
    for (int i = 0; i < 16; ++i) {
        const uint32_t m = kMes + i * kMesSize;
        if (!(mem.u32(m + 0x04) & 1))  // flags bit0 = actif
            continue;
        const int n = (int8_t)mem.u8(m + 0xE0);  // selNum
        if (n <= 0 || n > 8 || (int8_t)mem.u8(m + 0xE1) != 0)  // result deja rendu
            continue;
        int k = 0;
        for (int j = 0; j < n; ++j) {
            const uint32_t q = mem.u32(m + 0xC0 + j * 4);  // MesQue*
            if (!heap_ptr(q))
                break;
            c.choices[k].x = (float)(mem.m.read16(q + 0)) * 1.25f;
            c.choices[k].y = (float)(mem.m.read16(q + 2)) * 1.25f;
            ++k;
        }
        if (k != n)
            continue;
        c.choice_count = n;
        c.choice_cursor = (int8_t)mem.u8(m + 0xE2);
        c.choice_vertical = (mem.u32(m + 0x80) & 0x00800000) != 0;  // attr
        return true;
    }
    return false;
}

// Niveau de la mallette : l'inventaire (cItemMgr, 0x30 octets en .bss, adresse differente dans chaque version) est retrouve une fois en
// cherchant sa signature (pFlags tas, nFlags = 8, pItems tas, nItems = 0x180) ; les objets 0x7C..0x7F sont la mallette S, M, L, LL.
uint32_t find_item_mgr(Mem& mem) {
    static uint32_t found = 0;
    static bool tried = false;
    if (found || tried)
        return found;
    for (uint32_t a = 0x80200000u; a < 0x80300000u; a += 4) {
        if (mem.u32(a + 4) != 8 || mem.u32(a + 0x1C) != 0x180)
            continue;
        const uint32_t pf = mem.u32(a), pi = mem.u32(a + 0x14);
        if (heap_ptr(pf) && heap_ptr(pi) && pi != pf) {
            found = a;
            break;
        }
    }
    tried = found != 0;   // pas trouve (inventaire pas encore cree) : on recommence au prochain appel
    return found;
}

int case_level(Mem& mem) {
    static uint32_t last = 0;
    static int level = 0;
    const uint32_t now = SDL_GetTicks();
    if (now - last < 500)
        return level;
    last = now;
    const uint32_t mgr = find_item_mgr(mem);
    if (!mgr)
        return level;
    const uint32_t items = mem.u32(mgr + 0x14);
    if (!heap_ptr(items))
        return level;
    int best = -1;
    for (int i = 0; i < 0x180; ++i) {
        const uint32_t w = items + i * 0x0E;
        if (!(mem.u8(w + 4) & 1))
            continue;
        const uint16_t id = mem.u16(w);
        if (id >= 0x7C && id <= 0x7F)
            best = std::max<int>(best, id - 0x7C);
    }
    if (best >= 0)
        level = best;
    return level;
}

// (pG->wep_no, pG->wep_type) -> identifiant d'objet : lignes de wep_info (game/item.cpp). Chaque variante (silencieux, lunette,
// crosse, viseur laser) est un objet a part, avec son propre modele dans la mallette (ss/Item/idmXXX).
int weapon_item(int no, int type) {
    static const struct { uint8_t no, type; uint16_t id; } tbl[] = {
        {2, 0, 0x23}, {2, 1, 0x24}, {3, 0, 0x25}, {3, 2, 0x26}, {0x11, 0, 0x03}, {1, 0, 0x21}, {1, 1, 0x22}, {4, 0, 0x27}, {4, 1, 0x28},
        {5, 0, 0x29}, {7, 0, 0x2C}, {8, 0, 0x2D}, {9, 0, 0x2E}, {9, 1, 0x6B}, {9, 2, 0x99}, {0xA, 0, 0x2F}, {0xA, 1, 0x6C},
        {0xA, 2, 0x51}, {0xB, 0, 0x30}, {0xB, 1, 0x31}, {0xB, 2, 0x32}, {0xB, 3, 0x33}, {0xE, 0, 0x36}, {0xE, 1, 0xAB}, {6, 0, 0x2A},
        {6, 1, 0x2B}, {0xC, 0, 0x34}, {0xD, 0, 0x35}, {0xD, 1, 0x17}, {0xD, 2, 0x6D}, {0xF, 0, 0x37}, {0x10, 0, 0x38},
        {0x13, 0, 0x01}, {0x16, 0, 0x02}, {0x17, 0, 0x0E}, {0x19, 0, 0x08}, {0x1F, 0, 0x09}, {0x20, 0, 0x0A}, {0x21, 0, 0x94},
        {0x1C, 0, 0x52}};
    int base = -1;
    for (const auto& r : tbl) {
        if (r.no != no)
            continue;
        if (r.type == type)
            return r.id;
        if (base < 0)
            base = r.id;   // variante inconnue : l'arme de base
    }
    return base;
}

void read_prompt(Mem& mem, Re4Context& c) {
    if (!mem.u8(kActBtn + 0x42))  // active : une invite affichee a cette image
        return;
    const uint8_t btn = mem.u8(kCckptActionNo);
    // Type de l'invite (ActBtnWork.kind) : 0x18 = « marteler » (rocher, emrock.cpp), 0x25 = esquive.
    uint8_t kind = 0;
    bool found = false;
    const int num = std::min<int>(mem.u8(kActBtn + 0x40), 8);
    // Plusieurs invites peuvent partager le meme bouton (machine a ecrire : SAVE et une autre) : on garde la derniere dont le texte
    // est affiche (flags bit 3 = sans invite), sinon la premiere (16/10).
    for (int i = 0; i < num; ++i) {
        const uint32_t w = kActBtn + 0x44 + i * 0x18;
        if (mem.u8(w + 0x0F) != btn)
            continue;
        const bool shown = !(mem.u32(w + 0x14) & 8);
        const uint8_t k = mem.u8(w + 0x0C);
        if (!found || (shown && k >= kind)) {   // a egalite de bouton, le type le plus specifique (0 = TALK, le plus banal)
            kind = k;
            found = true;
        }
    }
    c.prompt = btn;
    c.prompt_kind = kind;
    // cActionButton::checkButton : Key 0x80000 = A, 0x40000 = B, 0x20000 = X, 0x10000 = Y,
    // 0xC00000 = L+R, 0xC0000 = A+B ; 9/0xA/0xB/0xD = affichage seul.
    switch (btn) {
    case 1: case 2: case 0xE:
        c.prompt_pad = 0x0100;
        if (kind == 0x18)
            c.qte = Re4Qte::Mash;
        else if (c.mode == Re4Mode::Event)
            c.qte = Re4Qte::Single;
        break;
    case 3: c.prompt_pad = 0x0040 | 0x0020; c.qte = Re4Qte::Pair; break;
    case 4: c.prompt_pad = 0x0100 | 0x0200; c.qte = Re4Qte::Pair; break;
    case 5: c.prompt_pad = 0x0200; c.qte = Re4Qte::Single; break;
    case 6: c.prompt_pad = 0x0400; c.qte = Re4Qte::Single; break;
    case 7: c.prompt_pad = 0x0800; c.qte = Re4Qte::Single; break;
    case 0xB: c.prompt_pad = 0x0100; c.qte = Re4Qte::Mash; break;  // PlGachaMove : A ou B
    default: break;
    }
}

// Jeu natif : l'etat envoye par le coeur (NJ_MSG_STATE) traduit en Re4Context. Les grilles de la
// mallette sont projetees ici, comme pour le jeu recompile (cell_screen).
bool board_from_native(const NjBoard& b, const float cam[3][4], float fovy, Re4Context::Grid& out) {
    if (!b.valid)
        return false;
    float board[3][4], inv[3][4];
    std::memcpy(board, b.mat, sizeof board);
    if (!(fovy > 1.0f && fovy < 170.0f) || !inverse(cam, inv))
        return false;
    float o[2], px[2], py[2];
    if (!cell_screen(board, inv, fovy, 0, 0, o) || !cell_screen(board, inv, fovy, 1, 0, px) ||
        !cell_screen(board, inv, fovy, 0, 1, py))
        return false;
    out.valid = true;
    out.w = b.w;
    out.h = b.h;
    out.cx = b.cx;
    out.cy = b.cy;
    out.origin[0] = o[0];
    out.origin[1] = o[1];
    out.ax[0] = px[0] - o[0];
    out.ax[1] = px[1] - o[1];
    out.ay[0] = py[0] - o[0];
    out.ay[1] = py[1] - o[1];
    return true;
}

NjState g_native_last{};   // dernier etat recu (pour re4_aiming, re4_motion_index)

void from_native(const NjState& st, Re4Context& c) {
    g_native_last = st;
    const uint32_t b = st.bits;
    c.valid = (b & NJS_VALID) != 0;
    if (!c.valid)
        return;
    c.mode = (Re4Mode)st.mode;
    c.qte = (Re4Qte)st.qte;
    c.skippable = b & NJS_SKIPPABLE;
    c.text_wait = b & NJS_TEXT_WAIT;
    c.card = b & NJS_CARD;
    c.death = b & NJS_DEATH;
    c.death_continue = b & NJS_DEATH_CONTINUE;
    c.inventory = b & NJS_INVENTORY;
    c.map = b & NJS_MAP;
    c.binoculars = b & NJS_BINOCULARS;
    c.title_menu = b & NJS_TITLE_MENU;
    c.title_menu_alpha = st.title_menu_alpha;
    c.fade = st.fade;
    c.wide = b & NJS_WIDE;
    c.player = b & NJS_PLAYER;
    c.aiming = b & NJS_AIMING;
    c.ashley = b & NJS_ASHLEY;
    c.ashley_follow = st.ashley_follow;
    c.aim_target = b & NJS_AIM_TARGET;
    c.aim_target_em = st.aim_target_em;
    c.aim_target_hp = st.aim_target_hp;
    c.wep_no = st.wep_no;
    c.wep_type = st.wep_type;
    c.wep_item = weapon_item(c.wep_no, c.wep_type);
    c.case_level = st.case_level;
    c.chapters_ended = st.chapters_ended;
    c.room = st.room;
    c.hp = st.hp;
    c.leon_mot = st.leon_mot;
    c.prompt = st.prompt;
    c.prompt_pad = st.prompt_pad;
    c.prompt_kind = st.prompt_kind;
    c.cursor = st.cursor;
    c.cursor_count = st.cursor_count;
    c.choice_count = st.choice_count > 8 ? 8 : st.choice_count;
    for (int i = 0; i < c.choice_count; ++i) {
        c.choices[i].x = st.choices[i][0];
        c.choices[i].y = st.choices[i][1];
    }
    c.choice_cursor = st.choice_cursor;
    c.choice_vertical = b & NJS_CHOICE_VERTICAL;
    float cam[3][4];
    std::memcpy(cam, st.cam, sizeof cam);
    board_from_native(st.boards[0], cam, st.fovy, c.grids[0]);
    board_from_native(st.boards[1], cam, st.fovy, c.grids[1]);
    c.cur_board = st.cur_board;
    c.hand = b & NJS_HAND;
    c.hand_x = st.hand_x;
    c.hand_y = st.hand_y;
    if (c.grids[0].valid) {   // ancienne grille unique = la mallette
        const auto& g0 = c.grids[0];
        c.grid = true;
        c.grid_w = g0.w;
        c.grid_h = g0.h;
        c.grid_cx = g0.cx;
        c.grid_cy = g0.cy;
        std::memcpy(c.grid_origin, g0.origin, sizeof c.grid_origin);
        std::memcpy(c.grid_ax, g0.ax, sizeof c.grid_ax);
        std::memcpy(c.grid_ay, g0.ay, sizeof c.grid_ay);
    }
}

} // namespace

// Joy[0] (0x802E8B30, include/joy.h) : vib_state +0x64 ; adresse et champs verifies en USA / Europe.
int re4_vib_state() {
    if (nj::native::loaded())
        return (int)nj::native::rumble();
    if (!g_guest)
        return 0;
    Mem mem{g_guest->mmu};
    return mem.u8(nj::runtime::ga(0x802E8B30) + 0x64);
}

bool re4_aiming() {
    if (nj::native::loaded()) {
        NjState st;
        return nj::native::game_state(st) && (st.bits & NJS_AIMING);
    }
    if (!g_guest)
        return false;
    Mem mem{g_guest->mmu};
    const uint32_t pl = mem.u32(kPPL);
    return heap_ptr(pl) && mem.u8(pl + 0xFD) == 6;
}

void re4_case_set_cursor(int board, int i, int j) {
    if (nj::native::loaded()) {
        const int32_t ij[2] = {i, j};
        nj::native::case_command(1, board, ij, sizeof ij);
        return;
    }
    if (!g_guest)
        return;
    Mem mem{g_guest->mmu};
    const uint32_t pl = mem.u32(kSubScreenWk + 0x2B0);
    if (!heap_ptr(pl))
        return;
    const uint32_t b = mem.u32(pl + (board ? 0x04 : 0x00));
    if (!heap_ptr(b))
        return;
    mem.m.write8(b + 0x3C, (uint8_t)(int8_t)i);
    mem.m.write8(b + 0x3D, (uint8_t)(int8_t)j);
    mem.m.write32(pl + 0x30, b);   // pzlPlayer::cur
}

void re4_case_set_hand(int board, float x, float y) {
    if (nj::native::loaded()) {
        const float xy[2] = {x, y};
        nj::native::case_command(2, board, xy, sizeof xy);
        return;
    }
    if (!g_guest)
        return;
    Mem mem{g_guest->mmu};
    const uint32_t pl = mem.u32(kSubScreenWk + 0x2B0);
    if (!heap_ptr(pl))
        return;
    const uint32_t b = mem.u32(pl + (board ? 0x04 : 0x00)), hand = mem.u32(pl + 0x10);
    if (!heap_ptr(b) || !heap_ptr(hand))
        return;
    mem.m.write_f32(hand + 0x14, x);   // pzlPiece::x, y : centre de la piece en cases
    mem.m.write_f32(hand + 0x18, y);
    mem.m.write32(pl + 0x30, b);
    // le jeu recalcule le curseur ainsi apres un deplacement (pzlPlayer::movePiece)
    const int w = (int8_t)mem.u8(b + 0x04), h = (int8_t)mem.u8(b + 0x05);
    int ci = (int)(x + 0.5f), cj = (int)(y + 0.5f);
    ci = ci < 0 ? 0 : ci > w - 1 ? w - 1 : ci;
    cj = cj < 0 ? 0 : cj > h - 1 ? h - 1 : cj;
    mem.m.write8(b + 0x3C, (uint8_t)ci);
    mem.m.write8(b + 0x3D, (uint8_t)cj);
}

int re4_motion_index(uint32_t motion) {
    if (nj::native::loaded())
        return motion && motion == g_native_last.leon_mot ? g_native_last.leon_mot_index : -1;
    if (!g_guest || !heap_ptr(motion))
        return -1;
    Mem mem{g_guest->mmu};
    const uint32_t pl = mem.u32(kPPL);
    if (!heap_ptr(pl))
        return -1;
    for (int t = 0; t < 2; ++t) {   // pMotTbl, puis pRegistMot (include/em.h : tables de pointeurs de MotionData)
        const uint32_t tbl = mem.u32(pl + (t ? 0x428 : 0x424));
        if (!heap_ptr(tbl))
            continue;
        for (int i = 0; i < 192; ++i)
            if (mem.u32(tbl + 4 * i) == motion)
                return 1000 * (t + 1) + i;
    }
    return -1;
}

Re4Context re4_context_read() {
    Re4Context c;
    if (nj::native::loaded()) {   // jeu natif : l'etat vient du coeur (port/njstate.cpp)
        NjState st;
        if (nj::native::game_state(st))
            from_native(st, c);
        return c;
    }
    if (!g_guest)
        return c;
    Mem mem{g_guest->mmu};
    const uint32_t g = mem.u32(kPG);
    if (!heap_ptr(g))
        return c;
    c.valid = true;
    c.wep_no = mem.u8(g + nj::runtime::pg(0x4FB0));
    c.wep_type = mem.u8(g + nj::runtime::pg(0x4FB1));
    c.wep_item = weapon_item(c.wep_no, c.wep_type);
    c.chapters_ended = mem.u8(g + nj::runtime::pg(0x4F8A));
    c.case_level = case_level(mem);
    c.room = mem.u16(g + nj::runtime::pg(0x4F9C));
    c.hp = (int16_t)mem.u16(g + nj::runtime::pg(0x4FA4));
    if (const uint32_t pl0 = mem.u32(kPPL); heap_ptr(pl0))
        c.leon_mot = mem.u32(pl0 + 0x1D8);

    const uint32_t f54 = mem.u32(g + 0x54), f170 = mem.u32(g + 0x170);
    const uint32_t f500C = mem.u32(g + nj::runtime::pg(0x500C)), f5014 = mem.u32(g + nj::runtime::pg(0x5014));
    const uint8_t step = mem.u8(g + 0x20);  // 3 jeu, 4 porte, 5 fin, 6 options/pause
    const uint32_t sub_type = mem.u32(kSubScreenWk + 0x2C);

    c.title_menu = mem.u32(kTask0Func) == kTitleTask && (mem.u32(kIdSysCkMenu) & kIdMenuBit) &&
                   !(mem.u32(kSofdecFlag) & 1);
    if (c.title_menu) {
        // Opacite reelle du menu : IdSys.num (+0x00), IdSys.pUnit (+0x4C), IdUnit de 0x138 octets,
        // flags +0x00 (0xFF libre, bit 0 vivant), type +0x02, col[3] (f32, 0..255) +0xF4.
        const int num = (int)mem.u32(nj::runtime::ga(0x802E020C));
        const uint32_t units = mem.u32(nj::runtime::ga(0x802E020C) + 0x4C);
        if (heap_ptr(units) && num > 0 && num <= 0x400) {
            for (int i = 0; i < num; ++i) {
                const uint32_t u = units + i * 0x138;
                const uint8_t fl = mem.u8(u);
                if (fl == 0xFF || !(fl & 1) || mem.u8(u + 2) != 0x29)
                    continue;
                c.title_menu_alpha = std::max(c.title_menu_alpha, mem.f32(u + 0xF4) / 255.0f);
            }
        }
    }
    for (int i = 0; i < 4; ++i) {
        const uint32_t f = kFade + i * 0x24;
        const uint16_t flags = (uint16_t)(mem.u32(f + 0x18) >> 16);
        if (flags & 3)
            c.fade = std::max(c.fade, mem.u8(f + 0x08 + 3) / 255.0f);
    }

    // 16:9 : opacite des bandes cinesco (lbz r31,-0x629B(r13) dans Draw_cinesco), hors video.
    // Les ecrans du GameCube sont dessines dans la meme bande (lignes 56..391, mesure du 28/09 :
    // mallette, carte, pause/options), comme les montre la Wii Edition en 16:9
    // (docs/SEIZE_NEUVIEMES.md).
    // step = index de game_func_tbl (game/game.cpp) : 5 gameEnding, 6 gameOption (pause et
    // options). Les deux etaient inverses ici : les options de la pause restaient en 4:3 (30/09).
    // Ecran titre : tout le titre, pas seulement son image (deja un 16:9 entre deux bandes) : les
    // menus LOAD (sauvegardes) et OPTIONS y sont dessines dans la bande (capture du 30/09, lignes
    // 56..392), et la Wii Edition recadre partout (bit 16:9 reapplique a chaque retour de menu).
    {
        const bool movie = mem.u32(kSofdecFlag) & 1;
        const bool sub = sub_type != 0 || (f5014 & 0x04000000);
        // Marchand (sous-ecran 0x10) compris, a la demande (30/09) : a verifier en image.
        const bool sub_band = sub;
        const bool title = mem.u32(kTask0Func) == kTitleTask;
        const bool game = mem.u8(kCinesco) != 0 && !sub && step != 5 && step != 6 && !title;
        // Fin de chapitre : SceChapterEnd (game/sce_com.cpp:741) met pG->flags_58 a 0xFFFFFFFF
        // sauf 0x2000, 0x800 et 0x10000 le temps de l'ecran ; aucun autre moment du jeu n'allume
        // presque tous ces bits. Les bandes cinesco n'y sont pas dessinees, d'ou ce test a part.
        const uint32_t f58 = mem.u32(g + 0x58);
        const bool chapter_end = (f58 | 0x12800u) == 0xFFFFFFFFu;
        // Videos : le lecteur passe d'ordinaire par le XFB (main.cpp, deja en 16:9) ; celles
        // dessinees dans l'EFB le sont dans la meme bande 56..392 : 16:9 aussi (30/09).
        // Fin de partie (step 5, gameEnding) comprise, a la demande (30/09) : a verifier en image.
        c.wide = movie || title || game || sub_band || chapter_end || step == 5 || step == 6;
    }

    if (mem.u32(kSofdecFlag) & 1)
        c.mode = Re4Mode::Movie;
    else if (mem.u32(kTask0Func) == kTitleTask || (f5014 & 0x8000) || (f500C & 0x100000) ||
             step == 6 || step == 5)
        c.mode = Re4Mode::Menu;  // titre, fin de partie, options, generique
    else if (sub_type == 0x10)
        c.mode = Re4Mode::Shop;
    else if (sub_type != 0 || (f5014 & 0x04000000))
        c.mode = Re4Mode::SubScreen;
    else if (f500C & 0x1000)
        // Pose au debut d'une cinematique, efface a la fin (game/sce_com.cpp:154 et :194).
        // flags_54 0x800 et flags_170 0x100 ne conviennent pas : le premier est pose par toute
        // salle (game.cpp:552), le second aussi par la mallette (sscrn.cpp:366).
        c.mode = Re4Mode::Event;
    else
        c.mode = Re4Mode::Gameplay;

    if (read_choices(mem, c) && (c.mode == Re4Mode::Gameplay || c.mode == Re4Mode::Event))
        c.mode = Re4Mode::Menu;  // dialogue Oui/Non en jeu (machine a ecrire, porte verrouillee...)

    // ---- situations precises (14/10) ----
    // Carte memoire : CardMainTask pose pG->flags_54 0x1000 tout le temps de l'ecran (card.cpp).
    c.card = (f54 & 0x1000) != 0;
    if (c.card && c.mode == Re4Mode::Gameplay)
        c.mode = Re4Mode::Menu;
    // Mort : DiedemoExec pose pG->flags_500C 0x100000 ; « Continue? » = unites IdSys de type 0x2E.
    c.death = (f500C & 0x100000) != 0;
    if (c.death) {
        const int num = (int)mem.u32(nj::runtime::ga(0x802E020C));
        const uint32_t units = mem.u32(nj::runtime::ga(0x802E020C) + 0x4C);
        if (heap_ptr(units) && num > 0 && num <= 0x400)
            for (int i = 0; i < num && !c.death_continue; ++i) {
                const uint32_t u = units + i * 0x138;
                const uint8_t fl = mem.u8(u);
                c.death_continue = fl != 0xFF && (fl & 1) && mem.u8(u + 2) == 0x2E;
            }
    }
    // Carte : SubScreenOpen(2) (touche Z, sscrn.cpp) ou onglet Map (x265 == 2 : Keys, Weapons, Map,
    // Files, Exit) de la mallette.
    c.map = c.mode == Re4Mode::SubScreen && (sub_type == 2 || (sub_type == 1 && mem.u8(kSubScreenWk + 0x265) == 2));
    c.inventory = c.mode == Re4Mode::SubScreen && sub_type == 1 && !c.map;
    // Cinematique annulable : EventMgr::Run n'appelle RunEvtCancel (Start) que si l'evenement n'a
    // aucun des bits 0x02000000, 0x04000000, 0x00800000, 0x100, ni 0x20000000 / 0x80000000.
    if (c.mode == Re4Mode::Event) {
        const uint32_t arr = mem.u32(kEvtMgr), n = mem.u32(kEvtMgr + 4), sz = mem.u32(kEvtMgr + 8);
        if (heap_ptr(arr) && n > 0 && n <= 64 && sz >= 0x48 && sz <= 0x400)
            for (uint32_t i = 0; i < n && !c.skippable; ++i) {
                const uint32_t e = arr + i * sz;
                if (!(mem.u32(e) & 1))
                    continue;
                const uint32_t st = mem.u32(e + 0x44);
                c.skippable = !(st & (0x80000000u | 0x20000000u | 0x02000000u | 0x04000000u | 0x00800000u | 0x100u));
            }
    }
    // Texte qui attend A : message actif, code08 commence (x70), attente finie (waitCnt), sans choix.
    if (c.choice_count == 0)
        for (int i = 0; i < 16 && !c.text_wait; ++i) {
            const uint32_t m = kMes + i * kMesSize;
            c.text_wait = (mem.u32(m + 0x04) & 1) && mem.u8(m + 0x70) && !mem.u16(m + 0x6E) && !mem.u8(m + 0xE0);
        }

    if (c.mode == Re4Mode::Gameplay || c.mode == Re4Mode::Event) {
        const uint32_t pl = mem.u32(kPPL);
        c.player = heap_ptr(pl);
        {
            const uint32_t sub = mem.u32(kPSUB);
            c.ashley = heap_ptr(sub) && mem.u8(sub + 0x100) == 3;
        }
        if (heap_ptr(pl)) {
            c.aiming = mem.u8(pl + 0xFD) == 6;  // xFD 6 = arme en main (visee)
            // Jumelles : flags_420 bit 2 (0x4) pose par cPlayer::moveBinocular / binoMode (+0x51F) != 0
            c.binoculars = (mem.u32(pl + 0x420) & 4) != 0 || mem.u8(pl + 0x51F) != 0;
        }
        if (c.aiming && heap_ptr(pl)) {
            // pPL->pWep (+0x788) -> pObj (+0x34) -> wep.target (+0x364, include/obj.h)
            const uint32_t w = mem.u32(pl + 0x788);
            const uint32_t o = heap_ptr(w) ? mem.u32(w + 0x34) : 0;
            const uint32_t em = heap_ptr(o) ? mem.u32(o + 0x364) : 0;
            c.aim_target = heap_ptr(em);
            if (c.aim_target) {
                c.aim_target_em = em;
                c.aim_target_hp = (int16_t)mem.u16(em + 0x320);
            }
        }
        read_prompt(mem, c);
        {   // consigne d'Ashley (20/10) : l'invite du bouton d'Ashley dit ce que le prochain appui fera
            const int num = std::min<int>(mem.u8(kActBtn + 0x40), 8);
            for (int i = 0; i < num; ++i) {
                const uint8_t k = mem.u8(kActBtn + 0x44 + i * 0x18 + 0x0C);
                if (k == 0x1E) c.ashley_follow = 1;        // « Wait » : elle suit
                else if (k == 0x21) c.ashley_follow = 0;   // « Follow me » : elle attend
            }
        }
    }
    // Mallette ouverte (type 1), menu principal sur « mallette » (x265 == 1)
    if (c.mode == Re4Mode::SubScreen && sub_type == 1 && mem.u8(kSubScreenWk + 0x265) == 1)
        read_grid(mem, g, c);
    // NJ_CTX_LOG=1 : les valeurs brutes toutes les 2 s, pour verifier une adresse en jeu.
    static const bool log = NJ_ENV("NJ_CTX_LOG") != nullptr;
    static uint32_t last_log = 0;
    if (log && SDL_GetTicks() - last_log > 2000) {
        last_log = SDL_GetTicks();
        NJ_PRINTF("[CTX] mode=%d step=%u f54=%08x f170=%08x f500C=%08x f5014=%08x sub=%x task=%08x "
                    "sofdec=%x act=%u btn=%u qte=%d choices=%d grid=%d\n",
                    (int)c.mode, step, f54, f170, f500C, f5014, sub_type, mem.u32(kTask0Func),
                    mem.u32(kSofdecFlag), mem.u8(kActBtn + 0x42), c.prompt, (int)c.qte,
                    c.choice_count, (int)c.grid);
        std::fflush(stdout);
    }
    if (c.mode == Re4Mode::SubScreen) {
        c.cursor = (int8_t)mem.u8(kSubScreenWk + 0x265);
        c.cursor_count = 5;
    }
    return c;
}

} // namespace nj::runtime::input
