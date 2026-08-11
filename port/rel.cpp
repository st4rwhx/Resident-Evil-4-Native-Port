// -*- coding: utf-8 -*-
// Les modules REL (salles, ennemis, armes, outils), compiles dans le binaire.
//
// Sur GameCube chaque module est un fichier que le jeu lit puis relie a la volee (OSLink) ; son
// en-tete donne l'identifiant du module et les adresses de ses trois points d'entree. Ici le code
// existe deja dans le binaire (port/build_rel.py) et le fichier du disque ne sert plus qu'a
// donner l'identifiant : on retourne l'en-tete, on y ecrit les adresses des fonctions du module
// correspondant (registre genere), et OSLink n'a plus rien a relier.
#include "port_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dolphin.h>
#include <dolphin/os/OSModule.h>

struct RelEntry {
    unsigned id;
    const char *name;
    void (*prolog)();
    void (*epilog)();
    void (*unresolved)();
};
extern const RelEntry g_rel_table[];

namespace {

inline u32 bswap(u32 v)
{
    return ((v >> 24) & 0xFFu) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | ((v << 24) & 0xFF000000u);
}

const RelEntry *find_id(u32 id)
{
    for (const RelEntry *e = g_rel_table; e->prolog; ++e)
        if (e->id == id)
            return e;
    return nullptr;
}

const RelEntry *find_prolog(u32 p)
{
    for (const RelEntry *e = g_rel_table; e->prolog; ++e)
        if ((u32) e->prolog == p)
            return e;
    return nullptr;
}

} // namespace

// Retourne l'en-tete d'un module charge depuis le disque (0x40 octets, gros-boutiste) et y pose
// les adresses du module compile. Rend le module trouve, ou null.
extern "C" const void *port_rel_prepare(void *hdr)
{
    u32 *w = (u32 *) hdr;
    // Deja prepare ? (le champ prolog designe alors une fonction du registre.)
    if (const RelEntry *e = find_prolog(w[0x34 / 4]))
        return e;

    const u32 id_le = w[0];
    const u32 id_be = bswap(w[0]);
    const RelEntry *e = find_id(id_be);
    if (!e) {
        std::printf("[PORT] module REL inconnu : identifiant %u (ou %u)\n", (unsigned) id_be, (unsigned) id_le);
        return nullptr;
    }
    // Mots 0..11 : id, liens, nombre de sections, tables, nom, version, bss, deplacements.
    for (int i = 0; i < 12; ++i)
        w[i] = bswap(w[i]);
    w[0x34 / 4] = (u32) e->prolog;
    w[0x38 / 4] = (u32) e->epilog;
    w[0x3C / 4] = (u32) e->unresolved;
    return e;
}

// Rien a relier : le code est deja dans le binaire. On verifie seulement que l'en-tete a ete
// prepare - un module qui arrive d'ailleurs (ARAM, copie) est prepare ici.
BOOL OSLink(OSModuleInfo *newModule, void *bss)
{
    (void) bss;
    return port_rel_prepare(newModule) ? TRUE : FALSE;
}

BOOL OSUnlink(OSModuleInfo *oldModule)
{
    (void) oldModule;
    return TRUE;
}

// Les _prolog des modules parcourent `_ctors` (liste zero-terminee des constructeurs statiques du
// module) : ici les constructeurs tournent au demarrage, avec ceux du reste du binaire, donc la
// liste est vide.
extern "C" {
void (*_ctors[1])(void) = { 0 };
void (*_dtors[1])(void) = { 0 };
}
