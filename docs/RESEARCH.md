# Research notes

## Prior art (checked 2026-09-10)

- **No matching-decompilation project exists for RE4 GameCube.** Checked
  `decomp.dev/projects` (tracks ~66 active GC/Wii decomp projects) and
  GitHub topic search — RE4 GC is not among them. This would be the first.
- **`emoose/re4-research`** (https://github.com/emoose/re4-research) —
  the most relevant existing asset. Not a decompilation, but a research
  toolkit:
  - `re4sym` — parses a **SYM debug symbol file from an RE4 GameCube debug
    build**, reportedly 10,380+ symbols across the main DOL and REL
    overlays (some packed in DRS containers). Exports IDA/Ghidra scripts
    and Dolphin `.map` files. We have the binary (`re4sym.exe`, confirmed
    "by emoose" in its own usage banner) in
    `../JaderLink/re4-research_build-1.3a/`, but **not** the actual `.sym`
    file yet — need to check the GitHub repo's releases/issues, and the
    documented proto/debug builds (see below) for where to actually obtain
    it.
  - `re4lfs` — LFS archive (de)compressor, supports both little-endian
    (PC/PS2-family) and `-x` big-endian (X360) modes. GameCube is
    big-endian PowerPC, so if GC uses LFS too, byte order needs checking
    against an actual GC-sourced `.lfs` file once we have the ISO.
  - `re4resample` — XWB audio resampler; XWB is an XACT wave bank, an
    Xbox/PC format, **not relevant to the GC version** (GC audio is AFC/DSP
    ADPCM via the GC audio HW, different pipeline entirely). Keep for
    reference only, don't expect it to apply to GC assets.
- **JADERLINK tool family** (`RE4-UHD-BIN-TPL-TOOLS`,
  `Re4QuadNewAge`/RE4QuadX) — asset viewers/converters for `.BIN` models
  and `.TPL` textures. TPL is GameCube's native GX texture format, so these
  *can* apply to real GC assets once extracted — useful later for visually
  verifying extracted models/textures, not for code decompilation itself.
- **`fmil95/recvx-gc-decomp`** — an actual matching-decomp project for
  *Resident Evil: Code Veronica X* (GameCube). Different game, same
  publisher/era — worth studying as a template for project structure and
  for any Capcom-GC-era engine conventions that might carry over.
- **TCRF proto/debug builds**: https://tcrf.net/Proto:Resident_Evil_4_(GameCube)
  and an Internet Archive collection
  (https://archive.org/details/resident-evil-4-pre-release-builds) —
  possible source of additional debug strings, symbol maps, or the actual
  `.sym` file referenced by `re4sym`.

## Known file formats (GameCube release)

| Format | Purpose |
|---|---|
| `.lfs` | compressed archive container |
| `.bin` | 3D model geometry |
| `.tpl` | GX-native texture |
| `.das` | stage/scenario data |
| `.esl`/`.ets`/`.ita`/`.aev` | scenario/script files |

Sourced from fan documentation (ZenHAX, TCRF); not yet independently
verified against an actual GC ISO in this project — first thing to confirm
once we have the ISO.

## Open questions to resolve once the ISO is available

1. Does the GC retail disc's filesystem actually match the formats above,
   or did some of these get documented from a later port (PC/PS2/Wii) and
   only partially apply to GC?
2. Is `re4-research`'s LFS handling (endianness, compression variant)
   directly compatible with GC `.lfs` files, or does it need a `-gc` mode
   that doesn't exist yet?
3. Where does the actual debug `.sym` file live — is it in the
   `emoose/re4-research` repo itself (release asset?), or does it need to
   be extracted from one of the TCRF/archive.org proto builds?
