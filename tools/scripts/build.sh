#!/usr/bin/env bash
# Full RE4DBG build pipeline: dtk split -> LCF patch -> ctors relocation fix
# -> symbol dedup -> mwldeppc link -> elf2dol. Encodes every manual step and
# hand-patch discovered while first getting this build to boot, so a fresh
# symbols.txt/splits.txt change doesn't require re-deriving them.
#
# Usage: tools/scripts/build.sh [--iso]
#   --iso   also rebuild build/RE4DBG/iso/RE4DBG_gc.iso (swaps our DOL into
#           a full disc image built from orig/debug_disc1/, needed for
#           anything past very early boot since a bare .dol has no DVD
#           filesystem for the game to read assets from)
#
# Known limitation (not fixed by this script): ~668 small-data (@sda21,
# r2-relative) loads resolve to addresses offset by a constant -0x1DE0 from
# the original, because mwldeppc's auto-computed _SDA2_BASE_ doesn't match
# the original build's (original: _SDA_BASE_ + 0x10000; ours: sdata2_start +
# 0x8000). Forcing the standard formula via an explicit
# `_SDA2_BASE_ = _f_sdata + 0x18000;` LCF assignment fixes those but breaks
# ~100 other symbols with "Range Error for small data relocation (109)" -
# some symbols genuinely need the narrower auto-computed base. Root cause
# needs a real per-symbol SDA/SDA2 classification fix, not a global
# override. Deferred - doesn't block boot or the title menu.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

DTK=dtk
MWLDEPPC="tools/compilers/GC/2.7/mwldeppc.exe"
CONFIG="config/RE4DBG/config.yml"
OBJ_DIR="build/RE4DBG/obj"
LCF="$OBJ_DIR/ldscript.lcf"

BUILD_ISO=0
if [[ "${1:-}" == "--iso" ]]; then
  BUILD_ISO=1
fi

echo "==> Cleaning previous split output"
rm -f "$OBJ_DIR"/obj/*.o "$OBJ_DIR"/asm/*.s "$LCF" "$OBJ_DIR"/config.json

echo "==> dtk dol split"
"$DTK" dol split "$CONFIG" "$OBJ_DIR"

echo "==> Patching ldscript.lcf (route .ctors/.dtors/extab/extabindex to the"
echo "    end of the GROUP instead of letting mwldeppc default them into"
echo "    .rodata, which shifts every section after it by ~32 bytes and"
echo "    breaks .sdata/.sdata2 r13-relative addressing game-wide)"
py - "$LCF" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path) as f:
    content = f.read()
marker = "        .stack ALIGN(0x100):{}\n    } > text"
replacement = ("        .stack ALIGN(0x100):{}\n"
                "        .ctors ALIGN(0x4):{}\n"
                "        .dtors ALIGN(0x4):{}\n"
                "        extab ALIGN(0x4):{}\n"
                "        extabindex ALIGN(0x4):{}\n"
                "    } > text")
if marker not in content:
    raise SystemExit(f"ERROR: expected marker not found in {path} - dtk's LCF template changed?")
content = content.replace(marker, replacement, 1)
with open(path, "w") as f:
    f.write(content)
print(f"Patched {path}")
PYEOF

echo "==> Extracting object link order from config.json"
py - > "$OBJ_DIR/objlist_raw.txt" <<'PYEOF'
import json
with open("build/RE4DBG/obj/config.json") as f:
    c = json.load(f)
for u in c["units"]:
    print(u["object"])
PYEOF
tr -d '\r' < "$OBJ_DIR/objlist_raw.txt" > "$OBJ_DIR/objlist.txt"
rm -f "$OBJ_DIR/objlist_raw.txt"
NUM_OBJS=$(wc -l < "$OBJ_DIR/objlist.txt")
echo "    $NUM_OBJS objects"

echo "==> Fixing __init_cpp's _ctors relocation (dtk's SDK-signature system"
echo "    always hardcodes this reference's symbol name to '_ctors', which"
echo "    collides with mwldeppc's own always-synthesized linker-generated"
echo "    _ctors symbol and silently wins - redirects it to the real,"
echo "    already-correct data address via direct .o relocation patch)"
py tools/scripts/fix_ctors_reloc.py

echo "==> Deduplicating multiply-defined symbols across split objects"
py tools/scripts/dedupe_elf_symbols.py $(cat "$OBJ_DIR/objlist.txt")

echo "==> Linking with mwldeppc"
"$MWLDEPPC" \
  -fp hardware -nodefaults \
  -lcf "$LCF" \
  -m FUN_80003100 \
  -o build/RE4DBG/linked.elf \
  $(cat "$OBJ_DIR/objlist.txt")

echo "==> Converting to DOL"
"$DTK" elf2dol build/RE4DBG/linked.elf build/RE4DBG/reconstructed.dol

echo "==> Done: build/RE4DBG/reconstructed.dol"
sha1sum build/RE4DBG/reconstructed.dol

if [[ "$BUILD_ISO" == "1" ]]; then
  echo "==> Building GC disc image (build/RE4DBG/iso/RE4DBG_gc.iso)"
  mkdir -p build/RE4DBG/iso
  py tools/scripts/build_gc_iso.py \
    "orig/debug_disc1" \
    "build/RE4DBG/reconstructed.dol" \
    "build/RE4DBG/iso/RE4DBG_gc.iso"
fi
