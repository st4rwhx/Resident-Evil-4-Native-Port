#!/bin/bash
# host_ab.sh <prefixe> "<etiquette>:<VARS>" ... : r100, a 110 s, 15 s de compteurs en parallele :
#  - appli hote (processus entier) : task-clock -> coeurs occupes ;
#  - fil principal du coeur : cpu-cycles, task-clock ; images comptees par le journal (i/s) -> Mcycles/image.
# perf_harden remis a 1 apres chaque mesure.
export MSYS_NO_PATHCONV=1
M="$(cd "$(dirname "$0")"; pwd)"; A="/c/Android/platform-tools/adb -s 51b3af08"; cd "$M"
PRE=$1; shift; T0=110; DUR=15
for spec in "$@"; do
  tag="${PRE}_${spec%%:*}"; ex="${spec#*:}"
  rm -f "${tag}_core.log"
  (EXTRA="RE4_NEWGAME_ROOM=${ROOM:-0x100} $ex" bash run_auto.sh "$tag" $((T0 + DUR + 4)) $((T0 + DUR + 6)) 10 > "${tag}_run.txt" 2>&1 &)
  sleep $T0
  H=$($A shell "ps -A -o PID,NAME | grep ' org.nightjar.natif$'" | tr -d '\r' | awk '{print $1}' | head -1)
  P=$($A shell "pidof libnjcore.so" | tr -d '\r' | head -1)
  # fil de rendu = le fil du coeur (autre que le principal) qui a le plus tourne
  R=$($A shell "for t in /proc/$P/task/*; do echo \$(basename \$t) \$(cut -d' ' -f1 \$t/schedstat); done" | tr -d '\r' | awk -v p=$P '$1!=p {print $2, $1}' | sort -n | tail -1 | awk '{print $2}')
  $A shell "setprop security.perf_harden 0"
  [ -n "$R" ] && $A shell "run-as org.nightjar.natif ./simpleperf stat -t $R -e cpu-cycles,task-clock --duration $DUR 2>&1" | tr -d '\r' > "${tag}_rstat.txt" &
  $A shell "run-as org.nightjar.natif ./simpleperf stat -p $H -e task-clock --duration $DUR 2>&1" | tr -d '\r' > "${tag}_hstat.txt" &
  $A shell "run-as org.nightjar.natif ./simpleperf stat -p $P -e task-clock --duration $DUR 2>&1" | tr -d '\r' > "${tag}_pstat.txt" &
  $A shell "run-as org.nightjar.natif ./simpleperf stat -t $P -e cpu-cycles,task-clock --duration $DUR 2>&1" | tr -d '\r' > "${tag}_stat.txt"
  wait
  $A shell "setprop security.perf_harden 1"
  for i in $(seq 1 60); do [ -f "${tag}_core.log" ] && break; sleep 3; done
  sleep 4
  python - "$tag" "$ex" "$DUR" <<'EOF'
import re, sys
t, ex, dur = sys.argv[1], sys.argv[2], float(sys.argv[3])
try:
    g = lambda s, k: float(re.search(r'([\d,\.]+)(?:\(ms\))?\s+' + k, s).group(1).replace(',', ''))
    s = open(t + '_stat.txt').read(); h = open(t + '_hstat.txt').read(); pp = open(t + '_pstat.txt').read(); L = open(t + '_core.log', errors='replace').read()
    f = [float(x) for x in re.findall(r'\(([\d.]+) i/s\)', L)][-4:]
    fps = sum(f) / len(f); n = dur * fps
    print('%-10s %-16s hote %.2f | coeur entier %.2f | fil du jeu : Mcycles/image %4.0f  ms CPU/image %5.1f  coeur %.2f  i/s %.2f' % (
        t, ex or '-', g(h, 'task-clock') / 1000 / dur, g(pp, 'task-clock') / 1000 / dur, g(s, 'cpu-cycles') / n / 1e6, g(s, 'task-clock') / n,
        g(s, 'task-clock') / 1000 / dur, fps))
    try:
        r = open(t + '_rstat.txt').read()
        print('%-10s fil de rendu : Mcycles/image %4.0f  ms CPU/image %5.1f  coeur %.2f' % (
            t, g(r, 'cpu-cycles') / n / 1e6, g(r, 'task-clock') / n, g(r, 'task-clock') / 1000 / dur))
    except Exception as e:
        print(t, 'fil de rendu non mesure :', e)
except Exception as e:
    print(t, 'mesure incomplete :', e)
EOF
  sleep 15
done
echo "perf_harden=$($A shell getprop security.perf_harden)"
