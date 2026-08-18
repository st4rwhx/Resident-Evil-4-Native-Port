// Images intermediaires : la logique du jeu reste a 30 i/s, le rendu en fabrique d'autres entre
// deux images du jeu.
//
// Pourquoi pas la logique a 60 ou plus : le moteur a bien un pas de temps (pG+0x70, pose a 1,0
// par gameInit, et les animations le suivent : mesure a 0,5 -> images de 0,5), mais une grande
// partie du jeu compte en images - coffres, tiroirs, chutes d'objets, QTE. L'UHD, qui tourne
// sur ce meme moteur, a du etre corrige un endroit apres l'autre par re4_tweaks, qui refuse
// encore tout ce qui n'est pas 30 ou 60. Ici le jeu ne voit jamais que 30 : rien ne casse.
//
// Comment (refait le 28/10, apres la comparaison avec l'etude de CubeEvil4) : chaque image du jeu arrive
// decodee (commandes GX, sommets, matrice de chaque dessin). On garde l'image precedente A, on apparie
// ses dessins a ceux de la nouvelle B, puis on dessine UNE image intermediaire a mi-chemin (t = 0,5) des
// l'arrivee de B, et B elle-meme une demi-periode plus tard. Dans l'image intermediaire :
//   - la scene 3D seulement : tout dessin en projection orthographique (interface, menus, textes) reste
//     exactement comme dans B ;
//   - un dessin est reconnu par sa forme ET l'etat du nuanceur et les textures qu'il utilise, puis apparie
//     par distance (le premier candidat libre assez proche, sinon le plus proche des huit premiers) ;
//   - la matrice modele-vue de chaque dessin (la camera y est comprise), ses normales, les sommets deja
//     transformes ou calcules par le CPU (peaux de Leon et des ennemis), la projection (zoom de la visee) ;
//   - les lumieres, deplacees comme la camera s'est deplacee (pas melangees en ligne droite : elles
//     nageaient) ;
//   - toute l'image est refusee quand elle n'est pas fiable : moins de la moitie des dessins retrouves, ou
//     plus de 30 % tournes de plus de 20 degres (coupure de camera), ou rien en 3D.
// Le rendu repart de l'etat d'avant l'image (g_gxregs sauvegarde puis restaure) : redessiner B depuis l'etat
// de sa propre fin donnait a ses premiers dessins les valeurs de ses derniers.
// Le prix : une image de retard, comme toute interpolation.
#include "runtime/gx/frame_interp.h"
#include "runtime/dev_tour.h"
#include "runtime/obf.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire
#include "runtime/gx/fifo_parser.h"
#include "runtime/gfx_settings.h"
#include "runtime/gx_state.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "runtime/env.h"
#include "runtime/njprof.h"

