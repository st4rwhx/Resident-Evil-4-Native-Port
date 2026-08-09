# Methodology: matching decompilation

## The loop

1. **Split** the retail `main.dol` into per-function/per-object units with
   `decomp-toolkit` (analogous to how `splat` works for N64 projects).
2. **Disassemble** each function to PowerPC assembly (Ghidra, with GameCube
   support via `Cuyler36/Ghidra-GameCube-Loader` +
   `aldelaro5/ghidra-gekko-broadway-lang`, since Ghidra has no built-in
   Gekko/Broadway support).
3. **Rewrite** the function in C/C++, guessing at the original source given
   the disassembly, known engine conventions, and any recovered symbol
   names/types.
4. **Recompile** that single translation unit with the *actual retail
   compiler* — Metrowerks CodeWarrior for GCN (`mwcceppc.exe`), run via
   `wibo` so it runs natively on Linux/macOS without a Windows install (and
   under Windows it just runs directly).
5. **Diff** the recompiled bytes against the original function bytes with
   `objdiff`. Iterate on the C source (not the assembly!) until it matches
   byte-for-byte, including instruction *scheduling* — CodeWarrior's
   optimizer is quirky and getting a "functionally equivalent" match often
   still isn't a byte match. That's the whole point of the exercise: if it
   matches, we know with certainty we reconstructed the real logic (or at
   least something the compiler treats identically).
6. Repeat, function by function, module by module (main DOL first, then
   each REL overlay).

## Why this is worth the pain over clean-room reimplementation

- **Verifiable correctness.** A byte match is proof, not a guess. A
  clean-room reimplementation can *look* right and still diverge on edge
  cases (frame-perfect timing, RNG, floating point rounding) that matter a
  lot for a game like this (enemy AI timing, QTEs, physics).
- **Legal posture is already clean.** The concern clean-room process exists
  to address — "did the author copy copyrighted code" — doesn't bite here
  because matching-decomp repos never contain Capcom's binary, assets, or
  even the disassembly itself long-term; they contain original prose (C
  source) that a user's own legally-obtained copy of the game is needed to
  build against and verify. This is the exact model every existing N64/GC
  matching-decomp project on GitHub operates under, unchallenged for years.
- **Directly reusable for the Windows port.** Once function `FUN_80123456`
  is matched and renamed to e.g. `Player_UpdatePhysics`, porting it to
  Windows is a normal refactor (swap `GXSetTexCoordGen` for a D3D/Vulkan
  call, swap `OSReport` for a normal logger, etc.) done against source you
  can actually read and trust, instead of reverse-engineering assembly
  every single time you touch a system.

## Realistic expectations

No prior decompilation work exists for this specific game (confirmed —
`decomp.dev` tracks ~66 GC/Wii projects and RE4 GC isn't one of them, see
[RESEARCH.md](RESEARCH.md)). The closest comparable finished/active project
is Metroid Prime (`PrimeDecomp`), still around ~56% matched after years of
sustained community effort. RE4 GC is a full 6th-gen single-player action
game (AI, physics, cutscenes, inventory/merchant systems, QTEs) — expect a
similarly large, multi-year, likely multi-contributor effort *if* pursued
to completion. The first few weeks (ISO extraction, DOL/REL split, symbol
import, first matched function) are realistic to reach solo; "most of the
game matches" is not a solo weekend project, and anyone telling you
otherwise is not being straight with you.

One real advantage here: a GameCube **debug build with ~10,380+ recovered
SYM symbols** has reportedly been documented by `emoose/re4-research` (see
RESEARCH.md) — if we can get our hands on that actual `.sym` file, it gives
real function names/addresses instead of starting from nothing, which is a
significant head start relative to most decomp projects at day one.
