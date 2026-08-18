// Moteur natif Nightjar : le coeur du jeu (notre portage, decompilation compilee) dans un processus a lui.
//
// Android 32 bits : la memoire du jeu doit etre a 0x80000000 (adresses GameCube) et cette zone est
// prise par le tas d'ART dans le processus de l'appli (mesure S5 du 11/11 : 0x7C480000-0x91480000).
// L'appli lance donc le coeur (programme range dans le dossier des bibliotheques de l'APK sous le nom
// libnjcore.so, seul endroit d'ou Android laisse executer) et lui parle par une prise Unix :
//   coeur -> appli : HELLO, BUFFERS + AHardwareBuffer, FRAME (emplacement + barriere GPU)
//   appli -> coeur : PAD, RELEASE, QUIT
// Le coeur dessine lui-meme ; l'appli copie l'image finie dans son tampon (efb) et la presente avec
// son interface (tactile, menus, 16:9). Protocole : runtime/native/njlink_proto.h (meme fichier que
// port/njlink.h du coeur). Aucune ligne de CubeEvil4 (DECISION_SANS_CUBEEVIL4_2026-11-11).
#include "runtime/native/native_core.h"
#include "runtime/virtual_disc.h"
#include "runtime/native/njlink_proto.h"
#include "runtime/gfx_settings.h"

#if defined(__ANDROID__)
#include <android/hardware_buffer.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#elif defined(__APPLE__)
// iOS (18/11, I6 du port natif complet) : le coeur natif est lie dans l'appli et tourne dans un fil de ce processus
// (pas de fork/exec ni d'AHardwareBuffer) ; meme contrat njlink, transport en memoire (port/njlink.cpp, NJ_LINK_INPROC).
#include <OpenGLES/ES3/gl.h>
#include <SDL.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#endif

namespace nj::native {

#if defined(__ANDROID__) || defined(__APPLE__)
// Copie par un dessin (15/11) : glBlitFramebuffer echouait par moments sur MuMu (GL_INVALID_OPERATION) et l'appli
// gardait alors l'image precedente (« les images reviennent en arriere »). Meme remede que cote coeur (port/njlink.cpp).
// Dessine la texture `tex` dans le tampon lie, rectangle (x, y, w, h). Etat GL rendu tel quel.
static bool copy_draw(GLuint tex, int x, int y, int w, int h) {
  static GLuint prog = 0, vao = 0;
  static GLint u_tex = -1;
  static bool failed = false;
  if (failed) return false;
  if (!prog) {
    const char *vs =
        "#version 300 es\n"
        "out vec2 v_uv;\n"
        "void main() {\n"
        "  vec2 p = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));\n"
        "  v_uv = p;\n"
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
      if (!ok) { failed = true; return false; }
    }
    prog = glCreateProgram();
    glAttachShader(prog, s[0]);
    glAttachShader(prog, s[1]);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(s[0]);
    glDeleteShader(s[1]);
    if (!ok) { failed = true; return false; }
    u_tex = glGetUniformLocation(prog, "u_tex");
    glGenVertexArrays(1, &vao);
  }
  GLint p0 = 0, vao0 = 0, at0 = 0, t0 = 0, smp0 = 0, vp[4];
  glGetIntegerv(GL_CURRENT_PROGRAM, &p0);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao0);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &at0);
  glGetIntegerv(GL_VIEWPORT, vp);
  const GLboolean bl = glIsEnabled(GL_BLEND), dt = glIsEnabled(GL_DEPTH_TEST), cf = glIsEnabled(GL_CULL_FACE),
                  st = glIsEnabled(GL_STENCIL_TEST);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &t0);
  glGetIntegerv(GL_SAMPLER_BINDING, &smp0);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_STENCIL_TEST);
  glUseProgram(prog);
  glBindVertexArray(vao);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindSampler(0, 0);
  glUniform1i(u_tex, 0);
  glViewport(x, y, w, h);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  glBindSampler(0, (GLuint)smp0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)t0);
  glActiveTexture((GLenum)at0);
  glBindVertexArray((GLuint)vao0);
  glUseProgram((GLuint)p0);
  if (bl) glEnable(GL_BLEND);
  if (dt) glEnable(GL_DEPTH_TEST);
  if (cf) glEnable(GL_CULL_FACE);
  if (st) glEnable(GL_STENCIL_TEST);
  return true;
}

