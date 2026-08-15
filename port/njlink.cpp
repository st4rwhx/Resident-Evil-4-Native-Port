// -*- coding: utf-8 -*-
// Lien avec l'appli Nightjar (phase N2 du plan natif, nj-work/docs/PLAN_NATIF_NIGHTJAR_2026-11-11.md).
//
// Sur Android, la memoire du jeu doit etre a 0x80000000 (adresses GameCube) et cette zone est prise
// dans le processus d'une appli (tas d'ART, mesure du 11/11 : 0x7C480000-0x91480000). Le jeu tourne
// donc dans un processus a lui, lance par l'appli, et ils se parlent par une prise Unix (paquets) :
//
//   coeur -> appli : HELLO, BUFFERS (puis n AHardwareBuffer), FRAME (emplacement + barriere GPU)
//   appli -> coeur : PAD (etat de la manette), RELEASE (emplacement rendu), QUIT
//
// Le coeur dessine lui-meme, dans sa memoire : chaque image finie est copiee dans une des images
// partagees (AHardwareBuffer), que l'appli affiche sous son interface. Aucune commande de rendu ne
// traverse les processus. Sans NJ_LINK_FD (lancement par adb), ce module ne fait rien.
#include "njlink.h"

#include <android/hardware_buffer.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int g_fd = -1;
const int kSlots = 4;   // 4 (15/11) : une affichee, deux en attente dans l appli (cadence fixe), une en dessin
AHardwareBuffer* g_ahb[kSlots];
GLuint g_tex[kSlots], g_fbo[kSlots];
bool g_busy[kSlots];   // emplacement envoye, pas encore rendu par l'appli
int g_w, g_h;
unsigned g_seq;
bool g_ready;
NjPad g_pad;
bool g_quit;

PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC p_getClientBuffer;
PFNEGLCREATEIMAGEKHRPROC p_createImage;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC p_imageTarget;
PFNEGLCREATESYNCKHRPROC p_createSync;
PFNEGLDESTROYSYNCKHRPROC p_destroySync;
PFNEGLDUPNATIVEFENCEFDANDROIDPROC p_dupFence;

extern "C" void njstate_fill(NjState* out);   // port/njstate.cpp
extern "C" void njstate_case(int op, int board, const void* data);

// Etat du jeu pris par le fil du jeu a la passation de l'image (njlink_capture_state, njgfx) : le fil de rendu
// envoyait un njstate_fill lu en direct dans la memoire du jeu pendant un changement de salle (plantage dans
// motion_index, 15/11 : le mouvement de Leon venait d'etre libere).
static NjState g_state_snap;
static bool g_state_valid = false;
extern "C" void njlink_capture_state(void)
{
    njstate_fill(&g_state_snap);
    g_state_valid = true;
}
// Etat du jeu pour les commandes tactiles (NJ_MSG_STATE) : un paquet NjMsg + NjState.
void send_state()
{
    struct {
        NjMsg m;
        NjState s;
    } p;
    std::memset(&p, 0, sizeof p);
    p.m.type = NJ_MSG_STATE;
    p.m.a = (uint32_t) sizeof(NjState);
    if (g_state_valid)
        p.s = g_state_snap;
    else
        njstate_fill(&p.s);
    send(g_fd, &p, sizeof p, MSG_NOSIGNAL | MSG_DONTWAIT);
}

bool send_msg(const NjMsg& m, int fence)
{
    struct iovec iov = {(void*) &m, sizeof m};
    struct msghdr mh = {};
    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;
    char ctl[CMSG_SPACE(sizeof(int))];
    if (fence >= 0) {
        std::memset(ctl, 0, sizeof ctl);
        mh.msg_control = ctl;
        mh.msg_controllen = sizeof ctl;
        struct cmsghdr* c = CMSG_FIRSTHDR(&mh);
        c->cmsg_level = SOL_SOCKET;
        c->cmsg_type = SCM_RIGHTS;
        c->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(c), &fence, sizeof(int));
    }
    return sendmsg(g_fd, &mh, MSG_NOSIGNAL) == (ssize_t) sizeof m;
}

// Messages de l'appli, sans attendre.
void poll_host()
{
    if (g_fd < 0)
        return;
    NjMsg m;
    for (;;) {
        const ssize_t n = recv(g_fd, &m, sizeof m, MSG_DONTWAIT);
        if (n == 0) {   // l'appli est partie
            g_quit = true;
            return;
        }
        if (n != (ssize_t) sizeof m)
            return;
        switch (m.type) {
        case NJ_MSG_PAD:
            std::memcpy(&g_pad, m.data, sizeof g_pad);
            break;
        case NJ_MSG_RELEASE:
            if (m.a < (unsigned) kSlots)
                g_busy[m.a] = false;
            break;
        case NJ_MSG_QUIT:
            g_quit = true;
            break;
        case NJ_MSG_CASE:
            njstate_case((int) m.a, (int) m.b, m.data);
            break;
        default:
            break;
        }
    }
}

}  // namespace

