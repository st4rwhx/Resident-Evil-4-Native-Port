// -*- coding: utf-8 -*-
// Le write-gather pipe, branche sur la file de commandes d'aurora.
//
// Une partie de RE4 ne passe pas par les fonctions de sommet : elle ecrit les composantes a la
// suite dans une fenetre materielle du Gekko (`GXWGFifo->s16 = v`). C'est le cas du rendu de
// texte et du module de debogage. port/wgpipe.h transforme ces ecritures en appels ; il reste
// a dire ou vont les octets.
//
// Ils vont exactement la ou vont ceux d'aurora : dans sa file de commandes. Ses propres
// fonctions de sommet (GXPosition3s16 et consorts) ne font rien d'autre qu'appeler
// `aurora::gx::fifo::write_u16`, et la file est stockee en gros-boutiste, comme sur le
// materiel - la conversion est faite par write_*.
//
// C'est important, et ce n'etait pas evident : tant que ces ecritures tombaient dans un puits,
// aurora lisait la commande suivante **a la place des donnees de sommet**. Le symptome etait
// un « unsupported primitive type 240 » tres loin de la cause. Un GXBegin annonce n sommets ;
// si les octets ne suivent pas, tout le flux glisse.
//
// Cette unite est compilee en C++20 (voir CMakeLists.txt) : les en-tetes internes d'aurora
// l'exigent, alors que le decompile se compile en gnu++11.
#include "gx/fifo.hpp"

extern "C" {

void port_wg_push_u8(unsigned char v) { aurora::gx::fifo::write_u8(v); }
void port_wg_push_s8(signed char v) { aurora::gx::fifo::write_u8((unsigned char) v); }
void port_wg_push_u16(unsigned short v) { aurora::gx::fifo::write_u16(v); }
void port_wg_push_s16(short v) { aurora::gx::fifo::write_u16((unsigned short) v); }
void port_wg_push_u32(unsigned long v) { aurora::gx::fifo::write_u32((unsigned int) v); }
void port_wg_push_s32(long v) { aurora::gx::fifo::write_u32((unsigned int) v); }
void port_wg_push_f32(float v) { aurora::gx::fifo::write_f32(v); }

// Un index de matrice par sommet : un octet dans le flux, comme le reste. aurora ne la fournit
// pas, mais elle appartient au meme chemin de soumission directe.
void GXMatrixIndex1u8(unsigned char idx) { aurora::gx::fifo::write_u8(idx); }

} // extern "C"
