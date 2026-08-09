# RE4 GameCube — Matching Decompilation Project

Goal: reconstruct the C/C++ source of the **2005 Nintendo GameCube release**
of Resident Evil 4 through *matching decompilation* (same methodology as the
Super Mario 64, Zelda OoT/MM, and Metroid Prime decomp projects), well
enough that it recompiles to a byte-identical (or near-identical) `main.dol`.
Once a large fraction of the game matches, the reconstructed source becomes
a real foundation for a native Windows port (swap GX/DSP/OS calls for
DirectX/XAudio2/Win32 equivalents, function by function, with the matched
source as ground truth).

This is **not** the 2007 PC port, and **not** the 2023 RE Engine remake.
Those are different codebases. This project only concerns itself with the
original GameCube binary and its own engine.

## Why matching decomp, not clean-room reimplementation

See [docs/METHODOLOGY.md](docs/METHODOLOGY.md). Short version: matching
decomp gives byte-for-byte verifiable correctness (via `objdiff`) and is the
proven approach for this class of project. Clean-room separation (spec team
never sees the binary / code team never sees original assets) isn't
practical for a single contributor, and matching decomp already sidesteps
the legal concern clean-room exists for — the repo never contains Capcom's
binary or assets, only tooling + reconstructed source that requires the
user's own legally-owned copy of the game to build against.

## Status

- [x] Legally-dumped GameCube ISO of RE4 obtained and hashed against known
      good dumps — verified as a clean Redump match: "Resident Evil 4
      (USA) (Disc 1)", CRC32 `f5c51b40`, SHA-1 `9de89c7f...`
- [x] Toolchain installed (see [docs/TOOLING.md](docs/TOOLING.md)) —
      `decomp-toolkit` (`dtk`) built from source via the GNU Rust toolchain
      (no MSVC Build Tools on this machine; see TOOLING.md note)
- [x] DOL/REL/filesystem extracted (`orig/disc1/`, gitignored) —
      `sys/main.dol` (2.17 MB, SHA-1 `5aa53851...`) + 11 REL modules in
      `files/Rel/` (`Sscrn.rel`, `st1_0..3.rel`, `st2_0..4.rel`, `st4_0.rel`
      — one module per stage/chapter, loaded over the main DOL)
- [ ] Symbol map imported (emoose/re4-research SYM data, if recoverable —
      see [docs/RESEARCH.md](docs/RESEARCH.md)) — **blocking real analysis**:
      `dtk dol info` already fails with overlapping-function errors around
      `0x8005C2E0` when run with no symbols, confirming naive auto-analysis
      isn't reliable on optimized CodeWarrior output
- [ ] First function matched
- [ ] ...

## Layout

```
config/   decomp-toolkit / splat config, symbol maps, build config
docs/     methodology, tooling setup, research notes (this is the real content for now)
tools/    downloaded toolchain (Ghidra, decomp-toolkit, wibo, objdiff, CodeWarrior) — gitignored
build/    build output — gitignored
```

No ISO, DOL, REL, or extracted asset ever gets committed — see `.gitignore`.
