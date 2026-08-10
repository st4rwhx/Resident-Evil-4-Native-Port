# Native build by static recompilation

The native Windows build does not use the matched sources in `src/`: it
recompiles the whole debug DOL (PowerPC -> C++ -> x86-64) with
[NWiiRecomp](https://github.com/BlackLineInteractive/NWiiRecomp). The RE4
fixes to the recompiler and runtime live on the `re4-native` branch of a
local NWiiRecomp clone next to this repo. That repo ignores `*.toml` and
`*.csv`, so the RE4 configuration is kept here.

| File | Purpose |
|---|---|
| `re4_config.toml` | recompiler config (copy to the NWiiRecomp root) |
| `re4_symbols.csv` | function table generated from `config/RE4DBG/symbols.txt` |
| `rename_crt.txt` | names renamed `re4_` because they collide with libc/CRT |
| `rename_runtime.txt` | names renamed so the game's own code wins over the runtime's (incl. the OS heap API) |

Regenerate the table after `symbols.txt` changes, then copy it next to the
config in NWiiRecomp:

```
py tools/scripts/make_recomp_symbols.py
```

## Build and run

1. From the NWiiRecomp root, in PowerShell (the recompiler prints nothing
   from Git Bash): `build\nWiiRecomp\nwiirecomp.exe re4_config.toml`.
2. Copy `export/` to a path without spaces (`windres` breaks on
   "RE4 PROJECT"), copying only files whose content changed so make stays
   incremental.
3. `cmake --build <build dir> -j 4`. The generated CMake defaults to Release
   at -O2; a full build takes about 10 minutes on 4 cores.
4. `RE4Native.exe "<extracted disc dir>"`, with SDL2 and the MinGW runtime
   DLLs next to the exe.

Keyboard: Enter = START, Space = A, arrows = D-pad. The title menu is up
after about 63 s; START on it opens the debug menu.

## Diagnostics

- `NWII_SCREENSHOT=<prefix>` dumps the EFB to BMP every 300 host frames.
  Desktop captures of the window come back black on NVIDIA: judge output
  from these dumps.
- `NWII_THREADS=1` prints every guest thread with its stack back-chain on
  exit. Resolve addresses with `tools/scripts/sym.py`.
- `[PERF]` lines report guest MIPS and the most sampled guest PCs every 5 s.
- `NWII_WATCH=<hex>` (+ `NWII_WATCH_REGS=1`) logs writes to one address.
- `[MMU] write.. to unmapped` flags guest writes below 0x80000000 (a DSI on
  hardware, silently aliased to low memory by the runtime).

## Status

Boots through the Nintendo/Capcom logos to the title menu, takes pad input,
opens the debug menu and loads stage 1 (REL modules run in the runtime
interpreter). Open issues: Sofdec movies decode but their clock stays at 0
(START skips them), and after the door demo the frame shows only the debug
HUD although the game submits thousands of draws.
