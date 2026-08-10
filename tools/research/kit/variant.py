#!/usr/bin/env python3
"""Compile a VARIANT source of one re4 unit with the unit's exact production command, never
touching build/, and judge it.  Backs variant.sh and rtl.sh (see README.md).

  variant.py <unit> <variant-src> [FUNC...] [--out DIR] [--cc1dir DIR] [--all] [--no-diff] [--dtk]
  variant.py --rtl <unit> <variant-src> [--out DIR] [--cc1dir DIR] [-dX ...]

--out DIR   where the object/dumps go (default: a fresh /tmp/kit.$USER/<unit>.XXXX)
--all       side-by-side with all lines, not only the differing ones
--no-diff   bytecmp only
--dtk       side-by-side from `dtk elf disasm` of the variant vs build/G4BE08/asm/<unit>.s instead
            of objdiff-cli (dtk shows relocation ADDENDS in the instruction fields of a relocatable
            object: branch targets/@sda21 immediates are garbage there -- objdiff resolves them)

unit        game/title, lib/sfd_tim (DOL) or <mod>/<file> (REL: st2_1/r20e, t_esp/db_widget)
variant-src a copy of the unit's source with your edit (any path; it is copied under OUT/src/<orig
            basename> so `#include "x.h"` next to the original still resolves via an added -I)
FUNC        mangled symbol(s) to word-diff and disassemble side by side; default: every function
            bytecmp reports as differing (at most 6)

The production command is `ninja -t commands build/G4BE08/src/<unit>.o` (last line), tokenised,
the source/object paths redirected to OUT, relative tool paths made absolute; `&&`-chained
post-processing (fold_linkonce.py / strip_unused.py) is re-run on the OUT object; dependency-file
steps are dropped.  GCC units go through tools/ngccc.py in-process (its wibo hang timeout + retry
and the linkonce placement keyed on the ORIGINAL unit path); MWCC units run the wibo command with
the same 120 s x3 timeout.  CC1DIR=<dir with cc1plus/cc1> (or --cc1dir) swaps the GCC compiler
proper, e.g. the hooked build from tools/research/sngdbg; env vars such as GDBG/LADBG/NOMALLOC
pass straight through.  RE4_ROOT=<dir> overrides the repo root (default: the git toplevel).
"""
import difflib
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def die(msg):
    print(f"variant: {msg}", file=sys.stderr)
    sys.exit(2)


def _repo_root():
    env = os.environ.get("RE4_ROOT")
    if env:
        return Path(env)
    top = subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True,
                         cwd=Path(__file__).resolve().parent)
    if top.returncode != 0:
        die("not inside the re4 repo; set RE4_ROOT")
    return Path(top.stdout.strip())


ROOT = _repo_root()
VER = os.environ.get("RE4_VERSION", "G4BE08")
BUILD = ROOT / "build" / VER
WIBO_TIMEOUT = 120
RTL_FLAGS = ["-dj", "-ds", "-dS", "-dR", "-dg", "-dl", "-dG", "-dL", "-fsched-verbose-9"]


def is_module(mod):
    return (ROOT / "config" / VER / "modules" / mod).is_dir()


def unit_paths(unit):
    """-> (obj under build/, target asm .s)"""
    mod, _, f = unit.partition("/")
    obj = BUILD / "src" / (unit + ".o")
    if is_module(mod):
        asm = BUILD / mod / "asm" / mod / (f + ".s")
    else:
        asm = BUILD / "asm" / (unit + ".s")
    return obj, asm


def production_command(obj):
    r = subprocess.run(["ninja", "-t", "commands", str(obj.relative_to(ROOT))], cwd=ROOT,
                       capture_output=True, text=True)
    if r.returncode != 0 or not r.stdout.strip():
        die(f"ninja -t commands failed for {obj}: {r.stderr.strip()}")
    return r.stdout.strip().splitlines()[-1]


def split_segments(cmd):
    """shlex-tokenise and split on `&&` (also the attached form `-lang=c++&&`)."""
    segs, cur = [], []
    for tok in shlex.split(cmd):
        if tok == "&&":
            segs.append(cur); cur = []
        elif tok.endswith("&&") and len(tok) > 2:
            cur.append(tok[:-2]); segs.append(cur); cur = []
        else:
            cur.append(tok)
    if cur:
        segs.append(cur)
    return segs


