#pragma once
// Gestionnaire de mods (13/10, 0.1.2). Un mod est un dossier de <mods>/<id>/ :
//   manifest.json   {"id", "name", "version", "author", "description", "features"}
//                   features (21/10) : fonctions du moteur que le mod allume, separees par des virgules (« krauser »)
//   textures/       textures HD (<l>x<h>_<format>_<empreinte>.png), listes *_keys.txt,
//                   buttons/<style>/, models/ : meme disposition que l'ancien textures_hd/
//   files/          fichiers du disque remplaces (chemins sous files/ du disque, casse ignoree)
// Un .njmod est un zip de ce dossier ; un zip qui ne contient qu'un dossier textures_hd/ (paquet
// d'avant le gestionnaire) s'installe aussi. Le dossier textures_hd/ a cote de l'exe reste lu,
// comme un mod « local » sans manifeste.
// <mods> : %LOCALAPPDATA%\Nightjar\mods (Windows), <stockage externe de l'appli>/mods (Android).
// Ordre = priorite : le premier mod active qui fournit une texture ou un fichier l'emporte.
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nj::mods {

struct Mod {
    std::string id, name, version, author, description;
    std::string features;     // « krauser » : le moteur n'allume cette fonction que si le mod est active
    std::string dir;          // dossier du mod
    bool enabled = true;
    bool local = false;       // textures_hd/ a cote de l'exe : ni deplacable en tete, ni supprimable
    bool has_textures = false, has_files = false;
    uint64_t bytes = 0;
};

// Ce que les mods actives fournissent, par ordre de priorite (instantane, lu par le rendu).
struct Index {
    std::unordered_map<std::string, std::string> textures;                       // empreinte -> png
    std::unordered_map<std::string, std::unordered_set<std::string>> lists;      // "item" -> empreintes
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> buttons;  // style -> empreinte -> png
    std::unordered_map<std::string, std::string> extra;                          // "models/ss_pzzl.dat" -> fichier
    std::vector<std::pair<std::string, std::string>> disc_files;                 // chemin disque -> fichier
};

std::string mods_dir();
std::vector<Mod> list();                    // copie, dans l'ordre de priorite
std::shared_ptr<const Index> index();       // instantane courant
uint32_t generation();                      // change a chaque modification (cache des textures)

void set_enabled(const std::string &id, bool on);
void move(const std::string &id, int delta);   // -1 : plus prioritaire
bool remove(const std::string &id);

// Installation dans un fil : .njmod / .zip / dossier. Progression et resultat lisibles a chaque image.
void install_async(const std::string &path);
// Etat de l'installation : en cours (nom, fraction) ou dernier resultat.
bool install_status(std::string &name, float &fraction, std::string &message, bool &ok);
// Archives .njmod / .zip posees directement dans <mods> (Android : copiees par le joueur avec un
// gestionnaire de fichiers) : installables depuis le menu ; l'archive est effacee apres l'installation.
std::vector<std::string> archives_in_folder();
// Windows : selecteur de fichier (.njmod, .zip) ; "" si annule ou ailleurs.
std::string pick_mod_file();
// Vrai une fois apres une installation reussie d'un mod de textures (le menu active les textures HD).
bool take_new_textures();
// Vrai si un mod active declare la fonction `name` dans « features » (ex. « krauser » : Krauser jouable).
bool feature_enabled(const char *name);

}  // namespace nj::mods
