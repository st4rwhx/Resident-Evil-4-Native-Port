#!/usr/bin/env python3
"""One-shot side-by-side diff of a function: target (left) vs our compiled object (right).

usage: fdiff.py <unit> <symbol> [-n LINES] [--all]
       e.g. fdiff.py game/cam_ctrl Comeback__13CameraControl

Rebuilds the unit's object first. Prints only differing lines unless --all is given.
"""
import argparse, json, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VER = os.environ.get("RE4_VERSION", "G4BE08")

ap = argparse.ArgumentParser()
ap.add_argument("unit"); ap.add_argument("symbol")
ap.add_argument("-n", type=int, default=80); ap.add_argument("--all", action="store_true")
a = ap.parse_args()
unit = re.sub(r"\.(cpp|c)$", "", a.unit)
obj = os.path.join("build", VER, "src", unit + ".o")
r = subprocess.run(["ninja", obj], cwd=ROOT, capture_output=True, text=True)
if r.returncode != 0:
    print(r.stdout[-4000:]); print(r.stderr[-4000:]); sys.exit("build failed")
out = os.path.join(ROOT, "build", VER, f"fdiff.{os.getpid()}.json")  # per-process: agents run concurrently
# objdiff names units <module>/<unit>: "main" for the DOL, the module name for REL units (st2_4/st2 -> st2_4/st2_4/st2)
mod = unit.split("/", 1)[0]
objdiff_unit = f"{mod}/{unit}" if os.path.isdir(os.path.join(ROOT, "config", VER, "modules", mod)) else f"main/{unit}"
r = subprocess.run([os.path.join(ROOT, "build/tools/objdiff-cli"), "diff", "-p", ROOT, "-u", objdiff_unit, a.symbol, "-o", out, "--format", "json"], capture_output=True, text=True)
if not os.path.exists(out):
    print(r.stdout[-2000:], r.stderr[-2000:]); sys.exit("objdiff failed")
d = json.load(open(out)); os.remove(out)
def find(side):
    for s in d[side]["symbols"]:
        if s["name"] == a.symbol: return s
L, R = find("left"), find("right")
symfile = f"config/{VER}/modules/{mod}/symbols.txt" if objdiff_unit.startswith(mod + "/") and mod != "main" else f"config/{VER}/symbols.txt"
if L is None: sys.exit(f"{a.symbol} not in target unit (check the name in {symfile})")
if R is None: sys.exit(f"{a.symbol} not in compiled object (symbol name mismatch? run tools/sync_symbols.py / sync_rel_symbols.py)")
print(f"match: {L.get('match_percent')}  target size {L['size']}  ours size {R['size']}")
def fmt(i):
    ins = i.get("instruction")
    if not ins: return ""
    return f"{int(ins.get('address', 0)):>5x} {ins['formatted']}"
rows = 0
for li, ri in zip(L["instructions"], R["instructions"]):
    kind = li.get("diff_kind") or ri.get("diff_kind") or ""
    kind = kind if kind and kind != "DIFF_NONE" else ""
    if a.all or kind:
        print(f"{'*' if kind else ' '} {fmt(li):<52} | {fmt(ri):<52} {kind.replace('DIFF_', '') if kind else ''}")
        rows += 1
        if rows >= a.n: print("..."); break
