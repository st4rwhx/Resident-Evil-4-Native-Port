// -*- coding: utf-8 -*-
// Couche GX du portage : le processeur graphique du GameCube vers OpenGL ES 3.
//
// Premiere tranche, volontairement etroite mais **complete de bout en bout** : etat, capture des
// sommets, transformation, rasterisation, et une image relisible. Les textures et TEV viendront
// par-dessus, sur une chaine deja verifiee, plutot que tout d'un coup.
//
// Trois choses a comprendre pour que la suite se lise :
//
//  * **la position ouvre le sommet.** GX n'a pas de "fin de sommet" : le jeu appelle
//    GXPosition3f32 puis les attributs actifs, dans l'ordre canonique du materiel. On accumule
//    donc dans un sommet courant, et une nouvelle position (ou GXEnd) valide le precedent.
//    C'est ce qui permet de ne pas dependre du nombre d'attributs actifs.
//
//  * **les matrices du GameCube sont 3x4, en lignes.** La matrice de position est une
//    transformation affine sans ligne de projection ; OpenGL en veut une 4x4 en colonnes. La
//    conversion est transposee **et** completee, et c'est une source classique de scenes vides.
//
//  * **GX_QUADS n'existe pas en ES.** On le convertit en deux triangles a l'indexation, pas en
//    dupliquant les sommets : le jeu en emet beaucoup pour l'interface.
#include "port_log.h"

#include <cstdio>
#include <cstring>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <dolphin.h>

namespace {

// ---------------------------------------------------------------- contexte

EGLDisplay g_dpy = EGL_NO_DISPLAY;
EGLSurface g_surf = EGL_NO_SURFACE;
EGLContext g_ctx = EGL_NO_CONTEXT;
bool g_gl_ready = false;
u32 g_fb_w = 640, g_fb_h = 448;

// ---------------------------------------------------------------- etat GX

struct Vertex {
    float x, y, z;
    unsigned char r, g, b, a;
    float u, v;
};

const int MAX_VERTS = 65536;
Vertex g_verts[MAX_VERTS];
int g_nverts = 0;

Vertex g_cur;              // sommet en cours d'assemblage
bool g_cur_open = false;   // une position a ete recue depuis le dernier commit
GXPrimitive g_prim = GX_TRIANGLES;
int g_prim_first = 0;      // premier sommet de la primitive en cours

float g_proj[16];
float g_modelview[16];
bool g_proj_ortho = false;

GXCullMode g_cull = GX_CULL_NONE;
bool g_ztest = true, g_zwrite = true;
GXCompare g_zfunc = GX_LEQUAL;
bool g_blend = false;
GXColor g_clear = {0, 0, 0, 255};

GLuint g_prog = 0, g_vao = 0, g_vbo = 0, g_ibo = 0;
GLint g_u_mvp = -1;
unsigned short g_idx[MAX_VERTS * 3];

// ---------------------------------------------------------------- matrices

void mtx_identity(float *m)
{
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// GameCube : 3x4 en lignes, affine. OpenGL : 4x4 en colonnes. Transposer **et** completer.
void mtx34_to_gl(const f32 src[3][4], float *dst)
{
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 3; ++r)
            dst[c * 4 + r] = src[r][c];
    dst[3] = dst[7] = dst[11] = 0.0f;
    dst[15] = 1.0f;
}

void mtx44_to_gl(const f32 src[4][4], float *dst)
{
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            dst[c * 4 + r] = src[r][c];
}

void mtx_mul(const float *a, const float *b, float *out)
{
    float t[16];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float s = 0;
            for (int k = 0; k < 4; ++k)
                s += a[k * 4 + r] * b[c * 4 + k];
            t[c * 4 + r] = s;
        }
    std::memcpy(out, t, sizeof(t));
}

// ---------------------------------------------------------------- shaders

const char *VS =
    "#version 300 es\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec4 aCol;\n"
    "layout(location=2) in vec2 aUV;\n"
    "uniform mat4 uMVP;\n"
    "out vec4 vCol;\n"
    "out vec2 vUV;\n"
    "void main(){ vCol = aCol; vUV = aUV; gl_Position = uMVP * vec4(aPos,1.0); }\n";

const char *FS =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec4 vCol;\n"
    "in vec2 vUV;\n"
    "out vec4 oCol;\n"
    "void main(){ oCol = vCol; }\n";

GLuint make_shader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {0};
        glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
        std::printf("[PORT] GX : shader refuse : %s\n", log);
    }
    return s;
}

GLenum gl_compare(GXCompare f)
{
    switch (f) {
    case GX_NEVER:   return GL_NEVER;
    case GX_LESS:    return GL_LESS;
    case GX_EQUAL:   return GL_EQUAL;
    case GX_LEQUAL:  return GL_LEQUAL;
    case GX_GREATER: return GL_GREATER;
    case GX_NEQUAL:  return GL_NOTEQUAL;
    case GX_GEQUAL:  return GL_GEQUAL;
    default:         return GL_ALWAYS;
    }
}