#endif

#if defined(__ANDROID__)
namespace {

const int kMaxSlots = 4;

struct Link {
  std::string exe;          // chemin du coeur (libnjcore.so)
  std::string saves;        // dossier des sauvegardes du coeur (data_dir/memcard)
  std::string err;
  pid_t pid = -1;
  int fd = -1;
  State st = State::Idle;
  int nslots = 0, w = 0, h = 0;
  AHardwareBuffer *ahb[kMaxSlots] = {};
  GLuint tex[kMaxSlots] = {}, fbo[kMaxSlots] = {};
  bool imported = false;
  int pending = -1, pending_fence = -1;   // image a montrer (tete de la file)
  // File des images recues, pas encore affichees (15/11, cadence fixe) : avant, une image arrivee pendant qu'une autre
  // attendait la remplacait (15-30 % d'images perdues). Tete = pending/pending_fence, puis q[]/qf[] dans l'ordre.
  int q[3] = {-1, -1, -1}, qf[3] = {-1, -1, -1};
  int qn = 0;
  int shown = -1;                          // emplacement affiche
  Pad last_pad{};
  bool pad_sent = false;
  std::mutex state_mu;      // etat du jeu (NJ_MSG_STATE), lu par le tactile
  NjState state{};
  bool has_state = false;
  uint32_t xfb = 0;          // zone de l'EFB copiee dans l'XFB (largeur << 16 | hauteur), envoyee avec chaque image
} g;

// libandroid : AHardwareBuffer (API 26) charge a l'execution, l'appli vise l'API 24.
int (*p_recv)(int, AHardwareBuffer **) = nullptr;
PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC p_client = nullptr;
PFNEGLCREATEIMAGEKHRPROC p_image = nullptr;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC p_target = nullptr;
PFNEGLCREATESYNCKHRPROC p_sync = nullptr;
PFNEGLWAITSYNCKHRPROC p_wait = nullptr;
PFNEGLDESTROYSYNCKHRPROC p_destroy = nullptr;

bool send(const NjMsg &m) {
  return g.fd >= 0 && ::send(g.fd, &m, sizeof m, MSG_NOSIGNAL | MSG_DONTWAIT) == (ssize_t)sizeof m;
}

void release_slot(int slot) {
  if (slot < 0) return;
  NjMsg m{};
  m.type = NJ_MSG_RELEASE;
  m.a = (uint32_t)slot;
  send(m);
}

// Messages du coeur, sans attendre (fil du rendu de l'appli).
void poll() {
  if (g.fd < 0) return;
  for (;;) {
    struct {
      NjMsg m;
      NjState s;
    } pk{};
    NjMsg &m = pk.m;
    struct iovec iov = {&pk, sizeof pk};
    char ctl[CMSG_SPACE(sizeof(int))];
    struct msghdr mh = {};
    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;
    mh.msg_control = ctl;
    mh.msg_controllen = sizeof ctl;
    const ssize_t n = recvmsg(g.fd, &mh, MSG_DONTWAIT);
    if (n == 0) { g.st = State::Ended; close(g.fd); g.fd = -1; return; }
    if (n < (ssize_t)sizeof m) return;
    int fence = -1;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c))
      if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) std::memcpy(&fence, CMSG_DATA(c), sizeof(int));
    switch (m.type) {
    case NJ_MSG_HELLO:
      if (m.a != NJ_LINK_VERSION) { g.err = "version du coeur differente de celle de l'appli"; g.st = State::Failed; }
      else g.st = State::Running;
      break;
    case NJ_MSG_BUFFERS:
      g.nslots = (int)(m.a < (uint32_t)kMaxSlots ? m.a : kMaxSlots);
      g.w = (int)m.b;
      g.h = (int)m.c;
      for (int i = 0; i < g.nslots; ++i) {   // le coeur les envoie juste apres : on peut attendre
        const int fl = fcntl(g.fd, F_GETFL);
        fcntl(g.fd, F_SETFL, fl & ~O_NONBLOCK);
        const int r = p_recv ? p_recv(g.fd, &g.ahb[i]) : -1;
        fcntl(g.fd, F_SETFL, fl);
        if (r != 0) { g.err = "image partagee non recue"; g.st = State::Failed; return; }
      }
      g.imported = false;
      break;
    case NJ_MSG_FRAME:
      if (m.a < (uint32_t)g.nslots) {
        if (g.pending < 0) {
          g.pending = (int)m.a;
          g.pending_fence = fence;
        } else {
          if (g.qn == 3) {   // file pleine : la plus ancienne en attente part (le retard ne s'accumule pas)
            release_slot(g.pending);
            if (g.pending_fence >= 0) close(g.pending_fence);
            g.pending = g.q[0];
            g.pending_fence = g.qf[0];
            for (int k = 0; k < 2; ++k) { g.q[k] = g.q[k + 1]; g.qf[k] = g.qf[k + 1]; }
            g.qn = 2;
          }
          g.q[g.qn] = (int)m.a;
          g.qf[g.qn] = fence;
          ++g.qn;
        }
        if (m.c) g.xfb = m.c;
        fence = -1;
      }
      break;
    case NJ_MSG_STATE:
      if (n == (ssize_t)sizeof pk && m.a == sizeof(NjState)) {
        std::lock_guard<std::mutex> lk(g.state_mu);
        g.state = pk.s;
        g.has_state = true;
      }
      break;
    default:
      break;
    }
    if (fence >= 0) close(fence);
  }
}

