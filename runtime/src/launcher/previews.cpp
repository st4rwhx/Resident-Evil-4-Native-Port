#include "previews.h"

#include "input/touch_overlay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace nj::rmenu::pv {

namespace {

void atlas_cell(const Ctx &c, int cell, ImVec2 m, float size, ImU32 tint, bool flip = false);   // defini plus bas (atlas tactile)

constexpr float kPi = 3.14159265f;

ImU32 col(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }
ImU32 alpha(ImU32 c, float k) { return (c & 0x00FFFFFF) | ((ImU32)(std::clamp(k, 0.0f, 1.0f) * 255.0f) << 24); }
ImU32 mix(ImU32 a, ImU32 b, float t) {
    auto ch = [&](int sh) { return (int)(((a >> sh) & 255) * (1.0f - t) + ((b >> sh) & 255) * t); };
    return IM_COL32(ch(0), ch(8), ch(16), 255);
}

float tsz(const Ctx &c, float k = 1.0f) { return 13.0f * c.dp * k; }

ImVec2 tsize(const Ctx &c, const char *s, float sz) { return c.font ? c.font->CalcTextSizeA(sz, 1e9f, 0.0f, s) : ImGui::CalcTextSize(s); }

void text(const Ctx &c, ImVec2 p, ImU32 col_, const char *s, float sz) {
    ImFont *f = c.font ? c.font : ImGui::GetFont();
    c.dl->AddText(f, sz, ImVec2(p.x + 1.0f * c.dp, p.y + 1.0f * c.dp), IM_COL32(0, 0, 0, 200), s);
    c.dl->AddText(f, sz, p, col_, s);
}

void text_c(const Ctx &c, ImVec2 mid, ImU32 col_, const char *s, float sz) {
    const ImVec2 w = tsize(c, s, sz);
    text(c, ImVec2(mid.x - w.x * 0.5f, mid.y - w.y * 0.5f), col_, s, sz);
}

// image du jeu (a l'envers) dans [p0, p1], recadree : uv0/uv1 en 0..1 (en haut a gauche -> en bas a droite de l'image reelle)
void game_image(const Ctx &c, ImVec2 p0, ImVec2 p1, float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f, float dim = 0.0f) {
    if (c.shot) {
        c.dl->AddImage(c.shot, p0, p1, ImVec2(u0, 1.0f - v0), ImVec2(u1, 1.0f - v1));
    } else {
        c.dl->AddRectFilledMultiColor(p0, p1, col(70, 62, 56), col(70, 62, 56), col(24, 20, 18), col(24, 20, 18));
        c.dl->AddCircleFilled(ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f), (p1.y - p0.y) * 0.18f, col(110, 96, 82));
    }
    if (dim > 0.0f) c.dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, (int)(dim * 255)));
}

ImVec2 fit_rect(ImVec2 a, ImVec2 b, float ar, float margin) {   // retourne la taille d'un rectangle d'aspect ar dans [a, b]
    const float w = b.x - a.x - 2 * margin, h = b.y - a.y - 2 * margin;
    return w / h > ar ? ImVec2(h * ar, h) : ImVec2(w, w / ar);
}

}  // namespace

// ================= Display / Graphics =================

