// njrecomp : notre recompilateur (docs/RECOMPILATEUR.md).
//
// usage : njrecomp <config.toml>                 genere le projet
//         njrecomp <config.toml> --mots <sortie> ecrit (adresse, mot) de tout le code charge,
//                                                 pour verifier le chargeur contre un autre pipeline
#include "nj_config.h"
#include "nj_gen.h"
#include "nj_lib.h"
#include <fstream>
#include <iterator>
#include <memory>
#include "nj_image.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage : njrecomp <config.toml> [--mots sortie]\n";
        return 2;
    }
    nj::Config cfg;
    if (!cfg.load(argv[1])) return 1;
    for (int i = 2; i + 1 < argc; ++i)
        if (!std::strcmp(argv[i], "--sortie")) cfg.output_dir = argv[i + 1];
        else if (!std::strcmp(argv[i], "--fragments")) cfg.fragments_path = argv[i + 1];

    nj::Image img;
    if (!img.load_dol(cfg.input_game_dir + "/sys/main.dol")) return 1;
    for (const auto& r : cfg.rel_aot) {
        std::string p{r.path};
        if (!(p.size() > 1 && (p[1] == ':' || p[0] == '/'))) p = cfg.input_game_dir + "/" + p;
        if (!img.declare_rel(p, r.module_base, r.bss_base)) return 1;
    }
    if (!img.load_rels()) return 1;
    size_t code = 0;
    for (const auto& s : img.segments)
        if (s.code) code += s.size / 4;
    std::printf("[njrecomp] DOL + %zu modules, %zu instructions de code, %zu correctifs REL, "
                "%zu appels vers le DOL\n",
                img.modules.size(), code, img.fixups.size(), img.branch_targets.size());

    for (int i = 2; i + 1 < argc; ++i)
        if (!std::strcmp(argv[i], "--mots")) {
            FILE* f = std::fopen(argv[i + 1], "wb");
            if (!f) return 1;
            for (const auto& s : img.segments)
                if (s.code)
                    for (uint32_t a = s.addr; a + 4 <= s.addr + s.size; a += 4) {
                        const uint32_t w = img.word(a);
                        const auto fx = img.fixups.find(a);
                        std::string tag = "-";
                        if (fx != img.fixups.end())
                            tag = std::to_string(fx->second.module) + (fx->second.bss ? "b" : "i");
                        std::fprintf(f, "%08X %08X %s\n", a, w, tag.c_str());
                    }
            std::fclose(f);
            std::printf("[njrecomp] mots ecrits dans %s\n", argv[i + 1]);
            return 0;
        }
    // --essai-lib <dossier genere> : la bibliotheque (nj_lib.h), le jeu charge par octets en memoire,
    // doit produire exactement les corps des fonctions des output_*.cpp de ce dossier (P3, jalon 2).
    for (int i = 2; i + 1 < argc; ++i)
        if (!std::strcmp(argv[i], "--essai-lib")) {
            auto read = [](const std::string& path) {
                std::ifstream f(path, std::ios::in | std::ios::binary);
                return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
            };
            auto mem = std::make_unique<nj::Image>();
            if (!mem->load_dol_bytes(read(cfg.input_game_dir + "/sys/main.dol"))) return 1;
            for (const auto& r : cfg.rel_aot) {
                std::string p{r.path};
                if (!(p.size() > 1 && (p[1] == ':' || p[0] == '/'))) p = cfg.input_game_dir + "/" + p;
                if (!mem->declare_rel_bytes(p, read(p), r.module_base, r.bss_base)) return 1;
            }
            if (!mem->load_rels()) return 1;
            nj::Config c2 = cfg;
            {   // la liste des fonctions passee en texte, comme le fera le profil embarque
                std::ifstream f(cfg.symbols_csv, std::ios::binary);
                c2.symbols_text.assign(std::istreambuf_iterator<char>(f), {});
            }
            nj::Translator t;
            if (!t.open(c2, std::move(mem))) return 1;
            std::vector<uint32_t> all;
            for (uint32_t k = 0; k < t.functions().size(); ++k) all.push_back(k);
            std::vector<nj::Translator::Resume> res;
            std::string lib = t.emit_batch(all, res);
            // reference : corps des fichiers, sans leur en-tete ni leur fin d'espace de noms
            const std::string dir = argv[i + 1];
            const std::string head_end = "namespace g {\n\n", tail = "}  // namespace g\n";
            std::string ref;
            for (int n = 0;; ++n) {
                std::ifstream f(dir + "/output_" + std::to_string(n) + ".cpp", std::ios::binary);
                if (!f) break;
                std::string x((std::istreambuf_iterator<char>(f)), {});
                const size_t h = x.find(head_end);
                ref += x.substr(h + head_end.size(), x.size() - (h + head_end.size()) - tail.size());
            }
            const size_t lh = lib.find(head_end);
            const std::string body = lib.substr(lh + head_end.size(), lib.size() - (lh + head_end.size()) - tail.size());
            std::printf("[essai-lib] %zu fonctions, %zu points de reprise ; bibliotheque %zu octets, fichiers %zu octets : %s\n",
                        t.functions().size(), res.size(), body.size(), ref.size(), body == ref ? "IDENTIQUES" : "DIFFERENTS");
            return body == ref ? 0 : 1;
        }
    nj::GenStats st;
    if (!nj::generate(cfg, img, st)) return 1;
    std::printf("[njrecomp] %zu fonctions (%zu hors symboles, %zu chevauchements corriges), %zu instructions,\n"
                "           %zu etiquettes, %zu points de reprise, %zu pointeurs de code dans les donnees,\n"
                "           %zu natives liees, %zu enveloppes, %zu formes non traduites, %zu fichiers -> %s\n",
                st.functions, st.gap_functions, st.overlaps, st.instructions, st.labels, st.resume_points,
                st.code_pointers, st.natives_bound, st.wraps, st.unknown, st.files, cfg.output_dir.c_str());
    std::printf("           points de reprise : %zu entrees, %zu apres appel, %zu apres sc/mtmsr, %zu retours de "
                "boucle, %zu pointeurs de code, %zu cibles d'autres fonctions\n",
                st.rp_by[0], st.rp_by[1], st.rp_by[2], st.rp_by[3], st.rp_by[4], st.rp_by[5]);
    return 0;
}
