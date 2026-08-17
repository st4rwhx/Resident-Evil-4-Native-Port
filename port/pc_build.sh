#!/bin/bash
# Compilation sur le PC principal (16/11 : portable hors ligne ; a n'utiliser qu'avec l'accord de l'utilisateur).
# Meme chaine que les scripts du portable (scratchpad m1/laptop_*), en local :
#   port/pc_build.sh <x86|arm> [etiquette_apk]
#   1. copie de travail sans espace ni collision de casse : C:/re4n/re4port_pc (src/tools -> src/tools_lc)
#   2. coeur : build_game.py (objets DOL, avec ppc_f2u sur ARM), build_rel.py (modules), pont njgfx, lien
#   3. APK (si etiquette) : nj-m1/android/build_apk.sh, PREBUILT=1, coeur range en libnjcore.so -> C:/re4n/ce4_m1_out/apk/
# Pre-requis : moteur Nightjar compile pour l'ABI (C:/re4n/android-<abi>-m1 : libnjruntime.a, libnj_imgui.a, SDL2).
set -e
ABI=${1:?x86 ou arm}
TAG=${2:-}
SRC="C:/Users/Admin/Downloads/RE4 PROJECT/re4-decomp-upstream"
W=/c/re4n/re4port_pc
NDK=C:/android/android-ndk-r26d
B=/c/android/android-ndk-r26d/toolchains/llvm/prebuilt/windows-x86_64/bin
PY=python
JOBS=${JOBS:-4}
if [ "$ABI" = x86 ]; then
    OUT=_port_build_x86; L=C:/re4n/android-x86-m1; TGT=i686-none-linux-android24; CXX=$B/i686-linux-android28-clang++.cmd
    ABIFLAGS=""; APKABI=x86; export PORT_ABI=x86
elif [ "$ABI" = x86_64 ]; then   # N7 (18/11) : coeur 64 bits pour MuMu ; moteur : scratchpad m1/laptop_x86_64_host.sh
    OUT=_port_build_x86_64; L=C:/re4n/android-x86_64-m1; TGT=x86_64-none-linux-android24; CXX=$B/x86_64-linux-android28-clang++.cmd
    ABIFLAGS=""; APKABI=x86_64; export PORT_ABI=x86_64
else
    OUT=_port_build; L=C:/re4n/android-armeabi-v7a-m1; TGT=armv7-none-linux-androideabi24; CXX=$B/armv7a-linux-androideabi28-clang++.cmd
    ABIFLAGS="-mfpu=neon-vfpv4 -march=armv7-a -mthumb"; APKABI=armeabi-v7a; unset PORT_ABI
fi
mkdir -p $W
T0=$(date +%s)
# 1. copie (rsync absent : tar ; les fichiers inchanges gardent leur date, donc le cache de build_game reste valable)
tar cf - -C "$SRC" --exclude=./.git --exclude=./_port_build --exclude=./_port_build_x86 --exclude=./_port_build_x86_64 --exclude=./_port_dbg . | (cd $W && tar xf - 2>/dev/null)
$PY "C:/Users/Admin/AppData/Local/Temp/claude/b1655763106ab44b/803bda34-669c-4bb9-b360-e05a864e00ed/scratchpad/m1/casse_tar.py" "$SRC" | (cd $W && tar xf -)
cd $W
sed -i 's|"tools/|"tools_lc/|g' config/G4BE08/modules.py
grep -rl '#include "tools/' src include | xargs -r sed -i 's|#include "tools/|#include "tools_lc/|g'
export NDK PORT_OPT=${PORT_OPT:--O2}
# 2. coeur
$PY port/build_game.py --jobs $JOBS --quiet > /c/re4n/pc_dol_$ABI.log 2>&1 || { tail -20 /c/re4n/pc_dol_$ABI.log; exit 1; }
tail -1 /c/re4n/pc_dol_$ABI.log
$PY port/build_rel.py --jobs $JOBS > /c/re4n/pc_rel_$ABI.log 2>&1 || { tail -20 /c/re4n/pc_rel_$ABI.log; exit 1; }
tail -2 /c/re4n/pc_rel_$ABI.log
$B/clang++.exe --target=$TGT -DNJ_DEVTOOLS=1 -DNJ_MOBILE=1 -DNJ_NO_WATCH -DANDROID -DNDEBUG -IC:/re4n/nj-m1/runtime/include \
    -IC:/re4n/nj-m1/runtime/third_party/zstd -I$L/_deps/sdl2-build/include -I$L/_deps/sdl2-build/include/SDL2 -O2 -std=gnu++20 \
    $ABIFLAGS -funwind-tables -c port/njgfx/njgfx.cpp -o $OUT/njgfx.o > /c/re4n/pc_njgfx_$ABI.log 2>&1 || { head -20 /c/re4n/pc_njgfx_$ABI.log; exit 1; }
