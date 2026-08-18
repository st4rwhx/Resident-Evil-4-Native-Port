// Placement des fils et mesures par fil (15/10, docs/PLAN_RADICAL_2026-10-15.md, levier E).
//
// Telephones big.LITTLE (Mali-G57 MC2 : Helio G99 / Dimensity 6080 = 2 gros coeurs A76 + 6 A55) :
// le fil du jeu et celui de la lecture du flux vont sur les gros coeurs ; les fils d'appoint
// (nettoyage, textures HD) sur les petits ; le rendu et les decodeurs restent libres. Indication
// de charge ADPF (Android 13+) sur le jeu et la lecture. Le journal recoit chaque seconde le temps
// processeur reel de chaque fil, son dernier coeur, la frequence des gros coeurs et la marge
// thermique. NJ_PLACE=0 (developpement) : pas de placement ni d'ADPF, mesures gardees.
#pragma once
#include <string>

namespace nj::runtime::place {

// Nombre de gros coeurs (capacite >= 60 % du plus fort) ; appareil homogene ou topologie illisible :
// tous les coeurs. Sert a regler les fils d'appoint (decodage des sommets).
unsigned big_cores();
// Appele au debut de chaque fil inscrit (njprof::register_thread) avec son nom court.
void thread_started(const char *name);
// Travail du jeu sur l'image qui vient de finir (ms, hors attentes) : pour ADPF. Hors du verrou
// du journal (peut ecrire un evenement).
void frame_work(float game_ms);
// Champs a ajouter a la ligne S du journal (une fois par seconde), commencant par une espace.
std::string second_fields();

}  // namespace nj::runtime::place
