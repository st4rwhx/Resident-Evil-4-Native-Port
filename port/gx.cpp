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
#include <time.h>
#include "port_log.h"

extern "C" unsigned int port_gx_bound_texture(int unit);



#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <dolphin.h>

#include "wgpipe.h"

namespace {

// ---------------------------------------------------------------- contexte

EGLDisplay g_dpy = EGL_NO_DISPLAY;
EGLSurface g_surf = EGL_NO_SURFACE;
EGLContext g_ctx = EGL_NO_CONTEXT;
bool g_gl_ready = false;
void tev_reset();
u32 g_fb_w = 640, g_fb_h = 448;

// ---------------------------------------------------------------- etat GX

struct Vertex {
    float x, y, z;
    unsigned char r, g, b, a;       // couleur 0
    unsigned char r1, g1, b1, a1;   // couleur 1
    float uv[4][2];                 // coordonnees de texture 0..3
};

const int MAX_VERTS = 65536;
Vertex g_verts[MAX_VERTS];
int g_nverts = 0;
long g_draws = 0;      // appels de dessin reellement emis, remis a zero a chaque image
long g_tris = 0;

Vertex g_cur;              // sommet en cours d'assemblage
bool g_cur_open = false;   // une position a ete recue depuis le dernier commit
GXPrimitive g_prim = GX_TRIANGLES;
int g_prim_first = 0;      // premier sommet de la primitive en cours
int g_prim_want = 0;       // sommets annonces par GXBegin ; 0 = pas de primitive ouverte
int g_tex_seq = 0, g_clr_seq = 0;   // rang du prochain attribut texture / couleur du sommet
int g_last_attr = -1;      // dernier attribut du descripteur : celui qui clot un sommet

float g_proj[16];
float g_modelview[16];
bool g_proj_ortho = false;

GXCullMode g_cull = GX_CULL_NONE;
unsigned char g_blend_type = 0, g_blend_src = 4, g_blend_dst = 5;   // GX_BL_SRCALPHA, GX_BL_INVSRCALPHA
bool g_ztest = true, g_zwrite = true;
GXCompare g_zfunc = GX_LEQUAL;
bool g_blend = false;
GXColor g_clear = {0, 0, 0, 255};

u8 g_frac_pos = 0, g_frac_tex = 0;   // bits fractionnaires des attributs entiers

GXAttrType g_desc[GX_VA_MAX_ATTR] = { GX_NONE };
struct ArrDesc { const unsigned char *base; unsigned stride; };
ArrDesc g_arr[GX_VA_MAX_ATTR];
unsigned char g_ctype[GX_VA_MAX_ATTR];   // GX_U8..GX_F32 (ou format de couleur)
unsigned char g_cfrac[GX_VA_MAX_ATTR];
u8 g_ncomp[GX_VA_MAX_ATTR] = { 0 };      // composantes par attribut, depuis GXSetVtxAttrFmt

int g_stream_attr = -1;                  // attribut en cours dans le flux direct
int g_stream_comp = 0;                   // composante en cours dans cet attribut
float g_stream_val[4] = { 0, 0, 0, 0 };

// L'ordre du materiel, celui dans lequel les composantes arrivent.
const GXAttr STREAM_ORDER[] = {
    GX_VA_PNMTXIDX, GX_VA_TEX0MTXIDX, GX_VA_TEX1MTXIDX, GX_VA_TEX2MTXIDX,
    GX_VA_TEX3MTXIDX, GX_VA_TEX4MTXIDX, GX_VA_TEX5MTXIDX, GX_VA_TEX6MTXIDX,
    GX_VA_TEX7MTXIDX, GX_VA_POS, GX_VA_NRM, GX_VA_CLR0, GX_VA_CLR1,
    GX_VA_TEX0, GX_VA_TEX1, GX_VA_TEX2, GX_VA_TEX3,
    GX_VA_TEX4, GX_VA_TEX5, GX_VA_TEX6, GX_VA_TEX7
};
const int STREAM_N = (int) (sizeof(STREAM_ORDER) / sizeof(STREAM_ORDER[0]));

int comps_of(GXAttr a)
{
    if (g_ncomp[a])
        return g_ncomp[a];
    // Valeurs par defaut du materiel quand le jeu n'a pas appele GXSetVtxAttrFmt.
    if (a == GX_VA_POS) return 3;
    if (a == GX_VA_NRM) return 3;
    if (a == GX_VA_CLR0 || a == GX_VA_CLR1) return 4;
    if (a >= GX_VA_TEX0 && a <= GX_VA_TEX7) return 2;
    return 1;
}

// Avance jusqu'au prochain attribut **direct** du descripteur, en repartant du debut quand le
// sommet est complet.
void stream_next(int from)
{
    for (int i = from; i < STREAM_N; ++i) {
        const GXAttr a = STREAM_ORDER[i];
        if (g_desc[a] == GX_DIRECT) {
            g_stream_attr = i;
            g_stream_comp = 0;
            return;
        }
    }
    g_stream_attr = -1;
}

inline float fixed_to_f(int v, u8 frac) { return (float) v / (float) (1 << frac); }

GLuint g_prog = 0, g_vao = 0, g_vbo = 0, g_ibo = 0;
const size_t kVboBytes = 2u << 20, kIboBytes = 1u << 20;
size_t g_voff = 0, g_ioff = 0;   // remplissage des grands tampons pendant l'image

// Rend les tampons au pilote (nouvel espace, sans attendre que le GPU ait fini l'ancien) et repart de 0.
void port_gx_orphan()
{
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)kVboBytes, nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)kIboBytes, nullptr, GL_STREAM_DRAW);
    g_voff = g_ioff = 0;
}
GLint g_u_mvp = -1, g_u_tex = -1, g_u_texon = -1;
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

// Etage unique en attendant TEV : couleur de sommet, multipliee par la texture quand il y en a
// une. TEV en a jusqu'a seize et sait bien davantage ; ce shader est la marche intermediaire qui
// rend les textures visibles sans prejuger de la generation a venir.
const char *FS =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec4 vCol;\n"
    "in vec2 vUV;\n"
    "uniform sampler2D uTex;\n"
    "uniform bool uTexOn;\n"
    "out vec4 oCol;\n"
    "void main(){ oCol = uTexOn ? vCol * texture(uTex, vUV) : vCol; }\n";

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

// **Le jeu n appelle jamais GXEnd.** Sur GameCube c est une macro vide : GXBegin annonce le
// nombre de sommets, et le materiel termine la primitive des qu il les a recus. Le portage
// attendait un appel qui ne vient pas - id_sys, l ecran-titre, toute l interface emettaient
// leurs quatre sommets dans le vide, et l image restait noire alors que la table
// d ordonnancement executait bien ses 96 entrees par image.
//
// Un sommet est complet quand son **dernier attribut declare** arrive : l ordre est celui du
// materiel (indices de matrice, position, normale, couleurs, coordonnees de texture), et le
// descripteur courant dit lesquels sont presents. C est la meme regle que suit deja le flux
// direct par le write-gather pipe.
void attr_done(GXAttr a);

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
    g_u_tex = glGetUniformLocation(g_prog, "uTex");
    g_u_texon = glGetUniformLocation(g_prog, "uTexOn");

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glGenBuffers(1, &g_ibo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)kVboBytes, nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)kIboBytes, nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *)12);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *)16);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)(20 + 8 * i));
    }

    mtx_identity(g_proj);
    mtx_identity(g_modelview);
    tev_reset();
    g_gl_ready = true;
    std::printf("[PORT] GX : %s, %ux%u hors ecran\n", (const char *)glGetString(GL_RENDERER), w, h);
    return true;
}

static void gx_replay();