bool import_buffers() {
  if (g.imported) return true;
  EGLDisplay dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY || !p_client || !p_image || !p_target) return false;
  for (int i = 0; i < g.nslots; ++i) {
    const EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    EGLImageKHR img = p_image(dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, p_client(g.ahb[i]), attrs);
    if (img == EGL_NO_IMAGE_KHR) return false;
    glGenTextures(1, &g.tex[i]);
    glBindTexture(GL_TEXTURE_2D, g.tex[i]);
    p_target(GL_TEXTURE_2D, (GLeglImageOES)img);
    glGenFramebuffers(1, &g.fbo[i]);
  }
  g.imported = true;
  return true;
}

}  // namespace

bool load(const std::string &lib_dir_in, std::string &error) {
  std::string lib_dir = lib_dir_in;
  if (lib_dir.empty()) {   // dossier des bibliotheques de l'APK : celui de libmain.so (ce code)
    Dl_info info{};
    if (dladdr(reinterpret_cast<void *>(&load), &info) && info.dli_fname) {
      lib_dir = info.dli_fname;
      lib_dir = lib_dir.substr(0, lib_dir.find_last_of('/'));
    }
  }
  g.exe = lib_dir + "/libnjcore.so";
  struct stat sb;
  if (stat(g.exe.c_str(), &sb) != 0) {
    error = "coeur natif introuvable : " + g.exe;
    return false;
  }
  if (void *a = dlopen("libandroid.so", RTLD_NOW))
    p_recv = reinterpret_cast<int (*)(int, AHardwareBuffer **)>(dlsym(a, "AHardwareBuffer_recvHandleFromUnixSocket"));
  p_client = (PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC)eglGetProcAddress("eglGetNativeClientBufferANDROID");
  p_image = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  p_target = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
  p_sync = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
  p_wait = (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglWaitSyncKHR");
  p_destroy = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
  if (!p_recv) {
    error = "AHardwareBuffer indisponible (Android 8 ou plus requis)";
    return false;
  }
  return true;
}

// data_dir : dossier du disque extrait (disc1/{sys,files}, disc2, memcard).
bool start(const std::string &data_dir, const std::string &, std::string &error) {
  if (g.exe.empty()) { error = "coeur non charge"; return false; }
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) != 0) { error = "socketpair : " + std::string(std::strerror(errno)); return false; }
  const std::string game = data_dir + "/disc1";
  nj::runtime::DiscView::get().set_folder(game + "/files");   // icones d'armes generees chez le joueur (icon_gen.cpp)
  const std::string saves = data_dir + "/memcard";
  g.saves = saves;
  mkdir(saves.c_str(), 0700);
  const std::string log = data_dir + "/../core.log";
  const pid_t pid = fork();
  if (pid < 0) { error = "fork : " + std::string(std::strerror(errno)); close(sv[0]); close(sv[1]); return false; }
  if (pid == 0) {   // coeur : la prise sv[1] reste ouverte a travers exec
    close(sv[0]);
    char num[16];
    std::snprintf(num, sizeof num, "%d", sv[1]);
    setenv("NJ_LINK_FD", num, 1);
    setenv("RE4_SAVE_DIR", saves.c_str(), 1);
    setenv("NJ_TITLE_APP", nj::runtime::gfx::g_title_menu ? "1" : "0", 1);   // menu titre de Nightjar (port/arm_hooks.cpp)
    setenv("NJ_DEBUG_HUD", nj::runtime::gfx::g_debug_mode ? "1" : "0", 1);   // barres et textes du debug du jeu
    setenv("NJ_COSTUME", std::to_string(nj::runtime::gfx::g_leon_outfit).c_str(), 1);   // garde-robe (18/11)
    setenv("NJ_TITLE_SCENE", nj::runtime::gfx::g_title_menu && nj::runtime::gfx::g_title_scene ? "0x100" : "0", 1);   // fond vivant : r100
    {   // dossier de stockage de l'appli (mods, textures HD, journaux) : le coeur n'a pas de Java pour le demander
      const std::string storage = data_dir.substr(0, data_dir.find_last_of('/'));
      setenv("NJ_STORAGE_DIR", storage.c_str(), 1);
      if (chdir(storage.c_str()) != 0) {}
    }
    const int lf = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); close(lf); }
    execl(g.exe.c_str(), g.exe.c_str(), game.c_str(), (char *)nullptr);
    _exit(127);
  }
  close(sv[1]);
  g.pid = pid;
  g.fd = sv[0];
  fcntl(g.fd, F_SETFL, fcntl(g.fd, F_GETFL) | O_NONBLOCK);
  fcntl(g.fd, F_SETFD, FD_CLOEXEC);
  g.st = State::Idle;
  return true;
}

