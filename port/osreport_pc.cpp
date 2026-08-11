// -*- coding: utf-8 -*-
// OSReport, OSPanic et le mot sur la sortie standard - dans leur propre unite, et sans un
// seul en-tete du SDK.
//
// C'est la raison d'etre du fichier : <dolphin/os.h> declare ces deux fonctions en
// `__declspec(weak)`. MinGW traduit cela par un alias faible au nom decore
// (.weak.OSReport.<quelque chose>), si bien que notre definition ne repond plus au nom
// attendu et que l'editeur de liens reclame toujours OSReport. Les declarer ici a la main,
// loin de cet en-tete, est la seule facon fiable de les fournir.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

extern "C" {

// Le journal de la console de developpement. RE4 y ecrit ses messages de demarrage et ses
// erreurs ; les perdre reviendrait a se priver du peu que le jeu dit de lui-meme.
void OSReport(const char *msg, ...)
{
    std::va_list ap;
    va_start(ap, msg);
    std::vprintf(msg, ap);
    va_end(ap);
}

void OSPanic(const char *file, int line, const char *msg, ...)
{
    std::va_list ap;
    va_start(ap, msg);
    std::fprintf(stderr, "[PANIC] %s:%d ", file, line);
    std::vfprintf(stderr, msg, ap);
    std::fputc(10, stderr);
    va_end(ap);
    std::abort();
}

// Pour le code du jeu, qui ne peut pas inclure <stdio.h> : ses propres en-tetes declarent
// deja printf et sprintf, et les deux jeux de declarations se contredisent.
void port_note(const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::vprintf(fmt, ap);
    va_end(ap);
}

} // extern "C"
