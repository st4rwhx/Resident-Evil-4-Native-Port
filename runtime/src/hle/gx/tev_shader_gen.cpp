// Nuanceurs GLSL d'un etat du GX : sommets (canaux de couleur eclaires, generateurs de coordonnees
// de texture) et fragments (etages du TEV, textures indirectes, brouillard, test alpha). Ecrit
// d'apres la description du materiel (registres XF des canaux et des lumieres, BP du TEV) ; les
// noms d'uniformes et les emplacements d'attributs sont ceux que renderer_gl.cpp lie.
#include "common/platform.h"
#include "runtime/env.h"
#include "runtime/gfx_settings.h"
#include "runtime/gx/tev_shader_gen.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>
#include <string>
#include "runtime/log.h"
#include "runtime/obf.h"

using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace nj::runtime::gx {

// En-tete GLSL, desktop ou mobile. Tout ce qui suit est legal en GLSL ES 300 : seules changent la
// ligne de version et les precisions, que ES exige. highp n'est pas decoratif : une position de
// salle depasse 32 000 (millimetres), mediump la quantifierait visiblement.
// Variante « sommets tires » du programme en cours de construction (0 : attributs classiques). Posee par le rendu
// autour de la construction (renderer_gl.cpp, pull::) : bit 7 actif, bit 0 normale, 1 couleur, 2 coordonnee,
// 3 normale s8, bits 4-6 type de couleur (decode_color). Les deux nuanceurs d'un programme ont la meme version.
uint16_t g_vs_pull_variant = 0;   // bit 8 : liste compilee (V2)

static const char *glsl_header() {
#ifdef NJ_GLES
    if (g_vs_pull_variant)
        return "#version 320 es\n"
               "precision highp float;\n"
               "precision highp int;\n"
               "precision highp sampler2D;\n"
               "precision highp usamplerBuffer;\n"_g;
    return "#version 300 es\n"
           "precision highp float;\n"
           "precision highp int;\n"
           "precision highp sampler2D;\n"_g;
#else
    return "#version 330 core\n"_g;
#endif
}

std::string glsl_header_string() { return glsl_header(); }

namespace {

// --- sommets ----------------------------------------------------------------------------------

// Entrees, sorties et uniformes du nuanceur de sommets.
const char *kVertexDecls = R"(layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTex0;
layout(location = 3) in vec3 aNrm;
layout(location = 4) in vec3 aObjPos;
layout(location = 5) in vec3 aObjNrm;
layout(location = 6) in vec4 aTexMtxIdxA;
layout(location = 7) in vec4 aTexMtxIdxB;
out vec4 vColor0;
out vec4 vColor1;
out vec3 vTex0;
out vec3 vTex1;
out vec3 vTex2;
out vec3 vTex3;
out vec3 vTex4;
out vec3 vTex5;
out vec3 vTex6;
out vec3 vTex7;
uniform vec4 uTMBank[64];
uniform vec4 uTGPost[24];
uniform mat4 mvp;
uniform mat4 uPosMtx;
uniform mat3 uNrmMtx;
uniform vec4 uMatColor[2];
uniform vec4 uAmbColor[2];
uniform int uChanCtrl[4];
uniform vec4 uLightCol[8];
uniform vec4 uLightPos[8];
uniform vec4 uLightDir[8];
uniform vec4 uLightCosAtt[8];
uniform vec4 uLightDistAtt[8];
)"_g;

// Canal de couleur du XF (registres de controle 0x100E-0x1011, couleurs materiau 0x100C-0x100D et
// ambiante 0x100A-0x100B, lumieres en 0x600) : la couleur rasterisee n'est pas celle du sommet mais
// le materiau (registre ou sommet) module par l'ambiante plus les lumieres choisies. Les jeux
// animent le registre materiau (fondus, clignotements) : l'ignorer les ferait disparaitre.
//   controle : bit 0 materiau du sommet, bit 1 eclaire, bits 2-5 et 11-14 masque des lumieres,
//   bit 6 ambiante du sommet, bits 7-8 fonction diffuse (0 aucune, 1 signe, 2 bornee),
//   bits 9-10 attenuation (1 speculaire, 3 projecteur, sinon aucune).
const char *kChannel = R"(
vec4 gx_channel(int ch, vec3 p, vec3 nrm, vec4 vcol) {
    int cc = uChanCtrl[ch];
    int ac = uChanCtrl[ch + 2];
    vec4 material = vec4((cc & 1) != 0 ? vcol.rgb : uMatColor[ch].rgb,
                         (ac & 1) != 0 ? vcol.a : uMatColor[ch].a);
    bool lit_rgb = (cc & 2) != 0;
    bool lit_a = (ac & 2) != 0;
    vec4 acc = vec4(lit_rgb ? ((cc & 0x40) != 0 ? vcol.rgb : uAmbColor[ch].rgb) : vec3(1.0),
                    lit_a ? ((ac & 0x40) != 0 ? vcol.a : uAmbColor[ch].a) : 1.0);
    if (lit_rgb) {
        int dif = (cc >> 7) & 3;
        int att = (cc >> 9) & 3;
        int mask = ((cc >> 2) & 0xF) | (((cc >> 11) & 0xF) << 4);
        // sommet sans normale : normale nulle (pas de terme diffus), pas normalize(0) = NaN
        vec3 n = (dot(nrm, nrm) > 0.0) ? normalize(nrm) : vec3(0.0);
        for (int i = 0; i < 8; ++i) {
            if ((mask & (1 << i)) == 0) continue;
            vec3 toL;
            float w;
            if (att == 3) {
                // projecteur : polynome de l'angle sur polynome de la distance
                toL = uLightPos[i].xyz - p;
                float d2 = dot(toL, toL);
                float d = sqrt(d2);
                toL = toL / d;
                float c = max(0.0, dot(toL, uLightDir[i].xyz));
                w = max(0.0, uLightCosAtt[i].x + uLightCosAtt[i].y * c +
                             uLightCosAtt[i].z * c * c) /
                    dot(uLightDistAtt[i].xyz, vec3(1.0, d, d2));
            } else if (att == 1) {
                // speculaire : angle demi-vecteur, cote eclaire seulement
                toL = normalize(uLightPos[i].xyz - p);
                float c = (dot(n, toL) >= 0.0) ? max(0.0, dot(n, uLightDir[i].xyz)) : 0.0;
                vec3 kd = (dif == 0) ? uLightDistAtt[i].xyz : normalize(uLightDistAtt[i].xyz);
                w = max(0.0, dot(uLightCosAtt[i].xyz, vec3(1.0, c, c * c))) /
                    dot(kd, vec3(1.0, c, c * c));
            } else {
                toL = uLightPos[i].xyz - p;
                toL = (length(toL) == 0.0) ? n : normalize(toL);
                w = 1.0;
            }
            float nl = (dif == 0) ? 1.0 : ((dif == 1) ? dot(toL, n) : max(0.0, dot(toL, n)));
            acc.rgb += w * nl * uLightCol[i].rgb;
            acc.a += w * nl * uLightCol[i].a;
        }
    }
    return clamp(material * clamp(acc, 0.0, 1.0), 0.0, 1.0);
}
)"_g;

