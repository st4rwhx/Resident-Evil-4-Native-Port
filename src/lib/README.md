# src/lib — Nintendo Dolphin SDK (MWCC-compiled) and runtime objects

The `*.c` files here with SDK object names (OS*, GX*, dvd*, CARD*, AX*, mtx*, ...) are the
Dolphin SDK libraries the game was linked against (SDK 2004, OS = May 21 2004 patch 1, other
libs Apr 5-7 2004, build id 0x2301). They are built with Metrowerks CodeWarrior GC/1.2.5n
(`DolphinLib` in `configure.py`), headers live in `include/dolphin/`, `include/libc/`,
`include/charPipeline/`.

Sources were taken from the public matching decompilations and adapted where this build differs:

- [doldecomp/dolsdk2004](https://github.com/doldecomp/dolsdk2004) (`src/<lib>/*.c`, `include/`)
  — all SDK libraries except `DebuggerDriver.c`.
- [mariopartyrd/partyboard](https://github.com/mariopartyrd/partyboard)
  (`src/OdemuExi2/DebuggerDriver.c`) — the OdemuExi2 debugger driver.

Like those repositories, this code is a reverse-engineered reconstruction of Nintendo's SDK and
is provided for research purposes only.

The game's linker (SN ProDG) dead-stripped unreferenced SDK symbols; `tools/strip_unused.py`
(run automatically after each MWCC compile) removes the same functions/data from our objects so
they can be linked byte-for-byte. Which symbols survive is read from `config/G4BE08/sym_map.tsv`.