void commit_vertex()
{
    if (g_cur_open && g_nverts < MAX_VERTS)
        g_verts[g_nverts++] = g_cur;
    g_cur_open = false;
}

} // namespace

// ---------------------------------------------------------------- initialisation

extern "C" bool port_gx_init_offscreen(u32 w, u32 h)
{
    if (g_gl_ready)
        return true;
    g_fb_w = w;
    g_fb_h = h;

    g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_dpy == EGL_NO_DISPLAY)
        return false;
    eglInitialize(g_dpy, nullptr, nullptr);

    const EGLint ca[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                          EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                          EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                          EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE };
    EGLConfig cfg;
    EGLint n = 0;
    if (eglChooseConfig(g_dpy, ca, &cfg, 1, &n) != EGL_TRUE || n == 0)
        return false;

    const EGLint sa[] = { EGL_WIDTH, (EGLint)w, EGL_HEIGHT, (EGLint)h, EGL_NONE };
    g_surf = eglCreatePbufferSurface(g_dpy, cfg, sa);
    const EGLint xa[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, xa);
    if (eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx) != EGL_TRUE)
        return false;

    GLuint vs = make_shader(GL_VERTEX_SHADER, VS);
    GLuint fs = make_shader(GL_FRAGMENT_SHADER, FS);
    g_prog = glCreateProgram();
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glLinkProgram(g_prog);
    g_u_mvp = glGetUniformLocation(g_prog, "uMVP");

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glGenBuffers(1, &g_ibo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *)12);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)16);

    mtx_identity(g_proj);
    mtx_identity(g_modelview);
    g_gl_ready = true;
    std::printf("[PORT] GX : %s, %ux%u hors ecran\n", (const char *)glGetString(GL_RENDERER), w, h);
    return true;
}

extern "C" void port_gx_read_pixels(int x, int y, int w, int h, void *dst)
{
    glFinish();
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dst);
}

// ---------------------------------------------------------------- API GX

GXFifoObj *GXInit(void *base, u32 size)
{
    (void)base; (void)size;
    port_gx_init_offscreen(g_fb_w, g_fb_h);
    return nullptr;   // le jeu ne fait que le ranger ; il n'y a pas de FIFO ici
}

void GXClearVtxDesc(void) {}
void GXSetVtxDesc(GXAttr attr, GXAttrType type) { (void)attr; (void)type; }
void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac)
{ (void)vtxfmt; (void)attr; (void)cnt; (void)type; (void)frac; }

void GXBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts)
{
    (void)vtxfmt; (void)nverts;
    g_prim = type;
    g_prim_first = g_nverts;
    g_cur_open = false;
    std::memset(&g_cur, 0, sizeof(g_cur));
    g_cur.r = g_cur.g = g_cur.b = g_cur.a = 255;
}

// La position ouvre un sommet : une nouvelle position valide le precedent.
void GXPosition3f32(f32 x, f32 y, f32 z)
{
    commit_vertex();
    g_cur.x = x; g_cur.y = y; g_cur.z = z;
    g_cur.u = g_cur.v = 0.0f;
    g_cur.r = g_cur.g = g_cur.b = g_cur.a = 255;
    g_cur_open = true;
}

void GXColor4u8(u8 r, u8 g, u8 b, u8 a) { g_cur.r = r; g_cur.g = g; g_cur.b = b; g_cur.a = a; }
void GXTexCoord2f32(f32 u, f32 v) { g_cur.u = u; g_cur.v = v; }
void GXNormal3f32(f32 x, f32 y, f32 z) { (void)x; (void)y; (void)z; }

