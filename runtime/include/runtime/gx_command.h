#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>
#include <memory>

namespace nj::runtime::gx {

// Ce que le FIFO demande : ecriture d'un registre de l'une des trois unites (BP : rasteriseur et
// texture, CP : format des sommets, XF : transformation) ou dessin d'une primitive.
enum class GxOpKind : uint8_t { BPRegister, CPRegister, XFRegister, DrawPrimitive };

// Un sommet decode, pret pour le rendu.
struct GxVertex {
    float pos[3];
    float norm[3];
    float color[4];
    // Un seul jeu de coordonnees de texture.
    //
    // Les huit canaux de GX etaient reserves ici, soit 64 octets par sommet,
    // alors que le decodeur n'ecrit que le canal 0 et que le rendu ne lit que
    // celui-la (verifie sur les trois seuls fichiers qui touchent a
    // GxVertex). La structure passait 128 octets, et a 6,3 millions de
    // sommets par fenetre de 2 s cela faisait 756 Mo d'ecritures pour rien.
    // Le jour ou le multitexturage arrivera, remettre 8 ici et rien d'autre
    // ne changera - les acces sont deja indices.
    float tex[1][2];
    // Position et normale d'objet, avant toute transformation au decodage (pretransformed) : sources des
    // generateurs de coordonnees de texture du GX (texgen POS / NRM : reflets projetes, placage
    // d'environnement). Copiees au decodage, avant la transformation en espace camera.
    float obj_pos[3] = {0, 0, 0};
    float obj_norm[3] = {0, 0, 0};

    // Attributs presents dans le format du dessin.
    bool has_pos = false, has_nrm = false, has_color = false;
    bool has_tex[1] = {false};
    // Position et normale deja passees en espace camera au decodage (matrices figees du dessin) :
    // le rendu ne les transforme pas une seconde fois.
    bool pretransformed = false;

    uint8_t posMtxIdx = 0;
    uint8_t texMtxIdx[8] = {0};   // index de matrice de texture par generateur (TEXnMTXIDX ou defaut)
};

// Dessin en attente (fifo_parser.h) : octets des sommets et etat fige a l'analyse. La plupart des
// dessins sont sautes avec leur image et jamais decodes ; DecodeDraw ne decode que les survivants.
struct RawDraw;

// Banque de matrices (positions 64 x 3x4, normales 32 x 3x3) photographiee a un point du flux, en
// lecture seule et partagee par tous les dessins qui la voient : un dessin de peau rejoue d'une liste
// en cache pointe la banque du moment au lieu d'en copier 352 flottants et tout son RawDraw.
struct MatrixBank {
    float pos[256];
    float nrm[96];
};

// Charge d'une commande XF : jusqu'a 16 flottants ranges dans la commande (matrice 12, normales 9,
// lumiere 16), au-dela sur le tas. Un std::vector allouait et liberait a chaque commande XF, des
// milliers par image (~6 % du fil hote au Galaxy S5, 29/09). Meme interface que le vector utilise.
class PayloadVec {
public:
    PayloadVec() = default;
    ~PayloadVec() {
        if (heap_) [[unlikely]] delete[] heap_;
    }
    PayloadVec(const PayloadVec& o) { assign(o.data(), o.size()); }
    PayloadVec& operator=(const PayloadVec& o) {
        if (this != &o) assign(o.data(), o.size());
        return *this;
    }
    PayloadVec(PayloadVec&& o) noexcept { take(o); }
    PayloadVec& operator=(PayloadVec&& o) noexcept {
        if (this != &o) {
            if (heap_) delete[] heap_;
            heap_ = nullptr;
            take(o);
        }
        return *this;
    }
    size_t size() const { return n_; }
    bool empty() const { return n_ == 0; }
    float* data() { return heap_ ? heap_ : inl_; }
    const float* data() const { return heap_ ? heap_ : inl_; }
    float& operator[](size_t i) { return data()[i]; }
    const float& operator[](size_t i) const { return data()[i]; }
    float* begin() { return data(); }
    float* end() { return data() + n_; }
    const float* begin() const { return data(); }
    const float* end() const { return data() + n_; }
    void resize(size_t n) {
        if (n <= kInline) {
            if (heap_) {
                std::memcpy(inl_, heap_, std::min(n, n_) * sizeof(float));
                delete[] heap_;
                heap_ = nullptr;
            }
            for (size_t i = n_; i < n; ++i) inl_[i] = 0.0f;
        } else {
            float* h = new float[n];
            const size_t keep = std::min(n, n_);
            std::memcpy(h, data(), keep * sizeof(float));
            for (size_t i = keep; i < n; ++i) h[i] = 0.0f;
            if (heap_) delete[] heap_;
            heap_ = h;
        }
        n_ = n;
    }
private:
    static constexpr size_t kInline = 16;
    void assign(const float* p, size_t n) {
        resize(0);
        resize(n);
        if (n) std::memcpy(data(), p, n * sizeof(float));
    }
    // Suppose heap_ libre (nul).
    void take(PayloadVec& o) {
        n_ = o.n_;
        if (o.heap_) {
            heap_ = o.heap_;
            o.heap_ = nullptr;
        } else {
            std::memcpy(inl_, o.inl_, n_ * sizeof(float));
        }
        o.n_ = 0;
    }
    size_t n_ = 0;
    float* heap_ = nullptr;   // au-dela de kInline ; un pointeur simple : detruire une commande
                              // ne coute rien (un std::vector vide coutait ~9 % du fil hote)
    float inl_[kInline];
};

struct GxOp {
    GxOpKind type;
    uint8_t prim_type;      // dessin : primitive GX (0x80 quads ... 0xB8 points)
    uint16_t reg;           // registre ecrit (adresse XF de depart pour XF)
    uint32_t val;           // valeur ecrite (BP, CP)
    // Sommets lus dans une copie videe par le jeu (modeles animes sur le processeur).
    bool volatile_src = false;
    std::vector<GxVertex> vertices;
    // Sommets partages avec le cache de decodage (fifo_parser.cpp, deccache) : ni copie ni
    // allocation par dessin quand le dessin est reconnu (~17 000 dessins, ~20 Mo par image au
    // Galaxy S5). Lire par verts() ; modifier seulement apres own_vertices().
    std::shared_ptr<const std::vector<GxVertex>> shared_vertices;
    const std::vector<GxVertex>& verts() const {
        return shared_vertices ? *shared_vertices : vertices;
    }
    void own_vertices() {
        if (shared_vertices) {
            vertices = *shared_vertices;
            shared_vertices.reset();
        }
    }
    std::shared_ptr<RawDraw> raw;
    // Dessin de peau rejoue d'une liste en cache : banque du moment (prioritaire sur celle du RawDraw).
    std::shared_ptr<const MatrixBank> bank;

