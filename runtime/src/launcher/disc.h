#pragma once
#include <string>

#include "runtime/game_version.h"

namespace nj::launcher {

enum class DiscCheck { Ok, Unreadable, NotGameCube, WrongGame, WrongBuild, OtherRegion, WrongDiscNumber };

struct DiscInfo {
    std::string id;   // identifiant du jeu (6 caracteres de l'en-tete)
    nj::runtime::GameVersion version = nj::runtime::GameVersion::Unknown;
};

// Verifie que `image` (.iso, .gcm, .rvz) est le disque 1 de Resident Evil 4 dans la version que
// cet executable sait jouer (celle du code traduit qu'il contient) et, si oui, extrait
// sys/main.dol, sys/boot.bin et sys/bi2.bin dans `game_dir`.
DiscCheck check_and_prepare(const std::string &image, const std::string &game_dir, DiscInfo *info);

// Le disque 2 du meme jeu : meme identifiant, numero de disque 1 dans l'en-tete (octet 6).
DiscCheck check_second(const std::string &image, const DiscInfo &first);

// Cherche le disque 2 a cote du disque 1 (meme dossier) ; "" si aucun.
std::string find_second_beside(const std::string &first_image, const DiscInfo &first);

const char *describe(DiscCheck c);
const char *region_name(const DiscInfo &d);

}  // namespace nj::launcher
