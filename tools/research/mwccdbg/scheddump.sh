#!/bin/sh
# scheddump.sh lib/unit FUNC [--src file.c] [--out DIR]
#   -> DIR/sched-{pre1,post1,pre2,post2}.txt (default DIR = $MWCCDBG_OUT/FUNC): the raw PCode of every block
#   (address, flags word, operands R<class>:<reg>:<rw>, alias record) right before/after the pre-RA (1) and post-RA (2)
#   scheduler runs of GC/2.6 mwcceppc.exe.  "pre" is taken after the scheduler's per-function alias init (0x511f20).
#   Feed DIR to sched.py.  cwd may be anywhere; the unit's flags are the CRI ones (-O4,p -proc gekko ...).
# Paths: RE4_ROOT (default: git toplevel of this file's repo), MWCCDBG_RW32 (the retrowin32 binary, see README.md
# "Build"; default <this dir>/retrowin32/target/lto/retrowin32), MWCCDBG_OUT (dump base, default /tmp/mwccdbg.$USER/out).
HERE=$(cd "$(dirname "$0")" && pwd)
RE4_ROOT=${RE4_ROOT:-$(git -C "$HERE" rev-parse --show-toplevel)}
MWCCDBG_RW32=${MWCCDBG_RW32:-$HERE/retrowin32/target/lto/retrowin32}
MWCCDBG_OUT=${MWCCDBG_OUT:-/tmp/mwccdbg.$USER/out}
[ -x "$MWCCDBG_RW32" ] || { echo "retrowin32 not found at $MWCCDBG_RW32: set MWCCDBG_RW32 (see README.md)" >&2; exit 1; }
cd "$RE4_ROOT"
UNIT=$1; FUNC=$2; shift 2; SRC=src/$UNIT.c; OUT=$MWCCDBG_OUT/$FUNC
while [ $# -gt 0 ]; do case $1 in --src) SRC=$2; shift 2;; --out) OUT=$2; shift 2;; *) shift;; esac; done
case $SRC in /*) SRC=$(realpath --relative-to="$RE4_ROOT" $SRC);; esac
mkdir -p $OUT
ARGS="build/compilers/GC/2.6/mwcceppc.exe -nodefaults -proc gekko -fp hard -fp_contract on -Cpp_exceptions off -enum int -char signed -warn pragmas -pragma 'cats off' -O4,p -inline auto -sdata 0 -sdata2 0 -str readonly -use_lmw_stmw on -I- -i include -i include/libc -i src/lib -i src/lib/cri -lang=c -c $SRC -o /tmp/mwsched_probe.o"
eval "$MWCCDBG_RW32" --gdb-stub $ARGS >/tmp/mwsched_emu.log 2>&1 &
sleep 1
gdb -batch -nx -ex "py FUNC='$FUNC'; OUT='$OUT'" -x "$HERE/scheddump_gdb.py" 2>&1 | grep -E "^dumped|rror:" | grep -v "Remote connection closed"
wait
echo "dumps in $OUT"
