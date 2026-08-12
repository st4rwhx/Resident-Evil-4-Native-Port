#!/bin/bash
# FifoReplay (rejeu .dff sur PC, methode ORACLE_FIFO) construit depuis l'arbre nj-m1 sur le PORTABLE, dans C:/re4n/nj-m1-pc.
#   laptop_fiforeplay.sh            : construit
#   laptop_fiforeplay.sh run <dff local> <prefixe> <premiere> <nombre> [VAR=val ...] : rejoue sur le portable, rapatrie les PPM
set -e
H=admin@192.168.1.124
S=/c/re4n/nj-m1
M="$(dirname "$0")"
if [ "${1:-}" != run ]; then
  ssh $H "mkdir -p /c/re4n/nj-m1-pc"
  tar cf - -C $S --exclude=./build --exclude=./.git --exclude=./ios --exclude=./docs --exclude=./android . | ssh $H "cd /c/re4n/nj-m1-pc && tar xf - 2>/dev/null"
  ssh $H "cd /c/re4n/nj-m1-pc && find runtime/src runtime/include -mmin -600 -type f -exec touch {} +"
  ssh $H 'W=C:/Users/Admin/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64; export PATH="$(cygpath -u "$W")/bin:/c/Program Files/CMake/bin:$PATH"; cd /c/re4n/nj-m1-pc
D=C:/re4n/nightjar-gen/build/_deps
if [ ! -f build/CMakeCache.txt ]; then cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_GLAD=$D/glad-src -DFETCHCONTENT_SOURCE_DIR_SDL2=$D/sdl2-src -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=$D/tomlplusplus-src -DNJ_P3_JIT=OFF > cfg.log 2>&1 || { tail -8 cfg.log; exit 1; }; fi
cmake --build build --target FifoReplay -j12 2>&1 | grep -E "error|undefined|Built target FifoReplay" | head -20
ls -la build/runtime/FifoReplay.exe'
  exit 0
fi
shift
DFF=$1; PFX=$2; FIRST=$3; N=$4; shift 4
ssh $H "mkdir -p /c/re4n/replay_in /c/re4n/replay_out"
B=$(basename "$DFF")
ssh $H "test -f /c/re4n/replay_in/$B" || cat "$DFF" | ssh $H "cat > /c/re4n/replay_in/$B"
ENVS="$*"
ssh $H "W=C:/Users/Admin/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64; export PATH=\"\$(cygpath -u \"\$W\")/bin:\$PATH\"; cd /c/re4n/replay_out && rm -f ${PFX}_*.ppm; env $ENVS /c/re4n/nj-m1-pc/build/runtime/FifoReplay.exe C:/re4n/replay_in/$B C:/re4n/replay_out/$PFX $FIRST $N > ${PFX}.log 2>&1; echo code=\$?; tail -3 ${PFX}.log; ls ${PFX}_*.ppm"
mkdir -p "$M/replay"
for f in $(ssh $H "cd /c/re4n/replay_out && ls ${PFX}_*.ppm ${PFX}.log"); do ssh $H "cat /c/re4n/replay_out/$f" > "$M/replay/$f"; done
ls "$M/replay" | grep "$PFX"