namespace nj::runtime::gx {
// NJ_FRAMELOG (gx.cpp) : images intermediaires rendues et leur t, et refus de la garde
// (jeu sous 28,5 i/s, ou budget : ecoule + rendu + analyse depasse 95 % de la periode) ; coupures refusees.
uint64_t g_fi_renders = 0, g_fi_tsum_milli = 0, g_fi_refuse_period = 0, g_fi_refuse_budget = 0, g_fi_cuts = 0;
namespace {

using Clock = std::chrono::steady_clock;

std::vector<GxOp> s_a, s_b;      // image precedente, image courante
Clock::time_point s_b_at{};           // arrivee de B
double s_period = 1.0 / 30.0;         // intervalle mesure entre deux images du jeu
bool s_have_a = false;
bool s_b_exact = true;                // B est-il en place tel quel (valeurs d'origine) ?
bool s_pending_final = false;         // l'image intermediaire est montree : B reste a dessiner
Clock::time_point s_final_at{};       // heure prevue de B
double s_render_s = 0.015;            // cout mesure d'un rendu d'image (moyenne glissante)
double s_parse_s = 0.010;             // cout mesure de l'analyse d'une image du jeu
std::unique_ptr<GxRegs> s_snap;       // etat du rendu avant B (= fin de A)

// NJ_FI_TEST=1 (banc FifoReplay) : cadence et budget figes, pour que deux rejeux donnent les memes images.
bool test_mode() {
    static const bool on = NJ_ENV("NJ_FI_TEST") != nullptr;
    return on;
}

// Le jeu attend que l'hote ait lu son image pour continuer : une image intermediaire qui
// deborde sur l'arrivee de la suivante le ralentit. On n'en dessine une que si elle tient,
// avec l'analyse de la suivante, avant l'heure prevue de cette arrivee.
// Et seulement si le jeu tient deja ses 30 i/s : en dessous, c'est l'hote qui le freine, et
// chaque image de plus le freinerait davantage (mesure : 27 -> 15 i/s a 144 sans ce garde).
bool affordable(double elapsed) {
    if (test_mode())
        return true;
    return s_period < 1.0 / 28.5 && elapsed + s_render_s + s_parse_s < s_period * 0.95;
}

void timed_render(FrameRenderer* r) {
    const auto t0 = Clock::now();
    r->draw_frame(s_b);
    const double d = std::chrono::duration<double>(Clock::now() - t0).count();
    s_render_s = s_render_s * 0.8 + d * 0.2;
}

// ---- Analyse d'une image : ce qui identifie chaque dessin --------------------------------------------------

struct DrawInfo {
    uint64_t sig;     // forme + etat du nuanceur + textures
    float t[3];       // translation de la matrice (espace vue) ; 0 sans matrice deportee
    bool mtx;         // matrice deportee sur le nuanceur (xf_gpu)
    bool persp;       // en projection perspective (la scene 3D)
    uint32_t idx;     // rang de la commande dans l'image
};
std::vector<DrawInfo> s_info_a, s_info_b;
std::vector<uint32_t> s_proj_a, s_proj_b;   // rangs des ecritures de projection en perspective
uint32_t s_bp[256];                  // registres BP tels que les laisse la derniere image analysee
bool s_ortho_carry = false;          // projection en vigueur a la fin de la derniere image analysee

// Registres qui font le programme du nuanceur et ses textures (meme ensemble que la cle des nuanceurs) :
// 0x00 (mode de generation), 0x10-0x1F et 0x27-0x2F (texgen, ordre TEV), 0x80-0xBF (reglages et images des
// textures), 0xC0-0xDF (etages TEV), 0xF1, 0xF3 (alpha), 0xF6-0xFD.
bool in_sig(uint32_t r) {
    return r == 0x00 || (r >= 0x10 && r <= 0x1F) || (r >= 0x27 && r <= 0x2F) || (r >= 0x80 && r <= 0xDF) ||
           r == 0xF1 || r == 0xF3 || (r >= 0xF6 && r <= 0xFD);
}

uint64_t mix64(uint64_t h, uint64_t v) { return (h ^ v) * 1099511628211ull; }

uint64_t state_hash() {
    uint64_t h = 1469598103934665603ull;
    for (uint32_t r = 0; r < 256; ++r)
        if (in_sig(r))
            h = mix64(h, s_bp[r]);
    return h;
}

uint64_t draw_shape_key(const GxOp& c) {
    uint64_t h = 1469598103934665603ull;
    h = mix64(h, c.prim_type);
    if (c.raw) {
        h = mix64(h, c.raw->count);
        h = mix64(h, c.raw->arr.base[0]);
        h = mix64(h, c.raw->arr.base[1]);
        h = mix64(h, c.raw->arr.base[2]);
        h = mix64(h, c.raw->mtx_idx_pos);
        h = mix64(h, c.raw->bytes ? c.raw->bytes->size() : 0);
        h = mix64(h, c.raw->mtx.pos_bank.empty() ? 0 : 1);
    } else {
        h = mix64(h, c.verts().size());
    }
    return h;
}

bool covers_projection_type(const GxOp& c) {
    return c.type == GxOpKind::XFRegister && c.reg <= 0x1026 && (int) c.reg + (int) c.payload.size() > 0x1026;
}

// Parcourt une image dans l'ordre et note chaque dessin avec le contexte (projection, nuanceur, textures)
// dans lequel il a ete trace. Les registres BP et la projection persistent d'une image a l'autre.
void analyze(const std::vector<GxOp>& f, std::vector<DrawInfo>& out, std::vector<uint32_t>& projs) {
    out.clear();
    projs.clear();
    out.reserve(f.size() / 8 + 16);
    bool ortho = s_ortho_carry;
    uint64_t sh = state_hash();
    bool dirty = false;
    for (uint32_t i = 0; i < (uint32_t) f.size(); ++i) {
        const GxOp& c = f[i];
        if (c.type == GxOpKind::BPRegister) {
            const uint32_t r = c.reg & 0xFF;
            s_bp[r] = c.val;
            if (in_sig(r))
                dirty = true;
        } else if (c.type == GxOpKind::XFRegister) {
            if (covers_projection_type(c)) {
                uint32_t ty;
                std::memcpy(&ty, &c.payload[0x1026 - c.reg], 4);
                ortho = ty != 0;
                if (!ortho && c.reg <= 0x1020)
                    projs.push_back(i);
            }
        } else if (c.type == GxOpKind::DrawPrimitive) {
            if (dirty) {
                sh = state_hash();
                dirty = false;
            }
            DrawInfo d{};
            d.sig = mix64(draw_shape_key(c), sh);
            d.mtx = c.xf_gpu;
            d.persp = !ortho;
            if (c.xf_gpu) {
                d.t[0] = c.xf_pos[3];
                d.t[1] = c.xf_pos[7];
                d.t[2] = c.xf_pos[11];
            }
            d.idx = i;
            out.push_back(d);
        }
    }
    s_ortho_carry = ortho;
}

// ---- Ce qu'on melange -----------------------------------------------------------------------------------------

struct MtxSlot {
    GxOp* b;
    const float* a_pos;
    const float* a_nrm;
    float orig_pos[12];    // ceux de B
    float orig_nrm[9];
};
struct VtxSlot {
    GxOp* b;
    const GxOp* a;
    std::vector<float> orig;   // positions (et normales) de B, 3 ou 6 par sommet
    bool norm;
};
struct ProjSlot {
    GxOp* b;
    const GxOp* a;
    std::vector<float> orig;
    uint16_t lo, hi;           // mots de la projection a melanger
};
struct LightSlot {
    GxOp* b;
    std::vector<float> orig;
    float a_pos[12], b_pos[12];   // matrice du dessin de reference, avant et apres
};
std::vector<MtxSlot> s_mtx;
std::vector<VtxSlot> s_vtx;
std::vector<ProjSlot> s_proj;
std::vector<LightSlot> s_light;

// Au-dela, c'est un changement de plan ou un objet qui apparait : pas de melange.
// Unites du monde de RE4 : ~1 mm ; 2,5 m par image du jeu, c'est deja 270 km/h.
constexpr float kJump = 2500.0f;
// Un sommet qui bouge de plus de 1500 unites en une image du jeu n'est pas la meme figure.
constexpr float kFar = 1500.0f;

// Rotation entre deux matrices 3x4 : vrai si aucune des trois lignes n'a tourne de plus de ~20 degres
// (cosinus > 0,94). Au-dela, c'est une coupure de camera ou un objet qui change de plan : pas de melange.
bool rot_close(const float* a, const float* b) {
    for (int r = 0; r < 3; ++r) {
        const float* pa = a + r * 4;
        const float* pb = b + r * 4;
        const float la = std::sqrt(pa[0] * pa[0] + pa[1] * pa[1] + pa[2] * pa[2]);
        const float lb = std::sqrt(pb[0] * pb[0] + pb[1] * pb[1] + pb[2] * pb[2]);
        if (la > 0.0f && lb > 0.0f && (pa[0] * pb[0] + pa[1] * pb[1] + pa[2] * pb[2]) / (la * lb) < 0.94f)
            return false;
    }
    return true;
}

float dist3(const float* a, const float* b) {
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Inverse de la partie 3x3 d'une matrice 3x4 ; faux si elle est singuliere.
bool inverse3(const float* n, float inv[3][3]) {
    auto m = [&](int r, int c) { return n[r * 4 + c]; };
    const float det = m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) - m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) +
                      m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
    if (!(std::fabs(det) > 1e-12f))
        return false;
    inv[0][0] = (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) / det;
    inv[0][1] = (m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2)) / det;
    inv[0][2] = (m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1)) / det;
    inv[1][0] = (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2)) / det;
    inv[1][1] = (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0)) / det;
    inv[1][2] = (m(0, 2) * m(1, 0) - m(0, 0) * m(1, 2)) / det;
    inv[2][0] = (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0)) / det;
    inv[2][1] = (m(0, 1) * m(2, 0) - m(0, 0) * m(2, 1)) / det;
    inv[2][2] = (m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0)) / det;
    return true;
}

