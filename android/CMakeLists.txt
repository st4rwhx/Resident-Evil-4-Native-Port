# Coque Android : le meme code que RE4Native, lie en libmain.so pour SDLActivity.
#
# L'export (C:\re4n\export) reste la seule copie compilee du code recompile ; ce fichier ne fait
# que la reprendre avec la chaine du NDK. La difference avec le CMakeLists de l'export tient en
# une ligne : SDL sur Android charge une bibliotheque partagee nommee `main` et y appelle
# `SDL_main`, que <SDL_main.h> substitue a notre `main`. Voir build_apk.sh pour la suite.
cmake_minimum_required(VERSION 3.20.0)
project(RE4Android LANGUAGES C CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(NOT NJ_EXPORT_DIR)
    message(FATAL_ERROR "NJ_EXPORT_DIR non defini (ex. C:/re4n/export)")
endif()

add_compile_options(-Wno-format-security)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# ARMv7 : le NDK compile pour VFPv3, sans fusion multiplication-addition. Chaque fmadd/ps_madd du
# Gekko devenait un appel a fma() de libm, emule en logiciel (frexp, ldexp, fesetround) : 9 % du
# fil CPU au profil du Galaxy S5. VFPv4 (vfma.f64) est present sur tout coeur ARMv7 depuis le
# Cortex-A7/A15 et le Krait, c'est-a-dire tout telephone Android 7+.
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    add_compile_options(-mfpu=neon-vfpv4)
endif()

# Reprendre les sources deja telechargees par la construction Windows : pas de clone a chaque
# configuration, et les memes versions exactement.
set(NJ_DEPS_DIR "${NJ_EXPORT_DIR}/build/_deps" CACHE PATH "Sources deja telechargees (SDL, toml++)")
set(FETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS "${NJ_DEPS_DIR}/tomlplusplus-src" CACHE PATH "")
set(FETCHCONTENT_SOURCE_DIR_SDL2 "${NJ_DEPS_DIR}/sdl2-src" CACHE PATH "")

include(FetchContent) # dependances recuperees a la configuration
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus
    GIT_TAG        v3.4.0)


# SDLActivity charge libSDL2.so par son nom : la version partagee est obligatoire sur Android.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "SDL2 n'est pas installe" FORCE)
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG        release-2.28.5)
FetchContent_MakeAvailable(tomlplusplus SDL2)