// Generateur de coordonnees de texture i, d'apres XF 0x1040+i (TEXMTXINFO) :
//   bit 1 projection (0 : 2x4, q = 1 ; 1 : 3x4), bit 2 entree (0 : (a, b, 1, 1) ; 1 : (a, b, c, 1)),
//   bits 4-6 type (0 matrice, 1 relief, 2 / 3 couleur 0 / 1), bits 7-11 source (0 position,
//   1 normale, 2 couleurs, 5-12 coordonnees). Double texture (XF 0x1012) : normalisation
//   (POSTMTXINFO bit 8) puis post-matrice 3x4. Coordonnee 0 seulement (sources 6-12 -> 0),
//   binormales et relief : repli sur la coordonnee 0.
void emit_texgen(std::ostream &vs, const GxRegs &st, int i) {
    const uint32_t info = st.xf[0x40 + i];
    const bool projective = (info >> 1) & 1, abc1 = (info >> 2) & 1;
    const int type = (info >> 4) & 7, src = (info >> 7) & 0x1F;
    const bool color_type = type == 2 || type == 3;
    // NJ_TEXGENSTAT (developpement) : chaque generateur dont la source n'est ni la position, ni la
    // normale, ni la couleur, ni TEX0 (une fois par valeur) - sources que le decodeur ne lit pas.
    static const bool s_stat = NJ_ENV("NJ_TEXGENSTAT") != nullptr;
    if (s_stat && !color_type && src != 0 && src != 1 && src != 2 && src != 5) {
        static std::set<uint32_t> seen;
        if (seen.insert(info).second)
            NJ_PRINTF("[TEXGEN] gen %d info %08x : type %d source %d (%s)\n", i, info, type, src,
                      src >= 5 ? "TEXn" : "binormale");
    }
    std::string in;
    if (color_type) in = type == 2 ? "vec4(vColor0.rg, 1.0, 1.0)"_g : "vec4(vColor1.rg, 1.0, 1.0)"_g;
    else if (src == 0) in = abc1 ? "vec4(aObjPos, 1.0)"_g : "vec4(aObjPos.xy, 1.0, 1.0)"_g;
    else if (src == 1) in = abc1 ? "vec4(aObjNrm, 1.0)"_g : "vec4(aObjNrm.xy, 1.0, 1.0)"_g;
    else if (src == 2) in = "vec4(aColor.rg, 1.0, 1.0)"_g;
    else in = "vec4(aTex0, 1.0, 1.0)"_g;
    vs << "    { // generateur "_g << i << " (info "_g << std::hex << info << std::dec << ")\n"_g
       << "        vec4 g = "_g << in << ";\n"_g;
    if (color_type) {
        vs << "        vec3 t = g.xyz;\n"_g;
    } else {
        // matrice choisie par sommet (os des modeles animes), lignes de la banque XF
        vs << "        int m = min(int("_g << (i < 4 ? "aTexMtxIdxA"_g : "aTexMtxIdxB"_g) << "["_g << (i & 3)
           << "] + 0.5), 61);\n"_g
           << "        vec3 t = vec3(dot(uTMBank[m], g), dot(uTMBank[m + 1], g), "_g
           << (projective ? "dot(uTMBank[m + 2], g)"_g : "1.0"_g) << ");\n"_g;
    }
    if (st.xf[0x12] & 1) {
        if ((st.xf[0x50 + i] >> 8) & 1) vs << "        t = normalize(t);\n"_g;
        const int r = 3 * i;
        vs << "        vec4 q = vec4(t, 1.0);\n"_g
           << "        t = vec3(dot(uTGPost["_g << r << "], q), dot(uTGPost["_g << r + 1 << "], q), dot(uTGPost["_g
           << r + 2 << "], q));\n"_g;
    }
    vs << "        vTex"_g << i << " = t;\n    }\n"_g;
}

