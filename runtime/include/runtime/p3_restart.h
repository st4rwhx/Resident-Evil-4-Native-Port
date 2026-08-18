#pragma once
// Relance apres la preparation du jeu (16/10) : la premiere session suivant l'installation du code traduit gardait un rendu
// fautif (MuMu, Adreno 650), disparu a la relance. p3_host.cpp leve ce drapeau quand il vient de compiler des tranches sans
// echec ; main.cpp relance alors l'application (host::restart_app) une seule fois (fichier-temoin).
#include <atomic>

namespace nj::runtime {
extern std::atomic<bool> g_p3_restart_wanted;
}
