#include <cstdint>
#include "runtime/log.h"
#include "runtime/texture_cache_gl.h"
#include "runtime/gx/tex_decode.h"
#include "runtime/gx/fifo_parser.h"
#include <iostream>
#include <cstring>
#include <cstdio>
#include "runtime/env.h"
#include "runtime/gfx_settings.h"
#include "runtime/mods.h"
#include "runtime/njprof.h"
#include "runtime/perf_journal.h"
#include "runtime/obf.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../input/touch/stb_image.h"

// Textures HD et boutons : fournis par les mods actives (runtime/mods.h, F10 « Mods ») ; l'ancien
// dossier textures_hd/ a cote de l'exe compte comme un mod. Instantane de l'index, renouvele quand
// la liste des mods change (generation), lu par le seul fil du rendu.
namespace {
std::shared_ptr<const nj::mods::Index> s_ix = std::make_shared<nj::mods::Index>();
const nj::mods::Index& ix() { return *s_ix; }
bool hd_any() { return !ix().textures.empty(); }
// Formes (largeur x hauteur, format) des empreintes de l'index : une texture du jeu dont la forme
// n'y est pas ne peut rien remplacer, on ne la hache pas (15/10 : avec un style de boutons choisi,
// chaque nouvelle texture etait hachee en entier, palette comprise).
uint64_t shape_of(uint32_t w, uint32_t h, uint32_t fmt) { return (uint64_t)w << 32 | (uint64_t)h << 8 | fmt; }
bool shape_known(uint32_t w, uint32_t h, uint32_t fmt) {
    static const nj::mods::Index* built = nullptr;
    static std::unordered_set<uint64_t> shapes;
    if (built != s_ix.get()) {
        built = s_ix.get();
        shapes.clear();
        auto add = [](const std::string& k) {   // "<w>x<h>_<fmt>_<empreinte>"
            unsigned kw = 0, kh = 0, kf = 0;
            if (std::sscanf(k.c_str(), "%ux%u_%u_", &kw, &kh, &kf) == 3) shapes.insert(shape_of(kw, kh, kf));
        };
        for (const auto& kv : ix().textures) add(kv.first);
        for (const auto& st : ix().buttons)
            for (const auto& kv : st.second) add(kv.first);
    }
    return shapes.count(shape_of(w, h, fmt)) != 0;
}
bool hd_has(const std::string& k) { return ix().textures.count(k) != 0; }
// Listes par categorie (<nom>_keys.txt des mods) : objets, armes, mallette, HUD, decors, interface,
// cinematiques temps reel ; sans liste : personnages et ennemis (« Characters »).
bool in_list(const char* name, const std::string& k) {
    const auto it = ix().lists.find(name);
    return it != ix().lists.end() && it->second.count(k) != 0;
}

// Boutons au choix : buttons/<style>/<cle>.png des mods (et icones internes, ui_buttons/).
const std::unordered_map<std::string, std::string>* button_dir() {
    // Xbox a une variante par disposition : GameCube (A B X Y L R d'origine) ou RE4 UHD (Type I a
    // III : B -> X, X -> Y, L/R -> LB/RB... hud_buttons.py, dossier xbox_uhd).
    static const char* const kDirs[] = {"", "xbox", "playstation", "keyboard", "mobile", "xbox_uhd",
                                        "playstation_uhd"};
    int st = nj::runtime::gfx::g_button_style;
    if (st <= 0 || st > 4)
        return nullptr;
    if (st == 2)   // PlayStation retire (13/10) : Xbox
        st = 1;
    if ((st == 1 || st == 2) && nj::runtime::gfx::g_controller_layout >= 1 && nj::runtime::gfx::g_controller_layout <= 3)
        st += 4;
    const auto b = ix().buttons.find(kDirs[st]);
    return b == ix().buttons.end() || b->second.empty() ? nullptr : &b->second;
}
const std::string* button_file(const std::string& k) {
    const auto* d = button_dir();
    if (!d)
        return nullptr;
    const auto f = d->find(k);
    return f == d->end() ? nullptr : &f->second;
}

std::string hd_key(const uint8_t* p, uint32_t size, uint32_t w, uint32_t h, uint32_t fmt,
                   const uint8_t* pal = nullptr, uint32_t pal_bytes = 0) {
    uint64_t v = 0xCBF29CE484222325ull;
    for (uint32_t i = 0; i < size; ++i)
        v = (v ^ p[i]) * 0x100000001B3ull;
    for (uint32_t i = 0; i < pal_bytes; ++i)
        v = (v ^ pal[i]) * 0x100000001B3ull;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%ux%u_%u_%016llx", w, h, fmt, (unsigned long long)v);
    return buf;
}
}  // namespace





extern uint64_t g_st_frames;   // gx.cpp : images rendues par le fil hote
std::atomic<int> g_tex_verify_all{0};   // F12 : verification complete des textures pendant 3 images (main.cpp)

