#!/bin/bash
# rec_run.sh <etiquette> "<VAR=val ...>" [debut_s=50] : un essai (run_auto) + simpleperf record de tout le processus du coeur
# (6 s, 1500 Hz) a <debut> s ; perf_harden remis a 1 ; rapport par fil puis par fonction (symboles : _port_build/re4).
export MSYS_NO_PATHCONV=1
M="$(cd "$(dirname "$0")"; pwd)"; A="/c/Android/platform-tools/adb -s 51b3af08"; TAG=$1; EXTRA_V=$2; T0=${3:-50}
SP=C:/Android/sdk/ndk/android-ndk-r27/simpleperf/bin/windows/x86_64/simpleperf.exe
cd "$M"
$A shell "setprop security.perf_harden 0"
(EXTRA="$EXTRA_V" MIN_BAT=40 bash run_auto.sh "$TAG" $((T0+12)) $((T0+14)) 10 > "${TAG}_run.txt" 2>&1 &)
sleep $T0
P=$($A shell "pidof libnjcore.so" | tr -d '\r' | head -1)
$A shell "run-as org.nightjar.natif ./simpleperf record -p $P -e cpu-clock -f 1500 --duration 6 -o rec.data 2>&1 | tail -1"
$A shell "setprop security.perf_harden 1"
$A exec-out "run-as org.nightjar.natif cat rec.data" > "${TAG}_rec.data"
sleep 12
echo "perf_harden=$($A shell getprop security.perf_harden)"
D=$($A shell "pm path org.nightjar.natif" | tr -d '\r' | head -1 | cut -d: -f2 | xargs dirname)
mkdir -p "symfs$D/lib/arm"; cp -f re4_${TAG} "symfs$D/lib/arm/libnjcore.so"
"$SP" report -i "${TAG}_rec.data" --symfs symfs --sort comm 2>/dev/null | tail -12
