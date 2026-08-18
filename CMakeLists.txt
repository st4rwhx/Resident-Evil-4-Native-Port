# Nightjar : recompilateur et runtime. Construit njrecomp (recomp/) et verifie que le runtime compile ;
# le jeu lui-meme se construit a partir du projet que le recompilateur genere (voir tools/build.ps1).
cmake_minimum_required(VERSION 3.20.0)
project(nightjar LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

# Certaines dependances (glad) exigent encore une compatibilite CMake < 3.5 que les CMake
# recents refusent : on la leur accorde.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent) # dependances recuperees a la configuration

# Lecture des fichiers de configuration (MIT).
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus
    GIT_TAG v3.4.0)


# Fenetre, entrees, son (zlib).
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "SDL2 n'est pas installe" FORCE)
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG release-2.28.5)
FetchContent_MakeAvailable(tomlplusplus SDL2)

add_subdirectory(runtime)
add_subdirectory(recomp)

# Cible precompilee 0.1.8 multi-region : UN SEUL binaire pour USA et Europe.
# Les sources de chaque region sont compilees avec des symboles renommes (-Dg=g_usa / g_eur)
# et liees ensemble via precomp_dispatch.cpp qui redirige au lancement selon le disque du joueur.
option(NJ_PRECOMPILED "Compiler Nightjar avec les sources C++ precompilees USA + Europe" OFF)
if(NJ_PRECOMPILED)
    target_compile_definitions(njruntime PRIVATE NJ_PRECOMPILED=1)
    set(NJ_USA_DIR "${CMAKE_SOURCE_DIR}/../nj-usa" CACHE PATH "Sources precompilees USA")
    set(NJ_EUR_DIR "${CMAKE_SOURCE_DIR}/../nj-eur" CACHE PATH "Sources precompilees Europe")

    # Defines communs : renommage des membres GuestCpu (anciens noms -> nouveaux)
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

    # --- USA ----------------------------------------------------------------
    file(GLOB NJ_USA_SOURCES "${NJ_USA_DIR}/output_*.cpp")
    add_library(nj_game_usa OBJECT "${NJ_USA_DIR}/main_output.cpp" ${NJ_USA_SOURCES})
    target_include_directories(nj_game_usa PRIVATE runtime/src runtime/include "${NJ_USA_DIR}")
    target_compile_definitions(nj_game_usa PRIVATE
        g=g_usa run_game=run_game_usa nwii_dispatch_one=nj_dispatch_one_usa
        g_nj_dol_fnv=g_nj_dol_fnv_usa g_rel_aot_modules=g_rel_aot_modules_usa
        g_rel_aot_count=g_rel_aot_count_usa
        GetWepTargetPos=GetWepTargetPos_usa PlWepLockRand=PlWepLockRand_usa
        cMot3__move=cMot3__move_usa joyKamae=joyKamae_usa joyLKamae=joyLKamae_usa
        PlayerInit=PlayerInit_usa cItemMgr__clear=cItemMgr__clear_usa
        cItemMgr__set_char=cItemMgr__set_char_usa
        ${NJ_COMPAT_DEFS})

    # --- Europe -------------------------------------------------------------
    file(GLOB NJ_EUR_SOURCES "${NJ_EUR_DIR}/output_*.cpp")
    add_library(nj_game_eur OBJECT "${NJ_EUR_DIR}/main_output.cpp" ${NJ_EUR_SOURCES})
    target_include_directories(nj_game_eur PRIVATE runtime/src runtime/include "${NJ_EUR_DIR}")
    target_compile_definitions(nj_game_eur PRIVATE
        g=g_eur run_game=run_game_eur nwii_dispatch_one=nj_dispatch_one_eur
        g_nj_dol_fnv=g_nj_dol_fnv_eur g_rel_aot_modules=g_rel_aot_modules_eur
        g_rel_aot_count=g_rel_aot_count_eur
        GetWepTargetPos=GetWepTargetPos_eur PlWepLockRand=PlWepLockRand_eur
        cMot3__move=cMot3__move_eur joyKamae=joyKamae_eur joyLKamae=joyLKamae_eur
        PlayerInit=PlayerInit_eur cItemMgr__clear=cItemMgr__clear_eur
        cItemMgr__set_char=cItemMgr__set_char_eur
        ${NJ_COMPAT_DEFS})

    # --- Executable unifie --------------------------------------------------
    add_executable(Nightjar WIN32
        runtime/src/core/main.cpp
        runtime/src/precomp/precomp_dispatch.cpp
    )
    target_include_directories(Nightjar PRIVATE runtime/src runtime/include)
    target_link_libraries(Nightjar PRIVATE njruntime nj_game_usa nj_game_eur)
    if(WIN32)
        if(NJ_RELEASE)
            target_compile_options(Nightjar PRIVATE
                "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
                "-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.")
            target_link_options(Nightjar PRIVATE -mwindows -static -static-libgcc -static-libstdc++ -s -Wl,--strip-all)
        endif()
        # icone et informations de version de l'exe (0.1.9 : la cible precompilee n'incluait pas ces ressources)
        enable_language(RC)
        target_sources(Nightjar PRIVATE runtime/res/re4native.rc)
        target_include_directories(Nightjar PRIVATE runtime/res)
        set_source_files_properties(runtime/res/re4native.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/runtime/res/nightjar.ico)
        target_link_libraries(Nightjar PRIVATE ws2_32 winhttp comdlg32 crypt32 winmm)
    endif()
endif()

