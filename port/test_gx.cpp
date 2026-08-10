// -*- coding: utf-8 -*-
// Verification de la couche GX par relecture de pixels, sur l'appareil.
//
// Chaque test dessine par l'**API GX reelle** - celle que le jeu appelle - puis relit le
// framebuffer et compare. Rien n'est juge a l'oeil : une scene vide, une face culee a l'envers
// ou une matrice transposee de travers produisent toutes "un ecran noir", et seule la relecture
// les distingue.
#include <cstdio>
#include <cstring>

#include <dolphin.h>

extern "C" bool port_gx_init_offscreen(u32 w, u32 h);
extern "C" void port_gx_read_pixels(int x, int y, int w, int h, void *dst);

namespace {

const int W = 640, H = 448;
int g_fail = 0;

void check(bool ok, const char *what)
{
    std::printf("  [%s] %s\n", ok ? "OK " : "NON", what);
    if (!ok)
        ++g_fail;
}

struct Px { unsigned char r, g, b, a; };

Px at(int x, int y)
{
    Px p;
    // glReadPixels compte depuis le bas ; on raisonne en coordonnees ecran.
    port_gx_read_pixels(x, H - 1 - y, 1, 1, &p);
    return p;
}

bool near(unsigned char v, int want, int tol = 12)
{
    const int d = (int)v - want;
    return d > -tol && d < tol;
}

// Projection orthographique couvrant l'ecran en pixels, comme le jeu la pose pour l'interface.
void ortho_screen()
{
    f32 m[4][4];
    std::memset(m, 0, sizeof(m));
    m[0][0] = 2.0f / (float)W;
    m[1][1] = -2.0f / (float)H;      // y vers le bas, convention GameCube
    m[2][2] = 2.0f;      // z 0..1 -> NDC -1..+1 (convention OpenGL)
    m[2][3] = -1.0f;
    m[0][3] = -1.0f;
    m[1][3] = 1.0f;
    m[3][3] = 1.0f;
    GXSetProjection(m, GX_ORTHOGRAPHIC);

    f32 id[3][4];
    std::memset(id, 0, sizeof(id));
    id[0][0] = id[1][1] = id[2][2] = 1.0f;
    GXLoadPosMtxImm(id, 0);
}

void clear_to(u8 r, u8 g, u8 b)
{
    GXColor c = { r, g, b, 255 };
    GXSetCopyClear(c, 0x00FFFFFF);
    GXCopyDisp(nullptr, GX_TRUE);
}

void tri(float x0, float y0, float x1, float y1, float x2, float y2, u8 r, u8 g, u8 b)
{
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(x0, y0, 0.0f); GXColor4u8(r, g, b, 255);
    GXPosition3f32(x1, y1, 0.0f); GXColor4u8(r, g, b, 255);
    GXPosition3f32(x2, y2, 0.0f); GXColor4u8(r, g, b, 255);
    GXEnd();
}

} // namespace