// Sommets tires (A1, nj-work/docs/REFLEXION_PROFONDE_2026-11-13.md) : les attributs deviennent des variables remplies
// par pull_fetch() depuis les octets bruts du GX (texture tampon R32UI, renderer_gl.cpp pull::). Meme resultat que
// ncp::decode + put_vertex (fifo_parser.cpp, renderer_gl.cpp). Index GL = (dessin << 16) | sommet du dessin ;
// fiche du dessin (16 mots a uPullRec + 16 x dessin) : 0 debut du flux, 1 taille de sommet, 2-9 (biais, pas) position,
// normale, couleur, coordonnee, 10 echelle position, 11 echelle coordonnee, 12-13 indices de matrices de texture.
// Bit 8 de la variante : liste compilee (V2) - flux d'index resident (uPullRes) et parametres de l'appel en uniformes
// (uLArr decalages des tableaux, uLStr pas, uLMisc = debut du flux, taille de sommet, indices de matrices de texture A/B,
// uLScale = echelles position / coordonnee) ; sans lui, fiche par dessin dans l'anneau (A1).
static void emit_pull_decls(std::ostream &vs, uint16_t v) {
    const bool hn = v & 1, hc = v & 2, ht = v & 4, n8 = v & 8, list = v & 0x100;
    const int cc = (v >> 4) & 7;
    vs << "uniform highp usamplerBuffer uPullBuf;\n"
          "vec3 aPos; vec4 aColor; vec2 aTex0; vec3 aNrm; vec3 aObjPos; vec3 aObjNrm; vec4 aTexMtxIdxA; vec4 aTexMtxIdxB;\n"
          "uint pw(int w) { return texelFetch(uPullBuf, w).r; }\n"
          "uint pb(uint o) { return (pw(int(o >> 2u)) >> ((o & 3u) * 8u)) & 0xFFu; }\n"
          "uint pbe16(uint o) { return (pb(o) << 8u) | pb(o + 1u); }\n"
          "float ple16s(uint o) { return float(int((pb(o) | (pb(o + 1u) << 8u)) << 16u) >> 16); }\n"
          "float ps8(uint o) { return float(int(pb(o) << 24u) >> 24); }\n"
          "vec4 pidx(uint t) { return vec4(float(t & 255u), float((t >> 8u) & 255u), float((t >> 16u) & 255u), float(t >> 24u)); }\n"_g;
    if (list)
        vs << "uniform highp usamplerBuffer uPullRes;\nuniform ivec4 uLArr;\nuniform ivec4 uLStr;\nuniform ivec4 uLMisc;\n"
              "uniform vec2 uLScale;\n"
              "uint rb(uint o) { return (texelFetch(uPullRes, int(o >> 2u)).r >> ((o & 3u) * 8u)) & 0xFFu; }\n"
              "uint sbe16(uint o) { return (rb(o) << 8u) | rb(o + 1u); }\n"
              "uint pat(int r, int a, uint i) { return uint(uLArr[a]) + i * uint(uLStr[a]); }\n"
              "float spos(int r) { return uLScale.x; }\nfloat stex(int r) { return uLScale.y; }\n"
              "uint tida(int r) { return uint(uLMisc.z); }\nuint tidb(int r) { return uint(uLMisc.w); }\n"
              "void pull_fetch() {\n"
              "    int r = 0;\n"
              "    uint so = uint(uLMisc.x) + uint(gl_VertexID) * uint(uLMisc.y);\n"_g;
    else
        vs << "uniform int uPullRec;\nuniform int uPullDat;\n"
              "uint sbe16(uint o) { return pbe16(o); }\n"
              "uint pat(int r, int a, uint i) { return pw(r + 2 + 2 * a) + i * pw(r + 3 + 2 * a); }\n"
              "float spos(int r) { return uintBitsToFloat(pw(r + 10)); }\nfloat stex(int r) { return uintBitsToFloat(pw(r + 11)); }\n"
              "uint tida(int r) { return pw(r + 12); }\nuint tidb(int r) { return pw(r + 13); }\n"
              "void pull_fetch() {\n"
              "    uint d = uint(gl_VertexID) >> 16u, vi = uint(gl_VertexID) & 0xFFFFu;\n"
              "    int r = uPullRec + int(d) * 16;\n"
              "    uint so = uint(uPullDat) + pw(r) + vi * pw(r + 1);\n"_g;
    vs << "    uint pa = pat(r, 0, sbe16(so)); so += 2u;\n"
          "    aPos = vec3(ple16s(pa), ple16s(pa + 2u), ple16s(pa + 4u)) * spos(r);\n"
          "    aObjPos = aPos;\n"
          "    aNrm = vec3(0.0); aObjNrm = vec3(0.0); aColor = vec4(1.0); aTex0 = vec2(0.0);\n"_g;
    if (hn) {
        vs << "    uint na = pat(r, 1, sbe16(so)); so += 2u;\n"_g;
        if (n8) vs << "    aNrm = vec3(ps8(na), ps8(na + 1u), ps8(na + 2u)); aObjNrm = aNrm * (1.0 / 64.0);\n"_g;
        else vs << "    aNrm = vec3(ple16s(na), ple16s(na + 2u), ple16s(na + 4u)); aObjNrm = aNrm * (1.0 / 16384.0);\n"_g;
    }
    if (hc) {
        vs << "    uint ca = pat(r, 2, sbe16(so)); so += 2u;\n"_g;
        if (cc == 0)        // RGB565, gros-boutiste comme decode_color
            vs << "    uint c = pbe16(ca); aColor = vec4(float((c >> 11u) & 31u) / 31.0, float((c >> 5u) & 63u) / 63.0, "
                  "float(c & 31u) / 31.0, 1.0);\n"_g;
        else if (cc == 3)   // RGBA4
            vs << "    uint c = pbe16(ca); aColor = vec4(float((c >> 12u) & 15u) / 15.0, float((c >> 8u) & 15u) / 15.0, "
                  "float((c >> 4u) & 15u) / 15.0, float(c & 15u) / 15.0);\n"_g;
        else if (cc == 4)   // RGBA6, trois octets
            vs << "    uint c = (pb(ca) << 16u) | (pb(ca + 1u) << 8u) | pb(ca + 2u); aColor = vec4(float((c >> 18u) & 63u) / 63.0, "
                  "float((c >> 12u) & 63u) / 63.0, float((c >> 6u) & 63u) / 63.0, float(c & 63u) / 63.0);\n"_g;
        else if (cc >= 5)   // RGBA8
            vs << "    aColor = vec4(float(pb(ca)) / 255.0, float(pb(ca + 1u)) / 255.0, float(pb(ca + 2u)) / 255.0, "
                  "float(pb(ca + 3u)) / 255.0);\n"_g;
        else                // RGB8 / RGBX8
            vs << "    aColor = vec4(float(pb(ca)) / 255.0, float(pb(ca + 1u)) / 255.0, float(pb(ca + 2u)) / 255.0, 1.0);\n"_g;
    }
    if (ht)
        vs << "    uint ta = pat(r, 3, sbe16(so)); so += 2u;\n"
              "    aTex0 = vec2(ple16s(ta), ple16s(ta + 2u)) * stex(r);\n"_g;
    vs << "    aTexMtxIdxA = pidx(tida(r)); aTexMtxIdxB = pidx(tidb(r));\n}\n"_g;
}

