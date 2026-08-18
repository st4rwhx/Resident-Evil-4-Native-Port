// P3 : table des symboles du moteur dont le code genere a besoin (docs/P3_TRADUCTEUR.md, « table de
// numeros »). Recensement du 06/10 sur les 130 fichiers generes (nm -u) : 52 symboles. Le code
// compile chez le joueur n'est relie qu'a cette table ; rien d'autre du moteur ne lui est visible.
// Les noms sont ceux de l'ABI C++ (Itanium, communs a MinGW et Android) ; seule leur empreinte
// reste dans l'executable (p3_table.h).
#include <cmath>
#include <cstdlib>
#include <cxxabi.h>
#include <chrono>

#include "runtime/cpu_context.h"
#include "runtime/host_clock.h"
#include "runtime/rel_aot.h"
#include "p3_table.h"
#include "runtime/obf.h"
#include "runtime/diag.h"
using namespace nj::obf_litteraux;   // "texte" : chiffre dans le binaire

using nj::runtime::GuestCpu;
using GuestFn = void (*)(GuestCpu &);

// fonctions natives du moteur qui remplacent celles du jeu (njrecomp hle_natives)
extern "C" {
void DCFlushRangeNoSync(GuestCpu &);
void DCStoreRangeNoSync(GuestCpu &);
void GXLoadNrmMtxImm(GuestCpu &);
void GXLoadPosMtxImm(GuestCpu &);
void GXSetAlphaCompare(GuestCpu &);
void GXSetDstAlpha(GuestCpu &);
void GXSetScissor(GuestCpu &);
void GXSetZMode(GuestCpu &);
void OSReport(GuestCpu &);
void PADInit(GuestCpu &);
void PADRead(GuestCpu &);
void VIWaitForRetrace(GuestCpu &);
}
// enveloppes (njrecomp hle_wraps)
void nj_wii_drawLaserSight(GuestCpu &, GuestFn);
void nj_wii_PlWepHitCheck2(GuestCpu &, GuestFn);
void nj_case_MemCreateHeap(GuestCpu &, GuestFn);
void nj_wii_PlWepLockCtrl(GuestCpu &, GuestFn);
void nj_dvd_CompareDiskID(GuestCpu &, GuestFn);
void nj_char_gameInit(GuestCpu &, GuestFn);
void nj_char_gameRoomInit(GuestCpu &, GuestFn);
void nj_wii_getMarkerPos(GuestCpu &, GuestFn);
void nj_wii_getPitch(GuestCpu &, GuestFn);
uint32_t nj_efb_peek_read(uint32_t);
#if defined(_WIN32) && defined(__x86_64__)
extern "C" void ___chkstk_ms();   // sonde de pile de MinGW (cadres > 4 Ko, code non optimise)
#endif
// Deroulement des exceptions C++ (libgcc / libstdc++). En developpement le JIT les trouvait dans les
// DLL de MinGW ; le build de diffusion est lie en statique : sans elles dans la table, aucun lot ne
// se liait (06/10, jeu ferme apres la preparation).
extern "C" void _Unwind_Resume(void *);
#if defined(_WIN32)
extern "C" int __gxx_personality_seh0(void *, void *, void *, void *);
#else
extern "C" int __gxx_personality_v0(int, int, unsigned long long, void *, void *);
#endif

