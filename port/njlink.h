// -*- coding: utf-8 -*-
// Lien coeur <-> appli Nightjar : messages de la prise Unix (meme definition des deux cotes).
// Copie a l'identique cote Nightjar : runtime/include/runtime/native/njlink_proto.h.
#ifndef PORT_NJLINK_H
#define PORT_NJLINK_H

#include <stdint.h>

#define NJ_LINK_VERSION 1

enum {
    NJ_MSG_HELLO = 1,    // coeur -> appli : a = version
    NJ_MSG_BUFFERS = 2,  // coeur -> appli : a = nombre, b = largeur, c = hauteur ; puis a AHardwareBuffer
    NJ_MSG_FRAME = 3,    // coeur -> appli : a = emplacement, b = numero, c = zone XFB (largeur << 16 | hauteur, 0 inconnue) ; barriere GPU jointe ou aucune
    NJ_MSG_STATE = 4,    // coeur -> appli : NjMsg puis NjState dans le meme paquet (etat du jeu, une fois par image)
    NJ_MSG_PAD = 16,     // appli -> coeur : data = NjPad ; a = acceleration « Wiimote » en milliemes de g (commandes Wii, 0 = aucune)
    NJ_MSG_RELEASE = 17, // appli -> coeur : a = emplacement qu'elle n'affiche plus
    NJ_MSG_QUIT = 18,    // appli -> coeur
    NJ_MSG_PAUSE = 20,   // appli -> coeur : a = 1 pause (menu Nightjar ouvert), 0 reprise
    NJ_MSG_CASE = 19,    // appli -> coeur : mallette au doigt ; a = 1 curseur (data : int32 i, j), 2 piece en main (data : f32 x, y) ; b = grille
};

// Etat du jeu lu par le coeur dans ses propres structures (port/njstate.cpp) : ce que l'appli lisait
// dans la memoire invitee pour les commandes tactiles (re4_context.cpp). Types de taille fixe, meme
// disposition en ARM32, ARM64 et iOS.
enum {
    NJS_VALID = 1u << 0, NJS_SKIPPABLE = 1u << 1, NJS_TEXT_WAIT = 1u << 2, NJS_CARD = 1u << 3,
    NJS_DEATH = 1u << 4, NJS_DEATH_CONTINUE = 1u << 5, NJS_INVENTORY = 1u << 6, NJS_MAP = 1u << 7,
    NJS_BINOCULARS = 1u << 8, NJS_TITLE_MENU = 1u << 9, NJS_WIDE = 1u << 10, NJS_PLAYER = 1u << 11,
    NJS_AIMING = 1u << 12, NJS_ASHLEY = 1u << 13, NJS_AIM_TARGET = 1u << 14, NJS_HAND = 1u << 15,
    NJS_CHOICE_VERTICAL = 1u << 16,
};

typedef struct NjBoard {   // planche de la mallette (pzlBoard) : l'appli la projette elle-meme a l'ecran
    int8_t valid, w, h, cx, cy, pad[3];
    float mat[12];          // planche -> monde (pzlBoard.mat, 3 lignes de 4)
} NjBoard;

typedef struct NjState {
    uint32_t bits;          // NJS_*
    uint8_t mode;           // Re4Mode (re4_context.h)
    uint8_t qte;            // Re4Qte
    uint8_t prompt;         // Cckpt.action.no
    uint8_t cur_board;
    int8_t ashley_follow, cursor, cursor_count, choice_count;
    int8_t choice_cursor, case_level, vib_state, pad0;
    uint16_t prompt_pad;
    int16_t prompt_kind;
    int16_t wep_no, wep_type;
    int16_t chapters_ended, hp;
    uint16_t room;
    int16_t aim_target_hp;
    uint32_t aim_target_em;   // identifiant seulement (adresse dans le coeur)
    uint32_t leon_mot;        // idem
    int32_t leon_mot_index;   // re4_motion_index(leon_mot)
    float title_menu_alpha, fade;
    float choices[8][2];      // 640x480
    float hand_x, hand_y;
    float cam[12];            // pG->Cam.mat
    float fovy;               // pG->Cam.param.fovy
    NjBoard boards[2];        // 0 mallette, 1 zone verte du marchand
} NjState;


// Manette telle que le SDK GameCube la voit (masques PAD_BUTTON_*).
typedef struct NjPad {
    uint16_t buttons;
    int8_t stick_x, stick_y, substick_x, substick_y;
    uint8_t trigger_l, trigger_r;
} NjPad;

typedef struct NjMsg {
    uint32_t type, a, b, c;
    uint8_t data[16];
} NjMsg;

#ifdef __cplusplus
extern "C" {
#endif
void njlink_init(void);
int njlink_active(void);
void njlink_gl_ready(int w, int h);
void njlink_frame(void);
int njlink_pad(NjPad* out);
int njlink_quit(void);
#ifdef __cplusplus
}
#endif

#endif