extern "C" void port_gx_read_pixels(int x, int y, int w, int h, void *dst)
{
    gx_replay();
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

// ------------------------------------------------- descripteur de sommet et flux direct
//
// Une partie du jeu - le rendu de texte, le module de debogage - n'appelle pas les fonctions
// d'attribut : elle ecrit les composantes **a la suite** dans le write-gather pipe, et le
// materiel les repartit selon le descripteur courant. Il faut donc tenir ce descripteur et
// refaire la repartition. Sans cela, ces unites ne dessinent rien du tout, en silence.

void GXClearVtxDesc(void)
{
    for (int i = 0; i < GX_VA_MAX_ATTR; ++i)
        g_desc[i] = GX_NONE;
}

void GXSetVtxDesc(GXAttr attr, GXAttrType type)
{
    if (attr >= 0 && attr < GX_VA_MAX_ATTR)
        g_desc[attr] = type;
}

// Le nombre de bits fractionnaires des attributs entiers. Sans lui, une position s16 arrive
// 2^frac fois trop grande : le modele est la, mais a l'echelle d'un immeuble.
void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac)
{
    (void)vtxfmt;
    if (attr < 0 || attr >= GX_VA_MAX_ATTR)
        return;
    g_ctype[attr] = (unsigned char) type;
    g_cfrac[attr] = frac;
    // GXCompCnt est un enum par famille d'attribut : pour une position, 0 = XY et 1 = XYZ ;
    // pour une couleur, 0 = RGB et 1 = RGBA ; pour une coordonnee de texture, 0 = S et 1 = ST.
    if (attr == GX_VA_POS) {
        g_ncomp[attr] = (u8) (cnt == GX_POS_XY ? 2 : 3);
        g_frac_pos = frac;
    } else if (attr == GX_VA_NRM) {
        g_ncomp[attr] = 3;
    } else if (attr == GX_VA_CLR0 || attr == GX_VA_CLR1) {
        g_ncomp[attr] = (u8) (cnt == GX_CLR_RGB ? 3 : 4);
    } else if (attr >= GX_VA_TEX0 && attr <= GX_VA_TEX7) {
        g_ncomp[attr] = (u8) (cnt == GX_TEX_S ? 1 : 2);
        g_frac_tex = frac;
    } else {
        g_ncomp[attr] = 1;
    }
}

void GXBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts)
{
    (void)vtxfmt;
    port_gx_end();   // une primitive laissee ouverte par un appelant qui ne ferme pas
    g_prim = type;
    g_prim_first = g_nverts;
    g_prim_want = nverts;
    g_last_attr = -1;
    for (int i = 0; i < STREAM_N; ++i)
        if (g_desc[STREAM_ORDER[i]] != GX_NONE)
            g_last_attr = (int) STREAM_ORDER[i];
    g_cur_open = false;
    std::memset(&g_cur, 0, sizeof(g_cur));
    g_cur.r = g_cur.g = g_cur.b = g_cur.a = 255;
    g_cur.r1 = g_cur.g1 = g_cur.b1 = g_cur.a1 = 255;
    g_tex_seq = g_clr_seq = 0;
    stream_next(0);   // arme le flux direct pour les unites qui ecrivent dans le pipe
}

// La position ouvre un sommet : une nouvelle position valide le precedent.
void GXPosition3f32(f32 x, f32 y, f32 z)
{
    commit_vertex();
    g_cur.x = x; g_cur.y = y; g_cur.z = z;
    std::memset(g_cur.uv, 0, sizeof(g_cur.uv));
    g_cur.r = g_cur.g = g_cur.b = g_cur.a = 255;
    g_cur.r1 = g_cur.g1 = g_cur.b1 = g_cur.a1 = 255;
    g_tex_seq = g_clr_seq = 0;
    g_cur_open = true;
    attr_done(GX_VA_POS);
}

// Les appels GXColor/GXTexCoord n'annoncent pas quel attribut ils remplissent : c'est le rang dans
// le descripteur (le premier attribut couleur present, puis le suivant...) qui le dit.
static GXAttr next_attr(GXAttr first, int count, int &seq)
{
    int k = seq++;
    for (int i = 0; i < count; ++i)
        if (g_desc[first + i] != GX_NONE && k-- == 0)
            return (GXAttr)(first + i);
    return first;
}

void GXColor4u8(u8 r, u8 g, u8 b, u8 a)
{
    const GXAttr at = next_attr(GX_VA_CLR0, 2, g_clr_seq);
    if (at == GX_VA_CLR1) { g_cur.r1 = r; g_cur.g1 = g; g_cur.b1 = b; g_cur.a1 = a; }
    else                  { g_cur.r = r; g_cur.g = g; g_cur.b = b; g_cur.a = a; }
    attr_done(at);
}
void GXTexCoord2f32(f32 u, f32 v)
{
    const GXAttr at = next_attr(GX_VA_TEX0, 8, g_tex_seq);
    const int k = at - GX_VA_TEX0;
    if (k < 4) { g_cur.uv[k][0] = u; g_cur.uv[k][1] = v; }
    attr_done(at);
}
void GXNormal3f32(f32 x, f32 y, f32 z) { (void)x; (void)y; (void)z; attr_done(GX_VA_NRM); }

// Les memes attributs en virgule fixe. RE4 s'en sert partout ou il peut : c'est par la que
// passe la geometrie des salles et des personnages, pas par les variantes flottantes.
void GXPosition3s16(s16 x, s16 y, s16 z)
{
    GXPosition3f32(fixed_to_f(x, g_frac_pos), fixed_to_f(y, g_frac_pos), fixed_to_f(z, g_frac_pos));
}

void GXPosition2u16(u16 x, u16 y)
{
    GXPosition3f32(fixed_to_f((int) x, g_frac_pos), fixed_to_f((int) y, g_frac_pos), 0.0f);
}

void GXTexCoord2s16(s16 s, s16 t)
{
    const GXAttr at = next_attr(GX_VA_TEX0, 8, g_tex_seq);
    const int k = at - GX_VA_TEX0;
    if (k < 4) { g_cur.uv[k][0] = fixed_to_f(s, g_frac_tex); g_cur.uv[k][1] = fixed_to_f(t, g_frac_tex); }
    attr_done(at);
}

// Les normales s8 sont toujours en 1.7 : le bit de signe et sept bits de fraction, donc 127
// vaut presque 1. Elles ne servent pas encore (l'eclairage viendra avec TEV), mais il faut les
// consommer : une normale laissee dans le flux decalerait l'attribut suivant.
void GXNormal3s8(s8 x, s8 y, s8 z) { (void)x; (void)y; (void)z; attr_done(GX_VA_NRM); }

// L'indice de matrice par sommet, pour la palette de skinning. Il sera lu quand le portage
// gerera les matrices multiples ; pour l'instant tout passe par la matrice courante.
void GXMatrixIndex1u8(u8 idx) { (void)idx; attr_done(GX_VA_PNMTXIDX); }

// ---------------------------------------------------------------- tableaux de sommets et listes d'affichage
//
// Les modeles de RE4 ne passent pas leurs sommets un par un : ils declarent des tableaux
// (GXSetArray) et appellent une liste d'affichage dont les sommets ne sont que des indices. La
// liste est telle qu'elle est sur le disque - gros-boutiste ; les tableaux, eux, ont ete retournes
// par port/endian.cpp (ou produits par le skinning du jeu) et se lisent dans le sens de la machine.

inline unsigned rd16be(const unsigned char *p) { return ((unsigned) p[0] << 8) | p[1]; }
inline unsigned rd32be(const unsigned char *p) { return ((unsigned) p[0] << 24) | ((unsigned) p[1] << 16) | ((unsigned) p[2] << 8) | p[3]; }

// Lit une composante numerique ; `be` : donnees de la liste (gros-boutiste) ou du tableau (natif).
// Rend le nombre d'octets consommes.
unsigned dl_read_comp(const unsigned char *p, int type, int frac, bool be, float *out)
{
    switch (type) {
    case 0: *out = (float) p[0] / (float) (1 << frac); return 1;
    case 1: *out = (float) (signed char) p[0] / (float) (1 << frac); return 1;
    case 2: { unsigned v = be ? rd16be(p) : (unsigned) (p[0] | (p[1] << 8)); *out = (float) v / (float) (1 << frac); return 2; }
    case 3: { int v = (short) (be ? rd16be(p) : (unsigned) (p[0] | (p[1] << 8))); *out = (float) v / (float) (1 << frac); return 2; }
    default: {
        unsigned v = be ? rd32be(p) : (unsigned) (p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned) p[3] << 24));
        float f; std::memcpy(&f, &v, 4); *out = f; return 4;
    }
    }
}

unsigned dl_attr_bytes(int attr, int type, int ncomp)
{
    if (attr == GX_VA_CLR0 || attr == GX_VA_CLR1) {
        switch (type) { case 0: return 2; case 1: return 3; case 2: return 4; case 3: return 2; case 4: return 3; default: return 4; }
    }
    const unsigned sz = (type == 0 || type == 1) ? 1 : (type == 2 || type == 3) ? 2 : 4;
    return sz * (unsigned) ncomp;
}

