// -*- coding: utf-8 -*-
// Phase N7 (17/11) : pointeurs de 4 octets dans les structures du jeu, en 32 comme en 64 bits.
//
// Les structures du jeu gardent la disposition GameCube : chaque champ pointeur y occupe 4 octets (fichiers relogés en
// place, emplacements de taille fixe, convertisseurs d'endian.cpp, jumeau Dolphin, scénarios du Studio en dépendent).
// En 64 bits, un champ `P32<T>` range un décalage 32 bits depuis une base (g_port_base) choisie pour que ce décalage soit
// **l'adresse GameCube** (MEM1 à base + 0x80000000) ; il redevient un vrai pointeur à chaque accès. En 32 bits, `P32<T>`
// est exactement `T*` : aucun coût, aucun changement de code machine (S5, MuMu x86).
//
//   P32<cModel> pModel;              // au lieu de  cModel* pModel;
//   FnP32<void (*)(cPlayer*)> pFn;   // au lieu de  void (*pFn)(cPlayer*);
//   u32 a = port_ptr32(p);  T* q = (T*) port_ptr64(a);   // au lieu de (u32) p et (T*) a
#ifndef PORT_PTR32_H
#define PORT_PTR32_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus) && UINTPTR_MAX > 0xFFFFFFFFu

#define PORT_P64 1
// Base de la fenêtre de 4 Go (port/os.cpp), ALIGNÉE SUR 4 Go (32 bits bas nuls), comme la « racine » de V8 : les 32 bits
// bas d'un pointeur de la fenêtre sont son décalage, donc `(u32)(uintptr_t) p` encode gratuitement et `base | v` décode ;
// les valeurs font l'aller-retour (décalage de fichier -> pointeur -> décalage), ce que le jeu fait sans cesse.
extern "C" uintptr_t g_port_base;

static inline uint32_t port_ptr32(const volatile void* p)
{
    return (uint32_t) (uintptr_t) p;
}
static inline void* port_ptr64(uint32_t v)
{
    return v ? (void*) (g_port_base | v) : nullptr;
}

template <class T> struct P32_ref { typedef T& type; };
template <> struct P32_ref<void> { typedef void type; };
template <> struct P32_ref<const void> { typedef void type; };

// Conversion explicite vers n'importe quel type : pointeur -> le vrai pointeur ; entier -> la valeur 32 bits, c'est-à-dire
// l'adresse GameCube (le jeu teste `(s32) p < 0` pour savoir si un champ de fichier est déjà relogé : 0x8xxxxxxx < 0).
template <class U> struct P32_cast {
    static U de(uint32_t v, void* p) { (void) p; return (U) v; }
};
template <class U> struct P32_cast<U*> {
    static U* de(uint32_t v, void* p) { (void) v; return (U*) p; }
};
template <> struct P32_cast<bool> {
    static bool de(uint32_t v, void* p) { (void) p; return v != 0; }
};

template <class T> struct P32 {
    uint32_t v;

    P32() = default;   // trivial : utilisable dans les unions et les structures copiées par memcpy
    P32(T* p) : v(port_ptr32(p)) {}                       // aussi `p = 0` / NULL (pas de surcharge nullptr : ambiguë)
    P32& operator=(T* p) { v = port_ptr32(p); return *this; }

    T* get() const { return (T*) port_ptr64(v); }
    operator T*() const { return get(); }
    T* operator->() const { return get(); }
    typename P32_ref<T>::type operator*() const { return *get(); }

    template <class U> explicit operator U() const { return P32_cast<U>::de(v, (void*) get()); }

    P32& operator++() { *this = get() + 1; return *this; }
    P32 operator++(int) { P32 t = *this; ++*this; return t; }
    P32& operator--() { *this = get() - 1; return *this; }
    P32 operator--(int) { P32 t = *this; --*this; return t; }
    P32& operator+=(ptrdiff_t n) { *this = get() + n; return *this; }
    P32& operator-=(ptrdiff_t n) { *this = get() - n; return *this; }
};
static_assert(sizeof(P32<int>) == 4, "P32 doit tenir sur 4 octets");

// Aides du jeu qui prennent une référence de pointeur (`PSet(void*& d, void* v)` dans ss_main.h, wep_mod.h, em32.cpp ;
// `PtrSet` dans mes.cpp) : une référence de T* ne peut pas désigner un champ P32 ; ces modèles prennent le relais.
template <class T, class V> static inline void PSet(P32<T>& d, V v) { d = (T*) v; }
template <class T, class V> static inline void PtrSet(P32<T>& d, V v) { d = (T*) v; }

template <class F> struct FnP32 {   // pointeur de fonction : décalage depuis la même base (le code est dans la fenêtre)
    uint32_t v;

    FnP32() = default;
    FnP32(F f) : v(port_ptr32((const void*) f)) {}
    FnP32& operator=(F f) { v = port_ptr32((const void*) f); return *this; }
    operator F() const { return (F) port_ptr64(v); }
    template <class U> explicit operator U() const { return P32_cast<U>::de(v, port_ptr64(v)); }
};

#else   // 32 bits (ou C) : les champs restent de vrais pointeurs

#define PORT_P64 0
#ifdef __cplusplus
template <class T> using P32 = T*;
template <class F> using FnP32 = F;
#endif
#define port_ptr32(p) ((uint32_t) (uintptr_t) (p))
#define port_ptr64(v) ((void*) (uintptr_t) (v))

#endif
#endif