State state() {
  poll();
  if (g.pid > 0) {
    int status = 0;
    if (waitpid(g.pid, &status, WNOHANG) == g.pid) {
      g.pid = -1;
      g.st = (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? State::Ended : State::Failed;
      if (g.st == State::Failed) {
        char b[96];
        std::snprintf(b, sizeof b, "coeur arrete (%s %d)", WIFSIGNALED(status) ? "signal" : "code",
                      WIFSIGNALED(status) ? WTERMSIG(status) : WEXITSTATUS(status));
        g.err = b;
      }
    }
  }
  return g.st;
}
std::string message() { return g.err; }

void pad(const Pad &p) {
  if (g.pad_sent && std::memcmp(&p, &g.last_pad, sizeof p) == 0) return;
  NjMsg m{};
  m.type = NJ_MSG_PAD;
  m.a = (uint32_t)p.accel_cg * 10u;   // milliemes de g : le coeur en tire les actions de secousse Wii (pad.cpp)
  NjPad np{p.buttons, p.stick_x, p.stick_y, p.substick_x, p.substick_y, p.trigger_l, p.trigger_r};
  std::memcpy(m.data, &np, sizeof np);
  if (send(m)) { g.last_pad = p; g.pad_sent = true; }
}

// Sommeil sur la prise du coeur (14/11) : l'appli se reveille des qu'une image arrive, au lieu de tourner toutes les
// 250 us (~0,46 coeur au S5 en r100, simpleperf). Rien ne s'accumule : poll() vide la prise a chaque tour.
bool wait_message(int timeout_ms) {
  if (g.fd < 0) return false;
  struct pollfd pf = {g.fd, POLLIN, 0};
  return ::poll(&pf, 1, timeout_ms) > 0;
}

bool frame_ready() {
  poll();
  return g.pending >= 0 && g.nslots > 0;
}

// Copie l'image recue dans le tampon lie (celui que l'appli presente) : rectangle (x, y, w, h).
bool frame_blit(int x, int y, int w, int h) {
  poll();
  if (g.pending < 0 || !import_buffers()) return false;
  const int slot = g.pending;
  if (g.pending_fence >= 0) {   // attendre (sur le GPU) que le coeur ait fini de dessiner l'image
    EGLDisplay dpy = eglGetCurrentDisplay();
    if (p_sync && p_wait && p_destroy) {
      const EGLint a[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, g.pending_fence, EGL_NONE};
      EGLSyncKHR s = p_sync(dpy, EGL_SYNC_NATIVE_FENCE_ANDROID, a);
      if (s != EGL_NO_SYNC_KHR) {
        g.pending_fence = -1;   // la barriere appartient maintenant a EGL
        p_wait(dpy, s, 0);
        p_destroy(dpy, s);
      }
    }
    if (g.pending_fence >= 0) { close(g.pending_fence); g.pending_fence = -1; }
  }
  GLint draw = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, g.fbo[slot]);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.tex[slot], 0);
  GLboolean cm[4];   // la copie obeit au ciseau et au masque de couleur : neutres le temps de la copie
  glGetBooleanv(GL_COLOR_WRITEMASK, cm);
  const GLboolean sc = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  static const bool use_blit = [] { const char *e = std::getenv("NJ_LINK_BLIT"); return e && e[0] == '1'; }();
  if (use_blit || !copy_draw(g.tex[slot], x, y, w, h))
    glBlitFramebuffer(0, 0, g.w, g.h, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glColorMask(cm[0], cm[1], cm[2], cm[3]);
  if (sc) glEnable(GL_SCISSOR_TEST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)draw);
  release_slot(g.shown);
  g.shown = slot;
  g.pending = -1;
  if (g.qn > 0) {   // l'image suivante de la file devient la tete
    g.pending = g.q[0];
    g.pending_fence = g.qf[0];
    for (int k = 0; k < 2; ++k) { g.q[k] = g.q[k + 1]; g.qf[k] = g.qf[k + 1]; }
    g.q[2] = -1;
    g.qf[2] = -1;
    --g.qn;
  }
  return true;
}