// Resultat de l'appariement : l'image est-elle fiable ?
struct Verdict {
    unsigned draws = 0, matched = 0, turned = 0;
    bool ok() const { return draws > 0 && matched * 2 >= draws && turned * 10 <= matched * 3; }
};

Verdict build_slots() {
    s_mtx.clear();
    s_vtx.clear();
    s_proj.clear();
    s_light.clear();
    Verdict v;
    if (!s_have_a || s_info_a.empty() || s_info_b.empty())
        return v;
    std::unordered_map<uint64_t, std::vector<uint32_t>> a_by_sig;
    for (uint32_t i = 0; i < (uint32_t) s_info_a.size(); ++i)
        if (s_info_a[i].persp)
            a_by_sig[s_info_a[i].sig].push_back(i);
    std::vector<uint8_t> used(s_info_a.size(), 0);
    struct Ref {
        uint32_t op;
        const float* a;
        const float* b;
    };
    std::vector<Ref> refs;   // dessins apparies avec matrice : repere du mouvement de la camera pour les lumieres
    for (const DrawInfo& db : s_info_b) {
        if (!db.persp)
            continue;   // 2D (interface, menus, textes) : tel que dans B
        ++v.draws;
        auto it = a_by_sig.find(db.sig);
        if (it == a_by_sig.end())
            continue;
        const float reach = db.mtx ? std::sqrt(db.t[0] * db.t[0] + db.t[1] * db.t[1] + db.t[2] * db.t[2]) : 0.0f;
        const float limit = 0.25f * reach + 400.0f;
        int best = -1;
        float best_d = 0.0f;
        unsigned looked = 0;
        for (uint32_t i : it->second) {
            if (used[i])
                continue;
            const DrawInfo& da = s_info_a[i];
            const float d = (db.mtx && da.mtx) ? dist3(da.t, db.t) : 0.0f;
            if (looked++ == 0 && d <= limit) {   // le jeu dessine dans le meme ordre d'une image a l'autre
                best = (int) i;
                break;
            }
            if (d <= limit && (best < 0 || d < best_d)) {
                best = (int) i;
                best_d = d;
            }
            if (looked >= 8)
                break;
        }
        if (best < 0)
            continue;
        used[best] = 1;
        ++v.matched;
        GxOp& c = s_b[db.idx];
        const GxOp& a = s_a[s_info_a[best].idx];
        if (c.xf_gpu && a.xf_gpu) {
            if (!rot_close(c.xf_pos, a.xf_pos)) {
                ++v.turned;
                continue;
            }
            refs.push_back({db.idx, a.xf_pos, c.xf_pos});
            if (std::memcmp(c.xf_pos, a.xf_pos, sizeof c.xf_pos) != 0 || std::memcmp(c.xf_nrm, a.xf_nrm, sizeof c.xf_nrm) != 0) {
                MtxSlot s{&c, a.xf_pos, a.xf_nrm, {}, {}};
                std::memcpy(s.orig_pos, c.xf_pos, sizeof s.orig_pos);
                std::memcpy(s.orig_nrm, c.xf_nrm, sizeof s.orig_nrm);
                s_mtx.push_back(s);
            }
        }
        // Sommets qui changent d'une image a l'autre : deja en espace vue, ou recalcules par le CPU
        // (peaux). Les autres sont identiques, inutile de les toucher.
        const size_t nv = c.verts().size();
        if (nv && nv == a.verts().size() && (c.volatile_src || c.verts()[0].pretransformed) && c.verts()[0].has_pos &&
            a.verts()[0].has_pos) {
            const bool norm = c.verts()[0].has_nrm && a.verts()[0].has_nrm;
            bool differs = false;
            float far = 0.0f;
            const auto& vb = c.verts();
            const auto& va = a.verts();
            for (size_t i = 0; i < nv; ++i) {
                for (int k = 0; k < 3; ++k) {
                    const float dd = std::fabs(vb[i].pos[k] - va[i].pos[k]);
                    if (dd != 0.0f)
                        differs = true;
                    far = std::max(far, dd);
                }
            }
            if (differs && far <= kFar) {
                c.own_vertices();   // modifies par apply() : copie privee
                VtxSlot s{&c, &a, {}, norm};
                const int w = norm ? 6 : 3;
                s.orig.resize(nv * w);
                for (size_t i = 0; i < nv; ++i) {
                    std::memcpy(&s.orig[i * w], c.vertices[i].pos, 12);
                    if (norm)
                        std::memcpy(&s.orig[i * w + 3], c.vertices[i].norm, 12);
                }
                s_vtx.push_back(std::move(s));
            }
        }
    }
    // Projection (zoom de la visee) : la i-eme ecriture en perspective de B avec la i-eme de A, si le type est le meme.
    for (size_t k = 0; k < s_proj_b.size() && k < s_proj_a.size(); ++k) {
        GxOp& c = s_b[s_proj_b[k]];
        const GxOp& a = s_a[s_proj_a[k]];
        if (c.payload.size() != a.payload.size() || c.reg != a.reg || c.reg > 0x1020)
            continue;
        const int lo = c.reg;
        uint32_t ta, tb;
        std::memcpy(&tb, &c.payload[0x1026 - lo], 4);
        std::memcpy(&ta, &a.payload[0x1026 - lo], 4);
        if (ta != tb)
            continue;
        if (std::memcmp(&c.payload[0x1020 - lo], &a.payload[0x1020 - lo], 6 * sizeof(float)) == 0)
            continue;
        ProjSlot s{&c, &a, {}, (uint16_t) (0x1020 - lo), (uint16_t) (0x1020 - lo + 6)};
        s.orig.assign(c.payload.begin(), c.payload.end());
        s_proj.push_back(std::move(s));
    }
    // Lumieres (position et direction, mots 10 a 15 de chaque bloc de 16, espace vue) : elles suivent le mouvement
    // de la camera tel que le montre le dessin apparie qui les suit.
    if (!refs.empty()) {
        for (uint32_t i = 0; i < (uint32_t) s_b.size(); ++i) {
            GxOp& c = s_b[i];
            if (c.type != GxOpKind::XFRegister || c.reg < 0x0600 || c.reg + c.payload.size() > 0x0680 || ((c.reg - 0x0600) % 16) != 0 ||
                c.payload.size() < 16)
                continue;
            const auto r = std::lower_bound(refs.begin(), refs.end(), i, [](const Ref& x, uint32_t op) { return x.op < op; });
            if (r == refs.end())
                continue;
            LightSlot s{&c, {}, {}, {}};
            std::memcpy(s.a_pos, r->a, sizeof s.a_pos);
            std::memcpy(s.b_pos, r->b, sizeof s.b_pos);
            s.orig.assign(c.payload.begin(), c.payload.end());
            s_light.push_back(std::move(s));
        }
    }
    return v;
}

