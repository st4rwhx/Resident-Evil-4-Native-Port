#pragma once
#include "input/input_source.h"

#include <chrono>
#include <cstdint>

namespace nj::runtime::input {

// Manette a distance pour le pont de debug (tools/re4/live_debug.py, 01/10) : la page web locale
// envoie l'etat complet d'une manette GameCube (boutons, sticks, gachettes) en UDP sur
// 127.0.0.1:<NJ_REMOTEPAD, 8771 par defaut>, ~60 fois par seconde. On s'en sert pour jouer et
// utiliser les vrais outils de debug du jeu (TOOL MENU : L + START) depuis la page.
// Paquet (12 octets) : "NWRP", u16 boutons (bits GameCube), stick x, y, C-stick x, y (s8),
// gachettes L, R (u8). Ignore apres 300 ms sans paquet. Ecoute en local seulement ; absent des
// builds de diffusion (NJ_ENV y vaut toujours nul).
class RemoteSource : public PadSource {
public:
    RemoteSource();
    ~RemoteSource() override;
    void poll(PadState pads[4]) override;
    static bool enabled();

private:
    int m_socket = -1;
    PadState m_state{};
    std::chrono::steady_clock::time_point m_last{};
    bool m_have = false;
};

}  // namespace nj::runtime::input
