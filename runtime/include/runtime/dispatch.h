#pragma once
// Repartition des fonctions du jeu traduit (P3, jalon 3) : la boucle principale, les points de
// reprise et la passerelle vers l'interpreteur vivent dans le moteur ; le code traduit ne fournit
// que des tables, enregistrees au demarrage (main_output.cpp aujourd'hui, le traducteur embarque
// demain). Voir docs/P3_TRADUCTEUR.md.
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace nj::runtime {

struct GuestCpu;
using GameFn = void (*)(GuestCpu&);

// Fonction [start, end) ; fn = numero de case ; hle = remplacee par le moteur (retour par lr).
struct FuncBound { uint32_t start, end; GameFn func; uint32_t fn; bool hle = false; };
// Point de reprise exact ; hle = remplacement du moteur a l'entree (retour par lr).
struct Resume { uint32_t pc; bool hle; GameFn func; uint32_t fn; };

struct DispatchTables {
    const FuncBound* bounds = nullptr;   // tries par start
    std::size_t bound_count = 0;
    // Points de reprise tries par pc, sans doublon. Absents (P3, moteur seul) : chaque point de
    // reprise appartient a la fonction qui le contient (mesure du 06/10 : 222 388 sur 222 388), la
    // repartition se fait par les bornes.
    const Resume* resumes = nullptr;
    std::size_t resume_count = 0;
    // Cases (appels par table) ; nullptr = appels directs, tout est natif.
    GameFn* slots = nullptr;
    bool* native = nullptr;
    std::size_t slot_count = 0;
    const char* const* slot_names = nullptr;   // nom C++ de chaque case (dans g::)
    uint32_t rel_scan_lo = 0;   // fin du DOL : au-dela, adresses des modules REL a reloger
    uint32_t entry = 0;         // point d'entree du DOL
};

void dispatch_register(const DispatchTables& t);
// P3 : fonction rencontree par l'interpreteur (numero de case) ; le gestionnaire la compile en
// priorite (promotion). nullptr hors du moteur seul.
extern void (*g_p3_want)(uint32_t fn);
// P3 : fonction d'un module REL non compilee. Son code n'existe qu'a son adresse traduite (0xB...),
// l'interpreteur ne peut pas l'executer : le gestionnaire la compile tout de suite (bloquant).
// Rend true si elle est native ensuite.
extern bool (*g_p3_need)(uint32_t fn);

// P3 : progression de la preparation (ecran du premier lancement, settings_menu.cpp prep_frame).
struct P3Progress {
    std::atomic<int> phase{0};          // 0 rien, 1 lecture du disque, 2 cache, 3 traduction,
                                        // 4 installation (-O2 de tout le jeu, premier lancement)
    std::atomic<uint32_t> done{0}, total{0};
};
extern P3Progress g_p3_progress;
const DispatchTables& dispatch_tables();

}  // namespace nj::runtime
