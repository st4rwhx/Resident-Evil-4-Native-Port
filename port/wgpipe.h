// -*- coding: utf-8 -*-
// Le write-gather pipe, vu du portage.
//
// Sur GameCube, une partie du jeu n'appelle pas de fonction pour poser un sommet : elle ecrit
// les composantes **a la suite** dans une fenetre materielle (`GXWGFifo->s16 = x;`), et le GPU
// les interprete selon le descripteur de sommet courant. C'est le cas du rendu de texte
// (mes.cpp) et du module de debogage.
//
// Il n'y a donc pas d'autre choix que de reproduire la machine a etats : savoir quels attributs
// sont actifs, dans quel ordre, avec combien de composantes chacun, et repartir les valeurs qui
// arrivent. C'est exactement ce que fait le materiel, et c'est ce que fait `port/gx.cpp`.
//
// Le detour par une structure de champs-mandataires permet de garder la **syntaxe d'origine**
// intacte dans le code du jeu : `GXWGFifo->s16 = v` appelle une fonction au lieu d'ecrire a une
// adresse. Pas une ligne du jeu a modifier pour ces unites-la.
#ifndef PORT_WGPIPE_H
#define PORT_WGPIPE_H

// Volontairement **sans include** : cet en-tete est tire depuis l'interieur de blocs
// `extern "C"` existants, et y faire entrer <dolphin/types.h> changerait la liaison de tout ce
// qu'il declare - une erreur qui se manifeste trente fichiers plus loin, sur `sprintf`.
// `extern "C++"` protege les declarations ci-dessous quel que soit le contexte d'inclusion.
#ifdef __cplusplus

extern "C++" {

extern "C" {
void port_wg_push_u8(u8 v);
void port_wg_push_u16(u16 v);
void port_wg_push_u32(u32 v);
void port_wg_push_s8(s8 v);
void port_wg_push_s16(s16 v);
void port_wg_push_s32(s32 v);
void port_wg_push_f32(f32 v);
}

struct PortWGPipe {
    // Chaque champ est un objet vide dont la seule operation est l'affectation. Le compilateur
    // les replie entierement : il ne reste que l'appel.
    struct Fu8  { void operator=(::u8 v) const { port_wg_push_u8(v); } }  u8;
    struct Fu16 { void operator=(::u16 v) const { port_wg_push_u16(v); } } u16;
    struct Fu32 { void operator=(::u32 v) const { port_wg_push_u32(v); } } u32;
    struct Fs8  { void operator=(::s8 v) const { port_wg_push_s8(v); } }  s8;
    struct Fs16 { void operator=(::s16 v) const { port_wg_push_s16(v); } } s16;
    struct Fs32 { void operator=(::s32 v) const { port_wg_push_s32(v); } } s32;
    struct Ff32 { void operator=(::f32 v) const { port_wg_push_f32(v); } } f32;
    struct Ff64 { void operator=(::f64 v) const { port_wg_push_f32((::f32) v); } } f64;
    struct Fu64 { void operator=(::u64) const {} } u64;
    struct Fs64 { void operator=(::s64) const {} } s64;

    PortWGPipe *operator->() { return this; }
};

extern PortWGPipe port_wg_pipe;

}   /* extern "C++" */

#endif   /* __cplusplus */

#endif
