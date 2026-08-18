// Voir ui_skin.h. Lecture des entrees d'un .dat d'ecran (EFF / UWF / ...) : meme conteneur que ss_pzzl.dat (roomdat.py de nos outils) :
// nombre d'entrees, decalages, extensions sur 4 octets ; les textures sont des TPL imbriques reperables par leur signature.
#include "ui_skin.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>

#ifndef NJ_LAB
#include "host.h"
#include "runtime/log.h"
#include "runtime/obf.h"
#include "runtime/virtual_disc.h"

using namespace nj::obf_litteraux;
#endif

namespace nj::ui_skin {
namespace {

constexpr uint32_t kVersion = 7;   // a monter quand les pieces changent
enum State { Idle, Working, Ready, Done };
std::atomic<int> g_state{Idle};
nj::icons::Image g_img[kPieceCount];
bool g_have[kPieceCount] = {};

struct Def {
    const char *file;   // dans ss/Eng/
    int w, h;           // taille de la texture cherchee (la premiere de cette taille)
    int cx, cy, cw, ch; // rognage ; cw == 0 : texture entiere
    bool raw;           // fichier .fnt : 0x20 octets d'en-tete puis un TPL (pas un conteneur d'entrees)
    int hud = -1;       // >= 0 : texture numero `hud` du HUD de etc/core.das (entree 25), au lieu d'une recherche par taille
};
const Def kDefs[kPieceCount] = {
    {"ss/Eng/ss_cmmn.dat", 511, 291, 0, 0, 0, 0, false},   // Bg : cloison rouillee
    {"ss/Eng/ss_cmmn.dat", 640, 360, 0, 0, 0, 0, false},   // Frame : cadre a engrenage
    {"ss/Eng/ss_file.dat", 241, 200, 0, 44, 228, 28, false},   // Row : une ligne de la liste des fichiers
    {"ss/Eng/ss_file.dat", 241, 200, 0, 0, 241, 28, false},    // Head : en-tete a fleches
    {"ss/Eng/ss_file.dat", 272, 232, 0, 0, 0, 0, false},       // Paper : papier a grain
    {"ss/Eng/ss_item.dat", 160, 288, 0, 0, 0, 0, false},       // Rack : rack de la mallette
    {"ss/Eng/ss_cap.dat", 259, 24, 0, 0, 24, 24, false},       // GlyphStick
    {"ss/Eng/ss_cap.dat", 259, 24, 78, 0, 24, 24, false},      // GlyphLR
    {"etc/core.das", 0, 0, 0, 0, 0, 0, false, 4},                // GlyphOrb : bouton X (HUD)
    {"font/sscrn_p.fnt", 1024, 1024, 0, 0, 0, 0, true},         // FontAtlas : police des sous-ecrans (latin complet, 32 px par case)
    {"ss/Eng/option.dat", 16, 104, 0, 0, 0, 0, false},          // Scroll : glissiere rouge a embouts (ecran d'options)
    {"etc/core.das", 0, 0, 0, 0, 0, 0, false, 24},               // GlyphA : bouton A vert (HUD)
    {"ss/Eng/ss_cap.dat", 259, 24, 106, 0, 24, 24, false},      // GlyphR : gachette R
    {"etc/core.das", 0, 0, 0, 0, 0, 0, false, 35},               // GlyphB : bouton B rouge (HUD)
    {"etc/core.das", 0, 0, 0, 0, 0, 0, false, 8},                // GlyphY : bouton Y (HUD)
    {"etc/core.das", 0, 0, 0, 0, 0, 0, false, 29},               // GlyphStart : « START / PAUSE » (HUD)
};

uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

uint32_t fnv(const std::vector<uint8_t> &b) {
    uint32_t h = 2166136261u;
    for (uint8_t c : b) h = (h ^ c) * 16777619u;
    return h;
}

#ifndef NJ_LAB
std::string cache_path() { return nj::launcher::host::data_dir() + "/ui_skin_v1.bin"; }

#endif

// Toutes les images (dans l'ordre) de tous les TPL imbriques d'un .dat.
bool all_textures(const std::vector<uint8_t> &d, std::vector<nj::icons::Image> &out) {
    if (d.size() < 16) return false;
    const uint32_t n = be32(&d[0]);
    if (n == 0 || n > 4096) return false;
    const uint32_t base = (be32(&d[4]) == 0 && be32(&d[8]) == 0) ? 0x10 : 4;
    if ((uint64_t)base + 8ull * n > d.size()) return false;
    std::vector<uint32_t> offs(n);
    for (uint32_t i = 0; i < n; ++i) offs[i] = be32(&d[base + 4 * i]);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t end = (uint32_t)d.size();
        for (uint32_t j = 0; j < n; ++j)
            if (offs[j] > offs[i] && offs[j] < end) end = offs[j];
        if (offs[i] + 12 >= end) continue;
        const uint8_t *b = &d[offs[i]];
        const size_t len = end - offs[i];
        for (size_t q = 0; q + 12 < len; ++q) {
            if (b[q] != 0x00 || b[q + 1] != 0x20 || b[q + 2] != 0xAF || b[q + 3] != 0x30) continue;
            std::vector<uint8_t> tpl(b + q, b + len);
            std::vector<nj::icons::Image> imgs;
            if (nj::icons::decode_tpl(tpl, imgs))
                for (auto &im : imgs)
                    if (im.w > 0 && im.h > 0) out.push_back(std::move(im));
        }
    }
    return !out.empty();
}

// .fnt : en-tete de 0x20 octets puis un TPL.
bool raw_textures(const std::vector<uint8_t> &d, std::vector<nj::icons::Image> &out) {
    for (size_t q = 0; q + 12 < d.size() && q < 0x100; ++q) {
        if (d[q] != 0x00 || d[q + 1] != 0x20 || d[q + 2] != 0xAF || d[q + 3] != 0x30) continue;
        std::vector<uint8_t> tpl(d.begin() + (long)q, d.end());
        return nj::icons::decode_tpl(tpl, out) && !out.empty();
    }
    return false;
}

// Textures du HUD (etc/core.das) : archive dont l'entree 25 porte, a partir de 0x2E0, une table de TPL (un fichier par entree ; la
// premiere image de chacun est gardee, a sa place : out[i] est le fichier i, vide s'il ne se lit pas). Voir tools/touch_hud_textures.py.
bool hud_textures(const std::vector<uint8_t> &d, std::vector<nj::icons::Image> &out) {
    if (d.size() < 0x40) return false;
    const uint32_t off = be32(&d[0x2c]), size = be32(&d[0x24]);
    if ((uint64_t)off + size > d.size() || size < 16) return false;
    const uint8_t *A = &d[off];
    const uint32_t n = be32(A);
    if (n < 26 || n > 4096) return false;
    const uint32_t base = (be32(A + 4) == 0 && be32(A + 8) == 0) ? 0x10 : 4;
    if ((uint64_t)base + 4ull * n > size) return false;
    const uint32_t e0 = be32(A + base + 4 * 25);
    uint32_t e1 = size;
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t o = be32(A + base + 4 * i);
        if (o > e0 && o < e1) e1 = o;
    }
    if (e0 + 0x2e0 + 8 >= e1) return false;
    const uint8_t *blob = A + e0 + 0x2e0;
    const uint32_t blen = std::min<uint32_t>(e1 - e0 - 0x2e0, 0x79660u - 0x2e0u);
    const uint32_t cnt = be32(blob);
    if (cnt == 0 || cnt > 256 || 4ull + 4ull * cnt > blen) return false;
    out.assign(cnt, nj::icons::Image{});
    for (uint32_t i = 0; i < cnt; ++i) {
        const uint32_t o = be32(blob + 4 + 4 * i), end = i + 1 < cnt ? be32(blob + 4 + 4 * (i + 1)) : blen;
        if (o + 12 >= end || end > blen) continue;
        for (uint32_t q = o; q + 12 < end && q < o + 0x100; ++q) {
            if (blob[q] != 0x00 || blob[q + 1] != 0x20 || blob[q + 2] != 0xAF || blob[q + 3] != 0x30) continue;
            std::vector<uint8_t> tpl(blob + q, blob + end);
            std::vector<nj::icons::Image> imgs;
            if (nj::icons::decode_tpl(tpl, imgs) && !imgs.empty()) out[i] = std::move(imgs[0]);
            break;
        }
    }
    return true;
}

