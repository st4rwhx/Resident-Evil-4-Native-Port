#pragma once
// Configuration de notre recompilateur (config/re4_config.toml).
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace nj {

struct RelAot {
    std::string path;
    uint32_t module_base = 0;
    uint32_t bss_base = 0;
    // profil P3 : le module est l'octet `offset` (taille `size`) du fichier `path` du disque
    // (files/Rel/<nom>.rel, ou le REL range dans une archive files/<dossier>/<nom>.drs)
    uint32_t offset = 0, size = 0;
};

struct Config {
    std::string project_name = "RE4Native";
    std::string input_game_dir;          // disque extrait (sys/, files/)
    std::string output_dir;              // projet genere
    std::string runtime_source_dir;      // runtime copie dans le projet genere
    std::string symbols_csv;             // Name,Location,Size (hex), une ligne par fonction
    uint32_t instructions_per_file = 20000;
    // Registres generaux du jeu en variables locales de chaque fonction generee (nj_emit.h).
    bool local_regs = false;   // mesure S5 29/09 : ~6 % plus lent en 32 bits (OPTIMISATION_S5.md)
    // Compteur d'instructions : false = seulement aux retours de boucle (longueur de la boucle),
    // true = a chaque bloc (ancien). Il ne sert qu'a espacer les verifications d'interruptions.
    bool block_count = false;
    // Base de MEM1 en variable locale des fonctions generees (nj::FM, nj_ops.h).
    bool local_mem_base = true;
    // Adresse PPC posee autour de chaque acces memoire (g_nj_site) : releve des sites lents (fastmem).
    bool slow_sites = false;
    // Reprise des retours de boucle par une copie froide du branchement (nj_gen.cpp).
    bool cold_loop_resume = false;
    // Condition des retours de boucle evaluee avant la verification periodique (nj_emit.cpp).
    bool cond_before_poll = false;
    // Budget d'instructions 32 bits aux retours de boucle (nj_emit.cpp poll, ios.cpp nj_budget_slow).
    bool budget = false;
    // psq specialises par valeur connue de GQR (nj_emit.cpp psq_call, nj_ops.h psq_l_k / psq_s_k).
    bool psq_const = false;
    // P3 : appels entre fonctions du jeu par la table nj_slot[numero] (code natif ou passerelle
    // vers l'interpreteur, nj_interp), au lieu d'appels directs (docs/P3_TRADUCTEUR.md).
    bool slot_calls = false;
    // Fastmem (P1) : fichier des sites PPC gardes (releve slow_sites) ; vide = fastmem coupe. Les
    // autres acces memoire deviennent base + (a & 0x0FFFFFFF) sans controle (nj_ops.h, NJ_FM).
    std::string fastmem_guard;
    // P3 : memes donnees deja en memoire (profil de version embarque) ; prioritaires si non vides.
    std::string symbols_text, fastmem_guard_text;
    // P3 : fonctions gardees en entier (fastmem), par adresse ; le profil embarque n'a pas les noms
    // (la regle des prefixes du SDK, nj_gen.cpp, est appliquee a sa fabrication).
    std::set<uint32_t> guard_fns;
    // P3 : liste chaude du profil (adresses de debut), compilee avant le demarrage du jeu.
    std::vector<uint32_t> hot_list;
    std::vector<RelAot> rel_aot{};
    // Fonctions du jeu remplacees par une fonction native du runtime du meme nom.
    std::vector<std::string> hle_natives;
    // adresse -> fonction native qui enveloppe la fonction du jeu : nom(ctx, &nom__orig)
    std::map<uint32_t, std::string> hle_wraps;
    // Fonctions du jeu appelees par le runtime par leur nom (espace global) : relais vers g::nom.
    std::vector<std::string> runtime_calls;
    // Option de la ligne de commande (--fragments) : fragment de chaque instruction, pour le banc.
    std::string fragments_path;

    bool load(const std::string& path);
    // Profil de version (TOML en memoire : symboles, fastmem_sites, fonctions_gardees en ligne).
    bool load_text(const std::string& text);

private:
    void read_table(const void* table);   // toml::table
};

}  // namespace nj
