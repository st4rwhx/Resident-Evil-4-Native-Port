#include "nj_config.h"

#include <iostream>
#include <toml++/toml.hpp>
#include "runtime/obf.h"
#include "runtime/diag.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace nj {

// Cles communes au fichier de configuration et au profil de version embarque (P3).
void Config::read_table(const void* table) {
    const toml::table& t = *static_cast<const toml::table*>(table);
    project_name = t["project_name"_g].value_or(project_name);
    input_game_dir = t["input_game_dir"_g].value_or(std::string());
    output_dir = t["output_dir"_g].value_or(std::string());
    runtime_source_dir = t["runtime_source_dir"_g].value_or(std::string());
    symbols_csv = t["symbols_csv"_g].value_or(std::string());
    instructions_per_file = uint32_t(t["instructions_per_file"_g].value_or(int64_t(instructions_per_file)));
    local_regs = t["local_regs"_g].value_or(local_regs);
    block_count = t["block_count"_g].value_or(block_count);
    local_mem_base = t["local_mem_base"_g].value_or(local_mem_base);
    slow_sites = t["slow_sites"_g].value_or(slow_sites);
    cold_loop_resume = t["cold_loop_resume"_g].value_or(cold_loop_resume);
    cond_before_poll = t["cond_before_poll"_g].value_or(cond_before_poll);
    budget = t["budget"_g].value_or(budget);
    psq_const = t["psq_const"_g].value_or(psq_const);
    slot_calls = t["appels_par_table"_g].value_or(slot_calls);
    fastmem_guard = t["fastmem_guard"_g].value_or(std::string());
    if (auto arr = t["rel_aot"_g].as_array())
        for (auto& e : *arr)
            if (auto r = e.as_table()) {
                RelAot a;
                a.path = (*r)["path"_g].value_or(std::string());
                a.module_base = uint32_t((*r)["module_base"_g].value_or(int64_t(0)));
                a.bss_base = uint32_t((*r)["bss_base"_g].value_or(int64_t(0)));
                a.offset = uint32_t((*r)["decalage"_g].value_or(int64_t(0)));
                a.size = uint32_t((*r)["taille"_g].value_or(int64_t(0)));
                rel_aot.push_back(a);
            }
    if (auto arr = t["hle_natives"_g].as_array())
        for (auto& e : *arr)
            if (auto s = e.value<std::string>()) hle_natives.push_back(*s);
    if (auto arr = t["runtime_calls"_g].as_array())
        for (auto& e : *arr)
            if (auto s = e.value<std::string>()) runtime_calls.push_back(*s);
    if (auto tab = t["hle_wraps"_g].as_table())
        for (auto& [k, v] : *tab)
            if (auto s = v.value<std::string>())
                hle_wraps[uint32_t(std::stoul(std::string(k.str()), nullptr, 16))] = *s;
    // profil de version (P3) : donnees en ligne au lieu de chemins
    if (auto s = t["symboles"_g].value<std::string>()) symbols_text = *s;
    if (auto s = t["fastmem_sites"_g].value<std::string>()) fastmem_guard_text = *s;
    if (auto arr = t["fonctions_gardees"_g].as_array())
        for (auto& e : *arr)
            if (auto v = e.value<int64_t>()) guard_fns.insert(uint32_t(*v));
    if (auto arr = t["liste_chaude"_g].as_array())
        for (auto& e : *arr)
            if (auto v = e.value<int64_t>()) hot_list.push_back(uint32_t(*v));
}

bool Config::load(const std::string& path) {
    toml::table t;
    try {
        t = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        std::cerr << "[config] "_g << path << " : "_g << e.description() << "\n"_g;
        return false;
    }
    read_table(&t);
    if (input_game_dir.empty() || output_dir.empty() || symbols_csv.empty()) {
        std::cerr << "[config] input_game_dir, output_dir et symbols_csv sont obligatoires\n"_g;
        return false;
    }
    return true;
}

bool Config::load_text(const std::string& text) {
    toml::table t;
    try {
        t = toml::parse(text);
    } catch (const toml::parse_error&) {
        return false;
    }
    read_table(&t);
    return !symbols_text.empty();
}

}  // namespace nj
