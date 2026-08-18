#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <cstdint>
#include "runtime/gx_command.h"
#include "runtime/gx_state.h"

namespace nj::runtime::gx {

// Analyse du FIFO du GX (fifo_parser.cpp). Les dessins ne sont pas decodes a l'analyse : chacun
// garde ses octets et l'etat qu'il lui faut (RawDraw), et n'est decode que s'il survit au saut
// d'images (DecodeDraw).

// Commandes completes de `fifo` ajoutees a `commands` ; le reste incomplet reste dans `fifo`.
void ParseFifo(std::vector<uint8_t>& fifo, std::vector<GxOp>& commands);
// Sommets d'un dessin, decodes avec l'etat fige a l'analyse ; rien si deja fait ou pas un dessin.
void DecodeDraw(GxOp& cmd);
// A appeler entre deux lots de decodage (aucun DecodeDraw en cours) : libere les entrees du cache de
// decodage retirees au lot d'avant.
void DecodeEpochAdvance();
// Ecriture d'un registre BP dans l'etat du GX.
void bp_apply(uint8_t reg, uint32_t val);
// Oracle du GX natif (port/gxn.cpp) : registres CP/BP vus a l'analyse, ombre XF 0x0000-0x10FF et ses drapeaux.
void OracleRegs(const uint32_t** cp, const uint32_t** bp, const uint32_t** xf, const uint8_t** xfok);
// Coeur natif : tableaux de sommets et matrices en memoire dans l'ordre de la machine (fifo_parser.cpp).
extern bool g_native_arrays;

// Compteurs de l'analyse (fifo_parser.cpp), publies par gx.cpp.
extern uint64_t g_prof_cmds, g_prof_unknown, g_prof_draws, g_pf_draw_bytes, g_prof_dl_calls, g_pf_list_bytes,
    g_prof_dl_hits, g_prof_dl_loadindx, g_prof_dl_us, g_prof_snap_us;
std::string TakeRacePages();   // « adresse:dessins/sommets,... » des courses vues depuis le dernier appel
extern std::atomic<uint32_t> g_race_draws;   // early_drawdone : memoire changee sous un dessin
extern std::atomic<uint64_t> g_prof_vol_draws;   // dessins lus dans des zones videes par le jeu

// Copie d'une zone de la memoire du jeu que le jeu a videe vers le GPU, prise au moment du vidage.
struct VolatileRange {
    uint32_t addr = 0;  // physique
    uint32_t len = 0;
    std::shared_ptr<const std::vector<uint8_t>> bytes;   // nul : zone seulement suivie (NJ_VOLDUMP seul)
};

// Matrices d'un dessin, figees a sa place dans le flux. Le rendu applique les chargements de matrice
// en retard ; le GX anime les personnages en changeant la matrice d'une case entre deux dessins :
// lire la case au decodage donnait la derniere ecrite. Dessin anime sommet par sommet (indice de
// matrice par sommet) : toute la banque est figee ; sinon une seule matrice, et rien pour les autres.
struct DrawMatrices {
    bool single = false;            // pos / nrm valides
    float pos[12];                  // 3 x 4
    float nrm[9];                   // 3 x 3
    std::vector<float> pos_bank;    // 64 cases de 3 x 4 (256 flottants), vide sauf animation par sommet
    std::vector<float> nrm_bank;    // 32 cases de 3 x 3 (96 flottants)
};

// Liste d'affichage compilee (V2, NJ_PULL=1) : tous ses dessins au meme format, triangles seulement. Construite une
// fois a l'analyse, immuable ensuite ; le rendu la garde sur le GPU (champs gpu_*, ecrits par le seul fil de rendu).
struct ListMesh {
    std::vector<uint8_t> stream;    // octets d'index de tous les dessins, bout a bout (gros-boutistes, vsz par sommet)
    std::vector<uint32_t> idx;      // triangles en numeros de sommet de la liste
    uint32_t vsz = 0, nverts = 0;
    uint32_t hi[4] = {0, 0, 0, 0};  // index le plus grand lu par attribut (position, normale, couleur, coordonnee)
    uint16_t variant = 0;           // variante du nuanceur (bits de format, voir renderer_gl.cpp pull::variant)
    // Dessin modele (format, pas, indices de matrices) : les adresses des tableaux viennent de l'etat de CHAQUE appel
    // (peaux en double tampon : meme liste, tableaux alternes d'une image a l'autre).
    std::shared_ptr<struct RawDraw> tmpl;
    mutable uint32_t gpu_gen = 0, gpu_stream = 0, gpu_idx = 0;
};

// Ce qu'il faut pour decoder un dessin plus tard.
struct RawDraw {
    VtxFormat vat;
    VtxArrays arr;              // tableaux indexes, figes a l'analyse
    uint8_t mtx_idx_pos;
    uint8_t mtx_idx_tex[8];
    bool uses_normals;          // eclairage actif a l'analyse
    DrawMatrices mtx;
    uint16_t count;
    // Partages : un dessin d'une liste d'affichage en cache qui refige ses matrices ne recopie que
    // cet en-tete, pas les octets des sommets (une copie par dessin auparavant).
    std::shared_ptr<const std::vector<uint8_t>> bytes;
    // Copies videes que lisent ses attributs indexes a la place de la memoire du jeu (vide sauf si
    // le jeu reecrit ces tampons).
    std::vector<VolatileRange> overlays;
    // Empreinte du debut du tableau de sommets, prise **a l'analyse**. Le
    // decodage la reprend et compare : si elle a change, la memoire a bouge
    // sous nos pieds entre les deux. C'est la mesure directe de la course,
    // sans passer par l'ecran. 0 = non prise.
    uint64_t vtx_fp = 0;
    // Cle du cache de decodage, calculee une fois : un RawDraw partage (liste d'affichage en
    // cache) est immuable, ses octets aussi. Rejoue ~15 000 fois par image, il rehachait ses
    // octets a chaque fois (fifo_parser.cpp, deccache). 0 = pas encore calculee.
    mutable uint64_t dc_key = 0;
    // Derniere entree validee du cache de decodage (deccache, fifo_parser.cpp) : numero de case (16 bits)
    // et numero de serie (32 bits). Le rejeu d'un dessin de liste en cache retrouve son resultat sans
    // recherche par cle ni verrou (~20 % du travail du fil de lecture). 0 = rien. Mot atomique relache :
    // copiable avec le RawDraw.
    struct DcRef {
        std::atomic<uint64_t> v{0};
        DcRef() = default;
        DcRef(const DcRef& o) : v(o.v.load(std::memory_order_relaxed)) {}
        DcRef& operator=(const DcRef& o) {
            v.store(o.v.load(std::memory_order_relaxed), std::memory_order_relaxed);
            return *this;
        }
    };
    mutable DcRef dc_ref;
};

}  // namespace nj::runtime::gx