std::string vertex_shader(const GxRegs &st) {
    std::ostringstream vs;
    if (g_vs_pull_variant) {
        const std::string decls = kVertexDecls;
        const size_t cut = decls.find("out vec4 vColor0;"_g);
        vs << glsl_header() << decls.substr(cut == std::string::npos ? 0 : cut);
        emit_pull_decls(vs, g_vs_pull_variant);
        vs << kChannel;
    } else {
        vs << glsl_header() << kVertexDecls << kChannel;
    }
    // eclairage en espace camera : transformer avant les canaux, pas seulement avant la projection
    vs << "void main() {\n"_g;
    if (g_vs_pull_variant) vs << "    pull_fetch();\n"_g;
    vs << "    vec3 p = (uPosMtx * vec4(aPos, 1.0)).xyz;\n"
          "    vec3 nrm = uNrmMtx * aNrm;\n"
          "    gl_Position = mvp * vec4(p, 1.0);\n"_g;
#ifndef NJ_GLES
    if (NJ_ENV("NJ_GLESCLIP"_g) != nullptr) {   // diagnostic : meme bornage que GL ES
        vs << "    if (mvp[2][3] == 0.0) gl_Position.z = clamp(gl_Position.z, -gl_Position.w, gl_Position.w);\n"_g;
    } else
    // Le GX decoupe contre near et far ; sans decoupe, une camera qui frole un modele ramenait tout
    // a la profondeur 0 (visage vu a travers les cheveux, salle 3-17). Perspective : m[2][3] != 0.
    vs << "    bool persp = mvp[2][3] != 0.0;\n"
          "    gl_ClipDistance[0] = persp ? gl_Position.z + gl_Position.w : 1.0;\n"
          "    gl_ClipDistance[1] = persp ? gl_Position.w - gl_Position.z : 1.0;\n"_g;
#else
    // GL ES (13/10) : pas de depth clamp ni de distances de decoupe. Le GPU decoupe near / far comme
    // le GX pour les dessins en perspective ; les passes orthographiques (plein ecran), que le GX
    // garde quelle que soit leur profondeur, sont bornees ici (ce que fait le clamp du PC).
    vs << "    if (mvp[2][3] == 0.0) gl_Position.z = clamp(gl_Position.z, -gl_Position.w, gl_Position.w);\n"_g;
#endif
    vs << "    vColor0 = gx_channel(0, p, nrm, aColor);\n"
          "    vColor1 = gx_channel(1, p, nrm, aColor);\n"_g;
    const int gens = st.n_texgen > 8 ? 8 : st.n_texgen;
    for (int i = 0; i < 8; ++i) {
        if (i < gens) emit_texgen(vs, st, i);
        else vs << "    vTex"_g << i << " = vec3(aTex0, 1.0);\n"_g;
    }
    vs << "}\n"_g;
    std::string out = vs.str();
    // Diagnostic (14/11, chateau noir) : NJ_NOLIGHT=1 coupe l'eclairage des canaux (materiau seul) ;
    // NJ_LIGHTDIAG=1 remplace la somme des lumieres par 1 (eclairage plein) pour voir si les lumieres valent 0.
    static const bool nolight = NJ_ENV("NJ_NOLIGHT"_g) != nullptr, fullacc = NJ_ENV("NJ_LIGHTDIAG"_g) != nullptr;
    if (nolight || fullacc) {
        const std::string k = "bool lit_a = (ac & 2) != 0;"_g;
        const size_t at = out.find(k);
        if (at != std::string::npos)
            out.insert(at + k.size(), nolight ? " lit_rgb = false; lit_a = false;"_g : ""_g);
        if (fullacc) {
            const std::string r = "return clamp(material * clamp(acc, 0.0, 1.0), 0.0, 1.0);"_g;
            const size_t at2 = out.find(r);
            if (at2 != std::string::npos) out.replace(at2, r.size(), "return clamp(material, 0.0, 1.0);"_g);
        }
    }
    return out;
}

