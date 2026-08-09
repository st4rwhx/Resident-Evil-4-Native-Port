#!/usr/bin/env python3
"""Computes honest matching-progress stats from config/progress.yaml.

No external dependencies (avoids requiring pyyaml) - the ledger format is
simple enough to parse by hand. Run from anywhere; paths are relative to
this script's location.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LEDGER = ROOT / "config" / "progress.yaml"


def parse_ledger(path):
    text = path.read_text(encoding="utf-8")
    total_main_dol = int(re.search(r"total_functions_main_dol:\s*(\d+)", text).group(1))
    total_all = int(re.search(r"total_symbol_lines_all_modules:\s*(\d+)", text).group(1))
    total_bytes = int(re.search(r"total_function_body_bytes_main_dol:\s*(\d+)", text).group(1))

    entries = []
    for block in re.split(r"\n\s*-\s*name:", text)[1:]:
        name = block.split("\n")[0].strip()
        status = re.search(r"status:\s*(\S+)", block).group(1)
        bytes_m = re.search(r"bytes:\s*(\d+)", block)
        size = int(bytes_m.group(1)) if bytes_m else 0
        entries.append((name, status, size))

    return total_main_dol, total_all, total_bytes, entries


def main():
    total_main_dol, total_all, total_bytes, entries = parse_ledger(LEDGER)

    matched = [e for e in entries if e[1] == "matched"]
    equivalent = [e for e in entries if e[1] == "equivalent"]
    matched_bytes = sum(e[2] for e in matched)
    equivalent_bytes = sum(e[2] for e in equivalent)

    print(f"Main DOL: {total_main_dol} named functions total, {total_bytes:,} bytes of function bodies")
    print(f"All modules combined: {total_all} symbol lines (123 modules)")
    print()
    print(f"Matched (byte-identical):        {len(matched):4d} fns  ({100*len(matched)/total_main_dol:.3f}% of fn count, {100*matched_bytes/total_bytes:.5f}% of code bytes)")
    print(f"Equivalent (logic-verified only): {len(equivalent):4d} fns  ({100*len(equivalent)/total_main_dol:.3f}% of fn count, {100*equivalent_bytes/total_bytes:.5f}% of code bytes)")
    print(f"Touched at all:                   {len(entries):4d} fns  ({100*len(entries)/total_main_dol:.3f}% of fn count, {100*(matched_bytes+equivalent_bytes)/total_bytes:.5f}% of code bytes)")
    print(f"Not yet started:                  {total_main_dol - len(entries)} fns ({100*(total_main_dol-len(entries))/total_main_dol:.3f}%)")
    print()
    print("Matched functions:")
    for name, _, size in matched:
        print(f"  - {name} ({size} bytes)")
    print("Equivalent functions:")
    for name, _, size in equivalent:
        print(f"  - {name} ({size} bytes)")


if __name__ == "__main__":
    main()
