#!/bin/bash
# Phase N1 : compile notre port (portable), le pousse sur le S5 et le lance.  port_cycle.sh <secondes> [variables RE4_*...]
export MSYS_NO_PATHCONV=1
M="$(dirname "$0")"; A="/c/Android/platform-tools/adb -s 51b3af08"
SEC="${1:-120}"; shift
bash "$M/laptop_port_build.sh" -O2 2>&1 | grep -E "build=|lie :|echec|ecarte" | head -6
ssh user@laptop "cat /c/re4n/re4port/_port_build/re4" > "$M/re4_n1" || exit 1
$A push "$M/re4_n1" /data/local/tmp/re4/re4_n1 >/dev/null
$A shell "cd /data/local/tmp/re4 && chmod 755 re4_n1 && $* RE4_SAVE_DIR=/data/local/tmp/re4/card timeout $SEC ./re4_n1 /sdcard/Android/data/org.nightjar.natif/files/game/disc1 > n1.log 2>&1; echo exit=\$?"
$A shell 'cd /data/local/tmp/re4 && echo "lignes $(wc -l < n1.log)"; grep -a -c "Read Error" n1.log; grep -a -E "Read File|PERF|signal|ROOM|room|salle" n1.log | tail -14; echo ....; grep -a "non implemente" n1.log | sort | uniq -c | sort -rn | head -12; echo ....; tail -6 n1.log' | cut -c1-200
