#include "runtime/disc_image.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#include <cstring>
#include <filesystem>
#include <list>
#include <mutex>
#include <vector>

#include "zstd.h"

namespace nj::runtime {

namespace {

uint32_t be32(const uint8_t *p) { return (uint32_t(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
uint64_t be64(const uint8_t *p) { return (uint64_t(be32(p)) << 32) | be32(p + 4); }

// Fichier hote ouvert en lecture, positionne a chaque appel (chemins UTF-8, y compris Windows).
// Android : un document choisi par l'utilisateur arrive en descripteur, nomme /proc/self/fd/N.
// Depuis Android 11 (stockage FUSE), rouvrir ce chemin est refuse a une application sans
// permission de stockage (MuMu, Android 15, 05/10) : on duplique le descripteur et on lit par
// pread, qui ne touche pas a la position partagee entre les doubles.
class HostFile {
 public:
#ifdef _WIN32
  bool open(const std::string &path) {
    f_ = _wfopen(std::filesystem::u8path(path).wstring().c_str(), L"rb");
    if (!f_) return false;
    if (_fseeki64(f_, 0, SEEK_END) != 0) return false;
    const long long end = _ftelli64(f_);
    if (end <= 0) return false;
    size_ = (uint64_t)end;
    return true;
  }
  ~HostFile() {
    if (f_) std::fclose(f_);
  }
  bool read(uint64_t off, void *dst, size_t n) {
    if (off + n > size_) return false;
    if (_fseeki64(f_, (long long)off, SEEK_SET) != 0) return false;
    return std::fread(dst, 1, n, f_) == n;
  }
#else
  bool open(const std::string &path) {
    static const char kFd[] = "/proc/self/fd/";
    if (path.compare(0, sizeof(kFd) - 1, kFd) == 0)
      fd_ = ::dup(std::atoi(path.c_str() + sizeof(kFd) - 1));
    else
      fd_ = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd_ < 0) return false;
    const off_t end = ::lseek(fd_, 0, SEEK_END);
    if (end <= 0) return false;
    size_ = (uint64_t)end;
    return true;
  }
  ~HostFile() {
    if (fd_ >= 0) ::close(fd_);
  }
  bool read(uint64_t off, void *dst, size_t n) {
    if (off + n > size_) return false;
    uint8_t *p = static_cast<uint8_t *>(dst);
    while (n) {
      const ssize_t r = ::pread(fd_, p, n, (off_t)off);
      if (r <= 0) return false;
      p += r;
      off += (uint64_t)r;
      n -= (size_t)r;
    }
    return true;
  }
#endif
  uint64_t size() const { return size_; }

 private:
#ifdef _WIN32
  FILE *f_ = nullptr;
#else
  int fd_ = -1;
#endif
  uint64_t size_ = 0;
};

// --- copie brute ---------------------------------------------------------------------------
class RawImage final : public DiscImage {
 public:
  explicit RawImage(std::unique_ptr<HostFile> f) : f_(std::move(f)) {}
  bool read(uint64_t off, void *dst, size_t n) override {
    std::lock_guard<std::mutex> g(lock_);
    return f_->read(off, dst, n);
  }
  uint64_t size() const override { return f_->size(); }
  const char *format() const override { return "ISO"; }

 private:
  std::unique_ptr<HostFile> f_;
  std::mutex lock_;
};

// --- RVZ -----------------------------------------------------------------------------------
// Donnees de remplissage du disque : generateur de Fibonacci decale (j = 32, k = 521) amorce par
// 17 mots ; octets tires du haut de chaque mot (>> 24, >> 18, >> 8, >> 0).
class JunkStream {
 public:
  JunkStream(const uint8_t seed[68], uint32_t skip) {
    for (int i = 0; i < 17; ++i) b_[i] = be32(seed + 4 * i);
    for (int i = 17; i < 521; ++i) b_[i] = (b_[i - 17] << 23) ^ (b_[i - 16] >> 9) ^ b_[i - 1];
    for (int i = 0; i < 4; ++i) forward();
    pos_ = 0;
    while (skip >= 521 * 4) {
      forward();
      skip -= 521 * 4;
    }
    pos_ = skip;
  }
  void take(uint8_t *out, size_t n) {
    static constexpr int kShift[4] = {24, 18, 8, 0};
    for (size_t i = 0; i < n; ++i) {
      if (pos_ == 521 * 4) {
        forward();
        pos_ = 0;
      }
      out[i] = uint8_t(b_[pos_ / 4] >> kShift[pos_ % 4]);
      ++pos_;
    }
  }

 private:
  void forward() {
    for (int i = 0; i < 32; ++i) b_[i] ^= b_[i + 521 - 32];
    for (int i = 32; i < 521; ++i) b_[i] ^= b_[i - 32];
  }
  uint32_t b_[521];
  uint32_t pos_ = 0;
};

class RvzImage final : public DiscImage {
 public:
  explicit RvzImage(std::unique_ptr<HostFile> f) : f_(std::move(f)) {}
  ~RvzImage() override {
    if (dctx_) ZSTD_freeDCtx(dctx_);
  }

  bool init() {
    uint8_t h[0x48 + 0xDC];
    if (!f_->read(0, h, sizeof h)) return false;
    rvz_ = std::memcmp(h, "RVZ\x01", 4) == 0;
    if (!rvz_ && std::memcmp(h, "WIA\x01", 4) != 0) return false;
    size_ = be64(h + 0x24);
    const uint8_t *d = h + 0x48;
    // GameCube seulement (type 1), sans compression ou en zstd (5).
    if (be32(d) != 1) return false;
    compression_ = be32(d + 4);
    if (compression_ != 0 && compression_ != 5) return false;
    chunk_ = be32(d + 0x0C);
    if (chunk_ < 0x8000 || chunk_ > (8u << 20)) return false;
    std::memcpy(head_, d + 0x10, 0x80);
    const uint32_t n_raw = be32(d + 0xB4), raw_size = be32(d + 0xC0);
    const uint32_t n_groups = be32(d + 0xC4), group_size = be32(d + 0xD0);
    if (n_raw > 4096 || n_groups > (1u << 22)) return false;
    dctx_ = ZSTD_createDCtx();
    std::vector<uint8_t> raw, grp;
    if (!table(be64(d + 0xB8), raw_size, 24ull * n_raw, raw) ||
        !table(be64(d + 0xC8), group_size, uint64_t(rvz_ ? 12 : 8) * n_groups, grp))
      return false;
    for (uint32_t i = 0; i < n_raw; ++i) {
      const uint8_t *e = raw.data() + 24 * i;
      Raw r{be64(e), be64(e + 8), be32(e + 16), be32(e + 20)};
      if (uint64_t(r.first_group) + r.n_groups > n_groups) return false;
      raws_.push_back(r);
    }
    const size_t gs = rvz_ ? 12 : 8;
    for (uint32_t i = 0; i < n_groups; ++i) {
      const uint8_t *e = grp.data() + gs * i;
      groups_.push_back({uint64_t(be32(e)) * 4, be32(e + 4), rvz_ ? be32(e + 8) : 0u});
    }
    return true;
  }

  bool read(uint64_t off, void *dst, size_t n) override {
    if (off + n > size_) return false;
    std::lock_guard<std::mutex> g(lock_);
    uint8_t *out = static_cast<uint8_t *>(dst);
    while (n > 0) {
      size_t k;
      if (off < 0x80) {
        k = std::min<uint64_t>(n, 0x80 - off);
        std::memcpy(out, head_ + off, k);
      } else {
        const Raw *r = nullptr;
        uint64_t start = 0;
        for (const Raw &x : raws_) {
          // la premiere zone commence a 0x80 et couvre depuis le debut de son bloc de 0x8000
          const uint64_t s = x.offset - x.offset % 0x8000;
          if (s <= off && off < x.offset + x.size) {
            r = &x;
            start = s;
            break;
          }
        }
        if (!r) {   // hors des zones : zeros
          k = n;
          std::memset(out, 0, k);
        } else {
          const uint32_t gi = r->first_group + uint32_t((off - start) / chunk_);
          if (gi >= r->first_group + r->n_groups) return false;
          const uint64_t g_disc = start + uint64_t(gi - r->first_group) * chunk_;
          const std::vector<uint8_t> *data = group(gi, g_disc);
          if (!data) return false;
          const uint64_t in = off - g_disc;
          k = (size_t)std::min<uint64_t>(n, chunk_ - in);
          if (in + k <= data->size())
            std::memcpy(out, data->data() + in, k);
          else {   // groupe plus court que le bloc (fin de zone) : le reste vaut zero
            const size_t have = in < data->size() ? data->size() - (size_t)in : 0;
            if (have) std::memcpy(out, data->data() + in, have);
            std::memset(out + have, 0, k - have);
          }
        }
      }
      off += k;
      out += k;
      n -= k;
    }
    return true;
  }
  uint64_t size() const override { return size_; }
  const char *format() const override { return rvz_ ? "RVZ" : "WIA"; }

 private:
  struct Raw {
    uint64_t offset, size;
    uint32_t first_group, n_groups;
  };
  struct Group {
    uint64_t file_off;
    uint32_t size;     // bit 31 : compresse
    uint32_t packed;   // RVZ : taille des donnees empaquetees (remplissage a regenerer), 0 sinon
  };

  // Table (zones, groupes) : compressee comme les donnees.
  bool table(uint64_t file_off, uint32_t stored, uint64_t want, std::vector<uint8_t> &out) {
    std::vector<uint8_t> in(stored);
    if (!f_->read(file_off, in.data(), stored)) return false;
    if (compression_ == 0) {
      out = std::move(in);
    } else {
      out.resize((size_t)want);
      const size_t got = ZSTD_decompressDCtx(dctx_, out.data(), out.size(), in.data(), in.size());
      if (ZSTD_isError(got)) return false;
      out.resize(got);
    }
    return out.size() >= want;
  }

  const std::vector<uint8_t> *group(uint32_t gi, uint64_t g_disc) {
    for (auto it = cache_.begin(); it != cache_.end(); ++it)
      if (it->first == gi) {
        cache_.splice(cache_.begin(), cache_, it);
        return &cache_.front().second;
      }
    const Group &g = groups_[gi];
    const bool compressed = (g.size & 0x80000000u) != 0;
    const uint32_t stored = g.size & 0x7FFFFFFFu;
    std::vector<std::uint8_t> data;
    if (stored == 0) {
      data.assign(chunk_, 0);
    } else {
      std::vector<uint8_t> in(stored);
      if (!f_->read(g.file_off, in.data(), stored)) return nullptr;
      if (compressed && compression_ == 5) {
        data.resize(std::max<size_t>(g.packed, chunk_) + 1024);
        const size_t got = ZSTD_decompressDCtx(dctx_, data.data(), data.size(), in.data(), in.size());
        if (ZSTD_isError(got)) return nullptr;
        data.resize(got);
      } else {
        data = std::move(in);
      }
      if (g.packed && !unpack(data, g_disc)) return nullptr;
    }
    cache_.emplace_front(gi, std::move(data));
    if (cache_.size() > 8) cache_.pop_back();
    return &cache_.front().second;
  }

  // Donnees empaquetees : suites de (u32 taille ; octets), le bit 31 de la taille annonce un
  // remplissage regenere depuis une graine de 68 octets.
  static bool unpack(std::vector<uint8_t> &data, uint64_t g_disc) {
    std::vector<uint8_t> out;
    out.reserve(data.size() * 2);
    size_t p = 0;
    while (p + 4 <= data.size()) {
      uint32_t n = be32(&data[p]);
      p += 4;
      if (n & 0x80000000u) {
        n &= 0x7FFFFFFFu;
        if (p + 68 > data.size()) return false;
        const uint64_t pos = g_disc + out.size();
        JunkStream junk(&data[p], uint32_t(pos % 0x8000));
        p += 68;
        const size_t at = out.size();
        out.resize(at + n);
        junk.take(out.data() + at, n);
      } else {
        if (p + n > data.size()) return false;
        out.insert(out.end(), data.begin() + p, data.begin() + p + n);
        p += n;
      }
    }
    data.swap(out);
    return true;
  }

  std::unique_ptr<HostFile> f_;
  std::mutex lock_;
  ZSTD_DCtx *dctx_ = nullptr;
  bool rvz_ = false;
  uint32_t compression_ = 0, chunk_ = 0;
  uint64_t size_ = 0;
  uint8_t head_[0x80] = {};
  std::vector<Raw> raws_;
  std::vector<Group> groups_;
  std::list<std::pair<uint32_t, std::vector<uint8_t>>> cache_;
};

// --- disque extrait --------------------------------------------------------------------------
// Les octets du disque sont recomposes : chaque plage [debut, fin) vient d'un fichier hote.
class DirImage final : public DiscImage {
 public:
  bool init(const std::filesystem::path &root) {
    namespace fs = ::std::filesystem;
    std::error_code ec;
    fs::path sys{root / "sys"};
    if (!fs::exists(sys / "boot.bin", ec)) sys = root;   // sys/ absent : tout a la racine
    const fs::path files = root / "files";
    std::vector<uint8_t> boot, fst;
    if (!slurp(sys / "boot.bin", boot) || boot.size() < 0x440 || !slurp(sys / "fst.bin", fst)) return false;
    const uint64_t dol_off = be32(&boot[0x420]), fst_off = be32(&boot[0x424]);
    add(0, sys / "boot.bin");
    add(0x440, sys / "bi2.bin");
    add(0x2440, sys / "apploader.img");
    if (dol_off) add(dol_off, sys / "main.dol");
    if (fst_off) add(fst_off, sys / "fst.bin");
    walk_fst(fst, [&](const FstEntry &e) {
      if (e.dir) return;
      fs::path host = files;
      for (const std::string &c : e.raw) {
        std::string u8;
        if (!fst_component_path(c, u8)) return;
        host /= fs::u8path(u8);
      }
      add(e.offset, host, e.size);
    });
    std::sort(parts_.begin(), parts_.end(), [](const Part &a, const Part &b) { return a.start < b.start; });
    for (const Part &p : parts_) size_ = std::max(size_, p.start + p.size);
    size_ = std::max<uint64_t>(size_, 0x57058000ull);   // taille d'un disque GameCube
    return true;
  }

  bool read(uint64_t off, void *dst, size_t n) override {
    if (off + n > size_) return false;
    std::lock_guard<std::mutex> g(lock_);
    uint8_t *out = static_cast<uint8_t *>(dst);
    std::memset(out, 0, n);   // entre les fichiers : zeros
    const uint64_t end = off + n;
    auto it = std::upper_bound(parts_.begin(), parts_.end(), off,
                               [](uint64_t v, const Part &p) { return v < p.start; });
    if (it != parts_.begin()) --it;
    for (; it != parts_.end() && it->start < end; ++it) {
      const uint64_t lo = std::max(off, it->start), hi = std::min(end, it->start + it->size);
      if (lo >= hi) continue;
      if (open_ != &*it) {
        file_ = std::make_unique<HostFile>();
        open_ = file_->open(it->host) ? &*it : nullptr;
        if (!open_) continue;   // fichier absent : zeros
      }
      const uint64_t avail = file_->size() > lo - it->start ? file_->size() - (lo - it->start) : 0;
      const size_t k = (size_t)std::min<uint64_t>(hi - lo, avail);
      if (k && !file_->read(lo - it->start, out + (lo - off), k)) return false;
    }
    return true;
  }
  uint64_t size() const override { return size_; }
  const char *format() const override { return "dossier"; }

 private:
  struct Part {
    uint64_t start, size;
    std::string host;   // UTF-8
  };
  static bool slurp(const std::filesystem::path &p, std::vector<uint8_t> &out) {
    const auto u8 = p.u8string();
    HostFile f;
    if (!f.open(std::string(u8.begin(), u8.end()))) return false;
    out.resize((size_t)f.size());
    return out.empty() || f.read(0, out.data(), out.size());
  }
  void add(uint64_t start, const std::filesystem::path &p, uint64_t size = 0) {
    std::error_code ec;
    const auto u8 = p.u8string();
    std::string host(u8.begin(), u8.end());
    if (!size) {
      size = std::filesystem::file_size(p, ec);
      if (ec) return;
    }
    if (size) parts_.push_back({start, size, std::move(host)});
  }

  std::vector<Part> parts_;
  uint64_t size_ = 0;
  std::mutex lock_;
  std::unique_ptr<HostFile> file_;
  const Part *open_ = nullptr;
};

}  // namespace

void walk_fst(const std::vector<uint8_t> &fst, const std::function<void(const FstEntry &)> &fn) {
  if (fst.size() < 12) return;
  const uint8_t *p = fst.data();
  const uint32_t n = be32(p + 8);
  if (uint64_t(n) * 12 > fst.size()) return;
  const char *names = reinterpret_cast<const char *>(p + size_t(n) * 12);
  const size_t names_len = fst.size() - size_t(n) * 12;
  struct Dir {
    uint32_t end;
    std::vector<std::string> raw;
    std::string lower;
  };
  std::vector<Dir> dirs{{n, {}, ""}};
  for (uint32_t i = 1; i < n; ++i) {
    while (dirs.size() > 1 && i >= dirs.back().end) dirs.pop_back();
    const uint8_t *e = p + size_t(i) * 12;
    const uint32_t name_off = (uint32_t(e[1]) << 16) | (uint32_t(e[2]) << 8) | e[3];
    std::string name;
    if (name_off < names_len) name.assign(names + name_off, strnlen(names + name_off, names_len - name_off));
    std::string low = name;
    for (char &c : low) c = (char)std::tolower((unsigned char)c);
    FstEntry entry;
    entry.raw = dirs.back().raw;
    entry.raw.push_back(name);
    entry.lower = dirs.back().lower.empty() ? low : dirs.back().lower + "/" + low;
    entry.dir = e[0] != 0;
    entry.offset = entry.dir ? 0 : be32(e + 4);
    entry.size = be32(e + 8);
    entry.index = i;
    fn(entry);
    if (entry.dir) dirs.push_back({std::max(entry.size, i + 1), entry.raw, entry.lower});
  }
}

bool fst_component_path(const std::string &raw, std::string &out) {
  if (raw.empty() || raw == "." || raw == ".." || raw.find_first_of("/\\:") != std::string::npos) return false;
  bool ascii = true;
  for (char c : raw) ascii = ascii && (unsigned char)c < 0x80;
  if (ascii) {
    out = raw;
    return true;
  }
#ifdef _WIN32
  // Shift-JIS (page 932) -> UTF-16 -> UTF-8 : le nom sous lequel l'extraction a ecrit le fichier
  // (RE4 en a un : files/ss/Item/IDモデルリスト.txt)
  const int nw = MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, raw.data(), (int)raw.size(), nullptr, 0);
  if (nw <= 0) return false;
  std::wstring w((size_t)nw, L'\0');
  MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, raw.data(), (int)raw.size(), w.data(), nw);
  const int nu = WideCharToMultiByte(CP_UTF8, 0, w.data(), nw, nullptr, 0, nullptr, nullptr);
  out.assign((size_t)nu, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), nw, out.data(), nu, nullptr, nullptr);
  return nu > 0;
#elif defined(__APPLE__)
  // iOS/macOS (19/11) : APFS refuse les noms qui ne sont pas de l'UTF-8 valide (1er essai iPhone : extraction
  // arretee sur files/ss/Item/IDモデルリスト.txt, liste de debug jamais lue par le jeu) -> nom saute.
  return false;
#else
  out = raw;   // chemins POSIX : octets tels quels
  return true;
#endif
}

std::unique_ptr<DiscImage> DiscImage::open(const std::string &path) {
  std::error_code ec;
  if (std::filesystem::is_directory(std::filesystem::u8path(path), ec)) {
    auto d = std::make_unique<DirImage>();
    if (!d->init(std::filesystem::u8path(path))) return nullptr;
    return d;
  }
  auto f = std::make_unique<HostFile>();
  if (!f->open(path)) return nullptr;
  uint8_t magic[4];
  if (!f->read(0, magic, 4)) return nullptr;
  if (std::memcmp(magic, "RVZ\x01", 4) == 0 || std::memcmp(magic, "WIA\x01", 4) == 0) {
    auto r = std::make_unique<RvzImage>(std::move(f));
    if (!r->init()) return nullptr;
    return r;
  }
  return std::make_unique<RawImage>(std::move(f));
}

}  // namespace nj::runtime