// Une liste d'affichage : des primitives (0x80..0xB8 | format), du remplissage (0), et quelques
// commandes de registre qu'on saute.
void gx_dl_run(const unsigned char *p, unsigned size)
{
    const unsigned char *end = p + size;
    while (p < end) {
        const unsigned op = *p++;
        if (op == 0 || op == 0x48 || op == 0x44)          // NOP, invalidation de cache
            continue;
        if ((op & 0x80) == 0)
            return;                                       // commande de registre : pas dans les modeles
        const GXPrimitive prim = (GXPrimitive) (op & 0xF8);
        if (p + 2 > end)
            return;
        const unsigned n = rd16be(p);
        p += 2;
        GXBegin(prim, (GXVtxFmt) (op & 7), (u16) n);
        for (unsigned v = 0; v < n; ++v) {
            float x = 0, y = 0, z = 0, a, b, c, d;
            const unsigned char *pos_src = nullptr;
            // indice de matrice par sommet puis de matrices de texture : un octet chacun, directs
            for (int s = 0; s < STREAM_N; ++s) {
                const GXAttr at = STREAM_ORDER[s];
                if (g_desc[at] == GX_NONE)
                    continue;
                const bool direct = g_desc[at] == GX_DIRECT;
                if (at <= GX_VA_TEX7MTXIDX) {
                    if (direct) ++p;
                    continue;
                }
                const unsigned char *src;
                bool be;
                if (direct) {
                    src = p; be = true;
                } else {
                    unsigned idx;
                    if (g_desc[at] == GX_INDEX8) { idx = *p; ++p; }
                    else { idx = rd16be(p); p += 2; }
                    src = g_arr[at].base ? g_arr[at].base + (size_t) idx * g_arr[at].stride : nullptr;
                    be = false;
                }
                const int type = g_ctype[at], frac = g_cfrac[at];
                const int nc = comps_of(at);
                const unsigned bytes = dl_attr_bytes(at, type, nc);
                if (direct)
                    p += bytes;
                if (!src)
                    src = (const unsigned char *) "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
                if (at == GX_VA_POS) {
                    dl_read_comp(src, type, frac, be, &x);
                    unsigned o = (type == 0 || type == 1) ? 1 : (type == 2 || type == 3) ? 2 : 4;
                    dl_read_comp(src + o, type, frac, be, &y);
                    if (nc > 2) dl_read_comp(src + 2 * o, type, frac, be, &z);
                    pos_src = src;
                    GXPosition3f32(x, y, z);
                } else if (at == GX_VA_NRM) {
                    attr_done(GX_VA_NRM);
                } else if (at == GX_VA_CLR0 || at == GX_VA_CLR1) {
                    a = src[0]; b = src[1]; c = src[2]; d = (type == 5) ? src[3] : 255;
                    GXColor4u8((u8) a, (u8) b, (u8) c, (u8) d);
                } else {
                    float u = 0, w = 0;
                    const unsigned o = (type == 0 || type == 1) ? 1 : (type == 2 || type == 3) ? 2 : 4;
                    dl_read_comp(src, type, frac, be, &u);
                    if (nc > 1) dl_read_comp(src + o, type, frac, be, &w);
                    GXTexCoord2f32(u, w);
                }
            }
            (void) pos_src;
        }
        port_gx_end();
    }
}

void GXSetArray(GXAttr attr, void *base_ptr, u8 stride)
{
    if (attr >= 0 && attr < GX_VA_MAX_ATTR) {
        g_arr[attr].base = (const unsigned char *) base_ptr;
        g_arr[attr].stride = stride;
    }
}

void GXCallDisplayList(void *list, u32 nbytes) { gx_dl_run((const unsigned char *) list, nbytes); }

// Le jeu demande un rappel quand le GPU a fini une etape : ici le dessin est enregistre et rejoue
// plus tard, donc l'etape est consideree finie tout de suite.
static GXDrawSyncCallback g_sync_cb = nullptr;
GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) { GXDrawSyncCallback o = g_sync_cb; g_sync_cb = cb; return o; }
void GXSetDrawSync(u16 token) { if (g_sync_cb) g_sync_cb(token); }
void GXDrawDone(void) {}
void GXPixModeSync(void) {}
void GXInvalidateVtxCache(void) {}


// ------------------------------------------------- flux direct : le pipe
//
// Toutes les ecritures du jeu dans le write-gather pipe arrivent ici. On accumule les
// composantes de l'attribut courant, et quand il est complet on le pose dans le sommet en
// cours ; quand le dernier attribut du descripteur est pose, le sommet est valide.
namespace {

void stream_flush_attr()
{
    const GXAttr a = STREAM_ORDER[g_stream_attr];
    switch (a) {
    case GX_VA_POS:
        // La position ouvre le sommet, comme pour la voie par fonctions.
        commit_vertex();
        g_cur.x = g_stream_val[0];
        g_cur.y = g_stream_val[1];
        g_cur.z = (comps_of(a) >= 3) ? g_stream_val[2] : 0.0f;
        g_cur_open = true;
        break;
    case GX_VA_CLR0:
        g_cur.r = (unsigned char) g_stream_val[0];
        g_cur.g = (unsigned char) g_stream_val[1];
        g_cur.b = (unsigned char) g_stream_val[2];
        g_cur.a = (comps_of(a) >= 4) ? (unsigned char) g_stream_val[3] : 255;
        break;
    case GX_VA_CLR1:
        g_cur.r1 = (unsigned char) g_stream_val[0];
        g_cur.g1 = (unsigned char) g_stream_val[1];
        g_cur.b1 = (unsigned char) g_stream_val[2];
        g_cur.a1 = (comps_of(a) >= 4) ? (unsigned char) g_stream_val[3] : 255;
        break;
    case GX_VA_TEX0: case GX_VA_TEX1: case GX_VA_TEX2: case GX_VA_TEX3:
        g_cur.uv[a - GX_VA_TEX0][0] = g_stream_val[0];
        g_cur.uv[a - GX_VA_TEX0][1] = (comps_of(a) >= 2) ? g_stream_val[1] : 0.0f;
        break;
    default:
        break;   // normales, indices de matrice, autres jeux de coordonnees : plus tard
    }
    stream_next(g_stream_attr + 1);
    if (g_stream_attr < 0)
        stream_next(0);   // sommet complet : le suivant recommence au debut du descripteur
}

void stream_push(float v, bool fixed_point)
{
    if (g_stream_attr < 0)
        return;
    const GXAttr a = STREAM_ORDER[g_stream_attr];
    float x = v;
    if (fixed_point) {
        if (a == GX_VA_POS)
            x = v / (float) (1 << g_frac_pos);
        else if (a >= GX_VA_TEX0 && a <= GX_VA_TEX7)
            x = v / (float) (1 << g_frac_tex);
    }
    if (g_stream_comp < 4)
        g_stream_val[g_stream_comp] = x;
    if (++g_stream_comp >= comps_of(a))
        stream_flush_attr();
}

} // namespace

extern "C" {
void port_wg_push_u8(u8 v) { stream_push((float) v, false); }
void port_wg_push_s8(s8 v) { stream_push((float) v, false); }
void port_wg_push_u16(u16 v) { stream_push((float) v, true); }
void port_wg_push_s16(s16 v) { stream_push((float) v, true); }
void port_wg_push_u32(u32 v) { stream_push((float) v, true); }
void port_wg_push_s32(s32 v) { stream_push((float) v, true); }
void port_wg_push_f32(f32 v) { stream_push(v, false); }
}

PortWGPipe port_wg_pipe;

// Voir la note au-dessus de commit_vertex : c est ce qui remplace le GXEnd que le jeu
// n emet pas.
namespace {
void attr_done(GXAttr a)
{
    if (g_last_attr < 0 || (int) a != g_last_attr)
        return;
    commit_vertex();
    if (g_prim_want && g_nverts - g_prim_first >= g_prim_want)
        port_gx_end();
}
} // namespace

static void port_gx_flush_prim(void);

void port_gx_end(void)
{
    if (!g_prim_want && g_nverts == g_prim_first && !g_cur_open)
        return;
    port_gx_flush_prim();
    // Une primitive ne se dessine qu une fois : la suivante repart d ici.
    g_prim_first = g_nverts;
    g_prim_want = 0;
    g_last_attr = -1;
}

