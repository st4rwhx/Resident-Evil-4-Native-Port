#!/usr/bin/env python3
"""Post-processes the freshly re4-exported symbols.txt: fixes the .init
section range, re-adds manually-discovered symbols dtk's signature matcher
needs, and sorts by address. Run after every ExportSymbolsForDtk.java pass,
since that script overwrites the file from scratch."""
import re

PATH = r"C:\Users\Admin\Downloads\RE4 PROJECT\re4-gc-decomp\config\RE4DBG\symbols.txt"
INIT_START, INIT_END = 0x80003100, 0x800034A0

# Manually-discovered symbols not in Ghidra's function list (found via a
# "Missing relocation" error from dtk's own SDK signature matcher pointing
# at an unnamed gap in the .text section).
MANUAL_ENTRIES = [
    "OSDefaultExceptionHandler = .text:0x80073150; // type:function size:0x24 scope:global align:4",
]


def main():
    lines = open(PATH, encoding="utf-8").read().splitlines()
    fixed = 0
    out = []
    for l in lines:
        m = re.search(r"\.text:0x([0-9A-Fa-f]+)", l)
        if m:
            addr = int(m.group(1), 16)
            if INIT_START <= addr < INIT_END:
                l = l.replace(f".text:0x{m.group(1)}", f".init:0x{m.group(1)}")
                fixed += 1
        out.append(l)

    out.extend(MANUAL_ENTRIES)

    def addr_of(line):
        m = re.search(r":0x([0-9A-Fa-f]+);", line)
        return int(m.group(1), 16) if m else -1

    pairs = [(addr_of(l), l) for l in out if l.strip()]
    pairs.sort(key=lambda x: x[0])
    open(PATH, "w", encoding="utf-8").write("\n".join(l for _, l in pairs) + "\n")
    print(f"Fixed {fixed} .init entries, added {len(MANUAL_ENTRIES)} manual entries, sorted {len(pairs)} total")


if __name__ == "__main__":
    main()