unsigned rumble() {
  std::lock_guard<std::mutex> lk(g.state_mu);
  return g.has_state ? (unsigned)(uint8_t)g.state.vib_state : 0;
}
bool xfb_size(unsigned &w, unsigned &h) {
  if (!g.xfb) return false;
  w = g.xfb >> 16;
  h = g.xfb & 0xFFFF;
  return true;
}
bool game_state(NjState &out) {
  poll();
  std::lock_guard<std::mutex> lk(g.state_mu);
  if (!g.has_state) return false;
  out = g.state;
  return true;
}
void title_command(int cmd, int slot) {
  NjMsg m{};
  m.type = NJ_MSG_TITLE;
  m.a = (uint32_t)cmd;
  m.b = (uint32_t)slot;
  send(m);
}
std::string saves_dir() { return g.saves; }
void setting(int id, int value) {
  NjMsg m{};
  m.type = NJ_MSG_SETTING;
  m.a = (uint32_t)id;
  m.b = (uint32_t)value;
  send(m);
}
void case_command(int op, int board, const void *data, unsigned size) {
  NjMsg m{};
  m.type = NJ_MSG_CASE;
  m.a = (uint32_t)op;
  m.b = (uint32_t)board;
  std::memcpy(m.data, data, size < sizeof m.data ? size : sizeof m.data);
  send(m);
}
int audio(int16_t *, int) { return 0; }   // son du coeur : phase N4
void vsync(int64_t) {}
void pause(bool on) {   // menu Nightjar ouvert : le jeu s'arrete au debut de l'image suivante (port/njlink.cpp)
  NjMsg m{};
  m.type = NJ_MSG_PAUSE;
  m.a = on ? 1u : 0u;
  send(m);
}
const RawChunk *gx_acquire() { return nullptr; }
void gx_release(const RawChunk *) {}
void gx_detach() {
  NjMsg m{};
  m.type = NJ_MSG_QUIT;
  send(m);
}
const CoreSnapshot *snapshot_acquire() { return nullptr; }   // contexte du tactile : a brancher
void snapshot_release(const CoreSnapshot *) {}
bool loaded() { return !g.exe.empty(); }

#elif defined(__APPLE__)   // iOS : le coeur natif dans un fil de ce processus

extern "C" {
int nj_core_main(int argc, char **argv);   // port/main_port.cpp
void nj_core_post(const NjMsg *m);         // port/njlink.cpp (transport en memoire)
int nj_core_take(NjMsg *out);
int nj_core_state(NjState *out);
int nj_core_audio_pull(short *out, int frames);   // port/audio_port.cpp
void nj_core_set_gl(int (*make_current)(void *), void *(*current)(void), void *ctx);   // port/njgfx/njgfx.cpp
}

