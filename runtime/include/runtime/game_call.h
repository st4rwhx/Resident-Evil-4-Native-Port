#pragma once
// Appel synchrone d'une fonction du jeu depuis le moteur (core/dispatch.cpp, 20/10).
#include <cstdint>
#include <initializer_list>

namespace nj::runtime {

struct GuestCpu;
// Execute la fonction du jeu `fn` (adresse de la version chargee : passer par ga() / GameAddr) avec r3.. = args, jusqu'a
// son retour ; rend r3. Tous les registres sont rendus tels quels, seule la memoire du jeu change. Fil du jeu seulement,
// a un point de passage sur (entre deux taches). Rend 0 sans rien faire pendant un autre appel.
uint32_t call_game(GuestCpu& ctx, uint32_t fn, std::initializer_list<uint32_t> args = {});
bool call_game_busy();

}  // namespace nj::runtime