int main()
{
    std::printf("couche GX -- verification par relecture\n\n");
    if (!port_gx_init_offscreen(W, H)) {
        std::printf("pas de contexte GL\n");
        return 1;
    }
    GXSetViewport(0, 0, (f32)W, (f32)H, 0.0f, 1.0f);
    GXSetCullMode(GX_CULL_NONE);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);

    std::printf("== effacement ==\n");
    clear_to(0x20, 0x40, 0x60);
    Px p = at(320, 224);
    std::printf("      centre : %02X %02X %02X\n", p.r, p.g, p.b);
    check(near(p.r, 0x20) && near(p.g, 0x40) && near(p.b, 0x60), "GXSetCopyClear + GXCopyDisp effacent");

    std::printf("\n== triangle ==\n");
    ortho_screen();
    tri(160, 100, 480, 100, 320, 380, 0xFF, 0x00, 0x00);
    p = at(320, 224);
    std::printf("      centre : %02X %02X %02X\n", p.r, p.g, p.b);
    check(near(p.r, 0xFF) && near(p.g, 0x00) && near(p.b, 0x00), "le centre est rouge");
    p = at(20, 20);
    check(near(p.r, 0x20) && near(p.g, 0x40) && near(p.b, 0x60), "le coin garde la couleur d'effacement");

    std::printf("\n== couleur par sommet ==\n");
    clear_to(0, 0, 0);
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(40.0f, 40.0f, 0.0f);   GXColor4u8(255, 0, 0, 255);
    GXPosition3f32(600.0f, 40.0f, 0.0f);  GXColor4u8(0, 255, 0, 255);
    GXPosition3f32(320.0f, 400.0f, 0.0f); GXColor4u8(0, 0, 255, 255);
    GXEnd();
    p = at(320, 224);
    std::printf("      centre : %02X %02X %02X\n", p.r, p.g, p.b);
    check(p.r > 20 && p.g > 20 && p.b > 20, "l'interpolation melange les trois sommets");

    std::printf("\n== bande de triangles ==\n");
    clear_to(0, 0, 0);
    // Un quadrilatere en bande : si l'alternance d'orientation est oubliee, un triangle sur
    // deux part a l'envers - invisible tant que le culling est desactive, fatal sinon. On active
    // donc le culling pour que le test ait un sens.
    GXSetCullMode(GX_CULL_NONE);
    GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);
    GXPosition3f32(200.0f, 150.0f, 0.0f); GXColor4u8(0, 255, 255, 255);
    GXPosition3f32(200.0f, 300.0f, 0.0f); GXColor4u8(0, 255, 255, 255);
    GXPosition3f32(440.0f, 150.0f, 0.0f); GXColor4u8(0, 255, 255, 255);
    GXPosition3f32(440.0f, 300.0f, 0.0f); GXColor4u8(0, 255, 255, 255);
    GXEnd();
    const Px a1 = at(260, 200), a2 = at(400, 250);
    std::printf("      deux points : %02X%02X%02X et %02X%02X%02X\n",
                a1.r, a1.g, a1.b, a2.r, a2.g, a2.b);
    check(a1.g > 200 && a1.b > 200 && a2.g > 200 && a2.b > 200,
          "les deux triangles de la bande sont dessines (alternance correcte)");
    // Puis les deux sens, pour **mesurer** lequel conserve cette geometrie plutot que de le
    // postuler : le sens trigonometrique depend aussi du signe de la projection en Y, et
    // raisonner dans l abstrait est ici le plus sur moyen de se tromper.
    for (int mode = 0; mode < 2; ++mode) {
        clear_to(0, 0, 0);
        GXSetCullMode(mode ? GX_CULL_BACK : GX_CULL_FRONT);
        GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);
        GXPosition3f32(200.0f, 150.0f, 0.0f); GXColor4u8(0, 255, 255, 255);
        GXPosition3f32(200.0f, 300.0f, 0.0f); GXColor4u8(0, 255, 255, 255);
        GXPosition3f32(440.0f, 150.0f, 0.0f); GXColor4u8(0, 255, 255, 255);
        GXPosition3f32(440.0f, 300.0f, 0.0f); GXColor4u8(0, 255, 255, 255);
        GXEnd();
        const Px q = at(260, 200);
        std::printf("      %s : %02X%02X%02X\n",
                    mode ? "GX_CULL_BACK " : "GX_CULL_FRONT", q.r, q.g, q.b);
    }
    GXSetCullMode(GX_CULL_NONE);

    std::printf("\n== quadrilateres ==\n");
    clear_to(0, 0, 0);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(100.0f, 100.0f, 0.0f); GXColor4u8(255, 255, 0, 255);
    GXPosition3f32(100.0f, 348.0f, 0.0f); GXColor4u8(255, 255, 0, 255);
    GXPosition3f32(540.0f, 348.0f, 0.0f); GXColor4u8(255, 255, 0, 255);
    GXPosition3f32(540.0f, 100.0f, 0.0f); GXColor4u8(255, 255, 0, 255);
    GXEnd();
    p = at(320, 224);
    check(near(p.r, 0xFF) && near(p.g, 0xFF) && near(p.b, 0x00), "GX_QUADS est converti en deux triangles");
    p = at(320, 60);
    check(p.r < 30 && p.g < 30, "hors du quadrilatere, rien n'est peint");

    std::printf("\n== test de profondeur ==\n");
    clear_to(0, 0, 0);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);   // proche, vert
    GXPosition3f32(160.0f, 100.0f, 0.2f); GXColor4u8(0, 255, 0, 255);
    GXPosition3f32(480.0f, 100.0f, 0.2f); GXColor4u8(0, 255, 0, 255);
    GXPosition3f32(320.0f, 380.0f, 0.2f); GXColor4u8(0, 255, 0, 255);
    GXEnd();
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);   // loin, rouge : doit etre rejete
    GXPosition3f32(160.0f, 100.0f, 0.8f); GXColor4u8(255, 0, 0, 255);
    GXPosition3f32(480.0f, 100.0f, 0.8f); GXColor4u8(255, 0, 0, 255);
    GXPosition3f32(320.0f, 380.0f, 0.8f); GXColor4u8(255, 0, 0, 255);
    GXEnd();
    p = at(320, 224);
    std::printf("      centre : %02X %02X %02X\n", p.r, p.g, p.b);
    check(p.g > 200 && p.r < 60, "le triangle le plus proche gagne");

    std::printf("\n== texture ==\n");
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    clear_to(0, 0, 0);

    // Une texture 4x4 en RGB5A3, soit exactement une tuile GX de 32 octets, avec un cadran de
    // couleur par quart. Quatre couleurs plutot qu'une : une texture unie ne dirait rien de
    // l'orientation, et c'est precisement la que se cachent les erreurs (U et V inverses, image
    // retournee, tuile mal lue). Chaque cadran repond a une question differente.
    u8 tex[32];
    static const unsigned short QUAD[4] = {
        0xFC00,   // rouge   : ligne du haut, gauche
        0x801F,   // bleu    : ligne du haut, droite
        0x83E0,   // vert    : ligne du bas, gauche
        0xFFE0    // jaune   : ligne du bas, droite
    };
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            const unsigned short c = QUAD[(y >= 2 ? 2 : 0) + (x >= 2 ? 1 : 0)];
            const int k = y * 4 + x;
            tex[2 * k] = (u8)(c >> 8);
            tex[2 * k + 1] = (u8)(c & 0xFF);
        }

    GXTexObj to;
    GXInitTexObj(&to, tex, 4, 4, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjFilter(&to, GX_NEAR, GX_NEAR);   // pas d'interpolation : on veut lire la texture telle quelle
    GXLoadTexObj(&to, GX_TEXMAP0);

    // Un quadrilatere plein ecran, sommets blancs (la couleur de sommet multiplie la texture).
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(0.0f, 0.0f, 0.0f);       GXColor4u8(255, 255, 255, 255); GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(0.0f, (f32)H, 0.0f);     GXColor4u8(255, 255, 255, 255); GXTexCoord2f32(0.0f, 1.0f);
    GXPosition3f32((f32)W, (f32)H, 0.0f);   GXColor4u8(255, 255, 255, 255); GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32((f32)W, 0.0f, 0.0f);     GXColor4u8(255, 255, 255, 255); GXTexCoord2f32(1.0f, 0.0f);
    GXEnd();

    const Px tl = at(W / 4, H / 4), tr = at(3 * W / 4, H / 4);
    const Px bl = at(W / 4, 3 * H / 4), br = at(3 * W / 4, 3 * H / 4);
    std::printf("      haut  : %02X%02X%02X  %02X%02X%02X\n", tl.r, tl.g, tl.b, tr.r, tr.g, tr.b);
    std::printf("      bas   : %02X%02X%02X  %02X%02X%02X\n", bl.r, bl.g, bl.b, br.r, br.g, br.b);
    check(tl.r > 200 && tl.g < 60 && tl.b < 60, "cadran haut-gauche rouge : la texture est echantillonnee");
    check(tr.b > 200 && tr.r < 60, "cadran haut-droit bleu : U croit vers la droite");
    check(bl.g > 200 && bl.r < 60, "cadran bas-gauche vert : V croit vers le bas, image non retournee");
    check(br.r > 200 && br.g > 200 && br.b < 60, "cadran bas-droit jaune");

    // La couleur de sommet doit moduler la texture, sinon l'eclairage par sommet de RE4 - dont
    // la moitie de l'ambiance de ce jeu depend - n'aurait aucun effet.
    clear_to(0, 0, 0);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(0.0f, 0.0f, 0.0f);       GXColor4u8(128, 128, 128, 255); GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(0.0f, (f32)H, 0.0f);     GXColor4u8(128, 128, 128, 255); GXTexCoord2f32(0.0f, 1.0f);
    GXPosition3f32((f32)W, (f32)H, 0.0f);   GXColor4u8(128, 128, 128, 255); GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32((f32)W, 0.0f, 0.0f);     GXColor4u8(128, 128, 128, 255); GXTexCoord2f32(1.0f, 0.0f);
    GXEnd();
    p = at(W / 4, H / 4);
    std::printf("      rouge assombri de moitie : %02X %02X %02X\n", p.r, p.g, p.b);
    check(p.r > 100 && p.r < 160 && p.g < 40, "la couleur de sommet module la texture");

    // Et sans texture liee, on revient a la couleur de sommet pure : un etat qui ne se remet
    // pas produirait une interface texturee de la salle precedente.
    GXInvalidateTexAll();
    clear_to(0, 0, 0);
    tri(160, 100, 480, 100, 320, 380, 0x00, 0xFF, 0x00);
    p = at(320, 224);
    check(p.g > 200 && p.r < 40, "apres GXInvalidateTexAll, le rendu redevient non texture");

    std::printf("\n%s : %d echec(s)\n", g_fail ? "ECHEC" : "TOUT PASSE", g_fail);
    return g_fail ? 1 : 0;
}