namespace {

const int kMaxSlots = 4;

struct Link {
  std::string saves, err;
  State st = State::Idle;
  bool started = false;
  int nslots = 0, w = 0, h = 0;
  GLuint tex[kMaxSlots] = {}, fbo[kMaxSlots] = {};
  int pending = -1;
  int q[3] = {-1, -1, -1};
  int qn = 0;
  int shown = -1;
  Pad last_pad{};
  bool pad_sent = false;
  std::mutex state_mu;
  NjState state{};
  bool has_state = false;
  uint32_t xfb = 0;
} g;

// contexte GL du coeur : cree par SDL dans le groupe de partage de celui du moteur (textures des images partagees)
SDL_Window *g_win = nullptr;
SDL_GLContext g_core_ctx = nullptr;
// 8e essai iPhone (19/11) : SDL_GL_MakeCurrent(fenetre, contexte) sur UIKit ATTACHE la vue de ce contexte a la fenetre
// (SDL_uikitopengles.m : setSDLWindow) ; le coeur, qui prend son contexte a chaque image, volait l'ecran au moteur avec
// une vue que personne ne presente -> ecran noir, jeu vivant derriere. Le coeur passe donc directement par EAGL (contexte
// courant du fil, sans toucher aux vues) : +[EAGLContext setCurrentContext:] / +currentContext, par le runtime ObjC.
int core_make_current(void *c) {
  using Set = signed char (*)(id, SEL, id);
  return ((Set)objc_msgSend)((id)objc_getClass("EAGLContext"), sel_registerName("setCurrentContext:"), (id)c) ? 0 : -1;
}
void *core_current(void) {
  using Get = id (*)(id, SEL);
  return (void *)((Get)objc_msgSend)((id)objc_getClass("EAGLContext"), sel_registerName("currentContext"));
}

void send(const NjMsg &m) { nj_core_post(&m); }

void release_slot(int slot) {
  if (slot < 0) return;
  NjMsg m{};
  m.type = NJ_MSG_RELEASE;
  m.a = (uint32_t)slot;
  send(m);
}

void poll() {
  NjMsg m;
  while (nj_core_take(&m)) {
    switch (m.type) {
    case NJ_MSG_HELLO:
      if (m.a != NJ_LINK_VERSION) { g.err = "version du coeur differente de celle de l'appli"; g.st = State::Failed; }
      else g.st = State::Running;
      break;
    case NJ_MSG_BUFFERS:   // textures GL du coeur, ids dans data[] (meme groupe de partage)
      g.nslots = (int)(m.a < (uint32_t)kMaxSlots ? m.a : kMaxSlots);
      g.w = (int)m.b;
      g.h = (int)m.c;
      for (int i = 0; i < g.nslots; ++i) std::memcpy(&g.tex[i], m.data + 4 * i, 4);
      break;
    case NJ_MSG_FRAME:   // image finie (glFinish cote coeur) : file de 3 comme sur Android
      if (m.a < (uint32_t)g.nslots) {
        if (g.pending < 0) {
          g.pending = (int)m.a;
        } else {
          if (g.qn == 3) {
            release_slot(g.pending);
            g.pending = g.q[0];
            for (int k = 0; k < 2; ++k) g.q[k] = g.q[k + 1];
            g.qn = 2;
          }
          g.q[g.qn++] = (int)m.a;
        }
        if (m.c) g.xfb = m.c;
      }
      break;
    default:
      break;
    }
  }
  NjState st;
  if (nj_core_state(&st)) {
    std::lock_guard<std::mutex> lk(g.state_mu);
    g.state = st;
    g.has_state = true;
  }
}

}  // namespace

bool load(const std::string &, std::string &) { return true; }   // le coeur est lie dans l'appli

