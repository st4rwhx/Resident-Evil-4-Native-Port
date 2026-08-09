# Toolchain setup

Nothing here is installed yet — this is the checklist for when we start.
None of these tools require the ISO to install; only using them on real
game data does.

## 1. decomp-toolkit (dtk-rs)

https://github.com/encounter/decomp-toolkit — Rust CLI, install via
`cargo install --git https://github.com/encounter/decomp-toolkit`. Splits
`main.dol` and `.rel` modules, replaces the linker step, drives the
build/diff loop. This is the GC/Wii equivalent of `splat` (splat itself is
N64-focused and not what GC/Wii projects use).

A project template exists at `encounter/dtk-template` — worth starting
from once we're past the research phase, rather than hand-rolling the
`config/` layout.

## 2. objdiff

https://github.com/encounter/objdiff — local GUI/CLI diffing tool, shows
exactly where a recompiled function diverges from the original byte-for-
byte (and instruction-for-instruction when it doesn't fully match). This is
how we know a function is "done."

## 3. Ghidra + GameCube support

Ghidra has no built-in Gekko/Broadway (PowerPC variant used by GC/Wii)
support. Need both:
- https://github.com/Cuyler36/Ghidra-GameCube-Loader — DOL/REL/Apploader
  loader, symbol map import (this is how we'd load `re4sym`'s exported
  `.ghidra.txt` symbols)
- https://github.com/aldelaro5/ghidra-gekko-broadway-lang — language/sla
  spec, covers Gekko-specific instructions (paired-singles, `dcbz_l`) that
  Ghidra's stock PowerPC support doesn't model correctly

## 4. The retail compiler — Metrowerks CodeWarrior for GCN

Byte-matching requires building with the *actual* compiler Capcom used
(`mwcceppc.exe`), not a modern GCC/Clang cross-compiler — CodeWarrior's
codegen/optimizer quirks are exactly what we're trying to reproduce.
Standard approach in every GC/Wii decomp project: run the real
`mwcceppc.exe` through **wibo** (https://github.com/decompals/wibo), a
minimal PE-to-native wrapper (much lighter than Wine) — works on Linux/
macOS, and on Windows it would just run natively anyway. Exact compiler
version needs to be identified once we can read version/build strings out
of the retail DOL.

## 5. ISO handling

- Verify the dump against Redump.org's known-good hash for "Resident Evil
  4 (USA/EUR/JPN)" GameCube before doing anything else with it — a bad dump
  wastes everyone's time chasing phantom mismatches.
- `decomp-toolkit` extracts the DOL + filesystem tree directly from the ISO
  — no separate extraction tool needed for the executable side.

## Order of operations once the ISO is in hand

1. Hash-verify the ISO against Redump.
2. `dtk` extract → get `main.dol`, `.rel` files, raw filesystem tree.
3. Try to recover/import the `re4-research` SYM data (see RESEARCH.md open
   question #3) — do this *before* manually naming anything, it saves a lot
   of duplicate work.
4. Load into Ghidra with the GC loader + Gekko/Broadway language.
5. Set up `dtk-template`-style `config/` and get a "no-op" build producing
   a `main.dol` that's already close to correct (data-only diff) before
   touching any actual function bodies.
6. Pick the smallest/simplest recognizable function (often something like a
   basic math util or a small init routine) as the first matching target,
   to prove the whole pipeline end-to-end.
