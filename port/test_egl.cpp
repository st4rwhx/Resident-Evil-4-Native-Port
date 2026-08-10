// -*- coding: utf-8 -*-
// Valide l'hypothese la plus risquee du portage graphique : obtenir un contexte OpenGL ES
// **depuis un binaire console**, sans Activity ni fenetre.
//
// Si cela marche, toute la couche GX peut etre developpee et testee sur l'appareil en rendant
// hors ecran puis en relisant les pixels - c'est-a-dire avec une **verification automatique**,
// pas en regardant un telephone. C'est la difference entre iterer en secondes et iterer en
// minutes, sur les 100 fonctions les plus delicates du portage.
//
// Si cela ne marche pas, il faut une Activity Android avant la premiere ligne de GX, et l'ordre
// des phases change.
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <cstdio>
#include <cstring>

static int g_fail = 0;

static void check(bool ok, const char *what)
{
    std::printf("  [%s] %s\n", ok ? "OK " : "NON", what);
    if (!ok)
        ++g_fail;
}

int main()
{
    std::printf("contexte GLES hors ecran\n\n");

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    check(dpy != EGL_NO_DISPLAY, "un affichage EGL est disponible");
    if (dpy == EGL_NO_DISPLAY)
        return 1;

    EGLint major = 0, minor = 0;
    check(eglInitialize(dpy, &major, &minor) == EGL_TRUE, "EGL s'initialise");
    std::printf("      EGL %d.%d\n", major, minor);

    // EGL_PBUFFER_BIT : une surface hors ecran. C'est ce qui evite d'avoir besoin d'une fenetre.
    const EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint ncfg = 0;
    check(eglChooseConfig(dpy, cfg_attr, &cfg, 1, &ncfg) == EGL_TRUE && ncfg > 0,
          "une configuration ES 3 avec pbuffer existe");
    if (!ncfg)
        return 1;

    // 640x448 : la taille de l'EFB du GameCube, celle que le portage rendra pour de vrai.
    const EGLint surf_attr[] = { EGL_WIDTH, 640, EGL_HEIGHT, 448, EGL_NONE };
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, surf_attr);
    check(surf != EGL_NO_SURFACE, "la surface hors ecran 640x448 se cree");

    const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    check(ctx != EGL_NO_CONTEXT, "le contexte ES 3 se cree");
    check(eglMakeCurrent(dpy, surf, surf, ctx) == EGL_TRUE, "le contexte devient courant");

    std::printf("      GL_VERSION  : %s\n", (const char *)glGetString(GL_VERSION));
    std::printf("      GL_RENDERER : %s\n", (const char *)glGetString(GL_RENDERER));

    // Effacer avec une couleur non triviale puis relire : c'est le test qui prouve que le GPU
    // a reellement travaille, et c'est exactement le mecanisme qui servira a verifier
    // automatiquement chaque etape de la couche GX.
    glViewport(0, 0, 640, 448);
    glClearColor(0.25f, 0.50f, 0.75f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFinish();

    unsigned char px[4] = {0, 0, 0, 0};
    glReadPixels(320, 224, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    std::printf("      pixel relu  : %02X %02X %02X %02X (attendu ~40 80 BF FF)\n",
                px[0], px[1], px[2], px[3]);
    check(px[0] > 0x38 && px[0] < 0x48 && px[1] > 0x78 && px[1] < 0x88 &&
          px[2] > 0xB7 && px[2] < 0xC7,
          "le pixel relu correspond a la couleur demandee");

    // Un shader minimal : la couche GX generera les siens depuis l'etat TEV, donc il faut
    // savoir compiler et lier ici.
    const char *vs_src = "#version 300 es\n"
                         "void main(){ gl_Position = vec4(0.0,0.0,0.0,1.0); gl_PointSize = 8.0; }\n";
    const char *fs_src = "#version 300 es\n"
                         "precision highp float;\n"
                         "out vec4 o;\n"
                         "void main(){ o = vec4(1.0,0.0,0.0,1.0); }\n";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, nullptr);
    glCompileShader(vs);
    GLint ok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    check(ok == GL_TRUE, "un vertex shader GLSL ES 300 compile");

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    check(ok == GL_TRUE, "un fragment shader GLSL ES 300 compile");

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    check(ok == GL_TRUE, "le programme se lie");

    // Dessiner un point au centre et verifier qu'il est rouge : la chaine complete
    // etat -> shader -> rasterisation -> relecture, celle dont GX aura besoin.
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glDrawArrays(GL_POINTS, 0, 1);
    glFinish();
    glReadPixels(320, 224, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    std::printf("      apres dessin : %02X %02X %02X %02X (attendu FF 00 00 FF)\n",
                px[0], px[1], px[2], px[3]);
    check(px[0] > 0xF0 && px[1] < 0x10 && px[2] < 0x10, "le point dessine est bien rouge");

    check(glGetError() == GL_NO_ERROR, "aucune erreur GL en suspens");

    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, surf);
    eglTerminate(dpy);

    std::printf("\n%s : %d echec(s)\n", g_fail ? "ECHEC" : "TOUT PASSE", g_fail);
    return g_fail ? 1 : 0;
}
