#include "runtime/hw/io_bus.h"
#include "runtime/log.h"

#include <iostream>

namespace nj::runtime {

IoBus &IoBus::get() {
  static IoBus bus;
  return bus;
}

void IoBus::map(uint32_t first, uint32_t last, Read32 rd, Write32 wr, Read16 rd16, Write16 wr16) {
  // Pages entierement couvertes par ce bloc et par aucun bloc inscrit avant lui (le premier
  // inscrit l'emporte, comme dans la recherche lineaire) : acces direct.
  for (uint32_t p = 0; p < 64; ++p) {
    const uint32_t lo = 0xCC000000u + (p << 10), hi = lo + 0x3FF;
    if (!(first <= lo && hi <= last) || page_[p] || devices_.size() >= 254) continue;
    bool shared = false;
    for (const Device &d : devices_)
      shared |= d.first <= hi && d.last >= lo;
    if (!shared) page_[p] = (uint8_t)(devices_.size() + 1);
  }
  devices_.push_back({first, last, std::move(rd), std::move(wr), std::move(rd16), std::move(wr16)});
}

IoBus::Device *IoBus::find(uint32_t addr) {
  if ((addr & 0xFFFF0000u) == 0xCC000000u) {
    if (const uint8_t i = page_[(addr >> 10) & 63]) return &devices_[i - 1];
  }
  for (Device &d : devices_)
    if (addr >= d.first && addr <= d.last) return &d;
  return nullptr;
}

void IoBus::complain(const char *what, uint32_t addr, uint32_t value) {
  static int n = 0;
  if (n++ < 32)
    NJ_LOG << "[IO] " << what << " sans peripherique : 0x" << std::hex << addr << " (0x" << value << ")"
              << std::dec << std::endl;
}

uint32_t IoBus::read32(uint32_t addr) {
  if (Device *d = find(addr)) return d->rd ? d->rd(addr) : 0;
  complain("lecture 32", addr, 0);
  return 0;
}

void IoBus::write32(uint32_t addr, uint32_t value) {
  if (Device *d = find(addr)) {
    if (d->wr) d->wr(addr, value);
    return;
  }
  complain("ecriture 32", addr, value);
}

uint16_t IoBus::read16(uint32_t addr) {
  if (Device *d = find(addr)) {
    if (d->rd16) return d->rd16(addr);
    return d->rd ? (uint16_t)d->rd(addr) : 0;
  }
  complain("lecture 16", addr, 0);
  return 0;
}

void IoBus::write16(uint32_t addr, uint16_t value) {
  if (Device *d = find(addr)) {
    if (d->wr16) d->wr16(addr, value);
    else if (d->wr) d->wr(addr, value);
    return;
  }
  complain("ecriture 16", addr, value);
}

}  // namespace nj::runtime
