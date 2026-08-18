#!/bin/bash
# mem_run.sh <tag> "<EXTRA>" [duree=100] : run_auto en fond + releve memoire (RSS njcore et appli, MemAvailable) toutes les 5 s
export MSYS_NO_PATHCONV=1
M="$(dirname "$0")"; A="/c/Android/platform-tools/adb "; TAG=$1; D=${3:-100}
( cd "$M" && EXTRA="$2" bash run_auto.sh "$TAG" "$D" "$D" 10 > "$M/${TAG}_run.txt" 2>&1 ) &
: > "$M/${TAG}_mem.txt"
for i in $(seq 1 $((D / 5))); do
  sleep 5
  $A shell 'for p in $(pidof libnjcore.so) $(pidof org.nightjar.natif); do printf "%s:%s " $p "$(grep -E "VmRSS|VmSwap|VmHWM" /proc/$p/status | tr -s " " | tr "\n" " ")"; done; cat /proc/$(pidof libnjcore.so)/oom_score_adj 2>/dev/null; grep -E "MemFree|^Cached|SwapFree" /proc/meminfo | tr -s " " | tr "\n" " "; echo' | tr -d '\r' | sed "s/^/$((i*5))s /" >> "$M/${TAG}_mem.txt"
done
wait