void screen_fit(const Ctx &c, float screen_w, float screen_h, int aspect43, int fit, int ratio) {
    ImDrawList *dl = c.dl;
    static const float kR[] = {0.0f, 4.0f / 3, 3.0f / 2, 16.0f / 10, 16.0f / 9, 18.0f / 9, 19.5f / 9, 20.0f / 9, 21.0f / 9};
    dl->AddRectFilled(c.a, c.b, col(40, 10, 10));
    const float sar = std::max(0.5f, screen_w / std::max(1.0f, screen_h));
    const ImVec2 ssz = fit_rect(c.a, c.b, sar, 16 * c.dp);
    const ImVec2 mid((c.a.x + c.b.x) * 0.5f, (c.a.y + c.b.y) * 0.5f);
    const ImVec2 s0(mid.x - ssz.x * 0.5f, mid.y - ssz.y * 0.5f), s1(mid.x + ssz.x * 0.5f, mid.y + ssz.y * 0.5f);
    dl->AddRectFilled(s0, s1, col(0, 0, 0));
    const float ar = ratio > 0 && ratio < 9 ? kR[ratio] : (aspect43 ? 4.0f / 3 : 16.0f / 9);
    dl->PushClipRect(s0, s1, true);
    if (fit == 3) {
        game_image(c, s0, s1);
    } else if (fit == 2) {
        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
        if (sar > ar) { const float vf = ar / sar; v0 = 0.5f - vf * 0.5f; v1 = 0.5f + vf * 0.5f; }
        else { const float uf = sar / ar; u0 = 0.5f - uf * 0.5f; u1 = 0.5f + uf * 0.5f; }
        game_image(c, s0, s1, u0, v0, u1, v1);
    } else {
        ImVec2 isz = fit_rect(s0, s1, ar, 0.0f);
        if (fit == 0 && sar > ar) {   // automatique : un peu etire vers les cotes
            const float gain = std::min(sar / ar, 1.0f + (sar / ar - 1.0f) * 0.35f);
            isz.x = std::min(isz.x * gain, ssz.x);
        }
        game_image(c, ImVec2(mid.x - isz.x * 0.5f, mid.y - isz.y * 0.5f), ImVec2(mid.x + isz.x * 0.5f, mid.y + isz.y * 0.5f));
    }
    dl->PopClipRect();
    dl->AddRect(s0, s1, c.cream, 0.0f, 0, 2.0f * c.dp);
    char b[48];
    std::snprintf(b, sizeof b, "%.0f:%.0f  >  %.2f", screen_w, screen_h, ar);
    text_c(c, ImVec2(mid.x, c.b.y - 8 * c.dp), c.dim, b, tsz(c, 0.7f));
}

void fps_corner(const Ctx &c, int on) {
    game_image(c, c.a, c.b);
    if (on) {
        const ImVec2 p(c.a.x + 8 * c.dp, c.a.y + 6 * c.dp);
        c.dl->AddRectFilled(ImVec2(p.x - 3 * c.dp, p.y - 2 * c.dp), ImVec2(p.x + 78 * c.dp, p.y + 18 * c.dp), col(0, 0, 0, 150));
        text(c, p, col(120, 255, 140), "60 FPS", tsz(c, 0.9f));
    }
}

void button_icons(const Ctx &c, const IconSet &s, int style) {
    ImDrawList *dl = c.dl;
    const float w = c.b.x - c.a.x, h = c.b.y - c.a.y;
    if (style == 4 && c.atlas && c.atlas->id) {   // mobile : les boutons de l'atlas tactile (A B X Y L R Z Start)
        const int cells[2][4] = {{16, 17, 18, 19}, {20, 21, 22, 23}};
        for (int r = 0; r < 2; ++r)
            for (int k = 0; k < 4; ++k)
                atlas_cell(c, cells[r][k], ImVec2(c.a.x + w * (0.2f + 0.2f * k), c.a.y + h * (0.32f + 0.38f * r)), h * 0.30f, col(255, 255, 255, 240));
        return;
    }
    if (style != 0) {   // Xbox, clavier : le jeu n'a pas d'image pour eux
        const char *t = c.tr("Shown for the GameCube icons only.");
        const ImVec2 sz = c.font ? c.font->CalcTextSizeA(tsz(c, 0.8f), w * 0.9f, w * 0.9f, t) : ImGui::CalcTextSize(t);
        dl->AddText(c.font ? c.font : ImGui::GetFont(), tsz(c, 0.8f), ImVec2(c.a.x + (w - sz.x) * 0.5f, c.a.y + (h - sz.y) * 0.5f), c.dim, t, nullptr, w * 0.9f);
        return;
    }
    const Img *row1[] = {&s.stick, &s.a, &s.b, &s.x, &s.y};
    const Img *row2[] = {&s.l, &s.r, &s.start};
    auto draw_row = [&](const Img *const *im, int n, float cy, float hh) {   // hh : hauteur voulue des icones
        float total = 0.0f;
        float gap = hh * 0.45f;
        for (int i = 0; i < n; ++i) total += (im[i]->id ? hh * im[i]->w / std::max(1, im[i]->h) : 0.0f) + (i ? gap : 0.0f);
        if (total > w * 0.86f) {   // trop large pour le cadre : tout est reduit d'autant
            const float k = w * 0.86f / total;
            hh *= k; gap *= k; total *= k;
        }
        float x = c.a.x + (w - total) * 0.5f;
        for (int i = 0; i < n; ++i) {
            if (!im[i]->id) continue;
            const float iw = hh * im[i]->w / std::max(1, im[i]->h);
            dl->AddImage(im[i]->id, ImVec2(x, cy - hh * 0.5f), ImVec2(x + iw, cy + hh * 0.5f));
            x += iw + gap;
        }
    };
    draw_row(row1, 5, c.a.y + h * 0.32f, h * 0.26f);
    draw_row(row2, 3, c.a.y + h * 0.72f, h * 0.26f);
}