set(NJ_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../runtime" CACHE PATH "Chemin vers runtime")
if(EXISTS "${NJ_RUNTIME_DIR}/CMakeLists.txt")
    add_subdirectory("${NJ_RUNTIME_DIR}" runtime)
else()
    set(NJ_RUNTIME_DIR "${NJ_EXPORT_DIR}/runtime")
    add_subdirectory("${NJ_RUNTIME_DIR}" runtime)
endif()

if(NJ_NATIVE_ONLY)
    # Variante moteur natif : aucun code recompile, le noyau (libre4_*.so) fait tourner le jeu.
    add_library(main SHARED
        "${NJ_RUNTIME_DIR}/src/core/main.cpp"
        native_stubs.cpp
        android_glue.cpp)
    target_include_directories(main PRIVATE "${NJ_RUNTIME_DIR}/src" "${NJ_RUNTIME_DIR}/include")
    target_compile_definitions(main PRIVATE NJ_NATIVE_ONLY)
elseif(NJ_P3_JIT)
    # P3 : moteur seul, aucun code du jeu ; le telephone traduit son disque au premier lancement
    # (docs/P3_TRADUCTEUR.md). NJ_EXPORT_DIR = projet du moteur seul (C:/re4n/nj-p3), LLVM et Clang
    # de l'ABI (portable : C:/re4n/llvm/build-android-<abi>, llvm_android.sh).
    add_library(main SHARED
        "${NJ_RUNTIME_DIR}/src/core/main.cpp"
        "${NJ_EXPORT_DIR}/runtime/src/p3/p3_moteur_seul.cpp"
        android_glue.cpp)
    target_include_directories(main PRIVATE
        "${NJ_RUNTIME_DIR}/src" "${NJ_RUNTIME_DIR}/include" "${NJ_EXPORT_DIR}")
elseif(NJ_PRECOMPILED_MULTI)
    target_compile_definitions(njruntime PRIVATE NJ_PRECOMPILED_MULTI=1)
    # 0.1.8 multi-region : un seul APK pour USA et Europe
    set(NJ_USA_DIR "C:/re4n/nj-usa" CACHE PATH "Sources precompilees USA")
    set(NJ_EUR_DIR "C:/re4n/nj-eur" CACHE PATH "Sources precompilees Europe")
    set(NJ_COMPAT_DEFS
        inst_count=retired is_running=alive exception_jmp_buf=exc_jump
        callback_depth=cb_nesting in_callback=inside_cb
        nwii_case_MemCreateHeap=nj_case_MemCreateHeap
        nwii_char_gameRoomInit=nj_char_gameRoomInit
        nwii_dvd_CompareDiskID=nj_dvd_CompareDiskID
        nwii_wii_PlWepHitCheck2=nj_wii_PlWepHitCheck2
        nwii_wii_PlWepLockCtrl=nj_wii_PlWepLockCtrl
        nwii_wii_drawLaserSight=nj_wii_drawLaserSight
        nwii_wii_getMarkerPos=nj_wii_getMarkerPos
        nwii_wii_getPitch=nj_wii_getPitch
    )
    if(NJ_RELEASE)
        list(APPEND NJ_COMPAT_DEFS NJ_RELEASE=1)
    endif()

    file(GLOB NJ_USA_SOURCES "${NJ_USA_DIR}/output_*.cpp")
    add_library(nj_game_usa OBJECT "${NJ_USA_DIR}/main_output.cpp" ${NJ_USA_SOURCES})
    target_include_directories(nj_game_usa PRIVATE "${NJ_RUNTIME_DIR}/src" "${NJ_RUNTIME_DIR}/include" "${NJ_USA_DIR}")
    target_compile_definitions(nj_game_usa PRIVATE
        g=g_usa run_game=run_game_usa nwii_dispatch_one=nj_dispatch_one_usa
        g_nj_dol_fnv=g_nj_dol_fnv_usa g_rel_aot_modules=g_rel_aot_modules_usa
        g_rel_aot_count=g_rel_aot_count_usa
        GetWepTargetPos=GetWepTargetPos_usa PlWepLockRand=PlWepLockRand_usa
        cMot3__move=cMot3__move_usa joyKamae=joyKamae_usa joyLKamae=joyLKamae_usa
        PlayerInit=PlayerInit_usa cItemMgr__clear=cItemMgr__clear_usa
        cItemMgr__set_char=cItemMgr__set_char_usa
        ${NJ_COMPAT_DEFS})

    file(GLOB NJ_EUR_SOURCES "${NJ_EUR_DIR}/output_*.cpp")
    add_library(nj_game_eur OBJECT "${NJ_EUR_DIR}/main_output.cpp" ${NJ_EUR_SOURCES})
    target_include_directories(nj_game_eur PRIVATE "${NJ_RUNTIME_DIR}/src" "${NJ_RUNTIME_DIR}/include" "${NJ_EUR_DIR}")
    target_compile_definitions(nj_game_eur PRIVATE
        g=g_eur run_game=run_game_eur nwii_dispatch_one=nj_dispatch_one_eur
        g_nj_dol_fnv=g_nj_dol_fnv_eur g_rel_aot_modules=g_rel_aot_modules_eur
        g_rel_aot_count=g_rel_aot_count_eur
        GetWepTargetPos=GetWepTargetPos_eur PlWepLockRand=PlWepLockRand_eur
        cMot3__move=cMot3__move_eur joyKamae=joyKamae_eur joyLKamae=joyLKamae_eur
        PlayerInit=PlayerInit_eur cItemMgr__clear=cItemMgr__clear_eur
        cItemMgr__set_char=cItemMgr__set_char_eur
        ${NJ_COMPAT_DEFS})

    add_library(main SHARED
        "${NJ_RUNTIME_DIR}/src/core/main.cpp"
        "${NJ_RUNTIME_DIR}/src/precomp/precomp_dispatch.cpp"
        android_glue.cpp)
    target_include_directories(main PRIVATE
        "${NJ_RUNTIME_DIR}/src" "${NJ_RUNTIME_DIR}/include")
    target_link_libraries(main PRIVATE nj_game_usa nj_game_eur)
else()
    file(GLOB NJ_RECOMP_SOURCES "${NJ_EXPORT_DIR}/output_*.cpp")
    add_library(main SHARED
        "${NJ_RUNTIME_DIR}/src/core/main.cpp"
        "${NJ_EXPORT_DIR}/main_output.cpp"
        android_glue.cpp
        ${NJ_RECOMP_SOURCES})
    target_include_directories(main PRIVATE
        "${NJ_RUNTIME_DIR}/src" "${NJ_RUNTIME_DIR}/include" "${NJ_EXPORT_DIR}")
endif()
target_link_libraries(main PRIVATE njruntime)

# -Oz retirerait 38 % du code machine (PORTAGE_ARM64.md), mais le processeur invite tourne
# entierement dans ce code : on garde -O2, comme sous Windows, tant que la cadence n'est pas
# mesuree sur l'appareil. NJ_RECOMP_OZ=ON pour l'essayer.
# Mesures du Galaxy S5 (29/09, profileur NJ_APROF) :
# - appels internes a la bibliotheque par la table .plt (fonctions interposables d'une
#   bibliotheque partagee) : ~7 % du fil du jeu. -Bsymbolic-functions les rend directs ;
# - divisions par une fonction logicielle (__udivmodsi4) : le Krait (Galaxy S5) a la division
#   materielle, -mcpu=cortex-a15 l'utilise (NJ_ARMV7_CPU=generic : ARMv7 generique, sans elle) ;
# - verification de pile (-fstack-protector-strong) sur le code recompile : retiree.
target_link_options(main PRIVATE "-Wl,-Bsymbolic-functions")
# cortex-a15 et non krait : sous le NDK r26d, -mcpu=krait n'active pas la division materielle en
# Thumb (aucune instruction udiv dans la bibliotheque, 29/09) ; le Krait l'a (idiva idivt).
set(NJ_ARMV7_CPU "cortex-a15" CACHE STRING "processeur ARMv7 vise (cortex-a15, krait, generic)")
if(ANDROID_ABI STREQUAL "armeabi-v7a" AND NOT NJ_ARMV7_CPU STREQUAL "generic")
    # -march=armv7ve en plus : le NDK passe -march=armv7-a, qui l'emporte sur -mcpu pour les
    # extensions ; sans lui, aucune instruction udiv/sdiv dans la bibliotheque (29/09).
    target_compile_options(main PRIVATE "-mcpu=${NJ_ARMV7_CPU}" "-march=armv7ve")
    target_compile_options(njruntime PRIVATE "-mcpu=${NJ_ARMV7_CPU}" "-march=armv7ve")
endif()
# Gardes des statiques locales sans barriere : sur ARM chaque lecture d'une statique locale
# (interrupteurs NJ_ENV, drapeaux lus une fois) faisait un dmb, des centaines de milliers de fois
# par seconde dans drain_callbacks et le rendu. Les singletons partages entre fils sont
# construits par main.cpp avant la creation des fils (aucune premiere initialisation concurrente).
# Adresse espionnee (NJ_WATCH, debogage) retiree des chemins rapides d'ecriture memoire : deux
# chargements par la GOT a chaque ecriture du jeu (runtime/cpu_context.h, NJ_WATCHING).
target_compile_definitions(main PRIVATE NJ_NO_WATCH)
target_compile_definitions(njruntime PRIVATE NJ_NO_WATCH)
option(NJ_TS_STATICS "Garder les gardes thread-safe des statiques locales" OFF)
if(NOT NJ_TS_STATICS)
    target_compile_options(njruntime PRIVATE "-fno-threadsafe-statics")
    # Sauf les fichiers aux textes chiffres "texte"_g (runtime/obf.h, 12/10) : une statique locale
    # par texte, dechiffree a la premiere lecture, et lue par plusieurs fils (installation P3,
    # rendu, journal). Sans garde, deux fils la dechiffrent en meme temps : memoire corrompue.
    file(GLOB_RECURSE _nj_src_all "${NJ_EXPORT_DIR}/runtime/src/*.cpp")
    if(NJ_P3_JIT AND NJ_RECOMP_SRC)
        file(GLOB _nj_recomp_all "${NJ_RECOMP_SRC}/nj_*.cpp")
        list(APPEND _nj_src_all ${_nj_recomp_all})
    endif()
    set(_nj_ts_files "")
    foreach(_f IN LISTS _nj_src_all)
        file(READ "${_f}" _txt)
        if(_txt MATCHES "obf_litteraux")
            list(APPEND _nj_ts_files "${_f}")
        endif()
    endforeach()
    if(_nj_ts_files)
        set_source_files_properties(${_nj_ts_files} PROPERTIES COMPILE_OPTIONS "-fthreadsafe-statics")
    endif()
    list(LENGTH _nj_ts_files _nj_ts_n)
    message(STATUS "Gardes thread-safe (textes chiffres) : ${_nj_ts_n} fichiers")
endif()
set_source_files_properties(${NJ_RECOMP_SOURCES} "${NJ_EXPORT_DIR}/main_output.cpp"
    PROPERTIES COMPILE_OPTIONS "-fno-stack-protector")
option(NJ_RECOMP_OZ "Compiler le code recompile en -Oz" OFF)
if(NJ_RECOMP_OZ)
    set_source_files_properties(${NJ_RECOMP_SOURCES} "${NJ_EXPORT_DIR}/main_output.cpp"
        PROPERTIES COMPILE_OPTIONS "-Oz")
endif()
# Build paranoiaque du fastmem : chaque acces rapide du code genere verifie qu'il tombe en MEM1,
# sinon [FMVIOL] et chemin complet (nj_ops.h, NJ_FASTMEM_CHECK). Zero [FMVIOL] exige avant le build rapide.
option(NJ_FASTMEM_CHECK "Fastmem : controle de chaque acces rapide" OFF)
if(NJ_FASTMEM_CHECK)
    target_compile_definitions(main PRIVATE NJ_FASTMEM_CHECK)
endif()
# Profil des divisions 64 bits logicielles (divprof.cpp) : APK de mesure seulement.
option(NJ_DIVPROF "Compter les appelants de __aeabi_uldivmod / __aeabi_ldivmod" OFF)
if(NJ_DIVPROF AND ANDROID_ABI STREQUAL "armeabi-v7a")
    target_sources(main PRIVATE divprof.cpp)
    target_link_options(main PRIVATE "-Wl,--wrap=__aeabi_uldivmod" "-Wl,--wrap=__aeabi_ldivmod")
endif()

# Build de diffusion : rien a lire dans libmain.so (docs/RELEASE.md, « Durcissement »).
# - symboles caches et table dynamique reduite a SDL_main (exports_release.map) : plus aucun nom de
#   fonction du runtime ni du code recompile ; --exclude-libs pour les bibliotheques statiques ;
# - chemins de compilation retires des chaines (__FILE__, assert) ;
# - sections inutilisees retirees, aucun identifiant de build ;
# Aucun cout a l'execution : les appels internes deviennent directs (comme -Bsymbolic-functions).
if(NJ_RELEASE)
    foreach(t main njruntime)
        target_compile_options(${t} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden
            -ffunction-sections -fdata-sections
            "-ffile-prefix-map=${NJ_EXPORT_DIR}=." "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
            "-fmacro-prefix-map=${NJ_EXPORT_DIR}=." "-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.")
    endforeach()
    # Pas d'informations de type pour le moteur (noms de classes lisibles) : aucun dynamic_cast ni
    # typeid ; les exceptions gardent les leurs. Le code recompile n'a pas de classes.
    # SDL aussi : ses messages d'erreur gardent __FILE__ (chemin des sources sur cette machine).
    target_compile_options(SDL2-static PRIVATE "-ffile-prefix-map=${NJ_EXPORT_DIR}=." "-fmacro-prefix-map=${NJ_EXPORT_DIR}=.")
    target_compile_options(njruntime PRIVATE -fno-rtti)
    set_source_files_properties("${NJ_EXPORT_DIR}/runtime/src/core/main.cpp" PROPERTIES COMPILE_OPTIONS -fno-rtti)
    if(NJ_DEVTOOLS)   # build prive de mesure : symboles gardes (simpleperf resout les fonctions du moteur), jamais diffuse
        target_link_options(main PRIVATE "-Wl,--gc-sections")
    else()
    target_link_options(main PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports_release.map"
        "-Wl,--exclude-libs,libnjruntime.a:libnj_imgui.a:libc++_static.a:libc++abi.a" "-Wl,--gc-sections" "-Wl,--build-id=none" "-Wl,-s")
    endif()
endif()