namespace nj::runtime::p3 {

#define F(name, fn) {name_hash_ct(name), reinterpret_cast<void *>(fn)}
const Symbol kTable[] = {
    // moteur
    F("_ZN2nj7runtime8on_watchEjji", &nj::runtime::on_watch),
    F("_ZN2nj7runtime18unmapped_write_hitEjji", &nj::runtime::unmapped_write_hit),
    F("_ZN2nj7runtime16GX_WGPIPE_Write8Eh", &nj::runtime::GX_WGPIPE_Write8),
    F("_ZN2nj7runtime17GX_WGPIPE_Write16Et", &nj::runtime::GX_WGPIPE_Write16),
    F("_ZN2nj7runtime17GX_WGPIPE_Write32Ej", &nj::runtime::GX_WGPIPE_Write32),
    F("_ZN2nj7runtime13gx_wgpipe_f32Ef", &nj::runtime::gx_wgpipe_f32),
    F("_ZN2nj7runtime13gx_wgpipe_f64Ed", &nj::runtime::gx_wgpipe_f64),
    F("_ZN2nj7runtime12hwreg_read16Ej", &nj::runtime::hwreg_read16),
    F("_ZN2nj7runtime12hwreg_read32Ej", &nj::runtime::hwreg_read32),
    F("_ZN2nj7runtime13hwreg_write16Ejt", &nj::runtime::hwreg_write16),
    F("_ZN2nj7runtime13hwreg_write32Ejj", &nj::runtime::hwreg_write32),
    F("_ZN2nj7runtime14nj_budget_slowERNS0_8GuestCpuEj", &nj::runtime::nj_budget_slow),
    F("_ZN2nj7runtime14interp_advanceERNS0_8GuestCpuE", &nj::runtime::interp_advance),
    F("_ZN2nj7runtime13interp_singleERNS0_8GuestCpuEjj", &nj::runtime::interp_single),
    F("_ZN2nj7runtime15service_syscallERNS0_8GuestCpuE", &nj::runtime::service_syscall),
    F("_ZN2nj7runtime15drain_callbacksERNS0_8GuestCpuE", &nj::runtime::drain_callbacks),
    F("_ZN2nj7runtime18hle_dispatch_leaveERNS0_8GuestCpuE", &nj::runtime::hle_dispatch_leave),
    F("_ZN2nj7runtime13log_null_callEjj", &nj::runtime::log_null_call),
    F("_Z16nj_efb_peek_readj", &nj_efb_peek_read),
    F("_ZN2nj7runtime12g_watch_addrE", &nj::runtime::g_watch_addr),
    F("_ZN2nj7runtime11g_rel_deltaE", &nj::runtime::g_rel_delta),
    F("_ZN2nj7runtime8g_nj_wgpE", &nj::runtime::g_nj_wgp),
    F("_ZN2nj11g_tsc_clockE", &nj::g_tsc_clock),
    {kSlotTableHash, nullptr},   // table des cases enregistree (p3_jit.cpp, dispatch_tables)
    // natives
    F("DCFlushRangeNoSync", &DCFlushRangeNoSync),
    F("DCStoreRangeNoSync", &DCStoreRangeNoSync),
    F("GXLoadNrmMtxImm", &GXLoadNrmMtxImm),
    F("GXLoadPosMtxImm", &GXLoadPosMtxImm),
    F("GXSetAlphaCompare", &GXSetAlphaCompare),
    F("GXSetDstAlpha", &GXSetDstAlpha),
    F("GXSetScissor", &GXSetScissor),
    F("GXSetZMode", &GXSetZMode),
    F("OSReport", &OSReport),
    F("PADInit", &PADInit),
    F("PADRead", &PADRead),
    F("VIWaitForRetrace", &VIWaitForRetrace),
    // enveloppes
    F("_Z21nj_wii_drawLaserSightRN2nj7runtime8GuestCpuEPFvS2_E", &nj_wii_drawLaserSight),
    F("_Z21nj_wii_PlWepHitCheck2RN2nj7runtime8GuestCpuEPFvS2_E", &nj_wii_PlWepHitCheck2),
    F("_Z21nj_case_MemCreateHeapRN2nj7runtime8GuestCpuEPFvS2_E", &nj_case_MemCreateHeap),
    F("_Z20nj_wii_PlWepLockCtrlRN2nj7runtime8GuestCpuEPFvS2_E", &nj_wii_PlWepLockCtrl),
    F("_Z20nj_dvd_CompareDiskIDRN2nj7runtime8GuestCpuEPFvS2_E", &nj_dvd_CompareDiskID),
    F("_Z20nj_char_gameRoomInitRN2nj7runtime8GuestCpuEPFvS2_E", &nj_char_gameRoomInit),
    F("_Z16nj_char_gameInitRN2nj7runtime8GuestCpuEPFvS2_E", &nj_char_gameInit),
    F("_Z19nj_wii_getMarkerPosRN2nj7runtime8GuestCpuEPFvS2_E", &nj_wii_getMarkerPos),
    F("_Z15nj_wii_getPitchRN2nj7runtime8GuestCpuEPFvS2_E", &nj_wii_getPitch),
    // C et C++ standard
    F("fma", static_cast<double (*)(double, double, double)>(&std::fma)),
    F("sqrt", static_cast<double (*)(double)>(&std::sqrt)),
    F("nearbyint", static_cast<double (*)(double)>(&std::nearbyint)),
    F("nearbyintf", static_cast<float (*)(float)>(&std::nearbyint)),
    F("ldexpf", static_cast<float (*)(float, int)>(&std::ldexp)),
#if defined(_WIN32) && defined(__x86_64__)
    F("___chkstk_ms", &___chkstk_ms),
#endif
    F("getenv", &std::getenv),
    F("__cxa_guard_acquire", &__cxxabiv1::__cxa_guard_acquire),
    F("__cxa_guard_release", &__cxxabiv1::__cxa_guard_release),
    F("__cxa_guard_abort", &__cxxabiv1::__cxa_guard_abort),
#if defined(__GLIBCXX__)
    // controles de la bibliotheque standard, actifs dans le code non optimise (-O0) de GCC 15+
    F("_ZSt21__glibcxx_assert_failPKciS0_S0_",
      static_cast<void (*)(const char *, int, const char *, const char *) noexcept>(&std::__glibcxx_assert_fail)),
#endif
    F("_Unwind_Resume", &_Unwind_Resume),
#if defined(_WIN32)
    F("__gxx_personality_seh0", &__gxx_personality_seh0),
#else
    F("__gxx_personality_v0", &__gxx_personality_v0),
#endif
#if defined(_LIBCPP_VERSION)   // Android (libc++ du NDK)
    F("_ZNSt6__ndk16chrono12steady_clock3nowEv", &std::chrono::steady_clock::now),
#else
    F("_ZNSt6chrono3_V212steady_clock3nowEv", &std::chrono::steady_clock::now),
#endif
};
#undef F
const unsigned kTableSize = sizeof(kTable) / sizeof(kTable[0]);

}  // namespace nj::runtime::p3
