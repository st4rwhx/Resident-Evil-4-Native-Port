// -*- coding: utf-8 -*-
// Mesure G1 (14/11) : avant d'ecrire le rendu au niveau du modele, combien de parties de modeles par image, combien
// de nuanceurs et de materiaux distincts, et le materiau d'une partie change-t-il d'une image a l'autre ?
// Repere : port_model_part, pose par commonModelTrans (src/game/trans.cpp) autour de son GXCallDisplayList.
// Inclus a la fin de port/gxn.cpp (etat type complet). Actif avec NJ_GXN=1. Ligne [G1] toutes les 300 images.
// Tables de hachage a taille fixe : les conteneurs standard ne compilent pas sous port_prelude.h.

namespace {

struct Fnv {
    uint64_t h = 1469598103934665603ull;
    template <class T> void add(const T& v) {
        const unsigned char* p = (const unsigned char*) &v;
        for (size_t i = 0; i < sizeof(T); ++i) h = (h ^ p[i]) * 1099511628211ull;
    }
};

// Structure du nuanceur : ce qui changerait le programme (combineurs, sources, echanges, comparaisons, generateurs,
// canaux, type de brouillard, indirect). Pas les valeurs (couleurs, references, matrices, textures).
uint64_t shader_key()
{
    Fnv f;
    f.add(P.n_stages);
    for (int i = 0; i < P.n_stages && i < 16; ++i) f.add(P.st[i]);
    f.add(P.swap);
    f.add(P.acmp0), f.add(P.aop), f.add(P.acmp1);
    f.add(P.fog_type);
    f.add(P.dst_alpha_en);
    f.add(S.n_texgens);
    for (int i = 0; i < S.n_texgens && i < 8; ++i) f.add(S.tg[i].func), f.add(S.tg[i].src), f.add(S.tg[i].normalize);
    f.add(L.n_chans);
    for (int i = 0; i < 4; ++i) f.add(L.ctrl[i]);
    f.add(I);
    for (int i = 0; i < 8; ++i) f.add(T.unit[i].fmt), f.add(T.unit[i].ci);
    return f.h;
}

// Materiau complet : structure + valeurs fixes de la partie (couleurs TEV, references alpha, melange, profondeur,
// textures et leurs reglages, couleurs ambiante et de matiere, face cachee). Hors matrices et lumieres (par image).
uint64_t material_key(uint64_t sk)
{
    Fnv f;
    f.add(sk);
    f.add(P.tev_reg), f.add(P.kcolor);
    f.add(P.aref0), f.add(P.aref1);
    f.add(P.blend_type), f.add(P.blend_src), f.add(P.blend_dst), f.add(P.logic_op);
    f.add(P.color_update), f.add(P.alpha_update), f.add(P.z_enable), f.add(P.z_func), f.add(P.z_update);
    f.add(P.dst_alpha);
    for (int i = 0; i < 8; ++i)
        if (T.unit_set[i]) f.add(T.unit[i]);
    f.add(L.amb), f.add(L.mat);
    f.add(S.cull);
    return f.h;
}

// Ensemble / table a adressage ouvert, taille fixe (0 = case vide ; une cle nulle est remplacee par 1).
template <unsigned N> struct HSet {
    uint64_t k[N];
    unsigned n;
    void clear() { memset(k, 0, sizeof k), n = 0; }
    void insert(uint64_t key) {
        if (!key) key = 1;
        for (unsigned i = (unsigned) (key % N), t = 0; t < N; i = (i + 1) % N, ++t) {
            if (k[i] == key) return;
            if (!k[i]) { k[i] = key, ++n; return; }
        }
    }
};
template <unsigned N> struct HMap {
    uint64_t k[N], v[N];
    unsigned n;
    // 0 : nouvelle cle ; 1 : meme valeur ; 2 : valeur changee (remplacee)
    int put(uint64_t key, uint64_t val) {
        if (!key) key = 1;
        for (unsigned i = (unsigned) (key % N), t = 0; t < N; i = (i + 1) % N, ++t) {
            if (k[i] == key) { const int r = v[i] == val ? 1 : 2; v[i] = val; return r; }
            if (!k[i]) { if (n * 4 >= N * 3) return 0; k[i] = key, v[i] = val, ++n; return 0; }
        }
        return 0;
    }
};

struct G1 {
    unsigned parts, frames, same, changed, fresh;
    HSet<8192> shaders, materials;
    HMap<32768> last;   // (liste << 32 | taille) -> materiau a la derniere rencontre
} g1;
unsigned g1_parts_this_frame = 0;

// Decomposition par poste (14/11) : qu'est-ce qui varie vraiment d'une image a l'autre ?
enum { K_TEV, K_ALPHA_FOG, K_TEXGEN, K_CHAN, K_LMASK, K_IND, K_TEXFMT, K_TEVCOL, K_TEX, K_MATCOL, K_PIXEL, K_N };
const char* const kKName[K_N] = {"tev", "alpha/brouillard", "generateurs", "canaux", "masque lumieres", "indirect",
                                 "formats tex", "couleurs tev", "textures", "couleurs matiere", "melange/z/face"};
void component_keys(uint64_t* k)
{
    Fnv f[K_N];
    f[K_TEV].add(P.n_stages);
    for (int i = 0; i < P.n_stages && i < 16; ++i) f[K_TEV].add(P.st[i]);
    f[K_TEV].add(P.swap);
    f[K_ALPHA_FOG].add(P.acmp0), f[K_ALPHA_FOG].add(P.aop), f[K_ALPHA_FOG].add(P.acmp1), f[K_ALPHA_FOG].add(P.fog_type),
        f[K_ALPHA_FOG].add(P.dst_alpha_en);
    f[K_TEXGEN].add(S.n_texgens);
    for (int i = 0; i < S.n_texgens && i < 8; ++i)
        f[K_TEXGEN].add(S.tg[i].func), f[K_TEXGEN].add(S.tg[i].src), f[K_TEXGEN].add(S.tg[i].normalize);
    f[K_CHAN].add(L.n_chans);
    for (int i = 0; i < 4; ++i)
        f[K_CHAN].add(L.ctrl[i].enable), f[K_CHAN].add(L.ctrl[i].amb_src), f[K_CHAN].add(L.ctrl[i].mat_src),
            f[K_CHAN].add(L.ctrl[i].diff_fn), f[K_CHAN].add(L.ctrl[i].attn_fn), f[K_LMASK].add(L.ctrl[i].light_mask);
    f[K_IND].add(I);
    for (int i = 0; i < 8; ++i) f[K_TEXFMT].add(T.unit[i].fmt), f[K_TEXFMT].add(T.unit[i].ci);
    f[K_TEVCOL].add(P.tev_reg), f[K_TEVCOL].add(P.kcolor), f[K_TEVCOL].add(P.aref0), f[K_TEVCOL].add(P.aref1);
    for (int i = 0; i < 8; ++i)
        if (T.unit_set[i]) f[K_TEX].add(T.unit[i]);
    f[K_MATCOL].add(L.amb), f[K_MATCOL].add(L.mat);
    f[K_PIXEL].add(P.blend_type), f[K_PIXEL].add(P.blend_src), f[K_PIXEL].add(P.blend_dst), f[K_PIXEL].add(P.logic_op),
        f[K_PIXEL].add(P.color_update), f[K_PIXEL].add(P.alpha_update), f[K_PIXEL].add(P.z_enable),
        f[K_PIXEL].add(P.z_func), f[K_PIXEL].add(P.z_update), f[K_PIXEL].add(P.dst_alpha), f[K_PIXEL].add(S.cull);
    for (int i = 0; i < K_N; ++i) k[i] = f[i].h;
}
struct G1K {
    HSet<8192> distinct[K_N];
    HMap<8192> last[K_N];
    unsigned changed[K_N], seen[K_N];
} g1k;

// Partage des maillages et regroupements possibles (14/11) : par image, listes distinctes, parties fixes (tableau de
// positions identique a la rencontre precedente de la meme instance) ou animees (tampon de peau en double), opaques
// (sans melange), et suites de parties VOISINES (aucune autre liste entre elles) au materiau identique (hors indirect).
struct G1M {
    HSet<4096> lists_frame;
    HMap<8192> pos_last;
    unsigned frame_lists, statics, dynamics, opaques, runs, run_parts;
    unsigned sum_lists, sum_statics, sum_dynamics, sum_opaques, sum_runs;
    uint64_t prev_mat;
    u32 prev_listcount;
} g1m;

}  // namespace

