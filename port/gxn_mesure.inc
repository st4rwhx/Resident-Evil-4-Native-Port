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

}  // namespace

void gxn_part_measure(const void* list, u32 nbytes)
{
    const uint64_t sk = shader_key(), mk = material_key(sk);
    ++g1.parts, ++g1_parts_this_frame;
    g1.shaders.insert(sk);
    g1.materials.insert(mk);
    const int r = g1.last.put((uint64_t) (uintptr_t) list << 32 | nbytes, mk);
    if (r == 0) ++g1.fresh;
    else if (r == 1) ++g1.same;
    else ++g1.changed;
}

void gxn_part_frame()
{
    if (!g1_parts_this_frame) return;   // VIFlush sans image du jeu
    g1_parts_this_frame = 0;
    if (++g1.frames < 300) return;
    const unsigned seen = g1.same + g1.changed;
    OSReport("[G1] %u images : %.0f parties de modeles par image ; nuanceurs distincts %u, materiaux distincts %u ; "
             "parties connues : materiau identique %u, change %u (%.1f %%) ; nouvelles %u ; listes suivies %u\n",
             g1.frames, (double) g1.parts / g1.frames, g1.shaders.n, g1.materials.n, g1.same,
             g1.changed, seen ? 100.0 * g1.changed / seen : 0.0, g1.fresh, g1.last.n);
    g1.parts = g1.frames = g1.same = g1.changed = g1.fresh = 0;
    g1.shaders.clear();
    g1.materials.clear();
}