// Textures HD decodees hors du fil hote (01/10). Le chargement etait synchrone au premier usage :
// lecture du PNG, decodage (jusqu'a 2048x2048), envoi et mipmaps pendant l'image, d'ou des images
// de 100 a 200 ms quand une zone apparait (relevé [FRAME] render=, village 8x HD). Maintenant le
// premier usage met la texture GameCube et demande le PNG a deux fils de faible priorite ; des
// qu'il est pret, le fil hote l'envoie dans la meme texture GL (3 au plus par image).
// NJ_HDSYNC=1 : ancien chargement synchrone.
namespace {
struct HdDecoded {
    int w = 0, h = 0;
    unsigned char* px = nullptr;   // stbi ; nul = echec de lecture ou de decodage
};
class HdAsync {
public:
    static HdAsync& get() { static HdAsync* a = new HdAsync; return *a; }   // fils detaches : jamais detruit
    std::atomic<uint32_t> gen{0};   // augmente a chaque PNG pret

    void request(const std::string& path) {
        std::lock_guard<std::mutex> lk(m_mu);
        if (m_pending.count(path) || m_ready.count(path))
            return;
        m_pending.insert(path);
        m_queue.push_back(path);
        if (!m_started) {
            m_started = true;
            for (int i = 0; i < 2; ++i)
                std::thread([this] { worker(); }).detach();
        }
        m_cv.notify_one();
    }
    bool take(const std::string& path, HdDecoded& out) {
        std::lock_guard<std::mutex> lk(m_mu);
        auto it = m_ready.find(path);
        if (it == m_ready.end())
            return false;
        out = it->second;
        m_ready.erase(it);
        return true;
    }

private:
    void worker() {
        nj::runtime::njprof::register_thread("texhd");
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
        for (;;) {
            std::string path;
            {
                std::unique_lock<std::mutex> lk(m_mu);
                m_cv.wait(lk, [&] { return !m_queue.empty(); });
                path = std::move(m_queue.front());
                m_queue.pop_front();
            }
            HdDecoded d = decode(path);
            std::lock_guard<std::mutex> lk(m_mu);
            m_pending.erase(path);
            // Garde-fou : PNG prets dont la texture a quitte le cache entre-temps.
            if (m_ready.size() >= 128) {
                stbi_image_free(m_ready.begin()->second.px);
                m_ready.erase(m_ready.begin());
            }
            m_ready[path] = d;
            gen.fetch_add(1, std::memory_order_release);
        }
    }
    std::mutex m_mu;
    std::condition_variable m_cv;
    std::deque<std::string> m_queue;
    std::unordered_set<std::string> m_pending;
    std::unordered_map<std::string, HdDecoded> m_ready;
    bool m_started = false;

public:
    static HdDecoded decode(const std::string& path) {
        HdDecoded d;
        std::ifstream f(path, std::ios::in | std::ios::binary);
        std::vector<unsigned char> buf((std::istreambuf_iterator<char>(f)), {});
        int n = 0;
        if (!buf.empty())
            d.px = stbi_load_from_memory(buf.data(), (int)buf.size(), &d.w, &d.h, &n, 4);
        return d;
    }
};

// Envoi d'une image HD decodee dans la texture liee (unite 15), mipmaps compris ; rend le nombre
// de niveaux. Libere les pixels.
uint32_t upload_hd(HdDecoded& d) {
    uint32_t lv = 1;
    for (int m = d.w > d.h ? d.w : d.h; m > 1; m >>= 1)
        ++lv;
    // Bornes des niveaux AVANT glGenerateMipmap : une texture GameCube provisoire remplacee garde
    // sinon son MAX_LEVEL (chaine courte), les niveaux HD au-dela ne sont pas generes et la texture,
    // incomplete, se lit en noir (scene noire au premier essai, 01/10).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)lv - 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, d.w, d.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d.px);
    stbi_image_free(d.px);
    d.px = nullptr;
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    nj_tex_mag(GL_LINEAR);
    return lv;
}
}  // namespace

