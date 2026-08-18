// -*- coding: utf-8 -*-
// A0 (documentation interne) : sommets tires par le nuanceur, inclus par glbench.cpp.
// Octets bruts au format GC (positions s16 gros-boutistes, pas 6, lues par un index u16 gros-boutiste du flux, comme
// nos listes) dans des textures tampon R32UI ; le nuanceur decode (texelFetch). Compare au chemin classique (flottants
// decodes par le CPU) : cout GPU et image identique (readPixels). Modele : Aurora 2026 (lib/gx/shader.cpp raw_fetch_*).
#ifndef __APPLE__   // iOS : pas d'EGL ni de tampons de texture (GLES 3.0 d'Apple) : banc inactif
#include <EGL/egl.h>
#else
static void* eglGetProcAddress(const char*) { return nullptr; }
#endif

namespace {
typedef void (*PFN_TEXBUF)(GLenum, GLenum, GLuint);
const GLenum kTB = 0x8C2A;   // GL_TEXTURE_BUFFER

GLuint prog_src(const char* vs, const char* fs, char* log, int nlog)
{
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    GLint ok = 0;
    glGetShaderiv(v, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(v, nlog, nullptr, log); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "p");
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(p, nlog, nullptr, log); return 0; }
    return p;
}
}  // namespace

