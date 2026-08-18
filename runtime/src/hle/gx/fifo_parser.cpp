#include <atomic>
#include "runtime/log.h"
#include "runtime/dev_tour.h"
extern std::atomic<uint64_t> g_fl_drawdones;   // gx.cpp, NJ_FRAMELOG
extern std::atomic<uint64_t> g_fl_xfb_copies;  // gx.cpp, [GXPROF] (images du jeu, sans rendu)
#include <chrono>
#include <cstdlib>
#include "runtime/env.h"
#include <iostream>
#include <vector>
#include "runtime/gx/fifo_parser.h"
#include "runtime/gx/efb_peek.h"
#include "runtime/gx/fifo_record.h"
#include "runtime/gx_state.h"
#include "runtime/cpu_context.h"
#include "runtime/hw/hw.h"
#include "runtime/config.h"
#include "runtime/gfx_settings.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <map>
#include <shared_mutex>

// Verrou lecteurs/redacteur des caches de decodage, pris a chaque dessin. Sous MinGW,
// std::shared_mutex passe par les rwlock de libwinpthread (mutex + conditions) : ~5 % du fil de
// lecture au profil du 01/10. Sous Windows on prend le SRWLOCK natif (un mot, sans appel systeme
// hors contention) ; ailleurs std::shared_mutex. Meme interface : shared_lock / unique_lock.
#ifdef _WIN32
extern "C" {
__declspec(dllimport) void __stdcall AcquireSRWLockShared(void**);
__declspec(dllimport) void __stdcall ReleaseSRWLockShared(void**);
__declspec(dllimport) void __stdcall AcquireSRWLockExclusive(void**);
__declspec(dllimport) void __stdcall ReleaseSRWLockExclusive(void**);
}
class NjSharedMutex {
public:
    void lock() { AcquireSRWLockExclusive(&m_); }
    void unlock() { ReleaseSRWLockExclusive(&m_); }
    void lock_shared() { AcquireSRWLockShared(&m_); }
    void unlock_shared() { ReleaseSRWLockShared(&m_); }
private:
    void* m_ = nullptr;   // SRWLOCK_INIT
};
#else
using NjSharedMutex = std::shared_mutex;
#endif
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <algorithm>
#include <deque>

namespace nj::runtime {
    extern MMU* g_guest_mem;
}

namespace nj::runtime::gx {
// Coeur natif (port/njgfx) : tableaux de sommets et matrices en memoire dans l'ordre de la machine ;
// listes d'affichage et flux restent gros-boutistes (comme le disque et le materiel).
bool g_native_arrays = false;
static float xf_array_f32(uint32_t addr) {
    if (!g_native_arrays) return nj::runtime::g_guest_mem->read_f32(addr);
    const uint8_t* q = nj::runtime::g_guest_mem->get_span(addr, 4);
    float f = 0.0f;
    if (q) std::memcpy(&f, q, 4);
    return f;
}

// copies EFB vers une texture (BP 0x52 sans le bit XFB) : releve des videos (main.cpp, 13/10)
std::atomic<uint64_t> g_tex_copies{0};

extern GxRegs g_gxregs;

// Etat de la lecture du FIFO (registres CP, BP et XF vus a l'analyse), distinct de celui du rendu
// (g_gxregs) : la lecture tourne sur son propre fil (gx.cpp, fil de lecture) pendant que le rendu
// de l'image precedente avance. Les fonctions de lecture ci-dessous le prennent sous le nom local
// g_gxregs (reference qui masque le global). Consequence : plus de pre-remplissage de l'etat du
// rendu par la lecture (NJ_NOSEED) - oracles FIFO identiques au pixel pres avec ou sans (29/09).
static GxRegs g_pstate;
static bool s_list_state_dirty = true;
static uint64_t s_cached_list_state = 0;

void xfstat_seq(const float* m);   // mesure, definie plus bas
bool xfgpu_on();                   // transformation sur le nuanceur, definie plus bas
// Reglages en direct (knobs.txt) : coupes dans le coeur natif ; evite l'appel a knob() par liste et par dessin (V0).
static inline bool knobs_live() {
    static const bool v = !(NJ_ENV("NJ_NATIVE") != nullptr && NJ_ENV("NJ_KNOBS") == nullptr);
    return v;
}

// Ombre de la memoire XF des matrices, tenue a l'analyse : chaque chargement (direct 0x10 ou par
// index 0x20-0x38) y est ecrit a sa place dans le flux, et un dessin y prend la matrice valable a
// sa position, avant que le rendu n'applique les chargements suivants au meme emplacement.
// P3 : ombre complete de l'espace XF (0x0000-0x10FF) tenue a l'analyse : une ecriture XF identique a ce qui est deja
// pose n'est pas transmise (le jeu renvoie tout son materiau a chaque partie : ~8 000 ecritures inutiles par image au
// chateau, autant de commandes creees puis detruites). Valeur connue seulement apres une ecriture vue ici ; les
// rejeux de listes en cache mettent l'ombre a jour. NJ_XFKEEP=1 : tout transmettre (A/B).
namespace xfshadow {
static uint32_t v[0x1100];
static uint8_t ok[0x1100];
uint64_t skipped = 0;
int in_list = 0;   // > 0 pendant l'analyse d'une liste d'affichage : rien n'est filtre (la liste est gardee en cache
                   // et rejouee plus tard dans un autre etat : une ecriture retiree lui manquerait)
static bool keep() { static const bool k = NJ_ENV("NJ_XFKEEP") != nullptr; return k; }
template <class P> static bool same(uint32_t reg, const P& p, size_t n) {
    if (keep() || n == 0 || in_list > 0) return false;
    for (size_t k = 0; k < n; ++k) {
        const uint32_t a = reg + (uint32_t)k;
        uint32_t w;
        std::memcpy(&w, &p[k], 4);
        if (a >= 0x1100 || !ok[a] || v[a] != w) return false;
    }
    return true;
}
template <class P> static void set(uint32_t reg, const P& p, size_t n) {
    for (size_t k = 0; k < n; ++k) {
        const uint32_t a = reg + (uint32_t)k;
        if (a >= 0x1100) break;
        std::memcpy(&v[a], &p[k], 4);
        ok[a] = 1;
    }
}
}  // namespace xfshadow
// GX natif (re4-decomp-upstream port/gxn.cpp, NJ_GXN=1) : etat vu a l'analyse, oracle du GX natif. Registres CP et BP
// tels que decodes, et ombre de l'espace XF (valeur, connue ou non).
void OracleRegs(const uint32_t** cp, const uint32_t** bp, const uint32_t** xf, const uint8_t** xfok) {
    *cp = g_pstate.cp.data();
    *bp = g_pstate.bp.data();
    *xf = xfshadow::v;
    *xfok = xfshadow::ok;
}
static float s_shadow_pos[256] = {0};   // positions (XF 0x000-0x0FF)
static float s_shadow_nrm[96] = {0};    // normales (XF 0x400-0x45F)

// Photo partagee de l'ombre, refaite seulement apres un chargement de matrice.
static std::shared_ptr<const MatrixBank> s_bank_photo;
static std::shared_ptr<const MatrixBank> current_bank() {
    if (!s_bank_photo) {
        auto b = std::make_shared<MatrixBank>();
        std::memcpy(b->pos, s_shadow_pos, sizeof b->pos);
        std::memcpy(b->nrm, s_shadow_nrm, sizeof b->nrm);
        s_bank_photo = std::move(b);
    }
    return s_bank_photo;
}

static void shadow_matrix_write(uint32_t xf_addr, const PayloadVec& words) {
    s_bank_photo.reset();   // la prochaine photo reprendra l'ombre modifiee
    uint32_t a = xf_addr;
    for (float w : words) {
        if (a < 256)
            s_shadow_pos[a] = w;
        else if (a >= 0x400 && a < 0x400 + 96)
            s_shadow_nrm[a - 0x400] = w;
        ++a;
    }
}

// PE finish/token signals release a game blocked in GXDrawDone or waiting on
// a draw-sync token. They are queued while a batch is parsed and raised by
// gx_pump_fifo only after that batch's draws were decoded, so the game
// cannot overwrite vertex buffers those draws still have to read.
enum class PeSignal { Finish, Token, TokenInt };
static std::vector<std::pair<PeSignal, uint32_t>> g_pe_signals;

static void QueuePeSignal(PeSignal kind, uint32_t token) {
    // NJ_PETRACE : voir hw_pe.cpp (meme horloge, meme modulo).
    if (kind == PeSignal::Finish && nj::runtime::hw::pe_tracing()) {
        static int n = 0;
        if (n++ < 200)
            NJ_LOG << "[PEtrace] " << std::dec
                      << (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now().time_since_epoch()).count() % 100000000ull
                      << " parsed-drawdone\n";
    }
    // coeur natif : la fin de dessin et les jetons sont delivres par le portage (port/gx_sdk.cpp) ;
    // personne ne vide cette file (FlushDeferredPeSignals) : elle grossissait jusqu'a la panne (nj12)
    if (g_native_arrays) return;
    g_pe_signals.push_back({kind, token});
}

void FlushDeferredPeSignals() {
    for (const auto& s : g_pe_signals) {
        if (s.first == PeSignal::Finish) {
            g_fl_drawdones.fetch_add(1, std::memory_order_relaxed);
            nj::runtime::hw::pe_finish();
        }
        else
            nj::runtime::hw::pe_token(s.second, s.first == PeSignal::TokenInt);
    }
    g_pe_signals.clear();
}

// Guest vertex buffers rewritten between draws. RE4 skins every character on
// the CPU into one 0x38000-byte scratch buffer (AllocDrawTmpBuf) and flushes
// it with DCStoreRangeNoSync before drawing; the next model overwrites it long
// before we decode. The DC seams copy the flushed bytes tagged with the FIFO
// position of the flush; the parser activates each copy when the stream
// reaches that position, and indexed vertex reads prefer it over live RAM.
static uint64_t g_stream_base = 0;  // absolute FIFO position of fifo[0]
static std::deque<std::pair<uint64_t, VolatileRange>> g_volatile_queue;
static std::vector<VolatileRange> g_volatile_active;
// Enveloppe des zones actives [lo, hi) : rejette d'un coup les tableaux qui tombent hors de toutes
// (presque tous). CollectVolatile comparait chaque tableau de chaque dessin a chaque zone : jusqu'a
// 12 x 64 comparaisons par dessin, ~17 800 dessins par image (~5 % du fil hote au Galaxy S5).
static uint64_t g_volatile_lo = ~0ull, g_volatile_hi = 0;
static std::vector<VolatileRange> g_volatile_sorted;
// Table directe par granule de 256 octets de [lo, hi) : nombre de zones triees qui commencent au
// plus a la fin du granule. CollectVolatile en deduit le resultat de upper_bound en reculant sur
// les quelques zones qui commencent apres l'adresse, au lieu d'une dichotomie par tableau de chaque
// dessin (3,9 % du fil de lecture au profil du S5, bc3). Vide si [lo, hi) depasse 16 Mo.
static std::vector<uint8_t> g_volatile_gran;
static void RebuildVolatileGran() {
    g_volatile_gran.clear();
    if (g_volatile_hi <= g_volatile_lo) return;
    const uint64_t n = (g_volatile_hi - g_volatile_lo + 255) >> 8;
    if (n > (1u << 16) || g_volatile_sorted.size() > 255) return;
    g_volatile_gran.resize((size_t)n);
    size_t k = 0;
    for (uint64_t g = 0; g < n; ++g) {
        const uint64_t end = g_volatile_lo + (g << 8) + 255;
        while (k < g_volatile_sorted.size() && g_volatile_sorted[k].addr <= end) ++k;
        g_volatile_gran[(size_t)g] = (uint8_t)k;
    }
}
static void RecomputeVolatileBounds() {
    g_volatile_lo = ~0ull;
    g_volatile_hi = 0;
    for (const auto& v : g_volatile_active) {
        if (v.addr < g_volatile_lo) g_volatile_lo = v.addr;
        if ((uint64_t)v.addr + v.len > g_volatile_hi) g_volatile_hi = (uint64_t)v.addr + v.len;
    }
}

void SetParseStreamBase(uint64_t base) { g_stream_base = base; }

void QueueVolatileRange(uint64_t pos, uint32_t addr, uint32_t len,
                        std::shared_ptr<const std::vector<uint8_t>> bytes) {
    g_volatile_queue.push_back(
        {pos, VolatileRange{addr & 0x01FFFFFFu, len, std::move(bytes)}});
}

static void ActivateVolatile(uint64_t pos) {
    while (!g_volatile_queue.empty() && g_volatile_queue.front().first <= pos) {
        VolatileRange r = std::move(g_volatile_queue.front().second);
        g_volatile_queue.pop_front();
        if (r.len == 0 && !r.bytes) {   // marqueur « tout effacer » (entree dans une salle sans copies volatiles, gx.cpp)
            g_volatile_active.clear();
            g_volatile_sorted.clear();
            RecomputeVolatileBounds();
            RebuildVolatileGran();
            continue;
        }
        if (nj::runtime::gx::rec::active() && r.bytes)
            nj::runtime::gx::rec::on_volatile(r.addr, r.bytes->data(), r.len);   // enregistreur .dff : copie a sa position
        const uint64_t end = (uint64_t)r.addr + r.len;
        static uint64_t n_act = 0, n_overlap = 0, n_cap = 0, n_partial = 0;
        // Recouvrement : la partie d'une ancienne copie que la nouvelle ne reecrit pas RESTE active (decoupee). Avant
        // (28/10), l'ancienne copie etait effacee en entier : ses sommets non reecrits etaient alors lus en memoire
        // vivante, deja reecrite par les modeles suivants (r100 : 7 000 a 8 000 effacements partiels pour 20 000
        // activations, contre 5 500 a 6 000 dans r101). NJ_VOLTRIM=0 : ancien comportement.
        static const bool trim = NJ_ENV("NJ_VOLTRIM") == nullptr || NJ_ENV("NJ_VOLTRIM")[0] != '0';
        std::vector<VolatileRange> keep;
        keep.reserve(g_volatile_active.size() + 2);
        for (VolatileRange& v : g_volatile_active) {
            const uint64_t vend = (uint64_t)v.addr + v.len;
            if (!(v.addr < end && r.addr < vend)) {
                keep.push_back(std::move(v));
                continue;
            }
            ++n_overlap;
            const bool left = v.addr < r.addr, right = vend > end;
            if (left || right) ++n_partial;
            if (!trim || !v.bytes) continue;
            if (left) {
                const uint32_t l = r.addr - v.addr;
                keep.push_back({v.addr, l, std::make_shared<const std::vector<uint8_t>>(v.bytes->begin(), v.bytes->begin() + l)});
            }
            if (right) {
                const uint32_t off = (uint32_t)(end - v.addr);
                keep.push_back({(uint32_t)end, (uint32_t)(vend - end),
                                std::make_shared<const std::vector<uint8_t>>(v.bytes->begin() + off, v.bytes->end())});
            }
        }
        g_volatile_active.swap(keep);
        g_volatile_active.push_back(std::move(r));
        if (g_volatile_active.size() > 192) {
            g_volatile_active.erase(g_volatile_active.begin());
            ++n_cap;
        }
        if (++n_act % 20000 == 0) {
            NJ_LOG << "[VolActive] " << n_act << " activees : " << n_overlap << " effacees par recouvrement (dont " << n_partial
                   << " partiel), " << n_cap << " ejectees au-dela de 64" << std::endl;
            n_overlap = n_cap = n_partial = 0;
        }
        RecomputeVolatileBounds();
        // Copie triee par adresse (les zones actives ne se recouvrent pas) : recherche
        // dichotomique dans CollectVolatile au lieu d'un parcours complet par tableau.
        g_volatile_sorted = g_volatile_active;
        std::sort(g_volatile_sorted.begin(), g_volatile_sorted.end(),
                  [](const VolatileRange& a, const VolatileRange& b) { return a.addr < b.addr; });
        RebuildVolatileGran();
    }
}

// Copies actives qui contiennent la base d'un tableau indexe de ce dessin (toutes les zones
// qu'un index pourrait atteindre : ~3 000 dessins de decor par image de trop). Elles ne se
// recouvrent pas : l'ordre ne compte pas.
// Pages (4 Ko) ou la memoire vivante a change sous un dessin (early_drawdone) : pour le journal,
// qui dit ainsi QUEL tampon du jeu est reecrit avant le decodage.
static std::mutex s_race_mu;
static std::map<uint32_t, std::pair<uint32_t, uint32_t>> s_race_pages;   // page -> (dessins, sommets)
static void NoteRacePage(uint32_t base, uint32_t verts) {
    std::lock_guard<std::mutex> lk(s_race_mu);
    auto& e = s_race_pages[base >> 12];
    ++e.first;
    e.second += verts;
}
std::string TakeRacePages() {
    std::lock_guard<std::mutex> lk(s_race_mu);
    std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>> v(s_race_pages.begin(), s_race_pages.end());
    s_race_pages.clear();
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
    std::string out;
    char b[48];
    for (size_t i = 0; i < v.size() && i < 4; ++i) {
        std::snprintf(b, sizeof b, "%s%x:%u/%u", i ? "," : "", v[i].first << 12, v[i].second.first, v[i].second.second);
        out += b;
    }
    return out;
}

// Empreinte des 32 premiers octets du tableau de positions indexe, lu en memoire vivante (sans
// copie videe) : early_drawdone compare l'analyse et le decodage. 0 = rien a surveiller.
static uint64_t LiveFingerprint(const RawDraw& r) {
    if (!r.overlays.empty() || (r.vat.pos.mode != AttrMode::Idx8 && r.vat.pos.mode != AttrMode::Idx16) ||
        !nj::runtime::g_guest_mem)
        return 0;
    const auto& m = nj::runtime::g_guest_mem->mem1;
    const uint32_t a = r.arr.base[0] & 0x01FFFFFFu;
    if (m.size() < 32 || a > m.size() - 32) return 0;
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint32_t i = 0; i < 32; ++i) h = (h ^ m[a + i]) * 0x100000001B3ull;
    return h | 1;
}

static void CollectVolatile(const RawDraw& r, std::vector<VolatileRange>& out) {
    out.clear();
    if (g_volatile_active.empty()) return;
    auto reach = [&](AttrMode m, int arr) {
        if (m != AttrMode::Idx8 && m != AttrMode::Idx16) return;
        const uint64_t base = r.arr.base[arr] & 0x01FFFFFFu;
        if (base < g_volatile_lo || base >= g_volatile_hi) return;
        // Derniere zone qui commence au plus a `base` : la seule qui peut la contenir.
        size_t k;
        if (!g_volatile_gran.empty()) {
            k = g_volatile_gran[(size_t)((base - g_volatile_lo) >> 8)];
            while (k > 0 && g_volatile_sorted[k - 1].addr > base) --k;
        } else {
            k = (size_t)(std::upper_bound(g_volatile_sorted.begin(), g_volatile_sorted.end(), base,
                                          [](uint64_t b, const VolatileRange& v) { return b < v.addr; }) -
                         g_volatile_sorted.begin());
        }
        if (k == 0) return;
        const VolatileRange& v = g_volatile_sorted[k - 1];
        if (base >= (uint64_t)v.addr + v.len) return;
        for (const auto& o : out)
            if (o.addr == v.addr) return;
        out.push_back(v);
    };
    reach(r.vat.pos.mode, 0);
    reach(r.vat.nrm.mode, 1);
    reach(r.vat.clr[0].mode, 2);
    reach(r.vat.clr[1].mode, 3);
    for (int t = 0; t < 8; t++) reach(r.vat.tex[t].mode, 4 + t);
}

// Le dessin en cours de decodage (ses copies de memoire, « recouvrements ») est passe en parametre
// jusqu'a ResolveArrayPtr. C'etait une variable thread_local : sous MinGW comme sur Android 32 bits
// elle passe par emutls (pthread_getspecific + verrou), lue a chaque attribut des dessins a
// recouvrements, nombreux au village (peaux calculees) : ~10 % du fil de lecture (profil du 01/10).
// Sondes lues une fois au chargement : un static local se paie d'une barriere a chaque appel sur
// ARM (garde d'initialisation), et ResolveArrayPtr tourne ~10 millions de fois par seconde.
static const bool s_probe_noresolve = NJ_ENV("NJ_PROBE_NORESOLVE") != nullptr;
static const bool s_probe_noread = NJ_ENV("NJ_PROBE_NOREAD") != nullptr;

