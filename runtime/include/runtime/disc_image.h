#pragma once
// Image d'un disque GameCube, lue a une position :
//   - copie brute (.iso / .gcm) ;
//   - RVZ (.rvz, format compresse de Dolphin, ecrit d'apres sa specification publique
//     docs/WiaAndRvz.md : groupes zstd, donnees de remplissage regenerees) ;
//   - disque extrait (dossier sys/ + files/, disposition de Dolphin) : les octets du disque sont
//     recomposes depuis sys/boot.bin, bi2.bin, apploader.img, main.dol, fst.bin et les fichiers
//     que la FST place.
// Lectures sures entre fils.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nj::runtime {

class DiscImage {
 public:
  virtual ~DiscImage() = default;
  // Lit n octets a la position off du disque (taille d'origine, non compresse). Faux si la
  // lecture sort du disque ou echoue.
  virtual bool read(uint64_t off, void *dst, size_t n) = 0;
  virtual uint64_t size() const = 0;
  virtual const char *format() const = 0;

  // Image (reconnue par son contenu, pas par l'extension) ou dossier extrait. nullptr si illisible.
  static std::unique_ptr<DiscImage> open(const std::string &path);
};

// Une entree de la FST (table des fichiers du disque).
struct FstEntry {
  std::vector<std::string> raw;   // elements du chemin sous files/, octets de la FST (Shift-JIS)
  std::string lower;              // chemin complet en minuscules, separe par '/'
  bool dir = false;
  uint64_t offset = 0;            // fichier : position sur le disque
  uint32_t size = 0;              // fichier : taille ; dossier : index qui suit son contenu
  uint32_t index = 0;             // rang dans la FST
};
// Parcourt la FST ; bornee (une table trafiquee ne fait pas lire au-dela).
void walk_fst(const std::vector<uint8_t> &fst, const std::function<void(const FstEntry &)> &fn);

// Chemin hote d'un element de nom de la FST : un seul element (ni vide, ni . / .., ni separateur,
// ni lecteur), Shift-JIS decode sous Windows. Faux si refuse.
bool fst_component_path(const std::string &raw, std::string &utf8_out);

}  // namespace nj::runtime