bool crop(const nj::icons::Image &s, const Def &df, nj::icons::Image &o) {
    int cx = df.cx, cy = df.cy, cw = df.cw ? df.cw : s.w, ch = df.ch ? df.ch : s.h;
    if (cx < 0 || cy < 0 || cx + cw > s.w || cy + ch > s.h) return false;
    o.w = cw;
    o.h = ch;
    o.rgba.resize((size_t)cw * ch * 4);
    for (int y = 0; y < ch; ++y) std::memcpy(&o.rgba[(size_t)y * cw * 4], &s.rgba[((size_t)(cy + y) * s.w + cx) * 4], (size_t)cw * 4);
    return true;
}

#ifndef NJ_LAB
uint32_t fingerprint() {
    std::vector<uint8_t> b;
    if (!nj::runtime::DiscView::get().read_file("ss/Eng/ss_cmmn.dat"_g, b)) return 0;
    return fnv(b) ^ kVersion;
}

bool load_cache(uint32_t fp) {
    std::ifstream f(cache_path(), std::ios::binary);
    if (!f) return false;
    uint32_t head[3] = {};
    f.read((char *)head, sizeof head);
    if (!f || head[0] != 0x4B534A4Eu /* "NJSK" */ || head[1] != fp || head[2] != kPieceCount) return false;
    for (int i = 0; i < kPieceCount; ++i) {
        uint32_t wh[3] = {};   // largeur, hauteur, present
        f.read((char *)wh, sizeof wh);
        if (!f || wh[0] > 2048 || wh[1] > 2048) return false;
        g_have[i] = wh[2] != 0;
        g_img[i].w = (int)wh[0];
        g_img[i].h = (int)wh[1];
        g_img[i].rgba.resize((size_t)wh[0] * wh[1] * 4);
        f.read((char *)g_img[i].rgba.data(), (std::streamsize)g_img[i].rgba.size());
        if (!f) return false;
    }
    return true;
}

