// Lectures de l'EFB par le processeur : voir runtime/gx/efb_peek.h.
#include "runtime/gx/efb_peek.h"
#include "runtime/log.h"

#include "runtime/env.h"

#include <atomic>
#include <cstdio>
#include <map>
#include <mutex>
#include <vector>

namespace nj::runtime::gx::peek {
namespace {

struct Pt {
    uint16_t x, y;
    bool operator<(const Pt& o) const { return x != o.x ? x < o.x : y < o.y; }
};

std::mutex g_mu;
// Rang courant, cote analyse (lu par le jeu) et cote rendu.
std::atomic<int> g_parse_rank{0};
int g_render_rank = 0;
// Points demandes par le jeu, par rang : image en cours et image precedente (publiee au rendu).
std::map<int, std::vector<Pt>> g_req_cur, g_req_pub;
// Profondeurs mesurees (z 24 bits GX), par rang puis par point : derniere mesure.
std::map<int, std::map<Pt, uint32_t>> g_depth;
// Mesure par point de la scene terminee (fin d'image) : point -> z.
std::map<Pt, uint32_t> g_frame_depth;

bool trace() {
    static const bool on = NJ_ENV("NJ_PEEKLOG") != nullptr;
    return on;
}

}  // namespace

uint32_t read(uint32_t paddr) {
    const uint16_t x = (uint16_t)((paddr >> 2) & 0x3FF);
    const uint16_t y = (uint16_t)((paddr >> 12) & 0x3FF);
    const uint32_t type = (paddr >> 22) & 3;
    if (type != 1)
        return 0;   // couleur (GXPeekARGB) : RE4 ne s'en sert pas
    static const bool off = NJ_ENV("NJ_NOPEEK") != nullptr;   // A/B : l'ancien 0 constant
    if (off)
        return 0;
    const int rank = g_parse_rank.load(std::memory_order_acquire);
    const Pt p{x, y};
    std::lock_guard<std::mutex> lk(g_mu);
    auto& req = g_req_cur[rank];
    if (req.size() < 256)
        req.push_back(p);
    uint32_t z = 0xFFFFFF;   // pas encore mesure : le fond (la source est visible)
    // Les tests de lumiere (lens flare, halos) passent apres la scene 3D. Leur rang de GXDrawDone
    // varie d'une image a l'autre (plusieurs effets, ordre changeant) : apparie par rang, le point
    // etait souvent mesure avant la scene (fond vide, « visible »). Salle 1-07, vue identique a
    // Dolphin : Dolphin cache le flare a chaque image (hideAlpha 0), nous le montrions par
    // intermittence (0 a 0,25). D'ou la mesure par point sur la scene terminee.
    if (!rank_mode()) {
        auto hit = g_frame_depth.find(p);
        if (hit != g_frame_depth.end()) {
            z = hit->second;
        } else if (!g_frame_depth.empty()) {
            int best = 32 * 32 + 1;   // point voisin mesure (le jeu deplace ses points avec la source)
            for (const auto& [q, v] : g_frame_depth) {
                const int d = (q.x - x) * (q.x - x) + (q.y - y) * (q.y - y);
                if (d < best) { best = d; z = v; }
            }
        }
    } else if (auto it = g_depth.find(rank); it != g_depth.end() && !it->second.empty()) {
        auto hit = it->second.find(p);
        if (hit != it->second.end()) {
            z = hit->second;
        } else {
            // Le point a bouge depuis la mesure : le plus proche mesure au meme rang.
            int best = 1 << 30;
            for (const auto& [q, v] : it->second) {
                const int d = (q.x - x) * (q.x - x) + (q.y - y) * (q.y - y);
                if (d < best) { best = d; z = v; }
            }
        }
    }
    if (trace()) {
        static int n = 0;
        if (n++ < 400)
            NJ_PRINTF("[Peek] rang %d (%u,%u) -> %06X\n", rank, x, y, z);
    }
    return z;
}

void parse_finish() { g_parse_rank.fetch_add(1, std::memory_order_acq_rel); }

void parse_xfb_copy() {
    std::lock_guard<std::mutex> lk(g_mu);
    g_parse_rank.store(0, std::memory_order_release);
    // Les demandes de l'image qui s'acheve deviennent celles que le rendu mesurera.
    if (!g_req_cur.empty())
        g_req_pub.swap(g_req_cur);
    else
        g_req_pub.clear();
    g_req_cur.clear();
}

// Profondeur [0, 1] -> z GX 24 bits. En float, 1,0 * 16777215 + 0,5 = 16777215,5 n'est pas
// representable (24 bits de mantisse) et s'arrondissait a 16777216 = 0x1000000, hors de la plage du
// registre (fond et ciel, releve [Peek] du 01/10) : calcul en double, borne a 0xFFFFFF.
static inline uint32_t to_z24(float d) {
    const double z = (double)d * 16777215.0 + 0.5;
    return z <= 0.0 ? 0u : (z >= 16777215.0 ? 0xFFFFFFu : (uint32_t)z);
}

void render_finish(const std::function<float(int, int)>& read_depth) {
    const int rank = ++g_render_rank;
    std::vector<Pt> pts;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_req_pub.find(rank);
        if (it == g_req_pub.end())
            return;
        pts = it->second;
    }
    std::map<Pt, uint32_t> out;
    for (const Pt& p : pts) {
        float d = read_depth(p.x, p.y);
        d = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d);
        out[p] = to_z24(d);
    }
    std::lock_guard<std::mutex> lk(g_mu);
    g_depth[rank] = std::move(out);
}

bool render_points(int& rank, std::vector<std::pair<int, int>>& pts) {
    rank = ++g_render_rank;
    pts.clear();
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_req_pub.find(rank);
    if (it == g_req_pub.end())
        return false;
    for (const Pt& p : it->second)
        pts.push_back({p.x, p.y});
    return !pts.empty();
}

bool rank_mode() {
    static const bool r = NJ_ENV("NJ_PEEKRANK") != nullptr;
    return r;
}

bool frame_points(std::vector<std::pair<int, int>>& pts) {
    pts.clear();
    std::lock_guard<std::mutex> lk(g_mu);
    std::map<Pt, bool> seen;
    for (const auto& [rank, v] : g_req_pub)
        for (const Pt& p : v)
            if (seen.size() < 256 && seen.emplace(p, true).second)
                pts.push_back({p.x, p.y});
    return !pts.empty();
}

void render_store(int rank, const std::vector<std::pair<int, int>>& pts, const std::vector<float>& depth) {
    if (rank < 0) {
        std::map<Pt, uint32_t> out;
        for (size_t i = 0; i < pts.size() && i < depth.size(); ++i) {
            float d = depth[i];
            d = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d);
            out[Pt{(uint16_t)pts[i].first, (uint16_t)pts[i].second}] = to_z24(d);
        }
        std::lock_guard<std::mutex> lk(g_mu);
        g_frame_depth = std::move(out);
        return;
    }
    std::map<Pt, uint32_t> out;
    for (size_t i = 0; i < pts.size() && i < depth.size(); ++i) {
        float d = depth[i];
        d = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d);
        out[Pt{(uint16_t)pts[i].first, (uint16_t)pts[i].second}] = to_z24(d);
    }
    std::lock_guard<std::mutex> lk(g_mu);
    g_depth[rank] = std::move(out);
}

void render_xfb_copy() { g_render_rank = 0; }

}  // namespace nj::runtime::gx::peek

// Point d'entree de la MMU (cpu_context.h), hors espace de noms comme les autres crochets.
uint32_t nj_efb_peek_read(uint32_t paddr) { return nj::runtime::gx::peek::read(paddr); }