// ================= Controls : apercus dans le style du jeu =================
// Fond : l'image du jeu assombrie et teintee de rouge (comme derriere les menus). Pictogrammes : contour blanc a liseré noir, comme les symboles
// du HUD (« ? », « ! », silhouettes). Laser : le point rouge du viseur laser du jeu.
namespace {

void tinted_backdrop(const Ctx &c, float dim = 0.5f) {
    game_image(c, c.a, c.b, 0, 0, 1, 1, dim);
    c.dl->AddRectFilled(c.a, c.b, col(40, 6, 6, 110));
    const float h = c.b.y - c.a.y;
    c.dl->AddRectFilledMultiColor(c.a, ImVec2(c.b.x, c.a.y + h * 0.25f), col(0, 0, 0, 150), col(0, 0, 0, 150), col(0, 0, 0, 0), col(0, 0, 0, 0));
    c.dl->AddRectFilledMultiColor(ImVec2(c.a.x, c.b.y - h * 0.25f), c.b, col(0, 0, 0, 0), col(0, 0, 0, 0), col(0, 0, 0, 160), col(0, 0, 0, 160));
}

// Traits a contour : liseré noir sous un trait blanc (symboles du HUD).
float hud_w(const Ctx &c) { return 2.0f * c.dp; }
void hud_line(const Ctx &c, ImVec2 p, ImVec2 q) {
    c.dl->AddLine(p, q, col(0, 0, 0, 230), hud_w(c) + 3.0f * c.dp);
    c.dl->AddLine(p, q, col(255, 255, 255), hud_w(c));
}
void hud_rect(const Ctx &c, ImVec2 p, ImVec2 q, float round) {
    c.dl->AddRect(p, q, col(0, 0, 0, 230), round, 0, hud_w(c) + 3.0f * c.dp);
    c.dl->AddRect(p, q, col(255, 255, 255), round, 0, hud_w(c));
}
void hud_circle(const Ctx &c, ImVec2 m, float r) {
    c.dl->AddCircle(m, r, col(0, 0, 0, 230), 32, hud_w(c) + 3.0f * c.dp);
    c.dl->AddCircle(m, r, col(255, 255, 255), 32, hud_w(c));
}

void laser(const Ctx &c, ImVec2 from, ImVec2 to) {
    c.dl->AddLine(from, to, col(220, 20, 16, 40), 7.0f * c.dp);
    c.dl->AddLine(from, to, col(255, 40, 30, 150), 1.6f * c.dp);
    for (int i = 3; i >= 1; --i) c.dl->AddCircleFilled(to, 3.0f * c.dp * i, col(255, 30, 24, 50 + 30 * (3 - i)), 16);
    c.dl->AddCircleFilled(to, 2.2f * c.dp, col(255, 220, 200), 12);
}

}  // namespace

