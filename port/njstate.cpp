// -*- coding: utf-8 -*-
// Etat du jeu pour les commandes tactiles de l'appli (pont d'etat du portage natif).
//
// En 0.1.9 l'appli lisait la memoire GameCube du jeu recompile (runtime/src/input/touch/re4_context.cpp)
// par adresses et decalages. Le jeu natif tourne dans un autre processus, et ses objets n'ont plus la
// disposition GameCube (clang range le pointeur de table virtuelle en tete, ARM64 a des pointeurs de
// 8 octets) : on lit donc ici, dans le coeur, les memes champs par leur nom (en-tetes du decompile),
// et on envoie le resultat a l'appli par njlink une fois par image (NJ_MSG_STATE). Les ecritures de
// l'appli (mallette au doigt) reviennent par NJ_MSG_CASE et sont appliquees par njstate_case().
// Chaque test reprend celui de re4_context.cpp, qui cite sa source dans le decompile.
#include <string.h>

#include "global.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_wep.h"
#include "obj.h"
#include "sscrn.h"
#include "puzzle.h"
#include "mes.h"
#include "id_sys.h"
#include "event.h"
#include "fade.h"
#include "joy.h"
#include "act_btn.h"
#include "cockpit.h"
#include "scheduler.h"
#include "sofdec.h"
#include "cinesco.h"
#include "title.h"

#include "njlink.h"
extern "C" void OSReport(const char* fmt, ...);

namespace {

// Re4Mode / Re4Qte de l'appli (runtime/include/input/re4_context.h)
enum { M_UNKNOWN, M_GAMEPLAY, M_EVENT, M_MOVIE, M_SUBSCREEN, M_SHOP, M_MENU };
enum { Q_NONE, Q_SINGLE, Q_PAIR, Q_MASH };

inline u32 id32(const void* p) { return (u32) (unsigned long) p; }

void copy_mtx(float out[12], const Mtx m)
{
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            out[r * 4 + c] = m[r][c];
}

void read_board(pzlBoard* b, NjBoard& out)
{
    if (!b || b->w <= 0 || b->h <= 0 || b->w > 32 || b->h > 32)
        return;
    out.valid = 1;
    out.w = b->w;
    out.h = b->h;
    out.cx = b->curX;
    out.cy = b->curY;
    copy_mtx(out.mat, b->mat);
}

bool read_choices(NjState& s)
{
    for (int i = 0; i < 16; ++i) {
        Message& m = cMes.mes[i];
        if (!(m.flags & 1))
            continue;
        const int n = m.selNum;
        if (n <= 0 || n > 8 || m.result != 0)
            continue;
        int k = 0;
        for (; k < n; ++k) {
            MesQue* q = m.selCur[k];
            if (!q)
                break;
            s.choices[k][0] = (float) q->x * 1.25f;   // espace des messages 512x384 -> 640x480
            s.choices[k][1] = (float) q->y * 1.25f;
        }
        if (k != n)
            continue;
        s.choice_count = (int8_t) n;
        s.choice_cursor = m.cursor;
        if (m.attr & 0x00800000)
            s.bits |= NJS_CHOICE_VERTICAL;
        return true;
    }
    return false;
}

void read_prompt(NjState& s)
{
    if (!ActBtn.active)
        return;
    const u8 btn = Cckpt.action.no;
    u8 kind = 0;
    bool found = false;
    const int num = ActBtn.num < 8 ? ActBtn.num : 8;
    for (int i = 0; i < num; ++i) {
        const ActBtnWork& w = ActBtn.work[i];
        if (w.btn != btn)
            continue;
        const bool shown = !(w.flags & 8);
        if (!found || (shown && w.kind >= kind)) {
            kind = w.kind;
            found = true;
        }
    }
    s.prompt = btn;
    s.prompt_kind = kind;
    switch (btn) {
    case 1: case 2: case 0xE:
        s.prompt_pad = 0x0100;
        if (kind == 0x18) s.qte = Q_MASH;
        else if (s.mode == M_EVENT) s.qte = Q_SINGLE;
        break;
    case 3: s.prompt_pad = 0x0040 | 0x0020; s.qte = Q_PAIR; break;
    case 4: s.prompt_pad = 0x0100 | 0x0200; s.qte = Q_PAIR; break;
    case 5: s.prompt_pad = 0x0200; s.qte = Q_SINGLE; break;
    case 6: s.prompt_pad = 0x0400; s.qte = Q_SINGLE; break;
    case 7: s.prompt_pad = 0x0800; s.qte = Q_SINGLE; break;
    case 0xB: s.prompt_pad = 0x0100; s.qte = Q_MASH; break;
    default: break;
    }
}

bool id_unit_alive(int type, float* alpha)
{
    bool any = false;
    if (!IdSys.pUnit || IdSys.num <= 0 || IdSys.num > 0x400)
        return false;
    for (int i = 0; i < IdSys.num; ++i) {
        IdUnit& u = IdSys.pUnit[i];
        if (u.flags == 0xFF || !(u.flags & 1) || u.type != type)
            continue;
        any = true;
        if (!alpha)
            return true;
        const float a = u.col[3] / 255.0f;
        if (a > *alpha) *alpha = a;
    }
    return any;
}

int motion_index(cPlayer* pl, void* motion)
{
    if (!pl || !motion)
        return -1;
    for (int t = 0; t < 2; ++t) {
        void** tbl = t ? pl->pRegistMot : pl->pMotTbl;
        if (!tbl)
            continue;
        for (int i = 0; i < 192; ++i)
            if (tbl[i] == motion)
                return 1000 * (t + 1) + i;
    }
    return -1;
}

}  // namespace

