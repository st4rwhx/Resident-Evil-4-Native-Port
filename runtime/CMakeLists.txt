cmake_minimum_required(VERSION 3.20.0)

# Cibles mobiles. Android et iOS rendent en OpenGL ES 3.0 : pas de chargeur a generer (les
# symboles ES sont lies directement, voir include/common/gl.h) et pas de bloc Cocoa, qui est
# macOS et non iOS. Tout le reste du runtime est deja portable - l'audit du 2026-09-20
# (tools/arm64_audit.py) ne trouve aucune dependance a l'architecture dans les 109 Mo de C++
# recompile, ni dans le runtime hors les trois points corriges ce jour-la.
if(ANDROID OR IOS)
    set(NJ_MOBILE ON)
endif()

include(FetchContent) # glad, recupere a la configuration

# Chargeur OpenGL 3.3 core pour le PC (glad 0.1.36, sortie C) ; les mobiles lient GLES sans
# chargeur. La tour et le portable donnent la copie deja telechargee
# (-DFETCHCONTENT_SOURCE_DIR_GLAD=...).
if(NOT NJ_MOBILE)
    set(GLAD_PROFILE core CACHE STRING "" FORCE)
    set(GLAD_API gl=3.3 CACHE STRING "" FORCE)
    set(GLAD_GENERATOR c CACHE STRING "" FORCE)
    FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git GIT_TAG v0.1.36)
    FetchContent_MakeAvailable(glad) # chargeur GL du bureau
endif()

# Sources du moteur, module par module : nj_module(<dossier sous src> <fichiers sans .cpp>).
set(NJ_SOURCES third_party/zstd/zstddeclib.c)
macro(nj_module dir)
    foreach(nj_f ${ARGN})
        list(APPEND NJ_SOURCES src/${dir}/${nj_f}.cpp)
    endforeach()
endmacro()
nj_module(boot program)
nj_module(core alloc_prof build_tag config console_boot crash_report dev_tour disc_image dispatch game_version gpu_select guest_ram guest_services host_clock host_paths host_video live_frame mods njprof perf_journal present_area rel_aot report saves slow_sites thread_place timers)
nj_module(input bindings input_manager input_setup mouse_events mouse_turn)
nj_module(input/sources autostart_source debug_menu_skip_source gamepad_source mouse_keyboard_source remote_source script_source)
nj_module(input/touch icon_gen icon_render re4_context touch_overlay touch_source)
nj_module(hle ax case_models char_select fallback_cpu gx krauser_mod os_dispatch pad texture_cache_gl virtual_disc wii_aim)
nj_module(native native_core disc_extract)
nj_module(hle/gx chunk_adapter efb_peek fifo_parser fifo_record frame_interp gx_direct renderer_gl shader_key tev_shader_gen tex_decode)
nj_module(hw hw_ai hw_all hw_cp hw_di hw_dsp hw_exi hw_pe hw_pi hw_si hw_vi io_bus)
nj_module(launcher disc discord_auth host i18n launcher launcher_fx previews rmenu settings_menu ui_fonts ui_skin)
# Brouillage : la connexion Discord est compilee a part par Polaris (plus bas), comme le traducteur.
if(NJ_OBFUSCATION)
    list(REMOVE_ITEM NJ_SOURCES src/launcher/discord_auth.cpp)
endif()
# Noyau natif en processus separe (ARM 32 bits Android) : client de l'interface re4ipc_*, provisoire
# (docs/BRANCHEMENT_NOYAU_NATIF) jusqu'a la reecriture du lot L3.
# (le noyau CubeEvil4 et son client re4ipc ne sont plus utilises : notre coeur parle njlink, native_core.cpp)
add_library(njruntime STATIC ${NJ_SOURCES})

