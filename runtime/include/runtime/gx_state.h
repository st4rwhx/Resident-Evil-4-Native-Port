#pragma once
// Etat du processeur graphique de la console tel que le moteur le suit : registres bruts (CP, XF,
// BP) et leur lecture decodee, regroupee par sujet (formats de sommets, etages TEV, unites de
// texture, copie de l'EFB, memoires XF et TLUT). Ecrit par l'analyseur de commandes
// (gx/fifo_parser.cpp), lu par le rendu (gx/renderer_gl.cpp).
#include <array>
#include <cstdint>

namespace nj::runtime::gx {

// Origine d'un attribut de sommet (VCD) : absent, dans le flux, ou par indice 8/16 bits.
enum class AttrMode : uint8_t { None = 0, Direct = 1, Idx8 = 2, Idx16 = 3 };
// Type d'une composante (VAT).
enum class CompType : uint8_t { U8 = 0, S8 = 1, U16 = 2, S16 = 3, F32 = 4 };

// Format d'un attribut : origine, type, decalage fixe (positions et coordonnees de texture), et
// nombre de composantes code par le bit « count » du VAT.
struct AttrFormat {
  AttrMode mode = AttrMode::None;
  CompType comp = CompType::U8;
  uint8_t frac = 0;
  bool cnt = false;
};

// Une entree du VAT, combinee au VCD : ce qu'un sommet de ce format contient, dans l'ordre.
struct VtxFormat {
  bool pnmtx_idx = false;      // indice de matrice de position en tete de sommet
  bool tmtx_idx[8] = {};       // indices de matrices de texture
  AttrFormat pos, nrm;
  // VCD bit 31 : normales NBT par indice, trois indices (normale, binormale, tangente) au lieu
  // d'un. L'oublier decale chaque sommet suivant de deux ou quatre octets.
  bool nrm_idx3 = false;
  AttrFormat clr[2];
  AttrFormat tex[8];
};

// Combineur d'un etage TEV (meme forme pour la couleur et pour l'alpha).
struct TevCombiner {
  uint8_t in[4];          // entrees a, b, c, d
  uint8_t op, bias, scale, clamp, dest;
};

// Un etage TEV. Ses 21 octets entrent tels quels dans la cle des nuanceurs (GetShaderHash).
struct TevStage {
  TevCombiner color, alpha;
  uint8_t tex_map, tex_coord, chan;
};

// Une unite de texture (BP 0x88.. / 0x94.. / 0x98..).
struct TexUnit {
  uint32_t data_addr;
  uint32_t width;
  uint32_t height;
  uint8_t format;
  uint32_t tlut_addr;
  uint8_t tlut_fmt;
};

// ALPHACOMPARE (BP 0xF3) : deux references, deux comparateurs, operation qui les combine.
struct AlphaCmp {
  uint8_t ref[2] = {0, 0};
  uint8_t cmp[2] = {7, 7};   // 7 = toujours
  uint8_t op = 0;
};

// ZMODE (BP 0x40).
struct DepthMode {
  uint8_t test;
  uint8_t fn;
  uint8_t write;
};

// Copie de l'EFB (source, destination, pas) et image affichee (XFB).
struct EfbCopy {
  uint16_t x, y, w, h;
  uint32_t dest, stride;
};
struct XfbInfo {
  uint32_t addr;
  uint16_t w, h, stride;
};

// Projection XF (0x1020..) : six parametres plus le type.
struct Projection {
  float m[7];
  uint32_t type;
  bool set;
};

// Memoire XF : matrices de position (0x0000), post-matrices des generateurs de coordonnees
// (0x0500, dual texture), matrices de normales (0x0400) et lumieres (0x0600 : 8 x 16 mots,
// 3 inutilises, couleur RGBA, cosatt[3], distatt[3], pos[3], dir[3]).
struct XfMemory {
  float pos[256];
  float post[256] = {0};
  float nrm[96];
  float light[8][16];
};

// Memoire des palettes (TLUT) et adresse source de la derniere charge.
struct TlutMemory {
  uint8_t mem[0x80000];
  uint32_t src;
};

// Tableaux de sommets indexes (CP 0xA0.. / 0xB0..).
struct VtxArrays {
  uint32_t base[16];
  uint32_t stride[16];
};

struct GxRegs {
  std::array<uint32_t, 256> cp;
  std::array<uint32_t, 256> xf;
  std::array<uint32_t, 256> bp;
  // BP 0xE0..0xE7 : registres de couleur du TEV (bit 23 = 0) et couleurs constantes KONST
  // (bit 23 = 1) partagent les memes adresses ; bp[] ne garde que la derniere ecriture.
  uint32_t tev_reg[8] = {0};
  uint32_t tev_konst[8] = {0};

  VtxFormat vat[8];
  VtxArrays arr;

  uint8_t mtx_idx_pos;
  // Matrice de texture par defaut de chaque coordonnee (CP matrix index A/B), remplacee sommet
  // par sommet quand le format porte des indices de matrices de texture.
  uint8_t mtx_idx_tex[8] = {0};

  uint8_t n_tev;
  uint8_t cull = 0;                       // GEN_MODE bits 14-15 : aucun, avant, arriere, tout
  uint32_t cull_hist[4] = {0, 0, 0, 0};   // modes rencontres, par dessin
  uint8_t n_texgen;
  uint8_t n_chan;
  TevStage tev[16];
  std::array<TexUnit, 16> tex_units;
  DepthMode zmode;
  AlphaCmp atest;
  uint32_t blend_mode;

  uint32_t clear[2];   // couleur d'effacement : AR, GB

  EfbCopy copy;
  XfbInfo xfb;
  bool clear_pending;
  bool frame_done;

  Projection proj;
  XfMemory xfm;
  TlutMemory tlut;

  // Version de l'etat des canaux XF et des huit lumieres : incrementee a chaque ecriture qui les
  // touche, elle evite de reexpedier les tableaux de lumieres (et de les deballer) a chacun des
  // milliers de changements d'etat d'une image, alors qu'ils bougent une fois par image au plus.
  uint32_t xf_gen = 1;

  // Cle du cache de nuanceurs : exactement ce que build_tev_program lit, ni moins (deux
  // configurations partageraient un programme), ni plus (chaque entree en trop multiplie les
  // variantes ; les references d'ALPHACOMPARE, que RE4 balaie a chaque fondu, sont un uniforme).
  // Le nuanceur de sommets ne lit aucun etat : il ne contribue rien. `prim_type` non plus.
  uint64_t GetShaderHash(uint8_t prim_type) const;
};

extern GxRegs g_gxregs;

}  // namespace nj::runtime::gx
