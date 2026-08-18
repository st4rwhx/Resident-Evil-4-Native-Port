// Icones d'armes generees chez le joueur (19/10/2026) : au premier lancement avec un disque, le moteur rend les modeles d'objet du disque
// (icon_render.cpp) dans un fil de fond, range le resultat dans le cache local (icons_v1.bin, jamais ailleurs) et le donne a la couche
// tactile (touch_overlay.cpp : atlas dynamique). Aux lancements suivants, le cache sert directement. Rien n'est livre avec l'application.
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>

#include "input/icon_render.h"
#include "input/touch_overlay.h"
#include "../../launcher/host.h"
#include "runtime/log.h"
#include "runtime/obf.h"
#include "runtime/virtual_disc.h"

using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace nj::runtime::input::touch {
namespace {

constexpr int kCell = 256, kCols = 8;
constexpr uint32_t kGenVersion = 6;   // a monter quand le rendu change : les caches anciens sont refaits (4, 21/10 : couteaux des archives d'arme ; 5, 28/10 : noms des couteaux corriges, mallettes)
enum State { Idle, Working, Ready, Done };
std::atomic<int> g_state{Idle};
std::thread g_thread;
std::vector<std::string> g_names;
std::vector<uint8_t> g_atlas;
int g_atlas_w = 0, g_atlas_h = 0;

std::string cache_path() { return nj::launcher::host::data_dir() + "/icons_v3.bin"; }

uint32_t fnv(const std::vector<uint8_t>& b) {
    uint32_t h = 2166136261u;
    for (uint8_t c : b) h = (h ^ c) * 16777619u;
    return h;
}

// Empreinte du disque pour le cache : contenu de l'objet de reference (le meme disque donne les memes icones).
uint32_t disc_fingerprint() {
    std::vector<uint8_t> b;
    char path[48];
    std::snprintf(path, sizeof path, "ss/Item/idm%03x.bin"_g, 0x21);
    if (!DiscView::get().read_file(path, b)) return 0;
    return fnv(b) ^ kGenVersion;
}

struct GenIcon {
    int id;
    std::vector<uint8_t> rgba;
};

void build_atlas(const std::vector<GenIcon>& icons) {
    const int rows = ((int)icons.size() + kCols - 1) / kCols;
    g_atlas_w = kCols * kCell;
    g_atlas_h = std::max(1, rows) * kCell;
    g_atlas.assign((size_t)g_atlas_w * g_atlas_h * 4, 0);
    g_names.clear();
    for (size_t k = 0; k < icons.size(); ++k) {
        char nm[16];
        if (icons[k].id >= 0x200)   // mallettes S, M, L, LL (28/10) : le tactile les cherche sous c_case0..3
            std::snprintf(nm, sizeof nm, "c_case%d", icons[k].id - 0x200);
        else
            std::snprintf(nm, sizeof nm, "w_it%02x", icons[k].id);
        g_names.push_back(nm);
        const int cx = (int)(k % kCols) * kCell, cy = (int)(k / kCols) * kCell;
        for (int y = 0; y < kCell; ++y)
            std::memcpy(&g_atlas[((size_t)(cy + y) * g_atlas_w + cx) * 4], &icons[k].rgba[(size_t)y * kCell * 4], (size_t)kCell * 4);
    }
}

bool load_cache(uint32_t fp, std::vector<GenIcon>& icons) {
    std::ifstream f(cache_path(), std::ios::binary);
    if (!f) return false;
    uint32_t head[4] = {};
    f.read((char*)head, sizeof head);
    if (!f || head[0] != 0x434A4E49u /* "INJC" */ || head[1] != fp || head[2] > 200 || head[3] != kCell) return false;
    for (uint32_t i = 0; i < head[2]; ++i) {
        GenIcon ic;
        uint32_t id = 0;
        f.read((char*)&id, 4);
        ic.id = (int)id;
        ic.rgba.resize((size_t)kCell * kCell * 4);
        f.read((char*)ic.rgba.data(), (std::streamsize)ic.rgba.size());
        if (!f) return false;
        icons.push_back(std::move(ic));
    }
    return !icons.empty();
}

void save_cache(uint32_t fp, const std::vector<GenIcon>& icons) {
    std::ofstream f(cache_path(), std::ios::binary | std::ios::trunc);
    if (!f) return;
    const uint32_t head[4] = {0x434A4E49u, fp, (uint32_t)icons.size(), (uint32_t)kCell};
    f.write((const char*)head, sizeof head);
    for (const GenIcon& ic : icons) {
        const uint32_t id = (uint32_t)ic.id;
        f.write((const char*)&id, 4);
        f.write((const char*)ic.rgba.data(), (std::streamsize)ic.rgba.size());
    }
}

// Modele et textures d'une archive d'arme em/wepNN.drs (format lu par nos outils, S:/RE4444/tools/fcv.py archive_files) :
// enveloppe (taille +0x24, decalage +0x2C), puis le nombre d'entrees, leurs decalages et leurs extensions sur 4 octets.
// Modele : le BIN de plus petit numero qui a plus de 40 sommets (les suivants portent des accessoires) ; textures : le TPL voisin.
bool weapon_archive_model(int wep, std::vector<uint8_t>& bin, std::vector<uint8_t>& tpl) {
    std::vector<uint8_t> d;
    char path[32];
    std::snprintf(path, sizeof path, "em/wep%02d.drs"_g, wep);
    if (!DiscView::get().read_file(path, d) || d.size() < 0x30) return false;
    auto be = [](const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; };
    const uint32_t off = be(&d[0x2C]), size = be(&d[0x24]);
    if ((uint64_t)off + size > d.size() || size < 16) return false;
    const uint8_t* A = &d[off];
    const uint32_t n = be(A);
    if (n == 0 || n > 4096) return false;
    const uint32_t base = (be(A + 4) == 0 && be(A + 8) == 0) ? 0x10 : 4;
    if ((uint64_t)base + 8ull * n > size) return false;
    std::vector<uint32_t> offs(n);
    std::vector<std::string> exts(n);
    for (uint32_t i = 0; i < n; ++i) {
        offs[i] = be(A + base + 4 * i);
        const char* e = (const char*)A + base + 4 * n + 4 * i;
        exts[i].assign(e, strnlen(e, 4));
    }
    auto blob = [&](uint32_t i, std::vector<uint8_t>& out) {
        uint32_t end = size;
        for (uint32_t j = 0; j < n; ++j)
            if (offs[j] > offs[i] && offs[j] < end) end = offs[j];
        if (offs[i] >= end) return false;
        out.assign(A + offs[i], A + end);
        return true;
    };
    for (uint32_t i = 0; i < n; ++i) {
        if (exts[i] != "BIN"_g || !blob(i, bin) || nj::icons::model_vertex_count(bin) <= 40) continue;
        for (int dj : {-1, 1, -2, 2}) {
            const int j = (int)i + dj;
            if (j >= 0 && j < (int)n && exts[j] == "TPL"_g && blob((uint32_t)j, tpl)) return true;
        }
        tpl.clear();
        return true;
    }
    return false;
}

// Une entree d'un conteneur .dat (nombre d'entrees, decalages, extensions) : meme format que ss_pzzl.dat / les .dat d'ecran.
bool dat_entry(const std::vector<uint8_t>& d, uint32_t index, std::vector<uint8_t>& out) {
    auto be = [&](size_t o) { return o + 4 <= d.size() ? (uint32_t)d[o] << 24 | (uint32_t)d[o + 1] << 16 | (uint32_t)d[o + 2] << 8 | d[o + 3] : 0u; };
    const uint32_t n = be(0);
    if (n == 0 || n > 4096 || index >= n) return false;
    const uint32_t base = (be(4) == 0 && be(8) == 0) ? 0x10 : 4;
    if ((uint64_t)base + 8ull * n > d.size()) return false;
    const uint32_t start = be(base + 4 * index);
    uint32_t end = (uint32_t)d.size();
    for (uint32_t j = 0; j < n; ++j) {
        const uint32_t o = be(base + 4 * j);
        if (o > start && o < end) end = o;
    }
    if (start >= end || end > d.size()) return false;
    out.assign(d.begin() + start, d.begin() + end);
    return true;
}

void work(uint32_t fp) {
    std::vector<GenIcon> icons;
    for (int id : nj::icons::weapon_item_ids()) {
        std::vector<uint8_t> bin, tpl;
        char path[48];
        std::snprintf(path, sizeof path, "ss/Item/idm%03x.bin"_g, id);
        if (!DiscView::get().read_file(path, bin)) continue;
        std::snprintf(path, sizeof path, "ss/Item/idm%03x.tpl"_g, id);
        DiscView::get().read_file(path, tpl);
        nj::icons::Image im;
        if (nj::icons::render_item_icon(bin, tpl, id, kCell, im)) icons.push_back({id, std::move(im.rgba)});
    }
    for (int wep : {26}) {   // couteau de Krauser (wep26, w_it126) ; celui de Leon reste l'objet 0x38 (w_it38) comme avant le 21/10
        std::vector<uint8_t> bin, tpl;
        nj::icons::Image im;
        if (weapon_archive_model(wep, bin, tpl) && nj::icons::render_archive_icon(bin, tpl, kCell, 0.0f, 90.0f, 90.0f, false, im))   // vue d'origine : lame a gauche, gravure visible
            icons.push_back({0x100 + (wep / 10) * 0x10 + wep % 10, std::move(im.rgba)});   // 0x116 / 0x126 : w_it116 / w_it126, les noms que cherche le tactile (avant : 0x110 / 0x11a, jamais trouves)
        else
            NJ_PRINTF("[Icones] couteau wep%02d : modele introuvable\n", wep);
    }
    {   // mallettes S / M / L / LL : modeles 3D d'origine, texture 417 (ss/Eng/ss_pzzl.dat, entrees 418 a 421)
        std::vector<uint8_t> pz, tpl;
        if (DiscView::get().read_file("ss/Eng/ss_pzzl.dat"_g, pz) && dat_entry(pz, 417, tpl)) {
            for (int k = 0; k < 4; ++k) {
                std::vector<uint8_t> bin;
                nj::icons::Image im;
                if (dat_entry(pz, 418 + k, bin) && nj::icons::render_case_icon(bin, tpl, kCell, 25.0f, -25.0f, im))
                    icons.push_back({0x200 + k, std::move(im.rgba)});
                else
                    NJ_PRINTF("[Icones] mallette %d : modele introuvable\n", k);
            }
        } else {
            NJ_PRINTF("[Icones] mallettes : ss_pzzl.dat introuvable\n");
        }
    }
    if (!icons.empty()) {
        save_cache(fp, icons);
        build_atlas(icons);
        NJ_PRINTF("[Icones] %zu icones generees\n", icons.size());
        g_state = Ready;
    } else {
        g_state = Done;   // disque sans ces modeles : les icones generiques restent
    }
}

}  // namespace

void icon_gen_poll() {
    switch (g_state.load()) {
    case Idle: {
        if (!DiscView::get().valid()) return;
        const uint32_t fp = disc_fingerprint();
        if (!fp) {
            g_state = Done;
            return;
        }
        std::vector<GenIcon> icons;
        if (load_cache(fp, icons)) {
            NJ_PRINTF("[Icones] cache charge : %zu icones\n", icons.size());
            build_atlas(icons);
            g_state = Ready;
            return;
        }
        NJ_PRINTF("[Icones] generation des icones (premier lancement avec ce disque)\n");
        g_state = Working;
        g_thread = std::thread(work, fp);
        g_thread.detach();
        break;
    }
    case Ready:
        overlay_set_dynamic_icons(g_names, g_atlas, g_atlas_w, g_atlas_h, kCell);
        g_atlas.clear();
        g_atlas.shrink_to_fit();
        g_state = Done;
        break;
    default:
        break;
    }
}

// Une icone d'arme du cache (icons_v3.bin) par son numero d'objet : pour le menu (apercu de visee). Faux sans cache.
bool weapon_icon_rgba(int item_id, std::vector<uint8_t>& rgba, int& size) {
    std::ifstream f(cache_path(), std::ios::binary);
    if (!f) return false;
    uint32_t head[4] = {};
    f.read((char*)head, sizeof head);
    if (!f || head[0] != 0x434A4E49u || head[2] > 200 || head[3] != (uint32_t)kCell) return false;
    for (uint32_t i = 0; i < head[2]; ++i) {
        uint32_t id = 0;
        f.read((char*)&id, 4);
        if (!f) return false;
        if ((int)id == item_id) {
            rgba.resize((size_t)kCell * kCell * 4);
            f.read((char*)rgba.data(), (std::streamsize)rgba.size());
            size = kCell;
            return (bool)f;
        }
        f.seekg((std::streamoff)kCell * kCell * 4, std::ios::cur);
    }
    return false;
}

}  // namespace nj::runtime::input::touch