// what : 0 « Aim with » (le mode choisi), 1 « Stick aim speed » (deux points : la vitesse d'origine et la vitesse choisie, avec la meme entree du stick),
// 2 « Aim camera » (cam 0 : la camera suit le viseur ; 1 : elle reste fixe).
// Visee : le pistolet (icone Aim de l'atlas), le laser rouge, le curseur de visee de l'atlas (pointeur libre), le vrai stick du jeu.
// what : 0 « Aim with » (le mode choisi), 1 « Stick aim speed » (deux points : vitesse d'origine et vitesse choisie, meme entree du stick),
// 2 « Aim camera » (cam 0 : la camera suit le viseur ; 1 : elle reste fixe).
void aim_demo(const Ctx &c, int mode, int speed, int what, int cam) {
    ImDrawList *dl = c.dl;
    const float w = c.b.x - c.a.x, h = c.b.y - c.a.y, tt = (float)c.t;
    const bool have_atlas = c.atlas && c.atlas->id;
    auto input = [&](float t, float &ix, float &iy) { ix = std::sin(t * 1.15f); iy = std::sin(t * 0.8f + 1.0f); };
    auto reach = [&](int sp) { return 0.20f + 0.10f * sp; };   // portee selon la vitesse (jusqu'au double en bout de course)
    float ix, iy;
    input(tt, ix, iy);
    const float main_reach = what == 1 ? reach(speed) : (mode == 1 ? reach(0) : 0.38f);
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    if (what == 2 && cam == 0) {   // la camera suit : l'image glisse et grossit vers ou l'on vise
        const float z = 0.86f, ox = (0.5f - z * 0.5f) + ix * main_reach * 0.55f, oy = (0.5f - z * 0.5f) + iy * main_reach * 0.44f;
        u0 = ox; v0 = oy; u1 = ox + z; v1 = oy + z;
    }
    game_image(c, c.a, c.b, u0, v0, u1, v1, 0.4f);
    dl->AddRectFilled(c.a, c.b, col(40, 6, 6, 90));
    const ImVec2 mid(c.a.x + w * 0.38f, c.a.y + h * 0.40f);   // le point reste a gauche du pistolet
    auto pos = [&](float t, float rk) {
        float x, y;
        input(t, x, y);
        return ImVec2(mid.x + x * rk * w * 0.62f, mid.y + y * rk * h * 0.5f);
    };
    const ImVec2 dot = pos(tt, main_reach);
    // le pistolet en bas a droite ; le laser part de sa bouche (a gauche de l'icone)
    const float gs = h * 0.36f;
    const ImVec2 gun(c.b.x - w * 0.15f, c.b.y - h * 0.20f);
    const bool real_gun = c.icons && c.icons->gun.id;
    const ImVec2 muzzle = real_gun ? ImVec2(gun.x - gs * 0.44f, gun.y - gs * 0.10f) : ImVec2(gun.x - gs * 0.30f, gun.y - gs * 0.17f);
    if (what == 1) {   // « Original » : le point blanc a la vitesse d'origine, pour comparer
        const ImVec2 g = pos(tt, reach(0));
        if (have_atlas) atlas_cell(c, 46, g, h * 0.15f, col(255, 255, 255, 230));
        text(c, ImVec2(g.x + 12 * c.dp, g.y - 20 * c.dp), c.cream, c.tr("Original"), tsz(c, 0.75f));
    }
    for (int i = 6; i >= 1; --i) dl->AddCircleFilled(pos(tt - i * 0.05f, main_reach), 2.0f * c.dp, col(255, 40, 30, 40 + (6 - i) * 10), 8);
    laser(c, muzzle, dot);
    if (have_atlas) {
        if (what == 2 || (what == 0 && mode != 1)) atlas_cell(c, 46, dot, h * 0.17f, col(255, 255, 255, 235));   // pointeur libre : le curseur de visee
        dl->AddRectFilled(ImVec2(gun.x - gs * 0.52f, gun.y - gs * 0.40f), ImVec2(gun.x + gs * 0.52f, gun.y + gs * 0.40f), col(0, 0, 0, 150), 8.0f * c.dp);
        dl->AddRect(ImVec2(gun.x - gs * 0.52f, gun.y - gs * 0.40f), ImVec2(gun.x + gs * 0.52f, gun.y + gs * 0.40f), c.line, 8.0f * c.dp, 0, 1.2f * c.dp);
        if (!(c.icons && c.icons->gun.id))
            atlas_cell(c, 3, gun, gs * 1.05f, col(245, 240, 230, 255));                                          // sans le disque : l'icone Aim de l'atlas
    }
    if (real_gun) {   // le vrai pistolet du jeu (modele du disque, rendu chez le joueur), par-dessus la plaque
        const float g2 = gs * 1.25f;
        dl->AddImage(c.icons->gun.id, ImVec2(gun.x - g2 * 0.5f, gun.y - g2 * 0.5f), ImVec2(gun.x + g2 * 0.5f, gun.y + g2 * 0.5f));
    }
    // la source de l'entree en bas a gauche : le vrai stick du jeu, seulement quand c'est lui qui vise
    const bool stick = what == 1 || (what == 0 && mode == 1);
    if (stick && c.icons && c.icons->stick.id) {
        const ImVec2 dv(c.a.x + w * 0.13f, c.b.y - h * 0.24f);
        const float s = h * 0.15f, tilt = s * 0.38f;
        dl->AddCircleFilled(dv, s * 1.35f, col(0, 0, 0, 130), 24);
        const ImVec2 m(dv.x + ix * tilt, dv.y + iy * tilt);
        dl->AddImage(c.icons->stick.id, ImVec2(m.x - s, m.y - s), ImVec2(m.x + s, m.y + s));
    }
}