// --- fragments --------------------------------------------------------------------------------

const char *kFragmentDecls = R"(in vec4 vColor0;
in vec4 vColor1;
in vec3 vTex0;
in vec3 vTex1;
in vec3 vTex2;
in vec3 vTex3;
in vec3 vTex4;
in vec3 vTex5;
in vec3 vTex6;
in vec3 vTex7;
out vec4 FragColor;
uniform sampler2D uTex0;
uniform sampler2D uTex1;
uniform sampler2D uTex2;
uniform sampler2D uTex3;
uniform sampler2D uTex4;
uniform sampler2D uTex5;
uniform sampler2D uTex6;
uniform sampler2D uTex7;
uniform vec2 uAlphaRef;
uniform vec4 uIndMtx[6];
uniform vec4 uTexSize[8];
uniform vec4 uTevColor[4];
uniform vec4 uTevKColor[4];
uniform vec4 uFogF;
uniform ivec4 uFogI;
uniform mat4 mvp;   // brouillard : projection (meme uniforme que le sommet)
uniform vec4 uFogColor;
vec4 reg[4];
vec4 texel;
vec4 ras;
)"_g;
// uIndMtx : 2 lignes par matrice indirecte, .w de la premiere = 2^exposant ; uTexSize : taille en
// texels (.xy) et echelle vers [0, 1] (.zw). uFogF : A, C ; uFogI : B (mantisse, decalage).

// Entrees du combinateur couleur (0-15) et alpha (0-7) ; 14 / 6 : constante choisie (KSEL).
const char *kColorIn[16] = {
    "reg[0].rgb"_g, "vec3(reg[0].a)"_g, "reg[1].rgb"_g, "vec3(reg[1].a)"_g, "reg[2].rgb"_g, "vec3(reg[2].a)"_g,
    "reg[3].rgb"_g, "vec3(reg[3].a)"_g, "texel.rgb"_g,  "vec3(texel.a)"_g,  "ras.rgb"_g,    "vec3(ras.a)"_g,
    "vec3(1.0)"_g,  "vec3(0.5)"_g,      nullptr,      "vec3(0.0)"_g,
};
const char *kAlphaIn[8] = {"reg[0].a"_g, "reg[1].a"_g, "reg[2].a"_g, "reg[3].a"_g, "texel.a"_g, "ras.a"_g, nullptr, "0.0"_g};
const char *kComp[4] = {"r"_g, "g"_g, "b"_g, "a"_g};

// Constante d'etage (KSEL) : 0-7 fractions 8/8..1/8, 0x0C-0x0F couleur K, 0x10-0x1F composante.
std::string konst_color(int k) {
    char buf[40];
    if (k <= 7) {
        std::snprintf(buf, sizeof buf, "vec3(%.6f)"_g, (8 - k) / 8.0f);
        return buf;
    }
    if (k >= 0x0C && k <= 0x0F) return "uTevKColor["_g + std::to_string(k - 0x0C) + "].rgb"_g;
    if (k >= 0x10) return "vec3(uTevKColor["_g + std::to_string((k - 0x10) % 4) + "]."_g + kComp[(k - 0x10) / 4] + ")"_g;
    return "vec3(0.0)"_g;
}
std::string konst_alpha(int k) {
    char buf[40];
    if (k <= 7) {
        std::snprintf(buf, sizeof buf, "%.6f"_g, (8 - k) / 8.0f);
        return buf;
    }
    if (k >= 0x10) return "uTevKColor["_g + std::to_string((k - 0x10) % 4) + "]."_g + kComp[(k - 0x10) / 4];
    return "0.0"_g;
}