// ---------------------------------------------------------------- TEV, canaux, texgen
//
// Le materiel GX enchaine jusqu'a 16 etages de melange (TEV) ; chaque etage calcule
//   sortie = (d +/- ((1-c)*a + c*b) + biais) * echelle
// sur la couleur puis sur l'alpha, a partir d'entrees fixes (couleur rasterisee, texture, registres,
// constantes). Le portage genere un shader par configuration distincte et le garde en cache ;
// les couleurs (registres, constantes, materiaux) sont des uniformes, donc les changer ne
// recompile rien. Reference de comportement : Dolphin.
namespace {

struct TevStageCfg {
    unsigned char ci[4], ai[4];
    unsigned char cop, cbias, cscale, cclamp, cout_;
    unsigned char aop, abias, ascale, aclamp, aout;
    unsigned char coord, map, chan, kcs, kas;
    unsigned char rswap, tswap;    // table d'echange de canaux (0..3) pour la couleur rasterisee / la texture
};
struct TexGenCfg { unsigned char func, src, mtx, pad; };
struct ChanCfg { unsigned char en, amb, mat, diff, attn, pad[3]; unsigned lmask; };

// La partie "statique" : tout ce qui change le code du shader. Hachee telle quelle.
struct TevStatic {
    unsigned char nstage, nchan, ntexgen, pad0;
    TevStageCfg st[16];
    TexGenCfg tg[8];
    ChanCfg ch[4];                       // couleur0, alpha0, couleur1, alpha1
    unsigned char acomp0, acomp1, aop, pad1;
    unsigned char swp[4][4];             // tables d'echange : swp[t][c] = canal source (0=R 1=G 2=B 3=A)
};
// La partie "dynamique" : valeurs seulement, envoyees en uniformes.
struct TevDyn {
    float reg[4][4];     // TEVPREV, REG0, REG1, REG2
    float k[4][4];       // KCOLOR0..3
    float mat[2][4];
    float amb[2][4];
    float aref[2];
    unsigned char cmask, amask, pad[2];
    float tm[10][16];    // GX_TEXMTX0..9, en 4x4 colonne par colonne
};
struct TevCfg { TevStatic s; TevDyn d; };

TevCfg g_tevcur;                 // etat courant, modifie par GXSetTev*
const int kMaxCfg = 4096;
TevCfg g_cfgpool[kMaxCfg];
int g_ncfg = 0;
bool g_cfg_dirty = true;

inline void tev_dirty() { g_cfg_dirty = true; }

float c8(unsigned char v) { return (float)v / 255.0f; }

unsigned hash_bytes(const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; ++i)
        h = (h ^ b[i]) * 16777619u;
    return h;
}

struct Prog {
    unsigned key;
    GLuint prog;
    GLint u_mvp, u_reg, u_k, u_mat, u_aref, u_tm;
};
Prog g_progs[256];
int g_nprogs = 0;

struct Buf {
    char *p; size_t n, cap;
    void add(const char *fmt, ...) __attribute__((format(printf, 2, 3)))
    {
        va_list ap; va_start(ap, fmt);
        n += (size_t)std::vsnprintf(p + n, cap - n, fmt, ap);
        va_end(ap);
        if (n >= cap) n = cap - 1;
    }
};

const char *cc_in(int sel)   // GXTevColorArg
{
    static const char *t[16] = {
        "r0.rgb", "vec3(r0.a)", "r1.rgb", "vec3(r1.a)", "r2.rgb", "vec3(r2.a)", "r3.rgb", "vec3(r3.a)",
        "texc.rgb", "vec3(texc.a)", "ras.rgb", "vec3(ras.a)", "vec3(1.0)", "vec3(0.5)", "KC", "vec3(0.0)" };
    return t[sel & 15];
}
const char *ca_in(int sel)   // GXTevAlphaArg
{
    static const char *t[8] = { "r0.a", "r1.a", "r2.a", "r3.a", "texc.a", "ras.a", "KA", "0.0" };
    return t[sel & 7];
}

// Selection de constante : fractions fixes, ou KCOLORi (composantes pour l'alpha).
void kconst(char *out, size_t n, int sel, bool alpha)
{
    static const char *frac[8] = { "1.0", "0.875", "0.75", "0.625", "0.5", "0.375", "0.25", "0.125" };
    static const char comp[4] = { 'r', 'g', 'b', 'a' };
    if (sel < 8) {
        std::snprintf(out, n, alpha ? "%s" : "vec3(%s)", frac[sel]);
    } else if (!alpha && sel >= 12 && sel < 16) {
        std::snprintf(out, n, "uK[%d].rgb", sel - 12);
    } else if (sel >= 16 && sel < 32) {
        const int k = (sel - 16) & 3, c = ((sel - 16) >> 2) & 3;
        std::snprintf(out, n, alpha ? "uK[%d].%c" : "vec3(uK[%d].%c)", k, comp[c]);
    } else {
        std::snprintf(out, n, alpha ? "1.0" : "vec3(1.0)");
    }
}

const char *cmp_expr(int f)   // GXCompare, sur a (0..255) et ref
{
    static const char *t[8] = { "false", "(a8 < REF)", "(a8 == REF)", "(a8 <= REF)",
                                "(a8 > REF)", "(a8 != REF)", "(a8 >= REF)", "true" };
    return t[f & 7];
}

