// harness : applique notre conversion (port/endian.cpp) a un fichier du disque GameCube, sur PC.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cstdint>
extern "C" void port_endian_fix(const char *name, void *data, unsigned size);
extern "C" void port_room_arc_fix(void *arc, unsigned size);
extern "C" int Fcc_next_axis_addr(int type, int n) {   // copie de src/game/motion.cpp
  static const int k[13] = {12, 8, 6, -1, 10, 6, 4, -1, 9, 5, 3, -1, -1};
  return (type >= 0 && type < 13 && k[type] >= 0) ? n * k[type] : -1; }
extern "C" const void *port_rel_prepare(void *h) { return h; }
static uint32_t be(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
int main(int c, char **v) {
  FILE *f = fopen(v[2], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> d(n); fread(d.data(), 1, n, f); fclose(f);
  size_t off = 0, sz = n;
  if (be(d.data()) == 0xcab6be20u) { sz = be(&d[0x24]); off = be(&d[0x2c]); }
  if (!strcmp(v[1], "room")) port_room_arc_fix(d.data() + off, (unsigned)sz); else port_endian_fix(v[1], d.data() + off, (unsigned)sz);
  f = fopen(v[3], "wb"); fwrite(d.data() + off, 1, sz, f); fclose(f);
  return 0;
}
void port_todo(const char *s) { __builtin_printf("[todo] %s\n", s); }
