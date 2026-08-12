// -*- coding: utf-8 -*-
// L'ARAM : les 16 Mo de memoire audio du GameCube, et la file de transferts qui y mene.
//
// Ce n'est pas seulement de la memoire pour le son. Sur GameCube, l'ARAM est **une reserve
// generale** : le processeur ne peut pas l'adresser directement, mais il y depose ce qu'il ne
// veut pas garder en MEM1 et le rapatrie par DMA au besoin. RE4 s'en sert intensivement pour
// ses banques de sons, ses textures differees et ses donnees de salle.
//
// D'ou deux choses a respecter et pas une de plus :
//  * une ARAM est un espace d'adresses **separe**, qui commence apres une zone reservee au
//    systeme (0x4000) et s'alloue par un pointeur qui ne recule jamais ;
//  * un transfert est un DMA asynchrone avec une callback. Ici c'est un memcpy suivi de la
//    callback, comme pour le DVD : la semantique est juste, seul le rythme est faux.
//
// Rendre 0 depuis ARAlloc - ce que faisait la souche - donne au jeu une adresse ARAM nulle
// qu'il utilise ensuite comme une adresse valide. Le plantage arrive bien plus tard, dans le
// transfert, sans rien qui pointe vers l'allocateur.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dolphin.h>
#include <dolphin/ar.h>

#include "port_log.h"

namespace {

const u32 ARAM_SIZE = 16u * 1024u * 1024u;
// La console reserve le bas de l'ARAM au systeme : ARInit y place ses tampons et
// ARGetBaseAddress rend la premiere adresse libre. Le jeu additionne a partir de la.
const u32 ARAM_USER_BASE = 0x4000u;

u8 *g_aram = nullptr;
u32 g_next = ARAM_USER_BASE;

void aram_init()
{
    if (!g_aram)
        g_aram = (u8 *) std::calloc(1, ARAM_SIZE);
}

// Traduit une adresse ARAM en pointeur hote, en refusant ce qui sort des bornes : un transfert
// hors ARAM ecrirait n'importe ou dans le processus.
u8 *host(u32 aram_addr, u32 length)
{
    aram_init();
    if (!g_aram || aram_addr >= ARAM_SIZE || (u64) aram_addr + length > ARAM_SIZE)
        return nullptr;
    return g_aram + aram_addr;
}

} // namespace

extern "C" {

u32 ARInit(u32 *stack_index_addr, u32 num_entries)
{
    (void) stack_index_addr;
    (void) num_entries;
    aram_init();
    g_next = ARAM_USER_BASE;
    std::printf("[PORT] ARAM : %u Mo, base utilisateur 0x%X\n",
                ARAM_SIZE / (1024u * 1024u), ARAM_USER_BASE);
    return ARAM_SIZE;
}

u32 ARGetBaseAddress(void) { return ARAM_USER_BASE; }

u32 ARGetSize(void) { aram_init(); return ARAM_SIZE; }

// Allocateur a pointeur qui avance, exactement comme celui du SDK : l'ARAM ne se libere pas
// bloc par bloc, elle se remet a zero entre deux zones de jeu.
u32 ARAlloc(u32 length)
{
    aram_init();
    const u32 addr = (g_next + 31u) & ~31u;
    if ((u64) addr + length > ARAM_SIZE) {
        std::printf("[PORT] ARAM saturee : %u octets demandes a 0x%X\n",
                    (unsigned) length, (unsigned) addr);
        return 0;
    }
    g_next = addr + length;
    return addr;
}

u32 ARFree(u32 *length)
{
    if (length)
        *length = ARAM_SIZE - g_next;
    g_next = ARAM_USER_BASE;
    return ARAM_USER_BASE;
}

BOOL ARCheckInit(void) { return g_aram != nullptr; }
void ARReset(void) { g_next = ARAM_USER_BASE; }
u32 ARGetInternalSize(void) { return ARAM_SIZE; }

// ---------------------------------------------------------------- transferts

void ARQInit(void) { aram_init(); }
void ARQReset(void) {}
void ARQFlushQueue(void) {}

// Le sens du transfert est porte par `type` : MRAM -> ARAM ou ARAM -> MRAM. S'y tromper donne
// un jeu qui charge du silence et ecrase ses propres donnees, sans erreur.
void ARQPostRequest(ARQRequest *request, u32 owner, u32 type, u32 priority,
                    u32 source, u32 dest, u32 length, ARQCallback callback)
{
    // **La requete doit etre remplie avant le transfert.** Le SDK y range tout ce que la
    // callback relira : `trans2aram_cb` de RE4 recupere le proprietaire par `r->owner` pour
    // retrouver sa file DVD, et la taille transferee par `r->length`. Une souche qui se
    // contente de copier les octets laisse ces champs indefinis, et la callback part suivre un
    // pointeur pris dans la pile - le chargement de etc/core.das tombait juste apres son
    // transfert ARAM, sur un appel a une adresse nulle.
    if (request) {
        request->next = nullptr;
        request->owner = owner;
        request->type = type;
        request->priority = priority;
        request->source = source;
        request->dest = dest;
        request->length = length;
        request->callback = callback;
    }
    if (length) {
        if (type == ARQ_TYPE_MRAM_TO_ARAM) {
            if (u8 *d = host(dest, length))
                std::memcpy(d, (const void *) (unsigned long) source, length);
        } else {
            if (const u8 *sp = host(source, length))
                std::memcpy((void *) (unsigned long) dest, sp, length);
        }
    }
    if (callback)
        callback((u32) (unsigned long) request);
}

void ARQRemoveRequest(ARQRequest *) {}

ARQCallback ARRegisterDMACallback(ARQCallback) { return nullptr; }
void ARSetSize(void) {}
void ARClear(u32) {}
void ARQRemoveOwnerRequest(u32) {}

// Lectures et ecritures directes, utilisees par le code qui ne passe pas par la file.
void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length)
{
    if (!length)
        return;
    if (type == ARQ_TYPE_MRAM_TO_ARAM) {
        if (u8 *d = host(aram_addr, length))
            std::memcpy(d, (const void *) (unsigned long) mainmem_addr, length);
    } else {
        if (const u8 *sp = host(aram_addr, length))
            std::memcpy((void *) (unsigned long) mainmem_addr, sp, length);
    }
}

u32 ARGetDMAStatus(void) { return 0; }   // jamais occupe : les transferts sont immediats

}

// Pour la conversion des donnees du disque posees en ARAM (port/endian.cpp).
extern "C" void *port_aram_host(u32 addr, u32 length) { return host(addr, length); }
