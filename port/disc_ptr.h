// -*- coding: utf-8 -*-
// DiscPtr : un pointeur de quatre octets, pour les structures plaquees sur les octets du disque.
//
// RE4 ne desserialise presque rien. Il lit un fichier en memoire principale, puis **plaque une
// structure C dessus** et remplace sur place les decalages par des adresses. Tant que les
// pointeurs font quatre octets, la structure en memoire *est* le fichier, et c'est ce qui rend
// le moteur rapide sur une console de 2001.
//
// En 64 bits, chaque pointeur en fait huit et toute la disposition glisse : `TEXPalette` passe
// de 12 a 16 octets, `TEXDescriptor` de 8 a 16, et le jeu lit ses descripteurs a cote. Ce n'est
// pas un defaut isole - cela vaut pour chaque famille de donnees lue depuis le disque.
//
// D'ou ce type. Il stocke quatre octets, se comporte comme un pointeur, et la conversion est
// gratuite : MEM1 est place a 0x80000000 (voir le correctif d'AllocMEM1 dans aurora), donc un
// u32 **est** une adresse valide - l'extension est un simple zero-extend.
//
// Pourquoi pas tout compiler en 32 bits, ce qui reglerait la question d'un coup ? Parce que les
// telephones modernes et iOS sont en arm64. Ce type fait marcher le portage des deux cotes ;
// le 32 bits nous enfermerait sur le seul materiel ancien.
//
// Une seule regle a respecter en l'employant : ce qu'un DiscPtr designe doit vivre dans MEM1.
// C'est le cas par construction - ce sont des adresses que le jeu a lui-meme calculees a partir
// de la base du fichier charge.
#ifndef PORT_DISC_PTR_H
#define PORT_DISC_PTR_H

#include <stdint.h>   // UINTPTR_MAX, teste plus bas

#if defined(RE4_PORT) && defined(__cplusplus)
#include "ptr32.h"   // N7 (18/11) : en 64 bits, adresse = base de la fenetre | u32 (en 32 bits, identite)

template <class T>
struct DiscPtr {
    unsigned int addr;

    // Conversion vers n importe quel type de pointeur. Le code du jeu ecrit aussi bien
    // `(u8*) h->data` que `TEXDescriptor* d = tpl->descriptorArray;` ; un unique
    // `operator T*` ne couvrirait que le second, car une conversion definie par
    // l utilisateur ne s enchaine pas avec un `void*` -> `u8*`.
    template <class U> operator U *() const { return (U *) (unsigned long long) (uintptr_t) port_ptr64(addr); }

    // Et vers les entiers, parce que le jeu manipule ces champs comme des nombres : il relit
    // le decalage du fichier avant de le remplacer par une adresse, et teste son signe pour
    // savoir si la relocalisation a deja eu lieu. Sur le GameCube, une adresse commence par
    // 0x8 : elle est negative en s32, et c est **le** critere employe.
    // Une seule conversion entiere, signee : deux (signee et non signee) rendraient `p != 0`
    // ambigu. Signee parce que c est le test du jeu - sur GameCube une adresse commence par
    // 0x8, donc elle est negative, et c est ainsi qu il reconnait un champ deja relocalise.
    // Un cast vers un entier non signe passe par elle sans rien perdre.
    operator long() const { return (long) (int) addr; }

    // Sans elle, `if (p)` serait ambigu entre les conversions ci-dessus.
    explicit operator bool() const { return addr != 0u; }

    // Modeles, pour que ces deux operations ne soient instanciees que si on les emploie :
    // DiscPtr<void> existe (les champs `data` des en-tetes de texture) et n a ni dereference
    // ni fleche.
    template <class U = T> U *operator->() const { return (U *) (unsigned long long) (uintptr_t) port_ptr64(addr); }
    template <class U = T> U &operator*() const { return *(U *) (unsigned long long) (uintptr_t) port_ptr64(addr); }
    template <class U = T> U &operator[](unsigned long i) const
    {
        return ((U *) (unsigned long long) (uintptr_t) port_ptr64(addr))[i];
    }

    DiscPtr &operator=(T *p)
    {
        addr = port_ptr32(p);
        return *this;
    }
    DiscPtr &operator=(long long v)   // pour les affectations litterales (0, NULL)
    {
        addr = (unsigned int) v;
        return *this;
    }
};

// La taille est la seule chose qui compte vraiment ici : si elle deriv[a]it, tout le reste
// serait faux en silence.
struct PortDiscPtrSizeCheck {
    char check[sizeof(DiscPtr<int>) == 4 ? 1 : -1];
};

#define DISC_PTR(T) DiscPtr<T>

#elif defined(RE4_PORT) && UINTPTR_MAX > 0xFFFFFFFFu
// C en 64 bits (N7, 18/11) : quatre octets aussi, sinon la disposition du C glisse par rapport au C++ et au disque
// (src/lib/texPalette.c lisait descriptorArray sur huit octets). Lecture par DISC_GET (base | u32).
#define DISC_PTR(T) uint32_t
#define DISC_GET(T, f) ((T *) port_ptr64(f))
#define DISC_SET(f, p) ((f) = port_ptr32(p))
#else
#define DISC_PTR(T) T *
#define DISC_GET(T, f) ((T *) (f))
#define DISC_SET(f, p) ((f) = (p))
#endif

#endif
