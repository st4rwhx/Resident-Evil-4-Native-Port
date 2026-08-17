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

// PORT_VDELTA (N7, 18/11) : ce que le pointeur de table virtuelle ajoute en 64 bits (8 octets au lieu de 4). Les champs
// d'une classe à table virtuelle glissent tous de 4 (les pointeurs du jeu sont des P32) ; les « calques » des .cpp qui
// sautent un nombre fixe d'octets dans une telle classe (`u8 pad[0x308]`) l'ajoutent. Inventaire : port/outils/calques_64.py.
#define PORT_VDELTA (sizeof(void*) - 4)

#if UINTPTR_MAX > 0xFFFFFFFFu
// 64 bits, C comme C++ (les unités C du SDK, GXInit.c..., lisent aussi des adresses GameCube fixes : 18/11, plantage à
// 0xffffffff800000f8 quand port_ptr64 n'existait qu'en C++).
#define PORT_P64 1
// Base de la fenêtre de 4 Go (port/os.cpp), ALIGNÉE SUR 4 Go (32 bits bas nuls), comme la « racine » de V8 : les 32 bits
// bas d'un pointeur de la fenêtre sont son décalage, donc `(u32)(uintptr_t) p` encode gratuitement et `base | v` décode ;
// les valeurs font l'aller-retour (décalage de fichier -> pointeur -> décalage), ce que le jeu fait sans cesse.
#ifdef __cplusplus
extern "C" uintptr_t g_port_base;
#else
extern uintptr_t g_port_base;
#endif

static inline uint32_t port_ptr32(const volatile void* p)
{
    return (uint32_t) (uintptr_t) p;
}
static inline void* port_ptr64(uint32_t v)
{
    return v ? (void*) (g_port_base | v) : (void*) 0;
}
#endif

#if defined(__cplusplus) && UINTPTR_MAX > 0xFFFFFFFFu

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
#ifdef PORT_P32_VARARGS
    // Compilation de DIAGNOSTIC seulement (18/11) : un constructeur de copie écrit à la main rend P32 non trivial, et
    // clang refuse alors de le passer à une fonction variadique (printf/sprintf/OSReport…), qui recevrait sinon les 4 octets
    // bruts (adresse sans la base). Liste les endroits à corriger avec port_p(...). Casse les unions : ne pas livrer.
    P32(const P32& o) : v(o.v) {}
    P32& operator=(const P32& o) { v = o.v; return *this; }
#endif
    // implicite (tables statiques, initialisations) ; conséquence : `c ? champP32 : pointeur` est ambigu -> écrire
    // `c ? port_p(champ) : pointeur` (port_p rend T* pour P32<T> comme pour T*). Pas de surcharge nullptr (ambiguë avec 0).
    P32(T* p) : v(port_ptr32(p)) {}
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
// La valeur passée est un pointeur (le cas courant), un champ P32, NULL, ou un entier 32 bits (adresse : base | valeur).
template <class V> static inline void* port_vers_ptr(V* v) { return (void*) v; }
template <class V> static inline void* port_vers_ptr(const P32<V>& v) { return (void*) v.get(); }
static inline void* port_vers_ptr(decltype(nullptr)) { return nullptr; }
static inline void* port_vers_ptr(uint32_t v) { return port_ptr64(v); }
static inline void* port_vers_ptr(int32_t v) { return port_ptr64((uint32_t) v); }
static inline void* port_vers_ptr(long v) { return v ? port_ptr64((uint32_t) v) : nullptr; }   // NULL (= 0L)
template <class T, class V> static inline void PSet(P32<T>& d, V v) { d = (T*) port_vers_ptr(v); }
template <class T, class V> static inline void PtrSet(P32<T>& d, V v) { d = (T*) port_vers_ptr(v); }

// Variantes locales des modules (salles, objets) : même rôle que PSet, autre nom.
template <class T, class V> static inline void PSetSat(P32<T>& d, V v) { d = (T*) port_vers_ptr(v); }
template <class T, class V> static inline void PSetRobo(P32<T>& d, V v) { d = (T*) port_vers_ptr(v); }
template <class T, class V> static inline void PSetTex(P32<T>& d, V v) { d = (T*) port_vers_ptr(v); }
template <class T, class V> static inline void PSetPrim(P32<T>& d, V v) { d = (T*) port_vers_ptr(v); }
template <class T, class V> static inline void EmSet(P32<T>& d, V v) { d = (T*) port_vers_ptr(v); }    // pl0f
template <class T, class V> static inline void EmiSet(P32<T>& d, V v) { d = (T*) port_vers_ptr(v); }   // em3a
template <class T> static inline T* PGet(P32<T>& p) { return p.get(); }                       // light.cpp
template <class T, class V> static inline void VSet(P32<T>& d, V v) { d = (T*) port_vers_ptr(v); }   // shadow.cpp

// port_p(x) : le vrai pointeur, que x soit un champ P32<T> ou déjà un T* (opérateur ternaire, déductions de modèles).
template <class T> static inline T* port_p(const P32<T>& p) { return p.get(); }
template <class T> static inline T* port_p(T* p) { return p; }

// port_out(champ) : paramètre de sortie `T**` vers un champ P32 (`PullEsp(port_out(e.p), id)` au lieu de `&e.p`) : la
// fonction écrit dans un pointeur ordinaire temporaire, recopié dans le champ à la fin de l'expression.
template <class T> struct P32_out {
    P32<T>* f;
    T* t;
    explicit P32_out(P32<T>& c) : f(&c), t(c.get()) {}
    P32_out(const P32_out& o) : f(o.f), t(o.t) {}
    ~P32_out() { *f = t; }
    operator T**() { return &t; }
    template <class U> operator U**() { return (U**) &t; }   // void** etc.
};
template <class T> static inline P32_out<T> port_out(P32<T>& c) { return P32_out<T>(c); }
template <class T> static inline T** port_out(T*& c) { return &c; }   // déjà un vrai pointeur : son adresse

template <class F> struct FnP32 {   // pointeur de fonction : décalage depuis la même base (le code est dans la fenêtre)
    uint32_t v;

    FnP32() = default;
    FnP32(F f) : v(port_ptr32((const void*) f)) {}
    FnP32& operator=(F f) { v = port_ptr32((const void*) f); return *this; }
    operator F() const { return (F) port_ptr64(v); }
    template <class U> explicit operator U() const { return P32_cast<U>::de(v, port_ptr64(v)); }
};

#else   // 32 bits (ou C) : les champs restent de vrais pointeurs

#if UINTPTR_MAX <= 0xFFFFFFFFu   // vrai 32 bits (en 64 bits, ce bloc ne sert qu'au C : port_ptr32/64 sont définis plus haut)
#define PORT_P64 0
#ifdef __cplusplus
template <class T> using P32 = T*;
template <class F> using FnP32 = F;
template <class T> static inline T* port_p(T* p) { return p; }
template <class T> static inline T** port_out(T*& c) { return &c; }
#endif
#define port_ptr32(p) ((uint32_t) (uintptr_t) (p))
#define port_ptr64(v) ((void*) (uintptr_t) (v))
#endif

#endif
#endif