// Combinateur d'un canal : d (+|-) mix(a, b, c) + biais, puis echelle, puis borne eventuelle.
void emit_combiner(std::ostream &fs, const char *type, const char *out, const std::string in[4], int op, int bias,
                   int scale, bool clamp_out, const std::string &dest) {
    static const char *kBias[3] = {"0.0"_g, "0.5"_g, "-0.5"_g};
    static const char *kScale[4] = {nullptr, "2.0"_g, "4.0"_g, "0.5"_g};
    const std::string b = std::string(type) == "float"_g ? kBias[bias < 3 ? bias : 0]
                                                       : std::string("vec3("_g) + kBias[bias < 3 ? bias : 0] + ")"_g;
    fs << "        "_g << type << " "_g << out << " = "_g << in[3] << (op == 1 ? " - "_g : " + "_g) << "mix("_g << in[0] << ", "_g
       << in[1] << ", "_g << in[2] << ") + "_g << b << ";\n"_g;
    if (scale >= 1 && scale <= 3) fs << "        "_g << out << " *= "_g << kScale[scale] << ";\n"_g;
    if (clamp_out) fs << "        "_g << out << " = clamp("_g << out << ", 0.0, 1.0);\n"_g;
    fs << "        "_g << dest << " = "_g << out << ";\n"_g;
}

// Lecture de texture d'un etage, directe ou decalee par un etage indirect (BP 0x10+i : etage
// indirect, format 8/5/4/3 bits, biais, matrice, repli des coordonnees, cumul avec le precedent).
void emit_texture(std::ostream &fs, const GxRegs &st, int i, const TevStage &s, int indirect_stages) {
    const int tc = s.tex_coord, map = s.tex_map;
    const uint32_t cmd = st.bp[0x10 + i];
    const int bt = cmd & 3, fmt = (cmd >> 2) & 3, bias = (cmd >> 4) & 7;
    const int mtx = (cmd >> 9) & 0xF, wrap_s = (cmd >> 13) & 7, wrap_t = (cmd >> 16) & 7;
    const bool add_prev = (cmd >> 20) & 1;
    // division projective (generateur 3x4), q = 0 traite comme 1
    fs << "        vec2 uv = vTex"_g << tc << ".xy / (vTex"_g << tc << ".z != 0.0 ? vTex"_g << tc << ".z : 1.0);\n"_g;
    const bool indirect = indirect_stages > 0 && bt < indirect_stages &&
                          (mtx != 0 || wrap_s != 0 || wrap_t != 0 || add_prev);
    if (!indirect) {
        fs << "        texel = texture(uTex"_g << map << ", uv * uTexSize["_g << map << "].zw);\n"_g;
        return;
    }
    // decalage en texels ; matrices statiques 1-3 seulement (dynamiques : pas de decalage)
    static const int kBits[4] = {8, 5, 4, 3};
    static const int kWrap[8] = {0, 256, 128, 64, 32, 16, 0, 0};
    fs << "        vec3 iv = ind"_g << bt << ";\n"_g;
    if (fmt != 0) fs << "        iv = mod(iv, "_g << (1 << kBits[fmt]) << ".0);\n"_g;
    for (int b = 0; b < 3; ++b)
        if (bias & (1 << b)) fs << "        iv."_g << "xyz"_g[b] << " += "_g << (fmt == 0 ? "-128.0"_g : "1.0"_g) << ";\n"_g;
    fs << "        vec2 shift = vec2(0.0);\n"_g;
    if (mtx >= 1 && mtx <= 3) {
        const int m = 2 * (mtx - 1);
        fs << "        shift = vec2(dot(uIndMtx["_g << m << "].xyz, iv), dot(uIndMtx["_g << m + 1 << "].xyz, iv)) * uIndMtx["_g
           << m << "].w;\n"_g;
    }
    if (add_prev) fs << "        shift += indCarry;\n"_g;
    fs << "        indCarry = shift;\n"_g
       << "        vec2 st = uv * uTexSize["_g << map << "].xy;\n"_g;
    if (wrap_s == 6) fs << "        st.x = 0.0;\n"_g;
    else if (kWrap[wrap_s]) fs << "        st.x = mod(st.x, "_g << kWrap[wrap_s] << ".0);\n"_g;
    if (wrap_t == 6) fs << "        st.y = 0.0;\n"_g;
    else if (kWrap[wrap_t]) fs << "        st.y = mod(st.y, "_g << kWrap[wrap_t] << ".0);\n"_g;
    fs << "        texel = texture(uTex"_g << map << ", (st + shift) / uTexSize["_g << map << "].xy * uTexSize["_g << map
       << "].zw);\n"_g;
}

