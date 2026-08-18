// P3 : compilation en memoire (Clang + LLVM embarques) du C++ produit par le traducteur.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "runtime/dispatch.h"

namespace nj::runtime::p3 {

// Fichiers servis a Clang sans toucher au disque : chemin -> contenu (le lot, functions.h, ...) ;
// `ref` = contenu partage, servi sans copie (en-tete precompile embarque, 22 Mo).
struct VirtualFile {
    std::string path, text;
    const std::string* ref = nullptr;
};
using VirtualFiles = std::vector<VirtualFile>;

struct Compiled {
    std::string name;   // identifiant C++ de la fonction du jeu (dans g::)
    GameFn fn;
};

// Niveau d'optimisation par defaut (NJ_P3_OPT, sinon -O2).
const char* opt_level();
// FMA materiel (20/10) : cible x86-64 dont le processeur a FMA3. Les ps_madd* du jeu appellent std::fma ; sans -mfma le compilateur en fait un appel
// de bibliotheque (~100 cycles, 12 par tour des boucles d'habillage des personnages). `extra` = options deja donnees (--target=... pour les en-tetes
// precompiles d'une autre cible). Vrai : make_ci ajoute -mfma -ffp-contract=off ; le cache de code en tient compte (p3_host.cpp).
bool x86_fma_flags(const std::vector<std::string>& extra);

// Compile `main` (un des fichiers virtuels ou un fichier reel) avec les options `args` (-I, -D...),
// charge le code dans le JIT partage et rend les fonctions du jeu qu'il definit. Plusieurs fils
// peuvent compiler en meme temps. Le code charge ne voit du moteur que la table p3_table.cpp.
bool jit_compile(const VirtualFiles& files, const std::string& main, const std::vector<std::string>& args,
                 std::vector<Compiled>& out, std::string& err, double* ms_front = nullptr);

// En-tete precompile : `header` (un fichier virtuel) compile une fois en `out_path` au niveau `opt` ;
// les lots du meme niveau l'utilisent ensuite par -include-pch (le frontal relisait ~3,5 s
// d'en-tetes par lot, 06/10).
bool jit_make_pch(const VirtualFiles& files, const std::string& header, const std::vector<std::string>& args,
                  const std::string& opt, const std::string& out_path, std::string& err);

// C++ -> fichier objet (Clang fait toute la chaine, niveau `opt`) ; le fichier sert aussi de cache.
bool jit_compile_object(const VirtualFiles& files, const std::string& main, const std::vector<std::string>& args,
                        const std::string& opt, const std::string& obj_path, std::string& err);

// Charge un fichier objet dans le JIT partage et rend les fonctions du jeu qu'il definit.
bool jit_load_object(const std::string& obj_path, std::vector<Compiled>& out, std::string& err);

// Branche des fonctions compilees dans les cases (fil du jeu : lecture atomique du pointeur).
unsigned patch_slots(const std::vector<Compiled>& fns);

}  // namespace nj::runtime::p3
