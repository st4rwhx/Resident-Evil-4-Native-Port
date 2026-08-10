#!/bin/bash
# Build SN Systems' GCC 2.95.3 ("SN BUILD v1.79 for Nintendo Gamecube") cc1plus and cc1
# natively for Linux from SN's GPL source drop, and install them into the re4 project.
#
#   ./build.sh            build into ./cc1plus ./cc1 and copy to $RE4/build/compilers/ProDG/3.9.3-v1.79/
#   ./build.sh clean      remove src/ obj/ and the binaries
#
# How it works (details in the Makefile and patches/linux-host.patch):
#   * gcc/ and include/ are copied from the drop, CRLF -> LF (the rtl reader chokes on '\r' in
#     rs6000.md), then patches/linux-host.patch is applied:
#       - config.h / hconfig.h / auto-host.h: i386 GNU/Linux host config instead of MSVC/WinNT
#         (HOST_WIDE_INT stays 32-bit because everything is compiled with -m32, like Win32).
#       - SNMisc.c: host_is_japanese() -> 0 (was GetSystemDefaultLangID).
#       - toplev.c: _MAX_PATH for the devstudio-error filename buffer.
#       - cp/decl.c: grokdeclarator took the address of a block-local (`next = &name`) and used
#         it after the block; modern GCC reuses the slot -> crash on every virtual destructor.
#     then patches/shipped-build-temp-flags.patch (function.c: non-aggregate stack temps and parm
#     slots get neither MEM_IN_STRUCT_P nor MEM_SCALAR_P -- the shipped build's behaviour, see the
#     patch header for the whole-tree evidence; installed 2026-09-11).
#   * The gen* tools are built and run against config/rs6000/rs6000.md (the pregenerated
#     cp/parse.c and c-parse.c are used as-is, bison is never run).
#   * Compiler defines follow SN's vsgcc.dsp/dolphin.bat: -DIN_GCC -DHAIFA -DCROSS_COMPILE
#     -DMULTIBYTE_CHARS, plus -DSN_CPP_BUILD for rs6000.c in cc1plus (selects cp/cp-tree.h).
#   * Host compiler flags: gcc -m32 -O2 -std=gnu89 -fpermissive -fcommon -fno-strict-aliasing
#     -fwrapv; static link.  Output is byte-identical to SN's cc1plus.exe v1.76 (under wibo) on
#     every re4 unit except the ones whose fpmem-address unspec split (rs6000.md 11 vs 17) fixes.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# Source drop: SN Systems' GPL source release of their GCC 2.95.3 port, shipped with ProDG for
# GameCube 3.9.3 as ProDGforNGCv393_Source_Code.zip -> "GC source code/NGC_GNU_SRC.zip".
# Point SN_GCC_SRC at the extracted NGC/ directory (the one containing gcc/ and include/).
DROP="${SN_GCC_SRC:?set SN_GCC_SRC to the extracted NGC_GNU_SRC/NGC directory}"
RE4="${RE4:-$(cd "$HERE/../.." && pwd)}"
DEST="$RE4/build/compilers/ProDG/3.9.3-v1.79"

cd "$HERE"

if [ "${1:-}" = "clean" ]; then
    /bin/rm -rf src obj cc1 cc1plus
    exit 0
fi

if [ ! -d src/gcc ]; then
    [ -d "$DROP/gcc/cp" ] || { echo "source drop not found at $DROP (set SN_GCC_SRC)"; exit 1; }
    mkdir -p src
    cp -r "$DROP/gcc" "$DROP/include" src/
    /bin/rm -rf src/gcc/ch            # CHILL front end, not used
    chmod -R u+w src
    find src -type f \( -name '*.c' -o -name '*.h' -o -name '*.md' -o -name '*.def' -o -name '*.y' \
        -o -name '*.cc' -o -name '*.cpp' -o -name '*.in' \) -print0 | xargs -0 sed -i 's/\r$//'
    (cd src && patch -p1 < "$HERE/patches/linux-host.patch")
    (cd src && patch -p1 < "$HERE/patches/shipped-build-temp-flags.patch")
fi

make -j"$(nproc)" all

./cc1plus -version -quiet /dev/null -o /dev/null 2>&1 | head -1
./cc1 -version -quiet /dev/null -o /dev/null 2>&1 | head -1

mkdir -p "$DEST"
cp cc1plus cc1 "$DEST/"
echo "installed to $DEST"