// Tables de permutation des canaux (13/10) : table n = R, G dans les bits 0-1 / 2-3 de KSEL 2n
// (BP 0xF6 + 2n), B, A dans ceux de KSEL 2n + 1 ; chaque etage choisit une table pour le
// rasteriseur (bits 0-1) et une pour la texture (bits 2-3) dans son combinateur alpha (BP 0xC1 +
// 2 x etage). Le lecteur video (sofdec.cpp) lit U et V dans une texture IA8 par ces tables :
// sans elles, teinte fausse des videos.
std::string swizzle(const GxRegs &st, int table) {
    static const char kCh[4] = {'r', 'g', 'b', 'a'};
    const uint32_t lo = st.bp[0xF6 + 2 * table], hi = st.bp[0xF7 + 2 * table];
    // registres jamais ecrits (bit 31, fifo_parser.cpp) : ordre normal ; ecrits a zero = rouge partout
    if (!((lo | hi) & 0x80000000u)) return std::string();
    std::string s = ".";
    s += kCh[lo & 3];
    s += kCh[(lo >> 2) & 3];
    s += kCh[hi & 3];
    s += kCh[(hi >> 2) & 3];
    return s == ".rgba" ? std::string() : s;
}

void emit_stage(std::ostream &fs, const GxRegs &st, int i, int indirect_stages) {
    const TevStage &s = st.tev[i];
    fs << "    { // etage "_g << i << "\n"_g;
    static const char *kRas[3] = {"vColor0"_g, "vColor1"_g, "vec4(0.0, 0.0, 0.0, 1.0)"_g};
    const uint32_t swaps = st.bp[0xC1 + 2 * i];
    fs << "        ras = "_g << kRas[s.chan < 2 ? s.chan : 2] << ";\n"_g;
    if (s.chan < 2) {
        const std::string rs = swizzle(st, swaps & 3);
        if (!rs.empty()) fs << "        ras = ras"_g << rs << ";\n"_g;
    }
    if (s.tex_map != 0xFF) {
        emit_texture(fs, st, i, s, indirect_stages);
        const std::string ts = swizzle(st, (swaps >> 2) & 3);
        if (!ts.empty()) fs << "        texel = texel"_g << ts << ";\n"_g;
    } else {
        fs << "        texel = vec4(1.0);\n"_g;
    }
    // constantes : deux etages par registre KSEL (BP 0xF6-0xFD)
    const uint32_t ksel = st.bp[0xF6 + i / 2] >> (i % 2 ? 14 : 4);
    std::string cin[4], ain[4];
    const uint8_t csel[4] = {s.color.in[0], s.color.in[1], s.color.in[2], s.color.in[3]};
    const uint8_t asel[4] = {s.alpha.in[0], s.alpha.in[1], s.alpha.in[2], s.alpha.in[3]};
    for (int k = 0; k < 4; ++k) {
        cin[k] = csel[k] == 14 ? konst_color(ksel & 0x1F) : csel[k] < 16 ? kColorIn[csel[k]] : "vec3(0.0)"_g;
        ain[k] = asel[k] == 6 ? konst_alpha((ksel >> 5) & 0x1F) : asel[k] < 8 ? kAlphaIn[asel[k]] : "0.0"_g;
    }
    emit_combiner(fs, "vec3"_g, "c"_g, cin, s.color.op, s.color.bias, s.color.scale, s.color.clamp == 1,
                  "reg["_g + std::to_string(s.color.dest) + "].rgb"_g);
    emit_combiner(fs, "float"_g, "a"_g, ain, s.alpha.op, s.alpha.bias, s.alpha.scale, s.alpha.clamp == 1,
                  "reg["_g + std::to_string(s.alpha.dest) + "].a"_g);
    fs << "    }\n"_g;
}

