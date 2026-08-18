#pragma once
// Modules REL recompiles (RE4 : armes, salles, ennemis, ecran d'inventaire...).
//
// Chaque module est recompile a une adresse virtuelle qui lui est propre (AOT, fenetre
// 0xB0000000). Le jeu, lui, le charge ou son tas a de la place et le lie avec OSLink. Deux
// choses relient les deux :
//   - le repartiteur : une adresse de code d'un module a son adresse reelle (pointeur de
//     fonction, table virtuelle, prolog...) est traduite en adresse AOT par rel_to_aot ;
//   - les donnees : chaque lis/addis qui porte la moitie haute d'une adresse du module reçoit
//     l'ecart g_rel_delta[id][0] (image) ou [1] (bss), mis a jour au premier passage.
// Les adresses de retour que le code recompile range sur la pile restent des adresses AOT :
// elles ne reviennent qu'au repartiteur, qui les connait.
#include <cstdint>

namespace nj::runtime {

struct GuestCpu;

struct RelAotModule {
    uint32_t id, base, image_size, bss, bss_size;
};

constexpr uint32_t kRelMaxId = 256;
extern int32_t g_rel_delta[kRelMaxId][2];

// ctx.pc (adresse reelle d'un module lie) -> adresse AOT ; inchangee si ce n'est pas du code
// d'un module connu.
uint32_t rel_to_aot(GuestCpu& ctx);

// Disposition des modules REL traduits (adresses AOT) : enregistree au demarrage par le code genere
// (main_output.cpp), ou par le moteur lui-meme quand il traduit le disque du joueur (P3).
void rel_aot_register(const RelAotModule* modules, uint32_t count);

} // namespace nj::runtime

namespace nwii::runtime {
using RelAotModule = nj::runtime::RelAotModule;
using nj::runtime::rel_to_aot;
}

