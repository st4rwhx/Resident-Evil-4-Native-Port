// Menu Nightjar « style RE4 » : voir rmenu.h. Mise en page calquee sur l'ecran d'options du jeu (option.dat, rendu depuis ses fichiers UWF) :
// fond du jeu assombri, titre en grand serif rouge ombre, entrees creme (celle choisie plus claire), valeurs en colonne, EXIT en bas.
// Les pages qui sont des listes de fichiers (Mods, Sauvegardes...) prennent l'habillage metal de la mallette / machine a ecrire.
// Aucun fichier du jeu ici : les textures viennent de ui_skin (disque du joueur) ; sans elles, tout reste dessine par nous.
#include "rmenu.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "imgui_internal.h"

namespace nj::rmenu {

namespace {
uint32_t next_cp(const char *&p) {   // UTF-8 -> point de code
    const unsigned char c0 = (unsigned char)*p++;
    if (c0 < 0x80) return c0;
    int n = c0 >= 0xF0 ? 3 : c0 >= 0xE0 ? 2 : 1;
    uint32_t cp = c0 & (0x3F >> n);
    while (n-- > 0 && ((unsigned char)*p & 0xC0) == 0x80) cp = (cp << 6) | ((unsigned char)*p++ & 0x3F);
    return cp;
}
}  // namespace

bool GlyphFont::build(const uint8_t *rgba, int w, int h) {
    if (w != 1024 || h != 1024) return false;
    // Agrandissement Catmull-Rom des 6 rangees de lettres (192 px de haut) sur couleurs premultipliees, puis alpha reprecise :
    // les cases de 32 px du jeu restent nettes quand le menu les affiche 2 a 3 fois plus grandes.
    const int S = scale, rows_px = 192;
    tw = w * S;
    th = rows_px * S;
    cell = 32 * S;
    pixels.assign((size_t)tw * th * 4, 0);
    auto cr = [](float t, float *wt) {
        const float t2 = t * t, t3 = t2 * t;
        wt[0] = 0.5f * (-t3 + 2 * t2 - t);
        wt[1] = 0.5f * (3 * t3 - 5 * t2 + 2);
        wt[2] = 0.5f * (-3 * t3 + 4 * t2 + t);
        wt[3] = 0.5f * (t3 - t2);
    };
    for (int y2 = 0; y2 < th; ++y2) {
        const float sy = (y2 + 0.5f) / S - 0.5f;
        const int iy = (int)std::floor(sy);
        float wy[4];
        cr(sy - iy, wy);
        for (int x2 = 0; x2 < tw; ++x2) {
            const float sx = (x2 + 0.5f) / S - 0.5f;
            const int ix = (int)std::floor(sx);
            float wx[4];
            cr(sx - ix, wx);
            float acc[4] = {0, 0, 0, 0};
            for (int j = 0; j < 4; ++j) {
                const int yy = std::clamp(iy - 1 + j, 0, rows_px - 1);
                for (int i = 0; i < 4; ++i) {
                    const int xx = std::clamp(ix - 1 + i, 0, w - 1);
                    const uint8_t *p = &rgba[((size_t)yy * w + xx) * 4];
                    const float a = p[3] / 255.0f, k = wx[i] * wy[j];
                    acc[0] += p[0] * a * k; acc[1] += p[1] * a * k; acc[2] += p[2] * a * k; acc[3] += a * k;
                }
            }
            float a = std::clamp(acc[3], 0.0f, 1.0f);
            uint8_t *o = &pixels[((size_t)y2 * tw + x2) * 4];
            if (a > 0.003f)
                for (int ch = 0; ch < 3; ++ch) o[ch] = (uint8_t)std::clamp(acc[ch] / acc[3], 0.0f, 255.0f);
            a = std::clamp((a - 0.5f) * 1.5f + 0.5f, 0.0f, 1.0f);   // bords plus nets
            o[3] = (uint8_t)(a * 255.0f + 0.5f);
        }
    }
    // ordre des cases, 32 par ligne (notes METHODS.md §4, verifie sur font/sscrn_p.fnt)
    static const char *const rows[] = {
        " ▷▽0123456789:%&+−/=,.°…()!?“”~㎝",
        "—<>[]①②ABCDEFGHIJKLMNOPQRSTUVWXY",
        "Zabcdefghijklmnopqrstuvwxyzâêîôû",
        "ÂÊÎÔÛàèìòùÀÈÌÒÙáéíóúýÁÉÍÓÚÝäëïöü",
        "ÿÄËÏÖÜŸãõÃÕñÑåÅçÇøØþÞšŠßÐƒµðæœÆŒ",
        "º¡¿'™;#@"};
    for (int r = 0; r < 6; ++r) {
        const char *p = rows[r];
        for (int col = 0; *p && col < 32; ++col) {
            const uint32_t cp = next_cp(p);
            const int cx = col * 32, cy = r * 32;
            int x0 = 32, x1 = -1;
            for (int y = 0; y < 32; ++y)
                for (int x = 0; x < 32; ++x)
                    if (rgba[((size_t)(cy + y) * w + cx + x) * 4 + 3] > 24) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
            G g{};
            if (x1 < x0) { g = {(uint16_t)(cx * S), (uint16_t)(cy * S), 0, 0, 9}; }   // espace / case vide
            else g = {(uint16_t)((cx + x0) * S), (uint16_t)(cy * S), (uint16_t)((x1 - x0 + 1) * S), (uint16_t)(x1 - x0 + 1), (int16_t)(x1 - x0 + 1 - 2)};
            map[cp] = g;
        }
    }
    map[0x2D] = map[0x2212];      // - -> signe moins
    map[0x22] = map[0x201D];      // " -> guillemet
    map[0x2019] = map[0x27];
    return true;
}

bool GlyphFont::supports(const char *t) const {
    while (*t) {
        const uint32_t cp = next_cp(t);
        if (cp == 10) continue;
        if (!map.count(cp)) return false;
    }
    return true;
}

namespace {

// Geste en cours (un seul menu a l'ecran a la fois).
struct Gesture {
    bool down = false, dragging = false;
    ImVec2 start{};
    float scroll0 = 0.0f;
    int slider = -1;
    bool rail = false, rail_click = false;   // glissiere de droite saisie
} g_g;

ImFont *font_of(const Skin &sk) { return sk.font ? sk.font : ImGui::GetFont(); }

int value_index(const Option &o) {
    if (o.kind != Kind::Choice || !o.value) return 0;
    for (int i = 0; i < o.n; ++i)
        if ((o.values ? o.values[i] : i) == *o.value) return i;
    return 0;
}

bool is_enabled(const Option &o) { return !o.enabled || o.enabled(); }

void set_value(Option &o, int v, bool &changed) {
    if (!o.value || *o.value == v) return;
    *o.value = v;
    changed = true;
    if (o.on_change) o.on_change(v);
    if (o.change_fn) o.change_fn(v);
}

void step_option(Option &o, int dir, bool &changed) {
    if (!is_enabled(o)) return;
    if (o.kind == Kind::Choice && o.n > 0) {
        const int i = (value_index(o) + dir + o.n) % o.n;
        set_value(o, o.values ? o.values[i] : i, changed);
    } else if (o.kind == Kind::Toggle) {
        set_value(o, *o.value ? 0 : 1, changed);
    } else if (o.kind == Kind::Slider) {
        set_value(o, std::clamp(*o.value + dir * o.step, o.lo, o.hi), changed);
    }
}

// ---- dessin ---------------------------------------------------------------------------------------------------------
struct Ctx {
    ImDrawList *dl;
    ImFont *font;
    const Skin *sk;
    Translate tr;
    float dp;
};

// Police du jeu si tout le texte y est (latin) ; sinon la serif systeme.
bool use_game(const Ctx &c, const char *t) { return c.sk->gfont && c.sk->gfont->tex && c.sk->gfont->supports(t); }
float game_scale(float size) { return size / 24.0f; }
float tw(const Ctx &c, const char *t, float size) {
    if (!use_game(c, t)) return c.font->CalcTextSizeA(size, 1e9f, 0.0f, t).x;
    float w = 0.0f;
    const float k = game_scale(size);
    for (const char *p = t; *p;) {
        const auto it = c.sk->gfont->map.find(next_cp(p));
        if (it != c.sk->gfont->map.end()) w += it->second.adv * k;
    }
    return w;
}
void txt(const Ctx &c, ImVec2 p, ImU32 col, const char *t, float size) {
    if (!use_game(c, t)) { c.dl->AddText(c.font, size, p, col, t); return; }
    const GlyphFont &f = *c.sk->gfont;
    const float k = game_scale(size);
    float x = p.x;
    const float y = p.y + size * 0.5f - 16.0f * k;
    for (const char *q = t; *q;) {
        const auto it = f.map.find(next_cp(q));
        if (it == f.map.end()) continue;
        const GlyphFont::G &g = it->second;
        if (g.w) c.dl->AddImage(f.tex, ImVec2(x, y), ImVec2(x + g.w * k, y + 32.0f * k), ImVec2(g.x / (float)f.tw, g.y / (float)f.th), ImVec2((g.x + g.pw) / (float)f.tw, (g.y + f.cell) / (float)f.th), col);
        x += g.adv * k;
    }
}
// Texte avec l'ombre portee du jeu.
void txt_sh(const Ctx &c, ImVec2 p, ImU32 col, const char *t, float size, float off = 2.0f) {
    if (use_game(c, t)) { txt(c, ImVec2(p.x + 1.0f * c.dp, p.y + 1.0f * c.dp), IM_COL32(0, 0, 0, 150), t, size); txt(c, p, col, t, size); return; }
    c.dl->AddText(c.font, size, ImVec2(p.x + off * c.dp, p.y + off * c.dp), IM_COL32(0, 0, 0, 230), t);
    c.dl->AddText(c.font, size, p, col, t);
}
float twrap(const Ctx &c, ImVec2 p, float w, ImU32 col, const char *t, float size, bool center) {
    if (use_game(c, t)) {   // mots a la suite, retour a la ligne dans la largeur
        std::string line, word;
        float y = p.y;
        const float lh = size * 1.25f;
        auto flush = [&](bool last) {
            if (line.empty() && !last) return;
            const float lw = tw(c, line.c_str(), size);
            txt(c, ImVec2(center ? p.x + (w - lw) * 0.5f : p.x, y), col, line.c_str(), size);
            y += lh;
            line.clear();
        };
        for (const char *q = t;; ++q) {
            if (*q == ' ' || *q == 0) {
                if (!word.empty()) {
                    const std::string cand = line.empty() ? word : line + " " + word;
                    if (!line.empty() && tw(c, cand.c_str(), size) > w) { flush(false); line = word; }
                    else line = cand;
                    word.clear();
                }
                if (*q == 0) break;
            } else word += *q;
        }
        flush(true);
        return y - p.y;
    }
    const ImVec2 s = c.font->CalcTextSizeA(size, 1e9f, w, t);
    c.dl->AddText(c.font, size, ImVec2(center ? p.x + (w - std::min(w, s.x)) * 0.5f : p.x, p.y), col, t, nullptr, w);
    return s.y;
}

// Majuscules pour le latin simple (les titres du jeu sont en capitales) ; le reste est laisse tel quel.
const char *upper(const char *t) {
    static char buf[8][128];
    static int k = 0;
    char *o = buf[k = (k + 1) & 7];
    size_t i = 0;
    for (; t[i] && i < 126; ++i) {
        if ((unsigned char)t[i] >= 0x80) { std::snprintf(o, 128, "%s", t); return o; }
        o[i] = (char)std::toupper((unsigned char)t[i]);
    }
    o[i] = 0;
    return o;
}

void triangle_cursor(const Ctx &c, ImVec2 p, float h) {   // curseur du jeu : petit triangle rouge pointant a droite
    c.dl->AddTriangleFilled(ImVec2(p.x, p.y - h * 0.5f), ImVec2(p.x, p.y + h * 0.5f), ImVec2(p.x + h * 0.8f, p.y), c.sk->red);
}

void arrow(const Ctx &c, ImVec2 m, float r, int dir, ImU32 col) {
    if (dir < 0) c.dl->AddTriangleFilled(ImVec2(m.x + r * 0.6f, m.y - r), ImVec2(m.x + r * 0.6f, m.y + r), ImVec2(m.x - r * 0.8f, m.y), col);
    else c.dl->AddTriangleFilled(ImVec2(m.x - r * 0.6f, m.y - r), ImVec2(m.x - r * 0.6f, m.y + r), ImVec2(m.x + r * 0.8f, m.y), col);
}

void slice9(ImDrawList *dl, const Skin::Tex &t, float sl, ImVec2 a, ImVec2 b, float m, ImU32 tint) {
    const float u = sl / t.w, v = sl / t.h;
    const float xs[4] = {a.x, a.x + m, b.x - m, b.x}, ys[4] = {a.y, a.y + m, b.y - m, b.y};
    const float us[4] = {0, u, 1 - u, 1}, vs[4] = {0, v, 1 - v, 1};
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i) dl->AddImage(t.id, ImVec2(xs[i], ys[j]), ImVec2(xs[i + 1], ys[j + 1]), ImVec2(us[i], vs[j]), ImVec2(us[i + 1], vs[j + 1]), tint);
}

// Cadre d'un apercu : le papier a grain du jeu (ss_file) autour d'un fond sombre, comme le panneau des fichiers.
void panel_frame(const Ctx &c, ImVec2 a, ImVec2 b, float dp, bool fill) {
    const Skin &sk = *c.sk;
    const float m = 8.0f * dp;
    const ImVec2 fa(a.x - m, a.y - m), fb(b.x + m, b.y + m);
    if (sk.paper.id) {
        slice9(c.dl, sk.paper, 14.0f, fa, fb, 12.0f * dp, IM_COL32_WHITE);
        if (fill) c.dl->AddRectFilled(ImVec2(fa.x + 3 * dp, fa.y + 3 * dp), ImVec2(fb.x - 3 * dp, fb.y - 3 * dp), IM_COL32(8, 5, 4, 200), 2.0f * dp);
    } else {
        if (fill) c.dl->AddRectFilled(fa, fb, IM_COL32(18, 12, 10, 235), 3.0f * dp);
        c.dl->AddRect(fa, fb, sk.line, 3.0f * dp, 0, 1.5f * dp);
    }
}

// Barre de titre a fleches (en-tete des fichiers) avec le nom du reglage.
void caption_bar(const Ctx &c, ImVec2 a, ImVec2 b, const char *text, float size) {
    const Skin &sk = *c.sk;
    if (sk.head.id) {
        const float e = 28.0f / sk.head.w;   // les fleches occupent les bouts : on n'etire que le milieu
        const float ew = 28.0f * (b.y - a.y) / sk.head.h;
        c.dl->AddImage(sk.head.id, a, ImVec2(a.x + ew, b.y), ImVec2(0, 0), ImVec2(e, 1));
        c.dl->AddImage(sk.head.id, ImVec2(a.x + ew, a.y), ImVec2(b.x - ew, b.y), ImVec2(e, 0), ImVec2(1 - e, 1));
        c.dl->AddImage(sk.head.id, ImVec2(b.x - ew, a.y), b, ImVec2(1 - e, 0), ImVec2(1, 1));
    } else {
        c.dl->AddRectFilled(a, b, IM_COL32(30, 22, 19, 255));
    }
    const char *u = upper(text);
    float s = size;
    while (s > size * 0.6f && tw(c, u, s) > (b.x - a.x) - 70.0f * c.dp) s *= 0.94f;
    txt_sh(c, ImVec2((a.x + b.x) * 0.5f - tw(c, u, s) * 0.5f, (a.y + b.y) * 0.5f - s * 0.55f), sk.cream_hi, u, s, 1.5f);
}

void backdrop(const Ctx &c, ImVec2 a, ImVec2 b) {
    const Skin &sk = *c.sk;
    if (sk.shot_before.id) {   // le jeu tel qu'il etait, assombri et teinte de rouge (le jeu fait de meme derriere ses options)
        c.dl->AddImage(sk.shot_before.id, a, b, ImVec2(0, 1), ImVec2(1, 0), IM_COL32(95, 70, 70, 255));
        c.dl->AddRectFilled(a, b, IM_COL32(26, 4, 4, 170));
    } else {
        c.dl->AddRectFilledMultiColor(a, b, IM_COL32(40, 8, 8, 255), IM_COL32(40, 8, 8, 255), IM_COL32(10, 2, 2, 255), IM_COL32(10, 2, 2, 255));
    }
    const float h = b.y - a.y;   // bords sombres
    c.dl->AddRectFilledMultiColor(a, ImVec2(b.x, a.y + h * 0.22f), IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    c.dl->AddRectFilledMultiColor(ImVec2(a.x, b.y - h * 0.22f), b, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 200));
}

void title(const Ctx &c, ImVec2 a, ImVec2 b, const char *t, float size) {
    const char *u = upper(t);
    const float w = tw(c, u, size);
    txt_sh(c, ImVec2((a.x + b.x) * 0.5f - w * 0.5f, a.y), c.sk->title, u, size, 3.0f);
}

// Barre d'aide : pictos de boutons du jeu (ou les notres) + un mot, comme la carte et les bouchons.
void hint_bar(const Ctx &c, ImVec2 pos, ImVec2 size, float dp, bool touch) {
    const Skin &sk = *c.sk;
    const float y = pos.y + size.y - 12.0f * dp - 24.0f * dp;
    const float s = 15.0f * dp, g = 22.0f * dp;
    float x = pos.x + 14.0f * dp;
    struct H { int kind; const char *text; };
    const H pad[] = {{0, "Navigate"}, {1, "Change"}, {2, "Back"}, {3, "Page"}};
    const H fin[] = {{0, "Drag: scroll"}, {1, "Tap: change"}, {2, "Back"}};
    const H *hl = touch ? fin : pad;
    const int hn = touch ? 3 : 4;
    for (int i = 0; i < hn; ++i) {
        const H &h = hl[i];
        const ImVec2 m(x + g * 0.5f, y + g * 0.5f);
        if (h.kind == 0 && sk.g_stick.id) c.dl->AddImage(sk.g_stick.id, ImVec2(x, y), ImVec2(x + g, y + g));
        else if (h.kind == 3 && sk.g_lr.id) c.dl->AddImage(sk.g_lr.id, ImVec2(x, y), ImVec2(x + g, y + g));
        else if (h.kind == 1 && sk.g_a.id) c.dl->AddImage(sk.g_a.id, ImVec2(x, y), ImVec2(x + g, y + g));
        else if (h.kind == 2 && sk.g_b.id) c.dl->AddImage(sk.g_b.id, ImVec2(x, y), ImVec2(x + g, y + g));
        else {
            // pas de B dans les textures du jeu : une bille rouge ombree comme celle du A (reflet en haut a gauche, liseré sombre)
            const ImU32 fill = h.kind == 1 ? IM_COL32(40, 120, 110, 255) : h.kind == 2 ? IM_COL32(176, 36, 30, 255) : IM_COL32(70, 70, 70, 255);
            c.dl->AddCircleFilled(ImVec2(m.x, m.y + 1.2f * dp), g * 0.5f, IM_COL32(0, 0, 0, 140));
            c.dl->AddCircleFilled(m, g * 0.5f, fill);
            c.dl->AddCircleFilled(ImVec2(m.x - g * 0.12f, m.y - g * 0.14f), g * 0.30f, IM_COL32(255, 255, 255, 52));
            c.dl->AddCircleFilled(ImVec2(m.x - g * 0.18f, m.y - g * 0.20f), g * 0.12f, IM_COL32(255, 255, 255, 70));
            c.dl->AddCircle(m, g * 0.5f, IM_COL32(20, 14, 12, 230), 0, 1.6f * dp);
            const char *l = h.kind == 1 ? "A" : h.kind == 2 ? "B" : h.kind == 3 ? "LR" : "+";
            txt(c, ImVec2(m.x - tw(c, l, s * 0.8f) * 0.5f, m.y - s * 0.45f), sk.cream, l, s * 0.8f);
        }
        x += g + 6.0f * dp;
        const char *t = c.tr(h.text);
        txt(c, ImVec2(x, y + g * 0.5f - s * 0.5f), sk.dim_text, t, s);
        x += tw(c, t, s) + 22.0f * dp;
    }
}

}  // namespace