void port_gx_end(void)
{
    commit_vertex();
    const int n = g_nverts - g_prim_first;
    if (n < 1 || !g_gl_ready)
        return;

    // Indexation plutot que duplication : GX_QUADS et les eventails n'existent pas en ES, mais
    // les convertir a l'indexation evite de recopier les sommets, et le jeu en emet beaucoup.
    int ni = 0;
    const int f = g_prim_first;
    switch (g_prim) {
    case GX_TRIANGLES:
        for (int i = 0; i + 2 < n; i += 3) {
            g_idx[ni++] = (unsigned short)(f + i);
            g_idx[ni++] = (unsigned short)(f + i + 1);
            g_idx[ni++] = (unsigned short)(f + i + 2);
        }
        break;
    case GX_TRIANGLESTRIP:
        for (int i = 0; i + 2 < n; ++i) {
            // L'ordre alterne, sinon une face sur deux part a l'envers et disparait au culling.
            if (i & 1) {
                g_idx[ni++] = (unsigned short)(f + i + 1);
                g_idx[ni++] = (unsigned short)(f + i);
            } else {
                g_idx[ni++] = (unsigned short)(f + i);
                g_idx[ni++] = (unsigned short)(f + i + 1);
            }
            g_idx[ni++] = (unsigned short)(f + i + 2);
        }
        break;
    case GX_TRIANGLEFAN:
        for (int i = 1; i + 1 < n; ++i) {
            g_idx[ni++] = (unsigned short)f;
            g_idx[ni++] = (unsigned short)(f + i);
            g_idx[ni++] = (unsigned short)(f + i + 1);
        }
        break;
    case GX_QUADS:
        for (int i = 0; i + 3 < n; i += 4) {
            g_idx[ni++] = (unsigned short)(f + i);
            g_idx[ni++] = (unsigned short)(f + i + 1);
            g_idx[ni++] = (unsigned short)(f + i + 2);
            g_idx[ni++] = (unsigned short)(f + i);
            g_idx[ni++] = (unsigned short)(f + i + 2);
            g_idx[ni++] = (unsigned short)(f + i + 3);
        }
        break;
    default:
        return;   // lignes et points : plus tard
    }
    if (ni == 0)
        return;

    float mvp[16];
    mtx_mul(g_proj, g_modelview, mvp);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(g_nverts * sizeof(Vertex)), g_verts, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(ni * sizeof(unsigned short)), g_idx, GL_STREAM_DRAW);

    glUseProgram(g_prog);
    glUniformMatrix4fv(g_u_mvp, 1, GL_FALSE, mvp);

    if (g_ztest) { glEnable(GL_DEPTH_TEST); glDepthFunc(gl_compare(g_zfunc)); }
    else glDisable(GL_DEPTH_TEST);
    glDepthMask(g_zwrite ? GL_TRUE : GL_FALSE);

    // Sens des faces : GX definit la face avant en **horaire**, OpenGL en anti-horaire
    // (confirme par opengx, le wrapper GL->GX de davidgfnet : "CCW is front for OpenGL while
    // front facing is defined CW in GX"). Cela appellerait une inversion... mais la projection
    // du GameCube a Y vers le bas, alors que le framebuffer GL a Y vers le haut : ce
    // retournement inverse deja le sens apparent. **Les deux inversions s'annulent**, et la
    // correspondance est donc directe.
    //
    // Mesure : avec une inversion manuelle en plus, le quadrilatere en bande disparaissait
    // entierement - trois inversions valent une. C'est le genre d'erreur qui ne produit aucune
    // erreur GL et se lit seulement comme "la scene est vide".
    switch (g_cull) {
    case GX_CULL_NONE:  glDisable(GL_CULL_FACE); break;
    case GX_CULL_FRONT: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
    case GX_CULL_BACK:  glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
    default:            glEnable(GL_CULL_FACE); glCullFace(GL_FRONT_AND_BACK); break;
    }

    if (g_blend) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
    else glDisable(GL_BLEND);

    glDrawElements(GL_TRIANGLES, ni, GL_UNSIGNED_SHORT, nullptr);
}

// ---------------------------------------------------------------- transformations

void GXSetProjection(const f32 mtx[4][4], GXProjectionType type)
{
    mtx44_to_gl(mtx, g_proj);
    g_proj_ortho = (type == GX_ORTHOGRAPHIC);
}

void GXLoadPosMtxImm(const f32 mtx[3][4], u32 id) { (void)id; mtx34_to_gl(mtx, g_modelview); }

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz)
{
    if (!g_gl_ready)
        return;
    // L'origine de l'ecran est en haut a gauche sur GameCube, en bas a gauche en OpenGL.
    glViewport((GLint)left, (GLint)((float)g_fb_h - top - ht), (GLsizei)wd, (GLsizei)ht);
    glDepthRangef(nearz, farz);
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht)
{
    if (!g_gl_ready)
        return;
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)left, (GLint)(g_fb_h - top - ht), (GLsizei)wd, (GLsizei)ht);
}

// ---------------------------------------------------------------- etat pixel

void GXSetCullMode(GXCullMode mode) { g_cull = mode; }

void GXSetZMode(GXBool compare_enable, GXCompare func, GXBool update_enable)
{
    g_ztest = compare_enable != 0;
    g_zfunc = func;
    g_zwrite = update_enable != 0;
}

void GXSetBlendMode(GXBlendMode type, GXBlendFactor src, GXBlendFactor dst, GXLogicOp op)
{
    (void)src; (void)dst; (void)op;
    g_blend = (type == GX_BM_BLEND);
}

void GXSetCopyClear(GXColor clear_clr, u32 clear_z) { (void)clear_z; g_clear = clear_clr; }

// Sur le materiel, l'EFB est resolu vers le XFB. Ici on rend directement dans la surface, donc
// il n'y a rien a copier ; l'effacement de l'image suivante est ce qui compte.
void GXCopyDisp(void *dest, GXBool clear)
{
    (void)dest;
    if (!g_gl_ready)
        return;
    glFinish();
    if (clear) {
        glClearColor(g_clear.r / 255.0f, g_clear.g / 255.0f, g_clear.b / 255.0f, g_clear.a / 255.0f);
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    g_nverts = 0;
}

void GXSetNumChans(u8 n) { (void)n; }
void GXSetNumTexGens(u8 n) { (void)n; }
void GXSetNumTevStages(u8 n) { (void)n; }
void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color)
{ (void)stage; (void)coord; (void)map; (void)color; }
