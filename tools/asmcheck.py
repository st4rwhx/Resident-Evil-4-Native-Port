#!/usr/bin/env python3
"""Count the machine instructions that inline asm contributes to a GCC-compiled unit.

usage: asmcheck.py <unit> [<unit> ...]     game/foo.cpp, lib/foo.c, or <mod>/<file>.cpp (REL unit)
       asmcheck.py --all                   every ProDG unit of build.ninja

Method: the unit is copied next to its source with a marker prefixed to the template string of
every `asm("...")` STATEMENT (register pins `asm("r9")` and symbol aliases `asm("name")` have no
whitespace in their template and are left alone; they never emit code). The production compile
command (from `ninja -t commands`) is run on the copy with `--save-asm`; cc1 copies templates into
the .s verbatim, so every marked line is asm-emitted text. Lines whose text is an assembler
directive (`.section`, `.comm`, `.globl`, `.set`, ...), a label, or empty are not counted; any
other marked line is an instruction the source placed by hand.

Exit status 1 when any listed unit emits an instruction from asm. Output: `<unit> <count>` per
unit and the emitted instructions.
"""
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MARK = "@ASMCHECK@"
NOARG = {"blr", "nop", "sync", "isync", "eieio", "sc", "rfi"}
ASM_STMT = re.compile(r'(\basm\s*(?:volatile\s*)?\(\s*)((?:"(?:[^"\\]|\\.)*"\s*)+)')
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def mark_source(src: str) -> str:
    def repl(m):
        lits = LITERAL.findall(m.group(2))  # adjacent literals are one template
        tpl = "".join(lits)
        if not re.search(r"\s", tpl) and tpl.strip() not in NOARG:
            return m.group(0)  # pin or alias
        line = src.count("\n", 0, m.start()) + 1
        mark = f"{MARK}{line}@"  # source line of the asm statement travels with every emitted line
        tpl = tpl.replace("\\n", "\\n" + mark)  # multi-line templates: mark every line
        return f'{m.group(1)}"{mark}{tpl}"'

    return ASM_STMT.sub(repl, src)


_COMMANDS = None


def ngccc_commands():
    """All ngccc compile commands of build.ninja, keyed by source path (REL units of shared
    sources appear once per module; any one of them serves for the asm census)."""
    global _COMMANDS
    if _COMMANDS is None:
        out = subprocess.run(["ninja", "-t", "commands"], cwd=ROOT, capture_output=True, text=True).stdout
        _COMMANDS = {}
        for line in out.splitlines():
            m = re.search(r"tools/ngccc\.py .* -c src/(\S+) -o (build/\S+\.o)", line)
            if m:
                _COMMANDS.setdefault(m.group(1), line.split("&&")[0].strip())
    return _COMMANDS


def compile_cmd(unit: str) -> str:
    cmd = ngccc_commands().get(unit)
    if cmd is None:
        raise SystemExit(f"{unit}: not a ProDG unit (no ngccc command for src/{unit})")
    return cmd


def check(unit: str) -> int:
    cmd = compile_cmd(unit)
    src = ROOT / "src" / unit
    tmp_src = src.with_name(".asmcheck_" + src.name)
    tmp_src.write_text(mark_source(src.read_text(errors="replace")))
    try:
        with tempfile.TemporaryDirectory(prefix="asmcheck.") as tmp:
            s = Path(tmp) / "out.s"
            o = Path(tmp) / "out.o"
            cmd = cmd.replace(f"-c src/{unit}", f"-c {tmp_src.relative_to(ROOT)}")
            cmd = re.sub(r"-o \S+", f"-o {o} --save-asm {s}", cmd, count=1)
            # the marked templates do not assemble; only cc1's .s (saved before ngcas runs) is needed
            rc = subprocess.run(cmd, shell=True, cwd=ROOT, capture_output=True, text=True)
            if not s.exists():
                print(f"{unit} COMPILE-FAILED\n{rc.stderr[-1500:]}")
                return 1
            lines = s.read_text(errors="replace").splitlines()
    finally:
        tmp_src.unlink(missing_ok=True)
    hits = []
    for ln in lines:
        if MARK not in ln:
            continue
        line, text = ln.split(MARK, 1)[1].split("@", 1)
        for part in re.split(r"\s*;\s*|\\n", text.strip()):
            part = part.strip()
            if not part or part.startswith(".") or part.startswith("#") or re.fullmatch(r"[\w.$]+:", part):
                continue
            hits.append((int(line), part))
    print(f"{unit} {len(hits)}")
    for line, h in hits:
        print(f"    {line:5d}  {h}")
    return len(hits)


def all_units():
    return sorted(ngccc_commands())


def main(argv):
    units = all_units() if argv == ["--all"] else argv
    if not units:
        print(__doc__)
        return 2
    total = 0
    for u in units:
        total += check(u)
    print(f"TOTAL {total}")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
