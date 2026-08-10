// -*- coding: utf-8 -*-
// Le decodeur yz2 (LZ + frequences adaptatives) est ecrit a la main en assembleur PowerPC
// (`src/game/yz2asm.s`, 465 lignes) et decompresse les archives de salle.
//
// Il n'est pas encore transcrit. Plutot que d'y passer avant de savoir s'il est sur le chemin
// reellement emprunte, le portage le laisse ici en souche **bruyante** : si le jeu l'appelle,
// on le saura a la premiere execution, et c'est la mesure qui decidera de l'ordre du travail
// plutot qu'une supposition.
#include <cstdio>

#include <dolphin.h>

#include "port_log.h"

extern "C" {

void yz2Decode_Decode(void *ctx, void *dst, u32 size, void *ev)
{
    (void) ctx;
    (void) dst;
    (void) ev;
    std::printf("[PORT] yz2Decode_Decode appele (%u octets attendus) : "
                "le decodeur n est pas encore transcrit\n", (unsigned) size);
    PORT_TODO("yz2Decode_Decode");
}

void FrequencyDecode_Decode(void) { PORT_TODO("FrequencyDecode_Decode"); }
void FrequencyDecode_Decode768(void) { PORT_TODO("FrequencyDecode_Decode768"); }

}
