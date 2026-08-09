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
- [x] Symbol map recovered — obtained the "Debug Disc 1" (2004-11-25)
      GameCube prototype, extracted it, found 123 `Bio4.*.sym` files
      (confirmed genuine: Hidden Palace explicitly documents this exact
      prototype as containing them), ran `re4sym` on all of them.
      **10,383 symbols** for the main executable alone (real C++
      `Class::Method` names, e.g. `CameraControl::HermiteExport`), plus
      full symbol coverage for every REL overlay *and* dozens of
      enemy/player/weapon/UI overlay modules not present on the retail
      disc at all (~25,974 symbol lines total). Exported as Dolphin `.map`
      + Ghidra import scripts in `config/symbols/`.
- [x] Ghidra set up with GameCube loader + symbols imported — Ghidra
      12.1.3 + `Cuyler36/Ghidra-GameCube-Loader` (built from source, JDK 21
      GNU toolchain) + Gekko/Broadway language, all headless (no GUI
      interaction available/needed). Debug `main.dol` imported, fully
      auto-analyzed (64s), all 10,383 symbols imported (0 skipped, 578
      new functions created), verified with a sample decompile —
      `CameraControl::AreaCheckOnOff`, `cameraHitCheck` etc. all produce
      clean, real pseudo-C referencing other named functions/globals
      (`EmHitCheck`, `cSatMgr::hitCheck`, `PSVECDistance`, `SatMgr`...).
      Reusable headless scripts: `tools/scripts/ImportRE4Symbols.java`,
      `tools/scripts/DecompileSample.java`.
- [ ] Reconcile debug-build addresses/symbols against the retail DOL (debug
      main.dol is 2.55 MB vs retail's 2.17 MB — different layout, so
      addresses won't line up 1:1; names should still transfer function-by-
      function once matched)
- [x] CodeWarrior compiler obtained — `files.decomp.dev/compilers_20251118.zip`
      (same archive PrimeDecomp/zeldaret/doldecomp use), extracted to
      `tools/compilers/GC/*` (gitignored, ~77 MB). `objdiff-cli` v3.8.1
      also fetched. Both run natively on Windows (no `wibo` needed here).
- [~] First matching attempt: `CameraControl::AreaCheckOnOff` — status
      **`Equivalent`** (zeldaret/tww terminology: logic 100% confirmed
      correct via 40+ tested combinations, not yet byte-exact — 52/56
      bytes). Root cause identified via real research, not guesswork: an
      `or. rX,rX,rX` vs `cmpwi` instruction-selection choice that's
      register-scheduler-emergent, not controllable from C source alone —
      confirmed by finding the exact same idiom in this binary's own
      linked-in `strncmp` (Metrowerks standard library, not Capcom code)
      and then *failing* to reproduce it with a hand-written function of
      identical shape. This specific class of problem is explicitly
      documented as a known limitation by real GC/Wii decomp projects
      (`mwcc-rs`, Persona4-Decompilation, zeldaret/tww's own `Equivalent`
      status convention, which this project now follows). Full log:
      [docs/matching/CameraControl_AreaCheckOnOff.md](docs/matching/CameraControl_AreaCheckOnOff.md).
      Current best-effort source:
      [src/camera/CameraControl.cpp](src/camera/CameraControl.cpp).
- [ ] ...

## Layout

```
config/   decomp-toolkit / splat config, symbol maps, build config
docs/     methodology, tooling setup, research notes (this is the real content for now)
tools/    downloaded toolchain (Ghidra, decomp-toolkit, wibo, objdiff, CodeWarrior) — gitignored
build/    build output — gitignored
```

No ISO, DOL, REL, or extracted asset ever gets committed — see `.gitignore`.