void apply(float t) {
    const float u = 1.0f - t;
    for (auto& s : s_mtx) {
        for (int i = 0; i < 12; ++i)
            s.b->xf_pos[i] = s.a_pos[i] * u + s.orig_pos[i] * t;
        for (int i = 0; i < 9; ++i)
            s.b->xf_nrm[i] = s.a_nrm[i] * u + s.orig_nrm[i] * t;
    }
    for (auto& s : s_vtx) {
        const int w = s.norm ? 6 : 3;
        auto& vb = s.b->vertices;
        const auto& va = s.a->verts();
        for (size_t i = 0; i < vb.size(); ++i) {
            const float* o = &s.orig[i * w];
            for (int k = 0; k < 3; ++k)
                vb[i].pos[k] = va[i].pos[k] * u + o[k] * t;
            if (s.norm)
                for (int k = 0; k < 3; ++k)
                    vb[i].norm[k] = va[i].norm[k] * u + o[3 + k] * t;
        }
    }
    for (auto& s : s_proj)
        for (int i = s.lo; i < s.hi; ++i)
            s.b->payload[i] = s.a->payload[i] * u + s.orig[i] * t;
    // Lumieres : C = mid * inverse(B), ou mid est la matrice du dessin de reference melangee. Les positions prennent
    // toute la transformation, les directions sa rotation.
    for (auto& s : s_light) {
        float mid[12];
        for (int i = 0; i < 12; ++i)
            mid[i] = s.a_pos[i] * u + s.b_pos[i] * t;
        float inv[3][3];
        if (t >= 1.0f || !inverse3(s.b_pos, inv)) {
            for (size_t i = 0; i < s.orig.size(); ++i)
                s.b->payload[i] = s.orig[i];
            continue;
        }
        float rot[3][3], shift[3];
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                rot[r][c] = mid[r * 4 + 0] * inv[0][c] + mid[r * 4 + 1] * inv[1][c] + mid[r * 4 + 2] * inv[2][c];
        for (int r = 0; r < 3; ++r)
            shift[r] = mid[r * 4 + 3] - (rot[r][0] * s.b_pos[3] + rot[r][1] * s.b_pos[7] + rot[r][2] * s.b_pos[11]);
        for (size_t blk = 0; blk + 16 <= s.orig.size(); blk += 16) {
            const float* p = &s.orig[blk + 10];
            const float* d = &s.orig[blk + 13];
            for (int r = 0; r < 3; ++r) {
                s.b->payload[blk + 10 + r] = rot[r][0] * p[0] + rot[r][1] * p[1] + rot[r][2] * p[2] + shift[r];
                s.b->payload[blk + 13 + r] = rot[r][0] * d[0] + rot[r][1] * d[1] + rot[r][2] * d[2];
            }
        }
    }
    s_b_exact = (t >= 1.0f);
}