void gxn_part_measure(const void* list, u32 nbytes)
{
    const uint64_t sk = shader_key(), mk = material_key(sk);
    {
        uint64_t k[K_N];
        component_keys(k);
        const uint64_t id = ((uint64_t) (uintptr_t) list << 32 | nbytes) ^ ((uint64_t) (u32) port_model_part * 0x9E3779B97F4A7C15ull);
        for (int i = 0; i < K_N; ++i) {
            g1k.distinct[i].insert(k[i]);
            const int r = g1k.last[i].put(id, k[i]);
            if (r) ++g1k.seen[i], g1k.changed[i] += r == 2;
        }
        // partage, fixe/anime, opaque, suites voisines
        Fnv m;
        for (int i = 0; i < K_N; ++i)
            if (i != K_IND) m.add(k[i]);
        const unsigned before = g1m.lists_frame.n;
        g1m.lists_frame.insert((uint64_t) (uintptr_t) list << 32 | nbytes);
        (void) before;
        const int pr = g1m.pos_last.put(id, S.arr_base[0]);   // tableau GX_VA_POS
        if (pr == 1) ++g1m.statics;
        else if (pr == 2) ++g1m.dynamics;
        if (P.blend_type == GX_BM_NONE) ++g1m.opaques;
        const bool adjacent = g1m.prev_listcount && st.lists == g1m.prev_listcount + 1;
        if (!(adjacent && m.h == g1m.prev_mat)) ++g1m.runs;
        g1m.prev_mat = m.h;
        g1m.prev_listcount = st.lists;
    }
    ++g1.parts, ++g1_parts_this_frame;
    g1.shaders.insert(sk);
    g1.materials.insert(mk);
    const int r = g1.last.put(((uint64_t) (uintptr_t) list << 32 | nbytes) ^ ((uint64_t) (u32) port_model_part * 0x9E3779B97F4A7C15ull), mk);
    if (r == 0) ++g1.fresh;
    else if (r == 1) ++g1.same;
    else ++g1.changed;
}

