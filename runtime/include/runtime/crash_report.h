#pragma once
// Rapport de plantage (core/crash_report.cpp) : une ligne par arret anormal dans
// <LOCALAPPDATA>/Nightjar/plantage.txt (+ plantage.dmp sur exception fatale).
namespace nj::crash {
inline constexpr unsigned MAX_PATH_LEN = 520;
void install();                                       // au debut de main()
void rearm();                                         // remet notre filtre (LLVM le remplace)
void quit_requested();                                // fermeture voulue : exit() n'est plus anormal
void note(const char *what, const char *detail);      // cause connue (erreur fatale du compilateur...)
}  // namespace nj::crash
