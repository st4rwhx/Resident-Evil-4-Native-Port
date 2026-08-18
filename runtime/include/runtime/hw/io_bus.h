#pragma once
// Registres materiels de la GameCube (0xCC00xxxx : CP, PE, VI, PI, MI, DSP, DI, SI, EXI, AI) :
// chaque bloc s'inscrit pour sa plage d'adresses avec ses fonctions de lecture et d'ecriture.
// Les acces 16 bits ont leurs propres fonctions quand le bloc en a besoin (DSP, VI) ; sinon ils
// passent par celles de 32 bits.
#include <cstdint>
#include <functional>
#include <vector>

namespace nj::runtime {

class IoBus {
 public:
  using Read32 = std::function<uint32_t(uint32_t addr)>;
  using Write32 = std::function<void(uint32_t addr, uint32_t value)>;
  using Read16 = std::function<uint16_t(uint32_t addr)>;
  using Write16 = std::function<void(uint32_t addr, uint16_t value)>;

  static IoBus &get();

  // Plage [first, last] (bornes comprises).
  void map(uint32_t first, uint32_t last, Read32 rd, Write32 wr, Read16 rd16 = nullptr, Write16 wr16 = nullptr);
  uint32_t read32(uint32_t a);
  void write32(uint32_t addr, uint32_t value);
  uint16_t read16(uint32_t a);
  void write16(uint32_t addr, uint16_t value);

 private:
  struct Device {
    uint32_t first, last;
    Read32 rd;
    Write32 wr;
    Read16 rd16;
    Write16 wr16;
  };
  std::vector<Device> devices_;
  // 0xCC000000-0xCC00FFFF par pages de 1 Ko : indice du bloc + 1 (0 : chercher dans la liste).
  uint8_t page_[64] = {};
  Device *find(uint32_t addr);
  void complain(const char *what, uint32_t addr, uint32_t value);
};

}  // namespace nj::runtime
