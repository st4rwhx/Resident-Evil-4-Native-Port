// Presentation par moyenne de surface (option « Area » du menu F10).
//
// glBlitFramebuffer en GL_LINEAR ne lit que 2 x 2 texels par pixel de la fenetre : a la
// resolution interne 8x reduite a 1080 lignes, un pixel en couvre environ 2,5 x 2,5 et le reste
// est ignore, d'ou le crenelage et le scintillement des details fins (feuillage, grillages).
// Ici chaque pixel de la fenetre fait la moyenne ponderee de tous les texels que son empreinte
// couvre, recouvrement partiel compris, en lumiere lineaire (gamma 2,2) pour que les bords clairs
// ne s'amincissent pas. En agrandissement (empreinte < 1 texel), c'est un bilineaire ordinaire.
#include "runtime/gfx_settings.h"
#include "runtime/log.h"

#include "common/gl.h"

#include <cstdio>
#include <string>
#include "runtime/obf.h"

using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace nj::runtime::gfx {

namespace {

GLuint g_prog = 0, g_vao = 0;
GLint g_u_src = -1, g_u_size = -1, g_u_tex = -1;
bool g_failed = false;

GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        NJ_EPRINTF("[PresentArea] nuanceur refuse : %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool build() {
#ifdef NJ_GLES
    const char *head = "#version 300 es\nprecision highp float;\nprecision highp int;\n"_g;
#else
    const char *head = "#version 330 core\n"_g;
#endif
    // Triangle couvrant la zone de destination (glViewport), coordonnee 0..1 dans uv.
    static const char *vs_body =
        "out vec2 uv;\n"
        "void main() {\n"
        "  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
        "  uv = p;\n"
        "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n"_g;
    // u_src = (x0, y0, x1, y1) en texels ; u_size = taille de la destination en pixels.
    static const char *fs_body =
        "in vec2 uv;\n"
        "out vec4 color;\n"
        "uniform sampler2D u_tex;\n"
        "uniform vec4 u_src;\n"
        "uniform vec2 u_size;\n"
        "vec3 lin(vec3 c) { return pow(c, vec3(2.2)); }\n"
        "void main() {\n"
        "  vec2 span = u_src.zw - u_src.xy;\n"
        "  vec2 p = u_src.xy + uv * span;\n"
        "  vec2 fp = abs(span) / u_size;\n"
        "  vec2 ts = vec2(textureSize(u_tex, 0));\n"
        "  if (fp.x <= 1.0 && fp.y <= 1.0) { color = vec4(texture(u_tex, p / ts).rgb, 1.0); return; }\n"
        "  fp = max(fp, vec2(1.0));\n"
        "  vec2 lo = p - 0.5 * fp, hi = p + 0.5 * fp;\n"
        "  ivec2 a = ivec2(floor(lo)), b = ivec2(ceil(hi)) - 1;\n"
        "  ivec2 mx = ivec2(ts) - 1;\n"
        "  vec3 sum = vec3(0.0); float wsum = 0.0;\n"
        "  for (int y = a.y; y <= b.y && y <= a.y + 15; ++y) {\n"
        "    float wy = min(hi.y, float(y + 1)) - max(lo.y, float(y));\n"
        "    for (int x = a.x; x <= b.x && x <= a.x + 15; ++x) {\n"
        "      float w = wy * (min(hi.x, float(x + 1)) - max(lo.x, float(x)));\n"
        "      sum += w * lin(texelFetch(u_tex, clamp(ivec2(x, y), ivec2(0), mx), 0).rgb);\n"
        "      wsum += w;\n"
        "    }\n"
        "  }\n"
        "  color = vec4(pow(sum / max(wsum, 1e-6), vec3(1.0 / 2.2)), 1.0);\n"
        "}\n"_g;
    std::string vs = std::string(head) + vs_body, fs = std::string(head) + fs_body;
    GLuint v = compile(GL_VERTEX_SHADER, vs.c_str()), f = compile(GL_FRAGMENT_SHADER, fs.c_str());
    if (!v || !f)
        return false;
    g_prog = glCreateProgram();
    glAttachShader(g_prog, v);
    glAttachShader(g_prog, f);
    glLinkProgram(g_prog);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(g_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        NJ_EPRINTF("[PresentArea] programme refuse\n");
        return false;
    }
    g_u_src = glGetUniformLocation(g_prog, "u_src"_g);
    g_u_size = glGetUniformLocation(g_prog, "u_size"_g);
    g_u_tex = glGetUniformLocation(g_prog, "u_tex"_g);
    glGenVertexArrays(1, &g_vao);
    return true;
}

}  // namespace

bool present_area(unsigned tex, int tex_w, int tex_h, int sx0, int sy0, int sx1, int sy1,
                  int dx0, int dy0, int dx1, int dy1, unsigned draw_fbo) {
    (void) tex_w;
    (void) tex_h;
    if (g_failed)
        return false;
    if (!g_prog && !build()) {
        g_failed = true;
        return false;
    }
    // Etat GL touche ici, remis tel quel ensuite (le rendu du jeu reprend derriere).
    GLint prog = 0, vao = 0, unit = 0, bound = 0, vp[4], draw_fb = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &unit);
    glGetIntegerv(GL_VIEWPORT, vp);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fb);
    const GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST),
                    scissor = glIsEnabled(GL_SCISSOR_TEST), cull = glIsEnabled(GL_CULL_FACE);
    GLboolean mask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);

    nj_fbo(GL_DRAW_FRAMEBUFFER, draw_fbo);
    nj_cap(GL_BLEND, false);
    nj_cap(GL_DEPTH_TEST, false);
    nj_cap(GL_SCISSOR_TEST, false);
    nj_cap(GL_CULL_FACE, false);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(dx0, dy0, dx1 - dx0, dy1 - dy0);
    glUseProgram(g_prog);
    glBindVertexArray(g_vao);
    nj_tex2d(tex);
    // texelFetch ignore le filtre ; le chemin d'agrandissement, lui, veut du lineaire sans mipmap.
    GLint min_f = 0, mag_f = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_f);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag_f);
    nj_tex_filter(GL_LINEAR);
    glUniform1i(g_u_tex, 0);
    glUniform4f(g_u_src, (float) sx0, (float) sy0, (float) sx1, (float) sy1);
    glUniform2f(g_u_size, (float) (dx1 - dx0), (float) (dy1 - dy0));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_f);
    nj_tex_mag(mag_f);

    glBindTexture(GL_TEXTURE_2D, (GLuint) bound);
    glActiveTexture((GLenum) unit);
    glBindVertexArray((GLuint) vao);
    glUseProgram((GLuint) prog);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint) draw_fb);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
    if (blend) nj_cap(GL_BLEND, true);
    if (depth) nj_cap(GL_DEPTH_TEST, true);
    if (scissor) nj_cap(GL_SCISSOR_TEST, true);
    if (cull) nj_cap(GL_CULL_FACE, true);
    return true;
}

}  // namespace nj::runtime::gfx
