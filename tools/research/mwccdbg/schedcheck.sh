#!/bin/sh
# schedcheck.sh "lib/unit Func" ... -> one line per function: scheddump + sched.py pre-RA and post-RA block counts
HERE=$(cd "$(dirname "$0")" && pwd)
MWCCDBG_OUT=${MWCCDBG_OUT:-/tmp/mwccdbg.$USER/out}
for f in "$@"; do
  set -- $f
  "$HERE/scheddump.sh" $1 $2 >/dev/null 2>&1
  D=$MWCCDBG_OUT/$2
  if [ ! -s $D/sched-pre1.txt ]; then echo "$2: no dump (name? inlined away?)"; continue; fi
  a=$(python3 "$HERE/sched.py" $D 2>&1 | tail -1 | sed 's/.*: //')
  b=$(python3 "$HERE/sched.py" $D --post 2>&1 | tail -1 | sed 's/.*: //')
  echo "$2: $a | $b"
done
