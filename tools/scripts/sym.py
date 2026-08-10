"""Resolve RE4 (GameCube debug build) addresses to symbol+offset.

Usage:  py sym.py 0x80078b44 0x802f95f8 ...
        py sym.py < some_log.txt        (resolves every 0x8xxxxxxx in the text)

Uses config/RE4DBG/symbols.txt, so data labels (queues, globals) resolve too,
not just functions.
"""
import bisect
import re
import sys

SYMBOLS = r"C:/Users/Admin/Downloads/RE4 PROJECT/re4-gc-decomp/config/RE4DBG/symbols.txt"
LINE = re.compile(
    r"^(\S+)\s*=\s*\.(\w+):(0x[0-9A-Fa-f]+);\s*//\s*type:(\w+)(?:\s+size:(0x[0-9A-Fa-f]+))?"
)

syms = []
with open(SYMBOLS, encoding="utf-8", errors="replace") as f:
    for line in f:
        m = LINE.match(line)
        if m:
            name, sec, addr, typ, size = m.groups()
            syms.append((int(addr, 16), int(size, 16) if size else 0, name, typ, sec))
syms.sort()
starts = [s[0] for s in syms]


def resolve(a: int) -> str:
    i = bisect.bisect_right(starts, a) - 1
    if i < 0:
        return "?"
    # Prefer a sized symbol that actually contains the address; unsized labels
    # inside a function would otherwise shadow the function itself.
    for j in range(i, max(-1, i - 64), -1):
        base, size, name, typ, sec = syms[j]
        if size and base <= a < base + size:
            return f"{name}+{a - base:#x}  [{sec} {typ}]"
    base, size, name, typ, sec = syms[i]
    return f"{name}+{a - base:#x}?  [{sec} {typ}, nearest preceding]"


text = " ".join(sys.argv[1:]) if len(sys.argv) > 1 else sys.stdin.read()
seen = set()
for tok in re.findall(r"0x[0-9a-fA-F]{8}", text):
    a = int(tok, 16)
    if a in seen or not (0x80000000 <= a < 0x81800000):
        continue
    seen.add(a)
    print(f"{a:08X}  {resolve(a)}")