def absolutise(tok):
    p = ROOT / tok
    if not tok.startswith(("-", "/")) and p.exists():
        return str(p)
    return tok


def kind_of(seg):
    s = " ".join(seg)
    if "ngccc.py" in s:
        return "gcc"
    if "mwcceppc.exe" in s:
        return "mwcc"
    die(f"unknown compiler in: {s[:120]}")


def run_wibo(cmd, cwd):
    """wibo (mwcc/cpp/as) occasionally hangs at 0% CPU; kill after WIBO_TIMEOUT and retry (ngccc.py)."""
    for attempt in range(3):
        try:
            return subprocess.run(cmd, cwd=cwd, timeout=WIBO_TIMEOUT).returncode
        except subprocess.TimeoutExpired:
            print(f"variant: {Path(cmd[0]).name} hung for {WIBO_TIMEOUT}s, retry {attempt + 1}/3",
                  file=sys.stderr)
    die("wibo hung three times")


def compile_variant(unit, variant_src, out, cc1dir=None, extra_cc1=(), rtl=False):
    obj, _ = unit_paths(unit)
    segs = split_segments(production_command(obj))
    comp = [absolutise(t) for t in segs[0]]
    kind = kind_of(comp)
    src_i = comp.index("-c") + 1
    orig_src = ROOT / segs[0][src_i]
    src_dir = orig_src.parent
    out.mkdir(parents=True, exist_ok=True)
    (out / "src").mkdir(exist_ok=True)
    vsrc = out / "src" / orig_src.name
    if Path(variant_src).resolve() != vsrc.resolve():
        shutil.copyfile(variant_src, vsrc)
    stem = orig_src.stem
    vobj = out / (stem + ".o")
    comp[src_i] = str(vsrc)
    o_i = comp.index("-o") + 1

    if kind == "gcc":
        comp[o_i] = str(vobj)
        # same-directory `#include "x.h"` of the original source: first -I
        first_I = next(i for i, t in enumerate(comp) if t == "-I" or t.startswith("-I"))
        comp[first_I:first_I] = ["-I", str(src_dir)]
        if cc1dir:
            comp[comp.index("--native-dir") + 1] = str(Path(cc1dir).resolve())
        argv = comp[comp.index("--prodg-dir"):] + list(extra_cc1)
        sys.path.insert(0, str(ROOT / "tools"))
        import ngccc  # noqa: E402
        orig_place = ngccc.place_linkonce
        ngccc.place_linkonce = lambda src, asm: orig_place(str(orig_src), asm)
        if rtl:
            # keep the .i and every -dX dump: cc1plus writes them next to the .i, in OUT
            import contextlib
            ngccc.tempfile.TemporaryDirectory = lambda prefix="": contextlib.nullcontext(str(out))
        print("+ ngccc.py " + " ".join(shlex.quote(a) for a in argv))
        rc = ngccc.main(argv)
    else:
        if rtl:
            die("MWCC has no RTL dumps; use tools/research/mwccdbg (ra.py / rasum.py / rasim.py / chaitin.py)")
        comp[o_i] = str(out)  # mwcc -o takes the directory; the object is OUT/<stem>.o
        comp.insert(comp.index("-I-") + 1, "-i")
        comp.insert(comp.index("-I-") + 2, str(src_dir))
        print("+ " + " ".join(shlex.quote(a) for a in comp))
        rc = run_wibo(comp, ROOT)
    if rc != 0 or not vobj.exists():
        die(f"compile failed (rc {rc})")

    obj_rel = str(obj.relative_to(ROOT))
    for seg in segs[1:]:
        if any("transform_dep.py" in t for t in seg):
            continue  # dependency files are irrelevant for a variant
        seg = [str(vobj) if t in (obj_rel, str(obj)) else absolutise(t) for t in seg]
        print("+ " + " ".join(shlex.quote(a) for a in seg))
        r = subprocess.run(seg, cwd=ROOT)
        if r.returncode != 0:
            die(f"post-processing failed: {seg[1] if len(seg) > 1 else seg[0]}")
    return vobj