GLuint build_program(const TevStatic &s)
{
    static char vsb[8192], fsb[32768];
    Buf v = { vsb, 0, sizeof(vsb) }, f = { fsb, 0, sizeof(fsb) };

    v.add("#version 300 es\n"
          "layout(location=0) in vec3 aPos;\n"
          "layout(location=1) in vec4 aC0;\n"
          "layout(location=2) in vec4 aC1;\n"
          "layout(location=3) in vec2 aT0;\n"
          "layout(location=4) in vec2 aT1;\n"
          "layout(location=5) in vec2 aT2;\n"
          "layout(location=6) in vec2 aT3;\n"
          "uniform mat4 uMVP;\n"
          "uniform mat4 uTM[10];\n"
          "out vec4 vC0; out vec4 vC1;\n"
          "out vec3 vT[8];\n"
          "void main(){\n"
          "  vC0 = aC0; vC1 = aC1;\n"
          "  vec2 t[4]; t[0]=aT0; t[1]=aT1; t[2]=aT2; t[3]=aT3;\n"
          "  vec4 p4 = vec4(aPos, 1.0);\n");
    for (int i = 0; i < s.ntexgen && i < 8; ++i) {
        const TexGenCfg &g = s.tg[i];
        const char *src;
        char tmp[64];
        if (g.src >= 4 && g.src <= 7)        { std::snprintf(tmp, sizeof(tmp), "vec4(t[%d], 1.0, 1.0)", g.src - 4); src = tmp; }
        else if (g.src == 0)                 src = "p4";
        else                                 src = "vec4(0.0, 0.0, 1.0, 1.0)";   // normales : pas encore
        if (g.mtx >= 30 && g.mtx <= 57 && ((g.mtx - 30) % 3) == 0)
            v.add("  { vec4 q = uTM[%d] * %s; vT[%d] = vec3(q.xy, %s); }\n", (g.mtx - 30) / 3, src, i,
                  g.func == 1 ? "1.0" : "q.z");
        else
            v.add("  { vec4 q = %s; vT[%d] = vec3(q.xy, 1.0); }\n", src, i);
    }
    v.add("  gl_Position = uMVP * p4;\n}\n");

    f.add("#version 300 es\nprecision highp float;\n"
          "in vec4 vC0; in vec4 vC1; in vec3 vT[8];\n"
          "uniform vec4 uReg[4]; uniform vec4 uK[4]; uniform vec4 uMat[2]; uniform float uARef[2];\n"
          "uniform sampler2D uTex0; uniform sampler2D uTex1; uniform sampler2D uTex2; uniform sampler2D uTex3;\n"
          "uniform sampler2D uTex4; uniform sampler2D uTex5; uniform sampler2D uTex6; uniform sampler2D uTex7;\n"
          "out vec4 oCol;\n"
          "void main(){\n"
          "  vec4 r0 = uReg[0], r1 = uReg[1], r2 = uReg[2], r3 = uReg[3];\n"
          "  vec4 ras0 = vec4(1.0), ras1 = vec4(1.0);\n");
    // Canaux sans eclairage : couleur du sommet ou couleur de materiau (l'eclairage viendra apres).
    f.add("  ras0 = vec4(%s, %s);\n", s.ch[0].mat ? "vC0.rgb" : "uMat[0].rgb", s.ch[1].mat ? "vC0.a" : "uMat[0].a");
    f.add("  ras1 = vec4(%s, %s);\n", s.ch[2].mat ? "vC1.rgb" : "uMat[1].rgb", s.ch[3].mat ? "vC1.a" : "uMat[1].a");
    // Les chaines : un canal non declare (nchan==0) rend du blanc, comme le materiel.
    for (int i = 0; i < s.nstage && i < 16; ++i) {
        const TevStageCfg &t = s.st[i];
        f.add("  {\n    vec4 ras = %s;\n", t.chan == 4 ? "ras0" : t.chan == 5 ? "ras1" : "vec4(0.0)");
        {
            static const char sw[4] = { 'r', 'g', 'b', 'a' };
            const unsigned char *rt = s.swp[t.rswap & 3];
            if (!(rt[0] == 0 && rt[1] == 1 && rt[2] == 2 && rt[3] == 3))
                f.add("    ras = ras.%c%c%c%c;\n", sw[rt[0] & 3], sw[rt[1] & 3], sw[rt[2] & 3], sw[rt[3] & 3]);
        }
        if (t.map < 8 && t.coord < 8)
            f.add("    vec4 texc = texture(uTex%d, vT[%d].xy / (vT[%d].z != 0.0 ? vT[%d].z : 1.0));\n",
                  t.map, t.coord, t.coord, t.coord);
        else
            f.add("    vec4 texc = vec4(1.0);\n");
        {
            static const char sw[4] = { 'r', 'g', 'b', 'a' };
            const unsigned char *tt = s.swp[t.tswap & 3];
            if (!(tt[0] == 0 && tt[1] == 1 && tt[2] == 2 && tt[3] == 3))
                f.add("    texc = texc.%c%c%c%c;\n", sw[tt[0] & 3], sw[tt[1] & 3], sw[tt[2] & 3], sw[tt[3] & 3]);
        }
        char kc[48], ka[48];
        kconst(kc, sizeof(kc), t.kcs, false);
        kconst(ka, sizeof(ka), t.kas, true);
        // Couleur
        const char *cb = t.cbias == 1 ? " + 0.5" : t.cbias == 2 ? " - 0.5" : "";
        const char *cs = t.cscale == 1 ? "2.0" : t.cscale == 2 ? "4.0" : t.cscale == 3 ? "0.5" : "1.0";
        auto csel = [&](int k, char *dst, size_t n) {
            const char *x = cc_in(t.ci[k]);
            if (std::strcmp(x, "KC") == 0) std::snprintf(dst, n, "%s", kc); else std::snprintf(dst, n, "%s", x);
        };
        char a[48], b[48], c[48], d[48];
        csel(0, a, sizeof(a)); csel(1, b, sizeof(b)); csel(2, c, sizeof(c)); csel(3, d, sizeof(d));
        f.add("    vec3 cres = (%s %s mix(%s, %s, %s)%s) * %s;\n", d, t.cop == 1 ? "-" : "+", a, b, c, cb, cs);
        if (t.cclamp) f.add("    cres = clamp(cres, 0.0, 1.0);\n");
        // Alpha
        auto asel = [&](int k, char *dst, size_t n) {
            const char *x = ca_in(t.ai[k]);
            if (std::strcmp(x, "KA") == 0) std::snprintf(dst, n, "%s", ka); else std::snprintf(dst, n, "%s", x);
        };
        char aa[48], ab[48], ac[48], ad[48];
        asel(0, aa, sizeof(aa)); asel(1, ab, sizeof(ab)); asel(2, ac, sizeof(ac)); asel(3, ad, sizeof(ad));
        const char *ab_ = t.abias == 1 ? " + 0.5" : t.abias == 2 ? " - 0.5" : "";
        const char *as_ = t.ascale == 1 ? "2.0" : t.ascale == 2 ? "4.0" : t.ascale == 3 ? "0.5" : "1.0";
        f.add("    float ares = (%s %s mix(%s, %s, %s)%s) * %s;\n", ad, t.aop == 1 ? "-" : "+", aa, ab, ac, ab_, as_);
        if (t.aclamp) f.add("    ares = clamp(ares, 0.0, 1.0);\n");
        f.add("    r%d.rgb = cres; r%d.a = ares;\n  }\n", t.cout_ & 3, t.aout & 3);
    }
    f.add("  vec4 col = r0;\n"
          "  float a8 = floor(col.a * 255.0 + 0.5);\n"
          "  float ref0 = floor(uARef[0] + 0.5), ref1 = floor(uARef[1] + 0.5);\n");
    {
        char e0[64], e1[64];
        std::snprintf(e0, sizeof(e0), "%s", cmp_expr(s.acomp0));
        std::snprintf(e1, sizeof(e1), "%s", cmp_expr(s.acomp1));
        auto subst = [](char *e, const char *ref) {
            char *p = std::strstr(e, "REF");
            if (!p) return;
            char tail[64]; std::snprintf(tail, sizeof(tail), "%s", p + 3);
            *p = 0; std::strcat(e, ref); std::strcat(e, tail);
        };
        subst(e0, "ref0"); subst(e1, "ref1");
        const char *op = s.aop == 0 ? "&&" : s.aop == 1 ? "||" : s.aop == 2 ? "!=" : "==";
        if (s.acomp0 == 7 && s.acomp1 == 7 && s.aop == 0) { /* toujours : rien a faire */ }
        else f.add("  if (!((%s) %s (%s))) discard;\n", e0, op, e1);
    }
    f.add("  oCol = col;\n}\n");
    if (std::getenv("RE4_SHADERLOG")) {
        static int shown = 0;
        if (shown++ < 14) std::printf("[SHADER %d]\n%s\n", shown, fsb);
    }

    GLuint vs = make_shader(GL_VERTEX_SHADER, vsb);
    GLuint fs = make_shader(GL_FRAGMENT_SHADER, fsb);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = { 0 };
        glGetProgramInfoLog(p, sizeof(log) - 1, nullptr, log);
        std::printf("[PORT] GX : programme refuse : %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

Prog *get_prog(const TevStatic &s)
{
    const unsigned key = hash_bytes(&s, sizeof(s));
    for (int i = 0; i < g_nprogs; ++i)
        if (g_progs[i].key == key)
            return &g_progs[i];
    if (g_nprogs >= 256)
        return &g_progs[0];
    Prog &pr = g_progs[g_nprogs++];
    pr.key = key;
    pr.prog = build_program(s);
    pr.u_mvp = glGetUniformLocation(pr.prog, "uMVP");
    pr.u_reg = glGetUniformLocation(pr.prog, "uReg");
    pr.u_k = glGetUniformLocation(pr.prog, "uK");
    pr.u_mat = glGetUniformLocation(pr.prog, "uMat");
    pr.u_aref = glGetUniformLocation(pr.prog, "uARef");
    pr.u_tm = glGetUniformLocation(pr.prog, "uTM");
    glUseProgram(pr.prog);
    for (int i = 0; i < 8; ++i) {
        char nm[16];
        std::snprintf(nm, sizeof(nm), "uTex%d", i);
        glUniform1i(glGetUniformLocation(pr.prog, nm), i);
    }
    std::printf("[PORT] GX : shader TEV %d (%d etages, %d texgen)\n", g_nprogs, s.nstage, s.ntexgen);
    return &pr;
}

// Etat initial du materiel : un etage "passe la couleur du sommet", une texture, pas d'alpha test.
void tev_reset()
{
    std::memset(&g_tevcur, 0, sizeof(g_tevcur));
    g_tevcur.s.nstage = 1;
    g_tevcur.s.nchan = 1;
    g_tevcur.s.ntexgen = 1;
    for (int i = 0; i < 16; ++i) {
        TevStageCfg &t = g_tevcur.s.st[i];
        t.ci[0] = 15; t.ci[1] = 15; t.ci[2] = 15; t.ci[3] = 10;   // RASC
        t.ai[0] = 7; t.ai[1] = 7; t.ai[2] = 7; t.ai[3] = 5;       // RASA
        t.cscale = t.ascale = 0; t.cclamp = t.aclamp = 1;
        t.cout_ = t.aout = 0;
        t.coord = t.map = t.chan = 0xFF;
        t.kcs = 0; t.kas = 0;
        t.rswap = t.tswap = 0;
    }
    // Les quatre tables par defaut du SDK : identite, rouge, vert, bleu (avec l'alpha d'origine).
    for (int t = 0; t < 4; ++t) {
        g_tevcur.s.swp[t][0] = (unsigned char)(t == 0 ? 0 : t - 1);
        g_tevcur.s.swp[t][1] = (unsigned char)(t == 0 ? 1 : t - 1);
        g_tevcur.s.swp[t][2] = (unsigned char)(t == 0 ? 2 : t - 1);
        g_tevcur.s.swp[t][3] = 3;
    }
    g_tevcur.s.tg[0].func = 1; g_tevcur.s.tg[0].src = 4; g_tevcur.s.tg[0].mtx = 60;
    for (int i = 0; i < 4; ++i) { g_tevcur.s.ch[i].mat = 1; g_tevcur.s.ch[i].amb = 0; }
    g_tevcur.s.acomp0 = g_tevcur.s.acomp1 = 7;
    g_tevcur.d.cmask = g_tevcur.d.amask = 1;
    for (int i = 0; i < 10; ++i) {
        std::memset(g_tevcur.d.tm[i], 0, sizeof(g_tevcur.d.tm[i]));
        g_tevcur.d.tm[i][0] = g_tevcur.d.tm[i][5] = g_tevcur.d.tm[i][10] = g_tevcur.d.tm[i][15] = 1.0f;
    }
    tev_dirty();
}

// Rend l'indice de la configuration courante dans le pool, en la creant si elle a change.
int tev_snapshot()
{
    if (!g_cfg_dirty && g_ncfg > 0)
        return g_ncfg - 1;
    if (g_ncfg >= kMaxCfg)
        return g_ncfg - 1;
    g_cfgpool[g_ncfg] = g_tevcur;
    g_cfg_dirty = false;
    return g_ncfg++;
}

} // namespace

void GXSetNumChans(u8 n) { g_tevcur.s.nchan = n; tev_dirty(); }
void GXSetNumTexGens(u8 n) { g_tevcur.s.ntexgen = n; tev_dirty(); }
void GXSetNumTevStages(u8 n) { g_tevcur.s.nstage = n; tev_dirty(); }
void GXSetNumIndStages(u8 n) { (void)n; }
void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color)
{
    if ((unsigned)stage >= 16) return;
    TevStageCfg &t = g_tevcur.s.st[stage];
    t.coord = (unsigned char)coord; t.map = (unsigned char)map; t.chan = (unsigned char)color;
    tev_dirty();
}

