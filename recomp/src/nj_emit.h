#pragma once
// Emetteur : une instruction PowerPC (Gekko) -> fragment C++ (docs/RECOMPILATEUR.md, v1).
#include <cstdint>
#include <functional>
#include <set>
#include <string>

namespace nj {

class Image;

// Ce que l'emetteur doit savoir de la fonction en cours et du programme.
struct EmitEnv {
    const Image* img = nullptr;
    uint32_t fn_start = 0, fn_end = 0;           // [debut, fin) de la fonction en cours
    const std::set<uint32_t>* labels = nullptr;   // etiquettes de la fonction
    // Nom C++ de la fonction dont l'entree est `addr` ("" si aucune).
    std::function<std::string(uint32_t)> entry_name;
    // Nom de la fonction native qui remplace la fonction a `addr` ("" si aucune).
    std::function<std::string(uint32_t)> native_name;
    // Correctif REL a l'adresse (module, bss) ; retourne false s'il n'y en a pas.
    std::function<bool(uint32_t, uint32_t&, bool&)> fixup;
    // Cible absolue d'un bl d'un module vers le DOL (0 si aucune).
    std::function<uint32_t(uint32_t)> branch_override;
};

// Registres generaux en variables locales (config local_regs) : R(n) donne `rN` et note n dans
// g_used_regs ; l'emetteur place NJ_WB (reecrire les locaux dans ctx) avant toute sortie ou appel,
// NJ_RL (les relire) apres un appel ; nj_gen remplace ces marqueurs une fois la fonction emise.
extern bool g_local_regs;
extern bool g_block_count;   // cfg.block_count (voir nj_config.h)
extern bool g_cond_before_poll;   // cfg.cond_before_poll
extern bool g_budget;             // cfg.budget
extern bool g_psq_const;          // cfg.psq_const
extern uint32_t g_used_regs;

struct EmitResult {
    bool ends_block = false;    // l'instruction transfere (ou peut transferer) le controle
    bool falls_through = true;  // l'instruction suivante peut s'executer juste apres
    bool unknown = false;       // forme non traduite
};

// Adresse de branchement d'un b/bc, en tenant compte de la cible notee pour les REL.
uint32_t branch_target(const EmitEnv& env, uint32_t addr, uint32_t op);

EmitResult emit_instr(const EmitEnv& env, uint32_t addr, uint32_t op, std::string& out);

// Vrai si l'instruction est un appel (bl, bcl, bctrl, blrl, bclrl) : l'adresse suivante est un
// point de reprise.
bool is_call(uint32_t op);
// Vrai si l'instruction est un branchement relatif (b/bc sans lien) vers `target` interne.
bool is_local_branch(uint32_t op);

}  // namespace nj
