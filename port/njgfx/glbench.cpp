// -*- coding: utf-8 -*-
// Banc des couts GL du pilote (NJ_GLBENCH=1, une fois, apres l'initialisation du rendu ; 13/11/2026).
// Question 4 de SYNTHESE_RECHERCHE_PROFONDE_2026-11-13.md : combien coute, cote CPU, chaque appel typique sur
// l'Adreno 420 (aucun chiffre public). Modele : flip_gl_bench du port Melee (dessin, liaison, programme, uniforme).
// Rendu hors ecran dans un petit framebuffer, contexte GL courant fourni par l'appelant ; rien n'est modifie ensuite.
#include <GLES3/gl3.h>
#include <chrono>
#include <cstdio>
#include <vector>
#include <cstring>

namespace {
using clk = std::chrono::steady_clock;
double us_since(clk::time_point t0) { return std::chrono::duration<double, std::micro>(clk::now() - t0).count(); }

GLuint compile(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}
GLuint program(float tint)
{
    static const char* vs = "#version 300 es\nlayout(location=0) in vec3 p; uniform vec4 u; void main(){ gl_Position = vec4(p*0.5 + u.xyz*0.0, 1.0); }";
    char fs[256];
    std::snprintf(fs, sizeof fs, "#version 300 es\nprecision mediump float; uniform sampler2D t; uniform vec4 u; out vec4 o; void main(){ o = texture(t, vec2(0.5))*u + vec4(%f); }", tint);
    GLuint p = glCreateProgram();
    glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
    glBindAttribLocation(p, 0, "p");
    glLinkProgram(p);
    return p;
}
}  // namespace

extern "C" void njgfx_glbench(void)
{
    GLint prev_fbo = 0, prev_prog = 0, prev_vao = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
    // cible 256x256
    GLuint tex_rt, fbo;
    glGenTextures(1, &tex_rt);
    glBindTexture(GL_TEXTURE_2D, tex_rt);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_rt, 0);
    glViewport(0, 0, 256, 256);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    // deux textures 64x64
    GLuint tx[2];
    glGenTextures(2, tx);
    std::vector<unsigned char> px(64 * 64 * 4, 200);
    for (GLuint t : tx) {
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    // maillage : 100 triangles
    std::vector<float> v;
    std::vector<unsigned short> idx;
    for (int i = 0; i < 100; ++i) {
        const float x = (i % 10) * 0.2f - 1.0f, y = (i / 10) * 0.2f - 1.0f;
        const unsigned short b = (unsigned short) (v.size() / 3);
        v.insert(v.end(), {x, y, 0, x + 0.1f, y, 0, x, y + 0.1f, 0});
        idx.insert(idx.end(), {b, (unsigned short) (b + 1), (unsigned short) (b + 2)});
    }
    GLuint vao, vbo, ibo, stream;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 2, idx.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &stream);
    const GLuint p0 = program(0.0f), p1 = program(0.01f);
    const GLint u0 = glGetUniformLocation(p0, "u"), u1 = glGetUniformLocation(p1, "u");
    glUseProgram(p0);
    glUniform4f(u0, 1, 1, 1, 1);
    glUseProgram(p1);
    glUniform4f(u1, 1, 1, 1, 1);
    glUseProgram(p0);
    glBindTexture(GL_TEXTURE_2D, tx[0]);
    const int N = 2000;
    auto draw = [&]() { glDrawElements(GL_TRIANGLES, 300, GL_UNSIGNED_SHORT, nullptr); };
    // echauffement
    for (int i = 0; i < 200; ++i) draw();
    glFinish();

    struct Row { const char* name; double cpu_us, total_us; };
    std::vector<Row> rows;
    auto measure = [&](const char* name, auto&& body) {
        glFinish();
        const auto t0 = clk::now();
        for (int i = 0; i < N; ++i) body(i);
        const double cpu = us_since(t0);
        glFinish();
        const double total = us_since(t0);
        rows.push_back({name, cpu / N, total / N});
    };
    measure("dessin seul (100 triangles)", [&](int) { draw(); });
    measure("dessin + liaison de texture alternee", [&](int i) { glBindTexture(GL_TEXTURE_2D, tx[i & 1]); draw(); });
    measure("dessin + programme alterne", [&](int i) { glUseProgram(i & 1 ? p1 : p0); draw(); });
    glUseProgram(p0);
    measure("dessin + glUniform4f", [&](int i) { glUniform4f(u0, 1, 1, 1, (float) (i & 7) * 0.1f + 0.3f); draw(); });
    // envoi dans un tampon que le GPU vient de lire (meme zone)
    std::vector<float> chunk(256, 0.5f);
    glBindBuffer(GL_ARRAY_BUFFER, stream);
    glBufferData(GL_ARRAY_BUFFER, 1 << 20, nullptr, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
    measure("dessin + glBufferSubData 1 Ko (tampon lu)", [&](int) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, 1024, chunk.data());
        draw();
    });
    measure("dessin + glBufferSubData 1 Ko (zone neuve, anneau)", [&](int i) {
        glBufferSubData(GL_ARRAY_BUFFER, (i % 1000) * 1024, 1024, chunk.data());
        draw();
    });
    measure("dessin + glBufferData 64 Ko (orphelin)", [&](int) {
        glBufferData(GL_ARRAY_BUFFER, 65536, nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 1024, chunk.data());
        draw();
    });
    measure("dessin + map non synchronise 1 Ko (anneau)", [&](int i) {
        void* p = glMapBufferRange(GL_ARRAY_BUFFER, (i % 1000) * 1024, 1024,
                                   GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
        if (p) { std::memcpy(p, chunk.data(), 1024); glUnmapBuffer(GL_ARRAY_BUFFER); }
        draw();
    });
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
    measure("changement de framebuffer + effacement + dessin", [&](int i) {
        glBindFramebuffer(GL_FRAMEBUFFER, i & 1 ? fbo : (GLuint) prev_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClear(GL_COLOR_BUFFER_BIT);
        draw();
    });
    std::printf("[GLBENCH] Adreno : %s / %s\n", (const char*) glGetString(GL_RENDERER), (const char*) glGetString(GL_VERSION));
    const char* ext = (const char*) glGetString(GL_EXTENSIONS);
    std::printf("[GLBENCH] buffer_storage=%d multi_draw_indirect=%d draw_indirect(ES3.1)=%d texture_array=1\n",
                ext && std::strstr(ext, "GL_EXT_buffer_storage") ? 1 : 0,
                ext && std::strstr(ext, "multi_draw_indirect") ? 1 : 0,
                (const char*) glGetString(GL_VERSION) && std::strstr((const char*) glGetString(GL_VERSION), "3.1") ? 1 : 0);
    for (const auto& r : rows)
        std::printf("[GLBENCH] %-48s CPU %7.2f us/appel, avec attente GPU %7.2f us\n", r.name, r.cpu_us, r.total_us);
    GLint mub = 0, mvu = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &mub);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &mvu);
    std::printf("[GLBENCH] GL_MAX_UNIFORM_BLOCK_SIZE=%d GL_MAX_VERTEX_UNIFORM_VECTORS=%d\n", mub, mvu);
    // nettoyage
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    glDeleteBuffers(1, &stream);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(2, tx);
    glDeleteTextures(1, &tex_rt);
    glDeleteFramebuffers(1, &fbo);
    glDeleteProgram(p0);
    glDeleteProgram(p1);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) prev_fbo);
    glUseProgram((GLuint) prev_prog);
    glBindVertexArray((GLuint) prev_vao);
}
