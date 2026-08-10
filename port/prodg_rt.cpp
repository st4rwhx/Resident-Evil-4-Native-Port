// -*- coding: utf-8 -*-
// Les quatre points d'entree memoire de ProDG.
//
// Le compilateur d'origine n'appelle pas `operator new` par son nom mange : il emet `bl
// __builtin_new`. Le decompile reproduit fidelement ces appels, donc le portage doit fournir
// ces quatre symboles - sans quoi le lien echoue sur quatre noms qui ne ressemblent a rien de
// connu, et qui sont pourtant simplement new, new[], delete et delete[].
//
// Ils passent par l'allocateur global du jeu, pas par celui de la plateforme : le jeu remplace
// `operator new` pour puiser dans son propre tas, et court-circuiter cela le ferait diverger de
// lui-meme.
#include <cstddef>

#include <dolphin.h>

void *operator new(std::size_t n);
void *operator new[](std::size_t n);
void operator delete(void *p);
void operator delete[](void *p);

extern "C" {

void *__builtin_new(std::size_t n) { return ::operator new(n); }
void *__builtin_vec_new(std::size_t n) { return ::operator new[](n); }
void __builtin_delete(void *p) { ::operator delete(p); }
void __builtin_vec_delete(void *p) { ::operator delete[](p); }

}
