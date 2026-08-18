// P3 : table des symboles du moteur offerts au code genere (p3_table.cpp).
#pragma once
#include <cstdint>

namespace nj::runtime::p3 {

// Les symboles sont reconnus par l'empreinte de leur nom (FNV-1a 64 bits), calculee a la
// compilation : les noms ne sont pas dans l'executable.
constexpr uint64_t name_hash(const char *s) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (; *s; ++s) h = (h ^ (uint8_t)*s) * 0x100000001b3ull;
    return h;
}
consteval uint64_t name_hash_ct(const char *s) { return name_hash(s); }

struct Symbol {
    uint64_t hash;
    void *addr;
};
extern const Symbol kTable[];
extern const unsigned kTableSize;
inline constexpr uint64_t kSlotTableHash = name_hash_ct("nj_slot");

}  // namespace nj::runtime::p3
