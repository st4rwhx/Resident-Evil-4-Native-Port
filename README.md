# Resident Evil 4 (GameCube) — decompilation

A complete, byte-identical decompilation of *Resident Evil 4* for the Nintendo GameCube: the
`G4BE08` **debug build** (the "Nov 25 2004" prototype, both discs), whose `Bio4.sym` files name every
function. Building the repository reproduces `main.dol` and all 110 REL overlays exactly
(`config/G4BE08/build.sha1`, checked on every build).

| | |
|---|---|
| Units | 992 (673 in the DOL, 319 across the RELs), all matching |
| Source | ~523k lines of C/C++ (`src/`), ~33k lines of headers (`include/`) |
| Game code | SN Systems ProDG 3.9.3 — GCC 2.95.3 "SN BUILD v1.79", built natively from SN's GPL source drop |
| CRI middleware (`src/lib/adx_*`, `sfd_*`, `mpv_*`, …) | Metrowerks CodeWarrior 2.4.7 (GC/2.7), the compiler CRI shipped the libraries with |
| Nintendo SDK (`src/lib/OS*`, `GX*`, …) | Metrowerks CodeWarrior GC/1.2.5n, sources from [dolsdk2004](https://github.com/doldecomp/dolsdk2004) |

The repository contains no game assets and no code or data copied from the disc. You need your
own image of the debug disc to build; the original files are read from it at configure time.

## Building

Linux, Python 3, [ninja](https://ninja-build.org/). Compilers and tools (decomp-toolkit, objdiff,
wibo, the CodeWarrior builds) are downloaded by the first configure run, except the native SN GCC:

```sh
# 1. the native cc1/cc1plus (once): needs SN's GPL source drop, see tools/sn-gcc/build.sh
SN_GCC_SRC=/path/to/NGC_GNU_SRC/NGC tools/sn-gcc/build.sh

# 2. your disc images (disc 1: main.dol + 110 RELs; disc 2: the four island-stage RELs st3_0..st3_3)
cp re4_debug_disc1.iso re4_debug_disc2.gcm orig/G4BE08/

# 3. build and verify
python3 configure.py && ninja
```

`ninja` ends with the progress report (100% matched and linked for the DOL and the REL modules);
`build/tools/dtk shasum -c config/G4BE08/build.sha1` prints 111 `OK` lines. To work on a unit, `python3 tools/bytecmp.py game/foo` compares its object with
the original word by word and `python3 tools/fdiff.py game/foo <symbol>` shows one function.

## Layout

- `src/game/` — the game (C++; a few newlib C units). `src/em*/` enemies, `src/wep*/` weapons,
  `src/pl*/` player characters, `src/st*/` rooms (one REL per room), `src/t_*/`, `src/Tools/`,
  `src/tools/` the in-game debug editors, `src/Sscrn/` the sub-screens, `src/lib/` SDK, CRI and runtime.
- `include/` — headers, including the reconstructed struct layouts.
- `config/G4BE08/` — unit lists (`objects.py`, `modules.py`), `symbols.txt`, `splits.txt`, linker
  scripts, per-module REL data (`modules/<mod>/`), `build.sha1`.
- `tools/` — build generator (`project.py`), the ProDG driver (`ngccc.py`), REL rebuild (`make_rel.py`,
  `link_rel.py`), the compare tools, `sn-gcc/` (native compiler build), `research/` (compiler-analysis kit).
- `docs/matching.md` — how the matching was done: compiler provenance, the catalogue of compiler
  mechanisms and the source shapes that reproduce them, rules of thumb for both compilers.
  `docs/unit-notes.md` — per-unit notes. `docs/research/` — the pass-by-pass research log.

## What "matching" means here

Every unit compiles to the original bytes with the original compilers. Where the compiler needed a
particular source shape to reproduce a register choice or a schedule and no natural spelling was
found, the construct is marked with a `// COMPILER-DIFF:` comment (644 of them: dead tests, empty
`asm("")` launders and anchors, `register T x asm("rN")` pins, padding statements). None of them
emits an instruction: `python3 tools/asmcheck.py --all` compiles every GCC unit with its asm templates
marked and lists the instructions that came from a template — the only hits are the hardware kernels
below. An earlier state of this tree had ~100 hand-placed instructions (`asm("li %0,0")`,
`asm("lis/addi")`, `asm("mr")`) in the game code and ~100 register-pinning `asm { }` blocks in the CRI
libraries; they were replaced by C on 2026-09-17 (`docs/research/asm-removal.md` records the recipe
and the compiler mechanism per site). Each tag's mechanism is documented in `docs/matching.md` and
`docs/research/`.

Assembly that remains, all of it code the original authors also wrote in assembly because their
compilers had no other way to express it:

- GCC 2.95 game code: paired-single kernels (`SINF`/`COSF`/`RSQRT`/`LIMIT_ANGLE` in `math_sub`, the
  matrix kernels in `trans`, `shape`, `dbmodule`, quantised `psq_l` in `Espgen42`/`espgen45`), the
  GQR setup in `main`/`scheduler`, and the libsn `sndvd` exception handler.
- MWCC CRI libraries: the paired-single / cache / SPR kernels (`mpv_umc`, `mpv_mc`, `dct_fsri`,
  `cftyp422_ppc`, `mpv_lib`), the SDK's `mtx`/`vec`/`quat`/`GX` intrinsics, and one register-steering
  block in `dct_ac` (`dctac_Init`: the vendor's compiler build pooled `.bss` but not the function's
  8-byte literals; ours pools both). Codeless `asm { mr r11, x; mr x, r11 }` pins (both moves are
  deleted by the allocator; they narrow the colour set by one register) and `asm { mr v, v }` self
  copies (an opaque second definition) remain in 27 places.
- Eight `.s` units: crt0 (`__start`), `eabi`, SN's `tealeaf`/`fileserver`/`ppcdown`/`proview`, and
  Capcom's `memset_2` and `yz2asm`.

## Legal

The reconstructed game and SDK source is the intellectual property of its respective owners
(Capcom, Nintendo, CRI Middleware) and is published for research and preservation only. The build
scripts, tools and documentation written for this project are released under CC0 (`LICENSE`).