def bytecmp(unit, vobj, funcs):
    env = dict(os.environ, OBJ=str(vobj))
    r = subprocess.run([sys.executable, str(ROOT / "tools/bytecmp.py"), unit, *funcs], cwd=ROOT,
                       env=env, capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    if r.stderr:
        sys.stderr.write(r.stderr)
    diff_funcs = re.findall(r"^\s+0x[0-9a-f]+ (\S+): (\d+) words", r.stdout, re.M)
    return [f for f, _ in diff_funcs]


def disasm(obj, out_s):
    r = subprocess.run([str(ROOT / "build/tools/dtk"), "elf", "disasm", str(obj), str(out_s)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        die(f"dtk elf disasm failed: {r.stderr.strip()}")
    return out_s.read_text(errors="replace")


FN_RE = re.compile(r"^\.fn (\S+), .*?$(.*?)^\.endfn", re.S | re.M)
INSN_RE = re.compile(r"^/\* ([0-9A-F]{8}) [0-9A-F]{8}  ((?:[0-9A-F]{2} ){4})\*/\t(.*)$")


def function_block(text, name):
    for m in FN_RE.finditer(text):
        if m.group(1) == name:
            return m.group(2)
    return None


def normalise(block):
    """dtk lines -> function-relative `+off  bytes  insn` with labels/branch targets made relative."""
    lines = block.strip("\n").split("\n")
    base = None
    for ln in lines:
        m = INSN_RE.match(ln)
        if m:
            base = int(m.group(1), 16); break
    if base is None:
        return lines
    out = []
    for ln in lines:
        m = INSN_RE.match(ln)
        if m:
            off = int(m.group(1), 16) - base
            insn = re.sub(r"\.L_([0-9A-F]{8})", lambda k: f".L+{int(k.group(1), 16) - base:x}", m.group(3))
            insn = re.sub(r"\.\.\.data|\.\.\.rodata", lambda k: k.group(0), insn)
            out.append(f"+{off:04x}  {m.group(2).strip()}  {insn}")
        elif re.match(r"^\.L_[0-9A-F]{8}:", ln):
            out.append(f".L+{int(ln[3:11], 16) - base:x}:")
        else:
            out.append(ln)
    return out


def side_by_side(tl, ol, show_all, width=56):
    sm = difflib.SequenceMatcher(None, tl, ol, autojunk=False)
    rows = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            if show_all:
                for k in range(i2 - i1):
                    print(f"  {tl[i1 + k]:<{width}} | {ol[j1 + k]}")
            continue
        n = max(i2 - i1, j2 - j1)
        for k in range(n):
            l = tl[i1 + k] if i1 + k < i2 else ""
            r = ol[j1 + k] if j1 + k < j2 else ""
            print(f"* {l:<{width}} | {r}")
            rows += 1
    return rows


def fdiff_dtk(unit, vobj, funcs, out, show_all):
    """`dtk elf disasm` of the variant object vs build/G4BE08/asm/<unit>.s.  dtk renders a
    RELOCATABLE object with the relocation addends in the instruction fields (branch targets and
    @sda21/@l immediates come out as garbage), so this is only a fallback (--dtk); the default
    side-by-side is objdiff-cli, which resolves relocations (what tools/fdiff.py uses)."""
    _, tasm = unit_paths(unit)
    if not tasm.exists():
        print(f"variant: no target asm {tasm}; skipping disassembly diff")
        return
    ttext = tasm.read_text(errors="replace")
    otext = disasm(vobj, out / "ours.s")
    for fn in funcs:
        tb, ob = function_block(ttext, fn), function_block(otext, fn)
        print(f"\n=== {fn}: target (left) | variant (right); only differing lines{' and context' if show_all else ''}")
        if tb is None:
            print(f"  not in target asm {tasm}"); continue
        if ob is None:
            print(f"  not in the variant object (placeholder name? see tools/sync_symbols.py)"); continue
        tl, ol = normalise(tb), normalise(ob)
        print(f"  target {len([l for l in tl if l.startswith('+')])} insns, variant "
              f"{len([l for l in ol if l.startswith('+')])} insns")
        if side_by_side(tl, ol, show_all) == 0:
            print("  (disassembly identical; any residue is in relocations/data -- see bytecmp above)")


def target_obj(unit):
    mod, _, f = unit.partition("/")
    if is_module(mod):
        return BUILD / mod / "obj" / mod / (f + ".o")
    return BUILD / "obj" / (unit + ".o")


def fdiff(unit, vobj, funcs, out, show_all, nlines=int(os.environ.get("KIT_NLINES", 400))):
    """objdiff-cli one-shot diff of each function: target split object (left) vs the variant
    object (right), relocations resolved symbolically; the tools/fdiff.py format."""
    import json
    tobj = target_obj(unit)
    if not tobj.exists():
        print(f"variant: no target object {tobj}; skipping disassembly diff")
        return
    for fn in funcs:
        js = out / f"fdiff.{fn[:60]}.json"
        r = subprocess.run([str(ROOT / "build/tools/objdiff-cli"), "diff", "-1", str(tobj), "-2", str(vobj),
                            fn, "-o", str(js), "--format", "json"], capture_output=True, text=True)
        print(f"\n=== {fn}: target (left) | variant (right); {'all' if show_all else 'only differing'} lines")
        if not js.exists():
            print(f"  objdiff failed: {(r.stdout + r.stderr).strip()[-400:]}"); continue
        d = json.loads(js.read_text())

        def find(side):
            for s in d[side]["symbols"]:
                if s["name"] == fn:
                    return s
        L, R = find("left"), find("right")
        if L is None:
            print(f"  {fn} not in the target object"); continue
        if R is None:
            print(f"  {fn} not in the variant object (placeholder name? see tools/sync_symbols.py)"); continue
        print(f"  match: {L.get('match_percent')}  target size {L['size']}  variant size {R['size']}")

        def fmt(i):
            ins = i.get("instruction")
            return f"{int(ins.get('address', 0)):>5x} {ins['formatted']}" if ins else ""
        rows = 0
        for li, ri in zip(L["instructions"], R["instructions"]):
            kind = li.get("diff_kind") or ri.get("diff_kind") or ""
            kind = kind if kind and kind != "DIFF_NONE" else ""
            if show_all or kind:
                print(f"{'*' if kind else ' '} {fmt(li):<52} | {fmt(ri):<52} {kind.replace('DIFF_', '')}")
                rows += 1
                if rows >= nlines:
                    print("..."); break


def main(argv):
    rtl = False
    if argv and argv[0] == "--rtl":
        rtl = True; argv = argv[1:]
    out = keep = show_all = no_diff = use_dtk = None
    cc1dir = os.environ.get("CC1DIR")
    pos, extra = [], []
    i = 0
    while i < len(argv):
        a = argv[i]; i += 1
        if a == "--out":
            out = Path(argv[i]); i += 1
        elif a == "--cc1dir":
            cc1dir = argv[i]; i += 1
        elif a == "--keep":
            keep = True
        elif a == "--all":
            show_all = True
        elif a == "--no-diff":
            no_diff = True
        elif a == "--dtk":
            use_dtk = True
        elif a.startswith("-") and rtl:
            extra.append(a)
        elif a.startswith("-"):
            die(f"unknown option {a}")
        else:
            pos.append(a)
    if len(pos) < 2:
        die(__doc__)
    unit = re.sub(r"\.(cpp|c)$", "", pos[0])
    variant_src, funcs = pos[1], pos[2:]
    if not Path(variant_src).exists():
        die(f"no such file {variant_src}")
    if out is None:
        base = Path(f"/tmp/kit.{os.environ.get('USER', 'u')}")
        base.mkdir(exist_ok=True)
        out = Path(tempfile.mkdtemp(prefix=unit.replace("/", "_") + ".", dir=base))
    if rtl:
        vobj = compile_variant(unit, variant_src, out, cc1dir, extra or RTL_FLAGS, rtl=True)
        dumps = sorted(p.name for p in out.iterdir() if ".i." in p.name or p.suffix in (".i", ".s"))
        print(f"\nRTL dumps in {out}:\n  " + "\n  ".join(dumps))
        return 0
    vobj = compile_variant(unit, variant_src, out, cc1dir)
    print(f"\nobject: {vobj}")
    diff_funcs = bytecmp(unit, vobj, funcs)
    if not no_diff:
        (fdiff_dtk if use_dtk else fdiff)(unit, vobj, funcs or diff_funcs[:6], out, show_all)
    print(f"\n(out dir: {out})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