// Sonde jetable (NJ_PROBE_NORESOLVE=1). Court-circuite la resolution
// d'adresse pour mesurer sa part du temps de decodage. **Casse le rendu** :
// sert uniquement a chiffrer avant de decider d'un refactor. Precedent a ne
// pas repeter : la division sortie de la boucle par sommet, refactoree sans
// mesure prealable, gain nul.
// Raccourci resolu **une fois par dessin** (voir DecodeDraw).
//
// Mesure du 2026-09-21, deux sondes jetables sur la scene du village :
//   sans resolution ni lecture   parse 28,8 -> 20,5 ms/image   -29 %
//   resolution, lecture detournee parse 28,8 -> 28,5 ms/image   -1 %
// Donc la lecture de la memoire invitee est quasi gratuite, et c'est la
// **logique** de resolution qui pese - 20 millions d'appels par fenetre de
// 2 s pour une base et un pas constants sur tout le dessin. L'hypothese de
// depart, les defauts de cache, etait fausse : d'ou les deux sondes.
//
// t_mem_hi = 0 desactive le raccourci (le dessin a des recouvrements, ou la
// memoire n'est pas encore la) et tout repasse par ResolveArrayPtr.
static const uint8_t* ResolveArrayPtr(uint32_t addr, size_t len, const RawDraw* r) {
    if (s_probe_noresolve) {
        static const uint8_t dummy[64] = {0};
        return len <= sizeof dummy ? dummy : nullptr;
    }
    if (r) {
        const uint32_t a = addr & 0x01FFFFFFu;
        for (const auto& v : r->overlays) {
            if (v.bytes && a >= v.addr && (uint64_t)(a - v.addr) + len <= v.bytes->size())
                return v.bytes->data() + (a - v.addr);
        }
    }
    const uint8_t* real = nj::runtime::g_guest_mem->get_span(addr, len);
    // Sonde jetable n°2 (NJ_PROBE_NOREAD=1). La resolution est faite
    // entierement, seule la **lecture** est detournee vers un tampon chaud.
    // La sonde n°1 supprimait les deux a la fois : son -29 % melangeait le
    // cout de la logique et celui des defauts de cache sur la memoire invitee.
    // L'ecart entre les deux sondes separe les deux causes, et decide s'il
    // faut refactorer la resolution ou s'attaquer a la localite.
    if (s_probe_noread && real) {
        static const uint8_t dummy[64] = {0};
        return len <= sizeof dummy ? dummy : real;
    }
    return real;
}

namespace {
    inline bool trace_gx() {
        static bool t = NJ_ENV("NJ_GXTRACE") != nullptr;
        return t;
    }

    // Entiers gros-boutistes du flux.
    inline uint16_t Read16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
    inline uint32_t Read24(const uint8_t* p) { return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]; }
    inline uint32_t Read32(const uint8_t* p) {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    }

    // Champ de n bits a partir du bit lo.
    constexpr uint32_t bits(uint32_t v, int lo, int n) { return (v >> lo) & ((1u << n) - 1u); }
    inline AttrMode attr_mask(uint32_t v, int lo) { return (AttrMode)bits(v, lo, 2); }
    inline CompType attr_type(uint32_t v, int lo) { return (CompType)bits(v, lo, 3); }

    // Registres de texture : unites 0-3 a partir de `first`, unites 4-7 a partir de first + 0x20.
    // `span` registres par groupe (les tables TLUT en couvrent 8 : les 4 derniers retombent sur
    // les unites 4-7 du second groupe, comme toujours). -1 hors des deux groupes.
    inline int tex_unit(uint8_t reg, uint8_t first, int span) {
        if (reg >= first && reg < first + span) return reg - first;
        if (reg >= first + 0x20 && reg < first + 0x20 + span) return reg - (first + 0x20) + 4;
        return -1;
    }

    void apply_bp(uint8_t reg, uint32_t val);

    // BP vu a l'analyse : les signaux du PE (fin de dessin, jetons) sont mis en file jusqu'au
    // decodage du lot ; l'etat de lecture garde la valeur (NJ_NOSEED=1 : non).
    void parse_bp(uint8_t reg, uint32_t val) {
        switch (reg) {
        case 0x45:   // PE_DONE : fin de dessin demandee
            if (bits(val, 0, 4) == 2) {
                nj::runtime::gx::peek::parse_finish();
                QueuePeSignal(PeSignal::Finish, 0);
            }
            break;
        case 0x47: QueuePeSignal(PeSignal::Token, val & 0xFFFF); break;      // jeton
        case 0x48: QueuePeSignal(PeSignal::TokenInt, val & 0xFFFF); break;   // jeton + interruption
        default: break;
        }
        static const bool no_seed = NJ_ENV("NJ_NOSEED") != nullptr;
        if (!no_seed) g_pstate.bp[reg] = val;
    }

    // Combinaison d'une etape du TEV : couleur (registre pair) ou alpha (impair).
    void set_tev_combiner(TevStage& s, bool alpha, uint32_t v) {
        if (!alpha) {
            s.color.in[3] = bits(v, 0, 4);
            s.color.in[2] = bits(v, 4, 4);
            s.color.in[1] = bits(v, 8, 4);
            s.color.in[0] = bits(v, 12, 4);
            s.color.bias = bits(v, 16, 2);
            s.color.op = bits(v, 18, 1);
            s.color.clamp = bits(v, 19, 1);
            s.color.scale = bits(v, 20, 2);
            s.color.dest = bits(v, 22, 2);
        } else {
            s.alpha.in[3] = bits(v, 4, 3);
            s.alpha.in[2] = bits(v, 7, 3);
            s.alpha.in[1] = bits(v, 10, 3);
            s.alpha.in[0] = bits(v, 13, 3);
            s.alpha.bias = bits(v, 16, 2);
            s.alpha.op = bits(v, 18, 1);
            s.alpha.clamp = bits(v, 19, 1);
            s.alpha.scale = bits(v, 20, 2);
            s.alpha.dest = bits(v, 22, 2);
        }
    }

    // BP applique a l'etat du rendu.
    void apply_bp(uint8_t reg, uint32_t val) {
        GxRegs& st = g_gxregs;
        st.bp[reg] = val;
        // KSEL (tables de permutation) : bit 31 = ecrit au moins une fois. Une table ecrite a zero
        // (rouge partout, ombres : shadow.cpp) n'est pas une table jamais ecrite (ordre normal).
        if (reg >= 0xF6 && reg <= 0xFD) st.bp[reg] |= 0x80000000u;
        if (reg >= 0xE0 && reg <= 0xE7)   // couleur du TEV ou constante (bit 23)
            (bits(val, 23, 1) ? st.tev_konst : st.tev_reg)[reg - 0xE0] = val;

        if (reg == 0x00) {   // GEN_MODE
            st.n_texgen = bits(val, 0, 4);
            st.n_chan = bits(val, 4, 3);
            st.n_tev = bits(val, 10, 4) + 1;
            st.cull = bits(val, 14, 2);
            return;
        }
        if (reg >= 0x28 && reg <= 0x2F) {   // TREF : deux etapes par registre, 12 bits chacune
            for (int k = 0; k < 2; ++k) {
                const uint32_t f = val >> (12 * k);
                TevStage& s = st.tev[(reg - 0x28) * 2 + k];
                s.tex_map = bits(f, 6, 1) ? (uint8_t)bits(f, 0, 3) : 0xFF;
                s.tex_coord = bits(f, 3, 3);
                s.chan = bits(f, 7, 3);
            }
            return;
        }
        if (reg >= 0xC0 && reg <= 0xDF) {
            set_tev_combiner(st.tev[(reg - 0xC0) >> 1], (reg - 0xC0) & 1, val);
            return;
        }
        int u;
        if ((u = tex_unit(reg, 0x88, 4)) >= 0) {   // IMAGE0 : taille, format
            st.tex_units[u].width = bits(val, 0, 10) + 1;
            st.tex_units[u].height = bits(val, 10, 10) + 1;
            st.tex_units[u].format = bits(val, 20, 4);
            return;
        }
        if ((u = tex_unit(reg, 0x94, 4)) >= 0) {   // IMAGE3 : adresse
            st.tex_units[u].data_addr = bits(val, 0, 24) << 5;
            return;
        }
        if ((u = tex_unit(reg, 0x98, 8)) >= 0) {   // TLUT de l'unite
            st.tex_units[u].tlut_addr = bits(val, 0, 10) << 9;
            st.tex_units[u].tlut_fmt = bits(val, 10, 2);
            return;
        }
        switch (reg) {
        case 0x64: {   // source d'un chargement de TLUT (adresse physique sur GameCube)
            uint32_t a = bits(val, 0, 24) << 5;
            if (nj::runtime::Config::get().platform == nj::runtime::Platform::GameCube) a &= 0x01FFFFFF;
            st.tlut.src = a;
            break;
        }
        case 0x65: {   // chargement de TLUT : destination, nombre de blocs de 32 octets
            const uint32_t dst = bits(val, 0, 10) << 9;
            const uint32_t len = bits(val, 10, 11) * 32;
            auto* mmu = nj::runtime::g_guest_mem;
            if (!mmu || dst + len > sizeof(st.tlut.mem)) break;
            if (const uint8_t* p = mmu->get_span(st.tlut.src, len))
                std::memcpy(&st.tlut.mem[dst], p, len);
            else
                for (uint32_t i = 0; i < len; ++i) st.tlut.mem[dst + i] = mmu->read8(st.tlut.src + i);
            break;
        }
        case 0x40:   // ZMODE
            st.zmode.test = bits(val, 0, 1);
            st.zmode.fn = bits(val, 1, 3);
            st.zmode.write = bits(val, 4, 1);
            break;
        case 0xF3:   // ALPHACOMPARE
            st.atest.ref[0] = bits(val, 0, 8);
            st.atest.ref[1] = bits(val, 8, 8);
            st.atest.cmp[0] = bits(val, 16, 3);
            st.atest.cmp[1] = bits(val, 19, 3);
            st.atest.op = bits(val, 22, 2);
            break;
        case 0x4F: st.clear[0] = bits(val, 0, 16); break;
        case 0x50: st.clear[1] = bits(val, 0, 16); break;
        case 0x49:   // copie EFB : coin source
            st.copy.x = bits(val, 0, 10);
            st.copy.y = bits(val, 10, 10);
            break;
        case 0x4A:   // copie EFB : taille
            st.copy.w = bits(val, 0, 10) + 1;
            st.copy.h = bits(val, 10, 10) + 1;
            break;
        case 0x4B: st.copy.dest = bits(val, 0, 24) << 5; break;
        case 0x4D: st.copy.stride = bits(val, 0, 10) << 5; break;
        case 0x52:   // execution de la copie : vers l'XFB (bit 14) et/ou effacement (bit 11)
            if (trace_gx())
                NJ_PRINTF("[GXTRACE] BP 0x52 copie val=0x%08X xfb=%d effacement=%d\n", val,
                            (int)bits(val, 14, 1), (int)bits(val, 11, 1));
            if (bits(val, 14, 1)) {
                st.xfb.addr = st.copy.dest;
                st.xfb.w = st.copy.w;
                st.xfb.h = st.copy.h;
                st.xfb.stride = st.copy.stride ? st.copy.stride : (uint32_t)st.copy.w * 2;
                st.frame_done = true;
                nj::runtime::gx::peek::parse_xfb_copy();
            }
            else
                g_tex_copies.fetch_add(1, std::memory_order_relaxed);
            if (bits(val, 11, 1)) st.clear_pending = true;
            QueuePeSignal(PeSignal::Finish, 0);
            break;
        default: break;
        }
    }

    // Formats de sommets (VAT) : les champs de chaque groupe A, B, C pour un emplacement.
    void set_vat_a(VtxFormat& s, uint32_t v) {
        s.pos.cnt = bits(v, 0, 1);
        s.pos.comp = attr_type(v, 1);
        s.pos.frac = bits(v, 4, 5);
        s.nrm.cnt = bits(v, 9, 1);
        s.nrm.comp = attr_type(v, 10);
        s.clr[0].cnt = bits(v, 13, 1);
        s.clr[0].comp = attr_type(v, 14);
        s.clr[1].cnt = bits(v, 17, 1);
        s.clr[1].comp = attr_type(v, 18);
        s.tex[0].cnt = bits(v, 21, 1);
        s.tex[0].comp = attr_type(v, 22);
        s.tex[0].frac = bits(v, 25, 5);
    }
    void set_vat_b(VtxFormat& s, uint32_t v) {   // coordonnees 1 a 3, debut de la 4
        for (int t = 1; t <= 3; ++t) {
            const int lo = 9 * (t - 1);
            s.tex[t].cnt = bits(v, lo, 1);
            s.tex[t].comp = attr_type(v, lo + 1);
            s.tex[t].frac = bits(v, lo + 4, 5);
        }
        s.tex[4].cnt = bits(v, 27, 1);
        s.tex[4].comp = attr_type(v, 28);
    }
    void set_vat_c(VtxFormat& s, uint32_t v) {   // fin de la 4, coordonnees 5 a 7
        s.tex[4].frac = bits(v, 0, 5);
        for (int t = 5; t <= 7; ++t) {
            const int lo = 5 + 9 * (t - 5);
            s.tex[t].cnt = bits(v, lo, 1);
            s.tex[t].comp = attr_type(v, lo + 1);
            s.tex[t].frac = bits(v, lo + 4, 5);
        }
    }

    // CP vu a l'analyse : tableaux, index de matrices, descripteur et formats de sommets.
    void parse_cp(uint8_t reg, uint32_t val) {
        s_list_state_dirty = true;
        GxRegs& st = g_pstate;
        st.cp[reg] = val;
        if (reg >= 0xA0 && reg <= 0xAF) { st.arr.base[reg - 0xA0] = val & 0x3FFFFFFF; return; }
        if (reg >= 0xB0 && reg <= 0xBF) { st.arr.stride[reg - 0xB0] = val & 0xFF; return; }
        if (reg >= 0x70 && reg <= 0x77) { set_vat_a(st.vat[reg - 0x70], val); return; }
        if (reg >= 0x80 && reg <= 0x87) { set_vat_b(st.vat[reg - 0x80], val); return; }
        if (reg >= 0x90 && reg <= 0x97) { set_vat_c(st.vat[reg - 0x90], val); return; }
        switch (reg) {
        case 0x30:   // index de matrices A : position/normale, coordonnees 0-3
            st.mtx_idx_pos = bits(val, 0, 6);
            for (int t = 0; t < 4; ++t) st.mtx_idx_tex[t] = bits(val, 6 + 6 * t, 6);
            break;
        case 0x40:   // index de matrices B : coordonnees 4-7
            for (int t = 0; t < 4; ++t) st.mtx_idx_tex[4 + t] = bits(val, 6 * t, 6);
            break;
        case 0x50:   // descripteur (VCD) bas, commun aux 8 formats
            for (VtxFormat& s : st.vat) {
                s.pnmtx_idx = bits(val, 0, 1);
                for (int t = 0; t < 8; ++t) s.tmtx_idx[t] = bits(val, 1 + t, 1);
                s.pos.mode = attr_mask(val, 9);
                s.nrm.mode = attr_mask(val, 11);
                s.clr[0].mode = attr_mask(val, 13);
                s.clr[1].mode = attr_mask(val, 15);
                s.nrm_idx3 = bits(val, 31, 1) != 0;
            }
            break;
        case 0x60:   // descripteur haut : coordonnees de texture
            for (VtxFormat& s : st.vat)
                for (int t = 0; t < 8; ++t) s.tex[t].mode = attr_mask(val, 2 * t);
            break;
        default: break;
        }
    }

    // Taille d'une composante de sommet selon son type.
    inline int type_size(CompType t) {
        if (t == CompType::U8 || t == CompType::S8) return 1;
        if (t == CompType::U16 || t == CompType::S16) return 2;
        return 4;   // flottant
    }

    inline uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
    inline uint32_t be32b(const uint8_t* p) {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    }
    // Valeur entiere d'une composante, avant mise a l'echelle.
    inline float int_component(const uint8_t* p, CompType t) {
        switch (t) {
        case CompType::U8: return (float)p[0];
        case CompType::S8: return (float)(int8_t)p[0];
        case CompType::U16: return (float)be16(p);
        default: return (float)(int16_t)be16(p);   // S16
        }
    }
    inline float as_float(const uint8_t* p) {
        const uint32_t w = be32b(p);
        float f;
        std::memcpy(&f, &w, 4);
        return f;
    }

    // Composante lue dans le flux : entiers divises par 2^shift.
    inline float decode_scalar(const uint8_t* p, CompType t, uint8_t shift) {
        return type_size(t) == 4 ? as_float(p) : int_component(p, t) / (float)(1 << shift);
    }

    // Index d'attribut (8 ou 16 bits) lu dans le flux ; faux si le flux s'arrete avant.
    inline bool read_index(const std::vector<uint8_t>& f, size_t& off, size_t size, AttrMode m, uint32_t& idx) {
        const size_t n = m == AttrMode::Idx8 ? 1 : 2;
        if (off + n > size) return false;
        idx = n == 1 ? f[off] : be16(&f[off]);
        off += n;
        return true;
    }

    // Zones de la memoire invitee lues par un dessin, par tableau (cache de decodage).
    struct SpanTrack {
        uint32_t base[8], lo[8], hi[8];
        int n = 0;
        bool bad = false;
        void add(uint32_t array_base, uint32_t pa, uint32_t len) {
            for (int i = 0; i < n; ++i)
                if (base[i] == array_base) {
                    lo[i] = std::min(lo[i], pa);
                    hi[i] = std::max(hi[i], pa + len);
                    return;
                }
            if (n == 8) {
                bad = true;
                return;
            }
            base[n] = array_base;
            lo[n] = pa;
            hi[n] = pa + len;
            ++n;
        }
    };

    // Attribut vectoriel (position, normale, coordonnee de texture) : direct dans le flux, ou par
    // index dans un tableau de la memoire invitee. `inv` = 1 / 2^shift, calcule une fois par
    // dessin. mem_base / mem_hi : acces direct a MEM1 quand le dessin n'a pas de copie de zone.
    bool read_vector(const std::vector<uint8_t>& fifo, size_t& fifo_offset, size_t fifo_size,
                        AttrMode mask, CompType type, uint8_t shift, float inv,
                        uint32_t array_base, uint32_t array_stride, int ncomp, float* out,
                        const uint8_t* mem_base, uint32_t mem_hi, SpanTrack* trk,
                        const RawDraw* ov) {
        std::fill(out, out + ncomp, 0.0f);
        if (mask == AttrMode::None) return true;
        const size_t sz = (size_t)type_size(type), total = sz * ncomp;
        if (mask == AttrMode::Direct) {
            if (fifo_offset + total > fifo_size) return false;
            const uint8_t* p = &fifo[fifo_offset];
            if (type == CompType::S16) {
                for (int i = 0; i < ncomp; ++i) {
                    int16_t v;
                    std::memcpy(&v, p + 2 * i, 2);
                    out[i] = (float)(int16_t)__builtin_bswap16((uint16_t)v) * inv;
                }
            } else if (type == CompType::F32) {
                for (int i = 0; i < ncomp; ++i) out[i] = as_float(p + 4 * i);
            } else {
                for (int i = 0; i < ncomp; ++i) out[i] = int_component(p + sz * i, type) * inv;
            }
            fifo_offset += total;
            return true;
        }
        uint32_t index;
        if (!read_index(fifo, fifo_offset, fifo_size, mask, index)) return false;
        if (!nj::runtime::g_guest_mem) return true;
        const uint32_t addr = array_base + index * array_stride;
        const uint32_t pa = addr & 0x3FFFFFFFu;
        const uint8_t* p;
        if (mem_hi && pa + (uint32_t)total < mem_hi) {   // cas courant : MEM1, sans appel
            p = mem_base + pa;
            if (trk) trk->add(array_base, pa, (uint32_t)total);
        } else {
            p = ResolveArrayPtr(addr, total, ov);
            if (trk) trk->bad = true;
        }
        if (p == nullptr) return true;
        if (g_native_arrays) {   // coeur natif : tableaux dans l'ordre de la machine (port/endian.cpp, calcul du jeu)
            for (int i = 0; i < ncomp; ++i) {
                const uint8_t* q = p + sz * i;
                switch (type) {
                case CompType::U8: out[i] = (float)q[0] * inv; break;
                case CompType::S8: out[i] = (float)(int8_t)q[0] * inv; break;
                case CompType::U16: { uint16_t v; std::memcpy(&v, q, 2); out[i] = (float)v * inv; break; }
                case CompType::S16: { int16_t v; std::memcpy(&v, q, 2); out[i] = (float)v * inv; break; }
                default: std::memcpy(&out[i], q, 4); break;
                }
            }
            return true;
        }
        // Fastpath branchless pour les formats GameCube dominants (99 % des sommets RE4)
        if (type == CompType::S16) {
            if (ncomp == 3) {
                int16_t x, y, z;
                std::memcpy(&x, p, 2);
                std::memcpy(&y, p + 2, 2);
                std::memcpy(&z, p + 4, 2);
                out[0] = (float)(int16_t)__builtin_bswap16((uint16_t)x) * inv;
                out[1] = (float)(int16_t)__builtin_bswap16((uint16_t)y) * inv;
                out[2] = (float)(int16_t)__builtin_bswap16((uint16_t)z) * inv;
                return true;
            } else if (ncomp == 2) {
                int16_t u, v;
                std::memcpy(&u, p, 2);
                std::memcpy(&v, p + 2, 2);
                out[0] = (float)(int16_t)__builtin_bswap16((uint16_t)u) * inv;
                out[1] = (float)(int16_t)__builtin_bswap16((uint16_t)v) * inv;
                return true;
            }
        } else if (type == CompType::S8 && ncomp == 3) {
            out[0] = (float)(int8_t)p[0] * inv;
            out[1] = (float)(int8_t)p[1] * inv;
            out[2] = (float)(int8_t)p[2] * inv;
            return true;
        } else if (type == CompType::F32) {
            if (ncomp == 3) {
                out[0] = as_float(p);
                out[1] = as_float(p + 4);
                out[2] = as_float(p + 8);
                return true;
            } else if (ncomp == 2) {
                out[0] = as_float(p);
                out[1] = as_float(p + 4);
                return true;
            }
        }
        for (int i = 0; i < ncomp; ++i) {
            const uint8_t* q = p + sz * i;
            out[i] = sz == 4 ? as_float(q) : int_component(q, type) * inv;
        }
        return true;
    }

    // Couleurs : 0 RGB565, 1 RGB8, 2 RGBX8, 3 RGBA4, 4 RGBA6, 5 RGBA8.
    int color_bytes(CompType t) {
        static constexpr int kBytes[6] = {2, 3, 4, 2, 3, 4};
        const int i = (int)t;
        return i >= 0 && i < 6 ? kBytes[i] : 4;
    }

    // Champ de n bits ramene a [0, 1].
    inline float unorm(uint32_t v, int lo, int n) { return (float)bits(v, lo, n) / (float)((1u << n) - 1u); }

    void decode_color(const uint8_t* p, CompType t, float out[4]) {
        switch ((int)t) {
        case 0: {   // RGB565
            const uint16_t c = be16(p);
            out[0] = unorm(c, 11, 5);
            out[1] = unorm(c, 5, 6);
            out[2] = unorm(c, 0, 5);
            out[3] = 1.0f;
            return;
        }
        case 3: {   // RGBA4
            const uint16_t c = be16(p);
            for (int k = 0; k < 4; ++k) out[k] = unorm(c, 12 - 4 * k, 4);
            return;
        }
        case 4: {   // RGBA6
            const uint32_t c = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
            for (int k = 0; k < 4; ++k) out[k] = unorm(c, 18 - 6 * k, 6);
            return;
        }
        default: {   // 1-2 : RGB8 / RGBX8, alpha plein ; 5 : RGBA8
            const bool has_alpha = (int)t >= 5;
            for (int k = 0; k < 4; ++k) out[k] = (k < 3 || has_alpha) ? p[k] / 255.0f : 1.0f;
            return;
        }
        }
    }

    // Couleur directe ou indexee ; blanc opaque si absente.
    bool read_color(const std::vector<uint8_t>& fifo, size_t& fifo_offset, size_t fifo_size,
                       AttrMode mask, CompType clrType,
                       uint32_t array_base, uint32_t array_stride, float out[4],
                       SpanTrack* trk, const RawDraw* ov) {
        std::fill(out, out + 4, 1.0f);
        const int n = color_bytes(clrType);
        const uint8_t* src = nullptr;
        switch (mask) {
        case AttrMode::None:
            return true;
        case AttrMode::Direct:
            if (fifo_offset + (size_t)n > fifo_size) return false;
            src = &fifo[fifo_offset];
            fifo_offset += (size_t)n;
            break;
        default: {
            uint32_t index;
            if (!read_index(fifo, fifo_offset, fifo_size, mask, index)) return false;
            if (nj::runtime::g_guest_mem == nullptr) return true;
            const uint32_t at = array_base + index * array_stride;
            src = ResolveArrayPtr(at, (size_t)n, ov);
            if (trk != nullptr) {
                // la plage n'est suivie que si elle tombe dans la MEM1 lue directement
                const uint32_t phys = at & 0x3FFFFFFFu;
                const auto& mem1 = nj::runtime::g_guest_mem->mem1;
                const bool direct = src && phys + (uint32_t)n < mem1.size() && src == mem1.data() + phys;
                if (direct) trk->add(array_base, phys, (uint32_t)n);
                trk->bad = trk->bad || !direct;
            }
            break;
        }
        }
        if (src) decode_color(src, clrType, out);
        return true;
    }
}


