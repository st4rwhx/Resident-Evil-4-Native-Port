// Carte graphique sous Windows : voir runtime/gpu_select.h.
#include "runtime/gpu_select.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_6.h>

// main.cpp : exports lus par les pilotes au lancement (1 = carte dediee)
extern "C" DWORD NvOptimusEnablement;
extern "C" int AmdPowerXpressRequestHighPerformance;
#endif

namespace nj::gpu {

#ifdef _WIN32
namespace {

const wchar_t kKey[] = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";

std::string utf8(const wchar_t *w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring exe_path() {
    wchar_t p[MAX_PATH * 2];
    const DWORD n = GetModuleFileNameW(nullptr, p, sizeof p / sizeof p[0]);
    return n ? std::wstring(p, n) : std::wstring();
}

Adapters enumerate() {
    Adapters a;
    HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
    if (!dxgi) return a;
    using CreateFn = HRESULT(WINAPI *)(REFIID, void **);
    auto create = reinterpret_cast<CreateFn>(reinterpret_cast<void *>(GetProcAddress(dxgi, "CreateDXGIFactory1")));
    IDXGIFactory6 *f6 = nullptr;
    if (create && SUCCEEDED(create(__uuidof(IDXGIFactory6), reinterpret_cast<void **>(&f6))) && f6) {
        auto first = [&](DXGI_GPU_PREFERENCE pref, std::string &name, LUID &luid) {
            IDXGIAdapter1 *ad = nullptr;
            // la premiere carte materielle dans l'ordre demande (pas le rendu logiciel de Microsoft)
            for (UINT i = 0; SUCCEEDED(f6->EnumAdapterByGpuPreference(i, pref, __uuidof(IDXGIAdapter1),
                                                                       reinterpret_cast<void **>(&ad))); ++i) {
                DXGI_ADAPTER_DESC1 d{};
                const bool ok = SUCCEEDED(ad->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
                ad->Release();
                if (ok) {
                    name = utf8(d.Description);
                    luid = d.AdapterLuid;
                    return true;
                }
            }
            return false;
        };
        LUID lo{}, hi{};
        const bool has_lo = first(DXGI_GPU_PREFERENCE_MINIMUM_POWER, a.low, lo);
        const bool has_hi = first(DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, a.high, hi);
        a.hybrid = has_lo && has_hi && (lo.LowPart != hi.LowPart || lo.HighPart != hi.HighPart);
        f6->Release();
    }
    FreeLibrary(dxgi);
    return a;
}

}  // namespace

const Adapters &adapters() {
    static const Adapters a = enumerate();
    return a;
}

int preference() {
    const std::wstring exe = exe_path();
    if (exe.empty()) return 0;
    wchar_t v[128] = {};
    DWORD size = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, exe.c_str(), RRF_RT_REG_SZ, nullptr, v, &size) != ERROR_SUCCESS)
        return 0;
    const std::wstring s(v);
    if (s.find(L"GpuPreference=1") != std::wstring::npos) return 1;
    if (s.find(L"GpuPreference=2") != std::wstring::npos) return 2;
    return 0;
}

bool set_preference(int pref) {
    const std::wstring exe = exe_path();
    if (exe.empty() || pref < 1 || pref > 2) return false;
    const std::wstring v = pref == 1 ? L"GpuPreference=1;" : L"GpuPreference=2;";
    return RegSetKeyValueW(HKEY_CURRENT_USER, kKey, exe.c_str(), REG_SZ, v.c_str(),
                           DWORD((v.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

void apply_at_startup() {
    // Cartes recensees ici, au demarrage, avant le contexte GL : le menu F10 ne fait que lire le
    // resultat (13/10 : DXGI charge pendant une image du menu, juste avant un plantage a l'ouverture).
    (void)adapters();
    // « Economie » choisie (dans Nightjar ou dans Windows) : les exports demandent alors aussi
    // l'integree, lus par le pilote a la creation du contexte GL. Sinon : carte dediee si elle existe.
    if (preference() == 1) {
        NvOptimusEnablement = 0;
        AmdPowerXpressRequestHighPerformance = 0;
    }
}
#else
const Adapters &adapters() {
    static const Adapters a;
    return a;
}
int preference() { return 0; }
bool set_preference(int) { return false; }
void apply_at_startup() {}
#endif

}  // namespace nj::gpu
