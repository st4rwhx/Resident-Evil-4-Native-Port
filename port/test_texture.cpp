// -*- coding: utf-8 -*-
// Verification du decodeur de textures, sur l'appareil, **contre un oracle**.
//
// Chaque cas porte des octets bruts pris dans une vraie texture de RE4 (ou du bruit
// reproductible pour les formats que le jeu n'emploie pas) et le RGBA que le decodeur Python
// de reference en tire. Le test ne juge pas "l'image a l'air bonne" : il exige l'egalite
// octet a octet. Un pixel de travers dans un coin suffit a faire echouer, ce qui est le seul
// niveau d'exigence utile ici - une tuile mal dimensionnee produit une image parfaitement
// plausible et fausse.
//
//   python port/make_tex_cases.py <dossier TPL> tex_cases.bin
//   adb push tex_cases.bin test_texture /data/local/tmp/re4/
//   adb shell /data/local/tmp/re4/test_texture /data/local/tmp/re4/tex_cases.bin
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dolphin.h>

extern "C" void port_tex_decode(int fmt, const void *data, int w, int h,
                                const void *tlut_rgba, int tlut_count, void *out);

namespace {

const char *FMT_NAME[] = {
    "I4", "I8", "IA4", "IA8", "RGB565", "RGB5A3", "RGBA8", "?",
    "C4", "C8", "C14X2", "?", "?", "?", "CMPR"
};

unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

} // namespace

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/data/local/tmp/re4/tex_cases.bin";

    std::FILE *f = std::fopen(path, "rb");
    if (!f) {
        std::printf("cas introuvables : %s\n", path);
        return 1;
    }
    std::fseek(f, 0, SEEK_END);
    const long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    unsigned char *blob = (unsigned char *)std::malloc((size_t)len);
    if (!blob || std::fread(blob, 1, (size_t)len, f) != (size_t)len) {
        std::printf("lecture incomplete\n");
        return 1;
    }
    std::fclose(f);

    if (std::memcmp(blob, "TXCS", 4) != 0) {
        std::printf("ce fichier n'est pas un jeu de cas\n");
        return 1;
    }
    const unsigned int ncase = rd32(blob + 4);
    std::printf("decodeur de textures -- %u cas, juges contre le decodeur de reference\n\n",
                ncase);

    const unsigned char *p = blob + 8;
    int fail = 0;
    for (unsigned int i = 0; i < ncase; ++i) {
        const unsigned int fmt = rd32(p), w = rd32(p + 4), h = rd32(p + 8);
        const unsigned int nraw = rd32(p + 12), npal = rd32(p + 16);
        p += 20;
        const unsigned char *raw = p;              p += nraw;
        const unsigned char *pal = p;              p += npal * 4;
        const unsigned char *want = p;             p += (size_t)w * h * 4;

        unsigned char *got = (unsigned char *)std::malloc((size_t)w * h * 4);
        port_tex_decode((int)fmt, raw, (int)w, (int)h, npal ? pal : nullptr, (int)npal, got);

        // Compter les pixels differents plutot que s'arreter au premier : "1 pixel sur 4096"
        // et "tous" sont deux bugs tres differents, et le nombre oriente tout de suite - un
        // ecart sur la moitie droite crie "tuile mal dimensionnee".
        unsigned int bad = 0, first = 0;
        for (unsigned int k = 0; k < w * h; ++k)
            if (std::memcmp(got + 4 * k, want + 4 * k, 4) != 0) {
                if (!bad)
                    first = k;
                ++bad;
            }

        const char *name = (fmt < 15) ? FMT_NAME[fmt] : "?";
        if (bad == 0) {
            std::printf("  [OK ] %-7s %3ux%-3u  identique au pixel pres\n", name, w, h);
        } else {
            ++fail;
            std::printf("  [NON] %-7s %3ux%-3u  %u/%u pixels differents, "
                        "premier en (%u,%u) : %02X%02X%02X%02X au lieu de %02X%02X%02X%02X\n",
                        name, w, h, bad, w * h, first % w, first / w,
                        got[4 * first], got[4 * first + 1], got[4 * first + 2], got[4 * first + 3],
                        want[4 * first], want[4 * first + 1], want[4 * first + 2],
                        want[4 * first + 3]);
        }
        std::free(got);
    }

    std::printf("\n%s : %d echec(s) sur %u\n", fail ? "ECHEC" : "TOUT PASSE", fail, ncase);
    std::free(blob);
    return fail ? 1 : 0;
}