// Etat du rendu : B repart toujours de l'etat que la fin de A a laisse.
void snapshot_state() {
    if (!s_snap)
        s_snap = std::make_unique<GxRegs>();
    *s_snap = g_gxregs;
}

void restore_state() {
    if (!s_snap)
        return;
    const uint32_t gen = g_gxregs.xf_gen;
    g_gxregs = *s_snap;
    g_gxregs.xf_gen = gen + 1;   // les canaux et lumieres sont reexpedies au GPU
}

}  // namespace

bool interp_enabled() {
    const int f = nj::runtime::gfx::g_fps_target;
    return f == 0 || f > 30;
}

namespace {

// Recyclage des images rendues. Detruire les ~20 000 commandes d'une image (sommets partages,
// RawDraw, charges XF : liberations et compteurs atomiques) coutait ~25 % du fil hote au Galaxy S5
// (profil NJ_APROF du 29/09). Le vecteur plein part a un fil de nettoyage, qui le vide et le rend
// avec sa capacite ; le rendu repart d'un vecteur vide deja dimensionne. NJ_NORECYCLE=1 : vider
// sur place, comme avant.
std::mutex s_rc_m;
std::condition_variable s_rc_cv;
std::vector<std::vector<GxOp>> s_rc_dirty, s_rc_clean;

void recycle(std::vector<GxOp>& v) {
    const bool off = NJ_ENV("NJ_NORECYCLE") != nullptr || nj::dev::knob("norecycle", 0) != 0;
    if (off || v.empty()) {
        v.clear();
        return;
    }
    static const bool started = [] {
        std::thread([] {
            nj::runtime::njprof::register_thread("nettoyage"_g);
            for (;;) {
                std::vector<GxOp> d;
                {
                    std::unique_lock<std::mutex> lk(s_rc_m);
                    s_rc_cv.wait(lk, [] { return !s_rc_dirty.empty(); });
                    d.swap(s_rc_dirty.back());
                    s_rc_dirty.pop_back();
                }
                d.clear();   // les destructions, hors du fil hote
                std::lock_guard<std::mutex> lk(s_rc_m);
                if (s_rc_clean.size() < 3)
                    s_rc_clean.push_back(std::move(d));
            }
        }).detach();
        return true;
    }();
    (void) started;
    std::vector<GxOp> fresh;
    {
        std::lock_guard<std::mutex> lk(s_rc_m);
        if (!s_rc_clean.empty()) {
            fresh.swap(s_rc_clean.back());
            s_rc_clean.pop_back();
        }
        s_rc_dirty.push_back(std::move(v));
    }
    s_rc_cv.notify_one();
    v.swap(fresh);
}

// B exacte : l'etat d'avant l'image est remis, puis B est redessinee avec ses valeurs d'origine.
void render_final(FrameRenderer* r) {
    restore_state();
    apply(1.0f);
    s_pending_final = false;
    timed_render(r);
}

}  // namespace

