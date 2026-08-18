#include "boot/program.h"
#include "runtime/log.h"
#include "runtime/disc_image.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace nj::boot {

namespace {
uint32_t be32_at(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

bool read_range(std::ifstream &f, uint64_t off, size_t n, std::vector<uint8_t> &out) {
  out.resize(n);
  f.seekg((std::streamoff)off);
  return n == 0 || (bool)f.read(reinterpret_cast<char *>(out.data()), (std::streamsize)n);
}

// Taille d'un DOL d'apres son en-tete (fin du segment le plus loin dans le fichier).
size_t dol_extent(const uint8_t *h) {
  size_t end = 0x100;
  for (int i = 0; i < 18; ++i) {
    const uint32_t off = be32_at(h + 4 * i), len = be32_at(h + 0x90 + 4 * i);
    if (off && len && off + (size_t)len > end) end = off + (size_t)len;
  }
  return end;
}
}  // namespace

bool parse_dol(const uint8_t *dol, size_t size, Program &out) {
  // En-tete de 0x100 octets : 7 segments de code puis 11 de donnees, chacun decrit par trois
  // tableaux paralleles (position dans le fichier 0x00, adresse 0x48, longueur 0x90) ; puis
  // la zone a zero (adresse 0xD8, longueur 0xDC) et l'entree (0xE0).
  if (size < 0x100) return false;
  out = {};
  for (int i = 0; i < 18; ++i) {
    const uint32_t off = be32_at(dol + 4 * i), addr = be32_at(dol + 0x48 + 4 * i), len = be32_at(dol + 0x90 + 4 * i);
    if (!off || !len) continue;
    if ((size_t)off + len > size) return false;
    Segment s;
    s.addr = addr;
    s.size = len;
    s.code = i < 7;
    s.bytes.assign(dol + off, dol + off + len);
    out.segments.push_back(std::move(s));
  }
  if (const uint32_t zlen = be32_at(dol + 0xDC)) {
    Segment z;
    z.addr = be32_at(dol + 0xD8);
    z.size = zlen;
    z.zero = true;
    out.segments.push_back(std::move(z));
  }
  out.entry = be32_at(dol + 0xE0);
  return !out.segments.empty();
}

bool load_program(const std::string &source, Program &out, std::string &game_id) {
  namespace fs = ::std::filesystem;
  std::vector<uint8_t> head, dol;
  if (fs::is_directory(fs::u8path(source))) {
    // disque extrait : sys/main.dol (ou main.dol a la racine), sys/boot.bin pour l'identifiant
    fs::path root = fs::u8path(source), sys = root / "sys";
    if (!fs::exists(sys / "main.dol")) sys = root;
    std::ifstream d(sys / "main.dol", std::ios::binary), b(sys / "boot.bin", std::ios::binary);
    if (!d) {
      NJ_ERR << "[Boot] main.dol introuvable" << std::endl;
      return false;
    }
    dol.assign(std::istreambuf_iterator<char>(d), {});
    if (b && read_range(b, 0, 6, head)) game_id.assign(head.begin(), head.end());
  } else {
    // image disque : identifiant a 0, position du DOL a 0x420
    // image disque (.iso, .gcm, .rvz) : identifiant a 0, position du DOL a 0x420
    auto img = nj::runtime::DiscImage::open(source);
    head.resize(0x440);
    if (!img || !img->read(0, head.data(), head.size())) {
      NJ_ERR << "[Boot] image disque illisible" << std::endl;
      return false;
    }
    game_id.assign(head.begin(), head.begin() + 6);
    const uint32_t dol_off = be32_at(&head[0x420]);
    std::vector<uint8_t> dh(0x100);
    if (!dol_off || !img->read(dol_off, dh.data(), dh.size())) return false;
    dol.resize(dol_extent(dh.data()));
    if (dol.size() > (16u << 20) || !img->read(dol_off, dol.data(), dol.size())) return false;
  }
  if (!parse_dol(dol.data(), dol.size(), out)) return false;
  out.file = std::move(dol);
  return true;
}

}  // namespace nj::boot
