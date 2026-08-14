#!/bin/bash
# Mission 1.0 : assemble l'APK org.nightjar.natif sur le PORTABLE avec le flux existant (android/build_apk.sh, PREBUILT=1)
#   laptop_m1_apk.sh <etiquette>
# hote  = C:/re4n/android-armeabi-v7a-m1/libmain.so (laptop_m1_host.sh)
# coeur = notre portage (C:/re4n/re4port/_port_build_x86/re4), range en libnjcore.so
# L'APK connu (C:/re4n/apk/Nightjar.apk du 27-29/10) est d'abord copie a cote ; la sortie est rangee dans C:/re4n/apk/m1x86/
# puis rapatriee dans C:/re4n/ce4_m1_out/apk/<etiquette>.apk. Rien n'est installe ici.
set -e
H=admin@192.168.1.124
TAG=${1:?etiquette}
ssh -o ConnectTimeout=8 $H "echo joignable" >/dev/null
ssh -o ServerAliveInterval=15 $H 'set -e
export PATH="/c/Python314:/c/Program Files/CMake/bin:/c/Users/Admin/AppData/Local/Microsoft/WinGet/Links:/usr/bin:$PATH"
mkdir -p /c/re4n/apk/m1x86
[ -f /c/re4n/apk/m1x86/Nightjar_connu_avant_m1x86.apk ] || cp -p /c/re4n/apk/Nightjar.apk /c/re4n/apk/m1x86/Nightjar_connu_avant_m1x86.apk
cd /c/re4n/nj-m1/android
NJ_APP_ID=org.nightjar.natif PREBUILT=1 NJ_SUFFIX=-m1 NJ_CORE_EXE=C:/re4n/re4port/_port_build_x86/re4 NJ_DEPS_DIR=C:/re4n/nightjar-gen/build/_deps NJ_EXPORT_DIR=C:/re4n/nj-m1 bash build_apk.sh x86 > /c/re4n/apk/m1x86/build_'"$TAG"'.log 2>&1 || { tail -20 /c/re4n/apk/m1x86/build_'"$TAG"'.log; exit 1; }
tail -1 /c/re4n/apk/m1x86/build_'"$TAG"'.log | cut -c1-160
cp -p /c/re4n/apk/Nightjar.apk /c/re4n/apk/m1x86/'"$TAG"'.apk
cp -p /c/re4n/apk/m1x86/Nightjar_connu_avant_m1x86.apk /c/re4n/apk/Nightjar.apk
sha256sum /c/re4n/apk/Nightjar.apk /c/re4n/apk/m1x86/'"$TAG"'.apk | cut -c1-110'
mkdir -p C:/re4n/ce4_m1_out/apk
ssh $H "cat /c/re4n/apk/m1x86/$TAG.apk" > C:/re4n/ce4_m1_out/apk/$TAG.apk
sha256sum C:/re4n/ce4_m1_out/apk/$TAG.apk | cut -c1-80
