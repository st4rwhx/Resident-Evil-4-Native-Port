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
    NJ_MSG_FRAME = 3,    // coeur -> appli : a = emplacement, b = numero ; barriere GPU jointe (SCM_RIGHTS) ou aucune
    NJ_MSG_PAD = 16,     // appli -> coeur : data = NjPad
    NJ_MSG_RELEASE = 17, // appli -> coeur : a = emplacement qu'elle n'affiche plus
    NJ_MSG_QUIT = 18,    // appli -> coeur
};

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