void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d)
{
    if ((unsigned)stage >= 16) return;
    TevStageCfg &t = g_tevcur.s.st[stage];
    t.ci[0] = (unsigned char)a; t.ci[1] = (unsigned char)b; t.ci[2] = (unsigned char)c; t.ci[3] = (unsigned char)d;
    tev_dirty();
}
void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d)
{
    if ((unsigned)stage >= 16) return;
    TevStageCfg &t = g_tevcur.s.st[stage];
    t.ai[0] = (unsigned char)a; t.ai[1] = (unsigned char)b; t.ai[2] = (unsigned char)c; t.ai[3] = (unsigned char)d;
    tev_dirty();
}
void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg)
{
    if ((unsigned)stage >= 16) return;
    TevStageCfg &t = g_tevcur.s.st[stage];
    t.cop = (unsigned char)op; t.cbias = (unsigned char)bias; t.cscale = (unsigned char)scale;
    t.cclamp = clamp ? 1 : 0; t.cout_ = (unsigned char)out_reg;
    tev_dirty();
}
void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg)
{
    if ((unsigned)stage >= 16) return;
    TevStageCfg &t = g_tevcur.s.st[stage];
    t.aop = (unsigned char)op; t.abias = (unsigned char)bias; t.ascale = (unsigned char)scale;
    t.aclamp = clamp ? 1 : 0; t.aout = (unsigned char)out_reg;
    tev_dirty();
}

// Les modes predefinis, tels que le SDK les definit : l'etage 0 lit la couleur rasterisee, les
// suivants lisent le resultat de l'etage precedent.
void GXSetTevOp(GXTevStageID id, GXTevMode mode)
{
    const int carg = (id == GX_TEVSTAGE0) ? GX_CC_RASC : GX_CC_CPREV;
    const int aarg = (id == GX_TEVSTAGE0) ? GX_CA_RASA : GX_CA_APREV;
    switch (mode) {
    case GX_MODULATE:
        GXSetTevColorIn(id, GX_CC_ZERO, GX_CC_TEXC, (GXTevColorArg)carg, GX_CC_ZERO);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_TEXA, (GXTevAlphaArg)aarg, GX_CA_ZERO);
        break;
    case GX_DECAL:
        GXSetTevColorIn(id, (GXTevColorArg)carg, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, (GXTevAlphaArg)aarg);
        break;
    case GX_BLEND:
        GXSetTevColorIn(id, (GXTevColorArg)carg, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_TEXA, (GXTevAlphaArg)aarg, GX_CA_ZERO);
        break;
    case GX_REPLACE:
        GXSetTevColorIn(id, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
        break;
    default:   // GX_PASSCLR
        GXSetTevColorIn(id, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, (GXTevColorArg)carg);
        GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, (GXTevAlphaArg)aarg);
        break;
    }
    GXSetTevColorOp(id, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(id, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

static void set_reg(float *dst, float r, float g, float b, float a) { dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = a; tev_dirty(); }
void GXSetTevColor(GXTevRegID id, GXColor c) { if ((unsigned)id < 4) set_reg(g_tevcur.d.reg[id], c8(c.r), c8(c.g), c8(c.b), c8(c.a)); }
void GXSetTevColorS10(GXTevRegID id, GXColorS10 c)
{ if ((unsigned)id < 4) set_reg(g_tevcur.d.reg[id], (float)c.r / 255.0f, (float)c.g / 255.0f, (float)c.b / 255.0f, (float)c.a / 255.0f); }
void GXSetTevKColor(GXTevKColorID id, GXColor c) { if ((unsigned)id < 4) set_reg(g_tevcur.d.k[id], c8(c.r), c8(c.g), c8(c.b), c8(c.a)); }
void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel) { if ((unsigned)stage < 16) { g_tevcur.s.st[stage].kcs = (unsigned char)sel; tev_dirty(); } }
void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel) { if ((unsigned)stage < 16) { g_tevcur.s.st[stage].kas = (unsigned char)sel; tev_dirty(); } }
void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel ras_sel, GXTevSwapSel tex_sel)
{
    if ((unsigned)stage < 16) {
        g_tevcur.s.st[stage].rswap = (unsigned char)(ras_sel & 3);
        g_tevcur.s.st[stage].tswap = (unsigned char)(tex_sel & 3);
        tev_dirty();
    }
}
void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan red, GXTevColorChan green, GXTevColorChan blue, GXTevColorChan alpha)
{
    if ((unsigned)table >= 4) return;
    unsigned char *t = g_tevcur.s.swp[table];
    t[0] = (unsigned char)red; t[1] = (unsigned char)green; t[2] = (unsigned char)blue; t[3] = (unsigned char)alpha;
    tev_dirty();
}
void GXSetTevDirect(GXTevStageID stage) { (void)stage; }

void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1)
{
    g_tevcur.s.acomp0 = (unsigned char)comp0; g_tevcur.s.acomp1 = (unsigned char)comp1; g_tevcur.s.aop = (unsigned char)op;
    g_tevcur.d.aref[0] = (float)ref0; g_tevcur.d.aref[1] = (float)ref1;
    tev_dirty();
}
void GXSetColorUpdate(GXBool e) { g_tevcur.d.cmask = e ? 1 : 0; tev_dirty(); }
void GXSetAlphaUpdate(GXBool e) { g_tevcur.d.amask = e ? 1 : 0; tev_dirty(); }

void GXSetTexCoordGen2(GXTexCoordID dst, GXTexGenType func, GXTexGenSrc src, u32 mtx, GXBool normalize, u32 pt)
{
    (void)normalize; (void)pt;
    if ((unsigned)dst >= 8) return;
    TexGenCfg &g = g_tevcur.s.tg[dst];
    g.func = (unsigned char)func; g.src = (unsigned char)src; g.mtx = (unsigned char)mtx;
    tev_dirty();
}

