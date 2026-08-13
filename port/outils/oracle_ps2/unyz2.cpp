// unyz2 <entree.das> <sortie> : decompresse une archive de salle (YZ2) avec le decodeur du jeu, sur PC 64 bits.
// Le decodeur range des adresses en u32 : toute la memoire de travail est prise sous 4 Go (VirtualAlloc a adresse basse).
#include <windows.h>
#include <cstdio>
#include <cstdint>
extern "C" unsigned Yz2DecodeSet(char *str, void *buf);
extern "C" void Yz2DecodeExec(void *dst);
void port_todo(const char *s) { std::printf("[todo] %s\n", s); }
int main(int c, char **v) {
  FILE *f = fopen(v[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *base = nullptr;
  for (uintptr_t a = 0x10000000; a < 0x80000000 && !base; a += 0x1000000)
    base = (uint8_t *)VirtualAlloc((void *)a, 0x8000000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!base || (uintptr_t)base > 0xF0000000u) { std::printf("pas de memoire basse\n"); return 1; }
  uint8_t *in = base, *work = base + 0x2000000, *out = base + 0x4000000;
  fread(in, 1, n, f); fclose(f);
  // Le flux est lu derriere une chaine d'en-tete ; c'est elle que Yz2DecodeSet analyse.
  unsigned sz = Yz2DecodeSet((char *)in, work);
  std::printf("taille decompressee %u (0x%x)\n", sz, sz);
  Yz2DecodeExec(out);
  f = fopen(v[2], "wb"); fwrite(out, 1, sz, f); fclose(f);
  return 0;
}
