# Phase 2 (native Windows port) scaffolding.
#
# This does NOT build a playable game - only ~115 of the main DOL's 10,383
# functions are matched/equivalent so far (see config/progress.yaml), and
# every one of them is a small, self-contained getter/setter/logic function
# with zero GameCube hardware dependencies (no GX/DSP/OS calls anywhere in
# src/ yet - grep confirms every matched .cpp only includes its own header
# and ultimately just types.h). What this DOES prove: the matched decomp
# source is genuinely portable C++ that compiles clean on a native Windows
# toolchain (MSVC or MinGW/g++) with zero modification, and behaves
# identically to the verified PowerPC/GameCube semantics - the actual
# workflow Phase 2 will scale up once Phase 1 has real engine coverage.
#
# Build:
#   cmake -S native -B native/build -G "MinGW Makefiles"
#   cmake --build native/build
#   native/build/re4_native_test.exe

cmake_minimum_required(VERSION 3.15)
project(re4_native_port CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RE4_SRC_DIR ${CMAKE_SOURCE_DIR}/../src)

file(GLOB_RECURSE RE4_MATCHED_SOURCES ${RE4_SRC_DIR}/*.cpp)

add_library(re4_matched STATIC ${RE4_MATCHED_SOURCES})
target_include_directories(re4_matched PUBLIC
    ${RE4_SRC_DIR}
    ${RE4_SRC_DIR}/camera
    ${RE4_SRC_DIR}/character
    ${RE4_SRC_DIR}/enemy
    ${RE4_SRC_DIR}/item
    ${RE4_SRC_DIR}/light
    ${RE4_SRC_DIR}/misc
    ${RE4_SRC_DIR}/npc
    ${RE4_SRC_DIR}/object
    ${RE4_SRC_DIR}/weapon
)

add_executable(re4_native_test test_main.cpp)
target_link_libraries(re4_native_test PRIVATE re4_matched)
target_include_directories(re4_native_test PRIVATE ${RE4_SRC_DIR}/camera)