extern "C" {

int njlink_active(void) { return g_fd >= 0; }

void njlink_init(void)
{
    const char* e = std::getenv("NJ_LINK_FD");
    if (!e)
        return;
    g_fd = std::atoi(e);
    fcntl(g_fd, F_SETFD, FD_CLOEXEC);
    NjMsg m = {};
    m.type = NJ_MSG_HELLO;
    m.a = NJ_LINK_VERSION;
    if (!send_msg(m, -1)) {
        std::printf("[PORT] lien appli : envoi impossible, lien coupe\n");
        g_fd = -1;
        return;
    }
    std::printf("[PORT] lien appli : actif (prise %d)\n", g_fd);
}

// Appelee une fois le contexte GL du coeur cree (taille de l'image du jeu).
void njlink_gl_ready(int w, int h)
{
    if (g_fd < 0 || g_ready)
        return;
    p_getClientBuffer = (PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC) eglGetProcAddress("eglGetNativeClientBufferANDROID");
    p_createImage = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    p_imageTarget = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");
    p_createSync = (PFNEGLCREATESYNCKHRPROC) eglGetProcAddress("eglCreateSyncKHR");
    p_destroySync = (PFNEGLDESTROYSYNCKHRPROC) eglGetProcAddress("eglDestroySyncKHR");
    p_dupFence = (PFNEGLDUPNATIVEFENCEFDANDROIDPROC) eglGetProcAddress("eglDupNativeFenceFDANDROID");
    if (!p_getClientBuffer || !p_createImage || !p_imageTarget) {
        std::printf("[PORT] lien appli : extensions EGL d'image absentes\n");
        return;
    }
    EGLDisplay dpy = eglGetCurrentDisplay();
    g_w = w;
    g_h = h;
    for (int i = 0; i < kSlots; ++i) {
        AHardwareBuffer_Desc d = {};
        d.width = (uint32_t) w;
        d.height = (uint32_t) h;
        d.layers = 1;
        d.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
        d.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
        if (AHardwareBuffer_allocate(&d, &g_ahb[i]) != 0) {
            std::printf("[PORT] lien appli : AHardwareBuffer_allocate a echoue\n");
            return;
        }
        EGLClientBuffer cb = p_getClientBuffer(g_ahb[i]);
        const EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
        EGLImageKHR img = p_createImage(dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, cb, attrs);
        glGenTextures(1, &g_tex[i]);
        glBindTexture(GL_TEXTURE_2D, g_tex[i]);
        p_imageTarget(GL_TEXTURE_2D, (GLeglImageOES) img);
        glGenFramebuffers(1, &g_fbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, g_fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_tex[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    NjMsg m = {};
    m.type = NJ_MSG_BUFFERS;
    m.a = kSlots;
    m.b = (unsigned) w;
    m.c = (unsigned) h;
    send_msg(m, -1);
    for (int i = 0; i < kSlots; ++i)
        AHardwareBuffer_sendHandleToUnixSocket(g_ahb[i], g_fd);
    g_ready = true;
    std::printf("[PORT] lien appli : %d images partagees %dx%d\n", kSlots, w, h);
}

// Image du jeu finie (dans le tampon par defaut du coeur) : copiee dans un emplacement libre et
// annoncee a l'appli. Sans emplacement libre (l'appli n'a pas encore rendu les autres), l'image
// n'est pas montree : le jeu n'attend jamais l'affichage.
void njlink_frame(void)
{
    poll_host();
    if (!g_ready)
        return;
    int slot = -1;
    for (int k = 1; k <= kSlots; ++k) {
        const int i = (int) ((g_seq + k) % kSlots);
        if (!g_busy[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return;
    GLint prev = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_fbo[slot]);
    glBlitFramebuffer(0, 0, g_w, g_h, 0, 0, g_w, g_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) prev);
    int fence = -1;
    EGLDisplay dpy = eglGetCurrentDisplay();
    if (p_createSync && p_dupFence) {
        const EGLint a[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID, EGL_NONE};
        EGLSyncKHR s = p_createSync(dpy, EGL_SYNC_NATIVE_FENCE_ANDROID, a);
        glFlush();
        if (s != EGL_NO_SYNC_KHR) {
            fence = p_dupFence(dpy, s);
            p_destroySync(dpy, s);
        }
    } else {
        glFinish();
    }
    NjMsg m = {};
    m.type = NJ_MSG_FRAME;
    m.a = (unsigned) slot;
    m.b = ++g_seq;
    g_busy[slot] = true;
    send_msg(m, fence);
    if (fence >= 0)
        close(fence);
}

// Rendu natif (njgfx) : l'image est dans le tampon `fbo`, rectangle (x, y, w, h).
void njlink_frame_fbo(unsigned fbo, int x, int y, int w, int h);
static unsigned g_xfb_w = 0, g_xfb_h = 0;   // zone de l'EFB que le jeu copie dans son XFB (FRAME.c = w << 16 | h)
void njlink_set_xfb(unsigned w, unsigned h) { g_xfb_w = w; g_xfb_h = h; }
static int g_dst_y0 = 0, g_dst_h = 0;   // lignes de destination (0 = en haut) ; g_dst_h 0 : image entiere

void njlink_frame_fbo2(unsigned fbo, int x, int y, int w, int h, int dy, int dh)
{
    g_dst_y0 = dy;
    g_dst_h = dh;
    njlink_frame_fbo(fbo, x, y, w, h);
    g_dst_h = 0;
}

// Copie EFB -> image partagee par un dessin (15/11). glBlitFramebuffer echouait par moments en GL_INVALID_OPERATION
// sur MuMu (sessions entieres noires, ou une image noire par intervalles : le « scintillement »), pour des tampons
// pourtant complets. Un rectangle texture est toujours valide. Source : la texture de couleur de `fbo`, de taille
// njlink_src_size. Le tampon de destination (image partagee) est deja lie en ecriture. Etat GL rendu tel quel.
static int g_src_w = 0, g_src_h = 0;
extern "C" void njlink_src_size(int w, int h) { g_src_w = w; g_src_h = h; }
static bool copy_draw(unsigned fbo, int x, int y, int w, int h, int dy0, int dy1)
{
    static GLuint prog = 0;
    static GLint u_src = -1, u_tex = -1;
    static bool failed = false;
    if (failed || g_src_w <= 0 || g_src_h <= 0)
        return false;
    if (!prog) {
        const char *vs =
            "#version 300 es\n"
            "uniform vec4 u_src;\n"
            "out vec2 v_uv;\n"
            "void main() {\n"
            "  vec2 p = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));\n"
            "  v_uv = mix(u_src.xy, u_src.zw, p);\n"
            "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
            "}\n";
        const char *fs =
            "#version 300 es\n"
            "precision mediump float;\n"
            "uniform sampler2D u_tex;\n"
            "in vec2 v_uv;\n"
            "out vec4 o;\n"
            "void main() { o = vec4(texture(u_tex, v_uv).rgb, 1.0); }\n";
        GLuint s[2] = {glCreateShader(GL_VERTEX_SHADER), glCreateShader(GL_FRAGMENT_SHADER)};
        glShaderSource(s[0], 1, &vs, nullptr);
        glShaderSource(s[1], 1, &fs, nullptr);
        GLint ok = 0;
        for (GLuint sh : s) {
            glCompileShader(sh);
            glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok) { failed = true; std::printf("[NJLINK] copie par dessin : nuanceur refuse, retour a la copie de tampon\n"); return false; }
        }
        prog = glCreateProgram();
        glAttachShader(prog, s[0]);
        glAttachShader(prog, s[1]);
        glLinkProgram(prog);
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        glDeleteShader(s[0]);
        glDeleteShader(s[1]);
        if (!ok) { failed = true; std::printf("[NJLINK] copie par dessin : programme refuse, retour a la copie de tampon\n"); return false; }
        u_src = glGetUniformLocation(prog, "u_src");
        u_tex = glGetUniformLocation(prog, "u_tex");
        std::printf("[NJLINK] copie par dessin active (source %dx%d)\n", g_src_w, g_src_h);
    }
    GLint tex = 0;
    glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex);
    if (!tex)
        return false;
    (void) fbo;
    // etat a rendre
    GLint p0 = 0, vao0 = 0, at0 = 0, t0 = 0, smp0 = 0, vp[4];
    glGetIntegerv(GL_CURRENT_PROGRAM, &p0);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao0);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &at0);
    glGetIntegerv(GL_VIEWPORT, vp);
    const GLboolean bl = glIsEnabled(GL_BLEND), dt = glIsEnabled(GL_DEPTH_TEST), cf = glIsEnabled(GL_CULL_FACE),
                    st = glIsEnabled(GL_STENCIL_TEST), rd = glIsEnabled(GL_RASTERIZER_DISCARD);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &t0);
    glGetIntegerv(GL_SAMPLER_BINDING, &smp0);
    static GLuint vao = 0;
    if (!vao) glGenVertexArrays(1, &vao);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_RASTERIZER_DISCARD);
    glUseProgram(prog);
    glBindVertexArray(vao);
    glBindTexture(GL_TEXTURE_2D, (GLuint) tex);
    glBindSampler(0, 0);
    glUniform1i(u_tex, 0);
    glUniform4f(u_src, (float) x / g_src_w, (float) y / g_src_h, (float) (x + w) / g_src_w, (float) (y + h) / g_src_h);
    glViewport(0, dy0, g_w, dy1 - dy0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glBindSampler(0, (GLuint) smp0);
    glBindTexture(GL_TEXTURE_2D, (GLuint) t0);
    glActiveTexture((GLenum) at0);
    glBindVertexArray((GLuint) vao0);
    glUseProgram((GLuint) p0);
    if (bl) glEnable(GL_BLEND);
    if (dt) glEnable(GL_DEPTH_TEST);
    if (cf) glEnable(GL_CULL_FACE);
    if (st) glEnable(GL_STENCIL_TEST);
    if (rd) glEnable(GL_RASTERIZER_DISCARD);
    return true;
}

