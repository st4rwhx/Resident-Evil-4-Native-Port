#pragma once
// Moteur natif : les images que produit le noyau (operations de registres, dessins deja
// convertis, textures GX brutes) traduites en liste de commandes pour notre rendu.
//
// Format des images (octets, petit-boutiste) : une suite d'operations, un mot d'en-tete (operation
// dans les bits 0-7, nombre de mots de la charge dans les bits 8-31) puis la charge.
//   1 REGISTRE BP   1 mot  : registre << 24 | valeur sur 24 bits
//   2 CHARGE XF     1 + n  : adresse XF, puis n mots
//   3 DESSIN       10 mots : primitive, disposition, pas, decalage des sommets, nombre de sommets,
//                            decalage des indices, nombre d'indices, 3 mots de matrices par defaut
//   4 TEXTURE      12 mots : unite, cle (2 mots), drapeaux, adresse, largeur, hauteur, format,
//                            niveaux, palette, decalage dans le bloc, octets (0 : deja envoyee)
//   5 PALETTE       3 mots : decalage TMEM, octets, decalage dans le bloc
//   6 OUBLI         2 mots : cle d'une texture
//   7 ETAT BP     256 mots : tous les registres BP (debut d'une image qui repart de zero)
#include <cstdint>
#include <cstdio>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include "runtime/gx_command.h"

namespace nj::runtime::gx::chunk {

struct Image {
  uint32_t flags = 0;          // 1 repart de zero, 2 fin par copie vers l'image affichee, 4 maintenue, 16 noir
  uint32_t sequence = 0;
  uint32_t xfb_address = 0, xfb_width = 0, xfb_height = 0;
  std::vector<uint32_t> ops;
  std::vector<uint8_t> vertices;
  std::vector<uint16_t> indices;
  std::vector<uint8_t> blob;
};

// Une image enregistree dans un fichier (magic « RE4GXCH1 »), pour les essais sur PC.
bool read_file(const std::string &path, Image &out);

// Une image vue sans copie (pointeurs du noyau, ou ceux d'une Image lue d'un fichier).
struct View {
  const uint32_t *ops;
  size_t op_words;
  const uint8_t *vertices;
  size_t vertex_bytes;
  const uint16_t *indices;
  size_t index_count;
  const uint8_t *blob;
  size_t blob_bytes;
};
inline View view_of(const Image &i) {
  return {i.ops.data(), i.ops.size(), i.vertices.data(), i.vertices.size(), i.indices.data(), i.indices.size(),
          i.blob.data(), i.blob.size()};
}

class Adapter {
public:
  // Traduit une image en commandes. Les textures sont ecrites dans la memoire invitee a
  // l'adresse que le jeu leur avait donnee ; les palettes passent par la memoire invitee et les
  // registres de chargement de palette, comme le jeu.
  void translate(const View &img, std::vector<GxOp> &out);

private:
  float xf_pos_[256] = {0};
  float xf_nrm_[96] = {0};
  bool pos_loaded_ = false;
  std::unordered_map<uint64_t, std::vector<uint8_t>> textures_;     // cle de contenu -> octets
  std::unordered_map<uint32_t, uint64_t> at_address_;               // adresse -> cle ecrite en dernier
  uint32_t palette_cursor_ = 0;
  // Dessins deja traduits : 99 % des dessins se repetent octet pour octet d'une image a la suivante.
  // Cle = empreinte des octets d'entree du dessin ; la valeur est partagee avec les commandes (shared_vertices),
  // et l'empreinte sert aussi de geom_hash pour la geometrie residente du rendu.
  struct Cached {
    std::shared_ptr<const std::vector<GxVertex>> vertices;
    uint64_t last_frame = 0;
  };
  std::unordered_map<uint64_t, Cached> draws_;
  uint64_t frame_no_ = 0;
  size_t cached_vertices_ = 0;
  // Geometrie des listes d'affichage gardee pour le noyau (operations 8 a 10) : sommets convertis une
  // seule fois, puis designes par leur cle. `generation` change a chaque envoi sous une meme cle.
  struct Geometry {
    std::vector<uint8_t> vertices;
    std::vector<uint16_t> indices;
    uint32_t generation = 0;
  };
  std::unordered_map<uint64_t, Geometry> geometry_;
  uint32_t geometry_generation_ = 0;
  // Un dessin (10 mots de Re4GxDraw) lu dans `vb` / `ix` ; `ref` non nul : il vient de la geometrie
  // gardee sous cette cle (le cache des dessins n'a alors pas a relire les octets).
  void translate_draw(const uint32_t *p, const uint8_t *vb_all, size_t vb_bytes, const uint16_t *ix_all,
                      size_t ix_count, uint64_t ref, uint32_t ref_generation, std::vector<GxOp> &out);
public:
  uint64_t cache_hits = 0, cache_misses = 0;   // pour les mesures
  uint64_t geometry_draws = 0;                 // dessins venus de la geometrie gardee
};

}  // namespace nj::runtime::gx::chunk