void bp_apply(uint8_t reg, uint32_t val) { apply_bp(reg, val); }

static void parse_stream(const std::vector<uint8_t>& fifo, size_t& offset, std::vector<GxOp>& commands, int depth);



// --- listes d'affichage ----------------------------------------------------------------------
// Le jeu rappelle a chaque image les memes listes (decor, modeles) : les reanalyser prenait la
// plus grosse part du vidage d'une scene. Les commandes analysees d'une liste sont gardees, cle
// (adresse, taille), avec deux empreintes : son contenu (echantillonne) et l'etat de lecture qui
// entre dans les dessins (CP 0x30-0xBF, XF 0x0E-0x0F). Au rejeu on refait ce que la liste change a
// l'etat de lecture, et on relit ce qui vit : matrices chargees par index (os animes), banque de
// matrices des dessins a matrice par sommet, matrice courante, copies des zones reecrites.
// NJ_NODLCACHE=1 : pas de cache.
struct ListSnap;   // cliche de liste (plus bas) : les sommets deja decodes de ses dessins et la memoire dont ils dependent
struct CachedList {
    uint64_t state, content;
    std::vector<GxOp> cmds;
    std::shared_ptr<ListSnap> snap;   // nul tant que la liste n'a pas servi deux fois
    uint32_t hits = 0, cool = 0, strikes = 0;   // cool : rappels a laisser passer apres une invalidation
    std::shared_ptr<const ListMesh> mesh;      // liste compilee (V2, NJ_PULL=1) ; nul si non compilable
    bool mesh_tried = false;
};
static std::unordered_map<uint64_t, CachedList> s_lists;
static void list_snap_apply(CachedList& L, std::vector<GxOp>& commands, size_t first);
static uint64_t mesh_key(uint64_t listkey, uint64_t content);
static bool mesh_call(uint64_t k2, const std::vector<GxOp>* cmds, std::vector<GxOp>& commands);
bool PullOn();
uint64_t g_prof_dl_hits = 0;

constexpr uint64_t kFnvBasis = 14695981039346656037ull;
inline uint64_t fnv_step(uint64_t h, uint64_t v) { return (h ^ v) * 1099511628211ull; }

// Memoire XF des matrices : positions (0x000-0x0FF) et normales (0x400-0x45F).
inline bool is_matrix_xf(uint32_t a) { return a < 256 || (a >= 0x400 && a < 0x460); }

static inline uint64_t list_state_hash() {
    if (!s_list_state_dirty) return s_cached_list_state;
    uint64_t h = kFnvBasis;
    for (int r = 0x30; r <= 0xBF; ++r) h = fnv_step(h, g_pstate.cp[r]);
    h = fnv_step(h, g_pstate.xf[0x00E]);
    h = fnv_step(h, g_pstate.xf[0x00F]);
    s_cached_list_state = h;
    s_list_state_dirty = false;
    return h;
}

// Pool prealloue pour RawDraw (Zero-Allocation Pipeline).
// Remplace 3 500 allocations et 3 500 liberations par image sur le tas Android.
class RawDrawPool {
public:
    static constexpr size_t kBlockSize = 512;
    static constexpr size_t kCapacity = 8192;

    static RawDrawPool& get() {
        static RawDrawPool pool;
        return pool;
    }

    void* alloc() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_free_count > 0) return m_stack[--m_free_count];
        return ::operator new(kBlockSize);
    }

    void free(void* ptr) noexcept {
        if (!ptr) return;
        std::lock_guard<std::mutex> lk(m_mutex);
        const uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        const uintptr_t base = reinterpret_cast<uintptr_t>(m_storage);
        if (p >= base && p < base + (kCapacity * kBlockSize)) {
            if (m_free_count < kCapacity) {
                m_stack[m_free_count++] = ptr;
                return;
            }
        }
        ::operator delete(ptr);
    }

private:
    RawDrawPool() {
        m_storage = new uint8_t[kCapacity * kBlockSize];
        m_stack = new void*[kCapacity];
        for (size_t i = 0; i < kCapacity; ++i) {
            m_stack[i] = m_storage + i * kBlockSize;
        }
        m_free_count = kCapacity;
    }

    ~RawDrawPool() {
        delete[] m_stack;
        delete[] m_storage;
    }

    uint8_t* m_storage = nullptr;
    void** m_stack = nullptr;
    size_t m_free_count = 0;
    std::mutex m_mutex;
};

template <typename T>
struct RawDrawAlloc {
    using value_type = T;
    RawDrawAlloc() noexcept = default;
    template <typename U> RawDrawAlloc(const RawDrawAlloc<U>&) noexcept {}

    T* allocate(size_t n) {
        if (n == 1 && sizeof(T) <= RawDrawPool::kBlockSize) {
            return static_cast<T*>(RawDrawPool::get().alloc());
        }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_t n) noexcept {
        if (n == 1 && sizeof(T) <= RawDrawPool::kBlockSize) {
            RawDrawPool::get().free(p);
            return;
        }
        ::operator delete(p);
    }

    template <typename U> bool operator==(const RawDrawAlloc<U>&) const noexcept { return true; }
    template <typename U> bool operator!=(const RawDrawAlloc<U>&) const noexcept { return false; }
};

inline std::shared_ptr<RawDraw> make_pooled_raw() {
    return std::allocate_shared<RawDraw>(RawDrawAlloc<RawDraw>());
}

inline std::shared_ptr<RawDraw> make_pooled_raw(const RawDraw& o) {
    return std::allocate_shared<RawDraw>(RawDrawAlloc<RawDraw>(), o);
}

class VecBlockPool {
public:
    static constexpr size_t kBlockSize = 96;
    static constexpr size_t kCapacity = 8192;

    static VecBlockPool& get() {
        static VecBlockPool pool;
        return pool;
    }

    void* alloc() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_free_count > 0) return m_stack[--m_free_count];
        return ::operator new(kBlockSize);
    }

    void free(void* ptr) noexcept {
        if (!ptr) return;
        std::lock_guard<std::mutex> lk(m_mutex);
        const uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        const uintptr_t base = reinterpret_cast<uintptr_t>(m_storage);
        if (p >= base && p < base + (kCapacity * kBlockSize)) {
            if (m_free_count < kCapacity) {
                m_stack[m_free_count++] = ptr;
                return;
            }
        }
        ::operator delete(ptr);
    }

private:
    VecBlockPool() {
        m_storage = new uint8_t[kCapacity * kBlockSize];
        m_stack = new void*[kCapacity];
        for (size_t i = 0; i < kCapacity; ++i) {
            m_stack[i] = m_storage + i * kBlockSize;
        }
        m_free_count = kCapacity;
    }

    ~VecBlockPool() {
        delete[] m_stack;
        delete[] m_storage;
    }

    uint8_t* m_storage = nullptr;
    void** m_stack = nullptr;
    size_t m_free_count = 0;
    std::mutex m_mutex;
};

template <typename T>
struct VecAlloc {
    using value_type = T;
    VecAlloc() noexcept = default;
    template <typename U> VecAlloc(const VecAlloc<U>&) noexcept {}

    T* allocate(size_t n) {
        if (n == 1 && sizeof(T) <= VecBlockPool::kBlockSize) {
            return static_cast<T*>(VecBlockPool::get().alloc());
        }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_t n) noexcept {
        if (n == 1 && sizeof(T) <= VecBlockPool::kBlockSize) {
            VecBlockPool::get().free(p);
            return;
        }
        ::operator delete(p);
    }

    template <typename U> bool operator==(const VecAlloc<U>&) const noexcept { return true; }
    template <typename U> bool operator!=(const VecAlloc<U>&) const noexcept { return false; }
};

inline std::shared_ptr<const std::vector<uint8_t>> make_pooled_bytes(const uint8_t* first, const uint8_t* last) {
    return std::allocate_shared<const std::vector<uint8_t>>(VecAlloc<const std::vector<uint8_t>>(), first, last);
}

// Empreinte echantillonnee (un octet sur 512, plus les 32 derniers) : bon marche meme pour une
// liste de plusieurs Mo, qui ne change plus une fois ecrite.
static uint64_t DLContentHash(const uint8_t* p, uint32_t size) {
    uint64_t h = kFnvBasis ^ size;
    for (uint32_t i = 0; i < size; i += 512) h = fnv_step(h, p[i]);
    for (uint32_t i = size > 32 ? size - 32 : 0; i < size; ++i) h = fnv_step(h, p[i]);
    return h;
}

// Chronometres des compteurs [GXPROF] dl_ms et snap_ms : deux lectures d'heure par liste
// d'affichage et par dessin, soit deux appels systeme au Galaxy S5 (pas de vDSO), plus une
// division 64 bits (DIVPROF du 30/09 : 9 % des divisions, dans parse_stream). Seulement avec
// NJ_GXPROF_T=1 ; sinon ces deux compteurs restent a 0.
static bool gxprof_timed() {
    static const bool on = NJ_ENV("NJ_GXPROF_T") != nullptr;
    return on;
}
static std::chrono::steady_clock::time_point gxprof_now() {
    return gxprof_timed() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
}
static uint64_t gxprof_us_since(std::chrono::steady_clock::time_point t0) {
    if (!gxprof_timed()) return 0;
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
}

// Une commande d'une liste gardee, rejouee a sa place dans le flux.
static void ReplayCachedCommand(GxOp& c) {
    bool own_raw = false;   // c.raw deja copie (la version du cache reste intacte)
    auto own = [&]() {
        if (!own_raw) {
            c.raw = make_pooled_raw(*c.raw);
            own_raw = true;
        }
    };
    switch (c.type) {
    case GxOpKind::CPRegister: parse_cp((uint8_t)c.reg, c.val); break;
    case GxOpKind::BPRegister: parse_bp((uint8_t)c.reg, c.val); break;
    case GxOpKind::XFRegister:
        if (c.val & 0x80000000u) {
            // chargement par index (marque) : relu en memoire a l'adresse du tableau d'aujourd'hui
            const int array = 12 + (int)((c.val >> 24) & 3);
            const int count = (int)((c.val >> 16) & 0xFF);
            const uint32_t base = g_pstate.arr.base[array] + (c.val & 0xFFFF) * g_pstate.arr.stride[array];
            for (int i = 0; i < count && i < (int)c.payload.size(); ++i)
                c.payload[i] = xf_array_f32(base + 4 * i);
        }
        if (is_matrix_xf(c.reg)) shadow_matrix_write(c.reg, c.payload);
        xfshadow::set(c.reg, c.payload, c.payload.size());   // le rejeu pose ces valeurs : l'ombre suit
        break;
    case GxOpKind::DrawPrimitive:
        if (!c.raw) break;
        if (!c.raw->mtx.pos_bank.empty()) {
            // matrice par sommet (peau) : toute la banque, prise a ce point du flux (sinon Leon,
            // les ennemis et les vehicules gardaient la pose de la premiere image). Une photo
            // partagee : copier le RawDraw et ses 352 flottants a chaque dessin de peau coutait des
            // milliers d'allocations par image face aux ennemis (07/10).
            c.bank = current_bank();
        } else if (c.raw->mtx.single) {
            // matrice courante du dessin, reprise de l'ombre d'analyse
            const uint32_t pb = (uint32_t)c.raw->mtx_idx_pos * 4;
            const uint32_t nb = 3u * (c.raw->mtx_idx_pos & 31);
            const bool fits = pb + 12 <= 256 && nb + 9 <= 96;
            // Transformation sur le nuanceur (defaut) : la matrice va dans la commande, le RawDraw
            // partage reste tel quel. NJ_NOREPLAYMTX=1 : ancienne copie.
            static const bool copy_raw_env = NJ_ENV("NJ_NOREPLAYMTX") != nullptr;
            const bool copy_raw = copy_raw_env || (knobs_live() && nj::dev::knob("noreplaymtx", 0) != 0);
            if (xfgpu_on() && !copy_raw) {
                if (fits) {
                    std::memcpy(c.xf_pos, &s_shadow_pos[pb], 12 * sizeof(float));
                    std::memcpy(c.xf_nrm, &s_shadow_nrm[nb], 9 * sizeof(float));
                    c.xf_from_replay = true;
                    xfstat_seq(c.xf_pos);
                }
            } else {
                own();
                if (fits) {
                    std::memcpy(c.raw->mtx.pos, &s_shadow_pos[pb], 12 * sizeof(float));
                    std::memcpy(c.raw->mtx.nrm, &s_shadow_nrm[nb], 9 * sizeof(float));
                    xfstat_seq(c.raw->mtx.pos);
                }
            }
        }
        if (!g_volatile_active.empty() || !c.raw->overlays.empty()) {
            // le dessin peut lire un tampon reecrit depuis l'analyse : copies courantes
            static std::vector<VolatileRange> now;
            CollectVolatile(*c.raw, now);
            const auto& had = c.raw->overlays;
            bool same = now.size() == had.size();
            for (size_t k = 0; same && k < now.size(); ++k)
                same = now[k].addr == had[k].addr && now[k].len == had[k].len && now[k].bytes == had[k].bytes;
            if (!same) {
                own();
                c.raw->overlays = now;
            }
        }
        break;
    default: break;
    }
}

// NJ_DLSTAT=1 (verification n 1 du plan, 13/11) : ce que contiennent les listes d'affichage appelees.
namespace dlstat {
static bool on() { static const bool v = NJ_ENV("NJ_DLSTAT") != nullptr; return v; }
uint64_t calls, only_draws, draws, bp, cp, xf_direct, xf_indexed, xf_mtx_direct, other;
uint64_t sizes_hist[6];   // listes par nombre de dessins : 1, 2-4, 5-16, 17-64, 65-256, >256
static void note(const GxOp* c, size_t n) {
    ++calls;
    uint64_t d = 0, nb = 0, nc = 0, xd = 0, xi = 0, xm = 0, o = 0;
    for (size_t i = 0; i < n; ++i) {
        switch (c[i].type) {
        case GxOpKind::DrawPrimitive: ++d; break;
        case GxOpKind::BPRegister: ++nb; break;
        case GxOpKind::CPRegister: ++nc; break;
        case GxOpKind::XFRegister:
            if (c[i].val & 0x80000000u) ++xi;
            else { ++xd; if (c[i].reg < 0x400) ++xm; }
            break;
        default: ++o; break;
        }
    }
    draws += d; bp += nb; cp += nc; xf_direct += xd; xf_indexed += xi; xf_mtx_direct += xm; other += o;
    if (nb + nc + xd + xi + o == 0) ++only_draws;
    const int h = d <= 1 ? 0 : d <= 4 ? 1 : d <= 16 ? 2 : d <= 64 ? 3 : d <= 256 ? 4 : 5;
    ++sizes_hist[h];
}
}  // namespace dlstat
void DlStatDump(unsigned frame) {
    if (!dlstat::on()) return;
    using namespace dlstat;
    std::printf("[DLSTAT] image %u : %llu appels de listes, %llu sans aucun etat (dessins seuls) ; contenu : %llu dessins, "
                "%llu BP, %llu CP, %llu XF directs (dont %llu matrices), %llu XF par index, %llu autres\n", frame,
                (unsigned long long)calls, (unsigned long long)only_draws, (unsigned long long)draws, (unsigned long long)bp,
                (unsigned long long)cp, (unsigned long long)xf_direct, (unsigned long long)xf_mtx_direct,
                (unsigned long long)xf_indexed, (unsigned long long)other);
    std::printf("[DLSTAT]   listes par nombre de dessins : 1 : %llu, 2-4 : %llu, 5-16 : %llu, 17-64 : %llu, 65-256 : %llu, >256 : %llu\n",
                (unsigned long long)sizes_hist[0], (unsigned long long)sizes_hist[1], (unsigned long long)sizes_hist[2],
                (unsigned long long)sizes_hist[3], (unsigned long long)sizes_hist[4], (unsigned long long)sizes_hist[5]);
    calls = only_draws = draws = bp = cp = xf_direct = xf_indexed = xf_mtx_direct = other = 0;
    for (auto& v : sizes_hist) v = 0;
}