namespace nj::runtime::hle {

using gx::g_gxregs;

// Texture provisoire (version GameCube) dont le PNG HD est demande : le remplacer s'il est pret.
void TexCache::promote_hd(CachedTexture& e) {
    const uint32_t g = HdAsync::get().gen.load(std::memory_order_acquire);
    if (g == e.hd_gen)
        return;
    static uint64_t s_frame = ~0ull;
    static int s_budget = 0;
    if (s_frame != g_st_frames) {
        s_frame = g_st_frames;
        s_budget = 3;
    }
    if (s_budget <= 0)
        return;   // e.hd_gen inchange : on reessaie a l'image suivante
    e.hd_gen = g;
    HdDecoded d;
    if (!HdAsync::get().take(e.hd_path, d))
        return;
    e.hd_path.clear();
    if (!d.px)
        return;
    --s_budget;
    GLint prev_unit = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
    nj_unit(15);
    glBindTexture(GL_TEXTURE_2D, e.tex);
    tex_levels[e.tex] = upload_hd(d);
    hd_textures.insert(e.tex);
    glActiveTexture((GLenum)prev_unit);
    ++m_generation;   // la texture a change de taille et de niveaux
}

TexCache& TexCache::get() {
    static TexCache instance;
    return instance;
}

// Une texture du cache detruite, avec ce qu'on savait d'elle (niveaux, version HD).
// Budget du cache de textures en octets de GPU (26/10). Il n'etait borne qu'en NOMBRE d'entrees (2048) : sur un telephone a memoire
// partagee, chaque image d'une video (textures neuves de centaines de Ko) faisait monter la memoire graphique de 28 a 297 Mo en 43 s
// (S5, 1,8 Go) jusqu'a la mort par le tueur de memoire d'Android. Android : un dixieme de la RAM entre 96 et 512 Mo ; PC : 1,5 Go.
static uint64_t cache_budget_bytes() {
    static const uint64_t b = [] {
#if defined(__ANDROID__)
        uint64_t total = 0;
        if (FILE* f = std::fopen("/proc/meminfo", "r")) {
            char l[128];
            unsigned long long kb = 0;
            while (std::fgets(l, sizeof l, f))
                if (std::sscanf(l, "MemTotal: %llu kB", &kb) == 1) total = kb * 1024ull;
            std::fclose(f);
        }
        uint64_t v = total ? total / 10 : (192ull << 20);
        if (v < (96ull << 20)) v = 96ull << 20;
        if (v > (512ull << 20)) v = 512ull << 20;
        return v;
#else
        return 1536ull << 20;
#endif
    }();
    return b;
}

TexCache::Cache::iterator TexCache::drop(Cache::iterator it) {
    cache_bytes -= std::min<uint64_t>(cache_bytes, it->second.bytes);
    tex_levels.erase(it->second.tex);
    hd_textures.erase(it->second.tex);
    glDeleteTextures(1, &it->second.tex);
    return cache.erase(it);
}

// Tout le cache repart a vide. Les copies de l'EFB restent : leurs textures appartiennent au rendu,
// qui les remplit a chaque copie.
void TexCache::clear() {
    for (auto it = cache.begin(); it != cache.end();)
        it = drop(it);
    cache_bytes = 0;
    tex_levels.clear();
    hd_textures.clear();
    ++m_generation;
}

// Cache plein : on retire les textures du premier quart de la plage des derniers usages (les plus
// longtemps inutilisees), sans tri.
void TexCache::evict_old() {
    if (cache.empty())
        return;
    const auto [lo, hi] = std::minmax_element(cache.begin(), cache.end(), [](const auto& x, const auto& y) {
        return x.second.last_use < y.second.last_use;
    });
    const uint64_t oldest = lo->second.last_use, cutoff = oldest + (hi->second.last_use - oldest) / 4 + 1;
    size_t dropped = 0;
    for (auto it = cache.begin(); it != cache.end();) {
        if (it->second.last_use < cutoff) {
            it = drop(it);
            ++dropped;
        } else {
            ++it;
        }
    }
    evicted += dropped;
    if (dropped)
        ++m_generation;
    static int reports = 0;
    if (reports < 20) {
        ++reports;
        NJ_LOG << "[TEXCACHE] " << dropped << " textures retirees (" << evicted << " en tout, "
                 << cache.size() << " gardees)" << std::endl;
    }
}

// Empreinte de la memoire invitee a l'adresse d'une copie EFB (128 mots repartis sur la zone).
// Les copies ne sont jamais ecrites dans la memoire invitee : tant que le jeu n'y touche pas,
// l'empreinte reste celle du moment de la copie. Si elle change, le jeu a recharge autre chose a
// cette adresse (texture d'une nouvelle salle) et la copie ne doit plus la remplacer. Sans ce
// controle, une copie faite dans un menu ou une cinematique masquait pour toujours la texture
// chargee ensuite au meme endroit : sol uni, chiffres du HUD brouilles (01/10, intermittent selon
// le chemin suivi avant la salle). Dolphin fait le meme controle sur ses copies.
}  // namespace nj::runtime::hle
namespace nj::runtime {
extern MMU* g_guest_mem;
}
namespace nj::runtime::hle {
static uint64_t guest_sig(uint32_t addr, uint32_t len) {
    if (!nj::runtime::g_guest_mem || len < 4)
        return 0;
    const uint8_t* p = nj::runtime::g_guest_mem->get_span(addr, len);
    if (!p)
        return 0;
    uint64_t hsh = 1469598103934665603ull;
    const uint32_t step = len / 128 >= 4 ? (len / 128) & ~3u : 4;
    for (uint32_t off = 0; off + 4 <= len; off += step) {
        uint32_t w;
        std::memcpy(&w, p + off, sizeof w);
        hsh = (hsh ^ w) * 1099511628211ull;
    }
    return hsh;
}

void TexCache::register_efb_texture(uint32_t addr, GLuint tex, uint32_t w, uint32_t h,
                                        uint32_t gw, uint32_t gh) {
    const uint32_t a = addr & 0x01FFFFFF;
    efb_gl_sizes[tex] = ((uint64_t)gw << 32) | gh;
    efb_texs[a] = tex;
    efb_sizes[a] = ((uint64_t)w << 32) | h;
    // Zone de la plus petite copie possible (4 bits par texel), bornee a 1 Mo : une zone plus large
    // deborderait sur un tampon voisin que le jeu modifie, et perimerait une copie valable.
    const uint32_t len = std::min<uint32_t>(w * h / 2, 1u << 20);
    efb_ram_len[a] = len;
    efb_ram_sig[a] = guest_sig(addr, len);
    ++efb_copy_gen[tex];
}

GLuint TexCache::efb_slot(uint32_t addr) const {
    auto it = efb_texs.find(addr & 0x01FFFFFF);
    return it == efb_texs.end() ? 0 : it->second;
}

GLuint TexCache::find_efb_texture(uint32_t addr) {
    const uint32_t a = addr & 0x01FFFFFF;
    auto it = efb_texs.find(a);
    if (it == efb_texs.end())
        return 0;
    auto sg = efb_ram_sig.find(a);
    if (sg == efb_ram_sig.end())
        return 0;   // perimee
    static const bool s_nocheck = NJ_ENV("NJ_EFBNOSIG") != nullptr;   // ancien comportement
    if (!s_nocheck && guest_sig(addr, efb_ram_len[a]) != sg->second) {
        static int reports = 0;
        if (reports < 20) {
            ++reports;
            NJ_PRINTF("[EFB] copie perimee a %08X : memoire recrite par le jeu\n", (unsigned)addr);
        }
        efb_ram_sig.erase(sg);
        return 0;
    }
    return it->second;
}

bool TexCache::efb_dims(uint32_t addr, uint32_t& w, uint32_t& h) {
    if (!find_efb_texture(addr)) return false;
    auto it = efb_sizes.find(addr & 0x01FFFFFF);
    if (it == efb_sizes.end()) return false;
    w = (uint32_t)(it->second >> 32);
    h = (uint32_t)it->second;
    return true;
}

uint32_t TexCache::levels_of(GLuint tex) const {
    auto it = tex_levels.find(tex);
    return it == tex_levels.end() ? 1 : it->second;
}

// FNV-1a 32 bits sur des mots de 4 octets (ordre de la memoire), un mot tous les `step` octets.
constexpr uint32_t kFnvBasis = 2166136261u, kFnvPrime = 16777619u;
static uint32_t fnv_words(uint32_t h, const uint8_t* p, uint32_t n, uint32_t step) {
    for (uint32_t at = 0; at + 4 <= n; at += step) {
        uint32_t word;
        std::memcpy(&word, p + at, 4);
        h = (h ^ word) * kFnvPrime;
    }
    return h;
}

// Octets de palette lus par un format indexe (2 par couleur) : C4 16 couleurs, C8 256, C14X2 16384.
static uint32_t palette_bytes(uint32_t format) {
    switch (format) {
    case 8: return 16 * 2;
    case 9: return 256 * 2;
    case 10: return 16384 * 2;
    default: return 0;
    }
}

// Empreinte de tous les octets de la texture (mots de 8 octets) et de la palette utilisee. L'empreinte
// d'echantillon de get_texture ne regarde qu'un mot sur huit au mieux : deux textures de meme taille
// a la meme adresse qui ne different que par une zone non echantillonnee (un visage avec ou sans
// barbe, une decalcomanie) donnaient la meme cle, donc l'ancienne image (19/10, journaux MuMu : defauts
// qui disparaissent au rechargement du point de controle).
static uint64_t full_hash64(const uint8_t* p, uint32_t size, const uint8_t* pal, uint32_t pal_n) {
    uint64_t h = 0x9E3779B97F4A7C15ull ^ size;
    auto mix = [&h](uint64_t w) {
        h = (h ^ w) * 0xFF51AFD7ED558CCDull;
        h ^= h >> 32;
    };
    uint32_t at = 0;
    for (; at + 8 <= size; at += 8) {
        uint64_t w;
        std::memcpy(&w, p + at, 8);
        mix(w);
    }
    for (; at < size; ++at)
        mix(p[at]);
    for (uint32_t k = 0; k < pal_n; ++k)
        mix(pal[k]);
    return h;
}

// Pages de 64 Ko sur les 64 Mo adressables (adresse & 0x03FFFFFF).
constexpr uint32_t kPageShift = 16, kPageCount = 0x04000000u >> kPageShift;

void TexCache::guest_texels_written(uint32_t addr, uint32_t len) {
    if (len == 0)
        return;
    if (page_seq.empty())
        page_seq.assign(kPageCount, 0);
    ++write_seq;
    const uint32_t a = addr & 0x03FFFFFFu;
    for (uint32_t pg = a >> kPageShift, end = (a + len - 1) >> kPageShift; pg <= end && pg < kPageCount; ++pg)
        page_seq[pg] = write_seq;
}

// Empreinte d'echantillon des texels (celle de la cle du cache, calcul inchange). Moteur natif : la
// memoire des textures n'est ecrite que par l'adaptateur d'images, qui le signale
// (guest_texels_written) ; tant qu'aucune ecriture n'a touche les pages de la texture, l'empreinte
// deja calculee est reprise. Avant, elle etait refaite a chaque liaison (jusqu'a 4096 mots lus,
// plusieurs centaines de liaisons par image). Sans signalement (chemin emule) : calcul a chaque appel.
uint64_t g_tex_frame = 1;   // rang de l'image en cours de rendu (draw_frame)

uint32_t TexCache::sample_hash(uint32_t addr, const uint8_t* p, uint32_t size) {
    const uint32_t step = size > 32 * 4096 ? size / 4096 : 32;
    if (size == 0)
        return fnv_words(kFnvBasis, p, size, step);
    static const bool memo_off = [] { const char* e = NJ_ENV("NJ_TEXMEMO"); return e && e[0] == '0'; }();   // A/B
    if (write_seq == 0 && (!gx::g_native_arrays || memo_off))   // chemin emule : calcul a chaque appel (inchange)
        return fnv_words(kFnvBasis, p, size, step);
    if (write_seq == 0) {
        // Coeur natif (port re4-decomp, 14/11) : rien ne signale les ecritures. L'empreinte est reprise dans la meme
        // image seulement (g_tex_frame, avance par draw_frame) : une texture liee plusieurs fois n'est lue qu'une fois
        // par image (~433 liaisons par image en r100). Une texture recrite par le jeu est vue a l'image suivante.
        if (sample_memo.size() > 16384)
            sample_memo.clear();
        const auto [it, fresh] = sample_memo.try_emplace((uint64_t)(addr & 0x03FFFFFFu) << 32 | size);
        if (fresh || it->second.seq != g_tex_frame) {
            it->second.hash = fnv_words(kFnvBasis, p, size, step);
            it->second.seq = g_tex_frame;
        }
        return it->second.hash;
    }
    const uint32_t a = addr & 0x03FFFFFFu;
    uint64_t last = 0;
    for (uint32_t pg = a >> kPageShift, end = (a + size - 1) >> kPageShift; pg <= end && pg < kPageCount; ++pg)
        last = std::max(last, page_seq[pg]);
    if (sample_memo.size() > 16384)
        sample_memo.clear();
    const auto [it, fresh] = sample_memo.try_emplace((uint64_t)a << 32 | size);
    if (fresh || it->second.seq < last) {
        it->second.hash = fnv_words(kFnvBasis, p, size, step);
        it->second.seq = write_seq;
    }
    return it->second.hash;
}

GLuint TexCache::get_texture(GuestCpu& ctx, const gx::TexUnit& stage,
                                 uint32_t mip_levels) {
    if (mip_levels < 1)
        mip_levels = 1;
    if (GLuint efb = find_efb_texture(stage.data_addr)) {
        if (stage.format == 8 || stage.format == 9)   // C4, C8 : a travers la palette
            return efb_paletted(efb, stage);
        return efb;
    }
    // Empreinte : un echantillon des texels (un mot tous les size / 4096 octets, au moins tous les
    // 32), puis toute la palette lue par un format indexe. Les textures HD sont retrouvees par elle :
    // le calcul ne doit pas changer.
    const uint32_t size = gx::tex_bytes(stage.width, stage.height, stage.format);
    uint32_t hash = kFnvBasis;
    const uint8_t* tex_p = ctx.mmu.get_span(stage.data_addr, size);
    if (tex_p)
        hash = sample_hash(stage.data_addr, tex_p, size);
    const auto& tlut = gx::g_gxregs.tlut.mem;
    const uint8_t* full_pal = nullptr;
    uint32_t full_pal_n = 0;
    if (stage.tlut_addr < sizeof tlut) {
        const uint8_t* pal = tlut + stage.tlut_addr;
        const uint32_t n = std::min<uint32_t>(palette_bytes(stage.format), (uint32_t)sizeof tlut - stage.tlut_addr);
        full_pal = pal;
        full_pal_n = n;
        hash = fnv_words(hash, pal, n, 4);
        for (uint32_t k = n & ~3u; k < n; ++k)   // octets de queue, un par un
            hash = (hash ^ pal[k]) * kFnvPrime;
    }

    // Bascule du menu F10 : les textures deja envoyees sont rechargees dans l'autre version.
    // NJ_HDTOGGLE=<s> (essais) : bascule g_hd_textures une fois, s secondes apres le premier appel,
    // comme un clic dans le menu F10 en pleine partie.
    static const char* s_toggle_env = NJ_ENV("NJ_HDTOGGLE");
    if (s_toggle_env) {
        static const auto t0 = std::chrono::steady_clock::now();
        static bool done;
        if (!done && std::chrono::steady_clock::now() - t0 > std::chrono::seconds(std::atoi(s_toggle_env))) {
            done = true;
            gfx::g_hd_textures ^= 1;
            NJ_PRINTF("[TexturesHD] bascule -> %d\n", gfx::g_hd_textures);
        }
    }
    static int s_hd_seen = -1;
    const int hd_now = gfx::g_hd_textures * 128 + gfx::g_hd_cutscenes * 64 + gfx::g_hd_interface * 32 +
                       gfx::g_hd_environment * 16 + gfx::g_hd_items * 8 + gfx::g_hd_weapons * 4 +
                       gfx::g_hd_case * 2 + gfx::g_hd_hud + gfx::g_button_style * 256 +
                       gfx::g_controller_layout * 4096;
    if (s_hd_seen != hd_now) {
        if (s_hd_seen >= 0)
            clear();
        s_hd_seen = hd_now;
    }
    // mods actives, desactives, deplaces ou installes : nouvel index, textures rechargees
    static uint32_t s_mods_gen = 0;
    if (const uint32_t g = nj::mods::generation(); g != s_mods_gen) {
        s_mods_gen = g;
        s_ix = nj::mods::index();
        clear();
    }

    TexKey key = {stage.data_addr, stage.width, stage.height,
                      stage.format, stage.tlut_addr, stage.tlut_fmt, hash,
                      mip_levels};
    ++use_tick;
    auto it = cache.find(key);
    if (it != std::end(cache)) {
        // Verification par le contenu entier, sur quelques entrees par image (la cle ne regarde qu'un
        // echantillon) : si la memoire a change ailleurs que dans l'echantillon, la texture est refaite.
        static const bool s_noverify = NJ_ENV("NJ_TEXNOVERIFY") != nullptr;
        static uint64_t s_vframe = ~0ull;
        static int s_vbudget = 0;
        // F12 (g_tex_verify_all) : pendant 3 images, TOUTES les textures du cache utilisees sont verifiees par
        // le contenu entier, et chaque texture perimee est journalisee (adresse, taille, format, palette).
        static int s_all_left = 0, s_all_checked = 0, s_all_stale = 0;
        if (s_vframe != g_st_frames) {
            if (s_all_left > 0 && --s_all_left == 0)
                NJ_PRINTF("[TexVerif] fin : %d textures verifiees, %d perimees\n", s_all_checked, s_all_stale);
            s_vframe = g_st_frames;
            s_vbudget = 6;
            if (g_tex_verify_all.exchange(0)) {
                s_all_left = 3;
                s_all_checked = s_all_stale = 0;
                NJ_PRINTF("[TexVerif] debut : verification complete des textures utilisees\n");
            }
        }
        const bool all = s_all_left > 0;
        if (!s_noverify && tex_p && it->second.full && (all || (s_vbudget > 0 && (use_tick & 3) == 0))) {
            if (!all) --s_vbudget;
            else ++s_all_checked;
            if (full_hash64(tex_p, size, full_pal, full_pal_n) != it->second.full) {
                nj::perf::g_tex_stale.fetch_add(1, std::memory_order_relaxed);
                static int s_logged = 0;
                if (all) {
                    ++s_all_stale;
                    NJ_PRINTF("[TexVerif] PERIMEE %ux%u fmt=%u adr=%08x tlut=%08x/%u mips=%u\n", stage.width,
                              stage.height, stage.format, stage.data_addr, stage.tlut_addr, stage.tlut_fmt, mip_levels);
                }
                if (s_logged < 40) {
                    ++s_logged;
                    char b[120];
                    std::snprintf(b, sizeof b, "texture perimee %ux%u fmt=%u adr=%08x"_g, stage.width, stage.height,
                                  stage.format, stage.data_addr);
                    nj::perf::event(b);
                }
                drop(it);
                it = std::end(cache);
            }
        }
    }
    if (it != std::end(cache)) {
        it->second.last_use = use_tick;
        if (!it->second.hd_path.empty())
            promote_hd(it->second);
        return it->second.tex;
    }

    // The cache used to be cleared outright past 512 entries. A village scene
    // in RE4 uses more textures than that, so every frame threw the whole
    // cache away and decoded and uploaded everything again. Keep more and
    // retire only the entries that have gone unused longest.
    static constexpr size_t kMaxCachedTextures = 2048;
    for (int pass = 0; pass < 4 && !cache.empty() && (cache.size() >= kMaxCachedTextures || cache_bytes > cache_budget_bytes()); ++pass)
        evict_old();   // nombre OU octets (26/10)

    // Upload the guest mip chain: GX stores each smaller level right after
    // the previous one. Filtering and wrap are set per bind by the renderer
    // from the texture stage's TexMode registers (this used to force
    // GL_NEAREST on everything, which pixelated every surface).
    // Le televersement se fait sur une unite de travail que les nuanceurs
    // n echantillonnent jamais (ils n utilisent que 0 a 7). Le renderer lie
    // les etages dans l ordre et appelle get_texture avant son propre
    // glActiveTexture : l unite encore active est celle de l etage
    // precedent, et un glBindTexture a cet endroit remplacait sa texture par
    // la nouvelle. Mesure a l oracle FIFO sur la manche de Leon (dessin
    // 13087) : l unite 0 echantillonnait la texture de l etage 1, la 1 celle
    // de l etage 2, et ainsi de suite, a chaque texture creee ou recreee.
    GLint prev_unit = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
    nj_unit(15);
    GLuint tex;
    tex = nj_new_tex();
    nj_tex2d(tex);
    std::string hd_wait;   // PNG HD demande, pas encore pret : texture provisoire

    // Version HD de cette texture : empreinte du niveau 0, plus la palette utilisee pour C4 / C8.
    if ((gfx::g_hd_textures || gfx::g_hd_cutscenes || gfx::g_hd_interface || gfx::g_hd_environment ||
         gfx::g_hd_items || gfx::g_hd_weapons || gfx::g_hd_case || gfx::g_hd_hud || gfx::g_button_style) &&
        stage.format != 10 && (hd_any() || button_dir()) &&   // style sans fichiers : pas d'empreinte (14/10)
        shape_known(stage.width, stage.height, stage.format)) {
        // L'empreinte ne couvre que les octets de l'image : blocs de 8x4 pour les formats 8 bits (I8,
        // IA4, C8). Avec les 8x8 de texture_data_size, une hauteur non multiple de 8 ajoutait les octets
        // qui suivent la texture, autres en jeu que dans le fichier (anneau du HUD 143x90, 30/09).
        const uint32_t fm = stage.format;
        const uint32_t hsize = (fm == 1 || fm == 2 || fm == 9)
                                   ? ((stage.width + 7) / 8) * ((stage.height + 3) / 4) * 32
                                   : size;
        if (const uint8_t* p = ctx.mmu.get_span(stage.data_addr, hsize)) {
            const uint8_t* pal = nullptr;
            uint32_t pal_bytes = 0;
            if (stage.format == 8 || stage.format == 9) {
                uint32_t maxi = 0;
                for (uint32_t i = 0; i < hsize; ++i) {
                    const uint32_t a = stage.format == 8 ? (uint32_t)(p[i] >> 4) > (uint32_t)(p[i] & 15)
                                                               ? (uint32_t)(p[i] >> 4) : (uint32_t)(p[i] & 15)
                                                         : p[i];
                    if (a > maxi) maxi = a;
                }
                pal_bytes = (maxi + 1) * 2;
                if (stage.tlut_addr + pal_bytes <= sizeof(g_gxregs.tlut.mem))
                    pal = g_gxregs.tlut.mem + stage.tlut_addr;
                else
                    pal_bytes = 0;
            }
            const std::string k = hd_key(p, hsize, stage.width, stage.height, stage.format, pal, pal_bytes);
            // Chaque cle est dans une seule liste (hdtex_build.py --categories) ; sans liste :
            // personnages et ennemis, option « Characters ».
            const bool wanted = in_list("weapon", k)      ? gfx::g_hd_weapons != 0
                                : in_list("case", k)        ? gfx::g_hd_case != 0
                                : in_list("hud", k)         ? gfx::g_hd_hud != 0
                                : in_list("item", k)        ? gfx::g_hd_items != 0
                                : in_list("interface", k)   ? gfx::g_hd_interface != 0
                                : in_list("cutscene", k)    ? gfx::g_hd_cutscenes != 0
                                : in_list("room", k)        ? gfx::g_hd_environment != 0
                                                            : gfx::g_hd_textures != 0;
            // NJ_HDMISS=1 : chaque empreinte sans HD, une fois (textures qu'on croit couvertes).
            static const bool s_miss = NJ_ENV("NJ_HDMISS") != nullptr;
            if (s_miss && !hd_has(k)) {
                static std::unordered_set<std::string> seen;
                if (seen.insert(k).second) {
                    NJ_PRINTF("[TexturesHD] -%s (pal %u)\n", k.c_str(), pal_bytes);
                    if (NJ_ENV("NJ_HDDUMP")) {       // octets bruts + palette, pour l'outil
                        std::ofstream o("hdmiss_" + k + ".bin", std::ios::binary);
                        o.write((const char*)p, hsize);
                        if (pal)
                            o.write((const char*)pal, pal_bytes);
                    }
                }
            }
            // Boutons au choix (F10 « Button prompts ») : prioritaires sur le reste.
            const std::string* btn = button_file(k);
            if ((btn || (wanted && hd_has(k)))) {
                const std::string path = btn ? *btn : ix().textures.at(k);
                static const bool s_sync = NJ_ENV("NJ_HDSYNC") != nullptr;
                HdDecoded d;
                bool have = false;
                if (s_sync || btn) {   // boutons : petits fichiers, pas d'image GameCube en attendant
                    d = HdAsync::decode(path);
                    have = true;
                } else if (!(have = HdAsync::get().take(path, d))) {
                    HdAsync::get().request(path);
                    hd_wait = path;   // version GameCube en attendant (plus bas)
                }
                if (have && d.px) {
                    const uint32_t lv = upload_hd(d);
                    glActiveTexture((GLenum)prev_unit);
                    tex_levels[tex] = lv;
                    hd_textures.insert(tex);
                    // NJ_HDSTAT=1 : chaque texture HD chargee, par format (verification des empreintes).
                    static const bool s_stat = NJ_ENV("NJ_HDSTAT") != nullptr;
                    if (s_stat) {
                        static unsigned by_fmt[16] = {};
                        ++by_fmt[stage.format & 15];
                        NJ_PRINTF("[TexturesHD] +%s (fmt %u : %u)\n", k.c_str(), stage.format, by_fmt[stage.format & 15]);
                    }
                    cache[key] = CachedTexture{tex, use_tick, {}, 0,
                                               tex_p ? full_hash64(tex_p, size, full_pal, full_pal_n) : 0};
                    return tex;
                }
            }
        }
    }
    gx::TexUnit level = stage;
    uint32_t uploaded = 0;
    uint64_t gpu_bytes = 0;
    for (uint32_t i = 0; i < mip_levels; ++i) {
        std::vector<gx::Texel> px;
        if (!gx::decode_tex(ctx.mmu, level, gx::g_gxregs.tlut.mem, sizeof gx::g_gxregs.tlut.mem, px))
            NJ_LOG << "[TexCache] format de texture inconnu : " << (int)level.format << std::endl;
        glTexImage2D(GL_TEXTURE_2D, (GLint)i, GL_RGBA, (GLsizei)level.width,
                     (GLsizei)level.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        gpu_bytes += (uint64_t)level.width * level.height * 4;
        ++uploaded;
        if (level.width == 1 && level.height == 1)
            break;
        level.data_addr += gx::tex_bytes(level.width, level.height, level.format);
        level.width = level.width > 1 ? level.width / 2 : 1;
        level.height = level.height > 1 ? level.height / 2 : 1;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)uploaded - 1);
    nj_tex_filter(GL_LINEAR);

    glActiveTexture((GLenum)prev_unit);

    tex_levels[tex] = uploaded;
    cache[key] = CachedTexture{tex, use_tick, std::move(hd_wait), 0,
                               tex_p ? full_hash64(tex_p, size, full_pal, full_pal_n) : 0, (uint32_t)std::min<uint64_t>(gpu_bytes, 0xFFFFFFFFull)};
    cache_bytes += (uint32_t)std::min<uint64_t>(gpu_bytes, 0xFFFFFFFFull);
    return tex;
}

}