void interp_new_frame(std::vector<GxOp>& frame, FrameRenderer* r, double parse_s) {
    const Clock::time_point now = Clock::now();
    if (parse_s > 0.0 && parse_s < 0.2)
        s_parse_s = s_parse_s * 0.8 + parse_s * 0.2;
    if (!s_b_exact)
        apply(1.0f);   // B retrouve ses valeurs avant de devenir A
    s_pending_final = false;
    if (s_have_a || !s_b.empty()) {
        const double dt = std::chrono::duration<double>(now - s_b_at).count();
        // Moyenne glissante, bornee : une pause (chargement) ne doit pas etirer la suivante.
        if (!test_mode() && dt > 0.005 && dt < 0.2)
            s_period = s_period * 0.8 + dt * 0.2;
    }
    s_a.swap(s_b);
    s_b.swap(frame);
    recycle(frame);
    s_have_a = !s_a.empty();
    s_b_at = now;
    s_info_a.swap(s_info_b);
    s_proj_a.swap(s_proj_b);
    const bool on = interp_enabled();
    if (on)
        analyze(s_b, s_info_b, s_proj_b);   // les registres BP suivis ne doivent pas rater d'image
    bool shown = false;
    // Une image intermediaire au moins doit tenir, sinon on montre B tel quel, sans retard.
    if (on && r && affordable(s_render_s) && s_have_a) {
        const Verdict v = build_slots();
        if (v.ok() && (!s_mtx.empty() || !s_vtx.empty() || !s_proj.empty() || !s_light.empty())) {
            snapshot_state();   // B repartira de la fin de A
            apply(0.5f);
            timed_render(r);
            s_pending_final = true;
            s_final_at = now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(s_period * 0.5));
            ++g_fi_renders;
            g_fi_tsum_milli += 500;
            shown = true;
        } else if (v.draws > 0 && !v.ok()) {
            ++g_fi_cuts;   // coupure de camera, objets nouveaux : image refusee
        }
    } else if (on) {
        if (!(s_period < 1.0 / 28.5))
            ++g_fi_refuse_period;
        else
            ++g_fi_refuse_budget;
    }
    if (!shown) {
        s_mtx.clear();
        s_vtx.clear();
        s_proj.clear();
        s_light.clear();
        s_b_exact = true;
        if (r)
            timed_render(r);
    }
}

bool interp_tick(FrameRenderer* r) {
    if (!s_pending_final || !r || s_b.empty())
        return false;
    if (Clock::now() < s_final_at && interp_enabled())
        return false;
    render_final(r);
    return true;
}

bool interp_test_final(FrameRenderer* r) {
    if (!s_pending_final || !r)
        return false;
    render_final(r);
    return true;
}

}  // namespace nj::runtime::gx
