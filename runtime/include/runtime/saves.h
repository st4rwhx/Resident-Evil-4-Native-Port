#pragma once
// Sauvegardes (13/10) : la carte memoire de Nightjar (memcard_a.raw, 251 blocs) et les echanges avec
// Dolphin. Format des cartes GameCube (bloc 0 en-tete, 1-2 repertoire, 3-4 table des blocs, donnees
// des le bloc 5 ; blocs de 8 Ko), lu et ecrit ici ; le contenu des sauvegardes n'est jamais touche
// (le jeu le protege par ses propres sommes de controle, src/game/card.cpp).
//
// - copie de securite de la carte a chaque lancement (backups/, 5 gardees, seulement si elle a change) ;
// - import d'un .gci (un fichier de sauvegarde, dossier GCI de Dolphin) ou d'une carte .raw de Dolphin
//   (toute taille) : seulement les fichiers de RE4 (G4B?), appliques au lancement suivant, apres une
//   copie de securite (le jeu garde la carte montee pendant la partie) ;
// - export : .gci de chaque fichier de RE4 et copie de la carte .raw (lisible par Dolphin).
#include <cstdint>
#include <string>
#include <vector>

namespace nj::saves {

struct File {
    std::string game;     // G4BE, G4BP...
    std::string maker;    // 08
    std::string name;     // bh4_data00, bh4_system
    std::string comment;  // « resident evil 4 » / « FILE 01 »... (commentaire du jeu, si lisible)
    uint32_t modtime = 0; // secondes depuis le 1/1/2000
    int blocks = 0;
    std::vector<uint8_t> gci;   // entree de repertoire (0x40) + donnees
};

// Fichiers de la carte de Nightjar (lus sur le disque). false si la carte n'existe pas ou n'est pas
// formatee (le jeu la formate a la premiere sauvegarde).
bool list_card(std::vector<File> &out, std::string &status);

// Lit un .gci ou une carte .raw : fichiers de RE4 seulement.
bool read_import(const std::string &path, std::vector<File> &out, std::string &err);

// Change la region d'un fichier (G4BE <-> G4BP) : entree de repertoire seulement.
void set_region(File &f, const std::string &game4);

// Met en attente l'ecriture de ces fichiers dans la carte (au lancement suivant ; meme nom = remplace).
bool queue_import(const std::vector<File> &files, std::string &err);
int pending_count();

// Exporte la carte (.gci de chaque fichier de RE4 + copie .raw) ; rend le dossier cree.
bool export_all(std::string &folder, std::string &err);
// Copie de la carte seule (Android : partagee par le selecteur de fichiers).
std::string card_path();
std::string backups_dir();

// Appele a l'ouverture de la carte (hw_exi), avant sa lecture : imports en attente, copie de securite.
// Rend un compte rendu court (vide si rien) pour le rapport.
std::string on_card_open(const std::string &card_file);

// Fichiers de sauvegarde de Dolphin trouves sur ce PC (cartes .raw et dossiers GCI) ; vide ailleurs.
std::vector<std::string> dolphin_candidates();

}  // namespace nj::saves
