// Rapport lisible de l'onglet « Report » (13/10) : voir runtime/report.h.
#include "runtime/report.h"

#include "common/platform.h"

#if __has_include("build_commit.inc")
#include "build_commit.inc"   // ecrit par les scripts de build (commit du code compile)
#endif
#ifndef NJ_COMMIT
#define NJ_COMMIT "dev"
#endif

#include <SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__ANDROID__)
#include <dlfcn.h>
#include <sys/system_properties.h>
#endif

namespace fs = std::filesystem;

namespace nj::report {
namespace {

std::mutex g_m;
std::vector<std::string> g_lines;
std::string g_gpu;
const auto g_t0 = std::chrono::steady_clock::now();

// ---- SHA-256 en flux (FIPS 180-4) : empreinte du binaire, la meme que celle des symboles gardes ----
struct Sha256 {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t buf[64];
    size_t n = 0;
    uint64_t total = 0;
    static uint32_t r(uint32_t x, int k) { return (x >> k) | (x << (32 - k)); }
    void block(const uint8_t *p) {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = uint32_t(p[4 * i]) << 24 | uint32_t(p[4 * i + 1]) << 16 | uint32_t(p[4 * i + 2]) << 8 | p[4 * i + 3];
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = r(w[i - 15], 7) ^ r(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = r(w[i - 2], 17) ^ r(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t t1 = hh + (r(e, 6) ^ r(e, 11) ^ r(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
            const uint32_t t2 = (r(a, 2) ^ r(a, 13) ^ r(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    void update(const uint8_t *p, size_t len) {
        total += len;
        while (len) {
            const size_t k = std::min(len, 64 - n);
            std::memcpy(buf + n, p, k);
            n += k; p += k; len -= k;
            if (n == 64) { block(buf); n = 0; }
        }
    }
    std::string hex() {
        const uint64_t bits = total * 8;
        const uint8_t one = 0x80, zero = 0;
        update(&one, 1);
        while (n != 56) update(&zero, 1);
        uint8_t l[8];
        for (int i = 0; i < 8; ++i) l[i] = uint8_t(bits >> (56 - 8 * i));
        update(l, 8);
        char s[65];
        for (int i = 0; i < 8; ++i) std::snprintf(s + 8 * i, 9, "%08x", h[i]);
        return s;
    }
};

std::string module_path() {
#if defined(_WIN32)
    wchar_t w[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, w, MAX_PATH * 4);
    if (!n) return {};
    const auto s = fs::path(std::wstring(w, n)).u8string();
    return std::string(s.begin(), s.end());
#elif defined(__ANDROID__)
    Dl_info info{};
    if (dladdr(reinterpret_cast<void *>(&add), &info) && info.dli_fname) return info.dli_fname;
    return {};
#else
    return {};
#endif
}

std::atomic<int> g_build_state{0};   // 0 pas commence, 1 en cours, 2 pret
std::string g_build_id;

void hash_build() {
    std::string id = "unknown";
    std::ifstream f(fs::u8path(module_path()), std::ios::binary);
    if (f) {
        Sha256 s;
        std::vector<char> b(1 << 20);
        while (f) {
            f.read(b.data(), (std::streamsize)b.size());
            s.update(reinterpret_cast<const uint8_t *>(b.data()), size_t(f.gcount()));
        }
        id = s.hex().substr(0, 16);
    }
    std::lock_guard<std::mutex> l(g_m);
    g_build_id = id;
    g_build_state = 2;
}

std::string base_dir() {
#if defined(_WIN32)
    char d[1024];
    const DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", d, sizeof d);
    return n && n < sizeof d ? std::string(d) + "/Nightjar" : std::string();
#else
    return nj::writable_path("");
#endif
}

std::string machine() {
    char b[512];
    std::string os;
#if defined(_WIN32)
    using RtlGetVersionFn = LONG(WINAPI *)(OSVERSIONINFOW *);
    OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof v;
    if (HMODULE nt = GetModuleHandleW(L"ntdll.dll"))
        if (auto fn = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void *>(GetProcAddress(nt, "RtlGetVersion"))))
            fn(&v);
    std::snprintf(b, sizeof b, "Windows %s (build %lu)", v.dwBuildNumber >= 22000 ? "11" : "10",
                  (unsigned long)v.dwBuildNumber);
    os = b;
    char cpu[128] = {};
    DWORD sz = sizeof cpu;
    RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString",
                 RRF_RT_REG_SZ, nullptr, cpu, &sz);
    std::string c = cpu;
    while (!c.empty() && c.back() == ' ') c.pop_back();
    if (!c.empty()) os += "\nCPU: " + c;
#elif defined(__ANDROID__)
    char man[PROP_VALUE_MAX] = {}, model[PROP_VALUE_MAX] = {}, rel[PROP_VALUE_MAX] = {}, sdk[PROP_VALUE_MAX] = {},
         soc[PROP_VALUE_MAX] = {};
    __system_property_get("ro.product.manufacturer", man);
    __system_property_get("ro.product.model", model);
    __system_property_get("ro.build.version.release", rel);
    __system_property_get("ro.build.version.sdk", sdk);
    __system_property_get("ro.soc.model", soc);
    std::snprintf(b, sizeof b, "%s %s, Android %s (API %s)%s%s", man, model, rel, sdk, soc[0] ? ", SoC " : "", soc);
    os = b;
#else
    os = SDL_GetPlatform();
#endif
    std::snprintf(b, sizeof b, "\nCores: %d, RAM: %.1f GB", SDL_GetCPUCount(), SDL_GetSystemRAM() / 1024.0);
    return os + b;
}

std::string plantage_path() {
#if defined(_WIN32)
    const std::string d = base_dir();
    return d.empty() ? std::string() : d + "/plantage.txt";
#else
    return nj::writable_path("plantage.txt");
#endif
}

// Lignes du rapport de plantage entre les deux derniers debuts de seance (la seance precedente).
std::vector<std::string> previous_session_lines() {
    std::ifstream f(fs::u8path(plantage_path()));
    std::vector<std::string> all;
    for (std::string l; std::getline(f, l);) all.push_back(l);
    std::vector<size_t> starts;
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i].find("session start") != std::string::npos) starts.push_back(i);
    std::vector<std::string> out;
    if (starts.size() < 2) return out;
    for (size_t i = starts[starts.size() - 2] + 1; i < starts.back(); ++i) out.push_back(all[i]);
    return out;
}

}  // namespace

void add(const std::string &line) {
    const auto s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - g_t0).count();
    char t[32];
    std::snprintf(t, sizeof t, "[%02lld:%02lld] ", (long long)(s / 60), (long long)(s % 60));
    std::lock_guard<std::mutex> l(g_m);
    if (g_lines.size() >= 300) g_lines.erase(g_lines.begin());
    g_lines.push_back(t + line);
}

void set_gpu(const char *renderer, const char *version) {
    std::lock_guard<std::mutex> l(g_m);
    if (!g_gpu.empty()) return;
    g_gpu = std::string(renderer ? renderer : "?") + " (" + (version ? version : "?") + ")";
}

Text build(const std::vector<std::string> &settings) {
    int expected = 0;
    if (g_build_state.compare_exchange_strong(expected, 1)) std::thread(hash_build).detach();
    std::vector<std::string> events;
    std::string gpu, build;
    {
        std::lock_guard<std::mutex> l(g_m);
        events = g_lines;
        gpu = g_gpu;
        build = g_build_state == 2 ? g_build_id : std::string("(computing...)");
    }
    Text t;
    std::string head = "**Nightjar report**\nBuild: " + build + " (commit " NJ_COMMIT ")\n" + machine() + "\nGPU: " +
                       (gpu.empty() ? std::string("(not started yet)") : gpu) + "\n";
    // manettes vues par SDL (14/10) : reconnue = utilisable ; sinon nom du peripherique brut
    std::string pads;
    for (int j = 0, n = SDL_WasInit(SDL_INIT_JOYSTICK) ? SDL_NumJoysticks() : 0; j < n; ++j) {
        const bool gc = SDL_IsGameController(j);
        const char *nm = gc ? SDL_GameControllerNameForIndex(j) : SDL_JoystickNameForIndex(j);
        if (!gc && nm && std::strstr(nm, "Accelerometer")) continue;   // capteur du telephone (Android)
        pads += std::string(pads.empty() ? "" : ", ") + (nm ? nm : "?") + (gc ? "" : " (not recognized)");
    }
    head += "Controllers: " + (pads.empty() ? std::string("none") : pads) + "\n";
    std::string set = "Settings:";
    for (const std::string &s : settings) set += " " + s + ";";
    set += "\n";
    const std::vector<std::string> prev = previous_session_lines();
    std::string prev_block;
    if (!prev.empty()) {
        t.crash = prev.front();
        const size_t sp = t.crash.find(' ', t.crash.find(' ') + 1);   // sans la date et l'heure
        if (sp != std::string::npos) t.crash = t.crash.substr(sp + 1);
        prev_block = "Last session ended abnormally:\n";
        for (const std::string &l : prev) prev_block += "  " + l + "\n";
    }
    std::string ev = "This session:\n";
    for (const std::string &e : events) ev += "  " + e + "\n";
    t.full = head + set + prev_block + ev;
    // version courte : en-tete, reglages, fin anormale (coupee), derniers evenements tant que ca tient
    std::string brief = head + set;
    if (!prev_block.empty()) brief += prev_block.substr(0, 600) + (prev_block.size() > 600 ? "  ...\n" : "");
    std::string tail;
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
        if (brief.size() + tail.size() + it->size() + 40 > 1900) break;
        tail = "  " + *it + "\n" + tail;
    }
    brief += "This session (latest):\n" + tail;
    t.brief = brief.substr(0, 1900);
    return t;
}

std::vector<std::string> attachments() {
    std::vector<std::string> out;
    const std::string jdir = base_dir().empty() ? std::string() : base_dir() + "/journal";
    std::vector<fs::path> njl;
    std::error_code ec;
    for (const auto &e : fs::directory_iterator(fs::u8path(jdir), ec))
        if (e.path().extension() == ".njl") njl.push_back(e.path());
    std::sort(njl.begin(), njl.end());
    for (size_t i = njl.size() > 2 ? njl.size() - 2 : 0; i < njl.size(); ++i) {
        const auto s = njl[i].u8string();
        out.emplace_back(s.begin(), s.end());
    }
    if (fs::exists(fs::u8path(plantage_path()), ec)) out.push_back(plantage_path());
    return out;
}

std::string folder() { return base_dir(); }

}  // namespace nj::report