    // Transformation deportee sur le nuanceur (NJ_XFGPU=1).
    //
    // Quand elle est active, le decodeur n'applique plus la matrice a chaque
    // sommet : il la range ici et le nuanceur la fait. Mesure du 2026-09-21 :
    // 311 matrices distinctes par image pour 328 changements en ordre de
    // flux, donc chaque matrice couvre une plage contigue et un uniforme par
    // dessin ne coute qu'environ 328 vidages de lot.
    //
    // xf_gpu faux = rien de depose, le rendu televerse l'identite.
    bool xf_gpu = false;
    // Empreinte du contenu des sommets decodes (champs lus par le rendu), calculee par le
    // decodeur sur ses fils : 0 = dessin non reutilisable (sommets transformes par le processeur,
    // donc dependants de la camera, ou lus dans une copie volatile). Le rendu s'en sert pour
    // reconnaitre un lot deja televerse (geometrie residente, renderer_gl.cpp).
    uint64_t geom_hash = 0;
    // Matrice deja posee par le rejeu d'une liste d'affichage (ExpandDisplayList) : DecodeDraw
    // ne la reprend pas du RawDraw partage. Evite de copier le RawDraw a chaque dessin pour
    // 21 flottants (26 % des allocations du fil hote, releve NJ_SHAPE du 27/09).
    bool xf_from_replay = false;
    float xf_pos[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    float xf_nrm[9]  = {1,0,0, 0,1,0, 0,0,1};

    // Sommets tires par le nuanceur (NJ_PULL=1, A1 de documentation interne) : aucun
    // decodage CPU ; a l'analyse, les octets bruts des zones de tableaux lues par le dessin sont copies dans pull_blob
    // (la memoire du jeu change a l'image suivante : peaux en double tampon). Adresse d'un element d'index i de
    // l'attribut a (0 position, 1 normale, 2 couleur 0, 3 coordonnee 0) dans pull_blob : pull_bias[a] + i * pas.
    // Une copie par tableau et par image, partagee par tous les dessins qui le lisent (parties d'un meme modele).
    bool pull = false;
    // Liste compilee (V2, documentation interne) : l'appel entier en UNE commande ;
    // octets du flux et indices residents sur le GPU, tableaux de l'appel dans pull_blob.
    std::shared_ptr<const struct ListMesh> list;
    uint32_t pull_frame = 0;   // image du jeu a l'analyse (copies EFB -> XFB vues dans le flux) : cle des copies
    std::shared_ptr<const std::vector<uint8_t>> pull_blob[4];
    int32_t pull_bias[4] = {0, 0, 0, 0};

    PayloadVec payload;
};

} 
