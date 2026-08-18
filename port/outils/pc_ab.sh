#!/bin/bash
# pc_ab.sh <prefixe> <duree_s> "<etiquette>:<VARS>" ... : essais au chateau avec NJ_PERFCNT=1 (compteurs du fil du jeu
# ouverts dans le coeur a sa premiere image). security.perf_harden vaut 0 seulement jusqu'a ce que le coeur ait ouvert
# ses compteurs (ligne [PERFCNT] compteurs dans core.log), puis il est remis a 1. Resultat : moyenne par image des
# fenetres de 120 images apres l'entree dans la salle (la premiere ecartee).
export MSYS_NO_PATHCONV=1
M="$(cd "$(dirname "$0")"; pwd)"; A="/c/Android/platform-tools/adb "; cd "$M"
F=/sdcard/Android/data/org.nightjar.natif/files
PRE=$1; DUR=$2; shift 2
for spec in "$@"; do
  tag="${PRE}_${spec%%:*}"; ex="${spec#*:}"
  rm -f "${tag}_core.log"
  $A shell "setprop security.perf_harden 0"
  (EXTRA="RE4_NEWGAME_ROOM=${ROOM:-0x204} NJ_PERFCNT=1 $ex" MIN_BAT=40 bash run_auto.sh "$tag" "$DUR" $((DUR + 2)) 10 > "${tag}_run.txt" 2>&1 &)
  for i in $(seq 1 40); do
    sleep 2
    $A shell "grep -a -q 'PERFCNT\] compteurs' $F/core.log 2>/dev/null" && break
  done
  $A shell "setprop security.perf_harden 1"
  for i in $(seq 1 90); do [ -f "${tag}_core.log" ] && break; sleep 3; done
  sleep 4
  python - "$tag" "$ex" <<'EOF'
import re, sys
t, ex = sys.argv[1], sys.argv[2]
try:
    L = open(t + '_core.log', errors='replace').read()
    ok = re.search(r'\[PERFCNT\] compteurs : ([^\n]*)', L)
    st = L[L.index("point d'entree"):]
    rows = [tuple(float(x) for x in m) for m in re.findall(
        r'\[PERFCNT\] par image \(\d+ images\) : ([\d.]+) Mcycles, ([\d.]+) Minstructions, processeur ([\d.]+) ms, '
        r'attente de son tour ([\d.]+) ms, ([\d.]+) tranches ; ([\d.]+) GHz', st)][1:]
    if not rows:
        print(t, 'aucune fenetre', ok.group(1) if ok else ''); sys.exit()
    avg = [sum(r[k] for r in rows) / len(rows) for k in range(6)]
    print('%-10s %-30s Mcycles %5.1f (%s)  Minstr %5.1f  CPU %5.1f ms  attente %4.1f ms  %.2f GHz  [%d fenetres]' % (
        t, ex or '-', avg[0], ' '.join('%.0f' % r[0] for r in rows), avg[1], avg[2], avg[3], avg[5], len(rows)))
except Exception as e:
    print(t, 'mesure incomplete :', e)
EOF
done
echo "perf_harden=$($A shell getprop security.perf_harden)"