static void expand_list(uint32_t addr, uint32_t size,
                              std::vector<GxOp>& commands, int depth) {
    const uint32_t phys = addr & 0x1FFFFFFF;
    if (!nj::runtime::g_guest_mem || size == 0 || size > 0x400000 || phys + size > 0x01800000 || depth >= 4)
        return;
    ++g_prof_dl_calls;
    g_pf_list_bytes += size;
    const auto t0 = gxprof_now();
    // La copie prise quand le jeu a fait l'appel d'abord : la memoire peut deja tenir la liste de
    // l'image suivante.
    const uint8_t* src = nj::runtime::g_guest_mem->get_span(0x80000000u | phys, size);
    const auto snap = nj::runtime::GX_DL_Fetch(addr, size);
    if (snap) src = snap->data();

    static const bool nocache_env = NJ_ENV("NJ_NODLCACHE") != nullptr;
    const bool nocache = nocache_env || (knobs_live() && nj::dev::knob("nodlcache", 0) != 0);   // knobs.txt : bascule en direct
    if (depth == 0 && src && !nocache) {
        const uint64_t content = DLContentHash(src, size);
        const uint64_t key = ((uint64_t)phys << 23) ^ size;
        const uint64_t state = list_state_hash();
        // Liste compilee (V2, NJ_PULL=1) : cle sans les adresses des tableaux ; une seule commande, sans relire la
        // liste (elle ne contient que des dessins : aucun effet sur l'etat de l'analyseur).
        const uint64_t k2 = PullOn() ? mesh_key(key, content) : 0;
        if (k2 && mesh_call(k2, nullptr, commands)) {
            ++g_prof_dl_hits;
            g_prof_dl_us += gxprof_us_since(t0);
            return;
        }
        auto it = s_lists.find(key);
        if (it != s_lists.end() && it->second.state == state && it->second.content == content) {
            ++g_prof_dl_hits;
            if (dlstat::on()) dlstat::note(it->second.cmds.data(), it->second.cmds.size());
            if (k2 && mesh_call(k2, &it->second.cmds, commands)) {   // compilee a ce rappel
                g_prof_dl_us += gxprof_us_since(t0);
                return;
            }
            const size_t first = commands.size();
            commands.reserve(first + it->second.cmds.size());
            const uint32_t pframe = (uint32_t)g_fl_xfb_copies.load(std::memory_order_relaxed);
            for (const GxOp& cached : it->second.cmds) {
                commands.push_back(cached);
                commands.back().pull_frame = pframe;
                ReplayCachedCommand(commands.back());
            }
            list_snap_apply(it->second, commands, first);
        } else {
            std::vector<uint8_t> bytes(src, src + size);
            std::vector<GxOp> parsed;
            size_t off = 0;
            ++xfshadow::in_list;
            parse_stream(bytes, off, parsed, depth + 1);
            --xfshadow::in_list;
            if (dlstat::on()) dlstat::note(parsed.data(), parsed.size());
            if (s_lists.size() > 512) s_lists.clear();
            s_lists[key] = {state, content, parsed};
            if (k2 && mesh_call(k2, &parsed, commands)) {   // compilee des la premiere analyse (peaux : jamais en cache)
                g_prof_dl_us += gxprof_us_since(t0);
                return;
            }
            commands.reserve(commands.size() + parsed.size());
            commands.insert(commands.end(), parsed.begin(), parsed.end());
        }
        g_prof_dl_us += gxprof_us_since(t0);
        return;
    }
    std::vector<uint8_t> bytes(size);
    if (src)
        std::memcpy(bytes.data(), src, size);
    else
        for (uint32_t i = 0; i < size; ++i) bytes[i] = nj::runtime::g_guest_mem->read8(addr + i);
    size_t off = 0;
    ++xfshadow::in_list;
    parse_stream(bytes, off, commands, depth + 1);
    --xfshadow::in_list;
    g_prof_dl_us += gxprof_us_since(t0);
}

// Compteurs du vidage, ligne [GXPROF] de gx_pump_fifo : le cout par cause plutot qu'a l'estime.
uint64_t g_prof_draws = 0;         // dessins copies
uint64_t g_pf_draw_bytes = 0;    // octets de sommets copies
uint64_t g_prof_dl_calls = 0;      // listes d'affichage developpees
uint64_t g_pf_list_bytes = 0;      // octets de listes copies et analyses
uint64_t g_prof_cmds = 0;          // commandes produites
uint64_t g_prof_unknown = 0;       // octets inconnus sautes
uint64_t g_prof_dl_us = 0;         // temps dans les listes (NJ_GXPROF_T)
uint64_t g_prof_snap_us = 0;       // temps de copie des dessins (NJ_GXPROF_T)
std::atomic<uint64_t> g_prof_decoded_verts{0};
std::atomic<uint64_t> g_prof_vol_draws{0};   // dessins a copies de zones (decodage sur plusieurs fils)
std::atomic<uint32_t> g_race_draws{0};   // early_drawdone : positions changees entre analyse et decodage
uint64_t g_prof_dl_loadindx = 0;   // chargements par index vus dans les listes

namespace {
    // Octets d'un attribut dans le flux d'un sommet : rien, un index, ou la valeur directe.
    inline size_t attr_bytes(AttrMode mask, CompType type, int ncomp, bool is_color) {
        if (mask == AttrMode::None) return 0;
        if (mask == AttrMode::Idx8) return 1;
        if (mask == AttrMode::Idx16) return 2;
        return is_color ? (size_t)color_bytes(type) : (size_t)type_size(type) * ncomp;
    }

    // Taille d'un sommet du flux pour ce format.
    size_t vertex_size(const VtxFormat& vat) {
        size_t n = vat.pnmtx_idx ? 1 : 0;   // index de matrice de position, puis de texture
        for (bool b : vat.tmtx_idx) n += b ? 1 : 0;
        n += attr_bytes(vat.pos.mode, vat.pos.comp, vat.pos.cnt ? 3 : 2, false);
        // normale (9 composantes avec binormale et tangente ; trois index dans ce cas si indexee)
        const bool indexed = vat.nrm.mode == AttrMode::Idx8 || vat.nrm.mode == AttrMode::Idx16;
        const size_t nrm = attr_bytes(vat.nrm.mode, vat.nrm.comp, vat.nrm.cnt ? 9 : 3, false);
        n += (vat.nrm_idx3 && vat.nrm.cnt && indexed) ? 3 * nrm : nrm;
        for (int c = 0; c < 2; ++c) n += attr_bytes(vat.clr[c].mode, vat.clr[c].comp, 1, true);
        for (int t = 0; t < 8; ++t) n += attr_bytes(vat.tex[t].mode, vat.tex[t].comp, vat.tex[t].cnt ? 2 : 1, false);
        return n;
    }
} // namespace

// ---------------------------------------------------------------- cache de sommets
//
// Le decodeur traite ~228 000 sommets par image et les **transforme** au
// passage (GxVertex::pretransformed). Or pour tout ce qui ne bouge pas - murs, sol,
// batiments, l'essentiel d'une scene de village - le resultat est identique
// d'une image a l'autre : on recalcule chaque image ce qu'on avait deja.
//
// La cle identifie tout ce dont le resultat depend :
//   - le tampon de sommets (le shared_ptr est partage par le cache de listes
//     d'affichage, donc son adresse est stable d'une image a l'autre),
//   - le nombre de sommets et la description du format (VAT),
//   - les tableaux indexes lus en RAM invitee (base et pas),
//   - les matrices figees pour ce dessin, qui changent des que ca s'anime.
//
// Additif : une cle absente retombe sur le decodage normal. Un defaut de cle
// coute donc au pire un gain nul, pas une image cassee.
//
// Reserve : les attributs indexes pointent dans la RAM invitee, que le jeu
// pourrait reecrire sans que l'adresse change. C'est precisement ce que le
// mecanisme `overlays` detecte, donc on ne cache que les dessins qui n'en ont
// aucun.
struct VtxCacheEntry {
    std::vector<GxVertex> verts;
    uint64_t last_used;
};
// DecodeDraw tourne sur **plusieurs threads** (DecodePool), donc ce cache est
// partage : un unordered_map sans verrou s'y corrompt et bloque le jeu. Le
// commentaire de g_prof_vol_draws le disait deja, et je ne l'avais pas lu.
//
// shared_mutex plutot qu'un mutex simple : la consultation domine largement -
// c'est tout l'interet du cache - et plusieurs lectures simultanees ne se
// genent pas. Seule l'insertion, rare, prend le verrou exclusif.
static std::unordered_map<uint64_t, VtxCacheEntry> s_vtx_cache;
static NjSharedMutex s_vtx_mutex;
static std::atomic<uint64_t> s_vtx_clock{0};
std::atomic<uint64_t> g_prof_vtx_hit{0}, g_prof_vtx_miss{0}, g_prof_vtx_hit_verts{0};
// Repartition des sommets par chemin de transformation. Mesure avant
// d'optimiser : le chemin a matrice unique est le seul qui se deporte
// facilement sur le nuanceur, et il ne vaut le travail que s'il pese.
std::atomic<uint64_t> g_prof_xf_bank{0}, g_prof_xf_single{0}, g_prof_xf_none{0};

// --- Mesure : combien de matrices DISTINCTES par image ? ------------------
// Question posee avant d'ecrire le deport de la transformation sur le GPU.
// Si on passe la matrice en uniforme par dessin, il faut vider le lot a
// chaque changement de matrice, donc autant d'appels de dessin que de
// matrices. Si le nombre est petit, un simple tableau d'uniformes indexe par
// le sommet preserve le regroupement. Ce chiffre decide de la conception,
// donc on le mesure au lieu de le supposer.
//
// Actif seulement avec NJ_XFSTAT=1 : le verrou par dessin fausserait les
// mesures de temps.
static std::mutex s_mtx_stat_lock;
static std::unordered_set<uint64_t> s_mtx_seen;
std::atomic<uint64_t> g_prof_mtx_frames{0}, g_prof_mtx_sum{0}, g_prof_mtx_max{0};
std::atomic<uint64_t> g_prof_mtx_draws{0};

// Deport de la transformation des sommets sur le nuanceur. Sous interrupteur
// tant que la mesure avant/apres n'est pas faite : c'est le coeur du rendu.
bool xfgpu_on() {
    // Par defaut depuis le 2026-09-21 : +60 % mesures, rendu verifie image
    // contre image, et tenu sur un run de 260 s sans derive (18,8 i/s, 17,4
    // a 19,8). NJ_XFGPU=0 remet l'ancien chemin, pour comparer.
    static const bool on0 = [] {
        const char* e = NJ_ENV("NJ_XFGPU");
        return (e == nullptr || e[0] != '0') && !(nj::runtime::gfx::g_compat_mask & 2);
    }();
    const int k = knobs_live() ? nj::dev::knob("xfgpu", -1) : -1;   // knobs.txt : 0 / 1 en direct
    return k < 0 ? on0 : k != 0;
}

static bool xfstat_on() {
    static const bool on = NJ_ENV("NJ_XFSTAT") != nullptr;
    return on;
}

static void xfstat_note(const RawDraw& r) {
    if (!xfstat_on() || !r.mtx.single)
        return;
    uint64_t h = kFnvBasis;
    const uint8_t* b = (const uint8_t*) r.mtx.pos;
    for (size_t i = 0; i < sizeof r.mtx.pos; ++i)
        h = (h ^ b[i]) * 1099511628211ull;
    ++g_prof_mtx_draws;
    std::lock_guard<std::mutex> g(s_mtx_stat_lock);
    s_mtx_seen.insert(h);
}

// Deuxieme moitie de la mesure, et la plus decisive : combien de fois la
// matrice **change** en ordre de flux. C'est ce nombre, pas le nombre de
// valeurs distinctes, qui donne le nombre de vidages de lot qu'un uniforme
// par dessin imposerait. Appele depuis la passe d'instantane, qui est
// sequentielle - le compter dans DecodeDraw serait faux, il tourne sur
// plusieurs threads et dans le desordre.
std::atomic<uint64_t> g_prof_mtx_changes{0};
static uint64_t s_mtx_last = 0;

void xfstat_seq(const float* m) {
    if (!xfstat_on())
        return;
    uint64_t h = kFnvBasis;
    const uint8_t* b = (const uint8_t*) m;
    for (size_t i = 0; i < 12 * sizeof(float); ++i)
        h = (h ^ b[i]) * 1099511628211ull;
    if (h != s_mtx_last) {
        s_mtx_last = h;
        ++g_prof_mtx_changes;
    }
}

void xfstat_frame_end() {
    if (!xfstat_on())
        return;
    std::lock_guard<std::mutex> g(s_mtx_stat_lock);
    const uint64_t n = s_mtx_seen.size();
    s_mtx_seen.clear();
    if (n == 0)
        return;
    ++g_prof_mtx_frames;
    g_prof_mtx_sum += n;
    uint64_t cur = g_prof_mtx_max.load();
    while (n > cur && !g_prof_mtx_max.compare_exchange_weak(cur, n)) {}
}

// Desactive par defaut : mesure du 2026-09-21, scene du village, 12,7 % des
// dessins et 14,2 % des sommets seulement. La cause est structurelle et non un
// defaut de la cle : le decodeur applique la transformation camera au moment
// du decodage (GxVertex::pretransformed), donc des que la camera bouge toutes les
// matrices changent et plus rien n'est reutilisable. Dans RE4 la camera bouge
// presque toujours.
//
// Cacher des sommets **deja transformes** est donc la mauvaise forme de cache.
// La conclusion utile est ailleurs : si la transformation passait au nuanceur
// de sommets, le resultat du decodage ne dependrait plus que de la geometrie,
// et deviendrait reutilisable presque integralement - voire televersable une
// seule fois dans un tampon GPU.
//
// L'infrastructure reste en place derriere NJ_VTXCACHE=1, avec ses
// compteurs : elle redeviendra utile le jour ou la transformation aura bouge.
static bool vtx_cache_on() {
    static const bool on = NJ_ENV("NJ_VTXCACHE") != nullptr;
    return on;
}

static uint64_t vtx_key(const RawDraw& r) {
    uint64_t h = kFnvBasis;
    auto mix = [&](const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*) p;
        for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    };
    const void* bytes = r.bytes.get();
    mix(&bytes, sizeof bytes);
    mix(&r.count, sizeof r.count);
    mix(&r.vat, sizeof r.vat);
    mix(r.arr.base, sizeof r.arr.base);
    mix(r.arr.stride, sizeof r.arr.stride);
    mix(&r.mtx_idx_pos, sizeof r.mtx_idx_pos);
    mix(r.mtx_idx_tex, sizeof r.mtx_idx_tex);
    mix(&r.uses_normals, sizeof r.uses_normals);
    mix(&r.mtx.single, sizeof r.mtx.single);
    // La matrice n'entre dans la cle que si le decodeur l'applique encore.
    //
    // Avec NJ_XFGPU=1 le sommet decode ne depend plus de la camera, c'est
    // tout l'interet du deport. La laisser dans la cle annulait ce gain sans
    // rien dire : premiere mesure du cache par-dessus le deport, vtx_hit = 0
    // sur 413 591 dessins, et 138 % de temps d'analyse en plus pour un hachage
    // qui ne servait a rien. La cle decrivait la camera, pas la geometrie.
    if (!xfgpu_on()) {
        mix(r.mtx.pos, sizeof r.mtx.pos);
        mix(r.mtx.nrm, sizeof r.mtx.nrm);
    }
    if (!r.mtx.pos_bank.empty())
        mix(r.mtx.pos_bank.data(), r.mtx.pos_bank.size() * sizeof(float));
    if (!r.mtx.nrm_bank.empty())
        mix(r.mtx.nrm_bank.data(), r.mtx.nrm_bank.size() * sizeof(float));
    return h;
}

// Cache de decodage (NJ_DECCACHE=0 : coupe).
//
// Le decodage d'un dessin est une fonction pure de ses octets (flux d'indices ou sommets
// directs), de son format (VAT, tableaux, index par defaut, primitive) et des zones de la memoire
// invitee qu'il lit dans les tableaux de sommets. Pour les dessins que le nuanceur transforme (le
// sommet ne depend pas de la camera) et qui ne lisent aucune copie volatile, on garde le resultat
// avec une copie de ces octets et de ces zones : si tout est identique a l'image suivante, le
// resultat l'est aussi, a l'octet pres. Galaxy S5 (29/09) : ~200 000 sommets decodes par image,
// l'essentiel identique d'une image a l'autre.
namespace deccache {
// Zones de la memoire du jeu lues par le dessin : en-tetes puis octets, dans UNE allocation (`sp`) ; une verification
// touchait quatre blocs du tas (le vecteur des zones et un par zone), ~480 ns par dessin dont l'essentiel est du
// temps d'attente de la memoire (16/10, NJ_SPANSTAT : 3 zones, 900 octets par dessin).
struct SpanH {
    uint32_t pa, len, off;   // off : debut des octets de la zone dans la partie donnees
};
struct Entry {
    std::vector<std::uint8_t> bytes;
    // Les octets partages d'ou vient l'entree : meme objet (vecteur immuable) = memes octets,
    // sans les comparer.
    std::shared_ptr<const std::vector<uint8_t>> bytes_src;
    std::unique_ptr<uint8_t[]> sp;   // [SpanH x nsp][octets des zones]
    uint32_t nsp = 0;
    const SpanH* hdr() const { return reinterpret_cast<const SpanH*>(sp.get()); }
    const uint8_t* span_data() const { return sp.get() + (size_t)nsp * sizeof(SpanH); }
    std::shared_ptr<const std::vector<GxVertex>> verts;
    uint64_t geom_hash;
    uint64_t dkey = 0;     // cle sous laquelle l'entree est rangee
    uint32_t serial = 0;   // numero unique, jamais reutilise
    uint32_t slot = 0;     // case de `slots` (serial & 0xFFFF)
};
static std::unordered_map<uint64_t, std::shared_ptr<const Entry>> map;
static NjSharedMutex mtx;
static std::atomic<uint64_t> hits{0}, misses{0}, stale{0};
// Acces sans verrou (RawDraw::dc_ref) : cases de pointeurs vers les entrees vivantes. Une entree sortie
// du cache (remplacee, ou cache vide) reste allouee jusqu'a la fin du lot de decodage suivant : les
// lecteurs ne vivent que pendant un lot (DecodePool::run attend la fin), donc personne ne la tient plus.
static constexpr uint32_t kSlots = 1u << 16;
static std::atomic<const Entry*> slots[kSlots];
static uint32_t next_serial = 1;   // sous mtx exclusif
static std::vector<std::shared_ptr<const Entry>> retired_cur, retired_prev;   // sous mtx exclusif
static void retire(std::shared_ptr<const Entry> e) {   // sous mtx exclusif
    const Entry* p = e.get();
    slots[p->slot].compare_exchange_strong(p, nullptr, std::memory_order_release);
    retired_cur.push_back(std::move(e));
}
static bool ref_on() {
    static const bool v = NJ_ENV("NJ_NODCREF") == nullptr && !(nj::runtime::gfx::g_compat_mask & 8);
    return v;
}
// NJ_DCREFCHECK=1 : chaque passage par le chemin rapide refait la recherche par cle et compte les ecarts
// (l'entree trouvee doit etre exactement celle de la case). Essai de validation, pas de gain.
static bool ref_check() {
    static const bool v = NJ_ENV("NJ_DCREFCHECK") != nullptr;
    return v;
}
static std::atomic<uint64_t> ref_checked{0}, ref_diverged{0}, ref_same_content{0};
// Mesure (NJ_SPANSTAT=1) : ce que la verification d'une entree compare vraiment.
static bool span_stat() {
    static const bool v = NJ_ENV("NJ_SPANSTAT") != nullptr;
    return v;
}
static std::atomic<uint64_t> st_valid{0}, st_spans{0}, st_span_bytes{0}, st_bytes_cmp{0}, st_bytes_cmp_n{0};
// Zones verifiees pendant un lot (un fil de decodage : mesure seulement), pour chiffrer le recouvrement.
static std::vector<std::pair<uint32_t, uint32_t>> st_batch;   // (debut, fin)
static bool on() {
    static const bool v0 = [] {
        const char* e = NJ_ENV("NJ_DECCACHE");
        return !(e && e[0] == '0');
    }();
    const bool v = v0 && !(knobs_live() && nj::dev::knob("nodeccache", 0) != 0);
    return v;
}
static uint64_t key(const GxOp& c, const RawDraw& r) {
    uint64_t h = 0x9E3779B97F4A7C15ull ^ ((uint64_t)c.prim_type << 40) ^ r.count;
    auto mixw = [&h](uint64_t w) {
        h ^= w + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
        h *= 0xFF51AFD7ED558CCDull;
    };
    auto mixb = [&](const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            uint64_t w;
            std::memcpy(&w, b + i, 8);
            mixw(w);
        }
        uint64_t t = 0;
        for (size_t k = 0; i < n; ++i, ++k) t |= (uint64_t)b[i] << (8 * k);
        mixw(t ^ n);
    };
    mixb(r.bytes->data(), r.bytes->size());
    mixb(&r.vat, sizeof r.vat);
    mixb(r.arr.base, sizeof r.arr.base);
    mixb(r.arr.stride, sizeof r.arr.stride);
    mixw((uint64_t)r.mtx_idx_pos | ((uint64_t)r.uses_normals << 8) | ((uint64_t)c.xf_gpu << 9));
    mixb(r.mtx_idx_tex, sizeof r.mtx_idx_tex);
    return h;
}
}  // namespace deccache