EXTRA="$OUT/njgfx.o $L/runtime/libnjruntime.a $L/runtime/libnj_imgui.a $L/_deps/sdl2-build/libSDL2.a -lOpenSLES -lGLESv1_CM -lm -ldl @port/gxn_wrap.rsp"
RE4_EXTRA_LINK="$EXTRA" $PY port/build_game.py --link --quiet --jobs $JOBS > /c/re4n/pc_build_$ABI.log 2>&1 || true
grep -q Traceback /c/re4n/pc_build_$ABI.log && { echo correctif_ppc_en_echec; tail -5 /c/re4n/pc_build_$ABI.log; exit 1; }
grep -h ppc_f2u /c/re4n/pc_build_$ABI.log || true
$CXX @$OUT/game.rsp -o $OUT/re4 -lEGL -lGLESv3 -llog -landroid -static-libstdc++ -Wl,--error-limit=0 $EXTRA \
    -Wl,--allow-multiple-definition -Wl,--wrap=memset -Wl,--wrap=calloc -Wl,--wrap=malloc > /c/re4n/pc_link_$ABI.log 2>&1 \
    || { grep -E "error" /c/re4n/pc_link_$ABI.log | sort -u | head -10; exit 1; }
echo "coeur $ABI : $(stat -c %s $OUT/re4) octets, $(( $(date +%s) - T0 )) s"
# 3. APK
if [ -n "$TAG" ]; then
    cd /c/re4n/nj-m1/android
    mkdir -p /c/re4n/apk/pc
    [ -f /c/re4n/apk/pc/Nightjar_avant_pc.apk ] || cp -p /c/re4n/apk/Nightjar.apk /c/re4n/apk/pc/Nightjar_avant_pc.apk 2>/dev/null || true
    NJ_APP_ID=org.nightjar.natif PREBUILT=1 NJ_SUFFIX=-m1 NJ_CORE_EXE=C:/re4n/re4port_pc/$OUT/re4 NJ_DEPS_DIR=C:/re4n/nightjar-gen/build/_deps \
        NJ_EXPORT_DIR=C:/re4n/nj-m1 JOBS=$JOBS bash build_apk.sh $APKABI > /c/re4n/apk/pc/build_$TAG.log 2>&1 || { tail -20 /c/re4n/apk/pc/build_$TAG.log; exit 1; }
    mkdir -p /c/re4n/ce4_m1_out/apk
    cp -p /c/re4n/apk/Nightjar.apk /c/re4n/ce4_m1_out/apk/$TAG.apk
    [ -f /c/re4n/apk/pc/Nightjar_avant_pc.apk ] && cp -p /c/re4n/apk/pc/Nightjar_avant_pc.apk /c/re4n/apk/Nightjar.apk
    echo "APK : C:/re4n/ce4_m1_out/apk/$TAG.apk ($(stat -c %s /c/re4n/ce4_m1_out/apk/$TAG.apk) octets), $(( $(date +%s) - T0 )) s au total"
fi