const char *choice_name(const Option &o) { return o.kind == Kind::Choice && o.n > 0 ? o.names[value_index(o)] : ""; }

// left (menu titre de Nightjar, 18/11 soir) : texte aligne a gauche sur une marge fixe, degrade sombre depuis le bord gauche
// (le decor reste libre a droite, Leon sur le tiers droit) ; sinon l'ancienne mise en page centree en bas.
int draw_home(const std::vector<HomeItem> &items, int &cursor, const Skin &sk, ImVec2 pos, ImVec2 size, float dp, const char *heading, bool left) {
    if (items.empty()) return -1;
    Ctx c{ImGui::GetWindowDrawList(), font_of(sk), &sk, [](const char *t) { return t; }, dp};
    ImDrawList *dl = c.dl;
    ImGuiIO &io = ImGui::GetIO();
    const int n = (int)items.size();
    const ImVec2 end(pos.x + size.x, pos.y + size.y);
    // bas de l'ecran assombri : l'image du titre reste visible au-dessus, le texte se lit dessous
    const float y0 = pos.y + size.y * (left ? 0.30f : 0.42f);
    const float x0 = pos.x + size.x * 0.08f;   // marge gauche (mise en page a gauche)
    if (left)
        dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + size.x * 0.62f, end.y), IM_COL32(8, 2, 2, 235), IM_COL32(8, 2, 2, 0), IM_COL32(8, 2, 2, 0), IM_COL32(8, 2, 2, 235));
    else
        dl->AddRectFilledMultiColor(ImVec2(pos.x, y0 - 40.0f * dp), ImVec2(end.x, end.y), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(8, 2, 2, 225), IM_COL32(8, 2, 2, 225));
    const float ts = 20.0f * dp;
    // hauteurs : la grande entree prend la place de deux
    float total = 0.0f;
    for (const auto &it : items) total += it.big ? 74.0f * dp : 42.0f * dp;
    float y = std::max(y0, end.y - 30.0f * dp - total);
    if (heading && *heading) {
        const char *h = upper(heading);
        txt_sh(c, ImVec2(left ? x0 : pos.x + size.x * 0.5f - tw(c, h, ts * 0.8f) * 0.5f, y - 30.0f * dp), sk.title, h, ts * 0.8f, 1.5f);
    }
    cursor = std::clamp(cursor, 0, n - 1);
    while (!items[cursor].enabled && cursor < n - 1) ++cursor;
    auto step = [&](int d) {
        int k = cursor;
        for (int g = 0; g < n; ++g) {
            k = std::clamp(k + d, 0, n - 1);
            if (items[k].enabled) { cursor = k; return; }
        }
    };
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown)) step(+1);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp)) step(-1);
    int chosen = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false))
        chosen = cursor;
    const bool released = io.MouseReleased[0];
    const float cx = pos.x + size.x * 0.5f;
    for (int i = 0; i < n; ++i) {
        const HomeItem &it = items[i];
        const float rh = it.big ? 74.0f * dp : 42.0f * dp;
        const bool sel = i == cursor;
        const float s = it.big ? ts * 1.55f : ts * 1.1f;
        const char *t = upper(it.label.c_str());
        const float w = tw(c, t, s);
        const float my = y + (it.big ? rh * 0.38f : rh * 0.5f);
        const float subw = it.sub.empty() ? 0.0f : tw(c, it.sub.c_str(), ts * 0.78f);
        // image de la sauvegarde a gauche : le bloc (image + texte) est centre ; sans image, le texte est centre
        const float ih = it.img ? rh - 8.0f * dp : 0.0f, iw = ih * it.img_aspect, gapi = it.img ? 16.0f * dp : 0.0f;
        const float textw = std::max(w, subw);
        const float block = iw + gapi + textw;
        const float bx = left ? x0 : cx - block * 0.5f;              // debut du bloc
        const float tcx = left ? bx + iw + gapi + w * 0.5f : it.img ? bx + iw + gapi + textw * 0.5f : cx;   // centre du titre
        const float rw = block + 90.0f * dp;
        const float bar0 = left ? x0 - 44.0f * dp : cx - rw * 0.5f, bar1 = left ? x0 + block + 70.0f * dp : cx + rw * 0.5f;
        if (sel && it.enabled) {
            if (left)
                dl->AddRectFilledMultiColor(ImVec2(bar0, y), ImVec2(bar1, y + rh - 4.0f * dp), IM_COL32(120, 20, 20, 120), IM_COL32(120, 20, 20, 0), IM_COL32(120, 20, 20, 0), IM_COL32(120, 20, 20, 120));
            else
                dl->AddRectFilledMultiColor(ImVec2(cx - rw * 0.5f, y), ImVec2(cx + rw * 0.5f, y + rh - 4.0f * dp), IM_COL32(120, 20, 20, 0), IM_COL32(120, 20, 20, 90), IM_COL32(120, 20, 20, 90), IM_COL32(120, 20, 20, 0));
        }
        if (it.img) {
            const ImVec2 ia(bx, y + 4.0f * dp), ib(bx + iw, y + 4.0f * dp + ih);
            dl->AddRectFilled(ImVec2(ia.x - 2.0f * dp, ia.y - 2.0f * dp), ImVec2(ib.x + 2.0f * dp, ib.y + 2.0f * dp), sel ? sk.red : sk.line);
            dl->AddImage(it.img, ia, ib, ImVec2(0, 0), ImVec2(1, 1), it.enabled ? IM_COL32_WHITE : IM_COL32(120, 120, 120, 255));
        }
        const ImU32 col = !it.enabled ? sk.off : sel ? sk.cream_hi : sk.dim_text;
        txt_sh(c, ImVec2(tcx - w * 0.5f, my - s * 0.55f), col, t, s, it.big ? 2.5f : 2.0f);
        if (sel && it.enabled) triangle_cursor(c, ImVec2((it.img ? bx : tcx - w * 0.5f) - 30.0f * dp, my), (it.big ? 20.0f : 15.0f) * dp);
        if (!it.sub.empty()) {
            const float ss = ts * 0.78f;
            const float sx = left ? bx + iw + gapi : tcx - subw * 0.5f;
            txt_sh(c, ImVec2(sx, my + s * 0.55f + 4.0f * dp), it.enabled ? sk.cream : sk.off, it.sub.c_str(), ss, 1.0f);
        }
        const bool in_x = left ? (io.MousePos.x >= bar0 && io.MousePos.x <= bar1 + 40.0f * dp) : std::fabs(io.MousePos.x - cx) <= rw * 0.5f + 40.0f * dp;
        if (released && it.enabled && io.MousePos.y >= y && io.MousePos.y <= y + rh && in_x) {
            cursor = i;
            chosen = i;
        }
        y += rh;
    }
    if (chosen >= 0 && !items[chosen].enabled) chosen = -1;
    return chosen;
}

