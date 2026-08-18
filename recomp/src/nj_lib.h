#pragma once
// njrecomp en bibliotheque (P3, docs/P3_TRADUCTEUR.md) : le moteur traduit le jeu du joueur par lots
// de fonctions, en memoire, au lieu des fichiers output_*.cpp. Meme analyse et meme emission que
// generate() (nj_gen.cpp) : le C++ d'un lot est celui que njrecomp ecrirait pour ces fonctions.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nj {

struct Config;
class Image;

class Translator {
public:
    struct Function {
        uint32_t start = 0, end = 0;
        bool native = false;   // remplacee par une fonction du moteur : jamais traduite
        std::string name;      // identifiant C++ (dans g::)
        std::string native_name;   // fonction du moteur qui la remplace (native = true)
    };
    struct Module {            // module REL traduit : adresses AOT (rel_aot.h)
        uint32_t id = 0, base = 0, image_size = 0, bss = 0, bss_size = 0;
    };
    struct Resume {
        uint32_t pc = 0;
        uint32_t fn = 0;       // indice dans functions()
    };

    Translator();
    ~Translator();
    // Analyse le jeu (image deja chargee : Image::load_dol_bytes / declare_rel_bytes / load_rels).
    bool open(const Config& cfg, std::unique_ptr<Image> img);
    const std::vector<Function>& functions() const { return funcs_; }
    std::vector<Module> modules() const;
    uint32_t entry() const;     // point d'entree du DOL
    uint32_t dol_end() const;   // fin du DOL (au-dela : modules REL)
    // En-tete des declarations (functions.h) dont le C++ des lots a besoin.
    std::string functions_header() const;
    // C++ d'un lot de fonctions (indices dans functions(), natives ignorees) ; ses points de reprise
    // sont ajoutes a `resumes`.
    std::string emit_batch(const std::vector<uint32_t>& fns, std::vector<Resume>& resumes);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<Function> funcs_;
};

}  // namespace nj
