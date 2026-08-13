// disc_ls <image> [sortie_dir fichier...] : liste la FST (chemin taille sha-court) ; extrait les fichiers nommes.
#include "runtime/disc_image.h"
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
using namespace nj::runtime;
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
int main(int c, char **v) {
  auto d = DiscImage::open(v[1]);
  if (!d) { std::printf("illisible\n"); return 1; }
  uint8_t h[0x440]; d->read(0, h, sizeof h);
  uint32_t fo = be32(h + 0x424), fs = be32(h + 0x428);
  std::vector<uint8_t> fst(fs); d->read(fo, fst.data(), fs);
  walk_fst(fst, [&](const FstEntry &e) {
    if (e.dir) return;
    uint64_t hsh = 1469598103934665603ull;
    std::vector<uint8_t> b(e.size); d->read(e.offset, b.data(), e.size);
    for (uint8_t x : b) hsh = (hsh ^ x) * 1099511628211ull;
    std::printf("%s\t%u\t%016llx\n", e.lower.c_str(), e.size, (unsigned long long)hsh);
    for (int i = 3; i < c; ++i) if (e.lower == v[i]) {
      std::string o = std::string(v[2]) + "/" + std::to_string(i) + "_" + std::string(strrchr(v[i], '/') ? strrchr(v[i], '/') + 1 : v[i]);
      FILE *f = fopen(o.c_str(), "wb"); fwrite(b.data(), 1, b.size(), f); fclose(f);
    }
  });
  return 0;
}