void gxn_part_frame()
{
    if (!g1_parts_this_frame) return;   // VIFlush sans image du jeu
    g1_parts_this_frame = 0;
    g1m.sum_lists += g1m.lists_frame.n, g1m.sum_statics += g1m.statics, g1m.sum_dynamics += g1m.dynamics,
        g1m.sum_opaques += g1m.opaques, g1m.sum_runs += g1m.runs;
    g1m.lists_frame.clear();
    g1m.statics = g1m.dynamics = g1m.opaques = g1m.runs = 0;
    g1m.prev_listcount = 0;
    if (++g1.frames < 300) return;
    {
        const double f = g1.frames, parts = (double) g1.parts / f;
        OSReport("[G1] dessins par image : %.0f parties, %.0f listes distinctes (%.1f parties par liste) ; fixes %.0f, animees "
                 "%.0f ; opaques %.0f ; suites voisines de meme materiau %.0f (fusion exacte : %.0f -> %.0f dessins)\n",
                 parts, g1m.sum_lists / f, g1m.sum_lists ? g1.parts / (double) g1m.sum_lists : 0.0, g1m.sum_statics / f,
                 g1m.sum_dynamics / f, g1m.sum_opaques / f, g1m.sum_runs / f, parts, g1m.sum_runs / f);
        g1m.sum_lists = g1m.sum_statics = g1m.sum_dynamics = g1m.sum_opaques = g1m.sum_runs = 0;
    }
    const unsigned seen = g1.same + g1.changed;
    OSReport("[G1] %u images : %.0f parties de modeles par image ; nuanceurs distincts %u, materiaux distincts %u ; "
             "parties connues : materiau identique %u, change %u (%.1f %%) ; nouvelles %u ; listes suivies %u\n",
             g1.frames, (double) g1.parts / g1.frames, g1.shaders.n, g1.materials.n, g1.same,
             g1.changed, seen ? 100.0 * g1.changed / seen : 0.0, g1.fresh, g1.last.n);
    {
        char line[640];
        int n = 0;
        for (int i = 0; i < K_N && n < 600; ++i) {
            n += snprintf(line + n, sizeof line - n, " %s %u/%.0f%%", kKName[i], g1k.distinct[i].n,
                          g1k.seen[i] ? 100.0 * g1k.changed[i] / g1k.seen[i] : 0.0);
            g1k.distinct[i].clear(), g1k.changed[i] = g1k.seen[i] = 0;
        }
        OSReport("[G1] par poste (distincts/change d'une image a l'autre) :%s\n", line);
    }
    g1.parts = g1.frames = g1.same = g1.changed = g1.fresh = 0;
    g1.shaders.clear();
    g1.materials.clear();
}