void save_cache(uint32_t fp) {
    std::ofstream f(cache_path(), std::ios::binary | std::ios::trunc);
    if (!f) return;
    const uint32_t head[3] = {0x4B534A4Eu, fp, (uint32_t)kPieceCount};
    f.write((const char *)head, sizeof head);
    for (int i = 0; i < kPieceCount; ++i) {
        const uint32_t wh[3] = {(uint32_t)g_img[i].w, (uint32_t)g_img[i].h, g_have[i] ? 1u : 0u};
        f.write((const char *)wh, sizeof wh);
        f.write((const char *)g_img[i].rgba.data(), (std::streamsize)g_img[i].rgba.size());
    }
}

#endif

int build(const Reader &read) {
    std::vector<nj::icons::Image> texs;
    std::string cur;
    for (int p = 0; p < kPieceCount; ++p) {
        if (cur != kDefs[p].file) {   // une lecture par fichier
            cur = kDefs[p].file;
            texs.clear();
            std::vector<uint8_t> d;
            bool ok = read(kDefs[p].file, d);
            if (!ok && kDefs[p].hud >= 0) ok = read("Etc/core.das", d);   // la casse du dossier change d'une edition a l'autre
            if (ok) (kDefs[p].hud >= 0 ? hud_textures(d, texs) : kDefs[p].raw ? raw_textures(d, texs) : all_textures(d, texs));
        }
        g_have[p] = false;
        if (kDefs[p].hud >= 0) {   // texture du HUD par son numero (carree et petite : sinon on n'y touche pas)
            const int i = kDefs[p].hud;
            if (i < (int)texs.size() && texs[i].w > 0 && texs[i].w <= 64 && texs[i].h <= 64) {
                g_img[p] = texs[i];
                g_have[p] = true;
            }
            continue;
        }
        for (const auto &t : texs)
            if (t.w == kDefs[p].w && t.h == kDefs[p].h && crop(t, kDefs[p], g_img[p])) {
                g_have[p] = true;
                break;
            }
    }
    int n = 0;
    for (bool h : g_have) n += h;
    return n;
}

#ifndef NJ_LAB
void work(uint32_t fp) {
    const int n = build([](const std::string &path, std::vector<uint8_t> &d) { return nj::runtime::DiscView::get().read_file(path.c_str(), d); });
    if (n > 0) save_cache(fp);
    NJ_PRINTF("[Menu] habillage : %d pieces sur %d\n", n, (int)kPieceCount);
    g_state = n > 0 ? Ready : Done;
}

#endif

}  // namespace

int build_from(const Reader &read) {
    const int n = build(read);
    if (n > 0) g_state = Ready;
    return n;
}

#ifndef NJ_LAB
void poll() {
    if (g_state.load() != Idle || !nj::runtime::DiscView::get().valid()) return;
    const uint32_t fp = fingerprint();
    if (!fp) {
        g_state = Done;
        return;
    }
    if (load_cache(fp)) {
        g_state = Ready;
        return;
    }
    g_state = Working;
    std::thread(work, fp).detach();
}

#endif

bool ready() { return g_state.load() == Ready; }

const nj::icons::Image *piece(int p) { return g_state.load() == Ready && p >= 0 && p < kPieceCount && g_have[p] ? &g_img[p] : nullptr; }

}  // namespace nj::ui_skin