namespace nj::runtime::hle {

GLuint TexCache::efb_paletted(GLuint efb, const gx::TexUnit& stage) {
    using gx::g_gxregs;
    const uint32_t entries = stage.format == 8 ? 16u : 256u;
    // Cle de palette : decalage, format et contenu de la TLUT utilisee.
    uint32_t key = 2166136261u;
    key = (key ^ stage.tlut_addr) * 16777619u;
    key = (key ^ stage.tlut_fmt) * 16777619u;
    for (uint32_t i = 0; i < entries * 2; ++i) {
        const uint32_t off = stage.tlut_addr + i;
        const uint8_t b = off < sizeof(g_gxregs.tlut.mem) ? g_gxregs.tlut.mem[off] : 0;
        key = (key ^ b) * 16777619u;
    }
    const uint32_t gen = efb_copy_gen[efb];
    PalettedEfb& pe = efb_paletted_cache[efb];
    if (pe.tex && pe.gen == gen && pe.key == key)
        return pe.tex;

    // Relire la copie (taille : niveau 0 de la texture) et l'aiguillage de canal qui donne l'indice.
    GLint prev_fb = 0, prev_tex = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fb);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    nj_tex2d(efb);
    GLint w = 0, h = 0, sw = GL_RED;
    {
        auto it = efb_gl_sizes.find(efb);
        if (it != efb_gl_sizes.end()) {
            w = (GLint)(it->second >> 32);
            h = (GLint)(uint32_t)it->second;
        }
    }
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, &sw);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
        glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
        return efb;
    }
    static GLuint s_fbo = 0;
    if (!s_fbo)
        glGenFramebuffers(1, &s_fbo);
    nj_fbo(GL_FRAMEBUFFER, s_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, efb, 0);
    std::vector<uint8_t> src((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, src.data());
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fb);
    const int ch = sw == GL_GREEN ? 1 : sw == GL_BLUE ? 2 : sw == GL_ALPHA ? 3 : 0;

    auto lookup = [&](uint32_t index) -> gx::Texel {
        const uint32_t off = stage.tlut_addr + index * 2;
        if (off + 1 >= sizeof(g_gxregs.tlut.mem))
            return gx::Texel{255, 0, 255, 255};
        return gx::palette_entry((uint16_t)((g_gxregs.tlut.mem[off] << 8) | g_gxregs.tlut.mem[off + 1]),
                                 stage.tlut_fmt);
    };
    std::vector<gx::Texel> out((size_t)w * h);
    for (size_t i = 0; i < out.size(); ++i) {
        const uint8_t v = src[i * 4 + ch];
        out[i] = lookup(stage.format == 8 ? (uint32_t)(v >> 4) : (uint32_t)v);
    }
    if (!pe.tex)
        glGenTextures(1, &pe.tex);
    glBindTexture(GL_TEXTURE_2D, pe.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    nj_tex_filter(GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
    pe.gen = gen;
    pe.key = key;
    return pe.tex;
}

}  // namespace nj::runtime::hle
