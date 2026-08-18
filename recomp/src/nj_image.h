#pragma once
// Image du programme a recompiler : le DOL et les modules REL places a leurs adresses AOT.
//
// Formats (tout en gros-boutiste) :
//  - DOL : en-tete de 0x100 octets. 7 sections de code puis 11 de donnees : decalages dans le
//    fichier (0x00, 0x1C), adresses (0x48, 0x64), tailles (0x90, 0xAC) ; bss 0xD8/0xDC ; entree 0xE0.
//  - REL : OSModuleHeader (re4-decomp-upstream/include/dolphin/os/OSModule.h) ; table de sections
//    {decalage | bit EXEC, taille} ; imports {id de module, decalage de la liste OSRel} ; chaque
//    OSRel {avance u16, type u8, section u8, addend u32}, types R_PPC_* et R_DOLPHIN_*
//    (src/lib/OSLink.c). Le module 0 est le DOL (adresses absolues).
//
// Les modules sont places a une adresse AOT fixe (fenetre 0xB0000000, tools/re4/gen_rel_aot.py) ;
// le jeu, lui, les charge ou il veut. D'ou deux sorties pour le generateur :
//  - `fixups` : chaque instruction dont l'immediat porte la moitie haute (HA/HI) d'une adresse
//    d'un module ; le code genere y ajoute l'ecart reel - AOT (g_rel_delta du runtime) ;
//  - `branch_targets` : les `bl` d'un module vers le DOL, hors de portee (+-32 Mo) depuis la
//    fenetre AOT : leur cible absolue, prise dans la relocation.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nj {

struct Segment {
    uint32_t addr = 0;
    std::vector<uint8_t> bytes;   // vide pour un bss
    uint32_t size = 0;
    bool code = false;
    int module = 0;               // 0 = DOL, sinon id du REL
};

struct RelModule {
    uint32_t id = 0;
    std::string path;
    uint32_t base = 0;        // adresse AOT de l'image
    uint32_t bss = 0;         // adresse AOT du bss
    uint32_t image_size = 0;
    uint32_t bss_size = 0;
    std::vector<uint8_t> image;
    std::vector<uint32_t> sec_addr;   // adresse AOT de chaque section (0 si absente)
};

struct Fixup {
    uint32_t module;   // id du module dont l'adresse est portee
    bool bss;          // l'adresse est dans le bss du module
};

class Image {
public:
    bool load_dol(const std::string &file);
    // Deux passes : declarer tous les modules (pour les relocations entre modules), puis les charger.
    bool declare_rel(const std::string& path, uint32_t base, uint32_t bss);
    // memes chargements depuis des octets en memoire (P3 : fichiers lus dans le disque du joueur)
    bool load_dol_bytes(std::vector<uint8_t> dol);
    bool declare_rel_bytes(const std::string& name, std::vector<uint8_t> image, uint32_t base, uint32_t bss);
    bool load_rels();

    uint32_t entry = 0;
    std::vector<Segment> segments;
    std::vector<RelModule> modules;
    std::map<uint32_t, Fixup> fixups;              // adresse d'instruction -> module porte
    std::map<uint32_t, uint32_t> branch_targets;   // adresse d'un bl -> cible absolue

    // Mot de 32 bits a une adresse (0 si hors image).
    uint32_t word(uint32_t addr) const;
    bool is_code(uint32_t addr) const;
    const Segment* segment_at(uint32_t addr) const;

private:
    bool relocate(RelModule& m);
    const RelModule* module_by_id(uint32_t id) const;
};

}  // namespace nj