// Decode `draws` dans l'ordre, un seul fil, en tirant d'avance dans le cache ce que les dessins suivants vont lire
// (RawDraw, case, entree, zones, sommets partages) : chaque dessin suit une chaine de pointeurs, et sa duree etait
// presque entierement de l'attente de la memoire. Aucun effet sur le resultat. NJ_NOPREFETCH=1 : boucle simple.
void DecodeDrawsPipelined(std::vector<GxOp*>& draws) {
    static const bool off = NJ_ENV("NJ_NOPREFETCH") != nullptr;
    const size_t n = draws.size();
    if (deccache::span_stat() && n >= 64) {
        // Mesure (NJ_SPANSTAT=1) : ou va le temps du decodage, dessins reconnus (cache) contre dessins decodes pour de bon.
        using clk = std::chrono::steady_clock;
        static uint64_t batches = 0, hits = 0, hit_ns = 0, dec = 0, dec_ns = 0, dec_verts = 0, none = 0, none_ns = 0;
        for (GxOp* c : draws) {
            const bool eligible = c->raw && c->raw->bytes;
            const auto t0 = clk::now();
            DecodeDraw(*c);
            const uint64_t dt = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(clk::now() - t0).count();
            if (!eligible) { ++none; none_ns += dt; }
            else if (c->shared_vertices) { ++hits; hit_ns += dt; }
            else { ++dec; dec_ns += dt; dec_verts += c->vertices.size(); }
        }
        if (++batches % 90 == 0) {
            const double b = (double)batches;
            NJ_LOG << "[DECSPLIT] par image : reconnus " << (hits / batches) << " (" << hit_ns / 1e6 / b << " ms), decodes "
                   << (dec / batches) << " (" << dec_ns / 1e6 / b << " ms, " << (dec_verts / batches) << " sommets), autres "
                   << (none / batches) << " (" << none_ns / 1e6 / b << " ms)" << std::endl;
            batches = hits = hit_ns = dec = dec_ns = dec_verts = none = none_ns = 0;
        }
        return;
    }
    if (off || n < 64) {
        for (GxOp* c : draws) DecodeDraw(*c);
        return;
    }
    auto entry_of = [](GxOp* c) -> const deccache::Entry* {
        const RawDraw* r = c->raw.get();
        if (!r) return nullptr;
        const uint64_t ref = r->dc_ref.v.load(std::memory_order_relaxed);
        return ref ? deccache::slots[ref & 0xFFFF].load(std::memory_order_relaxed) : nullptr;
    };
    constexpr size_t D0 = 24, D1 = 16, D2 = 10, D3 = 4;
    for (size_t i = 0; i < n; ++i) {
        if (i + D0 < n) {
            if (const RawDraw* r = draws[i + D0]->raw.get()) {   // RawDraw : plusieurs lignes
                __builtin_prefetch(r);
                __builtin_prefetch(reinterpret_cast<const char*>(r) + 64);
                __builtin_prefetch(reinterpret_cast<const char*>(r) + 128);
            }
        }
        if (i + D1 < n) {
            if (const RawDraw* r = draws[i + D1]->raw.get()) {
                const uint64_t ref = r->dc_ref.v.load(std::memory_order_relaxed);
                if (ref) __builtin_prefetch(&deccache::slots[ref & 0xFFFF]);
            }
        }
        if (i + D2 < n) {
            if (const deccache::Entry* e = entry_of(draws[i + D2])) {
                __builtin_prefetch(e);
                __builtin_prefetch(e->sp.get());
            }
        }
        if (i + D3 < n) {
            if (const deccache::Entry* e = entry_of(draws[i + D3])) {
                __builtin_prefetch(e->verts.get());
                if (e->nsp && nj::runtime::g_guest_mem) {
                    const auto& m = nj::runtime::g_guest_mem->mem1;
                    if (e->hdr()[0].pa < m.size()) __builtin_prefetch(m.data() + e->hdr()[0].pa);
                }
            }
        }
        DecodeDraw(*draws[i]);
    }
}

void DecodeEpochAdvance() {
    if (deccache::span_stat() && !deccache::st_batch.empty()) {
        // Recouvrement des zones verifiees pendant le lot qui finit : octets distincts / somme.
        static uint64_t n = 0;
        auto& v = deccache::st_batch;
        uint64_t sum = 0, uni = 0;
        for (const auto& s : v) sum += s.second - s.first;
        std::sort(v.begin(), v.end());
        uint32_t cs = v[0].first, ce = v[0].second;
        for (size_t i = 1; i < v.size(); ++i) {
            if (v[i].first <= ce) { if (v[i].second > ce) ce = v[i].second; }
            else { uni += ce - cs; cs = v[i].first; ce = v[i].second; }
        }
        uni += ce - cs;
        if (sum > 100000 && (++n % 30) == 0)
            NJ_LOG << "[SPANSTAT] lot : " << v.size() << " zones, somme " << sum << " octets, distincts " << uni
                   << " octets (" << (100 * uni / sum) << " %)" << std::endl;
        v.clear();
    }
    std::unique_lock<NjSharedMutex> lk(deccache::mtx);
    deccache::retired_prev.clear();   // retirees avant le lot qui vient de finir : plus personne ne les tient
    deccache::retired_prev.swap(deccache::retired_cur);
}

