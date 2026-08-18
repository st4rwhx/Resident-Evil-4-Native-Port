#!/bin/bash
# cnt_ab.sh <prefixe> "<etiquette>:<VARS>" ... : mesure EXACTE par image (14/11 soir), compteurs internes NJ_PERFCNT=1 :
#   [PERFCNT]   fil du jeu  (cycles par image rendue, processeur, attente de son tour)
#   [PERFCNT-R] fil de rendu (idem)
# perf_harden mis a 0 au lancement, remis a 1 des que les deux compteurs sont ouverts (ou apres 120 s au plus ; trap).
# Resultat : moyenne des 4 derniers releves (120 images chacun) de chaque fil. Salle : ROOM (defaut village 0x101).
export MSYS_NO_PATHCONV=1
M="$(cd "$(dirname "$0")"; pwd)"; A="/c/Android/platform-tools/adb "; cd "$M"
F=/sdcard/Android/data/org.nightjar.natif/files
PRE=$1; shift
harden1() { $A shell "setprop security.perf_harden 1"; }
trap harden1 EXIT INT TERM
for spec in "$@"; do
  tag="${PRE}_${spec%%:*}"; ex="${spec#*:}"
  rm -f "${tag}_core.log"
  $A shell "setprop security.perf_harden 0"
  (EXTRA="RE4_NEWGAME_ROOM=${ROOM:-0x101} NJ_PERFCNT=1 $ex" bash run_auto.sh "$tag" ${T1:-150} ${T2:-152} 10 > "${tag}_run.txt" 2>&1 &)
  for i in $(seq 1 60); do
    sleep 2
    n=$($A shell "grep -a -c 'compteur.*ouvert\|compteurs : cycles ouverts' $F/core.log 2>/dev/null" | tr -dc '0-9')
    [ "${n:-0}" -ge 2 ] && break
  done
  harden1
  for i in $(seq 1 120); do [ -f "${tag}_core.log" ] && break; sleep 3; done
  sleep 3
  python - "$tag" "$ex" <<'EOF'
import re, sys
t, ex = sys.argv[1], sys.argv[2]
L = open(t + '_core.log', errors='replace').read()
def avg(pat):
    v = [tuple(float(x) for x in m) for m in re.findall(pat, L)][-4:]
    return [sum(c) / len(v) for c in zip(*v)] if v else None
g = avg(r'\[PERFCNT\] par image \(\d+ images\) : ([\d.]+) Mcycles, [\d.]+ Minstructions, processeur ([\d.]+) ms, attente de son tour ([\d.]+) ms')
r = avg(r'\[PERFCNT-R\] par image \(\d+ images\) : ([\d.]+) Mcycles, processeur ([\d.]+) ms, attente de son tour ([\d.]+) ms')
f = [float(x) for x in re.findall(r'\(([\d.]+) i/s\)', L)][-6:]
fmt = lambda x: '%5.1f Mcycles %5.1f ms (+%4.1f attente)' % tuple(x) if x else 'absent'
print('%-12s %-18s jeu %s | rendu %s | i/s %.1f' % (t, ex or '-', fmt(g), fmt(r), sum(f) / len(f) if f else 0))
EOF
  sleep 10
done
echo "perf_harden=$($A shell getprop security.perf_harden)"
