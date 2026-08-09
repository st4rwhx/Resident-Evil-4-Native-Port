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

## Progress — where this project actually is

Run `python tools/scripts/progress_report.py` for current numbers (source
of truth: `config/progress.yaml`, updated every time a function's status
changes). As of the last update:

```
Main DOL: 10383 named functions, 2,199,665 bytes of function bodies
Matched (byte-identical):         3 fns  (0.029% of fn count, 0.00164% of code bytes)
Equivalent (logic-verified only): 5 fns  (0.048% of fn count, 0.00582% of code bytes)
Not yet started:              10375 fns  (99.923%)
```

**Read that plainly: we are at the very start of Phase 1, not close to
"done."** Every function tackled so far was deliberately picked because it
was small and simple, to validate the pipeline and build a track record —
this is not a representative sample of the codebase's difficulty. For
calibration: Metroid Prime's decomp (`PrimeDecomp`, an actively maintained
project with many contributors over several years) was measured at
~56.5% matched via `decomp.dev` when first researched for this project
(see [docs/RESEARCH.md](docs/RESEARCH.md)) — years of sustained multi-
contributor effort to get roughly half done, on a comparable-era GameCube
title. Nobody should read the numbers above as "8% done" or "close" — read
them as "the toolchain and methodology are proven correct; the actual work
of matching ~10,000 functions has barely begun."

### The two separate phases — don't conflate them

1. **Phase 1 (where we are): decompile the original GameCube game.**
   Recover C/C++ source for the ~10,383 functions in the main DOL (plus
   everything in the 11 REL modules) until it matches Capcom's actual
   binary, function by function. This alone is a multi-year undertaking
   at any realistic solo or small-team pace — see
   [docs/METHODOLOGY.md](docs/METHODOLOGY.md) "Realistic expectations."
   **Nothing runs on Windows yet, and nothing will until a meaningful
   fraction of this phase is done** — a "port" needs real source to port.
2. **Phase 2 (not started): port the matched source to native Windows.**
   Once enough of the engine is matched and understood, swap GX (graphics)
   calls for Direct3D/Vulkan, the GameCube audio HW/DSP calls for
   XAudio2/WASAPI, `OSReport`/thread/memory calls for Win32 equivalents,
   etc. — a real, substantial engineering project in its own right, done
   against source we can actually read and trust rather than raw assembly.
   This hasn't begun and can't usefully begin until Phase 1 has real
   coverage of the systems being ported (rendering, audio, input, etc.).

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
- [x] **Byte-perfect matches (3 so far)**: `CameraControl::CurrentAreaNo`,
      `CurrentCameraNo`, `getMotionInfoPtr` — all confirmed byte-identical
      against Ghidra-dumped ground truth. First real proof the full
      pipeline (Ghidra ground truth → hand-written C++ → real CodeWarrior
      compile → verified match) works end to end.
- [x] **`Equivalent` (logic/instructions/order confirmed correct, register
      numbering doesn't match)**: `AreaCheckOnOff`, `Disable`,
      `IsChangeCamera`, `AreaNum`, `setMotionBaseMatPtr`. A real pattern
      is emerging across these — see
      [docs/matching/CameraControl_pointers.md](docs/matching/CameraControl_pointers.md)
      "A real, reproducible pattern" section: register reuse vs. fresh-
      register allocation seems to depend on whether a loaded value is
      used as an arithmetic operand (reused) vs. as the base address of a
      *second* memory access (fresh register in the target, but not
      reproducible from our source so far).
- [ ] ...

## Layout

```
config/   decomp-toolkit / splat config, symbol maps, build config
docs/     methodology, tooling setup, research notes (this is the real content for now)
tools/    downloaded toolchain (Ghidra, decomp-toolkit, wibo, objdiff, CodeWarrior) — gitignored
build/    build output — gitignored
```

No ISO, DOL, REL, or extracted asset ever gets committed — see `.gitignore`.