// data_dir : dossier du disque extrait (disc1/{sys,files}, disc2, memcard), dans les Documents de l'appli.
bool start(const std::string &data_dir, const std::string &, std::string &error) {
  if (g.started) return true;
  const std::string game = data_dir + "/disc1";
  nj::runtime::DiscView::get().set_folder(game + "/files");
  const std::string saves = data_dir + "/memcard";
  g.saves = saves;
  mkdir(saves.c_str(), 0700);
  const std::string storage = data_dir.substr(0, data_dir.find_last_of('/'));
  setenv("NJ_LINK_INPROC", "1", 1);
  setenv("RE4_SAVE_DIR", saves.c_str(), 1);
  setenv("NJ_TITLE_APP", nj::runtime::gfx::g_title_menu ? "1" : "0", 1);
  setenv("NJ_DEBUG_HUD", nj::runtime::gfx::g_debug_mode ? "1" : "0", 1);
  setenv("NJ_COSTUME", std::to_string(nj::runtime::gfx::g_leon_outfit).c_str(), 1);
  setenv("NJ_TITLE_SCENE", nj::runtime::gfx::g_title_menu && nj::runtime::gfx::g_title_scene ? "0x100" : "0", 1);
  setenv("NJ_STORAGE_DIR", storage.c_str(), 1);
  // pas de NJ_CORE_LOG : meme processus, le journal du coeur rejoint journal.txt du moteur (Documents)
  // contexte du coeur partage avec celui du moteur (cree sur le fil du moteur, puis rendu ; le coeur le prend sur son fil)
  g_win = SDL_GL_GetCurrentWindow();
  SDL_GLContext mine = SDL_GL_GetCurrentContext();
  if (!g_win || !mine) { error = "contexte GL du moteur absent"; return false; }
  // 9e essai (19/11) : PAS de SDL_GL_CreateContext : sur UIKit il cree aussi une vue et l'attache a la fenetre (ecran
  // noir). Contexte EAGL nu, sans vue, dans le groupe de partage du contexte du moteur (textures des images partagees) :
  // [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3 sharegroup:[EAGLContext currentContext].sharegroup]
  {
    using Get = id (*)(id, SEL);
    using Init = id (*)(id, SEL, unsigned long, id);
    id cls = (id)objc_getClass("EAGLContext");
    id cur = ((Get)objc_msgSend)(cls, sel_registerName("currentContext"));
    id sg = cur ? ((Get)objc_msgSend)(cur, sel_registerName("sharegroup")) : nullptr;
    id obj = ((Get)objc_msgSend)(cls, sel_registerName("alloc"));
    g_core_ctx = (SDL_GLContext)((Init)objc_msgSend)(obj, sel_registerName("initWithAPI:sharegroup:"), 3ul, sg);
    (void)mine;
    std::printf("[Natif] contexte GL du coeur : EAGL %p, partage %p\n", (void *)g_core_ctx, (void *)sg);
  }
  if (!g_core_ctx) { error = "contexte GL du coeur : EAGL refuse"; return false; }
  nj_core_set_gl(core_make_current, core_current, g_core_ctx);
  static std::string arg0 = "njcore", arg1;
  arg1 = game;
  std::thread([] {
    char *argv[3] = {&arg0[0], &arg1[0], nullptr};
    const int r = nj_core_main(2, argv);
    std::printf("[Natif] le coeur a rendu %d\n", r);
  }).detach();
  g.started = true;
  g.st = State::Idle;
  return true;
}

State state() {
  poll();
  return g.st;
}
std::string message() { return g.err; }

void pad(const Pad &p) {
  if (g.pad_sent && std::memcmp(&p, &g.last_pad, sizeof p) == 0) return;
  NjMsg m{};
  m.type = NJ_MSG_PAD;
  m.a = (uint32_t)p.accel_cg * 10u;
  NjPad np{p.buttons, p.stick_x, p.stick_y, p.substick_x, p.substick_y, p.trigger_l, p.trigger_r};
  std::memcpy(m.data, &np, sizeof np);
  send(m);
  g.last_pad = p;
  g.pad_sent = true;
}

bool wait_message(int timeout_ms) {   // pas de prise a surveiller : courte attente, poll() relit les files
  SDL_Delay((Uint32)(timeout_ms > 2 ? 2 : (timeout_ms < 0 ? 0 : timeout_ms)));
  return true;
}

bool frame_ready() {
  poll();
  return g.pending >= 0 && g.nslots > 0;
}

