// Enregistreur de flux GX : voir include/runtime/gx/fifo_record.h.
#include "runtime/gx/fifo_record.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <thread>
#include <vector>

#include "common/platform.h"
#include "runtime/cpu_context.h"
#include "runtime/log.h"

namespace nj::runtime {
extern MMU *g_guest_mem;
}

namespace nj::runtime::gx::rec {
namespace {

#pragma pack(push, 1)
struct DffHeader {
  uint32_t fileId, file_version, min_loader_version;
  uint64_t bpMemOffset;   uint32_t bpMemSize;
  uint64_t cpMemOffset;   uint32_t cpMemSize;
  uint64_t xfMemOffset;   uint32_t xfMemSize;
  uint64_t xfRegsOffset;  uint32_t xfRegsSize;
  uint64_t frameListOffset; uint32_t frameCount, flags;
  uint64_t texMemOffset;  uint32_t texMemSize;
  uint32_t ram_size_real, exram_size_real;
  char gameId[8];
  uint8_t reserved[24];
};
struct DffFrameInfo {
  uint64_t fifoDataOffset;
  uint32_t fifoDataSize, fifoStart, fifoEnd;
  uint64_t memoryUpdatesOffset;
  uint32_t numMemoryUpdates;
  uint8_t reserved[32];
};
struct DffMemUpdate {
  uint32_t fifoPosition, address;
  uint64_t dataOffset;
  uint32_t dataSize;
  uint8_t type;
  uint8_t reserved[3];
};
#pragma pack(pop)
static_assert(sizeof(DffHeader) == 128 && sizeof(DffFrameInfo) == 64 && sizeof(DffMemUpdate) == 24, "format .dff");

struct Upd {
  uint32_t addr;
  std::vector<uint8_t> data;
  uint32_t pos = 0;   // position dans les octets de l'image ; 0 = avant le premier octet
  uint8_t type = 4;   // 4 : page de memoire ; 0x80 : copie volatile (a rejouer par le mecanisme du moteur)
};
struct Frame {
  std::vector<uint8_t> fifo;
  std::vector<Upd> ups;   // pages changees (pos 0) puis copies volatiles (pos croissante)
};
struct Rec {
  std::vector<Frame> frames;
  std::vector<uint8_t> shadow;   // MEM1 tel que deja enregistre
  uint32_t bp[256], cp[256], xf[4096], xr[88];
  int want = 1;
};

std::atomic<int> g_request{0};
std::atomic<bool> g_active{false};
Rec *g_r = nullptr;

constexpr size_t kPage = 4096;

// Pages de MEM1 qui different de l'ombre, regroupees en plages continues ; l'ombre est mise a jour.
void capture_dirty(Frame &f) {
  const auto &m = nj::runtime::g_guest_mem->mem1;
  const size_t n = m.size() - 8;
  std::vector<uint8_t> &sh = g_r->shadow;
  size_t a = 0;
  while (a < n) {
    const size_t e = std::min(a + kPage, n);
    if (std::memcmp(m.data() + a, sh.data() + a, e - a) == 0) {
      a = e;
      continue;
    }
    size_t end = e;
    while (end < n) {
      const size_t e2 = std::min(end + kPage, n);
      if (std::memcmp(m.data() + end, sh.data() + end, e2 - end) == 0) break;
      end = e2;
    }
    f.ups.push_back({(uint32_t)a, std::vector<uint8_t>(m.data() + a, m.data() + end), 0});
    std::memcpy(sh.data() + a, m.data() + a, end - a);
    a = end;
  }
}

void write_file(Rec *r) {
  static std::atomic<int> counter{0};
  char name[48];
  std::snprintf(name, sizeof name, "fifo_%ld_%d.dff", (long)std::time(nullptr) % 1000000, counter.fetch_add(1));
  const std::string path = nj::writable_path(name);
  FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) {
    NJ_LOG << "[FIFO] ecriture impossible : " << path << std::endl;
    delete r;
    return;
  }
  DffHeader h{};
  h.fileId = 0x0D01F1F0u;
  h.file_version = 6;
  h.min_loader_version = 1;
  h.ram_size_real = 0x01800000u;
  std::memcpy(h.gameId, "G4BE08", 6);
  std::fwrite(&h, sizeof h, 1, f);   // reecrit a la fin avec les decalages
  uint64_t pos = sizeof h;
  auto put = [&](const void *p, size_t n) {
    std::fwrite(p, 1, n, f);
    pos += n;
  };
  h.bpMemOffset = pos;  h.bpMemSize = 256;   put(r->bp, sizeof r->bp);
  h.cpMemOffset = pos;  h.cpMemSize = 256;   put(r->cp, sizeof r->cp);
  h.xfMemOffset = pos;  h.xfMemSize = 4096;  put(r->xf, sizeof r->xf);
  h.xfRegsOffset = pos; h.xfRegsSize = 88;   put(r->xr, sizeof r->xr);
  {
    static const std::vector<uint8_t> zero(1u << 20, 0);   // memoire de textures (TMEM) : non emulee
    h.texMemOffset = pos;
    h.texMemSize = (uint32_t)zero.size();
    put(zero.data(), zero.size());
  }
  std::vector<DffFrameInfo> table;
  uint64_t total_updates = 0;
  for (const Frame &fr : r->frames) {
    DffFrameInfo fi{};
    fi.fifoDataOffset = pos;
    fi.fifoDataSize = (uint32_t)fr.fifo.size();
    fi.fifoStart = 0;
    fi.fifoEnd = fi.fifoDataSize;
    put(fr.fifo.data(), fr.fifo.size());
    std::vector<DffMemUpdate> ups;
    // Dolphin et FifoReplay veulent les mises a jour triees par position : les pages (pos 0) d'abord.
    std::vector<const Upd *> order;
    for (const Upd &u : fr.ups) order.push_back(&u);
    std::stable_sort(order.begin(), order.end(), [](const Upd *a, const Upd *b) { return a->pos < b->pos; });
    for (const Upd *up : order) {
      const Upd &u = *up;
      DffMemUpdate mu{};
      mu.fifoPosition = u.pos;
      mu.address = u.addr;
      mu.dataOffset = pos;
      mu.dataSize = (uint32_t)u.data.size();
      mu.type = u.type;
      put(u.data.data(), u.data.size());
      ups.push_back(mu);
      total_updates += u.data.size();
    }
    fi.memoryUpdatesOffset = pos;
    fi.numMemoryUpdates = (uint32_t)ups.size();
    put(ups.data(), ups.size() * sizeof(DffMemUpdate));
    table.push_back(fi);
  }
  h.frameListOffset = pos;
  h.frameCount = (uint32_t)table.size();
  put(table.data(), table.size() * sizeof(DffFrameInfo));
  std::fseek(f, 0, SEEK_SET);
  std::fwrite(&h, sizeof h, 1, f);
  std::fclose(f);
  NJ_LOG << "[FIFO] enregistre : " << path << " (" << table.size() << " images, " << pos / 1024 << " Kio, memoire "
         << total_updates / 1024 << " Kio)" << std::endl;
  delete r;
}

}  // namespace

