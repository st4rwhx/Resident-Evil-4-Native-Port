#!/bin/bash
# Build the debug cc1plus/cc1 (production SN GCC 2.95.3 v1.79 source + the env-gated hooks of
# patches/dbg-hooks.patch) from the CURRENT production source tree tools/sn-gcc/src.  Run it after
# tools/sn-gcc/build.sh has populated tools/sn-gcc/src and obj/ (it copies both), and again whenever
# the production compiler source changes (a new patch in tools/sn-gcc/patches).
#   tools/research/sngdbg/rebuild.sh          # copy src/ + obj/ + Makefile from tools/sn-gcc into $SNGDBG_DIR,
#                                             # apply patches/dbg-hooks.patch, make (~5 s)
#   tools/research/sngdbg/rebuild.sh quick    # just `make all` in $SNGDBG_DIR (after a local hook edit, ~1 s)
# Output: $SNGDBG_DIR/cc1plus and cc1 (default SNGDBG_DIR = <repo>/build/sngdbg, untracked).  Use them with
# the kit: `CC1DIR=$SNGDBG_DIR GDBG=1 tools/research/kit/variant.sh <unit> <variant-src> [FUNC]`.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
RE4="${RE4_ROOT:-$(git -C "$HERE" rev-parse --show-toplevel)}"
SNGDBG_DIR="${SNGDBG_DIR:-$RE4/build/sngdbg}"
mkdir -p "$SNGDBG_DIR"
cd "$SNGDBG_DIR"
if [ "${1:-}" != "quick" ]; then
    [ -d "$RE4/tools/sn-gcc/src" ] || { echo "tools/sn-gcc/src missing: run tools/sn-gcc/build.sh first" >&2; exit 1; }
    /bin/rm -rf src obj
    cp -r "$RE4/tools/sn-gcc/src" "$RE4/tools/sn-gcc/obj" .
    cp "$RE4/tools/sn-gcc/Makefile" .
    patch -p0 < "$HERE/patches/dbg-hooks.patch"
fi
make -j"$(nproc)" all 2>&1 | grep -E 'error|Error' || true
./cc1plus -version -quiet /dev/null -o /dev/null 2>&1 | head -1
sha1sum cc1plus cc1
echo "debug compiler in $SNGDBG_DIR (CC1DIR=$SNGDBG_DIR)"
