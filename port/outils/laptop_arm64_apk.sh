#!/bin/bash
# Assemble l'APK org.nightjar.natif en ARM64 (arm64-v8a) sur le PORTABLE avec le flux existant (android/build_apk.sh, PREBUILT=1)
#   laptop_arm64_apk.sh <etiquette>
# hote  = C:/re4n/android-arm64-v8a-m1/libmain.so (laptop_arm64_host.sh)
# coeur = notre portage (C:/re4n/re4port/_port_build_arm64/re4), range en libnjcore.so
set -e
H=user@laptop
TAG=${1:?etiquette}
ssh -o ConnectTimeout=8 $H "echo joignable" >/dev/null
ssh -o ServerAliveInterval=15 $H 'set -e
export PATH="/c/Python314:/c/Program Files/CMake/bin:/c/Users/User/AppData/Local/Microsoft/WinGet/Links:/usr/bin:$PATH"
mkdir -p /c/re4n/apk/m1arm64
[ -f /c/re4n/apk/m1arm64/Nightjar_connu_avant_m1arm64.apk ] || cp -p /c/re4n/apk/Nightjar.apk /c/re4n/apk/m1arm64/Nightjar_connu_avant_m1arm64.apk 2>/dev/null || true
cd /c/re4n/nj-m1/android
NJ_APP_ID=org.nightjar.natif PREBUILT=1 NJ_SUFFIX=-m1 NJ_CORE_EXE=C:/re4n/re4port/_port_build_arm64/re4 NJ_DEPS_DIR=C:/re4n/nightjar-gen/build/_deps NJ_EXPORT_DIR=C:/re4n/nj-m1 bash build_apk.sh arm64-v8a > /c/re4n/apk/m1arm64/build_'"$TAG"'.log 2>&1 || { tail -20 /c/re4n/apk/m1arm64/build_'"$TAG"'.log; exit 1; }
tail -1 /c/re4n/apk/m1arm64/build_'"$TAG"'.log | cut -c1-160
cp -p /c/re4n/apk/Nightjar.apk /c/re4n/apk/m1arm64/'"$TAG"'.apk
[ -f /c/re4n/apk/m1arm64/Nightjar_connu_avant_m1arm64.apk ] && cp -p /c/re4n/apk/m1arm64/Nightjar_connu_avant_m1arm64.apk /c/re4n/apk/Nightjar.apk
sha256sum /c/re4n/apk/Nightjar.apk /c/re4n/apk/m1arm64/'"$TAG"'.apk | cut -c1-110'
mkdir -p C:/re4n/ce4_m1_out/apk
ssh $H "cat /c/re4n/apk/m1arm64/$TAG.apk" > C:/re4n/ce4_m1_out/apk/$TAG.apk
sha256sum C:/re4n/ce4_m1_out/apk/$TAG.apk | cut -c1-80
