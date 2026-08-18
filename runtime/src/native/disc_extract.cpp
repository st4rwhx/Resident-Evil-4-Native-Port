// Extraction d'un disque (ISO, RVZ, dossier) vers la disposition que lit le noyau natif :
//   <dest>/disc0/sys/{boot.bin,bi2.bin,apploader.img,main.dol,fst.bin}
//   <dest>/disc0/files/<fichiers de la FST>
// Les octets viennent du disque du joueur ; rien n'est livre avec l'application.
#include "runtime/native/disc_extract.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "runtime/disc_image.h"

namespace fs = std::filesystem;

namespace nj::native {
namespace {

uint32_t be32(const uint8_t *p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

bool copy_span(runtime::DiscImage &img, uint64_t off, uint64_t size, const fs::path &to) {
  std::error_code ec;
  fs::create_directories(to.parent_path(), ec);
  const fs::path tmp = to.string() + ".part";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    std::vector<uint8_t> buf(size_t(4) << 20);
    uint64_t left = size;
    while (left) {
      const size_t n = size_t(std::min<uint64_t>(left, buf.size()));
      if (!img.read(off + (size - left), buf.data(), n)) return false;
      out.write(reinterpret_cast<const char *>(buf.data()), (std::streamsize)n);
      left -= n;
    }
    if (!out) return false;
  }
  fs::rename(tmp, to, ec);
  return !ec;
}

// Taille d'un executable DOL d'apres son en-tete (7 sections de code, 11 de donnees).
uint64_t dol_size(runtime::DiscImage &img, uint64_t off) {
  uint8_t h[0x100];
  if (!img.read(off, h, sizeof h)) return 0;
  uint64_t end = 0x100;
  for (int i = 0; i < 18; ++i) {
    const uint32_t o = be32(h + i * 4), s = be32(h + 0x90 + i * 4);
    if (o && s) end = std::max<uint64_t>(end, uint64_t(o) + s);
  }
  return end;
}

}  // namespace

bool extract_disc(const std::string &source, const std::string &dest, int disc_number,
                  const std::function<void(int)> &progress, std::string &error) {
  auto img = runtime::DiscImage::open(source);
  if (!img) { error = "disque illisible"; return false; }
  uint8_t boot[0x440];
  if (!img->read(0, boot, sizeof boot)) { error = "en-tete illisible"; return false; }
  if (std::string(reinterpret_cast<char *>(boot), 4) != "G4BE") { error = "ce n'est pas Resident Evil 4 (GameCube, USA)"; return false; }
  const fs::path root = fs::u8path(dest) / ("disc" + std::to_string(disc_number));
  const fs::path sys = root / "sys", files = root / "files";
  const uint64_t dol_off = be32(boot + 0x420), fst_off = be32(boot + 0x424), fst_size = be32(boot + 0x428);
  uint8_t apl[0x20];
  if (!img->read(0x2440, apl, sizeof apl)) { error = "apploader illisible"; return false; }
  const uint64_t apl_size = 0x20 + be32(apl + 0x14) + be32(apl + 0x18);
  if (!copy_span(*img, 0, 0x440, sys / "boot.bin") || !copy_span(*img, 0x440, 0x2000, sys / "bi2.bin") ||
      !copy_span(*img, 0x2440, apl_size, sys / "apploader.img") ||
      !copy_span(*img, dol_off, dol_size(*img, dol_off), sys / "main.dol") ||
      !copy_span(*img, fst_off, fst_size, sys / "fst.bin")) {
    error = "ecriture de sys/ impossible";
    return false;
  }
  std::vector<uint8_t> fst(fst_size);
  if (!img->read(fst_off, fst.data(), fst.size())) { error = "FST illisible"; return false; }
  uint32_t total = 0, done = 0;
  runtime::walk_fst(fst, [&](const runtime::FstEntry &e) { if (!e.dir) ++total; });
  bool ok = true;
  runtime::walk_fst(fst, [&](const runtime::FstEntry &e) {
    if (e.dir || !ok) return;
    fs::path host = files;
    for (const std::string &c : e.raw) {
      std::string u8;
      if (!runtime::fst_component_path(c, u8)) return;   // nom refuse : on saute, comme la lecture de dossier
      host /= fs::u8path(u8);
    }
    if (!copy_span(*img, e.offset, e.size, host)) { ok = false; error = "ecriture impossible : " + host.string(); return; }
    if (progress && total) progress(int(100ull * ++done / total));
  });
  if (!ok) return false;
  // Drapeau de fin : ecrit en dernier, une extraction interrompue est donc refaite.
  std::ofstream(root / "ready.properties") << "format=1\nid=G4BE08\ndisc=" << disc_number << "\nreferences=" << total << "\n";
  return true;
}

bool disc_ready(const std::string &dest, int disc_number) {
  // La numerotation de l'importeur du noyau (disc1, disc2) est acceptee telle quelle.
  for (int n : {disc_number, disc_number + 1})
    if (fs::is_regular_file(fs::u8path(dest) / ("disc" + std::to_string(n)) / "ready.properties") ||
        fs::is_regular_file(fs::u8path(dest) / ("disc" + std::to_string(n)) / "ready1.properties") ||
        fs::is_regular_file(fs::u8path(dest) / "ready1.properties"))
      return true;
  return false;
}

}  // namespace nj::native
