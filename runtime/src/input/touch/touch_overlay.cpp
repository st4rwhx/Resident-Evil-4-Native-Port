#include "input/touch_overlay.h"
#include "runtime/log.h"
#include "runtime/gfx_settings.h"

#include "common/gl.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"
#include "runtime/obf.h"

using namespace nj::obf_litteraux;   // "texte"_g : GLSL chiffre dans le binaire

namespace nj::runtime::input::touch {

namespace {

#include "touch_atlas.inc"
#include "touch_action_atlas.inc"   // icones d'action : 2e texture (l'atlas principal ne grandit plus)

constexpr int kCols = kAtlasW / kAtlasCell;
static_assert(kIconCount == IconWiiCursor + 1, "enum Icon et NAMES de tools/touch_icons.py divergent");

// Case de l'atlas a dessiner : l'icone, ou l'image courante de son animation.
int atlas_cell(const Sprite& s) {
    for (const Flipbook& f : kFlipbooks) {
        if (f.icon != s.icon || f.count <= 0)
            continue;
        const float t = s.anim >= 0.0f ? s.anim : SDL_GetTicks() * 0.001f;
        return f.first + (int)(t * (float)f.fps) % f.count;
    }
    return s.icon;
}

#ifdef NJ_GLES
const char* kHeader = "#version 300 es\nprecision mediump float;\n"_g;
#else
const char* kHeader = "#version 330 core\n"_g;
#endif

const char* kVert =
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_col;\n"
    "uniform vec2 u_size;\n"
    "out vec2 v_uv; out vec4 v_col;\n"
    "void main() {\n"
    "  v_uv = a_uv; v_col = a_col;\n"
    "  gl_Position = vec4(a_pos.x / u_size.x * 2.0 - 1.0, 1.0 - a_pos.y / u_size.y * 2.0, 0.0, 1.0);\n"
    "}\n"_g;

// L'atlas est en couleur, alpha premultiplie au chargement ; la couleur du sommet teinte
// (blanc = couleurs d'origine) et son alpha regle l'opacite du bouton.
const char* kFrag =
    "uniform sampler2D u_tex;\n"
    "in vec2 v_uv; in vec4 v_col;\n"
    "out vec4 o_col;\n"
    "void main() {\n"
    "  vec4 t = texture(u_tex, v_uv);\n"
    "  o_col = vec4(t.rgb * v_col.rgb, t.a) * v_col.a;\n"
    "}\n"_g;

struct Vertex {
    float x, y, u, v;
    uint32_t rgba;  // octets dans l'ordre R, G, B, A en memoire
};

GLuint g_prog = 0, g_vao = 0, g_vbo = 0, g_tex = 0, g_tex2 = 0, g_tex3 = 0;
// Atlas des armes (3e texture) : celui du binaire (vide depuis le 19/10) ou celui que le moteur genere chez le joueur (icon_gen.cpp).
int g_big_w = kBigW, g_big_h = kBigH, g_big_cell = kBigCell;
std::vector<std::string> g_dyn_names;
std::vector<uint8_t> g_dyn_px;
int g_dyn_w = 0, g_dyn_h = 0, g_dyn_cell = 0;
bool g_dyn_dirty = false;
GLint g_u_size = -1;
bool g_failed = false;

GLuint compile(GLenum type, const char* body) {
    GLuint s = glCreateShader(type);
    const char* src[2] = {kHeader, body};
    glShaderSource(s, 2, src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        NJ_PRINTF("[Tactile] shader refuse : %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

// Charge un atlas PNG dans la texture `tex` (alpha premultiplie, mipmaps).
bool upload_atlas(GLuint tex, const unsigned char* png, int png_size, int aw, int ah) {
    nj_tex2d(tex);
    int iw = 0, ih = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(png, png_size, &iw, &ih, &n, 4);
    if (!px || iw != aw || ih != ah) {
        NJ_PRINTF("[Tactile] atlas illisible\n");
        stbi_image_free(px);
        return false;
    }
    // Alpha premultiplie : sans lui, le filtrage melange le blanc des traits avec le noir des
    // pixels transparents voisins et cerne chaque icone d'un liseré sombre.
    for (int i = 0; i < iw * ih; ++i) {
        unsigned char* p = px + i * 4;
        for (int k = 0; k < 3; ++k)
            p[k] = (unsigned char)((p[k] * p[3] + 127) / 255);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, aw, ah, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    stbi_image_free(px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    nj_tex_mag(GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return true;
}

bool init() {
    if (g_prog || g_failed)
        return g_prog != 0;
    GLuint vs = compile(GL_VERTEX_SHADER, kVert), fs = compile(GL_FRAGMENT_SHADER, kFrag);
    if (!vs || !fs) {
        g_failed = true;
        return false;
    }
    g_prog = glCreateProgram();
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glLinkProgram(g_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(g_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        NJ_PRINTF("[Tactile] programme refuse\n");
        glDeleteProgram(g_prog);
        g_prog = 0;
        g_failed = true;
        return false;
    }
    g_u_size = glGetUniformLocation(g_prog, "u_size"_g);

    g_tex = nj_new_tex();
    g_tex2 = nj_new_tex();
    g_tex3 = nj_new_tex();
    if (!upload_atlas(g_tex, kAtlasPng, (int)sizeof(kAtlasPng), kAtlasW, kAtlasH) ||
        !upload_atlas(g_tex2, kActPng, (int)sizeof(kActPng), kActW, kActH) ||
        !upload_atlas(g_tex3, kBigPng, (int)sizeof(kBigPng), kBigW, kBigH)) {
        g_failed = true;
        return false;
    }

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(4 * sizeof(float)));
    glBindVertexArray(0);
    NJ_PRINTF("[Tactile] incrustation prete (atlas %dx%d)\n", kAtlasW, kAtlasH);
    return true;
}

uint32_t to_bytes(uint32_t rgba) {
    // 0xRRGGBBAA -> octets R,G,B,A en memoire, quel que soit le boutisme de l'hote
    uint8_t b[4] = {uint8_t(rgba >> 24), uint8_t(rgba >> 16), uint8_t(rgba >> 8), uint8_t(rgba)};
    uint32_t out;
    std::memcpy(&out, b, 4);
    return out;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Bandeau de l'ecran titre : « native » puis « ANDROID / WINDOWS PORT BY AYANO », en Pathway Gothic One
// (police libre, usure a nous : tools/title_brand.py, tools/title_banner_atlas.py). Memes effets que le logo du jeu
// (title.dat, table 6) : lettres gris sombre (72, 71, 68) sur un halo additif chaud
// (235, 195, 125) dont l'intensite scintille en boucle sur la courbe de l'unite #24 ; a
// l'apparition, chaque lettre s'allume a son tour avec un eclat de halo.

namespace {

// Courbe de couleur de l'unite #24 (halo du logo) : (image a 30/s, valeur / 255), en boucle.
constexpr float kFlicker[][2] = {{0, 128}, {2, 150.4f}, {4, 114.4f}, {7, 133.6f}, {9, 90.4f},
                                 {10, 138.4f}, {11, 98.4f}, {14, 120.8f}, {29, 93.6f},
                                 {34.7f, 114.8f}, {37, 92.8f}, {38, 136}};

float flicker(float t) {
    const int n = sizeof(kFlicker) / sizeof(kFlicker[0]);
    const float period = kFlicker[n - 1][0];
    float f = std::fmod(t * 30.0f, period);
    for (int i = 1; i < n; ++i) {
        if (f <= kFlicker[i][0]) {
            const float a = kFlicker[i - 1][0], b = kFlicker[i][0];
            const float k = (f - a) / std::max(b - a, 1e-3f);
            return (kFlicker[i - 1][1] + (kFlicker[i][1] - kFlicker[i - 1][1]) * k) / 255.0f;
        }
    }
    return kFlicker[0][1] / 255.0f;
}

uint32_t rgba(float r, float g, float b, float a) {
    auto c = [](float v) { return (uint32_t)std::clamp(v, 0.0f, 255.0f); };
    return c(r) << 24 | c(g) << 16 | c(b) << 8 | c(a * 255.0f);
}

float smooth(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

} // namespace

void title_banner_draw(bool title_menu, float menu_alpha, float fade_cover, int draw_w, int draw_h) {
    // Le bandeau s'allume quand le menu du titre apparait (fin de l'intro du logo), puis suit
    // l'opacite du menu et les fondus du jeu.
    static uint32_t since = 0;
    if (!title_menu) {
        since = 0;
        return;
    }
    const uint32_t now = SDL_GetTicks();
    if (since == 0 && menu_alpha > 0.5f)
        since = now;
    if (since == 0)
        return;
    const float t = (now - since) * 0.001f;
    const float vis = (1.0f - std::clamp(fade_cover, 0.0f, 1.0f)) * std::clamp(menu_alpha, 0.0f, 1.0f);
    if (vis <= 0.01f)
        return;

    // L'image du titre est un 16:9 (640 x 360) au milieu de l'image 4:3 du jeu, entre deux bandes
    // noires (unites #00 et #01 de la table 6) : les fractions de la maquette s'y rapportent.
    const Rect g4 = game_rect(draw_w, draw_h);
    const Rect gr{g4.x, g4.y + g4.h * 0.125f, g4.w, g4.h * 0.75f};
    const float cx = gr.x + gr.w * 0.5f;
    const float lh = gr.h * kTitleNativeH;              // hauteur des lettres
    const float cy = gr.y + gr.h * kTitleNativeTop + lh * 0.5f;
    title_banner_draw_at(cx, cy, lh, t, vis, draw_w, draw_h);
}

void title_banner_draw_at(float cx, float cy, float lh, float t, float vis, int draw_w, int draw_h) {
    const float grh = lh / kTitleNativeH;               // hauteur de l'image du titre correspondante
    const float cell = lh / kTitleLetterInCell;         // cote d'une case a l'ecran
    // courbe du jeu (0,35..0,59), portee a la force du halo du logo a l'ecran (maquette validee)
    const float glow = flicker(t) * 2.4f;

    static std::vector<Sprite> sprites;
    sprites.clear();
    for (int i = 0; i < kTitleLetterCount; ++i) {
        // la lettre i s'allume a 0,3 s + 0,12 s * i, en 0,5 s, avec un eclat de halo
        const float q = (t - 0.3f - 0.12f * i) / 0.5f;
        const float on = smooth(q);
        if (on <= 0.0f)
            continue;
        const float flash = q > 0 && q < 2 ? std::sin(std::min(q, 2.0f) * 1.5707963f) * (q < 1 ? 1.0f : 2.0f - q) : 0.0f;
        const float x = cx + kTitleLetterDx[i] * lh;
        Sprite h{IconBlank, x, cy, cell, rgba(235, 195, 125, vis * on * (glow + 0.9f * flash))};
        h.cell = kTitleLetterFirst + 2 * i + 1;
        h.additive = true;
        sprites.push_back(h);
        Sprite l{IconBlank, x, cy, cell, rgba(72, 71, 68, vis * on)};
        l.cell = kTitleLetterFirst + 2 * i;
        sprites.push_back(l);
    }
    // la ligne arrive en fondu une fois le mot allume
    const float ql = smooth((t - 1.2f) / 0.6f);
    if (ql > 0.0f) {
        const float lcell = grh * kTitleLineH / kTitleLineInCell;
#ifdef __ANDROID__
        const int first = kTitleLineAndroid, halo = kTitleLineGlowAndroid;
#else
        const int first = kTitleLineWindows, halo = kTitleLineGlowWindows;
#endif
        // 13/10 : meme traitement que « native » (halo chaud additif qui scintille, lettres sombres)
        const float ly = cy - lh * 0.5f + grh * (kTitleLineTop - kTitleNativeTop + kTitleLineH * 0.5f);
        Sprite h{IconBlank, cx, ly, lcell, rgba(235, 195, 125, vis * ql * glow)};
        h.cell = halo;
        h.span = (uint8_t)kTitleLineCols;
        h.additive = true;
        sprites.push_back(h);
        Sprite s{IconBlank, cx, ly, lcell, rgba(72, 71, 68, vis * ql)};
        s.cell = first;
        s.span = (uint8_t)kTitleLineCols;
        sprites.push_back(s);
    }
    overlay_draw(sprites, draw_w, draw_h);
}

namespace {
bool g_wide = false;
}

void set_wide(bool on) { g_wide = on; }
bool wide() { return g_wide; }

// Rectangle ou presenter une image de proportions `content` (largeur / hauteur) sur un ecran w x h, selon Screen fit / Picture ratio
// (menu F10, 19/10). Le rectangle peut depasser l'ecran (Remplir) : le blit et le toucher s'en accommodent.
Rect fit_rect(int w, int h, float content) {
    const float fw = (float)w, fh = (float)h, s = fw / fh;
    float a = content;
    if (const float r = nj::runtime::gfx::screen_ratio_value(nj::runtime::gfx::g_screen_ratio); r > 0.0f) a = r;
    switch (nj::runtime::gfx::g_screen_fit) {
    case 3:   // etire
        return {0.0f, 0.0f, fw, fh};
    case 2: {   // remplit : l'image couvre l'ecran, le surplus est coupe
        const float gw = s > a ? fw : fh * a, gh = s > a ? fw / a : fh;
        return {(fw - gw) * 0.5f, (fh - gh) * 0.5f, gw, gh};
    }
    case 0:   // automatique : sur un ecran plus large, l'image s'etire d'au plus 10 % pour reduire les barres
        if (s > a) a = std::min(s, a * 1.10f);
        break;
    default:
        break;
    }
    if (s > a) {
        const float gw = fh * a;
        return {(fw - gw) * 0.5f, 0.0f, gw, fh};
    }
    const float gh = fw / a;
    return {0.0f, (fh - gh) * 0.5f, fw, gh};
}

Rect wide_rect(int w, int h) { return fit_rect(w, h, 16.0f / 9.0f); }

Rect game_rect(int w, int h) {
    if (g_wide) {
        const Rect r = wide_rect(w, h);
        const float gh = r.h / kWideH;
        return {r.x, r.y - gh * kWideTop, r.w, gh};
    }
    return fit_rect(w, h, 4.0f / 3.0f);
}

int overlay_hitmarker_cell() { return kAtlasHitmarkerCell; }
int overlay_style_cell(int k) { return kTouchStyleFirst + k; }

bool overlay_atlas_rgba(std::vector<unsigned char>& rgba, int& w, int& h) {
    int n = 0;
    unsigned char* px = stbi_load_from_memory(kAtlasPng, (int)sizeof(kAtlasPng), &w, &h, &n, 4);
    if (!px) return false;
    rgba.assign(px, px + (size_t)w * h * 4);
    stbi_image_free(px);
    return true;
}
int overlay_zoom_out_cell() { return kAtlasZoomOutCell; }

constexpr int kStaticBooks = (int)(sizeof(kActionBooks) / sizeof(kActionBooks[0]));

int overlay_action_book(const char* name) {
    for (int i = 0; i < kStaticBooks; ++i)
        if (std::strcmp(kActionBooks[i].name, name) == 0)
            return i;
    for (size_t k = 0; k < g_dyn_names.size(); ++k)   // icones generees : livres numerotes apres ceux du binaire
        if (g_dyn_names[k] == name)
            return kStaticBooks + (int)k;
    return -1;
}

void overlay_set_dynamic_icons(const std::vector<std::string>& names, const std::vector<uint8_t>& rgba, int w, int h, int cell) {
    g_dyn_names = names;
    g_dyn_px = rgba;
    g_dyn_w = w;
    g_dyn_h = h;
    g_dyn_cell = cell;
    g_dyn_dirty = true;
}

bool overlay_action_ring(int book) {
    return book >= 0 && book < kStaticBooks && kActionBooks[book].ring != 0;   // les icones generees n'ont pas d'anneau : le moteur le pose
}

int overlay_action_atlas(int book) {
    if (book >= kStaticBooks)
        return 2;
    return book >= 0 ? kActionBooks[book].atlas : 1;
}

int overlay_action_cell(int book, float t) {
    if (book >= kStaticBooks)
        return book - kStaticBooks;
    if (book < 0)
        return 0;
    const ActionBook& b = kActionBooks[book];
    return b.first + (int)(t * (float)b.fps) % std::max(1, b.count);
}

void overlay_draw(const std::vector<Sprite>& sprites, int draw_w, int draw_h) {
    if (sprites.empty() || draw_w <= 0 || draw_h <= 0 || !init())
        return;
    if (g_dyn_dirty) {   // icones generees chez le joueur : l'atlas des armes est remplace
        g_dyn_dirty = false;
        nj_tex2d(g_tex3);
        for (size_t i = 0; i + 3 < g_dyn_px.size(); i += 4)   // alpha premultiplie, comme l'atlas du binaire
            for (int k = 0; k < 3; ++k)
                g_dyn_px[i + k] = (unsigned char)((g_dyn_px[i + k] * g_dyn_px[i + 3] + 127) / 255);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_dyn_w, g_dyn_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_dyn_px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        nj_tex_mag(GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_2D);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        g_big_w = g_dyn_w;
        g_big_h = g_dyn_h;
        g_big_cell = g_dyn_cell;
        g_dyn_px.clear();
        g_dyn_px.shrink_to_fit();
    }

    // Deux lots : les sprites additifs (halos) d'abord, puis les autres par-dessus, comme le
    // logo du jeu (halo additif sous des lettres sombres).
    static std::vector<Vertex> verts;
    verts.clear();
    // Lots de dessin : l'ordre des sprites est garde, un lot change quand la texture (atlas) ou le melange change.
    struct Run { int atlas; bool add; size_t first, count; };
    static std::vector<Run> runs;
    runs.clear();
    const float cu1 = (float)kAtlasCell / kAtlasW, cv1 = (float)kAtlasCell / kAtlasH;
    const float cu2 = (float)kActCell / kActW, cv2 = (float)kActCell / kActH;
    const float cu3 = (float)g_big_cell / g_big_w, cv3 = (float)g_big_cell / g_big_h;
    constexpr int kCols2 = kActW / kActCell;
    const int kCols3 = g_big_w / g_big_cell;
    for (int pass = 0; pass < 2; ++pass) {
        for (const Sprite& s : sprites) {
            if (s.additive != (pass == 0))
                continue;
            const float cu = s.atlas == 2 ? cu3 : s.atlas == 1 ? cu2 : cu1, cv = s.atlas == 2 ? cv3 : s.atlas == 1 ? cv2 : cv1;
            const int cols = s.atlas == 2 ? kCols3 : s.atlas == 1 ? kCols2 : kCols;
            const int cell = s.cell >= 0 ? s.cell : atlas_cell(s);
            const float span = (float)std::max<int>(1, s.span);
            float u0 = (cell % cols) * cu;
            const float v0 = (cell / cols) * cv;
            float du = cu * span;
            if (s.flip) {  // miroir : on lit la case de droite a gauche
                u0 += du;
                du = -du;
            }
            const float h = s.size * 0.5f, hw = h * span;
            const float cs = std::cos(s.angle), sn = std::sin(s.angle);
            auto corner = [&](float dx, float dy, float u, float v) {
                return Vertex{s.x + dx * cs - dy * sn, s.y + dx * sn + dy * cs, u, v, to_bytes(s.rgba)};
            };
            float xa = -hw, xb = hw, ua = u0, ub = u0 + du;
            if (s.half < 0) { xb = 0; ub = u0 + du * 0.5f; }
            else if (s.half > 0) { xa = 0; ua = u0 + du * 0.5f; }
            const Vertex a = corner(xa, -h, ua, v0), b = corner(xb, -h, ub, v0);
            const Vertex c = corner(xb, h, ub, v0 + cv), d = corner(xa, h, ua, v0 + cv);
            if (runs.empty() || runs.back().atlas != s.atlas || runs.back().add != s.additive)
                runs.push_back({s.atlas, s.additive, verts.size(), 0});
            verts.insert(verts.end(), {a, b, c, a, c, d});
            runs.back().count += 6;
        }
    }

    GLint prev_prog = 0, prev_vao = 0, prev_buf = 0, prev_tex = 0, prev_active = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_buf);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    const GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cull = glIsEnabled(GL_CULL_FACE), scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLint bsrc = 0, bdst = 0, bsrca = 0, bdsta = 0, beq = 0, beqa = 0;
    glGetIntegerv(GL_BLEND_SRC_RGB, &bsrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &bdst);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &bsrca);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &bdsta);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &beq);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &beqa);
    GLboolean cmask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, cmask);

    nj_cap(GL_DEPTH_TEST, false);
    nj_cap(GL_CULL_FACE, false);
    nj_cap(GL_SCISSOR_TEST, false);
    nj_cap(GL_BLEND, true);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glUseProgram(g_prog);
    glUniform2f(g_u_size, (float)draw_w, (float)draw_h);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(Vertex)), verts.data(), GL_STREAM_DRAW);
    for (const Run& r : runs) {
        nj_tex2d(r.atlas == 2 ? g_tex3 : r.atlas == 1 ? g_tex2 : g_tex);
        // additif : la couleur (deja multipliee par l'alpha) s'ajoute
        glBlendFunc(GL_ONE, r.add ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
        glDrawArrays(GL_TRIANGLES, (GLint)r.first, (GLsizei)r.count);
    }
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray((GLuint)prev_vao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prev_buf);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
    glActiveTexture((GLenum)prev_active);
    glUseProgram((GLuint)prev_prog);
    glBlendFuncSeparate(bsrc, bdst, bsrca, bdsta);
    glBlendEquationSeparate(beq, beqa);
    glColorMask(cmask[0], cmask[1], cmask[2], cmask[3]);
    if (!blend) nj_cap(GL_BLEND, false);
    if (depth) nj_cap(GL_DEPTH_TEST, true);
    if (cull) nj_cap(GL_CULL_FACE, true);
    if (scissor) nj_cap(GL_SCISSOR_TEST, true);
}

} // namespace nj::runtime::input::touch