extern "C" void njstate_fill(NjState* out)
{
    NjState s;
    memset(&s, 0, sizeof s);
    s.ashley_follow = -1;
    s.cursor = -1;
    s.choice_cursor = -1;
    s.prompt_kind = -1;
    s.leon_mot_index = -1;
    s.vib_state = (int8_t) Joy[0].vib_state;
    GlobalWork* g = pG;
    if (!g) {
        *out = s;
        return;
    }
    s.bits |= NJS_VALID;
    s.wep_no = g->wep_no;
    s.wep_type = g->wep_type;
    s.chapters_ended = g->x4F8A;
    s.room = g->room_id;
    s.hp = (s16) g->pl_life;
    s.case_level = (int8_t) SubScreenWk.x2AE;
    copy_mtx(s.cam, g->Cam.mat);
    s.fovy = g->Cam.param.fovy;
    cPlayer* pl = pPL;
    if (pl) {
        s.leon_mot = id32(pl->pMotion);
        s.leon_mot_index = motion_index(pl, pl->pMotion);
    }

    const u32 f54 = g->flags_54, f500C = g->flags_500C, f5014 = g->flags_5014;
    const u8 step = g->x20;   // 3 jeu, 4 porte, 5 fin, 6 options/pause
    const s32 sub_type = SubScreenWk.type;
    const bool title = (void*) Task[0].func == (void*) &Title_task;
    const bool movie = (Sofdec.flag & 1) != 0;

    float menu_alpha = 0.0f;
    if (title && (IdSys.ck[0x29 >> 5] & (0x80000000u >> (0x29 & 31))) && !movie) {
        s.bits |= NJS_TITLE_MENU;
        id_unit_alive(0x29, &menu_alpha);
    }
    s.title_menu_alpha = menu_alpha;
    for (int i = 0; i < 4; ++i)
        if (Fade[i].flags & 3) {
            const float a = Fade[i].cur.a / 255.0f;
            if (a > s.fade) s.fade = a;
        }

    {   // 16:9 (re4_context.cpp, docs/SEIZE_NEUVIEMES.md)
        const bool sub = sub_type != 0 || (f5014 & 0x04000000);
        const bool game = cine_work.alpha != 0 && !sub && step != 5 && step != 6 && !title;
        const bool chapter_end = (g->flags_58 | 0x12800u) == 0xFFFFFFFFu;
        if (movie || title || game || sub || chapter_end || step == 5 || step == 6)
            s.bits |= NJS_WIDE;
    }

    if (movie) s.mode = M_MOVIE;
    else if (title || (f5014 & 0x8000) || (f500C & 0x100000) || step == 6 || step == 5) s.mode = M_MENU;
    else if (sub_type == 0x10) s.mode = M_SHOP;
    else if (sub_type != 0 || (f5014 & 0x04000000)) s.mode = M_SUBSCREEN;
    else if (f500C & 0x1000) s.mode = M_EVENT;
    else s.mode = M_GAMEPLAY;

    if (read_choices(s) && (s.mode == M_GAMEPLAY || s.mode == M_EVENT))
        s.mode = M_MENU;

    if (f54 & 0x1000) {
        s.bits |= NJS_CARD;
        if (s.mode == M_GAMEPLAY) s.mode = M_MENU;
    }
    if (f500C & 0x100000) {
        s.bits |= NJS_DEATH;
        if (id_unit_alive(0x2E, 0)) s.bits |= NJS_DEATH_CONTINUE;
    }
    const bool map = s.mode == M_SUBSCREEN && (sub_type == 2 || (sub_type == 1 && SubScreenWk.x265 == 2));
    if (map) s.bits |= NJS_MAP;
    if (s.mode == M_SUBSCREEN && sub_type == 1 && !map) s.bits |= NJS_INVENTORY;

    if (s.mode == M_EVENT && EvtMgr.pArray && EvtMgr.nArray > 0 && EvtMgr.nArray <= 64) {
        for (u32 i = 0; i < EvtMgr.nArray; ++i) {
            Event* e = (Event*) ((char*) EvtMgr.pArray + i * EvtMgr.size);
            if (!(e->be_flag & 1))
                continue;
            if (!(e->status & (0x80000000u | 0x20000000u | 0x02000000u | 0x04000000u | 0x00800000u | 0x100u))) {
                s.bits |= NJS_SKIPPABLE;
                break;
            }
        }
    }
    if (s.choice_count == 0)
        for (int i = 0; i < 16; ++i) {
            Message& m = cMes.mes[i];
            if ((m.flags & 1) && m.x70 && !m.waitCnt && !m.selNum) {
                s.bits |= NJS_TEXT_WAIT;
                break;
            }
        }

    if (s.mode == M_GAMEPLAY || s.mode == M_EVENT) {
        if (pl) s.bits |= NJS_PLAYER;
        if (pSUB && pSUB->id == 3) s.bits |= NJS_ASHLEY;
        if (pl) {
            if (pl->xFD == 6) s.bits |= NJS_AIMING;
            if ((pl->flags_420 & 4) || pl->binoMode != 0) s.bits |= NJS_BINOCULARS;
            if (pl->xFD == 6 && pl->pWep && pl->pWep->pObj) {
                cEm* em = pl->pWep->pObj->wep.target;
                if (em) {
                    s.bits |= NJS_AIM_TARGET;
                    s.aim_target_em = id32(em);
                    s.aim_target_hp = em->hp;
                }
            }
        }
        read_prompt(s);
        const int num = ActBtn.num < 8 ? ActBtn.num : 8;
        for (int i = 0; i < num; ++i) {
            const u8 k = ActBtn.work[i].kind;
            if (k == 0x1E) s.ashley_follow = 1;
            else if (k == 0x21) s.ashley_follow = 0;
        }
    }
    if (s.mode == M_SUBSCREEN && sub_type == 1 && SubScreenWk.x265 == 1) {
        if (pzlPlayer* pp = SubScreenWk.x2B0) {
            read_board(pp->caseBoard, s.boards[0]);
            read_board(pp->spaceBoard, s.boards[1]);
            s.cur_board = (pp->cur == pp->spaceBoard && pp->spaceBoard) ? 1 : 0;
            if (pp->hand) {
                s.bits |= NJS_HAND;
                s.hand_x = pp->hand->x;
                s.hand_y = pp->hand->y;
            }
        }
    }
    if (s.mode == M_SUBSCREEN) {
        s.cursor = (int8_t) SubScreenWk.x265;
        s.cursor_count = 5;
    }
    *out = s;
}

