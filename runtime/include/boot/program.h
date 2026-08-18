#pragma once
// Le programme principal du disque (main.dol) : ses segments de code et de donnees, sa zone a
// remettre a zero et son point d'entree. Lu depuis l'image disque choisie dans le lanceur
// (l'en-tete du disque donne la position du DOL a 0x420) ou depuis un disque extrait
// (sys/main.dol), format decrit dans YAGCD (§ DOL).
#include <cstdint>
#include <string>
#include <vector>

namespace nj::boot {

struct Segment {
  uint32_t addr = 0;
  uint32_t size = 0;
  bool code = false;     // segment de code (« text »)
  bool zero = false;     // zone a remettre a zero (« bss ») : pas d'octets
  std::vector<std::uint8_t> bytes;
};

struct Program {
  uint32_t entry = 0;
  std::vector<Segment> segments;
  std::vector<uint8_t> file;   // le DOL tel que sur le disque (empreinte -> version du jeu)
};

// Octets d'un DOL complet -> Program. Faux si l'en-tete est incoherent.
bool parse_dol(const uint8_t *dol, size_t size, Program &out);
// `source` : image disque (.iso/.gcm) ou dossier d'un disque extrait. Renseigne aussi
// l'identifiant du jeu (6 caracteres de l'en-tete) dans `game_id`.
bool load_program(const std::string &source, Program &out, std::string &game_id);

}  // namespace nj::boot