bool frame_blit(int x, int y, int w, int h) {
  poll();
  if (g.pending < 0) return false;
  const int slot = g.pending;
  if (!g.fbo[slot]) glGenFramebuffers(1, &g.fbo[slot]);
  GLint draw = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, g.fbo[slot]);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.tex[slot], 0);
  GLboolean cm[4];
  glGetBooleanv(GL_COLOR_WRITEMASK, cm);
  const GLboolean sc = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  if (!copy_draw(g.tex[slot], x, y, w, h))
    glBlitFramebuffer(0, 0, g.w, g.h, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glColorMask(cm[0], cm[1], cm[2], cm[3]);
  if (sc) glEnable(GL_SCISSOR_TEST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)draw);
  release_slot(g.shown);
  g.shown = slot;
  g.pending = -1;
  if (g.qn > 0) {
    g.pending = g.q[0];
    for (int k = 0; k < 2; ++k) g.q[k] = g.q[k + 1];
    g.q[2] = -1;
    --g.qn;
  }
  return true;
}

unsigned rumble() {
  std::lock_guard<std::mutex> lk(g.state_mu);
  return g.has_state ? (unsigned)(uint8_t)g.state.vib_state : 0;
}
bool xfb_size(unsigned &w, unsigned &h) {
  if (!g.xfb) return false;
  w = g.xfb >> 16;
  h = g.xfb & 0xFFFF;
  return true;
}
bool game_state(NjState &out) {
  poll();
  std::lock_guard<std::mutex> lk(g.state_mu);
  if (!g.has_state) return false;
  out = g.state;
  return true;
}
void title_command(int cmd, int slot) {
  NjMsg m{};
  m.type = NJ_MSG_TITLE;
  m.a = (uint32_t)cmd;
  m.b = (uint32_t)slot;
  send(m);
}
std::string saves_dir() { return g.saves; }
void setting(int id, int value) {
  NjMsg m{};
  m.type = NJ_MSG_SETTING;
  m.a = (uint32_t)id;
  m.b = (uint32_t)value;
  send(m);
}
void case_command(int op, int board, const void *data, unsigned size) {
  NjMsg m{};
  m.type = NJ_MSG_CASE;
  m.a = (uint32_t)op;
  m.b = (uint32_t)board;
  std::memcpy(m.data, data, size < sizeof m.data ? size : sizeof m.data);
  send(m);
}
int audio(int16_t *out, int frames) { return g.started ? nj_core_audio_pull(out, frames) : 0; }   // son du coeur, tire par le moteur
void vsync(int64_t) {}
void pause(bool on) {
  NjMsg m{};
  m.type = NJ_MSG_PAUSE;
  m.a = on ? 1u : 0u;
  send(m);
}
const RawChunk *gx_acquire() { return nullptr; }
void gx_release(const RawChunk *) {}
void gx_detach() {
  NjMsg m{};
  m.type = NJ_MSG_QUIT;
  send(m);
}
const CoreSnapshot *snapshot_acquire() { return nullptr; }
void snapshot_release(const CoreSnapshot *) {}
bool loaded() { return g.started; }

#else   // ni Android ni iOS : pas de coeur natif (PC : a venir)

bool load(const std::string &, std::string &error) { error = "moteur natif absent sur cette plate-forme"; return false; }
bool start(const std::string &, const std::string &, std::string &error) { error = "moteur natif absent"; return false; }
State state() { return State::Idle; }
std::string message() { return ""; }
void pad(const Pad &) {}
bool frame_ready() { return false; }
bool wait_message(int) { return false; }
bool frame_blit(int, int, int, int) { return false; }
unsigned rumble() { return 0; }
bool game_state(NjState &) { return false; }
bool xfb_size(unsigned &, unsigned &) { return false; }
void case_command(int, int, const void *, unsigned) {}
void title_command(int, int) {}
std::string saves_dir() { return ""; }
void setting(int, int) {}
int audio(int16_t *, int) { return 0; }
void vsync(int64_t) {}
void pause(bool) {}
const RawChunk *gx_acquire() { return nullptr; }
void gx_release(const RawChunk *) {}
void gx_detach() {}
const CoreSnapshot *snapshot_acquire() { return nullptr; }
void snapshot_release(const CoreSnapshot *) {}
bool loaded() { return false; }

#endif

}  // namespace nj::native
