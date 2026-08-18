#!/bin/bash
# Phase N3 : cycle complet natif.  nj_cycle.sh <etiquette> [secondes=70] [host|nohost]
#  1. bibliotheque Nightjar + libmain (portable, branche natif-nj)  2. coeur (portable)  3. APK
#  4. installation sur le S5, lancement de l'appli, journal du coeur resume (+ capture si CAPTURE=1)
export MSYS_NO_PATHCONV=1
M="$(dirname "$0")"; A="/c/Android/platform-tools/adb "
TAG=${1:?etiquette}; SEC=${2:-70}
# le portable se coupe parfois du reseau : attendre qu'il reponde (jusqu'a 10 min)
wait_laptop() { for i in $(seq 1 60); do ssh -o ConnectTimeout=8 user@laptop "echo ok" >/dev/null 2>&1 && return 0; sleep 10; done; echo "ARRET : portable injoignable"; exit 1; }
wait_laptop
rm -f "C:/re4n/ce4_m1_out/apk/$TAG.apk"
[ "${3:-host}" = host ] && bash "$M/laptop_m1_host.sh" 2>&1 | grep -E "ninja=|error:" | head -5
wait_laptop; bash "$M/laptop_port_build.sh" -O2 2>&1 | tee "$M/last_build.txt" | grep -E "build=|en echec|error|pont_en_echec" | head -6
# pont njgfx rate : le lien reprendrait l ancien objet sans rien dire
grep -q pont_en_echec "$M/last_build.txt" && { echo "ARRET : pont njgfx en echec"; grep -m3 error "$M/last_build.txt"; exit 1; }
wait_laptop; bash "$M/laptop_nj_apk.sh" "$TAG" 2>&1 | tail -1 | cut -c1-80
[ -f "C:/re4n/ce4_m1_out/apk/$TAG.apk" ] || { echo "ARRET : APK $TAG absent (construction ratee)"; exit 1; }
$A install -r "C:/re4n/ce4_m1_out/apk/$TAG.apk" 2>&1 | tail -1
# batterie : ne jamais lancer le jeu sous 40 % (le S5 s'eteint) ; NORUN=1 : installer seulement
LVL=$($A shell "dumpsys battery | grep level" | tr -dc '0-9')
if [ -n "${NORUN:-}" ] || [ -z "$LVL" ] || [ "$LVL" -lt "${MIN_BAT:-40}" ]; then echo "installe, pas lance (batterie ${LVL:-?} %)"; exit 0; fi
$A shell "input keyevent KEYCODE_WAKEUP; am start -n org.nightjar.natif/org.nightjar.re4.RE4Activity >/dev/null; sleep $SEC; am force-stop org.nightjar.natif"
F=/sdcard/Android/data/org.nightjar.natif/files
$A shell "echo coeur_vivant=\$(ps -A | grep -c njcore); grep -a -E 'NJGFX|signal|OSPanic' $F/core.log | head -10; echo ...; tail -4 $F/core.log" | cut -c1-200
if [ -n "${CAPTURE:-}" ]; then
  $A exec-out screencap -p > "$M/${TAG}_screen.png"
  python -c "from PIL import Image; im=Image.open(r'$M/${TAG}_screen.png'); im.thumbnail((480,270)); im.save(r'$M/${TAG}_small.png')"
fi
if $A shell "grep -a -q 'signal' $F/core.log"; then
  $A shell "grep -a -A16 'signal' $F/core.log" | tr -d '\r' > "$M/${TAG}_crash.txt"
  ssh user@laptop "cat /c/re4n/re4port/_port_build/re4" > "$M/re4_${TAG}"
  ADDRS=$(grep -oE "libnjcore.so \+ 0x[0-9a-f]+" "$M/${TAG}_crash.txt" | awk '{print $3}' | tr '\n' ' ')
  /c/Android/android-ndk-r26d/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-addr2line.exe -e "$M/re4_${TAG}" -f -C $ADDRS 2>&1 | paste - - | cut -c1-190 | grep -v on_fault | head -12
fi