void request(int frames) {
  if (!g_active.load()) g_request.store(std::max(1, frames));
}
bool armed() { return g_request.load(std::memory_order_relaxed) > 0 && !g_active.load(std::memory_order_relaxed); }
bool active() { return g_active.load(std::memory_order_relaxed); }

void begin(const uint32_t bp[256], const uint32_t cp[256], const uint32_t xfmem[4096], const uint32_t xfregs[88]) {
  if (!nj::runtime::g_guest_mem) {
    g_request.store(0);
    return;
  }
  Rec *r = new Rec;
  std::memcpy(r->bp, bp, sizeof r->bp);
  std::memcpy(r->cp, cp, sizeof r->cp);
  std::memcpy(r->xf, xfmem, sizeof r->xf);
  std::memcpy(r->xr, xfregs, sizeof r->xr);
  r->want = g_request.exchange(0);
  r->shadow.assign(nj::runtime::g_guest_mem->mem1.size(), 0);
  r->frames.emplace_back();
  g_r = r;
  g_active.store(true);
  NJ_LOG << "[FIFO] enregistrement de " << r->want << " images commence" << std::endl;
}

void on_volatile(uint32_t phys_addr, const uint8_t *data, uint32_t len) {
  if (!g_active.load(std::memory_order_relaxed) || !g_r || !data || !len) return;
  Frame &f = g_r->frames.back();
  Upd u;
  u.addr = phys_addr & 0x01FFFFFFu;
  u.data.assign(data, data + len);
  u.pos = (uint32_t)f.fifo.size();
  u.type = 0x80;
  f.ups.push_back(std::move(u));
}

void on_command(const uint8_t *p, size_t n, bool xfb_copy) {
  if (!g_active.load(std::memory_order_relaxed) || !g_r) return;
  Frame &f = g_r->frames.back();
  f.fifo.insert(f.fifo.end(), p, p + n);
  if (!xfb_copy) return;
  capture_dirty(f);
  if ((int)g_r->frames.size() >= g_r->want) {
    Rec *r = g_r;
    g_r = nullptr;
    g_active.store(false);
    std::thread([r] { write_file(r); }).detach();
  } else {
    g_r->frames.emplace_back();
  }
}

}  // namespace nj::runtime::gx::rec
