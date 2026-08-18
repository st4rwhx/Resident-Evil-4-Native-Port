# Coque iOS (14/10) : le moteur seul (P3, aucun code du jeu), lie en application Nightjar.app pour
# arm64-apple-ios. Construite sur le runner macOS du depot prive nightjar-build (etage 2), avec le
# LLVM iOS de l'etage 1 (LLVM_DIR / Clang_DIR). Le jeu est traduit sur l'iPhone au premier lancement,
# ce qui demande la memoire executable de StikDebug (ios_jit.mm).
#
#   cmake -S ios -B build-ios -G Ninja -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
#         -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DNJ_P3_JIT=ON -DLLVM_DIR=... -DClang_DIR=...
cmake_minimum_required(VERSION 3.24)
project(NightjarIOS LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

get_filename_component(NJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(NJ_RECOMP_SRC "${NJ_ROOT}/recomp/src" CACHE PATH "Sources du traducteur (P3)" FORCE)
if(NOT NJ_P3_PROFILS)
    set(NJ_P3_PROFILS "${NJ_ROOT}/p3profils" CACHE PATH "Profils de version (profil_*.inc)" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(tomlplusplus GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git GIT_TAG v3.4.0)
FetchContent_MakeAvailable(tomlplusplus)
# SDL statique : une application iOS n'embarque pas de bibliotheque partagee a nous.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-2.28.5)
FetchContent_MakeAvailable(SDL2)

add_subdirectory("${NJ_ROOT}/runtime" runtime)

# NJ_PRECOMPILED_DYLIB=ON : le code precompile du jeu (USA / Europe) n'est pas dans ce depot ; il est charge
# au demarrage depuis Frameworks/libnjgame_<version>.dylib, compile sur la machine de l'utilisateur et glisse
# dans l'IPA avant la signature (tools/ios/). Sinon : le moteur seul, traduction au premier lancement (P3).
option(NJ_PRECOMPILED_DYLIB "Charger le jeu precompile depuis Frameworks/*.dylib" OFF)
# NJ_NATIVE_CORE=ON (18/11, port natif complet sur iPhone) : le coeur natif (decompilation, re4-decomp) est lie DANS
# l'appli et tourne dans un fil de ce processus (port/njlink.cpp en transport « meme processus », runtime/src/native/
# native_core.cpp branche iOS). NJ_NATIVE_LIB = libnjnatif.a de port/outils/macho_lien.py ; NJ_NATIVE_SRC = racine de
# re4-decomp (pour le pont port/njgfx/njgfx.cpp, compile avec les en-tetes du moteur). Build local, rien sur GitHub.
option(NJ_NATIVE_CORE "Coeur natif lie dans l'appli (port natif iOS)" OFF)
set(NJ_NATIVE_LIB "" CACHE FILEPATH "libnjnatif.a (port/outils/macho_lien.py)")
set(NJ_NATIVE_SRC "" CACHE PATH "racine de re4-decomp")
if(NJ_NATIVE_CORE)
    target_compile_definitions(njruntime PRIVATE NJ_NATIVE_ONLY=1)
    add_executable(Nightjar MACOSX_BUNDLE
        "${NJ_ROOT}/runtime/src/core/main.cpp"
        "${NJ_ROOT}/runtime/src/p3/p3_moteur_seul.cpp"
        "${NJ_NATIVE_SRC}/port/njgfx/njgfx.cpp")
    target_compile_definitions(Nightjar PRIVATE NJ_NATIVE_ONLY=1 NJ_MOBILE=1 NJ_NO_WATCH=1)
    target_include_directories(Nightjar PRIVATE "${NJ_NATIVE_SRC}/port/njgfx" "${NJ_ROOT}/runtime/third_party/zstd")
    # tout le coeur : rien dans le moteur ne reference ses fonctions avant l'execution (point d'entree nj_core_main)
    target_link_options(Nightjar PRIVATE "-Wl,-force_load,${NJ_NATIVE_LIB}")
elseif(NJ_PRECOMPILED_DYLIB)
    target_compile_definitions(njruntime PRIVATE NJ_PRECOMPILED_MULTI=1)
    add_executable(Nightjar MACOSX_BUNDLE
        "${NJ_ROOT}/runtime/src/core/main.cpp"
        "${NJ_ROOT}/runtime/src/precomp/precomp_dylib.cpp")
    # le dylib du jeu retrouve les fonctions du moteur par leur nom : l'executable les exporte
    target_link_options(Nightjar PRIVATE "-Wl,-export_dynamic")
    # le dylib du jeu appelle des fonctions du moteur (GX*, PAD*, modules HLE) que rien dans l'executable ne
    # reference : sans -force_load l'editeur de liens ne les prend pas dans la bibliotheque statique
    target_link_options(Nightjar PRIVATE "-Wl,-force_load,$<TARGET_FILE:njruntime>")
else()
    add_executable(Nightjar MACOSX_BUNDLE
        "${NJ_ROOT}/runtime/src/core/main.cpp"
        "${NJ_ROOT}/runtime/src/p3/p3_moteur_seul.cpp")
endif()
# icone de l'appli : PNG a la racine du paquet (voir Info.plist.in)
file(GLOB NJ_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/icons/*.png")
target_sources(Nightjar PRIVATE ${NJ_ICONS})
set_source_files_properties(${NJ_ICONS} PROPERTIES MACOSX_PACKAGE_LOCATION "")
target_include_directories(Nightjar PRIVATE "${NJ_ROOT}/runtime/src" "${NJ_ROOT}/runtime/include")
# SDL2main fournit le vrai main() d'iOS (UIApplicationMain), qui appelle notre main renomme SDL_main.
target_link_libraries(Nightjar PRIVATE njruntime SDL2::SDL2main SDL2::SDL2-static)

foreach(fw Foundation UIKit OpenGLES QuartzCore CoreGraphics CoreMotion CoreHaptics GameController
           AVFoundation AudioToolbox CoreAudio Metal CoreBluetooth CoreVideo IOKit)
    find_library(NJ_FW_${fw} ${fw})
    if(NJ_FW_${fw})
        target_link_libraries(Nightjar PRIVATE ${NJ_FW_${fw}})
    endif()
endforeach()

set_target_properties(Nightjar PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.ayano.nightjar"
    MACOSX_BUNDLE_BUNDLE_NAME "Nightjar"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.9"
    MACOSX_BUNDLE_BUNDLE_VERSION "11")
