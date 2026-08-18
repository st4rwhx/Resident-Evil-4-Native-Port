#pragma once
#include "runtime/gx_state.h"
#include <string>

namespace nj::runtime::gx {

struct ShaderPair {
    std::string vs_text;
    std::string fs_text;
};

ShaderPair build_tev_program(const GxRegs& state, uint8_t prim_type);
// En-tete GLSL du rendu (version, precisions GLES) pour les petits shaders utilitaires.
std::string glsl_header_string();

} 