# Lanceur (version, accueil, disque) : Dear ImGui, livre dans third_party/imgui (licence MIT).
add_library(nj_imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl2.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(nj_imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
target_link_libraries(nj_imgui PUBLIC SDL2-static)
if(NJ_MOBILE)
    target_compile_definitions(nj_imgui PRIVATE IMGUI_IMPL_OPENGL_ES3)
endif()

target_link_libraries(njruntime PUBLIC SDL2-static tomlplusplus::tomlplusplus nj_imgui)
# Build de diffusion : le lanceur (verification de version, disque) devient obligatoire.
# Filigrane de la beta fermee (src/core/build_tag.cpp) : un build par testeur. Etiquette = 16 premiers
# octets de SHA-256(secret en hexadecimal + nom) ; le secret et la liste des testeurs restent sur la
# tour (tools/p3/filigrane.py).
set(NIGHTJAR_TESTER "" CACHE STRING "Testeur de la beta fermee pour qui ce build est marque")
set(NIGHTJAR_FILIGRANE_SECRET "C:/re4n/filigrane/secret.bin" CACHE FILEPATH "Secret du filigrane")
if(NIGHTJAR_TESTER)
    file(READ "${NIGHTJAR_FILIGRANE_SECRET}" NJ_SECRET_HEX HEX)
    if(NOT NJ_SECRET_HEX)
        message(FATAL_ERROR "secret du filigrane absent : ${NIGHTJAR_FILIGRANE_SECRET}")
    endif()
    string(SHA256 NJ_TAG_HASH "${NJ_SECRET_HEX}${NIGHTJAR_TESTER}")
    string(SUBSTRING "${NJ_TAG_HASH}" 0 32 NJ_TAG_HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," NJ_TAG_BYTES "${NJ_TAG_HEX}")
    set_source_files_properties(src/core/build_tag.cpp PROPERTIES COMPILE_DEFINITIONS "NJ_BUILD_TAG=${NJ_TAG_BYTES}")
endif()
option(NJ_RELEASE "Build de diffusion" OFF)
if(NJ_RELEASE)
    target_compile_definitions(njruntime PUBLIC NJ_RELEASE=1)
endif()
# P3, jalon 1 (docs/P3_TRADUCTEUR.md) : compilation en memoire pendant la partie (src/p3/p3_jit.cpp),
# LLVM + Clang lies au moteur. Le code charge n'est relie qu'a la table src/p3/p3_table.cpp.
#   -DNJ_P3_JIT=ON -DLLVM_DIR=<build>/lib/cmake/llvm -DClang_DIR=<build>/lib/cmake/clang
# Exceptions de la console (setjmp / longjmp de exc_jump) sans deroulement de pile Windows :
# le code compile en memoire (P3) n'a pas d'informations de deroulement (0xC0000028 sinon, 06/10).
if(MINGW)
    target_compile_definitions(njruntime PUBLIC __USE_MINGW_SETJMP_NON_SEH)
endif()
# --- Brouillage du traducteur (NOTRE PI) par Polaris-Clang, en objets normaux (hors LTO) ---
# docs/SECURITE_ET_JURIDIQUE.md 3 ter. OFF par defaut : build ordinaire/dev inchange. Ces fichiers
# sont compiles par notre Clang (objets COFF x86_64-w64-windows-gnu, meme ABI/SEH que GCC) puis lies
# par g++ avec le reste du moteur en GCC+LTO. Pas de LTO Clang (incompatible avec la LTO GCC du lien).
option(NJ_OBFUSCATION "Brouiller le traducteur (njrecomp + gestionnaire P3) avec Polaris-Clang" OFF)
if(NJ_OBFUSCATION)
    set(NJ_POLARIS_CLANG "C:/re4n/polaris23/bin/clang.exe" CACHE FILEPATH "Clang-Polaris (brouillage)")
    set(NJ_POLARIS_RESDIR "C:/re4n/polaris23/lib/clang/23" CACHE PATH "resource-dir de Polaris-Clang")
    # fla ecarte (miscompile / plante le backend sur grosses fonctions) : sub,bcf,mba seulement.
    set(NJ_OBF_PASSES "sub,bcf,mba" CACHE STRING "Passes Polaris")
    get_filename_component(_nj_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)        # .../mingw64/bin
    get_filename_component(NJ_MINGW_SYSROOT "${_nj_bin}" DIRECTORY)        # .../mingw64
    # Memes defines que njruntime : NJ_RELEASE seulement en diffusion (sinon main.cpp coupe les
    # journaux), __USE_MINGW_SETJMP_NON_SEH sous MinGW, SDL_MAIN_HANDLED toujours.
    set(_nj_obf_defs -DSDL_MAIN_HANDLED)
    if(MINGW)
        list(APPEND _nj_obf_defs -D__USE_MINGW_SETJMP_NON_SEH)
    endif()
    if(NJ_RELEASE)
        list(APPEND _nj_obf_defs -DNJ_RELEASE=1)
    endif()
    # Memes dirs d'inclusion que njruntime (propres + interfaces des dependances).
    set(_nj_obf_incs
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zstd
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        $<TARGET_PROPERTY:SDL2-static,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:tomlplusplus::tomlplusplus,INTERFACE_INCLUDE_DIRECTORIES>)
    if(TARGET glad)
        list(APPEND _nj_obf_incs $<TARGET_PROPERTY:glad,INTERFACE_INCLUDE_DIRECTORIES>)
    endif()
    # Cible des objets brouilles. PC : COFF MinGW. Android (13/10) : ELF de l'ABI, sysroot et libc++
    # du NDK, memes definitions que njruntime (NJ_MOBILE, NJ_NO_WATCH...) pour garder les memes
    # structures des deux cotes.
    if(ANDROID)
        set(_nj_obf_triple "${CMAKE_CXX_COMPILER_TARGET}")
        if(NOT _nj_obf_triple)
            if(ANDROID_ABI STREQUAL "armeabi-v7a")
                set(_nj_obf_triple "armv7-none-linux-androideabi24")
            elseif(ANDROID_ABI STREQUAL "x86_64")
                set(_nj_obf_triple "x86_64-none-linux-android24")
            else()
                set(_nj_obf_triple "aarch64-none-linux-android24")
            endif()
        endif()
        set(_nj_obf_sysroot "${CMAKE_SYSROOT}")
        if(NOT _nj_obf_sysroot)
            set(_nj_obf_sysroot "${ANDROID_TOOLCHAIN_ROOT}/sysroot")
        endif()
        set(_nj_obf_target --target=${_nj_obf_triple} "--sysroot=${_nj_obf_sysroot}" -stdlib=libc++
            -fPIC -ffunction-sections -fdata-sections -DANDROID)
        # un seul argument entre guillemets (le ; de JOIN ne doit pas couper l'expression)
        set(_nj_obf_rtdefs "$<$<BOOL:$<TARGET_PROPERTY:njruntime,COMPILE_DEFINITIONS>>:-D$<JOIN:$<TARGET_PROPERTY:njruntime,COMPILE_DEFINITIONS>,;-D>>")
        if(ANDROID_ABI STREQUAL "armeabi-v7a")
            list(APPEND _nj_obf_target -mfpu=neon-vfpv4)
        endif()
        set(_nj_obf_ext o)
    else()
        set(_nj_obf_target --target=x86_64-w64-windows-gnu "--sysroot=${NJ_MINGW_SYSROOT}")
        set(_nj_obf_rtdefs -DNJ_OBF=1)
        set(_nj_obf_ext obj)
    endif()
    # Brouille un .cpp en un objet normal ; renvoie le chemin de l'objet dans out_var.
    find_program(NJ_BASH bash HINTS "C:/Program Files/Git/bin" "C:/Program Files/Git/usr/bin")
    # PASSES <liste> (21/10) : transformations propres a un fichier (defaut : NJ_OBF_PASSES).
    function(nj_obfuscate out_var src)
        cmake_parse_arguments(_o "" "PASSES" "" ${ARGN})
        set(_passes ${NJ_OBF_PASSES})
        if(_o_PASSES)
            set(_passes ${_o_PASSES})
        endif()
        get_filename_component(_b "${src}" NAME_WE)
        set(_obj "${CMAKE_CURRENT_BINARY_DIR}/obf_${_b}.${_nj_obf_ext}")
        add_custom_command(OUTPUT "${_obj}"
            COMMAND "${NJ_BASH}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/polaris_retry.sh"
                    "${NJ_POLARIS_CLANG}" ${_nj_obf_target} "${_nj_obf_rtdefs}" -resource-dir "${NJ_POLARIS_RESDIR}"
                    -O2 -DNDEBUG -std=gnu++20 -fno-rtti
                    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=." -ffile-prefix-map=C:/re4n=.
                    ${_nj_obf_defs}
                    "$<$<BOOL:${_nj_obf_incs}>:-I$<JOIN:${_nj_obf_incs},;-I>>"
                    ${_o_UNPARSED_ARGUMENTS}
                    -mllvm -obf-all -mllvm -passes=${_passes}
                    -c "${src}" -o "${_obj}"
            DEPENDS "${src}"
            COMMENT "Polaris (brouillage) : ${_b}"
            COMMAND_EXPAND_LISTS VERBATIM)
        set_source_files_properties("${_obj}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
        set(${out_var} "${_obj}" PARENT_SCOPE)
    endfunction()
    # Connexion Discord et laissez-passer (13/10) : brouilles aussi, pour qu'un saut de la
    # verification ou un laissez-passer fabrique demande un vrai travail.
    nj_obfuscate(_obf_auth ${CMAKE_CURRENT_SOURCE_DIR}/src/launcher/discord_auth.cpp
        -I${CMAKE_CURRENT_SOURCE_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR}/src/launcher)
    target_sources(njruntime PRIVATE ${_obf_auth})
endif()

option(NJ_P3_JIT "Compilation en memoire pendant la partie (P3, developpement)" OFF)
if(NJ_P3_JIT)
    find_package(LLVM REQUIRED CONFIG)
    find_package(Clang REQUIRED CONFIG)
    # p3_jit / p3_table restent en GCC (ils lient les en-tetes LLVM) ; ne sont pas notre traducteur.
    target_sources(njruntime PRIVATE src/p3/p3_jit.cpp src/p3/p3_table.cpp)
    # njrecomp en bibliotheque (nj_lib.h) : le moteur seul traduit le disque (p3_host.cpp, jalon 3)
    set(NJ_RECOMP_SRC "C:/re4n/nightjar/recomp/src" CACHE PATH "Sources de njrecomp (P3)")
    # profils de version et en-tetes precompiles chiffres (tools/p3/gen_profil.py, gen_pch_blob.py),
    # fabriques hors du depot
    set(NJ_P3_PROFILS "C:/re4n/p3profils" CACHE PATH "Profils P3 (profil_<nom>.inc, pch_embarque.inc)")
    if(NJ_OBFUSCATION)
        # Les 6 fichiers du traducteur : compiles brouilles par Polaris, en objets normaux.
        nj_obfuscate(_obf_img  ${NJ_RECOMP_SRC}/nj_image.cpp  -I${NJ_RECOMP_SRC})
        nj_obfuscate(_obf_cfg  ${NJ_RECOMP_SRC}/nj_config.cpp -I${NJ_RECOMP_SRC})
        # 21/10 : le traducteur qui ECRIT le code C++ sans « mba » : un tirage de cette passe a casse le dechiffrement d'un
        # de ses textes (Android x86_64 : 12 octets chiffres a la place de l'indentation, 381 lots refuses par Clang).
        nj_obfuscate(_obf_emit ${NJ_RECOMP_SRC}/nj_emit.cpp   -I${NJ_RECOMP_SRC} PASSES "sub,bcf")
        nj_obfuscate(_obf_gen  ${NJ_RECOMP_SRC}/nj_gen.cpp    -I${NJ_RECOMP_SRC} PASSES "sub,bcf")
        nj_obfuscate(_obf_host ${CMAKE_CURRENT_SOURCE_DIR}/src/p3/p3_host.cpp -I${NJ_RECOMP_SRC} -I${NJ_P3_PROFILS})
        nj_obfuscate(_obf_prof ${CMAKE_CURRENT_SOURCE_DIR}/src/p3/p3_profils.cpp -I${NJ_P3_PROFILS})
        target_sources(njruntime PRIVATE ${_obf_img} ${_obf_cfg} ${_obf_emit} ${_obf_gen} ${_obf_host} ${_obf_prof})
    else()
        target_sources(njruntime PRIVATE src/p3/p3_host.cpp src/p3/p3_profils.cpp
            ${NJ_RECOMP_SRC}/nj_image.cpp ${NJ_RECOMP_SRC}/nj_config.cpp
            ${NJ_RECOMP_SRC}/nj_emit.cpp ${NJ_RECOMP_SRC}/nj_gen.cpp)
        set_source_files_properties(src/p3/p3_host.cpp PROPERTIES INCLUDE_DIRECTORIES "${NJ_RECOMP_SRC};${NJ_P3_PROFILS}")
        set_source_files_properties(src/p3/p3_profils.cpp PROPERTIES INCLUDE_DIRECTORIES "${NJ_P3_PROFILS}")
    endif()
    target_link_libraries(njruntime PUBLIC tomlplusplus::tomlplusplus)
    set_source_files_properties(src/p3/p3_jit.cpp PROPERTIES INCLUDE_DIRECTORIES
        "${LLVM_INCLUDE_DIRS};${CLANG_INCLUDE_DIRS}")
    separate_arguments(NJ_LLVM_DEFS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
    # LLVM sans informations de type (RTTI, cas des builds Android) : les classes qui derivent des
    # siennes (TableGenerator) non plus, sinon leur typeinfo reclame celle de LLVM, absente.
    if(NOT LLVM_ENABLE_RTTI)
        list(APPEND NJ_LLVM_DEFS -fno-rtti)
    endif()
    set_source_files_properties(src/p3/p3_jit.cpp PROPERTIES COMPILE_OPTIONS "${NJ_LLVM_DEFS}")
    llvm_map_components_to_libnames(NJ_LLVM_LIBS ${LLVM_TARGETS_TO_BUILD} OrcJIT ExecutionEngine JITLink Object
        native Passes Core Support MC Target TargetParser)
    target_link_libraries(njruntime PUBLIC clangCodeGen clangFrontend clangDriver clangSerialization clangParse
        clangSema clangAnalysis clangAST clangLex clangBasic ${NJ_LLVM_LIBS})
    if(MINGW)
        target_link_libraries(njruntime PUBLIC psapi version ole32 uuid ws2_32 ntdll)
    endif()
else()
    target_sources(njruntime PRIVATE src/p3/p3_jit_stub.cpp)
endif()
# Outils de developpement (visite automatique des salles, src/core/dev_tour.cpp) : jamais dans une diffusion publique.
option(NJ_DEVTOOLS "Outils de developpement (prives)" OFF)
if(NJ_DEVTOOLS)
    target_compile_definitions(njruntime PUBLIC NJ_DEVTOOLS=1)
endif()
# Releve des sites d'acces lents (src/core/slow_sites.cpp, njrecomp slow_sites = true).
option(NJ_SLOW_SITES "Compter les chemins lents de la memoire par site PPC" OFF)
if(NJ_SLOW_SITES)
    target_compile_definitions(njruntime PUBLIC NJ_SLOW_SITES=1)
endif()
if(NJ_MOBILE)
    target_compile_definitions(njruntime PUBLIC NJ_MOBILE=1)
    if(ANDROID)
        target_link_libraries(njruntime PUBLIC GLESv3 EGL log android)
    else()  # iOS
        # hote du lanceur en Objective-C++ (reseau, connexion Discord, fichiers) : host_ios.mm
        target_sources(njruntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/launcher/host_ios.mm)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/launcher/host_ios.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
        foreach(nj_fw AuthenticationServices UniformTypeIdentifiers Security Foundation)
            find_library(NJ_FW_RT_${nj_fw} ${nj_fw})
            target_link_libraries(njruntime PUBLIC ${NJ_FW_RT_${nj_fw}})
        endforeach()
        find_library(OPENGLES_FRAMEWORK OpenGLES)
        find_library(UIKIT_FRAMEWORK UIKit)
        find_library(QUARTZCORE_FRAMEWORK QuartzCore)
        find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
        find_library(AVFOUNDATION_FRAMEWORK AVFoundation)
        target_link_libraries(njruntime PUBLIC # bibliotheques du systeme
            ${OPENGLES_FRAMEWORK} ${UIKIT_FRAMEWORK} ${QUARTZCORE_FRAMEWORK}
            ${COREGRAPHICS_FRAMEWORK} ${AVFOUNDATION_FRAMEWORK})
    endif()
else()
    target_link_libraries(njruntime PUBLIC glad)
endif()

if(WIN32)
    # Winsock (manette distante), WinHTTP (version a jour), boite de dialogue de fichiers (choix du
    # disque) ; SDL_MAIN_HANDLED : main() reste le point d'entree, sans WinMain.
    target_link_libraries(njruntime PUBLIC ws2_32 winhttp comdlg32 crypt32)
    target_compile_definitions(njruntime PUBLIC SDL_MAIN_HANDLED)
endif()

# include/ : en-tetes publics du moteur.
target_include_directories(njruntime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# zstd (decodeur seul, licence BSD : third_party/zstd/LICENSE) pour les images RVZ.
target_include_directories(njruntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zstd)
target_include_directories(njruntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/core_ipc)
set_source_files_properties(third_party/zstd/zstddeclib.c PROPERTIES COMPILE_OPTIONS "-O2;-DZSTD_DISABLE_ASM")

# Harnais d'oracle de rendu : rejoue un log FIFO de Dolphin (.dff) dans le
# meme pipeline GX que le jeu, puis ecrit l'EFB. Cible a part, liee au seul
# runtime : elle n'a pas besoin des 30 unites de code recompile, donc elle se
# lie en secondes au lieu de minutes.
if(NOT NJ_MOBILE)
    # verifie que main.cpp (moteur natif compris) compile, sans le code recompile
    add_library(NativeMainCheck OBJECT src/core/main.cpp)
    target_include_directories(NativeMainCheck PRIVATE src include)
    target_link_libraries(NativeMainCheck PRIVATE njruntime)
    add_executable(ChunkReplay src/tools/chunk_replay.cpp)
    target_include_directories(ChunkReplay PRIVATE src include)
    target_link_libraries(ChunkReplay PRIVATE njruntime)
    add_executable(FifoReplay src/tools/fifo_replay.cpp)
    target_include_directories(FifoReplay PRIVATE src include)
    target_link_libraries(FifoReplay PRIVATE njruntime)
    if(WIN32)
        target_link_libraries(FifoReplay PRIVATE winmm)
    endif()
endif()