// Mallette au doigt (NJ_MSG_CASE) : memes ecritures que re4_case_set_cursor / re4_case_set_hand.
extern "C" void njstate_case(int op, int board, const void* data)
{
    pzlPlayer* pp = SubScreenWk.x2B0;
    if (!pp)
        return;
    pzlBoard* b = board ? pp->spaceBoard : pp->caseBoard;
    if (!b)
        return;
    if (op == 1) {
        int ij[2];
        memcpy(ij, data, sizeof ij);
        b->curX = (s8) ij[0];
        b->curY = (s8) ij[1];
        pp->cur = b;
    } else if (op == 2 && pp->hand) {
        float xy[2];
        memcpy(xy, data, sizeof xy);
        pp->hand->x = xy[0];
        pp->hand->y = xy[1];
        pp->cur = b;
        int ci = (int) (xy[0] + 0.5f), cj = (int) (xy[1] + 0.5f);   // pzlPlayer::movePiece
        ci = ci < 0 ? 0 : ci > b->w - 1 ? b->w - 1 : ci;
        cj = cj < 0 ? 0 : cj > b->h - 1 ? b->h - 1 : cj;
        b->curX = (s8) ci;
        b->curY = (s8) cj;
    }
}

// Diagnostic N3 (titre noir dans le coeur) : les unites IdSys vivantes et ce qui fait leur couleur.
extern "C" void njstate_debug_ids(void)
{
    if (!IdSys.pUnit || IdSys.num <= 0 || IdSys.num > 0x400)
        return;
    int shown = 0;
    for (int i = 0; i < IdSys.num && shown < 24; ++i) {
        IdUnit& u = IdSys.pUnit[i];
        if (u.flags == 0xFF || !(u.flags & 1))
            continue;
        ++shown;
        int knum = -1;
        float kt = -1.0f;
        if (u.curve[2]) {
            knum = u.curve[2]->num;
            if (knum > 0) kt = u.curve[2]->key[knum - 1].t;
        }
        OSReport("[IDS] %d type %02x id %02x fl %02x tex %u col %d,%d,%d,%d col0 %02x%02x%02x%02x col1 %02x%02x%02x%02x t2 %d courbe %d fin %d parent %d\n",
                 i, u.type, u.id, u.flags, (unsigned) u.texId, (int) u.col[0], (int) u.col[1], (int) u.col[2], (int) u.col[3],
                 u.col0[0], u.col0[1], u.col0[2], u.col0[3], u.col1[0], u.col1[1], u.col1[2], u.col1[3], (int) (s16) u.timer[2],
                 knum, (int) kt, u.parent ? (int) (u.parent - IdSys.pUnit) : -1);
    }
}
