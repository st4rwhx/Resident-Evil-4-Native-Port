#pragma once
// Carte graphique sous Windows (12/10). Sur un PC a deux cartes (integree + dediee), Windows lance
// les programmes sur l'integree, sauf demande du programme : Nightjar exporte NvOptimusEnablement et
// AmdPowerXpressRequestHighPerformance (main.cpp), lus par les pilotes NVIDIA et AMD ; sans carte
// dediee ils sont sans effet. OpenGL ne choisit pas sa carte lui-meme : le choix du joueur passe par
// la preference graphique de Windows pour cet exe (la meme que Parametres > Affichage > Graphiques).
#include <string>

namespace nj::gpu {

struct Adapters {
    bool hybrid = false;     // une carte « economie » et une carte « performance » differentes
    std::string low, high;   // leurs noms (DXGI), vides si inconnus
};

const Adapters &adapters();   // une seule enumeration (DXGI charge a la demande)
int preference();             // preference Windows de cet exe : 0 non reglee, 1 economie, 2 performance
bool set_preference(int pref);
void apply_at_startup();      // avant la creation du contexte GL

}  // namespace nj::gpu
