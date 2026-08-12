// -*- coding: utf-8 -*-
// Pont entre le portage (types du jeu) et le rendu Nightjar (types du moteur), compiles a part :
// seules ces fonctions C traversent. Implementation : port/njgfx/njgfx.cpp (options de Nightjar).
#ifndef PORT_NJGFX_BRIDGE_H
#define PORT_NJGFX_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Lit et decode tout le flux recu (le jeu va reutiliser sa memoire) ; le dessin attend la fin d'image.
void njgfx_sync(const unsigned char* bytes, unsigned size);
// Fin d'une image : lit le reste, dessine tout, envoie l'image a l'appli (njlink).
void njgfx_frame(const unsigned char* bytes, unsigned size);

#ifdef __cplusplus
}
#endif

#endif