void njlink_frame_fbo(unsigned fbo, int x, int y, int w, int h)
{
    poll_host();
    if (!g_ready)
        return;
    send_state();
    int slot = -1;
    for (int k = 1; k <= kSlots; ++k) {
        const int i = (int) ((g_seq + k) % kSlots);
        if (!g_busy[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return;
    GLint prev = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint) fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_fbo[slot]);
    // la copie obeit au ciseau et au masque de couleur que le rendu laisse tels que le jeu les a poses
    // (en jeu : rectangle de ciseau, ecriture de l'alpha seul) : l'image partait noire (nj22)
    GLboolean cm0[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, cm0);
    const GLboolean sc0 = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    static unsigned n_blit = 0;
    const bool diag = (++n_blit % 300) == 0;
    GLenum e0 = 0, e1 = 0, rs = 0, ds = 0;
    GLint rb = 0;
    if (diag) {
        e0 = glGetError();
        rs = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
        ds = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        glGetIntegerv(GL_READ_BUFFER, &rb);
    }
    static const bool use_blit = [] { const char *e = std::getenv("NJ_LINK_BLIT"); return e && e[0] == '1'; }();
    if (g_dst_h > 0) {   // bandes noires au-dessus et au-dessous, comme l'ecran du GameCube
        const int y0 = g_h - (g_dst_y0 + g_dst_h) * g_h / 480, y1 = g_h - g_dst_y0 * g_h / 480;
        GLfloat cc[4];
        glGetFloatv(GL_COLOR_CLEAR_VALUE, cc);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(cc[0], cc[1], cc[2], cc[3]);
        if (use_blit || !copy_draw(fbo, x, y, w, h, y0, y1))
            glBlitFramebuffer(x, y, x + w, y + h, 0, y0, g_w, y1, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    } else {
        if (use_blit || !copy_draw(fbo, x, y, w, h, 0, g_h))
            glBlitFramebuffer(x, y, x + w, y + h, 0, 0, g_w, g_h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    if (diag) {
        e1 = glGetError();
        {   // pixel source (EFB) et destination (image partagee), au milieu de la zone de jeu (512 x 448)
            unsigned char ps[4] = {}, pd[4] = {};
            GLint rf = 0, df = 0, ca = 0;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rf);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &df);
            glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &ca);
            glReadPixels(x + 256, y + h - 224, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, ps);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, g_fbo[slot]);
            glReadPixels(g_w * 256 / 640, g_h - g_h * 224 / 480, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pd);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint) fbo);
            std::printf("[NJLINK] pixels %u : fbo lu %d (couleur %d) ecrit %d ; source %02x%02x%02x%02x destination %02x%02x%02x%02x\n", n_blit,
                        rf, ca, df, ps[0], ps[1], ps[2], ps[3], pd[0], pd[1], pd[2], pd[3]);
        }
        std::printf("[NJLINK] copie %u : erreur avant %x apres %x, lecture %x (tampon %x), ecriture %x, src (%d,%d %dx%d)\n", n_blit, e0, e1, rs,
                    (unsigned) rb, ds, x, y, w, h);
    }
    glColorMask(cm0[0], cm0[1], cm0[2], cm0[3]);
    if (sc0) glEnable(GL_SCISSOR_TEST);
    {   // l'alpha de l'EFB n'est pas une opacite (le XFB du GameCube n'en a pas) : en jeu il vaut souvent 0,
        // et l'appli affichait alors l'image transparente, donc noire (nj18 : centre 7D7D76, alpha 00)
        GLboolean cm[4];
        GLfloat cc[4];
        glGetBooleanv(GL_COLOR_WRITEMASK, cm);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, cc);
        const GLboolean sc = glIsEnabled(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, g_fbo[slot]);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glColorMask(cm[0], cm[1], cm[2], cm[3]);
        glClearColor(cc[0], cc[1], cc[2], cc[3]);
        if (sc) glEnable(GL_SCISSOR_TEST);
        static unsigned n_meas = 0;
        if (++n_meas % 300 == 0) {   // diagnostic N3 : ce qui part vraiment a l'appli (16 points de l'image partagee)
            glBindFramebuffer(GL_READ_FRAMEBUFFER, g_fbo[slot]);
            unsigned sum = 0, lit = 0;
            unsigned char px[4];
            for (int j = 0; j < 4; ++j)
                for (int i = 0; i < 4; ++i) {
                    glReadPixels(g_w * (2 * i + 1) / 8, g_h * (2 * j + 1) / 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
                    sum += px[0] + px[1] + px[2];
                    lit += (px[0] | px[1] | px[2]) > 8;
                }
            // image complete dans un fichier (PPM), dossier de l'appli. NJ_DUMP_AT=2400,3000,... (multiples de 300) :
            // images choisies, pour la comparaison A/B au pixel (banc G1, 14/11) ; defaut 2400 et 2700.
            static unsigned dump_at[16] = {2400, 2700};
            static int n_dump = [] {
                const char* e = std::getenv("NJ_DUMP_AT");
                if (!e) return 2;
                int n = 0;
                for (const char* q = e; *q && n < 16;) {
                    dump_at[n++] = (unsigned) std::strtoul(q, nullptr, 10);
                    const char* c = std::strchr(q, ',');
                    if (!c) break;
                    q = c + 1;
                }
                return n;
            }();
            bool want = false;
            for (int k = 0; k < n_dump; ++k) want |= n_meas == dump_at[k];
            if (want) {
                static unsigned char* img = nullptr;
                static size_t img_size = 0;
                if (img_size < (size_t) g_w * g_h * 4) img = (unsigned char*) std::realloc(img, img_size = (size_t) g_w * g_h * 4);
                if (img) {
                    glReadPixels(0, 0, g_w, g_h, GL_RGBA, GL_UNSIGNED_BYTE, img);
                    char path[300];
                    const char* d = std::getenv("NJ_STORAGE_DIR");
                    std::snprintf(path, sizeof path, "%s/njlink_%u.ppm", d ? d : ".", n_meas);
                    if (std::FILE* f = std::fopen(path, "wb")) {
                        std::fprintf(f, "P6 %d %d 255\n", g_w, g_h);
                        for (int y = g_h - 1; y >= 0; --y)
                            for (int x = 0; x < g_w; ++x) std::fwrite(img + 4 * (y * g_w + x), 1, 3, f);
                        std::fclose(f);
                    }
                }
            }
            std::printf("[NJLINK] envoi %u : %ux%u, %u/16 points allumes, somme %u, alpha %u\n", n_meas, g_w, g_h, lit, sum, px[3]);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) prev);
    int fence = -1;
    EGLDisplay dpy = eglGetCurrentDisplay();
    if (p_createSync && p_dupFence) {
        const EGLint a[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID, EGL_NONE};
        EGLSyncKHR s = p_createSync(dpy, EGL_SYNC_NATIVE_FENCE_ANDROID, a);
        glFlush();
        if (s != EGL_NO_SYNC_KHR) {
            fence = p_dupFence(dpy, s);
            p_destroySync(dpy, s);
        }
    } else {
        glFinish();
    }
    NjMsg m = {};
    m.type = NJ_MSG_FRAME;
    m.a = (unsigned) slot;
    m.b = ++g_seq;
    m.c = (g_xfb_w << 16) | (g_xfb_h & 0xFFFF);
    g_busy[slot] = true;
    send_msg(m, fence);
    if (fence >= 0)
        close(fence);
}

int njlink_pad(NjPad* out)
{
    poll_host();
    if (g_fd < 0)
        return 0;
    *out = g_pad;
    return 1;
}

int njlink_quit(void)
{
    poll_host();
    return g_quit;
}

}