void GXLoadTexMtxImm(const f32 mtx[][4], u32 id, GXTexMtxType type)
{
    if (id < 30 || id > 57) return;
    float *m = g_tevcur.d.tm[(id - 30) / 3];
    const int rows = (type == GX_MTX2x4) ? 2 : 3;
    std::memset(m, 0, 16 * sizeof(float));
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < rows; ++r)
            m[c * 4 + r] = mtx[r][c];
    if (rows == 2) m[10] = 1.0f;   // z passe tel quel
    m[15] = 1.0f;
    tev_dirty();
}

void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc amb_src, GXColorSrc mat_src, u32 light_mask, GXDiffuseFn diff_fn, GXAttnFn attn_fn)
{
    int idx[2], n = 0;
    switch (chan) {
    case 0: idx[n++] = 0; break;                        // GX_COLOR0
    case 1: idx[n++] = 2; break;                        // GX_COLOR1
    case 2: idx[n++] = 1; break;                        // GX_ALPHA0
    case 3: idx[n++] = 3; break;                        // GX_ALPHA1
    case 4: idx[n++] = 0; idx[n++] = 1; break;          // GX_COLOR0A0
    case 5: idx[n++] = 2; idx[n++] = 3; break;          // GX_COLOR1A1
    default: return;
    }
    for (int i = 0; i < n; ++i) {
        ChanCfg &c = g_tevcur.s.ch[idx[i]];
        c.en = enable ? 1 : 0; c.amb = (unsigned char)amb_src; c.mat = (unsigned char)mat_src;
        c.lmask = light_mask; c.diff = (unsigned char)diff_fn; c.attn = (unsigned char)attn_fn;
    }
    tev_dirty();
}
void GXSetChanMatColor(GXChannelID chan, GXColor c)
{
    const int k = (chan == 1 || chan == 3 || chan == 5) ? 1 : 0;
    set_reg(g_tevcur.d.mat[k], c8(c.r), c8(c.g), c8(c.b), c8(c.a));
}
void GXSetChanAmbColor(GXChannelID chan, GXColor c)
{
    const int k = (chan == 1 || chan == 3 || chan == 5) ? 1 : 0;
    set_reg(g_tevcur.d.amb[k], c8(c.r), c8(c.g), c8(c.b), c8(c.a));
}

struct Cmd {
    int type;                 // 0 dessin, 1 viewport, 2 scissor
    unsigned int ioff, ni;
    GLuint tex[8];
    int cfg;                  // indice dans g_cfgpool
    float mvp[16];
    unsigned char zt, zf, zw, cull, blend, bsrc, bdst, btype;
    int v[4];
    float nz, fz;
};
const int kMaxCmd = 4096;
const unsigned kMaxPoolIdx = 1u << 20;
Cmd g_cmd[kMaxCmd];
int g_ncmd = 0;
unsigned short g_ipool[kMaxPoolIdx];
unsigned g_pni = 0;

// Rejoue les commandes enregistrees : un seul envoi de sommets et d'indices, puis les dessins avec
// un etat GL memorise (un appel redondant coute cher dans le pilote Adreno).
static GLenum gl_factor(int f)
{
    switch (f) {   // GXBlendFactor
    case 0: return GL_ZERO;
    case 1: return GL_ONE;
    case 2: return GL_SRC_COLOR;            // GX_BL_SRCCLR / DSTCLR selon la voie
    case 3: return GL_ONE_MINUS_SRC_COLOR;
    case 4: return GL_SRC_ALPHA;
    case 5: return GL_ONE_MINUS_SRC_ALPHA;
    case 6: return GL_DST_ALPHA;
    default: return GL_ONE_MINUS_DST_ALPHA;
    }
}

static void gx_replay()
{
    if (g_ncmd == 0) { g_pni = 0; g_ncfg = 0; g_cfg_dirty = true; return; }
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)g_nverts * sizeof(Vertex)), g_verts, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)((size_t)g_pni * sizeof(unsigned short)), g_ipool, GL_STREAM_DRAW);

    int s_zt = -1, s_zf = -1, s_zw = -1, s_cull = -1, s_blend = -1, s_bsrc = -1, s_bdst = -1, s_cfg = -1, s_cm = -1, s_am = -1;
    GLuint s_tex[8];
    for (int k = 0; k < 8; ++k) s_tex[k] = 0xFFFFFFFFu;
    Prog *pr = nullptr, *s_pr = nullptr;
    float s_mvp[16]; bool s_mvp_ok = false;
    for (int i = 0; i < g_ncmd; ++i) {
        const Cmd &c = g_cmd[i];
        if (c.type == 1) { glViewport(c.v[0], c.v[1], c.v[2], c.v[3]); glDepthRangef(c.nz, c.fz); continue; }
        if (c.type == 2) { glEnable(GL_SCISSOR_TEST); glScissor(c.v[0], c.v[1], c.v[2], c.v[3]); continue; }
        const TevCfg &cfg = g_cfgpool[c.cfg];
        if (c.cfg != s_cfg) {
            pr = get_prog(cfg.s);
            if (pr != s_pr) { glUseProgram(pr->prog); s_pr = pr; s_mvp_ok = false; }
            glUniform4fv(pr->u_reg, 4, &cfg.d.reg[0][0]);
            glUniform4fv(pr->u_k, 4, &cfg.d.k[0][0]);
            glUniform4fv(pr->u_mat, 2, &cfg.d.mat[0][0]);
            glUniform1fv(pr->u_aref, 2, cfg.d.aref);
            glUniformMatrix4fv(pr->u_tm, 10, GL_FALSE, &cfg.d.tm[0][0]);
            const int cm = cfg.d.cmask, am = cfg.d.amask;
            if (cm != s_cm || am != s_am) { glColorMask(cm, cm, cm, am); s_cm = cm; s_am = am; }
            s_cfg = c.cfg;
        }
        if (!s_mvp_ok || std::memcmp(s_mvp, c.mvp, sizeof(s_mvp)) != 0) {
            glUniformMatrix4fv(pr->u_mvp, 1, GL_FALSE, c.mvp);
            std::memcpy(s_mvp, c.mvp, sizeof(s_mvp)); s_mvp_ok = true;
        }
        for (int k = 0; k < 8; ++k)
            if (c.tex[k] != s_tex[k]) {
                glActiveTexture(GL_TEXTURE0 + k);
                glBindTexture(GL_TEXTURE_2D, c.tex[k]);
                s_tex[k] = c.tex[k];
            }
        if (s_zt != c.zt) { if (c.zt) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST); s_zt = c.zt; }
        if (c.zt && s_zf != c.zf) { glDepthFunc(gl_compare((GXCompare)c.zf)); s_zf = c.zf; }
        if (s_zw != c.zw) { glDepthMask(c.zw ? GL_TRUE : GL_FALSE); s_zw = c.zw; }
        if (s_cull != c.cull) {
            s_cull = c.cull;
            switch (c.cull) {
            case GX_CULL_NONE:  glDisable(GL_CULL_FACE); break;
            case GX_CULL_FRONT: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
            case GX_CULL_BACK:  glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
            default:            glEnable(GL_CULL_FACE); glCullFace(GL_FRONT_AND_BACK); break;
            }
        }
        if (s_blend != c.blend || s_bsrc != c.bsrc || s_bdst != c.bdst) {
            s_blend = c.blend; s_bsrc = c.bsrc; s_bdst = c.bdst;
            if (c.blend) {
                glEnable(GL_BLEND);
                if (c.btype == GX_BM_SUBTRACT) { glBlendEquation(GL_FUNC_REVERSE_SUBTRACT); glBlendFunc(GL_ONE, GL_ONE); }
                else { glBlendEquation(GL_FUNC_ADD); glBlendFunc(gl_factor(c.bsrc), gl_factor(c.bdst)); }
            } else glDisable(GL_BLEND);
        }
        glDrawElements(GL_TRIANGLES, (GLsizei)c.ni, GL_UNSIGNED_SHORT, (const void *)(size_t)(c.ioff * sizeof(unsigned short)));
    }
    glActiveTexture(GL_TEXTURE0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    g_ncmd = 0;
    g_pni = 0;
    g_ncfg = 0;
    g_cfg_dirty = true;
}