// Vibration : la croix, A et B de l'atlas (une manette en pictos) qui tremblent a chaque « coup », avec les traits d'impact de l'atlas ;
// l'intensite en trois crans, sur le rail de la luminosite du jeu.
void vibration(const Ctx &c, int level) {
    ImDrawList *dl = c.dl;
    tinted_backdrop(c, 0.62f);
    const float w = c.b.x - c.a.x, h = c.b.y - c.a.y;
    const float amp = level == 2 ? 5.0f : level == 1 ? 2.2f : 0.0f;
    const float ph = std::fmod((float)c.t, 2.0f);
    const bool burst = ph < 0.8f;
    const float decay = burst ? 1.0f - ph / 0.8f : 0.0f;
    const float sh = burst ? std::sin((float)c.t * 95.0f) * amp * c.dp * (0.4f + 0.6f * decay) : 0.0f;
    if (c.atlas && c.atlas->id) {
        const float s = h * 0.30f;
        const float y = c.a.y + h * 0.40f + sh * 0.6f;
        const float x0 = c.a.x + w * 0.5f + sh;
        atlas_cell(c, 29, ImVec2(x0 - s * 1.1f, y), s, col(255, 255, 255, 235));    // croix
        atlas_cell(c, 17, ImVec2(x0 + s * 0.15f, y + s * 0.15f), s * 0.8f, col(255, 255, 255, 235));   // B
        atlas_cell(c, 16, ImVec2(x0 + s * 1.15f, y - s * 0.05f), s, col(255, 255, 255, 235));          // A
        if (burst && level > 0) {   // traits d'impact autour de la manette
            const int n = level == 2 ? 2 : 1;
            for (int i = 0; i < n; ++i) {
                atlas_cell(c, 41, ImVec2(x0 - s * 1.9f, y - s * 0.55f), s * (1.0f + 0.5f * i + (1.0f - decay) * 0.4f), col(255, 90, 70, (int)(230 * decay)), false);
                atlas_cell(c, 41, ImVec2(x0 + s * 1.95f, y - s * 0.55f), s * (1.0f + 0.5f * i + (1.0f - decay) * 0.4f), col(255, 90, 70, (int)(230 * decay)), true);
            }
        }
    }
    // l'intensite en trois crans (rail de la luminosite du jeu : paliers de gris, repere rouge)
    const float bw = w * 0.56f, bx = c.a.x + (w - bw) * 0.5f, by = c.b.y - h * 0.17f;
    for (int k = 0; k < 12; ++k) {
        const int gr = 20 + k * 16;
        dl->AddRectFilled(ImVec2(bx + bw * k / 12.0f, by), ImVec2(bx + bw * (k + 1) / 12.0f, by + 9 * c.dp), col(gr, gr, gr));
    }
    dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + 9 * c.dp), c.line, 0.0f, 0, 1.0f * c.dp);
    const float mk = bx + bw * (level == 0 ? 0.04f : level == 1 ? 0.5f : 0.96f);
    dl->AddLine(ImVec2(mk, by - 5 * c.dp), ImVec2(mk, by + 14 * c.dp), c.red, 3.0f * c.dp);
    const char *names[3] = {"Off", "Light", "Strong"};
    for (int k = 0; k < 3; ++k) {
        const float x = bx + bw * (k == 0 ? 0.04f : k == 1 ? 0.5f : 0.96f);
        text_c(c, ImVec2(x, by + 24 * c.dp), k == level ? c.cream : c.dim, c.tr(names[k]), tsz(c, 0.75f));
    }
}


