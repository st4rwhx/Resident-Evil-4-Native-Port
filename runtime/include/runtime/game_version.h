#pragma once
// Version du jeu chargee (debug, USA, Europe) et traduction des adresses du jeu que le moteur cite.
//
// Le moteur ecrit chaque adresse du jeu sous sa valeur dans le DOL debug (celle des notes et du
// decompile) ; ga() rend l'adresse de la meme variable ou du meme point du code dans la version
// chargee (table generee par tools/re4/gen_addr_table.py depuis versions_map.py).
#include <cstddef>
#include <cstdint>
#include <string>

namespace nj::runtime {

enum class GameVersion { Unknown, Debug, Usa, Europe };

std::string sha1_hex(const uint8_t *data, size_t size);

// Reconnait la version par l'empreinte de main.dol (le fichier tel que sur le disque).
GameVersion detect_game_version(const uint8_t *dol, size_t size);
void set_game_version(GameVersion v);
GameVersion game_version();
const char *game_version_name(GameVersion v);

// Adresse du jeu pour la version chargee. Une adresse absente de la table reste telle quelle.
uint32_t ga(uint32_t debug_addr);

// Constante d'adresse du jeu : sa valeur debug (plus un decalage dans l'objet), traduite a l'usage.
struct GameAddr {
  uint32_t debug;
  uint32_t offset = 0;
  operator uint32_t() const { return ga(debug) + offset; }
};

// Champ de pG (Global) : decalage du debug -> decalage de la version chargee (13/10). Les versions
// commerciales (USA et Europe) ont 12 octets de plus avant pG+0x4F90 : tout champ a partir de la est
// decale de +0xC (releve sur 87 champs par tools/re4/pg_offsets.py ; avant 0x4F80, rien ne bouge).
// Sans cette traduction, le moteur lisait le mauvais drapeau de cinematique, de menu, l'arme en main...
inline uint32_t pg(uint32_t debug_off) {
  const GameVersion v = game_version();
  return debug_off >= 0x4F90 && (v == GameVersion::Usa || v == GameVersion::Europe) ? debug_off + 0xC
                                                                                       : debug_off;
}

}  // namespace nj::runtime
