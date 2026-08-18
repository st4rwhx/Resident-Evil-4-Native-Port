#!/bin/bash
# Mission 1.0 : libmain.so (hote Nightjar, ARMv7) construit sur le PORTABLE depuis l'arbre isole C:/re4n/nj-m1
# (derive de C:/re4n/mesures/laptop_native_apk.sh, memes options ; dossier de build a part, rien d'existant n'est touche).
#   laptop_m1_host.sh
set -e
H=user@laptop
S=/c/re4n/nj-m1
ssh -o ConnectTimeout=8 $H "echo joignable" >/dev/null
ssh $H "mkdir -p /c/re4n/nj-m1"
tar cf - -C $S --exclude=./build --exclude=./.git --exclude=./ios --exclude=./docs . | ssh $H "cd /c/re4n/nj-m1 && tar xf - 2>/dev/null"
# horloge du portable en avance de ~2 min : les fichiers modifies recemment sont redates pour que ninja les reprenne
ssh $H "cd /c/re4n/nj-m1 && find runtime/src runtime/include android -mmin -240 -type f -exec touch {} +"
ssh -o ServerAliveInterval=15 $H 'export PATH="/c/Python314:/c/Program Files/CMake/bin:/c/Users/User/AppData/Local/Microsoft/WinGet/Links:/usr/bin:$PATH"
NDK=C:/android/android-ndk-r26d
D=C:/re4n/nightjar-gen/build/_deps
B=C:/re4n/android-x86-m1
cd /c/re4n/nj-m1
T0=$(date +%s)
if [ ! -f /c/re4n/android-x86-m1/build.ninja ]; then
  cmake -S android -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86 -DANDROID_PLATFORM=android-24 -DNJ_EXPORT_DIR=C:/re4n/nj-m1 -DNJ_DEPS_DIR=$D -DNJ_NATIVE_ONLY=ON -DNJ_P3_JIT=OFF -DNJ_DEVTOOLS=ON > /c/re4n/android-x86-m1.cfg.log 2>&1 || { tail -15 /c/re4n/android-x86-m1.cfg.log; exit 1; }
fi
ninja -C $B -k 0 -j12 main > /c/re4n/android-x86-m1.log 2>&1; R=$?
echo "ninja=$R duree=$(( $(date +%s) - T0 )) s"
grep -E "error:|undefined symbol|FAILED" /c/re4n/android-x86-m1.log | cut -c1-260 | head -30
tail -1 /c/re4n/android-x86-m1.log | cut -c1-160
ls -la $B/libmain.so; sha256sum $B/libmain.so | cut -c1-64
exit $R'