static void njgfx_glbench_pull()
{
#ifdef __APPLE__
    return;
#endif
    const char* ext = (const char*) glGetString(GL_EXTENSIONS);
    GLint vssbo = -1, tbsize = -1, vtex = -1;
    glGetIntegerv(0x90D6, &vssbo);   // GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS
    glGetIntegerv(0x8C2B, &tbsize);  // GL_MAX_TEXTURE_BUFFER_SIZE
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &vtex);
    while (glGetError() != GL_NO_ERROR) {}
    std::printf("[GLBENCH] A0 : ssbo au sommet=%d, texture tampon max=%d texels, unites de texture au sommet=%d, "
                "EXT_texture_buffer=%d OES=%d\n",
                vssbo, tbsize, vtex, ext && std::strstr(ext, "GL_EXT_texture_buffer") ? 1 : 0,
                ext && std::strstr(ext, "GL_OES_texture_buffer") ? 1 : 0);
    PFN_TEXBUF texbuf = (PFN_TEXBUF) eglGetProcAddress("glTexBuffer");
    if (!texbuf) texbuf = (PFN_TEXBUF) eglGetProcAddress("glTexBufferEXT");
    if (!texbuf) texbuf = (PFN_TEXBUF) eglGetProcAddress("glTexBufferOES");
    if (!texbuf) { std::printf("[GLBENCH] A0 : glTexBuffer introuvable\n"); return; }

    // grille de G x G carres, NV sommets distincts, flux d'index (cote GL : non indexe, gl_VertexID)
    const int G = 100, NV = (G + 1) * (G + 1), NT = G * G * 2, NI = NT * 3;
    const float scale = 1.0f / 1024.0f;   // facteur d'echelle GC (decalage 10)
    std::vector<unsigned char> arr(NV * 6 + 8, 0), str(NI * 2 + 8, 0);
    std::vector<float> flat(NI * 3);
    auto put16 = [](unsigned char* d, int v) {
        d[0] = (unsigned char) ((v >> 8) & 0xFF);
        d[1] = (unsigned char) (v & 0xFF);
    };
    for (int y = 0; y <= G; ++y)
        for (int x = 0; x <= G; ++x) {
            const int i = y * (G + 1) + x;
            put16(&arr[i * 6 + 0], (x * 2048) / G - 1024 + ((i * 37) % 7) - 3);   // negatifs compris
            put16(&arr[i * 6 + 2], (y * 2048) / G - 1024);
            put16(&arr[i * 6 + 4], (i * 13) % 2000 - 1000);
        }
    auto s16 = [&](int off) { return (short) ((arr[off] << 8) | arr[off + 1]); };
    int k = 0;
    for (int y = 0; y < G; ++y)
        for (int x = 0; x < G; ++x) {
            const int a = y * (G + 1) + x, b = a + 1, c = a + G + 1, d = c + 1;
            const int quad[6] = {a, b, c, b, d, c};
            for (int idx : quad) {
                put16(&str[k * 2], idx);
                flat[k * 3 + 0] = s16(idx * 6) * scale;
                flat[k * 3 + 1] = s16(idx * 6 + 2) * scale;
                flat[k * 3 + 2] = s16(idx * 6 + 4) * scale;
                ++k;
            }
        }
    static const char* fs =
        "#version 300 es\nprecision highp float; in vec3 c; out vec4 o; void main(){ o = vec4(fract(c * 8.0), 1.0); }";
    static const char* vs_attr =
        "#version 300 es\nlayout(location=0) in vec3 p; out vec3 c;\n"
        "void main(){ c = p; gl_Position = vec4(p.xy, p.z * 0.5, 1.0); }";
    static const char* vs_pull =
        "#version 320 es\nprecision highp float; precision highp int;\n"
        "uniform highp usamplerBuffer arr; uniform highp usamplerBuffer str; uniform float scale; out vec3 c;\n"
        "uint a8(uint o){ return (texelFetch(arr, int(o >> 2u)).r >> ((o & 3u) * 8u)) & 0xFFu; }\n"
        "uint s8(uint o){ return (texelFetch(str, int(o >> 2u)).r >> ((o & 3u) * 8u)) & 0xFFu; }\n"
        "float as16(uint o){ return float((int((a8(o) << 8u) | a8(o + 1u)) << 16) >> 16); }\n"
        "void main(){ uint vi = uint(gl_VertexID) * 2u; uint idx = (s8(vi) << 8u) | s8(vi + 1u); uint b = idx * 6u;\n"
        "  vec3 p = vec3(as16(b), as16(b + 2u), as16(b + 4u)) * scale; c = p;\n"
        "  gl_Position = vec4(p.xy, p.z * 0.5, 1.0); }";
    static const char* fs32 =
        "#version 320 es\nprecision highp float; in vec3 c; out vec4 o; void main(){ o = vec4(fract(c * 8.0), 1.0); }";
    static const char* vs_ssbo =
        "#version 320 es\nprecision highp float; precision highp int;\n"
        "layout(std430, binding = 0) readonly buffer A { uint wa[]; };\n"
        "layout(std430, binding = 1) readonly buffer S { uint ws[]; };\n"
        "uniform float scale; out vec3 c;\n"
        "uint a8(uint o){ return (wa[o >> 2u] >> ((o & 3u) * 8u)) & 0xFFu; }\n"
        "uint s8(uint o){ return (ws[o >> 2u] >> ((o & 3u) * 8u)) & 0xFFu; }\n"
        "float as16(uint o){ return float((int((a8(o) << 8u) | a8(o + 1u)) << 16) >> 16); }\n"
        "void main(){ uint vi = uint(gl_VertexID) * 2u; uint idx = (s8(vi) << 8u) | s8(vi + 1u); uint b = idx * 6u;\n"
        "  vec3 p = vec3(as16(b), as16(b + 2u), as16(b + 4u)) * scale; c = p;\n"
        "  gl_Position = vec4(p.xy, p.z * 0.5, 1.0); }";
    char log[1024] = {0};
    const GLuint pa = prog_src(vs_attr, fs, log, sizeof log);
    const GLuint pp = pa ? prog_src(vs_pull, fs32, log, sizeof log) : 0;
    if (!pa || !pp) { std::printf("[GLBENCH] A0 : nuanceur refuse : %s\n", log); return; }
    char log2[1024] = {0};
    const GLuint ps = prog_src(vs_ssbo, fs32, log2, sizeof log2);
    if (!ps) std::printf("[GLBENCH] A0 : nuanceur SSBO refuse : %s\n", log2);

    GLint prev_fbo = 0, prev_prog = 0, prev_vao = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
    GLuint rt, fbo;
    glGenTextures(1, &rt);
    glBindTexture(GL_TEXTURE_2D, rt);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt, 0);
    glViewport(0, 0, 256, 256);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);   // meme ordre de dessin dans les deux chemins : image comparable sans profondeur
    GLuint vao, vbo, bufs[2], tex[2];
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (flat.size() * 4), flat.data(), GL_STATIC_DRAW);
    glGenBuffers(2, bufs);
    glGenTextures(2, tex);
    const std::vector<unsigned char>* src[2] = {&arr, &str};
    for (int i = 0; i < 2; ++i) {
        glBindBuffer(kTB, bufs[i]);
        glBufferData(kTB, (GLsizeiptr) (src[i]->size() & ~(size_t) 3), src[i]->data(), GL_STATIC_DRAW);
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(kTB, tex[i]);
        texbuf(kTB, GL_R32UI, bufs[i]);
    }
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(pp);
    glUniform1i(glGetUniformLocation(pp, "arr"), 1);
    glUniform1i(glGetUniformLocation(pp, "str"), 2);
    glUniform1f(glGetUniformLocation(pp, "scale"), scale);
    if (ps) {
        glUseProgram(ps);
        glUniform1f(glGetUniformLocation(ps, "scale"), scale);
        glBindBufferBase(0x90D2, 0, bufs[0]);   // GL_SHADER_STORAGE_BUFFER
        glBindBufferBase(0x90D2, 1, bufs[1]);
    }
    std::vector<unsigned char> img[3];
    double us[3] = {0, 0, 0};
    const GLuint progs[3] = {pa, pp, ps};
    for (int path = 0; path < 3; ++path) {
        if (!progs[path]) continue;
        glUseProgram(progs[path]);
        glBindVertexArray(vao);
        if (path) {
            glDisableVertexAttribArray(0);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
        }
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, NI);
        glFinish();
        img[path].resize(256 * 256 * 4);
        glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, img[path].data());
        const int R = 40;
        const auto t0 = clk::now();
        for (int r = 0; r < R; ++r) glDrawArrays(GL_TRIANGLES, 0, NI);
        glFinish();
        us[path] = us_since(t0) / R;
    }
    long diff = 0, diff2 = -1, lit = 0;
    if (ps) diff2 = 0;
    for (size_t i = 0; i < img[0].size(); i += 4) {
        diff += std::memcmp(&img[0][i], &img[1][i], 4) != 0;
        if (ps) diff2 += std::memcmp(&img[0][i], &img[2][i], 4) != 0;
        lit += (img[0][i] | img[0][i + 1]) != 0;
    }
    std::printf("[GLBENCH] A0 : %d sommets/dessin ; classique %.1f us ; texture tampon %.1f us (%.2f ns/sommet, "
                "pixels differents %ld) ; SSBO %.1f us (%.2f ns/sommet, pixels differents %ld) ; allumes %ld/65536 ; "
                "erreur GL %#x\n",
                NI, us[0], us[1], us[1] * 1000.0 / NI, diff, us[2], us[2] * 1000.0 / NI, diff2, lit, glGetError());
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(2, bufs);
    glDeleteTextures(2, tex);
    glDeleteTextures(1, &rt);
    glDeleteFramebuffers(1, &fbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(pa);
    glDeleteProgram(pp);
    if (ps) glDeleteProgram(ps);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) prev_fbo);
    glUseProgram((GLuint) prev_prog);
    glBindVertexArray((GLuint) prev_vao);
}