// Brouillard (BP 0xEE-0xF2) : profondeur oeil tiree de la profondeur fenetre sur 24 bits
// (perspective : A / (B - (z >> decalage)), orthographique : A * z), moins C, selon le type, puis
// melange sur 8 bits vers la couleur. Ajustement de plage (BP 0xE8) non applique.
void emit_fog(std::ostream &fs, const GxRegs &st) {
    const uint32_t f = st.bp[0xF1];
    const uint32_t type = (f >> 21) & 7;
    if (type < 2 || nj::runtime::gfx::g_no_fog) return;   // 21/10 : option « Disable fog » (tres vieux telephones)
    fs << "    {\n        int z24 = int(gl_FragCoord.z * 16777216.0);\n"_g;
    if ((f >> 20) & 1)
        fs << "        float ze = uFogF.x * float(z24) / 16777216.0;\n"_g;
    else
        // 16/11 : en perspective, B - z = -p5 / d (d = distance a l'oeil = 1 / gl_FragCoord.w, p5 = mvp[3][2] / 2),
        // donc ze = A' * d / (-p5) avec A' = A * 2^decalage : meme valeur, sans soustraire deux nombres presque egaux.
        // L'ancienne forme perd tout sur l'Adreno 420 du S5 (brouillard de r100 absent) ; gardee en orthographique.
        fs << "        float ze;\n"
              "        if (mvp[2][3] != 0.0 && mvp[3][2] != 0.0)\n"
              "            ze = uFogF.x * exp2(float(uFogI.w)) * (1.0 / gl_FragCoord.w) / (-0.5 * mvp[3][2]);\n"
              "        else\n"
              "            ze = (uFogF.x * 16777216.0) / float(uFogI.y - (z24 >> uFogI.w));\n"_g;
    fs << "        float k = clamp(ze - uFogF.y, 0.0, 1.0);\n"_g;
    switch (type) {
    case 4: fs << "        k = 1.0 - pow(2.0, -8.0 * k);\n"_g; break;                 // exponentiel
    case 5: fs << "        k = 1.0 - pow(2.0, -8.0 * k * k);\n"_g; break;             // exp. carre
    case 6: fs << "        k = pow(2.0, -8.0 * (1.0 - k));\n"_g; break;               // exp. inverse
    case 7: fs << "        k = 1.0 - k;\n        k = pow(2.0, -8.0 * k * k);\n"_g; break;
    default: break;                                                                  // lineaire
    }
    fs << "        float k8 = floor(k * 256.0 + 0.5);\n"
          "        vec3 col8 = floor(clamp(FragColor.rgb, 0.0, 1.0) * 255.0 + 0.5);\n"
          "        FragColor.rgb = floor((col8 * (256.0 - k8) + floor(uFogColor.rgb * 255.0 + 0.5) * k8) / 256.0)"
          " / 255.0;\n    }\n"_g;
}

// Test alpha : deux comparaisons a la reference (uniforme : RE4 la fait balayer a chaque fondu,
// une valeur ecrite dans la source compilerait un nuanceur par valeur), combinees par et / ou / xor
// / xnor.
void emit_alpha_test(std::ostream &fs, const AlphaCmp &at) {
    if (at.cmp[0] == 7 && at.cmp[1] == 7 && at.op == 0) return;   // toujours vrai
    static const char *kCmp[8] = {"false"_g, "<"_g, "=="_g, "<="_g, ">"_g, "!="_g, ">="_g, "true"_g};
    auto cmp = [](int c, const char *ref) -> std::string {
        if (c == 0 || c >= 7) return kCmp[c >= 7 ? 7 : 0];
        return std::string("FragColor.a "_g) + kCmp[c] + " "_g + ref;
    };
    const std::string a = cmp(at.cmp[0], "uAlphaRef.x"_g), b = cmp(at.cmp[1], "uAlphaRef.y"_g);
    std::string pass;
    switch (at.op) {
    case 0: pass = "("_g + a + ") && ("_g + b + ")"_g; break;
    case 1: pass = "("_g + a + ") || ("_g + b + ")"_g; break;
    case 2: pass = "(("_g + a + ") != ("_g + b + "))"_g; break;
    default: pass = "(("_g + a + ") == ("_g + b + "))"_g; break;
    }
    fs << "    if (!("_g << pass << ")) discard;\n"_g;
}

std::string fragment_shader(const GxRegs &st) {
    std::ostringstream fs;
    fs << glsl_header() << kFragmentDecls << "void main() {\n"_g;
    for (int r = 0; r < 4; ++r) fs << "    reg["_g << r << "] = uTevColor["_g << r << "];\n"_g;
    // etages indirects : texture de perturbation (s, t, u = alpha, bleu, vert sur 8 bits), coordonnee
    // divisee par 2^echelle (BP 0x25 / 0x26)
    const int indirect = (int)((st.bp[0x00] >> 16) & 7);
    for (int k = 0; k < indirect && k < 4; ++k) {
        const uint32_t iref = st.bp[0x27];
        const int map = (iref >> (6 * k)) & 7, tc = (iref >> (6 * k + 3)) & 7;
        const uint32_t sc = st.bp[k < 2 ? 0x25 : 0x26] >> (8 * (k & 1));
        fs << "    vec3 ind"_g << k << " = floor(texture(uTex"_g << map << ", (vTex"_g << tc << ".xy / (vTex"_g << tc
           << ".z != 0.0 ? vTex"_g << tc << ".z : 1.0)) / vec2("_g << (1 << (sc & 0xF)) << ".0, "_g << (1 << ((sc >> 4) & 0xF))
           << ".0) * uTexSize["_g << map << "].zw).abg * 255.0 + 0.5);\n"_g;
    }
    fs << "    vec2 indCarry = vec2(0.0);\n"_g;
    const int stages = st.n_tev ? st.n_tev : 1;
    for (int i = 0; i < stages; ++i) emit_stage(fs, st, i, indirect);
    fs << "    FragColor = reg[0];\n"_g;
    emit_fog(fs, st);
    emit_alpha_test(fs, st.atest);
    fs << "}\n"_g;
    return fs.str();
}

}  // namespace

ShaderPair build_tev_program(const GxRegs &state, uint8_t) {
    ShaderPair out;
    out.vs_text = vertex_shader(state);
    out.fs_text = fragment_shader(state);
    return out;
}

}  // namespace nj::runtime::gx
