"""Generate NWiiRecomp's re4_symbols.csv from config/RE4DBG/symbols.txt.

NWiiRecomp seeds function discovery from a `Name,Location,Size` CSV and binds
its native HLE handlers by symbol name, so some RE4 names must be renamed
(prefixed `re4_`) before they reach the recompiler:

- names that collide with libc/CRT symbols once the generated C++ is linked
  natively. RE4's SDK carries its own memcpy, abort, __main... __main in
  particular hijacked MinGW's global-constructor runner and crashed before
  main() (the built-in lists below plus rename_crt.txt);
- names nWiiRuntime already defines, so the game's own recompiled version is
  linked instead (rename_runtime.txt). That includes the whole OS heap API:
  the runtime's HLE allocator and the game's OSDestroyHeap/OSCheckHeap cannot
  share state, so RE4 runs its heap fully recompiled.

Usage: py tools/scripts/make_recomp_symbols.py [output.csv]
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SYMBOLS = ROOT / "config" / "RE4DBG" / "symbols.txt"
RECOMP = ROOT / "native" / "recomp"

LIBC = set(
    """abort atan atan2 atof atoi cos exit fabs fflush floor fprintf fread isdigit
    isspace log log10 main memcmp memcpy memmove memset pow printf qsort sprintf
    sqrt sscanf strcat strchr strcmp strcpy strlen strncat strncmp strncpy strrchr
    strstr strtod strtol strtoul tan tolower vfprintf vprintf vsprintf setjmp
    longjmp free malloc calloc realloc time clock rand srand abs toupper""".split()
)
CRT = {"__main", "__do_global_ctors", "__do_global_dtors", "_init", "_fini",
       "_start", "atexit", "__init"}

LINE = re.compile(
    r"^(\S+)\s*=\s*\.(\w+):(0x[0-9A-Fa-f]+);\s*//\s*type:function(?:\s+size:(0x[0-9A-Fa-f]+))?"
)


def main() -> None:
    out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else RECOMP / "re4_symbols.csv"
    rename = LIBC | CRT
    for listing in ("rename_crt.txt", "rename_runtime.txt"):
        rename |= set((RECOMP / listing).read_text(encoding="utf-8").split())

    rows = ["Name,Location,Size"]
    renamed = 0
    for line in SYMBOLS.read_text(encoding="utf-8").splitlines():
        m = LINE.match(line)
        if not m:
            continue
        name, _section, addr, size = m.groups()
        if name in rename:
            name = "re4_" + name
            renamed += 1
        rows.append(f"{name},{int(addr, 16):08X},{int(size, 16) if size else 0}")

    # LF line endings, as in the table the verified build used; newline="\n"
    # stops Windows from turning them into CRLF.
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    print(f"{out}: {len(rows) - 1} functions, {renamed} renamed")


if __name__ == "__main__":
    main()