bool draw(std::vector<Category> &cats, State &st, const Skin &sk, Translate tr, ImVec2 pos, ImVec2 size, float dp, bool &quitter) {
    if (cats.empty()) return false;
    Ctx c{ImGui::GetWindowDrawList(), font_of(sk), &sk, tr, dp};
    ImDrawList *dl = c.dl;
    ImGuiIO &io = ImGui::GetIO();
    bool changed = false;
    const ImVec2 end(pos.x + size.x, pos.y + size.y);
    const bool wide = size.x >= size.y * 1.05f;
    const float ts = 20.0f * dp, tt = 40.0f * dp;
    const int ncat = (int)cats.size();
    // st.page : -1 = liste des categories ; sinon index de la categorie ouverte

    backdrop(c, pos, end);
    const bool in_page = st.page >= 0 && st.page < ncat;
    Category *cat = in_page ? &cats[st.page] : nullptr;
    title(c, ImVec2(pos.x, pos.y + 16.0f * dp), end, in_page ? tr(cat->label) : tr("Options"), tt);

    const float top = pos.y + 16.0f * dp + tt + 20.0f * dp;
    const float desc_h = in_page && !cat->custom ? 64.0f * dp : 0.0f;
    const float bot = end.y - 44.0f * dp - desc_h;
    const float view_h = bot - top;

    if (in_page && cat->custom) {
        // ----- page libre : contenu ImGui sur fond de mallette -----
        const ImVec2 a(pos.x + 28.0f * dp, top), b(end.x - 28.0f * dp, end.y - 48.0f * dp);
        if (sk.paper.id) {
            slice9(dl, sk.paper, 14.0f, a, b, 12.0f * dp, IM_COL32_WHITE);
            dl->AddRectFilled(ImVec2(a.x + 3 * dp, a.y + 3 * dp), ImVec2(b.x - 3 * dp, b.y - 3 * dp), IM_COL32(8, 5, 4, 175), 2.0f * dp);
        } else {
            dl->AddRectFilled(a, b, IM_COL32(18, 12, 10, 235), 3.0f * dp);
            dl->AddRect(a, b, sk.line, 3.0f * dp, 0, 1.5f * dp);
        }
        ImGui::SetCursorScreenPos(ImVec2(a.x + 12.0f * dp, a.y + 10.0f * dp));
        ImGui::PushStyleColor(ImGuiCol_Text, sk.cream);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, sk.dim_text);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 14, 12, 230));
        ImGui::PushStyleColor(ImGuiCol_Button, sk.rust);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sk.red);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, sk.red);
        ImGui::PushStyleColor(ImGuiCol_Header, sk.rust);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, sk.cream);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, sk.cream);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(24, 16, 13, 250));
        ImGui::PushStyleColor(ImGuiCol_Border, sk.line);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f * dp, 9.0f * dp));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * dp, 10.0f * dp));
        const ImVec2 cs(b.x - a.x - 24.0f * dp, b.y - a.y - 20.0f * dp);
        if (ImGui::BeginChild("##rm_custom", cs, false, ImGuiWindowFlags_NoBackground)) {
            if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0, 6.0f * dp) && !ImGui::IsAnyItemActive()) ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
            cat->custom(cs);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(12);
        st.preview_active = false;
    } else if (!in_page && wide && std::any_of(cats.begin(), cats.end(), [](const Category &k) { return k.column == 1; })) {
        // ----- page principale en deux colonnes (18/11) : « Le jeu » a gauche, « Nightjar » a droite, sans defilement -----
        std::vector<int> colv[2];
        for (int i = 0; i < ncat; ++i) colv[cats[i].column == 1 ? 1 : 0].push_back(i);
        const int nrows = (int)std::max(colv[0].size(), colv[1].size());
        const float head_h = ts * 1.1f, exit_h = 40.0f * dp;
        const float rh = std::clamp((view_h - head_h - 12.0f * dp - exit_h) / (float)std::max(1, nrows), 26.0f * dp, 58.0f * dp);
        const int exit_idx = ncat;
        st.row = std::clamp(st.row, 0, exit_idx);
        static int s_last_col = 0;
        int col = -1, at = 0;
        for (int k = 0; k < 2; ++k)
            for (int j = 0; j < (int)colv[k].size(); ++j)
                if (colv[k][j] == st.row) { col = k; at = j; }
        if (col >= 0) s_last_col = col;
        auto open_page = [&](int p) { st.page = p; st.row = 0; st.scroll = 0.0f; st.velocity = 0.0f; };
        // navigation croix / clavier : haut/bas dans la colonne, gauche/droite d'une colonne a l'autre, bas en fin de colonne = QUITTER
        const bool kd = ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown);
        const bool ku = ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp);
        const bool kl = ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft);
        const bool kr = ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight);
        const bool act = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false);
        if (col < 0) {   // sur QUITTER
            const auto &cv = colv[s_last_col].empty() ? colv[1 - s_last_col] : colv[s_last_col];
            if (ku && !cv.empty()) st.row = cv.back();
        } else {
            if (kd) st.row = at + 1 < (int)colv[col].size() ? colv[col][at + 1] : exit_idx;
            if (ku && at > 0) st.row = colv[col][at - 1];
            if ((kl && col == 1) || (kr && col == 0)) {
                const auto &ov = colv[1 - col];
                if (!ov.empty()) st.row = ov[std::min(at, (int)ov.size() - 1)];
            }
        }
        if (act) { if (st.row == exit_idx) quitter = true; else open_page(st.row); }

        const bool released = io.MouseReleased[0] && !g_g.dragging;
        int page_to_open = -1;
        const float colw = size.x * 0.44f;
        const float cx[2] = {pos.x + size.x * 0.27f, pos.x + size.x * 0.73f};
        const char *heads[2] = {tr("The game"), tr("NIGHTJAR")};
        for (int k = 0; k < 2; ++k) {
            if (colv[k].empty()) continue;
            const char *h = upper(heads[k]);
            const float hs = ts * 0.85f;
            txt_sh(c, ImVec2(cx[k] - tw(c, h, hs) * 0.5f, top), sk.title, h, hs, 1.5f);
            dl->AddLine(ImVec2(cx[k] - colw * 0.32f, top + head_h), ImVec2(cx[k] + colw * 0.32f, top + head_h), sk.line, 1.0f * dp);
            for (int j = 0; j < (int)colv[k].size(); ++j) {
                const int i = colv[k][j];
                const float y = top + head_h + 12.0f * dp + j * rh, my = y + rh * 0.5f;
                const bool sel = i == st.row;
                const ImVec2 ra(cx[k] - colw * 0.5f, y), rb(cx[k] + colw * 0.5f, y + rh - 2.0f * dp);
                if (sel) dl->AddRectFilledMultiColor(ra, rb, IM_COL32(120, 20, 20, 0), IM_COL32(120, 20, 20, 80), IM_COL32(120, 20, 20, 80), IM_COL32(120, 20, 20, 0));
                const char *t = upper(tr(cats[i].label));
                float s2 = std::min(ts * 1.1f, rh * 0.52f);
                while (s2 > ts * 0.6f && tw(c, t, s2) > colw - 56.0f * dp) s2 *= 0.94f;
                const float w = tw(c, t, s2);
                txt_sh(c, ImVec2(cx[k] - w * 0.5f, my - s2 * 0.55f), sel ? sk.cream_hi : sk.dim_text, t, s2);
                if (sel) triangle_cursor(c, ImVec2(cx[k] - w * 0.5f - 26.0f * dp, my), 14.0f * dp);
                if (released && io.MousePos.x >= ra.x && io.MousePos.x <= rb.x && io.MousePos.y >= ra.y && io.MousePos.y <= rb.y) { st.row = i; page_to_open = i; }
            }
        }
        {   // QUITTER, centre sous les deux colonnes
            const float y = bot - exit_h, my = y + exit_h * 0.5f;
            const bool sel = st.row == exit_idx;
            const char *t = upper(tr("Exit"));
            const float w = tw(c, t, ts);
            txt_sh(c, ImVec2(pos.x + size.x * 0.5f - w * 0.5f, my - ts * 0.55f), sel ? sk.cream_hi : sk.dim_text, t, ts);
            if (sel) triangle_cursor(c, ImVec2(pos.x + size.x * 0.5f - w * 0.5f - 26.0f * dp, my), 14.0f * dp);
            if (released && std::fabs(io.MousePos.x - (pos.x + size.x * 0.5f)) < w * 0.5f + 40.0f * dp && io.MousePos.y >= y && io.MousePos.y <= y + exit_h) { st.row = exit_idx; quitter = true; }
        }
        if (io.MouseReleased[0]) { g_g.down = false; g_g.dragging = false; g_g.slider = -1; }
        if (page_to_open >= 0) open_page(page_to_open);
        st.preview_active = false;
    } else {
        // ----- lignes : categories (page principale) ou options de la categorie -----
        const int nopt = in_page ? (int)cat->options.size() : ncat;
        const int count = nopt + 1;   // + EXIT / BACK
        const float rh = (in_page ? 44.0f : 62.0f) * dp, gap = 3.0f * dp;
        const float content_h = count * (rh + gap);
        const float max_scroll = std::max(0.0f, content_h - view_h);
        st.row = std::clamp(st.row, 0, count - 1);
        const float col_x = pos.x + size.x * (in_page ? (wide ? 0.37f : 0.44f) : 0.5f);   // colonne des valeurs
        const float right = wide ? pos.x + size.x * 0.63f : end.x - 20.0f * dp;   // bord de la colonne quand un apercu occupe la droite
        const float right_all = end.x - 40.0f * dp;
        const bool want_preview = in_page && wide;
        const bool sel_prev = want_preview && st.row < nopt && (cat->options[st.row].preview || cat->options[st.row].preview_fn || cat->options[st.row].table_fn);

        // navigation croix / clavier
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown)) st.row = std::min(count - 1, st.row + 1);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp)) st.row = std::max(0, st.row - 1);
        const bool act = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false);   // pas de repetition : une action = une pression
        const bool left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft);
        const bool rightk = ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight);
        auto open_page = [&](int p) {
            const int from = st.page;
            st.page = p;
            st.row = p < 0 ? std::max(0, from) : 0;   // en revenant : le curseur retombe sur la categorie quittee
            st.scroll = 0.0f;
            st.velocity = 0.0f;
        };
        const bool act_exit = act && st.row == nopt;
        if (in_page && st.row < nopt) {
            Option &o = cat->options[st.row];
            if (left) step_option(o, -1, changed);
            if (rightk) step_option(o, +1, changed);
            if (act) {
                if (o.kind == Kind::Action && (o.run_fn || o.run)) { if (o.run_fn) o.run_fn(); else o.run(); }
                else step_option(o, +1, changed);
            }
        }
        if (act_exit) { if (in_page) open_page(-1); else quitter = true; }
        else if (act && !in_page && st.row < ncat) open_page(st.row);
        if (st.row != st.seen_row) {   // la ligne choisie reste visible, mais seulement quand elle change (sinon le defilement au doigt ou a la molette est annule a chaque image)
            const float ry = st.row * (rh + gap);
            if (ry < st.scroll) st.scroll = ry;
            if (ry + rh > st.scroll + view_h) st.scroll = ry + rh - view_h;
            st.seen_row = st.row;
        }

        // geste : tirer = defiler (inertie) ; un curseur tire ne defile pas
        const ImVec2 la(pos.x, top), lb(end.x, bot);
        const bool in_list = io.MousePos.x >= la.x && io.MousePos.x <= lb.x && io.MousePos.y >= la.y && io.MousePos.y <= lb.y;
        // glissiere de droite : on la saisit et on la tire ; molette : defile la liste
        const float rail_x = end.x - 19.0f * dp, rail_w = 14.0f * dp, rail_y0 = top + 4.0f * dp, rail_y1 = bot - 4.0f * dp;
        if (io.MouseClicked[0]) {
            g_g.rail_click = max_scroll > 0.0f && io.MousePos.x >= rail_x - 14.0f * dp && io.MousePos.x <= rail_x + rail_w + 14.0f * dp &&
                             io.MousePos.y >= rail_y0 - 8.0f * dp && io.MousePos.y <= rail_y1 + 8.0f * dp;
            g_g.rail = g_g.rail_click;
        }
        if (g_g.rail) {
            if (io.MouseDown[0]) {
                st.scroll = std::clamp((io.MousePos.y - rail_y0) / std::max(1.0f, rail_y1 - rail_y0), 0.0f, 1.0f) * max_scroll;
                st.velocity = 0.0f;
            } else {
                g_g.rail = false;
            }
        }
        if (in_list && io.MouseWheel != 0.0f && max_scroll > 0.0f) {
            st.scroll = std::clamp(st.scroll - io.MouseWheel * (rh + gap) * 1.5f, 0.0f, max_scroll);
            st.velocity = 0.0f;
        }
        if (io.MouseClicked[0] && in_list && !g_g.rail_click) { g_g.down = true; g_g.dragging = false; g_g.start = io.MousePos; g_g.scroll0 = st.scroll; g_g.slider = -1; st.velocity = 0.0f; }
        if (g_g.down && io.MouseDown[0]) {
            if (g_g.slider < 0 && std::fabs(io.MousePos.y - g_g.start.y) > 8.0f * dp) g_g.dragging = true;
            if (g_g.dragging) {
                const float prev = st.scroll;
                st.scroll = std::clamp(g_g.scroll0 - (io.MousePos.y - g_g.start.y), 0.0f, max_scroll);
                st.velocity = (st.scroll - prev) / std::max(io.DeltaTime, 0.001f);
            }
        } else if (g_g.down && !io.MouseDown[0]) {
            g_g.down = false;
        } else if (!g_g.down && !g_g.rail) {
            st.scroll = std::clamp(st.scroll + st.velocity * io.DeltaTime, 0.0f, max_scroll);
            st.velocity *= std::pow(0.04f, io.DeltaTime);
            if (std::fabs(st.velocity) < 8.0f) st.velocity = 0.0f;
        }
        st.scroll = std::clamp(st.scroll, 0.0f, max_scroll);
        const bool released = io.MouseReleased[0] && !g_g.dragging && !g_g.rail_click;
        int page_to_open = -2;   // ouverture differee : la liste n'est pas modifiee en plein dessin
        bool go_back = false;

        dl->PushClipRect(la, lb, true);
        for (int i = 0; i < count; ++i) {
            const float y = top + i * (rh + gap) - st.scroll;
            if (y + rh < la.y || y > lb.y) continue;
            const float my = y + rh * 0.5f;
            const bool sel = i == st.row;
            const bool tap = released && io.MousePos.y >= std::max(y, la.y) && io.MousePos.y <= std::min(y + rh, lb.y) && io.MousePos.x >= la.x && io.MousePos.x <= lb.x;
            if (sel)   // surbrillance douce
                dl->AddRectFilledMultiColor(ImVec2(pos.x + 20.0f * dp, y), ImVec2(end.x - 20.0f * dp, y + rh), IM_COL32(120, 20, 20, 0), IM_COL32(120, 20, 20, 70), IM_COL32(120, 20, 20, 70), IM_COL32(120, 20, 20, 0));
            const ImU32 on_col = sk.cream_hi, off_col = sk.dim_text;
            if (i == nopt) {   // EXIT / BACK
                const char *t = upper(tr(in_page ? "Back" : "Exit"));
                const float w = tw(c, t, ts);
                txt_sh(c, ImVec2(pos.x + size.x * 0.5f - w * 0.5f, my - ts * 0.55f), sel ? on_col : off_col, t, ts);
                if (sel) triangle_cursor(c, ImVec2(pos.x + size.x * 0.5f - w * 0.5f - 26.0f * dp, my), 14.0f * dp);
                if (tap) { st.row = i; if (in_page) go_back = true; else quitter = true; }
                continue;
            }
            if (!in_page) {   // liste des categories : mots centres
                const char *t = upper(tr(cats[i].label));
                const float s2 = ts * 1.15f, w = tw(c, t, s2);
                txt_sh(c, ImVec2(pos.x + size.x * 0.5f - w * 0.5f, my - s2 * 0.55f), sel ? on_col : off_col, t, s2);
                if (sel) triangle_cursor(c, ImVec2(pos.x + size.x * 0.5f - w * 0.5f - 28.0f * dp, my), 16.0f * dp);
                if (tap) { st.row = i; page_to_open = i; }
                continue;
            }
            // option
            Option &o = cat->options[i];
            const bool en = is_enabled(o);
            const char *lab = upper(o.label_str.empty() ? tr(o.label) : o.label_str.c_str());
            const float lw = tw(c, lab, ts);
            const ImU32 lc = !en ? sk.off : sel ? on_col : off_col;
            txt_sh(c, ImVec2(col_x - 22.0f * dp - lw, my - ts * 0.55f), lc, lab, ts);
            if (sel) triangle_cursor(c, ImVec2(col_x - 22.0f * dp - lw - 30.0f * dp, my), 14.0f * dp);
            const float vx = col_x + 16.0f * dp, vmax = (sel_prev ? right : right_all) - vx;
            if (tap && io.MousePos.x < col_x) st.row = i;
            if (o.kind == Kind::Toggle) {
                const bool v = o.value && *o.value;
                const char *a = upper(tr("On")), *b = upper(tr("Off"));
                txt_sh(c, ImVec2(vx, my - ts * 0.55f), !en ? sk.off : v ? sk.cream_hi : sk.dim_text, a, ts);
                const float bx = vx + tw(c, a, ts) + 38.0f * dp;
                txt_sh(c, ImVec2(bx, my - ts * 0.55f), !en ? sk.off : !v ? sk.cream_hi : sk.dim_text, b, ts);
                float lx = bx + tw(c, b, ts) + 40.0f * dp;
                std::function<void()> link_hit;
                for (auto &lk : o.links) {   // petits mots cliquables : HAUT / BAS / RETIRER
                    const char *lt = upper(tr(lk.first.c_str()));
                    const float lwid = tw(c, lt, ts * 0.8f);
                    txt_sh(c, ImVec2(lx, my - ts * 0.45f), sel ? sk.cream : sk.dim_text, lt, ts * 0.8f);
                    dl->AddLine(ImVec2(lx, my + ts * 0.5f), ImVec2(lx + lwid, my + ts * 0.5f), sk.red, 1.5f * dp);
                    if (tap && io.MousePos.x >= lx - 8.0f * dp && io.MousePos.x <= lx + lwid + 8.0f * dp) link_hit = lk.second;
                    lx += lwid + 26.0f * dp;
                }
                if (link_hit) { st.row = i; link_hit(); }
                else if (tap && en && io.MousePos.x >= vx - 10.0f * dp && io.MousePos.x < bx + tw(c, b, ts) + 20.0f * dp) { st.row = i; set_value(o, io.MousePos.x < bx - 12.0f * dp ? 1 : 0, changed); }
            } else if (o.kind == Kind::Choice) {
                // tous les choix en ligne s'ils tiennent (on touche directement celui voulu), sinon ◀ valeur ▶
                float total = 0.0f;
                const float sp = 34.0f * dp;
                for (int k = 0; k < o.n; ++k) total += tw(c, upper(tr(o.names[k])), ts) + (k ? sp : 0.0f);
                const int cur = value_index(o);
                if (o.n > 0 && total <= vmax) {
                    float x = vx;
                    for (int k = 0; k < o.n; ++k) {
                        const char *t = upper(tr(o.names[k]));
                        const float w = tw(c, t, ts);
                        txt_sh(c, ImVec2(x, my - ts * 0.55f), !en ? sk.off : k == cur ? sk.cream_hi : sk.dim_text, t, ts);
                        if (k == cur) dl->AddLine(ImVec2(x, my + ts * 0.6f), ImVec2(x + w, my + ts * 0.6f), sk.red, 2.0f * dp);
                        if (tap && en && io.MousePos.x >= x - sp * 0.5f && io.MousePos.x < x + w + sp * 0.5f && io.MousePos.x >= vx - 10.0f * dp) {
                            st.row = i;
                            set_value(o, o.values ? o.values[k] : k, changed);
                        }
                        x += w + sp;
                    }
                } else if (o.n > 0) {
                    std::string tfit = upper(tr(o.names[cur]));
                    float s2 = ts;
                    while (s2 > ts * 0.82f && tw(c, tfit.c_str(), s2) > vmax - 80.0f * dp) s2 *= 0.94f;
                    if (tw(c, tfit.c_str(), s2) > vmax - 80.0f * dp) {   // encore trop long : on coupe et on met « ... » (jamais sous la fleche)
                        while (tfit.size() > 3 && tw(c, (tfit + "...").c_str(), s2) > vmax - 80.0f * dp) {
                            tfit.pop_back();
                            while (!tfit.empty() && ((unsigned char)tfit.back() & 0xC0) == 0x80) tfit.pop_back();
                            if (!tfit.empty() && (unsigned char)tfit.back() >= 0xC0) tfit.pop_back();
                        }
                        while (!tfit.empty() && tfit.back() == ' ') tfit.pop_back();
                        tfit += "...";
                    }
                    const char *t = tfit.c_str();
                    const float aw = 26.0f * dp;
                    txt_sh(c, ImVec2(vx + aw, my - s2 * 0.55f), !en ? sk.off : sk.cream_hi, t, s2);
                    const float xr = vx + aw + std::min(tw(c, t, s2), vmax - 80.0f * dp) + 14.0f * dp;
                    arrow(c, ImVec2(vx + aw * 0.4f, my), 9.0f * dp, -1, en ? sk.red : sk.off);
                    arrow(c, ImVec2(xr + aw * 0.4f, my), 9.0f * dp, +1, en ? sk.red : sk.off);
                    if (tap && en && io.MousePos.x >= vx - 10.0f * dp && io.MousePos.x <= vx + aw * 1.2f) { st.row = i; step_option(o, -1, changed); }
                    else if (tap && en && io.MousePos.x >= xr - 6.0f * dp && io.MousePos.x <= xr + aw * 1.6f) { st.row = i; step_option(o, +1, changed); }
                    else if (tap && en && io.MousePos.x > vx + aw * 1.2f && io.MousePos.x < xr) { st.row = i; step_option(o, +1, changed); }
                }
            } else if (o.kind == Kind::Slider) {
                const float sw = std::min(vmax - 70.0f * dp, 280.0f * dp);
                const ImVec2 sa(vx, my - 7.0f * dp), sb(vx + sw, my + 7.0f * dp);
                const float f = std::clamp((float)(*o.value - o.lo) / std::max(1, o.hi - o.lo), 0.0f, 1.0f);
                for (int k = 0; k < 16; ++k) {   // barre en paliers de gris comme celle de la luminosite du jeu
                    const int gr = 15 + k * 14;
                    dl->AddRectFilled(ImVec2(sa.x + sw * k / 16.0f, sa.y), ImVec2(sa.x + sw * (k + 1) / 16.0f, sb.y), IM_COL32(gr, gr, gr, 255));
                }
                dl->AddRect(sa, sb, sk.line, 0.0f, 0, 1.0f * dp);
                dl->AddLine(ImVec2(sa.x + sw * f, sa.y - 7.0f * dp), ImVec2(sa.x + sw * f, sb.y + 7.0f * dp), sk.red, 3.0f * dp);
                char v[16];
                std::snprintf(v, sizeof v, "%d", *o.value);
                txt_sh(c, ImVec2(sb.x + 14.0f * dp, my - ts * 0.55f), sk.dim_text, v, ts);
                if (en && io.MouseClicked[0] && g_g.slider < 0 && io.MousePos.y >= y && io.MousePos.y <= y + rh && io.MousePos.x >= sa.x - 12.0f * dp && io.MousePos.x <= sb.x + 12.0f * dp) g_g.slider = i;
                if (en && g_g.slider == i && io.MouseDown[0]) {
                    const float t = std::clamp((io.MousePos.x - sa.x) / sw, 0.0f, 1.0f);
                    set_value(o, std::clamp(o.lo + (int)std::lround(t * (o.hi - o.lo) / (float)o.step) * o.step, o.lo, o.hi), changed);
                    st.row = i;
                }
            } else if (o.kind == Kind::Info) {
                if (o.text_fn) txt_sh(c, ImVec2(vx, my - ts * 0.55f), sk.dim_text, upper(o.text_fn().c_str()), ts);
                else if (o.text) txt_sh(c, ImVec2(vx, my - ts * 0.55f), sk.dim_text, upper(tr(o.text())), ts);
            } else {   // Action
                const char *t = upper(o.text_fn ? o.text_fn().c_str() : tr(o.text ? o.text() : "Run"));
                txt_sh(c, ImVec2(vx, my - ts * 0.55f), sel ? sk.cream_hi : sk.dim_text, t, ts);
                dl->AddLine(ImVec2(vx, my + ts * 0.6f), ImVec2(vx + tw(c, t, ts), my + ts * 0.6f), sk.red, 2.0f * dp);
                if (tap && en && io.MousePos.x >= vx - 10.0f * dp) { st.row = i; if (o.run_fn) o.run_fn(); else if (o.run) o.run(); }
            }
        }
        dl->PopClipRect();
        if (max_scroll > 0.0f) {   // (18/11) il y a d'autres lignes hors de vue : on le dit en clair, la glissiere seule passait inapercue
            const int below = std::max(0, nopt - (int)((st.scroll + view_h) / (rh + gap)));
            if (below > 0 && st.scroll < max_scroll - 2.0f * dp) {
                const float gh = 46.0f * dp;
                dl->AddRectFilledMultiColor(ImVec2(pos.x, lb.y - gh), ImVec2(end.x - 30.0f * dp, lb.y), IM_COL32(10, 2, 2, 0), IM_COL32(10, 2, 2, 0), IM_COL32(10, 2, 2, 235), IM_COL32(10, 2, 2, 235));
                char mb[64];
                std::snprintf(mb, sizeof mb, tr("%d more settings"), below);
                const char *mt = upper(mb);
                const float ms = ts * 0.8f, mw = tw(c, mt, ms), mx = pos.x + size.x * 0.5f, my2 = lb.y - ms * 1.1f;
                const float pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 4.0f);
                c.dl->AddTriangleFilled(ImVec2(mx - mw * 0.5f - 30.0f * dp, my2 - 5.0f * dp), ImVec2(mx - mw * 0.5f - 16.0f * dp, my2 - 5.0f * dp), ImVec2(mx - mw * 0.5f - 23.0f * dp, my2 + (5.0f + 3.0f * pulse) * dp), sk.red);
                txt_sh(c, ImVec2(mx - mw * 0.5f, my2 - ms * 0.55f), sk.cream, mt, ms, 1.0f);
                // un toucher sur l'indication fait defiler d'un ecran
                if (released && std::fabs(io.MousePos.x - mx) < mw * 0.5f + 50.0f * dp && io.MousePos.y >= lb.y - gh && io.MousePos.y <= lb.y)
                    st.scroll = std::min(max_scroll, st.scroll + view_h * 0.8f);
            }
            if (st.scroll > 2.0f * dp) {   // et vers le haut
                const float mx = pos.x + size.x * 0.5f;
                dl->AddTriangleFilled(ImVec2(mx - 8.0f * dp, la.y + 12.0f * dp), ImVec2(mx + 8.0f * dp, la.y + 12.0f * dp), ImVec2(mx, la.y + 3.0f * dp), sk.dim_text);
            }
        }
        if (io.MouseReleased[0]) { g_g.down = false; g_g.dragging = false; g_g.slider = -1; }
        if (page_to_open >= 0) open_page(page_to_open);
        else if (go_back) open_page(-1);

        // ----- description sous la liste, apercu a droite -----
        st.preview_active = false;
        if (in_page && st.page >= 0 && st.row < (int)cat->options.size()) {
            const Option &o = cat->options[st.row];
            float y = bot + 8.0f * dp;
            if (o.hint) y += twrap(c, ImVec2(pos.x + size.x * 0.14f, y), size.x * 0.72f, sk.dim_text, tr(o.hint), ts * 0.78f, true) + 4.0f * dp;
            if (o.warn && o.warn_when && o.value && o.warn_when(*o.value))
                twrap(c, ImVec2(pos.x + size.x * 0.14f, y), size.x * 0.72f, sk.red, tr(o.warn), ts * 0.78f, true);
            const char *cap_text = o.label_str.empty() ? tr(o.label) : o.label_str.c_str();
            const float bar_h = 26.0f * dp, ptop = top + 4.0f * dp + bar_h + 14.0f * dp, pleft = right + 38.0f * dp, pwid = end.x - pleft - 34.0f * dp;
            auto bar = [&] { caption_bar(c, ImVec2(pleft - 8.0f * dp, top + 4.0f * dp), ImVec2(pleft + pwid + 8.0f * dp, top + 4.0f * dp + bar_h), cap_text, ts * 0.8f); };
            if (o.table_fn && want_preview) {   // table des touches : panneau « papier » comme Use / Combine / Examine
                const auto rows = o.table_fn();
                const float rh2 = ts * 1.45f;
                const ImVec2 pa(pleft, ptop), pb(pa.x + pwid, pa.y + rh2 * (float)rows.size() + 20.0f * dp);
                bar();
                panel_frame(c, pa, pb, dp, true);
                float yy = pa.y + 10.0f * dp;
                const float isz = ts * 1.05f;
                for (const auto &rw : rows) {
                    const char *k = upper(tr(rw.first.c_str())), *v = upper(tr(rw.second.c_str()));
                    const Skin::Tex *ic = rw.first == "A" ? &sk.g_a : rw.first == "X" ? &sk.g_orb : rw.first == "L" ? &sk.g_lr : rw.first == "R" ? &sk.g_r : rw.first == "B" ? &sk.g_b : rw.first == "Y" ? &sk.g_y : rw.first == "Start" ? &sk.g_start : nullptr;
                    if (ic && ic->id) {   // a son rapport d'aspect (le X est plus etroit, START est large)
                        const float iw = std::min(isz * (float)ic->w / (float)std::max(1, ic->h), pwid * 0.34f);
                        dl->AddImage(ic->id, ImVec2(pa.x + 12.0f * dp, yy - 2.0f * dp), ImVec2(pa.x + 12.0f * dp + iw, yy - 2.0f * dp + isz));
                    }
                    else txt_sh(c, ImVec2(pa.x + 14.0f * dp, yy), sk.title, k, ts * 0.85f);
                    txt_sh(c, ImVec2(pa.x + pwid * 0.40f, yy), sk.cream_hi, v, ts * 0.85f);
                    yy += rh2;
                }
            }
            if (o.preview_fn && want_preview) {
                const ImVec2 pa(pleft, ptop), pb(pa.x + pwid, pa.y + pwid * o.preview_h);
                bar();
                panel_frame(c, pa, pb, dp, false);
                dl->PushClipRect(pa, pb, true);
                o.preview_fn(dl, pa, pb, dp);
                dl->PopClipRect();
                if (o.preview_cap) twrap(c, ImVec2(pa.x, pb.y + 14.0f * dp), pwid, sk.dim_text, tr(o.preview_cap), ts * 0.7f, true);
            }
            st.preview_active = o.preview != 0 && sk.shot_before.id && sk.shot_live.id;
            if (st.preview_active && want_preview) {   // avant / maintenant : l'image du jeu, coupee en deux
                const float bw = pwid, bh = bw * (float)sk.shot_live.h / std::max(1, sk.shot_live.w);
                const ImVec2 ba(pleft, ptop), bb(ba.x + bw, ba.y + bh);
                bar();
                panel_frame(c, ba, bb, dp, false);
                const float sx = ba.x + bw * st.split;
                dl->AddImage(sk.shot_before.id, ba, ImVec2(sx, bb.y), ImVec2(0.0f, 1.0f), ImVec2(st.split, 0.0f));
                dl->AddImage(sk.shot_live.id, ImVec2(sx, ba.y), bb, ImVec2(st.split, 1.0f), ImVec2(1.0f, 0.0f));
                dl->AddLine(ImVec2(sx, ba.y), ImVec2(sx, bb.y), sk.cream, 2.0f * dp);
                dl->AddCircleFilled(ImVec2(sx, (ba.y + bb.y) * 0.5f), 9.0f * dp, sk.cream);
                txt_sh(c, ImVec2(ba.x + 6.0f * dp, bb.y + 14.0f * dp), sk.cream, tr("Before"), ts * 0.75f, 1.0f);
                const char *nw = tr("Now");
                txt_sh(c, ImVec2(bb.x - tw(c, nw, ts * 0.75f), bb.y + 14.0f * dp), sk.cream, nw, ts * 0.75f, 1.0f);
                if (ImGui::IsMouseDown(0) && io.MousePos.x >= ba.x - 10.0f * dp && io.MousePos.x <= bb.x + 10.0f * dp && io.MousePos.y >= ba.y && io.MousePos.y <= bb.y)
                    st.split = std::clamp((io.MousePos.x - ba.x) / bw, 0.05f, 0.95f);
            }
        }
        {   // glissiere du jeu (option.dat) ; le repere descend avec la ligne choisie : on voit ou l'on est dans le menu
            const float rx = end.x - 19.0f * dp, rw = 14.0f * dp, ry0 = top + 4.0f * dp, ry1 = bot - 4.0f * dp;
            // le repere suit le defilement quand la liste defile (molette, doigt, glissiere) ; sinon la ligne choisie
            const float f = max_scroll > 0.0f ? std::clamp(st.scroll / max_scroll, 0.0f, 1.0f) : (count > 1 ? (float)st.row / (float)(count - 1) : 0.0f);
            if (sk.scroll.id && sk.scroll.h > 24) {
                const float cap = 8.0f, hh = (float)sk.scroll.h, cd = cap * rw / (float)sk.scroll.w;   // embouts de 8 px de la texture
                const ImTextureID t = sk.scroll.id;
                dl->AddImage(t, ImVec2(rx, ry0), ImVec2(rx + rw, ry0 + cd), ImVec2(0, 0), ImVec2(1, cap / hh));
                dl->AddImage(t, ImVec2(rx, ry0 + cd), ImVec2(rx + rw, ry1 - cd), ImVec2(0, cap / hh), ImVec2(1, 1.0f - cap / hh));
                dl->AddImage(t, ImVec2(rx, ry1 - cd), ImVec2(rx + rw, ry1), ImVec2(0, 1.0f - cap / hh), ImVec2(1, 1));
            } else {
                dl->AddRectFilled(ImVec2(rx + rw * 0.4f, ry0), ImVec2(rx + rw * 0.6f, ry1), sk.line);
            }
            const float my = ry0 + (ry1 - ry0) * f;
            dl->AddRectFilled(ImVec2(rx - 4.0f * dp, my - 1.5f * dp), ImVec2(rx + rw + 4.0f * dp, my + 1.5f * dp), sk.cream_hi, 1.0f * dp);
        }
    }

    // retour : B, Echap, ou fleche en haut a gauche
    {
        const ImVec2 m(pos.x + 34.0f * dp, pos.y + 16.0f * dp + tt * 0.5f);
        arrow(c, m, 13.0f * dp, -1, sk.cream);
        const bool hit = io.MouseReleased[0] && !g_g.dragging && std::fabs(io.MousePos.x - m.x) < 34.0f * dp && std::fabs(io.MousePos.y - m.y) < 34.0f * dp;
        if (hit || ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
            if (in_page) { const int pp = st.page; st.page = -1; st.row = pp; st.scroll = 0.0f; st.velocity = 0.0f; }
            else quitter = true;
        }
    }
    hint_bar(c, pos, size, dp, sk.touch);
    return changed;
}

}  // namespace nj::rmenu