// ---------------------------------------------------------------- chargeur rapide (plan rendu natif, E1)
//
// Format dominant de RE4 (98 % des dessins de r100, tous dans des listes, NJ_FMTSTAT) : position, normale, couleur 0
// et coordonnee 0 indexees sur 16 bits, positions et uv en s16 a virgule fixe, normales s8 ou s16, tableaux dans
// l'ordre de la machine (coeur natif). Aucun cache, aucune empreinte, aucune allocation par sommet : on lit l'index,
// on va chercher la valeur, on ecrit le sommet. Resultat identique a DecodeDraw (verifie par NJ_NCP=check).
std::atomic<uint64_t>& g_snap_build_ext();
std::atomic<uint64_t>& g_snap_hit_ext();
std::atomic<uint64_t>& g_snap_draws_ext();
std::atomic<uint64_t>& g_snap_invalid_ext();
std::atomic<uint64_t>& g_snap_bad_ext();
namespace ncp {
std::atomic<uint64_t> chk_draws{0}, chk_verts{0}, chk_bad_draws{0}, chk_bad_fields{0}, t_old_ns{0}, t_new_ns{0};
static int mode() {   // 0 coupe, 1 verification, 2 actif (remplace DecodeDraw pour les dessins eligibles)
    static const int m = [] {
        const char* e = NJ_ENV("NJ_NCP");
        // Actif par defaut depuis le 13/11 (P1 : x2 S5 avec NJ_PULL) ; NJ_NCP=0 coupe, NJ_NCP=check verifie.
        return !e ? 2 : e[0] == 'c' ? 1 : e[0] == '0' ? 0 : 2;
    }();
    return m;
}
static bool eligible(const RawDraw& r) {
    const VtxFormat& f = r.vat;
    if (!g_native_arrays || !r.overlays.empty() || !r.mtx.pos_bank.empty() || f.pnmtx_idx || r.count == 0) return false;
    for (int t = 0; t < 8; ++t) if (f.tmtx_idx[t]) return false;
    if (f.pos.mode != AttrMode::Idx16 || f.pos.comp != CompType::S16 || !f.pos.cnt) return false;
    if (f.nrm.mode != AttrMode::None &&
        (f.nrm.mode != AttrMode::Idx16 || f.nrm_idx3 || f.nrm.cnt || (f.nrm.comp != CompType::S8 && f.nrm.comp != CompType::S16)))
        return false;
    if (f.clr[0].mode != AttrMode::None && f.clr[0].mode != AttrMode::Idx16) return false;
    if (f.clr[1].mode != AttrMode::None) return false;
    if (f.tex[0].mode != AttrMode::None && (f.tex[0].mode != AttrMode::Idx16 || f.tex[0].comp != CompType::S16 || !f.tex[0].cnt))
        return false;
    for (int t = 1; t < 8; ++t) if (f.tex[t].mode != AttrMode::None) return false;
    if (r.mtx.single && !xfgpu_on()) return false;   // transformation CPU : laissee a l'ancien chemin
    return true;
}
static inline const uint8_t* at(uint32_t base, uint32_t idx, uint32_t stride, size_t len, const uint8_t* mem, size_t memsz) {
    const uint32_t a = base + idx * stride;
    const uint32_t phys = a & 0x3FFFFFFFu;
    if (mem && (size_t)phys + len <= memsz) return mem + phys;
    return ResolveArrayPtr(a, len, nullptr);
}
// Decode r dans out (vide). Rend faux si le flux est trop court (dessin laisse a l'ancien chemin).
static bool decode(const RawDraw& r, std::vector<GxVertex>& out) {
    const VtxFormat& f = r.vat;
    const std::vector<uint8_t>& b = *r.bytes;
    const bool hn = f.nrm.mode != AttrMode::None, hc = f.clr[0].mode != AttrMode::None, ht = f.tex[0].mode != AttrMode::None;
    const size_t vsz = 2 + (hn ? 2 : 0) + (hc ? 2 : 0) + (ht ? 2 : 0);
    if (b.size() < vsz * r.count) return false;
    const uint8_t* mem = nj::runtime::g_guest_mem ? nj::runtime::g_guest_mem->mem1.data() : nullptr;
    const size_t memsz = nj::runtime::g_guest_mem ? nj::runtime::g_guest_mem->mem1.size() : 0;
    const float ip = 1.0f / (float)(1 << f.pos.frac), it = 1.0f / (float)(1 << f.tex[0].frac);
    static const bool nrm_raw = NJ_ENV("NJ_NRMRAW") != nullptr;
    const bool n8 = f.nrm.comp == CompType::S8;
    const float nk = nrm_raw ? 1.0f : n8 ? 1.0f / 64.0f : 1.0f / 16384.0f;
    const int cb = color_bytes(f.clr[0].comp);
    out.resize(r.count);
    const uint8_t* q = b.data();
    for (uint32_t i = 0; i < r.count; ++i) {
        GxVertex& v = out[i];
        v = GxVertex{};
        v.posMtxIdx = r.mtx_idx_pos;
        std::memcpy(v.texMtxIdx, r.mtx_idx_tex, 8);
        uint32_t k = ((uint32_t)q[0] << 8) | q[1];
        q += 2;
        v.has_pos = true;
        if (const uint8_t* p = at(r.arr.base[0], k, r.arr.stride[0], 6, mem, memsz)) {
            int16_t x[3];
            std::memcpy(x, p, 6);
            v.pos[0] = x[0] * ip; v.pos[1] = x[1] * ip; v.pos[2] = x[2] * ip;
        }
        if (hn) {
            k = ((uint32_t)q[0] << 8) | q[1];
            q += 2;
            v.has_nrm = true;
            if (n8) {
                if (const uint8_t* p = at(r.arr.base[1], k, r.arr.stride[1], 3, mem, memsz)) {
                    v.norm[0] = (float)(int8_t)p[0]; v.norm[1] = (float)(int8_t)p[1]; v.norm[2] = (float)(int8_t)p[2];
                }
            } else if (const uint8_t* p = at(r.arr.base[1], k, r.arr.stride[1], 6, mem, memsz)) {
                int16_t x[3];
                std::memcpy(x, p, 6);
                v.norm[0] = x[0]; v.norm[1] = x[1]; v.norm[2] = x[2];
            }
        }
        if (hc) {
            k = ((uint32_t)q[0] << 8) | q[1];
            q += 2;
            v.has_color = true;
            float col[4] = {1, 1, 1, 1};
            if (const uint8_t* p = at(r.arr.base[2], k, r.arr.stride[2], (size_t)cb, mem, memsz)) decode_color(p, f.clr[0].comp, col);
            std::memcpy(v.color, col, sizeof col);
        }
        if (ht) {
            k = ((uint32_t)q[0] << 8) | q[1];
            q += 2;
            v.has_tex[0] = true;
            if (const uint8_t* p = at(r.arr.base[4], k, r.arr.stride[4], 4, mem, memsz)) {
                int16_t x[2];
                std::memcpy(x, p, 4);
                v.tex[0][0] = x[0] * it; v.tex[0][1] = x[1] * it;
            }
        }
        std::memcpy(v.obj_pos, v.pos, sizeof v.obj_pos);
        for (int j = 0; j < 3; ++j) v.obj_norm[j] = v.norm[j] * nk;
        v.pretransformed = r.mtx.single;
    }
    return true;
}
// Zones de tableaux lues par r (pa debut, pa fin) : index min/max de chaque attribut. Rend faux si une zone sort de MEM1.
static bool ranges(const RawDraw& r, std::vector<std::pair<uint32_t, uint32_t>>& out) {
    const VtxFormat& f = r.vat;
    const bool hn = f.nrm.mode != AttrMode::None, hc = f.clr[0].mode != AttrMode::None, ht = f.tex[0].mode != AttrMode::None;
    const int na = 1 + hn + hc + ht;
    const int arr[4] = {0, hn ? 1 : -1, hc ? 2 : -1, ht ? 4 : -1};
    const size_t len[4] = {6, (size_t)(f.nrm.comp == CompType::S8 ? 3 : 6), (size_t)color_bytes(f.clr[0].comp), 4};
    uint32_t lo[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}, hi[4] = {0, 0, 0, 0};
    const std::vector<uint8_t>& b = *r.bytes;
    if (b.size() < (size_t)na * 2 * r.count) return false;
    const uint8_t* q = b.data();
    for (uint32_t i = 0; i < r.count; ++i)
        for (int a = 0; a < 4; ++a) {
            if (arr[a] < 0) continue;
            const uint32_t k = ((uint32_t)q[0] << 8) | q[1];
            q += 2;
            if (k < lo[a]) lo[a] = k;
            if (k > hi[a]) hi[a] = k;
        }
    const size_t memsz = nj::runtime::g_guest_mem ? nj::runtime::g_guest_mem->mem1.size() : 0;
    for (int a = 0; a < 4; ++a) {
        if (arr[a] < 0) continue;
        const uint32_t st = r.arr.stride[arr[a]];
        const uint32_t s0 = (r.arr.base[arr[a]] + lo[a] * st) & 0x3FFFFFFFu, s1 = (r.arr.base[arr[a]] + hi[a] * st) & 0x3FFFFFFFu;
        const uint32_t e = s1 + (uint32_t)len[a];
        if ((size_t)e + 8 > memsz) return false;
        out.emplace_back(s0, e);
    }
    return true;
}
// Empreinte du contenu, meme calcul que la fin de DecodeDraw : le rendu s'en sert pour garder la geometrie sur le GPU
// (cache resident, renderer_gl.cpp). 0 si un sommet n'est pas « transforme » (chemin CPU) : pas d'empreinte.
static uint64_t geom_hash(uint8_t prim, const std::vector<GxVertex>& verts) {
    if (verts.empty()) return 0;
    uint64_t h = 0x9E3779B97F4A7C15ull ^ ((uint64_t)prim << 32) ^ verts.size();
    auto mixw = [&h](uint64_t w) {
        h ^= w + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
        h *= 0xFF51AFD7ED558CCDull;
    };
    auto mixf = [&](const float* f, int n) {
        for (int k = 0; k + 1 < n; k += 2) {
            uint64_t w;
            std::memcpy(&w, f + k, 8);
            mixw(w);
        }
        if (n & 1) {
            uint32_t w;
            std::memcpy(&w, f + n - 1, 4);
            mixw(w);
        }
    };
    for (const auto& v : verts) {
        if (!v.pretransformed) return 0;
        mixf(v.pos, 3);
        mixf(v.norm, 3);
        mixf(v.color, 4);
        mixf(v.tex[0], 2);
        mixf(v.obj_pos, 3);
        mixf(v.obj_norm, 3);
        uint64_t idx;
        std::memcpy(&idx, v.texMtxIdx, 8);
        mixw(idx);
        mixw((uint64_t)v.posMtxIdx | ((uint64_t)v.has_pos << 8) | ((uint64_t)v.has_nrm << 9) |
             ((uint64_t)v.has_color << 10) | ((uint64_t)v.has_tex[0] << 11));
    }
    return h | 1;
}
static bool same(const GxVertex& a, const GxVertex& b) {
    auto eq = [](const float* x, const float* y, int n) { for (int i = 0; i < n; ++i) if (x[i] != y[i]) return false; return true; };
    return a.has_pos == b.has_pos && a.has_nrm == b.has_nrm && a.has_color == b.has_color && a.has_tex[0] == b.has_tex[0] &&
           eq(a.obj_pos, b.obj_pos, 3) && eq(a.pos, b.pos, 3) && eq(a.norm, b.norm, 3) && eq(a.obj_norm, b.obj_norm, 3) &&
           eq(a.color, b.color, 4) && eq(a.tex[0], b.tex[0], 2) && a.posMtxIdx == b.posMtxIdx &&
           a.pretransformed == b.pretransformed && !std::memcmp(a.texMtxIdx, b.texMtxIdx, 8);
}
// Verification a la sortie de DecodeDraw : compare ce que l'ancien chemin a produit au chargeur rapide.
struct Check {
    GxOp& c;
    std::vector<GxVertex> fast;
    bool on = false;
    std::chrono::steady_clock::time_point t0;
    explicit Check(GxOp& cc) : c(cc) {
        if (mode() != 1 || !c.raw || !eligible(*c.raw)) return;
        const auto a = std::chrono::steady_clock::now();
        if (!decode(*c.raw, fast)) return;
        t_new_ns += (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - a).count();
        on = true;
        t0 = std::chrono::steady_clock::now();
    }
    ~Check() {
        if (!on) return;
        t_old_ns += (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
        const auto& old = c.verts();
        ++chk_draws;
        chk_verts += fast.size();
        bool bad = old.size() != fast.size();
        for (size_t i = 0; !bad && i < old.size(); ++i)
            if (!same(old[i], fast[i])) { bad = true; ++chk_bad_fields; }
        if (bad && ++chk_bad_draws <= 5) {
            const GxVertex& a = old.empty() ? GxVertex{} : old[0];
            const GxVertex& z = fast.empty() ? GxVertex{} : fast[0];
            std::printf("[NCP] ECART : %zu/%zu sommets ; ancien pos %g %g %g nrm %g %g %g col %g uv %g %g ; rapide pos %g %g %g nrm %g %g %g col %g uv %g %g\n",
                        old.size(), fast.size(), a.pos[0], a.pos[1], a.pos[2], a.norm[0], a.norm[1], a.norm[2], a.color[0], a.tex[0][0], a.tex[0][1],
                        z.pos[0], z.pos[1], z.pos[2], z.norm[0], z.norm[1], z.norm[2], z.color[0], z.tex[0][0], z.tex[0][1]);
        }
    }
};
void dump(unsigned frame) {
    if (mode() == 0) return;
    const uint64_t d = chk_draws.exchange(0), v = chk_verts.exchange(0), bd = chk_bad_draws.exchange(0);
    const double to = t_old_ns.exchange(0) / 1e6, tn = t_new_ns.exchange(0) / 1e6;
    chk_bad_fields = 0;
    std::printf("[NCP] cliches de listes : %llu construits, %llu poses (%llu dessins), %llu invalides, %llu ecarts\n",
                (unsigned long long)g_snap_build_ext().exchange(0), (unsigned long long)g_snap_hit_ext().exchange(0),
                (unsigned long long)g_snap_draws_ext().exchange(0), (unsigned long long)g_snap_invalid_ext().exchange(0),
                (unsigned long long)g_snap_bad_ext().exchange(0));
    std::printf("[NCP] image %u : %llu dessins verifies (%llu sommets), %llu en ecart ; ancien %.2f ms, rapide %.2f ms\n",
                frame, (unsigned long long)d, (unsigned long long)v, (unsigned long long)bd, to, tn);
}
}  // namespace ncp

// Sommets tires par le nuanceur (NJ_PULL=1, A1) : pas de decodage, copie des octets bruts des zones lues (GxOp::pull_blob).
bool PullOn() {
    // Actif par defaut depuis le 13/11 (P1) ; NJ_PULL=0 coupe. Jamais sur iOS : GL ES 3.0 sans texture tampon (le rendu
    // ne saurait pas dessiner ces commandes). Le coeur natif seul en profite (ncp::eligible exige les tableaux natifs).
#if defined(__APPLE__)
    static const bool v = false;
#else
    static const bool v = [] { const char* e = NJ_ENV("NJ_PULL"); return !(e && e[0] == '0'); }();
#endif
    return v;
}
std::atomic<uint64_t> g_pull_draws{0}, g_pull_bytes{0};
// Copies de l'image en cours : (debut du tableau, pas) -> copie depuis le debut du tableau jusqu'a la fin lue la plus
// loin. Le jeu ecrit ses tableaux avant de dessiner, et ses tampons de peau viennent d'un allocateur par image (une
// adresse = un modele) : la meme copie sert a toutes les parties de l'image. Remise a zero par PullFrameEnd.
namespace pullcopy {
struct Ent { std::shared_ptr<const std::vector<uint8_t>> blob; uint32_t end; uint32_t frame; };
static std::mutex mu;
static std::unordered_map<uint64_t, Ent> map;
static uint32_t last_purge = 0;
}
// Copie (partagee pour l'image) du tableau qui commence a base, lue jusqu'a e. Verrou pullcopy::mu tenu par l'appelant.
static std::shared_ptr<const std::vector<uint8_t>> pull_copy(const uint8_t* mem, uint32_t base, uint32_t stride, uint32_t e,
                                                             uint32_t frame) {
    // Une copie ne sert qu'a l'image ou elle a ete prise (cle : image d'analyse du dessin). Purge des images passees,
    // ici pour les deux appelants : placee dans pull_capture seulement, elle ne tournait jamais quand toute l'image passe
    // par les listes compilees (chateau : +10 Mo/s, coeur tue par Android vers 700 Mo, 14/11).
    if (frame >= pullcopy::last_purge + 4 || frame < pullcopy::last_purge) {
        for (auto it = pullcopy::map.begin(); it != pullcopy::map.end();)
            it = (it->second.frame + 2 < frame) ? pullcopy::map.erase(it) : std::next(it);
        pullcopy::last_purge = frame;
    }
    const uint64_t key = ((uint64_t)stride << 32) ^ ((uint64_t)frame << 40) ^ base;
    auto& en = pullcopy::map[key];
    en.frame = frame;
    if (!en.blob || en.end < e) {   // premiere lecture de l'image, ou plus loin qu'avant : copie (plus grande)
        // croissance au moins double quand une partie lit plus loin (sinon recopies en cascade : 9,8 Mo/image)
        uint32_t ne = e;
        if (en.blob) {
            const uint64_t want = (uint64_t)base + 2ull * (en.end - base);
            const uint64_t lim = (uint64_t)nj::runtime::g_guest_mem->mem1.size() - 8;
            ne = (uint32_t)std::min<uint64_t>(std::max<uint64_t>(e, want), lim);
            if (ne < e) ne = e;
        }
        auto b = std::make_shared<std::vector<uint8_t>>(((size_t)(ne - base) + 11u) & ~(size_t)3u, 0);
        std::memcpy(b->data(), mem + base, ne - base);
        en.blob = std::move(b);
        en.end = ne;
        g_pull_bytes.fetch_add(ne - base, std::memory_order_relaxed);
    }
    return en.blob;
}
static bool pull_capture(GxOp& c, const RawDraw& r) {
    std::vector<std::pair<uint32_t, uint32_t>> rr;
    if (!nj::runtime::g_guest_mem || !ncp::ranges(r, rr)) return false;
    const uint8_t* mem = nj::runtime::g_guest_mem->mem1.data();
    const VtxFormat& f = r.vat;
    const bool have[4] = {true, f.nrm.mode != AttrMode::None, f.clr[0].mode != AttrMode::None, f.tex[0].mode != AttrMode::None};
    const int arr[4] = {0, 1, 2, 4};
    size_t k = 0;
    std::lock_guard<std::mutex> lk(pullcopy::mu);
    for (int a = 0; a < 4; ++a) {
        c.pull_bias[a] = 0;
        if (!have[a]) { c.pull_blob[a].reset(); continue; }
        const uint32_t e = rr[k].second;
        ++k;
        const uint32_t base = r.arr.base[arr[a]] & 0x3FFFFFFFu;
        c.pull_blob[a] = pull_copy(mem, base, r.arr.stride[arr[a]], e, c.pull_frame);
    }
    c.pull = true;
    c.geom_hash = 0;
    g_pull_draws.fetch_add(1, std::memory_order_relaxed);
    return true;
}

// ---------------------------------------------------------------- listes compilees (V2)
std::atomic<uint64_t> g_pull_lists{0}, g_pull_list_draws{0};
static std::atomic<uint64_t> g_lm_rej[8];   // raisons de refus de compilation (journal [PULL])
static uint64_t pull_fmt_sig(const RawDraw& r) {   // champs du format lus par le nuanceur tire, et etat de l'appel
    const VtxFormat& f = r.vat;
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    mix((uint64_t)f.pos.mode); mix((uint64_t)f.pos.comp); mix(f.pos.frac); mix(f.pos.cnt);
    mix((uint64_t)f.nrm.mode); mix((uint64_t)f.nrm.comp); mix(f.nrm.cnt); mix(f.nrm_idx3);
    mix((uint64_t)f.clr[0].mode); mix((uint64_t)f.clr[0].comp);
    mix((uint64_t)f.tex[0].mode); mix((uint64_t)f.tex[0].comp); mix(f.tex[0].frac); mix(f.tex[0].cnt);
    mix(r.mtx_idx_pos);
    for (int t = 0; t < 8; ++t) mix(r.mtx_idx_tex[t]);
    for (int a : {0, 1, 2, 4}) { mix(r.arr.base[a]); mix(r.arr.stride[a]); }
    return h;
}
static std::shared_ptr<const ListMesh> compile_list_mesh(const std::vector<GxOp>& cmds) {
    if (cmds.empty() || !xfgpu_on()) return nullptr;
    const GxOp& o0 = cmds[0];
    if (o0.type != GxOpKind::DrawPrimitive || !o0.raw) return nullptr;
    const uint64_t sig = pull_fmt_sig(*o0.raw);
    auto m = std::make_shared<ListMesh>();
    const VtxFormat& f = o0.raw->vat;
    const bool hn = f.nrm.mode != AttrMode::None, hc = f.clr[0].mode != AttrMode::None, ht = f.tex[0].mode != AttrMode::None;
    m->vsz = 2u * (1u + hn + hc + ht);
    m->variant = (uint16_t)(0x180 | (hn ? (1 | (f.nrm.comp == CompType::S8 ? 8 : 0)) : 0) |
                            (hc ? (2 | (((int)f.clr[0].comp & 7) << 4)) : 0) | (ht ? 4 : 0));
    const bool have[4] = {true, hn, hc, ht};
    for (const GxOp& o : cmds) {
        if (o.type != GxOpKind::DrawPrimitive || !o.raw || !o.raw->bytes) { g_lm_rej[0]++; return nullptr; }
        const RawDraw& r = *o.raw;
        if (!ncp::eligible(r)) { g_lm_rej[1]++; return nullptr; }
        if (!r.mtx.single) { g_lm_rej[2]++; return nullptr; }
        if (pull_fmt_sig(r) != sig) { g_lm_rej[3]++; return nullptr; }
        const uint8_t prim = o.prim_type;
        if (prim != 0x80 && prim != 0x90 && prim != 0x98 && prim != 0xA0) { g_lm_rej[4]++; return nullptr; }
        const uint32_t count = r.count;
        if (r.bytes->size() < (size_t)m->vsz * count) return nullptr;
        const uint32_t b = m->nverts;
        const uint8_t* q = r.bytes->data();
        for (uint32_t i = 0; i < count; ++i)
            for (int a = 0, k = 0; a < 4; ++a) {
                if (!have[a]) continue;
                const uint32_t ix = ((uint32_t)q[i * m->vsz + 2 * k] << 8) | q[i * m->vsz + 2 * k + 1];
                if (ix > m->hi[a]) m->hi[a] = ix;
                ++k;
            }
        m->stream.insert(m->stream.end(), q, q + (size_t)m->vsz * count);
        auto tri = [&](uint32_t x, uint32_t y, uint32_t z) { m->idx.push_back(b + x); m->idx.push_back(b + y); m->idx.push_back(b + z); };
        switch (prim) {   // memes motifs que push_prim (renderer_gl.cpp)
        case 0x80: for (uint32_t q4 = 0; q4 + 3 < count; q4 += 4) { tri(q4, q4 + 1, q4 + 2); tri(q4 + 2, q4 + 3, q4); } break;
        case 0x90: for (uint32_t t = 0; t + 2 < count; t += 3) tri(t, t + 1, t + 2); break;
        case 0x98: for (uint32_t k = 2; k < count; ++k) { if (k % 2) tri(k - 1, k - 2, k); else tri(k - 2, k - 1, k); } break;
        default: for (uint32_t k = 2; k < count; ++k) tri(0, k - 1, k); break;
        }
        m->nverts += count;
    }
    m->stream.resize((m->stream.size() + 11) & ~(size_t)3, 0);
    if (m->idx.empty()) return nullptr;
    return m;
}
// Listes compilees par (adresse, contenu, etat SANS les adresses des tableaux) : une peau en double tampon garde sa
// liste compilee d'une image a l'autre ; chaque appel prend les adresses de l'etat courant.
static std::unordered_map<uint64_t, std::shared_ptr<const ListMesh>> s_meshes;
static std::unordered_set<uint64_t> s_mesh_fail;
static uint64_t mesh_key(uint64_t listkey, uint64_t content) {
    uint64_t h = kFnvBasis ^ listkey;
    h = fnv_step(h, content);
    for (int r = 0x30; r <= 0x9F; ++r) h = fnv_step(h, g_pstate.cp[r]);
    for (int r = 0xB0; r <= 0xBF; ++r) h = fnv_step(h, g_pstate.cp[r]);
    h = fnv_step(h, g_pstate.xf[0x00E]);
    h = fnv_step(h, g_pstate.xf[0x00F]);
    return h | 1;
}
static bool emit_mesh(const std::shared_ptr<const ListMesh>& m, std::vector<GxOp>& commands);
// Liste deja compilee pour cette cle : une commande. Sinon, si cmds est fourni, compilation (une fois par cle).
static bool mesh_call(uint64_t k2, const std::vector<GxOp>* cmds, std::vector<GxOp>& commands) {
    static const bool off = NJ_ENV("NJ_PULL_NOLIST") != nullptr;
    if (off || !k2 || !nj::runtime::g_guest_mem) return false;
    auto it = s_meshes.find(k2);
    if (it != s_meshes.end()) return emit_mesh(it->second, commands);
    if (!cmds || s_mesh_fail.count(k2)) return false;
    auto m = compile_list_mesh(*cmds);
    if (!m) {
        g_lm_rej[5]++;
        if (s_mesh_fail.size() > 8192) s_mesh_fail.clear();
        s_mesh_fail.insert(k2);
        return false;
    }
    std::const_pointer_cast<ListMesh>(m)->tmpl = (*cmds)[0].raw;
    if (s_meshes.size() > 4096) s_meshes.clear();
    s_meshes.emplace(k2, m);
    return emit_mesh(m, commands);
}
// Appel d'une liste compilee : UNE commande (matrice de l'appel, tableaux de l'etat courant copies pour l'image).
static bool emit_mesh(const std::shared_ptr<const ListMesh>& mesh, std::vector<GxOp>& commands) {
    if (!mesh->tmpl) return false;
    const RawDraw& r = *mesh->tmpl;
    const uint32_t pb = (uint32_t)r.mtx_idx_pos * 4, nb = 3u * (r.mtx_idx_pos & 31);
    if (pb + 12 > 256 || nb + 9 > 96) return false;
    const VtxFormat& f = r.vat;
    const size_t len[4] = {6, (size_t)(f.nrm.comp == CompType::S8 ? 3 : 6), (size_t)color_bytes(f.clr[0].comp), 4};
    const bool have[4] = {true, f.nrm.mode != AttrMode::None, f.clr[0].mode != AttrMode::None, f.tex[0].mode != AttrMode::None};
    const int arr[4] = {0, 1, 2, 4};
    const size_t memsz = nj::runtime::g_guest_mem->mem1.size();
    GxOp op;
    op.type = GxOpKind::DrawPrimitive;
    op.prim_type = 0x90;
    op.raw = mesh->tmpl;
    op.list = mesh;
    op.pull = true;
    op.xf_gpu = true;
    op.xf_from_replay = true;
    op.pull_frame = (uint32_t)g_fl_xfb_copies.load(std::memory_order_relaxed);
    std::memcpy(op.xf_pos, &s_shadow_pos[pb], 12 * sizeof(float));
    std::memcpy(op.xf_nrm, &s_shadow_nrm[nb], 9 * sizeof(float));
    {
        std::lock_guard<std::mutex> lk(pullcopy::mu);
        for (int a = 0; a < 4; ++a) {
            if (!have[a]) continue;
            const uint32_t base = g_pstate.arr.base[arr[a]] & 0x3FFFFFFFu, st = g_pstate.arr.stride[arr[a]];   // etat de CET appel
            const uint64_t e = (uint64_t)base + (uint64_t)mesh->hi[a] * st + len[a];
            if (e + 8 > memsz) return false;
            op.pull_blob[a] = pull_copy(nj::runtime::g_guest_mem->mem1.data(), base, st, (uint32_t)e, op.pull_frame);
        }
    }
    commands.push_back(std::move(op));
    g_pull_lists.fetch_add(1, std::memory_order_relaxed);
    g_pull_list_draws.fetch_add(mesh->nverts, std::memory_order_relaxed);
    return true;
}

void DecodeDraw(GxOp& c) {
    ncp::Check ncp_check(c);
    if (c.type != GxOpKind::DrawPrimitive || !c.raw || !c.raw->bytes ||
        !c.vertices.empty() || c.shared_vertices || c.pull)
        return;
    const RawDraw& r = *c.raw;
    if (PullOn() && ncp::eligible(r) && xfgpu_on() && r.mtx.single && r.mtx.pos_bank.empty()) {
        c.xf_gpu = true;
        if (!c.xf_from_replay) {
            std::memcpy(c.xf_pos, r.mtx.pos, sizeof c.xf_pos);
            std::memcpy(c.xf_nrm, r.mtx.nrm, sizeof c.xf_nrm);
        }
        if (pull_capture(c, r)) return;
    }
    // Chargeur rapide actif (NJ_NCP=1) : dessins du format dominant, meme resultat que la suite (verifie NJ_NCP=check).
    if (ncp::mode() == 2 && ncp::eligible(r)) {
        if (xfgpu_on() && r.mtx.single && r.mtx.pos_bank.empty()) {
            c.xf_gpu = true;
            if (!c.xf_from_replay) {
                std::memcpy(c.xf_pos, r.mtx.pos, sizeof c.xf_pos);
                std::memcpy(c.xf_nrm, r.mtx.nrm, sizeof c.xf_nrm);
            }
        }
        if (ncp::decode(r, c.vertices)) {
            c.geom_hash = 0;
            return;
        }
        c.vertices.clear();
    }
    if (r.vtx_fp && LiveFingerprint(r) != r.vtx_fp) {
        g_race_draws.fetch_add(1, std::memory_order_relaxed);
        NoteRacePage(r.arr.base[0] & 0x01FFFFFFu, r.count);
    }
    xfstat_note(r);

    // Raccourci de resolution, arme une fois pour tout le dessin et passe en
    // **parametre**, pas par une variable de thread.
    //
    // Premiere version avec thread_local : -9,4 % d'images par seconde, pire
    // que sans raccourci. Sous MinGW un acces thread_local dans une boucle
    // chaude passe par emutls, donc un appel de fonction - on remplacait un
    // appel par un autre. Le detail de plateforme mangeait tout le gain.
    const uint8_t* mem_base = nullptr;
    uint32_t mem_hi = 0;
    if (r.overlays.empty() && nj::runtime::g_guest_mem &&
        nj::runtime::g_guest_mem->mem1.size() > 8) {
        mem_base = nj::runtime::g_guest_mem->mem1.data();
        mem_hi = (uint32_t) (nj::runtime::g_guest_mem->mem1.size() - 8);
    }
    if (xfgpu_on() && r.mtx.single && r.mtx.pos_bank.empty()) {
        c.xf_gpu = true;
        if (!c.xf_from_replay) {
            std::memcpy(c.xf_pos, r.mtx.pos, sizeof c.xf_pos);
            std::memcpy(c.xf_nrm, r.mtx.nrm, sizeof c.xf_nrm);
        }
    }
    const RawDraw* const ov = r.overlays.empty() ? nullptr : &r;   // recouvrements de ce dessin
    const std::vector<uint8_t>& buf = *r.bytes;
    g_prof_decoded_verts.store(g_prof_decoded_verts.load(std::memory_order_relaxed) + r.count, std::memory_order_relaxed);
    if (!r.overlays.empty()) {
        ++g_prof_vol_draws;
        c.volatile_src = true;
    }

    // Cache de decodage (voir deccache) : dessins transformes par le nuanceur, sans copie volatile.
    const bool dc = deccache::on() && r.overlays.empty() && r.mtx.pos_bank.empty() && mem_base &&
                    r.count > 0 && (c.xf_gpu || !r.mtx.single);
    uint64_t dkey = 0;
    SpanTrack track;
    SpanTrack* trk = nullptr;
    if (dc) {
        // La cle depend aussi de la primitive et de xf_gpu (propres a la commande) : la cle memorisee
        // sur le RawDraw n'est reprise que pour la meme combinaison.
        const uint64_t salt = ((uint64_t)c.prim_type << 1) | (uint64_t)c.xf_gpu;
        if (r.dc_key && (r.dc_key & 0x1FF) == salt) {
            dkey = r.dc_key;
        } else {
            dkey = (deccache::key(c, r) & ~0x1FFull) | salt;
            r.dc_key = dkey;
        }
        {
            static std::atomic<uint64_t> tick{0};
            const uint64_t t = tick.load(std::memory_order_relaxed) + 1;
            tick.store(t, std::memory_order_relaxed);
            if ((t & 0x1FFFF) == 0)
                NJ_LOG << "[DECCACHE] reutilises=" << deccache::hits.exchange(0)
                          << " decodes=" << deccache::misses.exchange(0)
                          << " perimes=" << deccache::stale.exchange(0) << std::endl;
        }
        // Verification commune : memes octets, memes zones de la memoire du jeu.
        auto still_valid = [&](const deccache::Entry& en) {
            if (deccache::span_stat()) {
                auto add = [](std::atomic<uint64_t>& a, uint64_t n) { a.store(a.load(std::memory_order_relaxed) + n, std::memory_order_relaxed); };
                add(deccache::st_valid, 1);
                add(deccache::st_spans, en.nsp);
                for (uint32_t k = 0; k < en.nsp; ++k) {
                    const deccache::SpanH& sp = en.hdr()[k];
                    add(deccache::st_span_bytes, sp.len);
                    if (deccache::st_batch.size() < (1u << 22))
                        deccache::st_batch.emplace_back(sp.pa, sp.pa + sp.len);
                }
                if (en.bytes_src != r.bytes) {
                    add(deccache::st_bytes_cmp_n, 1);
                    add(deccache::st_bytes_cmp, en.bytes.size());
                }
                if ((deccache::st_valid.load(std::memory_order_relaxed) & 0x3FFFF) == 0)
                    NJ_LOG << "[SPANSTAT] par verification : " << deccache::st_spans.load() / deccache::st_valid.load()
                           << " zones, " << deccache::st_span_bytes.load() / deccache::st_valid.load()
                           << " octets de zones, octets de dessin compares sur "
                           << (100 * deccache::st_bytes_cmp_n.load() / deccache::st_valid.load()) << " % des verifications ("
                           << (deccache::st_bytes_cmp_n.load() ? deccache::st_bytes_cmp.load() / deccache::st_bytes_cmp_n.load() : 0)
                           << " octets en moyenne)" << std::endl;
            }
            bool same = en.bytes_src == r.bytes ||
                        (en.bytes.size() == r.bytes->size() &&
                         std::memcmp(en.bytes.data(), r.bytes->data(), en.bytes.size()) == 0);
            if (same && en.nsp) {
                const deccache::SpanH* h = en.hdr();
                const uint8_t* d = en.span_data();
                for (uint32_t i = 0; same && i < en.nsp; ++i)
                    same = (size_t)h[i].pa + h[i].len <= (size_t)mem_hi &&
                           std::memcmp(mem_base + h[i].pa, d + h[i].off, h[i].len) == 0;
            }
            return same;
        };
        // Chemin rapide : l'entree validee la derniere fois par ce dessin (sans recherche ni verrou).
        if (deccache::ref_on()) {
            const uint64_t ref = r.dc_ref.v.load(std::memory_order_relaxed);
            if (ref) {
                const deccache::Entry* en = deccache::slots[ref & 0xFFFF].load(std::memory_order_acquire);
                if (en && en->serial == (uint32_t)(ref >> 16) && en->dkey == dkey && still_valid(*en)) {
                    if (deccache::ref_check()) {
                        std::shared_ptr<const deccache::Entry> chk;
                        {
                            std::shared_lock<NjSharedMutex> lk(deccache::mtx);
                            auto it = deccache::map.find(dkey);
                            if (it != deccache::map.end()) chk = it->second;
                        }
                        const uint64_t n = ++deccache::ref_checked;
                        // Une autre entree sous la meme cle de contenu identique (deux fils ont decode le meme dessin
                        // en meme temps, le second a remplace le premier) n'est pas un ecart : meme resultat.
                        if (chk.get() != en) {
                            if (chk && chk->verts && en->verts && chk->geom_hash == en->geom_hash &&
                                chk->verts->size() == en->verts->size()) {
                                ++deccache::ref_same_content;
                            } else {
                                const uint64_t d = ++deccache::ref_diverged;
                                if (d <= 20)
                                    NJ_LOG << "[DCREFCHECK] ECART #" << d << " sur " << n << " (cle " << dkey
                                           << (chk ? " entree differente" : " plus d'entree") << ")" << std::endl;
                            }
                        }
                        if ((n & 0xFFFFF) == 0)
                            NJ_LOG << "[DCREFCHECK] " << n << " verifies, " << deccache::ref_diverged.load()
                                   << " ecarts, " << deccache::ref_same_content.load() << " remplacees a l'identique" << std::endl;
                    }
                    c.shared_vertices = en->verts;
                    c.geom_hash = en->geom_hash;
                    deccache::hits.store(deccache::hits.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
                    return;
                }
            }
        }
        std::shared_ptr<const deccache::Entry> e;
        {
            std::shared_lock<NjSharedMutex> lk(deccache::mtx);
            auto it = deccache::map.find(dkey);
            if (it != deccache::map.end()) e = it->second;
        }
        if (e) {
            if (still_valid(*e)) {
                c.shared_vertices = e->verts;
                c.geom_hash = e->geom_hash;
                ++deccache::hits;
                if (deccache::ref_on())
                    r.dc_ref.v.store(((uint64_t)e->serial << 16) | e->slot, std::memory_order_relaxed);
                return;
            }
            ++deccache::stale;
        }
        ++deccache::misses;
        trk = &track;
    }
    // Consultation du cache de sommets. Uniquement quand le dessin n'a aucun
    // recouvrement : ceux-la lisent des tampons que le jeu reecrit, donc leur
    // resultat n'est pas reproductible d'une image a l'autre.
    const bool vc = vtx_cache_on() && r.overlays.empty() && r.count > 0 && !c.bank;
    uint64_t vkey = 0;
    if (vc) {
        vkey = vtx_key(r);
        {
            std::shared_lock<NjSharedMutex> lk(s_vtx_mutex);
            auto it = s_vtx_cache.find(vkey);
            if (it != s_vtx_cache.end()) {
                c.vertices = it->second.verts;
                ++g_prof_vtx_hit;
                g_prof_vtx_hit_verts += r.count;
                c.raw.reset();
                return;
            }
        }
        ++g_prof_vtx_miss;
    }
    // Echelles de quantification, une fois pour le dessin (le decalage vient du format, les
    // normales n'en ont pas : lues brutes, voir plus bas).
    const VtxFormat& fmt = r.vat;
    const float inv_pos = 1.0f / (float)(1 << fmt.pos.frac);
    const float inv_tex0 = 1.0f / (float)(1 << fmt.tex[0].frac);
    // Normale indexee avec binormale et tangente : trois index dans le flux, seule la normale sert.
    const bool nrm_indexed = fmt.nrm.mode == AttrMode::Idx8 || fmt.nrm.mode == AttrMode::Idx16;
    const size_t nrm_extra = (fmt.nrm_idx3 && fmt.nrm.cnt && nrm_indexed)
                                 ? 2 * (fmt.nrm.mode == AttrMode::Idx16 ? 2 : 1) : 0;
    const int pos_n = fmt.pos.cnt ? 3 : 2, nrm_n = fmt.nrm.cnt ? 9 : 3, tex0_n = fmt.tex[0].cnt ? 2 : 1;
    // Echelle fixe des normales du GX : S8 = 1/64, S16 = 1/16384, quel que soit le format. Lues
    // brutes, l'eclairage renormalise, mais pas les generateurs NRM (placage d'environnement des
    // personnages : grain sur le t-shirt de Leon). NJ_NRMRAW=1 : brutes partout.
    static const bool nrm_raw = NJ_ENV("NJ_NRMRAW") != nullptr;
    const float nrm_k = nrm_raw ? 1.0f : fmt.nrm.comp == CompType::S8 ? 1.0f / 64.0f
                                       : fmt.nrm.comp == CompType::S16 ? 1.0f / 16384.0f : 1.0f;
    // Matrices GX : position 3x4, normale 3x3.
    auto xf_pos = [](const float* m, float* v) {
        const float x = v[0], y = v[1], z = v[2];
        v[0] = m[0]*x + m[1]*y + m[2]*z + m[3];
        v[1] = m[4]*x + m[5]*y + m[6]*z + m[7];
        v[2] = m[8]*x + m[9]*y + m[10]*z + m[11];
    };
    auto xf_nrm = [](const float* n, float* v) {
        const float a = v[0], b = v[1], cc = v[2];
        v[0] = n[0]*a + n[1]*b + n[2]*cc;
        v[1] = n[3]*a + n[4]*b + n[5]*cc;
        v[2] = n[6]*a + n[7]*b + n[8]*cc;
    };
    const size_t end = buf.size();
    size_t off = 0;
    c.vertices.reserve(size_t(r.count));
    for (uint32_t i = 0; i < r.count; ++i) {
        // Initialise : un format de position XY (menus, HUD, quads plein ecran) n'ecrit que pos[0] et pos[1] ;
        // pos[2] gardait ce que la pile contenait (0 ou une valeur absurde, selon l'execution), le cache de
        // decodage le conservait, et la passe de finition de r101 (z = 1e27) etait rejetee : filtre jaune (28/10).
        GxVertex v{};
        v.posMtxIdx = r.mtx_idx_pos;
        std::memcpy(v.texMtxIdx, r.mtx_idx_tex, 8);
        // index de matrices par sommet : position, puis chaque coordonnee de texture (os des
        // modeles animes, pour les generateurs NRM / POS)
        if (fmt.pnmtx_idx) {
            if (off >= end) return;
            v.posMtxIdx = buf[off++];
        }
        for (int t = 0; t < 8; ++t)
            if (fmt.tmtx_idx[t]) {
                if (off >= end) return;
                v.texMtxIdx[t] = buf[off++];
            }
        v.has_pos = fmt.pos.mode != AttrMode::None;
        if (!read_vector(buf, off, end, fmt.pos.mode, fmt.pos.comp, fmt.pos.frac, inv_pos, r.arr.base[0],
                            r.arr.stride[0], pos_n, v.pos, mem_base, mem_hi, trk, ov))
            return;
        // normale : decodee pour les dessins eclaires, sinon sautee (la position dans le flux doit
        // rester juste : une erreur ici ne se voit pas sur ce sommet, mais sur tous les suivants)
        if (fmt.nrm.mode != AttrMode::None && r.uses_normals) {
            v.has_nrm = true;
            float nbt[9];
            if (!read_vector(buf, off, end, fmt.nrm.mode, fmt.nrm.comp, 0, 1.0f, r.arr.base[1],
                                r.arr.stride[1], nrm_n, nbt, mem_base, mem_hi, trk, ov))
                return;
            std::memcpy(v.norm, nbt, 3 * sizeof(float));
        } else {
            off += attr_bytes(fmt.nrm.mode, fmt.nrm.comp, nrm_n, false);
        }
        off += nrm_extra;
        // couleur 0 (la 1 n'est que sautee : le rendu n'utilise que la 0)
        if (fmt.clr[0].mode != AttrMode::None) {
            float col[4];
            if (!read_color(buf, off, end, fmt.clr[0].mode, fmt.clr[0].comp, r.arr.base[2], r.arr.stride[2],
                               col, trk, ov))
                return;
            v.has_color = true;
            std::memcpy(v.color, col, sizeof col);
        }
        off += attr_bytes(fmt.clr[1].mode, fmt.clr[1].comp, 1, true);
        // coordonnee de texture 0 (les 1-7 sautees)
        if (fmt.tex[0].mode != AttrMode::None) {
            v.has_tex[0] = true;
            if (!read_vector(buf, off, end, fmt.tex[0].mode, fmt.tex[0].comp, fmt.tex[0].frac, inv_tex0,
                                r.arr.base[4], r.arr.stride[4], tex0_n, v.tex[0], mem_base, mem_hi, trk, ov))
                return;
        }
        for (int t = 1; t < 8; ++t)
            off += attr_bytes(fmt.tex[t].mode, fmt.tex[t].comp, fmt.tex[t].cnt ? 2 : 1, false);
        if (off > end) return;

        // Transformation par la matrice prise a la position du dessin dans le flux (pas celle du
        // moment du rendu, deja reecrite : peaux melangees).
        std::memcpy(v.obj_pos, v.pos, sizeof v.obj_pos);
        std::memcpy(v.obj_norm, v.norm, sizeof v.obj_norm);
        for (float& n : v.obj_norm) n *= nrm_k;
        if (!r.mtx.pos_bank.empty() && v.has_pos) {
            // matrice par sommet (peau) : banque du rejeu si le dessin vient d'une liste en cache,
            // sinon celle figee dans le RawDraw
            const uint32_t pb = (uint32_t)v.posMtxIdx * 4, nb = 3u * (v.posMtxIdx & 31);
            const float* bank_pos = c.bank ? c.bank->pos : r.mtx.pos_bank.data();
            const float* bank_nrm = c.bank ? c.bank->nrm : (r.mtx.nrm_bank.empty() ? nullptr : r.mtx.nrm_bank.data());
            if (pb <= 256 - 12 && nb <= 96 - 9) {
                xf_pos(&bank_pos[pb], v.pos);
                if (v.has_nrm && bank_nrm) xf_nrm(&bank_nrm[nb], v.norm);
                v.pretransformed = true;
                ++g_prof_xf_bank;
            }
        } else if (r.mtx.single && v.has_pos) {
            // matrice unique : sur le nuanceur (le sommet ne change pas, le decodage ne depend
            // plus de la camera et se garde en cache), sinon ici
            if (!xfgpu_on()) {
                xf_pos(r.mtx.pos, v.pos);
                if (v.has_nrm) xf_nrm(r.mtx.nrm, v.norm);
            }
            v.pretransformed = true;
            ++g_prof_xf_single;
        } else {
            ++g_prof_xf_none;
        }
        c.vertices.push_back(v);
    }
    // Empreinte du contenu (geometrie residente) : seulement si aucun sommet n'a ete transforme
    // par le processeur (chemin nuanceur a matrice unique ou sans matrice) et hors copie volatile.
    // Champ par champ : les octets de bourrage de GxVertex ne sont pas initialises.
    c.geom_hash = 0;
    if (!c.vertices.empty() && r.mtx.pos_bank.empty() && !c.volatile_src &&
        (c.xf_gpu || !r.mtx.single)) {
        uint64_t h = 0x9E3779B97F4A7C15ull ^ ((uint64_t)c.prim_type << 32) ^ c.vertices.size();
        auto mixw = [&h](uint64_t w) {
            h ^= w + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
            h *= 0xFF51AFD7ED558CCDull;
        };
        auto mixf = [&](const float* f, int n) {
            for (int k = 0; k + 1 < n; k += 2) {
                uint64_t w;
                std::memcpy(&w, f + k, 8);
                mixw(w);
            }
            if (n & 1) {
                uint32_t w;
                std::memcpy(&w, f + n - 1, 4);
                mixw(w);
            }
        };
        bool ok = true;
        for (const auto& v : c.vertices) {
            if (!v.pretransformed) { ok = false; break; }
            mixf(v.pos, 3);
            mixf(v.norm, 3);
            mixf(v.color, 4);
            mixf(v.tex[0], 2);
            mixf(v.obj_pos, 3);
            mixf(v.obj_norm, 3);
            uint64_t idx;
            std::memcpy(&idx, v.texMtxIdx, 8);
            mixw(idx);
            mixw((uint64_t)v.posMtxIdx | ((uint64_t)v.has_pos << 8) | ((uint64_t)v.has_nrm << 9) |
                 ((uint64_t)v.has_color << 10) | ((uint64_t)v.has_tex[0] << 11));
        }
        c.geom_hash = ok ? (h | 1) : 0;
    }
    if (trk && !track.bad && c.geom_hash && c.vertices.size() == r.count) {
        auto e = std::make_shared<deccache::Entry>();
        e->bytes = *r.bytes;
        e->bytes_src = r.bytes;
        {
            size_t total = 0;
            for (int i = 0; i < track.n; ++i) total += track.hi[i] - track.lo[i];
            e->nsp = (uint32_t)track.n;
            e->sp.reset(new uint8_t[(size_t)track.n * sizeof(deccache::SpanH) + total]);
            deccache::SpanH* hh = reinterpret_cast<deccache::SpanH*>(e->sp.get());
            uint8_t* dd = e->sp.get() + (size_t)track.n * sizeof(deccache::SpanH);
            uint32_t off = 0;
            for (int i = 0; i < track.n; ++i) {
                const uint32_t len = track.hi[i] - track.lo[i];
                hh[i] = {track.lo[i], len, off};
                std::memcpy(dd + off, mem_base + track.lo[i], len);
                off += len;
            }
        }
        e->verts = std::make_shared<const std::vector<GxVertex>>(c.vertices);
        e->geom_hash = c.geom_hash;
        std::unique_lock<NjSharedMutex> lk(deccache::mtx);
        if (deccache::map.size() > 40000) {
            for (auto& kv : deccache::map) deccache::retire(std::move(kv.second));
            deccache::map.clear();
        }
        e->dkey = dkey;
        e->serial = deccache::next_serial++;
        e->slot = e->serial & 0xFFFF;
        const deccache::Entry* raw_e = e.get();
        auto& place = deccache::map[dkey];
        if (place) deccache::retire(std::move(place));
        place = std::move(e);
        deccache::slots[raw_e->slot].store(raw_e, std::memory_order_release);
        r.dc_ref.v.store(((uint64_t)raw_e->serial << 16) | raw_e->slot, std::memory_order_relaxed);
    }
    if (vc && c.vertices.size() == r.count) {
        // Borne memoire : une scene chargee tient dans quelques milliers
        // d'entrees ; au-dela on vide plutot que de grossir sans fin.
        std::unique_lock<NjSharedMutex> lk(s_vtx_mutex);
        if (s_vtx_cache.size() > 8192)
            s_vtx_cache.clear();
        s_vtx_cache[vkey] = VtxCacheEntry{c.vertices, ++s_vtx_clock};
    }
    c.raw.reset();   // decode : la copie brute ne sert plus
}

// ---------------------------------------------------------------- cliche d'une liste d'affichage
//
// Une liste en cache rejoue ses ~40 dessins a chaque appel, et chacun repasse par DecodeDraw : reconnaitre l'entree du
// cache de decodage, relire ~900 octets de la memoire du jeu, copier le pointeur des sommets (~300 ns par dessin, un
// bon tiers du temps du fil de lecture au village, NJ_SPANSTAT du 16/10). Mais ce que ces dessins lisent est presque
// partout les memes quelques Ko (les tableaux du modele) : on retient, par liste, les sommets de tous ses dessins
// reconnus et UNE copie de l'union de leurs zones. Au rappel, si l'union est inchangee en memoire, les sommets de
// tous ces dessins sont ceux de la derniere fois (ils ne dependent que de leurs octets, de leur format et de ces
// zones) : on les pose d'un coup, sans appeler DecodeDraw. Une zone qui change invalide le cliche, et chaque dessin
// reprend le chemin normal. NJ_NOLISTSNAP=1 : coupe ; NJ_SNAPCHECK=1 : compare chaque dessin au chemin normal.
struct ListSnap {
    struct Range { uint32_t pa, len, off; };
    std::vector<Range> ranges;
    std::vector<uint8_t> data;
    std::vector<uint32_t> idx;                                       // dessins couverts (rang dans la liste)
    std::vector<std::shared_ptr<const std::vector<GxVertex>>> verts;
    std::vector<uint64_t> geom;
    uint64_t total_verts = 0;
};

static bool entry_valid_now(const deccache::Entry& en, const RawDraw& r, const uint8_t* mem_base, uint32_t mem_hi) {
    bool same = en.bytes_src == r.bytes ||
                (en.bytes.size() == r.bytes->size() && std::memcmp(en.bytes.data(), r.bytes->data(), en.bytes.size()) == 0);
    if (same && en.nsp) {
        const deccache::SpanH* h = en.hdr();
        const uint8_t* d = en.span_data();
        for (uint32_t i = 0; same && i < en.nsp; ++i)
            same = (size_t)h[i].pa + h[i].len <= (size_t)mem_hi && std::memcmp(mem_base + h[i].pa, d + h[i].off, h[i].len) == 0;
    }
    return same;
}

static std::atomic<uint64_t> g_snap_hit{0}, g_snap_draws{0}, g_snap_build{0}, g_snap_invalid{0}, g_snap_bad{0};
std::atomic<uint64_t>& g_snap_build_ext() { return g_snap_build; }
std::atomic<uint64_t>& g_snap_hit_ext() { return g_snap_hit; }
std::atomic<uint64_t>& g_snap_draws_ext() { return g_snap_draws; }
std::atomic<uint64_t>& g_snap_invalid_ext() { return g_snap_invalid; }
std::atomic<uint64_t>& g_snap_bad_ext() { return g_snap_bad; }

static void list_snap_apply(CachedList& L, std::vector<GxOp>& commands, size_t first) {
    static const bool off_env = NJ_ENV("NJ_NOLISTSNAP") != nullptr;
    // Avec NJ_PULL, les cliches restent : une partie statique deja residente ne coute rien ; seuls les dessins sans
    // cliche valide (peaux, zones changees) sont tires (S5 13/11 : cliches coupes = 7,5 i/s au lieu de 16 a l'intro).
    static const bool pull_nosnap = NJ_ENV("NJ_PULL_NOSNAP") != nullptr;
    const bool off = off_env || (pull_nosnap && PullOn()) || (nj::runtime::gfx::g_compat_mask & 8) != 0 || (knobs_live() && nj::dev::knob("nosnap", 0) != 0);
    static const bool check = NJ_ENV("NJ_SNAPCHECK") != nullptr;
    const bool fast = ncp::mode() == 2;   // cliche construit par le chargeur rapide (plan rendu natif, E3)
    if (off || (!deccache::on() && !fast) || !nj::runtime::g_guest_mem) return;
    const auto& m = nj::runtime::g_guest_mem->mem1;
    if (m.size() <= 8) return;
    const uint8_t* mem_base = m.data();
    const uint32_t mem_hi = (uint32_t)(m.size() - 8);
    if (first + L.cmds.size() > commands.size()) return;
    ListSnap* S = L.snap.get();
    if (S) {
        for (const auto& r : S->ranges)
            if ((size_t)r.pa + r.len > mem_hi || std::memcmp(mem_base + r.pa, S->data.data() + r.off, r.len) != 0) {
                L.snap.reset();   // la memoire a bouge : reconstruit plus tard (de plus en plus tard si ca recommence)
                L.cool = 4u << std::min<uint32_t>(++L.strikes, 6u);
                S = nullptr;
                g_snap_invalid.fetch_add(1, std::memory_order_relaxed);
                break;
            }
    }
    if (!S) {
        if (++L.hits < 2) return;
        if (L.cool) { --L.cool; return; }
        // Construction : seulement des dessins dont l'entree du cache de decodage est valide MAINTENANT.
        auto snap = std::make_shared<ListSnap>();
        std::vector<std::pair<uint32_t, uint32_t>> rs;
        for (size_t i = 0; i < L.cmds.size(); ++i) {
            const GxOp& o = L.cmds[i];
            if (o.type != GxOpKind::DrawPrimitive || !o.raw || !o.raw->bytes) continue;
            const RawDraw& r = *o.raw;
            if (!r.overlays.empty() || !r.mtx.pos_bank.empty() || r.count == 0) continue;
            if (fast) {
                if (!ncp::eligible(r)) continue;
                std::vector<std::pair<uint32_t, uint32_t>> rr;
                if (!ncp::ranges(r, rr)) continue;
                auto v = std::make_shared<std::vector<GxVertex>>();
                if (!ncp::decode(r, *v)) continue;
                snap->idx.push_back((uint32_t)i);
                snap->geom.push_back((xfgpu_on() && r.mtx.single) ? ncp::geom_hash((uint8_t)o.prim_type, *v) : 0);
                snap->verts.push_back(std::move(v));
                snap->total_verts += r.count;
                rs.insert(rs.end(), rr.begin(), rr.end());
                continue;
            }
            const bool xfg = xfgpu_on() && r.mtx.single;
            if (!(xfg || !r.mtx.single)) continue;
            const uint64_t ref = r.dc_ref.v.load(std::memory_order_relaxed);
            if (!ref) continue;
            const deccache::Entry* en = deccache::slots[ref & 0xFFFF].load(std::memory_order_acquire);
            if (!en || en->serial != (uint32_t)(ref >> 16) || !en->verts) continue;
            if (r.dc_key == 0 || en->dkey != r.dc_key || (r.dc_key & 0x1FF) != (((uint64_t)o.prim_type << 1) | (uint64_t)xfg)) continue;
            if (!entry_valid_now(*en, r, mem_base, mem_hi)) continue;
            snap->idx.push_back((uint32_t)i);
            snap->verts.push_back(en->verts);
            snap->geom.push_back(en->geom_hash);
            snap->total_verts += r.count;
            for (uint32_t k = 0; k < en->nsp; ++k) rs.emplace_back(en->hdr()[k].pa, en->hdr()[k].pa + en->hdr()[k].len);
        }
        if (snap->idx.empty()) return;
        if (!rs.empty()) {   // des dessins sans tableau indexe n'ont aucune zone a surveiller
            std::sort(rs.begin(), rs.end());
            uint32_t cs = rs[0].first, ce = rs[0].second;
            auto flush = [&](uint32_t a, uint32_t b) {
                snap->ranges.push_back({a, b - a, (uint32_t)snap->data.size()});
                snap->data.insert(snap->data.end(), mem_base + a, mem_base + b);
            };
            for (size_t i = 1; i < rs.size(); ++i) {
                if (rs[i].first <= ce) ce = std::max(ce, rs[i].second);
                else { flush(cs, ce); cs = rs[i].first; ce = rs[i].second; }
            }
            flush(cs, ce);
        }
        L.snap = snap;
        S = snap.get();
        g_snap_build.fetch_add(1, std::memory_order_relaxed);
    }
    // Pose des sommets : memes gestes que le chemin « reconnu » de DecodeDraw.
    uint64_t done = 0;
    for (size_t k = 0; k < S->idx.size(); ++k) {
        GxOp& c = commands[first + S->idx[k]];
        const GxOp& o = L.cmds[S->idx[k]];
        if (c.type != GxOpKind::DrawPrimitive || c.raw.get() != o.raw.get() || !c.vertices.empty() || c.shared_vertices || c.bank)
            continue;   // rejoue autrement (matrices ou copies de zones propres) : chemin normal
        const RawDraw& r = *c.raw;
        if (!r.overlays.empty()) continue;
        if (check) {
            GxOp tmp = c;
            DecodeDraw(tmp);
            // Le chemin normal peut produire les memes sommets dans un autre vecteur (entree evincee, redecodee) :
            // meme empreinte du contenu (geom_hash : tous les champs lus par le rendu) et meme nombre de sommets = pareil.
            bool same = tmp.shared_vertices.get() == S->verts[k].get() ||
                        (tmp.geom_hash == S->geom[k] && tmp.geom_hash != 0 && tmp.verts().size() == S->verts[k]->size());
            if (fast) {
                const auto& a2 = tmp.verts();
                const auto& b3 = *S->verts[k];
                same = a2.size() == b3.size();
                for (size_t q2 = 0; same && q2 < a2.size(); ++q2) same = ncp::same(a2[q2], b3[q2]);
            }
            if (!same) {
                const uint64_t bad = g_snap_bad.fetch_add(1, std::memory_order_relaxed) + 1;
                if (bad <= 10) {
                    NJ_LOG << "[SNAPCHECK] ECART #" << bad << " dessin " << S->idx[k] << " de la liste : geom " << std::hex
                           << tmp.geom_hash << " / " << S->geom[k] << std::dec << ", sommets " << tmp.verts().size() << " / "
                           << S->verts[k]->size() << ", partage " << (tmp.shared_vertices ? 1 : 0) << std::endl;
                    if (bad <= 3) {
                        const auto& a = tmp.verts();
                        const auto& b2 = *S->verts[k];
                        for (size_t q = 0; q < a.size() && q < b2.size() && q < 4; ++q) {
                            auto f = [](const GxVertex& v) {
                                char s[400];
                                std::snprintf(s, sizeof s, "pos %.3f %.3f %.3f nrm %.3f %.3f %.3f col %.3f %.3f %.3f %.3f tex %.3f %.3f obj %.3f %.3f %.3f idx %u hasP%d N%d C%d T%d",
                                              v.pos[0], v.pos[1], v.pos[2], v.norm[0], v.norm[1], v.norm[2], v.color[0], v.color[1], v.color[2],
                                              v.color[3], v.tex[0][0], v.tex[0][1], v.obj_pos[0], v.obj_pos[1], v.obj_pos[2], (unsigned)v.posMtxIdx,
                                              (int)v.has_pos, (int)v.has_nrm, (int)v.has_color, (int)v.has_tex[0]);
                                return std::string(s);
                            };
                            NJ_LOG << "   normal  " << q << ": " << f(a[q]) << std::endl;
                            NJ_LOG << "   cliche  " << q << ": " << f(b2[q]) << std::endl;
                        }
                        NJ_LOG << "   dessin : prim " << (int)c.prim_type << " count " << r.count << " pos.mode " << (int)r.vat.pos.mode
                               << " arr0 " << std::hex << r.arr.base[0] << std::dec << " single " << (int)r.mtx.single << " nsp-snapshot "
                               << S->ranges.size() << " xf_from_replay " << (int)c.xf_from_replay << " dc_key " << std::hex << r.dc_key
                               << std::dec << std::endl;
                    }
                }
            }
        }
        if (xfgpu_on() && r.mtx.single && r.mtx.pos_bank.empty()) {
            c.xf_gpu = true;
            if (!c.xf_from_replay) {
                std::memcpy(c.xf_pos, r.mtx.pos, sizeof c.xf_pos);
                std::memcpy(c.xf_nrm, r.mtx.nrm, sizeof c.xf_nrm);
            }
        }
        c.shared_vertices = S->verts[k];
        c.geom_hash = S->geom[k];
        done += r.count;
    }
    g_prof_decoded_verts.store(g_prof_decoded_verts.load(std::memory_order_relaxed) + done, std::memory_order_relaxed);
    g_snap_hit.fetch_add(1, std::memory_order_relaxed);
    const uint64_t d = g_snap_draws.fetch_add(S->idx.size(), std::memory_order_relaxed) + S->idx.size();
    if (check && (g_snap_hit.load(std::memory_order_relaxed) & 0xFFFF) == 0)
        NJ_LOG << "[SNAPCHECK] " << g_snap_hit.load() << " listes, " << d << " dessins, " << g_snap_bad.load() << " ecarts, "
               << g_snap_build.load() << " cliches, " << g_snap_invalid.load() << " invalides" << std::endl;
}

void ParseFifo(std::vector<uint8_t>& fifo, std::vector<GxOp>& commands) {
    size_t offset = 0;
    parse_stream(fifo, offset, commands, 0);
    // ce qui est analyse sort du tampon ; une commande coupee attend la suite
    if (offset >= fifo.size()) {
        fifo.clear();
    } else if (offset > 0) {
        fifo.erase(fifo.begin(), fifo.begin() + offset);
    }
}

namespace {

// Commandes du flux GX (premier octet) et ce qu'elles occupent.
enum : uint8_t {
    kOpNop = 0x00, kOpLoadCP = 0x08, kOpLoadXF = 0x10, kOpCallList = 0x40, kOpInvalidateVtx = 0x48,
    kOpLoadBP = 0x61, kOpMerge = 0x78,   // 0x78 : notre marque, pas une commande du GX (voir plus bas)
};
inline bool is_load_indexed(uint8_t op) { return op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38; }
inline bool is_draw(uint8_t op) { return op >= 0x80 && op <= 0xBF; }

GxOp reg_command(GxOpKind type, uint32_t reg, uint32_t val) {
    GxOp c;
    c.type = type;
    c.reg = reg;
    c.val = val;
    return c;
}

// Fusion d'un registre a la position de la marque dans le flux : les fonctions GX natives ne
// possedent que quelques bits de GEN_MODE / CMODE0 / CP 0x30. Fusionner au moment de l'appel
// courait contre l'analyse avec une base en retard d'une image (nombre de generateurs, bits de
// tramage, index de matrices de texture ecrases).
void parse_merge(const uint8_t* p, std::vector<GxOp>& out) {
    const uint8_t kind = p[1], reg = p[2];
    const uint32_t mask = Read32(p + 3), val = Read32(p + 7);
    GxRegs& st = g_pstate;
    if (kind == 0) {
        const uint32_t merged = (st.bp[reg] & ~mask) | (val & mask);
        parse_bp(reg, merged);
        out.push_back(reg_command(GxOpKind::BPRegister, reg, merged));
    } else {
        const uint32_t merged = (st.cp[reg] & ~mask) | (val & mask);
        st.cp[reg] = merged;
        parse_cp(reg, merged);
        out.push_back(reg_command(GxOpKind::CPRegister, reg, merged));
    }
}

// Chargement XF par index : `count` mots lus dans le tableau 12-15 a l'index donne. La commande
// garde la marque (bit 31, tableau, nombre, index) qui permet au cache des listes de les relire.
// Lit `n` flottants d'un tableau indexe en preferant la copie volatile active (tampon que le jeu reecrit pour chaque
// modele : matrices d'os). Sans cela, l'analyse lit la memoire vivante, deja reecrite par le modele suivant, et le
// modele anime recoit de mauvaises matrices (surfaces etirees, 28/10). Faux : la zone n'est pas couverte.
static bool read_floats_volatile(uint32_t addr, int n, float* out) {
    if (g_volatile_active.empty()) return false;
    const uint64_t a = addr & 0x01FFFFFFu, len = (uint64_t)n * 4;
    for (const VolatileRange& v : g_volatile_active) {
        if (!v.bytes || a < v.addr || a + len > (uint64_t)v.addr + v.len || a + len - v.addr > v.bytes->size()) continue;
        const uint8_t* q = v.bytes->data() + (a - v.addr);
        for (int i = 0; i < n; ++i) {
            const uint32_t w = Read32(q + 4 * i);
            std::memcpy(&out[i], &w, 4);
        }
        return true;
    }
    return false;
}

void parse_load_indexed(uint8_t op, const uint8_t* p, std::vector<GxOp>& out) {
    if (!nj::runtime::g_guest_mem) return;
    const GxRegs& st = g_pstate;
    const uint32_t word = Read32(p + 1);
    const int array = 12 + (op - 0x20) / 8;
    const uint32_t index = word >> 16, xf_addr = word & 0xFFF;
    const int count = (int)((word >> 12) & 0xF) + 1;
    GxOp c;
    c.type = GxOpKind::XFRegister;
    c.reg = xf_addr;
    c.val = 0x80000000u | ((uint32_t)(array - 12) << 24) | ((uint32_t)count << 16) | (index & 0xFFFF);
    c.payload.resize(count);
    const uint32_t base = st.arr.base[array] + index * st.arr.stride[array];
    // recompile : copies volatiles d'abord ; coeur natif : memoire du jeu dans l'ordre de la machine
    if (g_native_arrays || !read_floats_volatile(base, count, c.payload.data()))
        for (int i = 0; i < count; ++i) c.payload[i] = xf_array_f32(base + 4 * i);
    if (xfshadow::same(xf_addr, c.payload, c.payload.size())) { ++xfshadow::skipped; return; }   // rien ne change
    xfshadow::set(xf_addr, c.payload, c.payload.size());
    if (is_matrix_xf(xf_addr)) shadow_matrix_write(xf_addr, c.payload);
    out.push_back(std::move(c));
}

// Ecriture XF directe : `n` mots (n = champ + 1) a partir de l'adresse.
void parse_load_xf(const uint8_t* p, uint16_t length, std::vector<GxOp>& out) {
    GxOp c;
    c.type = GxOpKind::XFRegister;
    c.reg = Read16(p + 3);
    c.payload.resize(length + 1);
    for (int i = 0; i <= length; ++i) {
        const uint32_t w = Read32(p + 5 + 4 * i);
        std::memcpy(&c.payload[i], &w, 4);
    }
    if (xfshadow::same(c.reg, c.payload, c.payload.size())) { ++xfshadow::skipped; return; }   // rien ne change
    xfshadow::set(c.reg, c.payload, c.payload.size());
    if (is_matrix_xf(c.reg)) shadow_matrix_write(c.reg, c.payload);
    out.push_back(std::move(c));
}

// Dessin : les octets des sommets sont copies, le decodage vient plus tard (seuls les dessins
// gardes sont decodes). La matrice du dessin est prise a sa position dans le flux.
// NJ_FMTSTAT=1 (plan rendu natif, E0) : formats de sommets reellement utilises, par image : dessins, sommets,
// en liste ou en direct. FmtStatDump() imprime et remet a zero (appele par le rejeu ou le coeur).
int g_fmt_depth = 0;
static bool fmtstat_on() { static const bool v = NJ_ENV("NJ_FMTSTAT") != nullptr; return v; }
struct FmtStatEnt { uint64_t draws = 0, verts = 0, list_draws = 0; };
static std::map<std::string, FmtStatEnt> s_fmtstat;
static const char* fs_mode(AttrMode m) { return m == AttrMode::None ? "-" : m == AttrMode::Direct ? "D" : m == AttrMode::Idx8 ? "I8" : "I16"; }
static const char* fs_comp(CompType c) { static const char* n[] = {"u8", "s8", "u16", "s16", "f32"}; return n[(int)c & 7]; }
static void fmtstat_note(const VtxFormat& v, uint16_t count) {
    char b[256];
    int o = std::snprintf(b, sizeof b, "%sP:%s %s%s/%u N:%s %s%s C0:%s C1:%s", v.pnmtx_idx ? "pn " : "", fs_mode(v.pos.mode),
                          fs_comp(v.pos.comp), v.pos.cnt ? "x3" : "x2", (unsigned)v.pos.frac, fs_mode(v.nrm.mode),
                          fs_comp(v.nrm.comp), v.nrm_idx3 ? " nbt3" : "", fs_mode(v.clr[0].mode), fs_mode(v.clr[1].mode));
    for (int t = 0; t < 8 && o < (int)sizeof b - 32; ++t)
        if (v.tex[t].mode != AttrMode::None || v.tmtx_idx[t])
            o += std::snprintf(b + o, sizeof b - o, " T%d:%s %s/%u%s", t, fs_mode(v.tex[t].mode), fs_comp(v.tex[t].comp),
                               (unsigned)v.tex[t].frac, v.tmtx_idx[t] ? " tm" : "");
    auto& e = s_fmtstat[b];
    ++e.draws;
    e.verts += count;
    if (g_fmt_depth > 0) ++e.list_draws;
}
void FmtStatDump(unsigned frame) {
    ncp::dump(frame);
    DlStatDump(frame);
    if (!fmtstat_on()) return;
    std::printf("[PULL] image %u : %llu listes compilees appelees (%llu dessins), %llu dessins tires (%llu octets copies), "
                "actif=%d xfgpu=%d tableaux_natifs=%d\n", frame,
                (unsigned long long)g_pull_lists.exchange(0), (unsigned long long)g_pull_list_draws.exchange(0),
                (unsigned long long)g_pull_draws.exchange(0), (unsigned long long)g_pull_bytes.exchange(0), (int)PullOn(),
                (int)xfgpu_on(), (int)g_native_arrays);
    std::printf("[PULL]   refus : pas un dessin %llu, format non eligible %llu, matrices multiples %llu, format/etat different %llu, "
                "lignes/points %llu ; appels refuses %llu\n", (unsigned long long)g_lm_rej[0].load(), (unsigned long long)g_lm_rej[1].load(),
                (unsigned long long)g_lm_rej[2].load(), (unsigned long long)g_lm_rej[3].load(), (unsigned long long)g_lm_rej[4].load(),
                (unsigned long long)g_lm_rej[5].exchange(0));
    uint64_t d = 0, v = 0;
    for (auto& kv : s_fmtstat) { d += kv.second.draws; v += kv.second.verts; }
    std::printf("[FMT] image %u : %zu formats, %llu dessins, %llu sommets\n", frame, s_fmtstat.size(), (unsigned long long)d, (unsigned long long)v);
    for (auto& kv : s_fmtstat)
        std::printf("[FMT]   %6llu dessins %7llu sommets (%llu en liste)  %s\n", (unsigned long long)kv.second.draws,
                  (unsigned long long)kv.second.verts, (unsigned long long)kv.second.list_draws, kv.first.c_str());
    s_fmtstat.clear();
}

void parse_draw(uint8_t op, const uint8_t* p, uint16_t count, size_t need, std::vector<GxOp>& out) {
    const GxRegs& st = g_pstate;
    const VtxFormat& vat = st.vat[op & 7];
    if (fmtstat_on()) fmtstat_note(vat, count);
    GxOp c;
    c.type = GxOpKind::DrawPrimitive;
    c.prim_type = op & 0xF8;
    auto raw = make_pooled_raw();
    raw->vat = vat;
    raw->mtx_idx_pos = st.mtx_idx_pos;
    std::memcpy(raw->mtx_idx_tex, st.mtx_idx_tex, 8);
    if (vat.pnmtx_idx) {
        // matrice par sommet : toute la banque est figee (si quelque chose y a ete charge)
        if (std::any_of(s_shadow_pos, s_shadow_pos + 256, [](float f) { return f != 0.0f; })) {
            raw->mtx.pos_bank.assign(s_shadow_pos, s_shadow_pos + 256);
            raw->mtx.nrm_bank.assign(s_shadow_nrm, s_shadow_nrm + 96);
        }
    } else {
        // matrice courante, si son emplacement a ete charge (sinon celle du rendu)
        const uint32_t pb = (uint32_t)st.mtx_idx_pos * 4, nb = 3u * (st.mtx_idx_pos & 31);
        if (pb <= 256 - 12 && nb <= 96 - 9) {
            const float* pm = &s_shadow_pos[pb];
            if (std::any_of(pm, pm + 12, [](float f) { return f != 0.0f; })) {
                std::memcpy(raw->mtx.pos, pm, 12 * sizeof(float));
                std::memcpy(raw->mtx.nrm, &s_shadow_nrm[nb], 9 * sizeof(float));
                raw->mtx.single = true;
                xfstat_seq(pm);
            }
        }
    }
    // Normales toujours decodees quand le format en porte : l'etat XF des canaux de couleur n'est a
    // jour qu'au rendu (decider ici sur l'image precedente noircissait le village).
    raw->uses_normals = true;
    std::memcpy(raw->arr.base, st.arr.base, sizeof raw->arr.base);
    std::memcpy(raw->arr.stride, st.arr.stride, sizeof raw->arr.stride);
    CollectVolatile(*raw, raw->overlays);
    raw->count = count;
    raw->bytes = make_pooled_bytes(p + 3, p + need);
    if (nj::runtime::gfx::g_early_drawdone) {
        static const bool check_fp = NJ_ENV("NJ_CHECK_FP") != nullptr;
        if (check_fp) raw->vtx_fp = LiveFingerprint(*raw);
    }
    c.raw = std::move(raw);
    c.pull_frame = (uint32_t)g_fl_xfb_copies.load(std::memory_order_relaxed);
    ++g_prof_draws;
    g_pf_draw_bytes += need;
    out.push_back(std::move(c));
}

}  // namespace

static void parse_stream(const std::vector<uint8_t>& fifo, size_t& offset, std::vector<GxOp>& commands, int depth) {
    const size_t size = fifo.size();
    struct DepthScope { int prev; explicit DepthScope(int d) : prev(g_fmt_depth) { g_fmt_depth = d; } ~DepthScope() { g_fmt_depth = prev; } } fmt_depth(depth);
    while (offset < size) {
        // Les listes d'affichage s'executent a leur point d'appel : seul le flux principal fait
        // avancer les copies de tampons.
        if (depth == 0 && !g_volatile_queue.empty()) ActivateVolatile(g_stream_base + offset);
        const uint8_t op = fifo[offset];
        const uint8_t* p = fifo.data() + offset;
        const size_t left = size - offset;
        size_t used;
        if (op == kOpNop || op == kOpInvalidateVtx) {
            used = 1;
        } else if (op == kOpCallList) {
            if (left < 9) break;
            expand_list(Read32(p + 1), Read32(p + 5), commands, depth);
            used = 9;
        } else if (op == kOpMerge) {
            if (left < 11) break;
            parse_merge(p, commands);
            used = 11;
        } else if (is_load_indexed(op)) {
            if (depth > 0) ++g_prof_dl_loadindx;
            if (left < 5) break;
            parse_load_indexed(op, p, commands);
            used = 5;
        } else if (op == kOpLoadBP) {
            if (left < 5) break;
            const uint8_t reg = p[1];
            const uint32_t val = Read24(p + 2);
            // Ecriture BP redondante filtree a l'analyse (27/10) : registre de pur etat (meme liste que
            // bp_noop du rendu), deja ecrit au moins une fois dans le flux, valeur identique. Le fil hote,
            // sature au Galaxy S5, n'a plus a la parcourir. NJ_BPKEEP=1 : on garde tout, comme avant.
            static const bool s_bp_keep = NJ_ENV("NJ_BPKEEP") != nullptr || NJ_ENV("NJ_NOSEED") != nullptr;
            static bool s_bp_seen[256];
            const bool pure = reg == 0x00 || (reg >= 0x06 && reg <= 0x0E) || (reg >= 0x10 && reg <= 0x44) ||
                              (reg >= 0x59 && reg <= 0x5A) || (reg >= 0x80 && reg <= 0xBB) ||
                              (reg >= 0xC0 && reg <= 0xDF) || (reg >= 0xE8 && reg <= 0xF5);
            const bool redundant = !s_bp_keep && pure && s_bp_seen[reg] && g_pstate.bp[reg] == val;
            s_bp_seen[reg] = true;
            parse_bp(reg, val);
            // copie EFB -> XFB : une par image du jeu, comptee ici (le rendu ne tourne pas sans tete)
            if (reg == 0x52 && (val & 0x4000)) g_fl_xfb_copies.fetch_add(1, std::memory_order_relaxed);
            if (!redundant) commands.push_back(reg_command(GxOpKind::BPRegister, reg, val));
            used = 5;
        } else if (op == kOpLoadCP) {
            if (left < 6) break;
            const uint8_t reg = p[1];
            const uint32_t val = Read32(p + 2);
            parse_cp(reg, val);
            commands.push_back(reg_command(GxOpKind::CPRegister, reg, val));
            used = 6;
        } else if (op == kOpLoadXF) {
            if (left < 5) break;
            const uint16_t length = Read16(p + 1);
            used = 5 + ((size_t)length + 1) * 4;
            if (left < used) break;
            parse_load_xf(p, length, commands);
        } else if (is_draw(op)) {
            if (left < 3) break;
            const uint16_t count = Read16(p + 1);
            // taille d'un sommet constante pour le dessin : la longueur est connue sans decoder
            used = 3 + (size_t)count * vertex_size(g_pstate.vat[op & 7]);
            if (left < used) break;   // incomplet : attendre la suite
            const auto t0 = gxprof_now();
            parse_draw(op, p, count, used, commands);
            g_prof_snap_us += gxprof_us_since(t0);
        } else {
            ++g_prof_unknown;
            used = 1;
        }
        if (depth == 0 && (nj::runtime::gx::rec::active() || nj::runtime::gx::rec::armed())) {
            // Enregistreur .dff (fifo_record.h) : octets bruts du flux principal, avant tout filtrage.
            const bool xfb = op == kOpLoadBP && p[1] == 0x52 && (Read24(p + 2) & 0x4000);
            if (xfb && nj::runtime::gx::rec::armed()) {
                // debut : etat des registres juste apres cette copie (elle-meme n'est pas gardee)
                static uint32_t xfmem[4096], xfregs[88];
                std::memset(xfmem, 0, sizeof xfmem);
                std::memset(xfregs, 0, sizeof xfregs);
                std::memcpy(xfmem, s_shadow_pos, sizeof s_shadow_pos);
                std::memcpy(xfmem + 0x400, s_shadow_nrm, sizeof s_shadow_nrm);
                std::memcpy(xfregs, g_pstate.xf.data(), sizeof xfregs);
                nj::runtime::gx::rec::begin(g_pstate.bp.data(), g_pstate.cp.data(), xfmem, xfregs);
            } else {
                nj::runtime::gx::rec::on_command(p, used, xfb);
            }
        }
        offset += used;
    }
}

}

// Statistique des formats (NJ_FMTSTAT) pour le rejeu (fifo_replay.cpp).
void FmtStatDump_ext(unsigned f) { nj::runtime::gx::FmtStatDump(f); }