static void port_gx_flush_prim(void)
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
    ++g_draws;
    g_tris += ni / 3;

    // Enregistre le dessin ; il est rejoue en bloc par gx_replay() (GXCopyDisp). Avant, chaque dessin
    // renvoyait au pilote TOUS les sommets accumules depuis le debut de l'image : le cout croissait
    // avec le carre du nombre de dessins (mesure : 0,15 ms par dessin, 86 % du temps dans le pilote).
    if (g_ncmd >= kMaxCmd || g_pni + (unsigned)ni > kMaxPoolIdx || g_ncfg >= kMaxCfg - 2)
        gx_replay();
    Cmd &c = g_cmd[g_ncmd++];
    c.type = 0;
    c.ioff = g_pni;
    c.ni = (unsigned)ni;
    std::memcpy(g_ipool + g_pni, g_idx, (size_t)ni * sizeof(unsigned short));
    g_pni += (unsigned)ni;
    mtx_mul(g_proj, g_modelview, c.mvp);
    for (int k = 0; k < 8; ++k)
        c.tex[k] = port_gx_bound_texture(k);
    c.cfg = tev_snapshot();
    c.bsrc = g_blend_src; c.bdst = g_blend_dst; c.btype = g_blend_type;
    c.zt = g_ztest ? 1 : 0;
    c.zf = (unsigned char)g_zfunc;
    c.zw = g_zwrite ? 1 : 0;
    c.cull = (unsigned char)g_cull;
    c.blend = g_blend ? 1 : 0;
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
    { static int n = 0; if (getenv("RE4_GXLOG") && n++ < 12) std::printf("[GXLOG] viewport %.0f %.0f %.0f %.0f z %.2f %.2f\n", left, top, wd, ht, nearz, farz); }
    // L'origine de l'ecran est en haut a gauche sur GameCube, en bas a gauche en OpenGL.
    if (g_ncmd >= kMaxCmd) gx_replay();
    Cmd &c = g_cmd[g_ncmd++];
    c.type = 1;
    c.v[0] = (GLint)left; c.v[1] = (GLint)((float)g_fb_h - top - ht); c.v[2] = (GLsizei)wd; c.v[3] = (GLsizei)ht;
    c.nz = nearz; c.fz = farz;
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht)
{
    if (!g_gl_ready)
        return;
    { static int n = 0; if (getenv("RE4_GXLOG") && n++ < 12) std::printf("[GXLOG] scissor %u %u %u %u\n", left, top, wd, ht); }
    if (g_ncmd >= kMaxCmd) gx_replay();
    Cmd &c = g_cmd[g_ncmd++];
    c.type = 2;
    c.v[0] = (GLint)left; c.v[1] = (GLint)(g_fb_h - top - ht); c.v[2] = (GLsizei)wd; c.v[3] = (GLsizei)ht;
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
    (void)op;
    g_blend = (type == GX_BM_BLEND || type == GX_BM_SUBTRACT);
    g_blend_type = (unsigned char)type;
    g_blend_src = (unsigned char)src;
    g_blend_dst = (unsigned char)dst;
}

void GXSetCopyClear(GXColor clear_clr, u32 clear_z) { (void)clear_z; g_clear = clear_clr; }

// Sur le materiel, l'EFB est resolu vers le XFB. Ici on rend directement dans la surface, donc
// il n'y a rien a copier ; l'effacement de l'image suivante est ce qui compte.
// Le portage rend hors ecran : aucun ecran ne montre le resultat, et une execution sans
// plantage ne prouve rien. `RE4_DUMP_FRAME=<n>` enregistre donc la n-ieme image resolue en PPM
// binaire - un format qu on ecrit en dix lignes, sans dependance - et le jeu s arrete la.
// C est le meme principe que les harnais de verification : on relit ce que GLES a produit.
static void port_dump_frame(void)
{
    // RE4_DUMP_EVERY=N : une image toutes les N images, dans dump_<numero>.ppm, sans arreter le jeu
    // (40 images au plus). Sert a suivre un parcours scripte par RE4_PADFILE.
    static long every = -2, nseen = 0, ndump = 0;
    if (every == -2) {
        const char *e = std::getenv("RE4_DUMP_EVERY");
        every = e ? std::strtol(e, nullptr, 10) : -1;
    }
    if (every > 0) {
        if ((nseen++ % every) != 0 || ndump >= 40)
            return;
        const int w = (int) g_fb_w, h = (int) g_fb_h;
        unsigned char *px = (unsigned char *) std::malloc((size_t) w * h * 4);
        if (!px)
            return;
        port_gx_read_pixels(0, 0, w, h, px);
        char name[64];
        std::snprintf(name, sizeof(name), "dump_%05ld.ppm", nseen - 1);
        std::FILE *f = std::fopen(name, "wb");
        if (f) {
            std::fprintf(f, "P6\n%d %d\n255\n", w, h);
            for (int y = h - 1; y >= 0; --y)
                for (int x = 0; x < w; ++x)
                    std::fwrite(px + ((size_t) y * w + x) * 4, 1, 3, f);
            std::fclose(f);
            ++ndump;
        }
        std::free(px);
        return;
    }
    static long want = -2;
    static long seen = 0;
    if (want == -2) {
        const char *e = std::getenv("RE4_DUMP_FRAME");
        want = e ? std::strtol(e, nullptr, 10) : -1;
    }
    if (want < 0)
        return;
    if (seen++ != want)
        return;

    const int w = (int) g_fb_w, h = (int) g_fb_h;
    unsigned char *px = (unsigned char *) std::malloc((size_t) w * h * 4);
    if (!px)
        return;
    port_gx_read_pixels(0, 0, w, h, px);

    const char *path = std::getenv("RE4_DUMP_PATH");
    std::FILE *f = std::fopen(path ? path : "frame.ppm", "wb");
    if (f) {
        std::fprintf(f, "P6\n%d %d\n255\n", w, h);
        // GLES rend l origine en bas ; on reecrit les lignes a l endroit.
        for (int y = h - 1; y >= 0; --y)
            for (int x = 0; x < w; ++x)
                std::fwrite(px + ((size_t) y * w + x) * 4, 1, 3, f);
        std::fclose(f);
        std::printf("[PORT] image %ld enregistree (%dx%d), %ld appels de dessin, %ld triangles\n",
                    want, w, h, g_draws, g_tris);
    }
    std::free(px);
    std::fflush(stdout);
    std::_Exit(0);
}

extern "C" int port_tex_cache_count();
extern "C" void port_tex_stats(unsigned *n, unsigned *b);
void GXCopyDisp(void *dest, GXBool clear)
{
    (void)dest;
    if (!g_gl_ready)
        return;
    gx_replay();
    {
        static struct timespec t0; static unsigned n = 0; static double tgl = 0;
        struct timespec a1, a2; clock_gettime(CLOCK_MONOTONIC, &a1);
        static const bool fin = getenv("RE4_FINISH") != nullptr;
        if (fin) glFinish();
        clock_gettime(CLOCK_MONOTONIC, &a2);
        tgl += (a2.tv_sec - a1.tv_sec) * 1e3 + (a2.tv_nsec - a1.tv_nsec) / 1e6;
        if (n == 0) t0 = a2;
        if (++n == 30) {
            unsigned dn, db; port_tex_stats(&dn, &db);
            double ms = (a2.tv_sec - t0.tv_sec) * 1e3 + (a2.tv_nsec - t0.tv_nsec) / 1e6;
            std::printf("[PERF] 30 images GX en %.0f ms (%.1f ms/image, dont glFinish %.1f), %ld dessins, %ld tri, cache %d, decodes %u (%u Ko)\n", ms, ms / 30, tgl / 30, g_draws, g_tris, port_tex_cache_count(), dn, db / 1024);
            std::fflush(stdout); n = 0; tgl = 0;
        }
    }
    port_dump_frame();
    if (clear) {
        glClearColor(g_clear.r / 255.0f, g_clear.g / 255.0f, g_clear.b / 255.0f, g_clear.a / 255.0f);
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    g_nverts = 0;
    g_draws = 0;
    g_tris = 0;
}


// Masque des etages de texture indirecte. Le portage ne fait pas encore d'indirect texturing
// (id_sys s'en sert pour ses deformations), mais le symbole est interne au SDK et doit exister.
extern "C" void __GXSetIndirectMask(u32 mask) { (void) mask; }
