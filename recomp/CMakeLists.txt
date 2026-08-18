# njrecomp : notre recompilateur PowerPC -> C++ (docs/RECOMPILATEUR.md).
# Il ne depend que de toml++ ; le runtime n'est compile que dans le projet genere.
add_executable(njrecomp
    src/nj_main.cpp
    src/nj_image.cpp
    src/nj_config.cpp
    src/nj_emit.cpp
    src/nj_gen.cpp)
target_link_libraries(njrecomp PRIVATE tomlplusplus::tomlplusplus)
# runtime/obf.h, runtime/diag.h : litteraux chiffres et messages sans texte (memes sources que Nightjar.exe)
target_include_directories(njrecomp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(njrecomp PRIVATE -Wall)
endif()
