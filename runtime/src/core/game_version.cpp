#include "runtime/game_version.h"
#include "runtime/log.h"

#include <algorithm>
#include <cstdio>
#include <iostream>

namespace nj::runtime {

namespace {

struct AddrRow {
  uint32_t debug, usa, europe;   // 0 : pas encore connue pour cette version
};

// tools/re4/gen_addr_table.py
constexpr AddrRow kAddrs[] = {
#include "game_addr_table.inc"
};

GameVersion g_version = GameVersion::Debug;

uint32_t be32(const uint8_t *p) { return (uint32_t(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

}  // namespace

// SHA-1 (FIPS 180-1), pour reconnaitre un fichier.
std::string sha1_hex(const uint8_t *data, size_t size) {
  uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  auto rol = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
  auto block = [&](const uint8_t *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) w[i] = be32(p + i * 4);
    for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
      else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
      else { f = b ^ c ^ d; k = 0xCA62C1D6; }
      const uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
  };
  size_t off = 0;
  for (; off + 64 <= size; off += 64) block(data + off);
  uint8_t tail[128] = {};
  const size_t rest = size - off;
  std::copy(data + off, data + size, tail);
  tail[rest] = 0x80;
  const size_t n = rest < 56 ? 64 : 128;
  const uint64_t bits = uint64_t(size) * 8;
  for (int i = 0; i < 8; ++i) tail[n - 1 - i] = uint8_t(bits >> (i * 8));
  for (size_t o = 0; o < n; o += 64) block(tail + o);
  char out[41];
  for (int i = 0; i < 5; ++i) std::snprintf(out + i * 8, 9, "%08x", h[i]);
  return out;
}

GameVersion detect_game_version(const uint8_t *dol, size_t size) {
  const std::string s = sha1_hex(dol, size);
  if (s == "61e0cdacaa48a2de4a935d925feedac968c6bf6f") return GameVersion::Debug;
  if (s == "5aa5385172d976aa1d9348553b1a3fbc2f5d9622") return GameVersion::Usa;
  if (s == "3467e9e0b0659cf7c4d35acb5661ad4848ea147c") return GameVersion::Europe;
  return GameVersion::Unknown;
}

void set_game_version(GameVersion v) { g_version = v; }
GameVersion game_version() { return g_version; }

const char *game_version_name(GameVersion v) {
  switch (v) {
  case GameVersion::Debug: return "debug";
  case GameVersion::Usa: return "USA";
  case GameVersion::Europe: return "Europe";
  default: return "?";
  }
}

uint32_t ga(uint32_t debug_addr) {
  if (g_version == GameVersion::Debug || g_version == GameVersion::Unknown) return debug_addr;
  const AddrRow *end = kAddrs + sizeof(kAddrs) / sizeof(kAddrs[0]);
  const AddrRow *r = std::lower_bound(kAddrs, end, debug_addr,
                                      [](const AddrRow &row, uint32_t a) { return row.debug < a; });
  if (r == end || r->debug != debug_addr) return debug_addr;
  const uint32_t v = g_version == GameVersion::Usa ? r->usa : r->europe;
  if (!v) {
    static int warned;
    if (warned++ < 8)
      NJ_LOG << "[Version] adresse 0x" << std::hex << debug_addr << std::dec << " inconnue pour cette version\n";
    return debug_addr;
  }
  return v;
}

}  // namespace nj::runtime
