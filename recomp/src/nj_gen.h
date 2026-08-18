#pragma once
// Generateur : fonctions (d'apres les symboles), etiquettes et points de reprise, fichiers C++,
// table de repartition, boucle principale, CMake du jeu (docs/RECOMPILATEUR.md).
#include <cstdint>
#include <string>

namespace nj {

struct Config;
class Image;

struct GenStats {
    size_t functions = 0, gap_functions = 0, overlaps = 0, instructions = 0;
    size_t labels = 0, resume_points = 0, unknown = 0, natives_bound = 0, wraps = 0, files = 0;
    size_t code_pointers = 0;
    // Points de reprise par raison : entree, apres appel, apres sc/mtmsr, retour de boucle,
    // pointeur de code dans les donnees, cible de branchement venue d'une autre fonction.
    size_t rp_by[6] = {};
};

bool generate(const Config& cfg, const Image& img, GenStats& st);

}  // namespace nj
