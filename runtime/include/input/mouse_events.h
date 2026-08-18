#pragma once

// Etat de la souris construit a partir des **evenements** SDL, pas par sondage.
//
// Pourquoi : SDL_GetMouseState depend du focus de la fenetre et du mode de
// capture. Avec la capture relative permanente, il rendait un masque de
// boutons vide - mesure au clic droit reel maintenu 25 secondes, boutons=0000
// sur 38 a 59 lectures de manette par fenetre de 2 s. Le bouton n'atteignait
// jamais le jeu, et la visee etait impossible pour cette seule raison.
//
// Les evenements, eux, sont delivres a la fenetre qui les recoit, quel que
// soit le mode de capture. L'etat est donc construit une fois dans la boucle
// hote et lu par les sources d'entree.

union SDL_Event;

namespace nj::runtime::input {

// Appele pour chaque evenement de la boucle hote.
void mouse_event_feed(const SDL_Event& e);

// Masque des boutons enfonces, aux memes bits que SDL_BUTTON(n).
unsigned mouse_buttons();

// Nombre d evenements souris recus, et d appuis de bouton : distingue un
// correctif qui echoue d un test qui n a jamais eu lieu.
void mouse_event_stats(unsigned* seen, unsigned* btn);

// Deplacement accumule depuis le dernier appel, puis remis a zero.
void mouse_take_motion(int* dx, int* dy);

// Derniere position absolue connue, pour le pointeur facon Wiimote.
void mouse_position(int* x, int* y);

} // namespace nj::runtime::input