namespace {
// Une case de l'atlas tactile au centre m, cote `size`, teinte rgba (alpha comprise).
void atlas_cell(const Ctx &c, int cell, ImVec2 m, float size, ImU32 tint, bool flip) {
    const Atlas &a = *c.atlas;
    const int cols = std::max(1, a.w / a.cell);
    const float u0 = (float)(cell % cols) * a.cell / a.w, v0 = (float)(cell / cols) * a.cell / a.h;
    const float u1 = u0 + (float)a.cell / a.w, v1 = v0 + (float)a.cell / a.h;
    const float h = size * 0.5f;
    c.dl->AddImage(a.id, ImVec2(m.x - h, m.y - h), ImVec2(m.x + h, m.y + h), ImVec2(flip ? u1 : u0, v0), ImVec2(flip ? u0 : u1, v1), tint);
}
ImU32 with_a(ImU32 rgb, float a) { return (rgb & 0x00FFFFFF) | ((ImU32)(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f) << 24); }
}  // namespace

void touch_buttons(const Ctx &c, int style, int opacity, int simple, int size, int action_always) {
    using namespace nj::runtime::input::touch;
    if (!c.atlas || !c.atlas->id) return;
    game_image(c, c.a, c.b, 0, 0, 1, 1, 0.35f);
    const float W = 16.0f, H = 9.0f;                                   // ecran virtuel 16:9 ; unite du jeu = min(W, H) / 9 * taille
    const float u = std::clamp(size, 40, 160) / 100.0f;
    const float sc = (c.b.x - c.a.x) / W;
    auto at = [&](float x, float y) { return ImVec2(c.a.x + x * sc, c.a.y + y * sc); };
    const bool styled = style != 0, gold = style == 2;
    const float op = std::clamp(opacity, 20, 100) / 100.0f, lens_k = 0.35f + 0.65f * op;
    const int piece = c.atlas->style_first + (gold ? 2 : 0);
    const ImU32 ink = gold ? IM_COL32(0xFF, 0xD8, 0x84, 255) : IM_COL32(0xFF, 0xF6, 0xE2, 255);
    const ImU32 white = IM_COL32(255, 255, 255, 255);
    auto button = [&](int icon, float x, float y, float size_u) {
        const ImVec2 m = at(x, y);
        const float px = size_u * sc;
        if (styled) {
            atlas_cell(c, piece, m, px, with_a(white, lens_k));
            atlas_cell(c, icon, m, px, with_a(ink, std::min(1.0f, op + 0.15f)));
            atlas_cell(c, piece + 1, m, px, with_a(white, op));
        } else {
            atlas_cell(c, icon, m, px, with_a(white, std::min(1.0f, op + 0.15f)));
        }
    };
    // stick au repos
    {
        const ImVec2 m = at(2.6f * u, H - 2.6f * u);
        const float r = 1.5f * u * sc;
        if (styled) {
            atlas_cell(c, piece, m, r * 2.0f, with_a(white, lens_k * 0.9f));
            atlas_cell(c, piece + 1, m, r * 2.0f, with_a(white, op));
            atlas_cell(c, c.atlas->style_first + (gold ? 6 : 5), m, r * 0.9f, with_a(white, std::max(op, lens_k)));
        } else {
            atlas_cell(c, (int)IconStick, m, r * 2.0f, with_a(white, 0.55f * op / 0.72f));
            atlas_cell(c, (int)IconKnob, m, r * 0.9f, with_a(white, 0.5f * op / 0.72f));
        }
    }
    const bool show_action = action_always || std::fmod((float)c.t, 4.0f) < 2.0f;   // « seulement si besoin » : il apparait quand il y a quelque chose a faire
    if (show_action) button((int)IconHand, W - 1.8f * u, H - 1.8f * u, 2.1f * u);
    if (!simple) button((int)IconRun, W - 4.1f * u, H - 1.2f * u, 1.5f * u);
    button((int)IconAim, W - 1.4f * u, H - 4.3f * u, 1.6f * u);
    button((int)IconKnife, W - 3.6f * u, H - 3.5f * u, 1.3f * u);
    if (!simple) button((int)IconTurn, W - 5.9f * u, H - 1.2f * u, 1.3f * u);
    button((int)IconCase, W - 1.2f * u, 1.1f * u, 1.3f * u);
    button((int)IconMap, W - 2.8f * u, 1.1f * u, 1.3f * u);
    button((int)IconPause, 1.1f * u, 1.1f * u, 1.1f * u);
}

}  // namespace nj::rmenu::pv
