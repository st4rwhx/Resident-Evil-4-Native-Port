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

## SYM file location — resolved (2026-09-10)

Checked `emoose/re4-research`'s README, all 4 GitHub releases, commit
history, and the `retroReversing` PR that documents the SYM discovery
(https://github.com/emoose/retroReversing/commit/03d950c4c5b05b37f0e7cd91440f39b84f8892a4,
mirrored at https://www.retroreversing.com/gamecube-debug-symbols).

- **Confirmed**: `re4sym` reads `Bio4.*.SYM` files "included with the RE4 GC
  debug build," covering the main DOL + REL overlays (some RELs packed in
  DRS containers), ~10,380+ symbols total. Even emoose says the origin of
  who generated these SYMs is unknown.
- **Not found**: no standalone `.sym` file is shipped in any release asset,
  archive.org item, or forum post I could locate. It is not a separate
  downloadable artifact.
- **Inferred (unverified) most likely path**: the SYM files live *inside* a
  debug-build GameCube disc image — e.g. the "Resident Evil 4 Debug (Disc 1)"
  prototype documented on Hidden Palace
  (https://hiddenpalace.org/File:Resident_Evil_4_Debug_(Disc_1).7z) and
  mirrored on Internet Archive as "Resident Evil 4 - Debug Disc 1.iso"
  (https://archive.org/details/resident-evil-4-pre-release-builds) — you'd
  extract that disc's filesystem and look for `Bio4.*.SYM` files directly,
  the same way we extracted the retail DOL.

**Which specific prototype — resolved (2026-09-10)**: there are at least two
distinct GameCube "Debug" builds, not one:
- **"Resident Evil 4 - Debug Disc 1" (2004-11-25, DOL timestamp
  10:19:49)** — full game, ~1.4 GB ISO. Hidden Palace's own page for this
  exact prototype states directly: *"Includes debugging symbols for the
  main executable and overlays (function names only), can be extracted
  with re4sym tool."* (https://hiddenpalace.org/Resident_Evil_4_(Nov_25,_2004_prototype))
  — **this is source-confirmed as the target build**, not a guess. A Disc 2
  from the same prototype may also exist (passing mention only, unverified).
- **"Debug E3" (2004-09-02)** — much smaller (144.8 MB), E3-demo scale.
  No confirmation either way that it carries SYM data; likely a stripped
  show build. Not the target.

**Important distinction from the retail ISO**: the retail disc we're
building against is something the user dumped from their own legally
purchased copy — clean legal footing. A *debug/prototype build* was never
sold at retail; it only exists because it leaked out of Capcom/Nintendo's
internal QA pipeline. Preservation sites like Hidden Palace/TCRF host these
for archival/research purposes, but "own a legit copy" doesn't apply the
same way here. Whether to pursue this path is the user's call, not
something to grab automatically.

## Symbols obtained — 2026-09-10

User supplied "Resident Evil 4 Debug (Disc 1).iso" (1.36 GB, raw ISO, not
Redump-verifiable since it's an unreleased prototype — that's expected).
Extracted with `dtk disc extract`. Found **123 `Bio4.*.sym` files** at
`files/` root of the debug disc, confirming Hidden Palace's documentation.
Ran `re4sym` (from `JaderLink/re4-research_build-1.3a/`, confirmed to be
emoose's real tool) on all of them.

Results copied into `config/symbols/*.dolphin.map` and `*.ghidra.txt`
(2.0 MB total, 246 files — kept in the repo, not gitignored, since this is
derived name/address metadata, not the binary/ISO itself; this matches
established practice in other GC/N64 decomp communities that have
recovered leaked debug symbol data, e.g. Melee, Perfect Dark).

- `Bio4.sym` (main DOL): **10,383 symbols**, real demangled C++
  `Class::Method` names (e.g. `CameraControl::HermiteExport`,
  `CameraControl::AreaCheckOnOff`) — confirms the engine is written in C++
  with a fairly conventional OOP structure (per-system classes:
  `CameraControl` and presumably similarly for player/enemy/weapon
  systems).
- Every retail REL (`Sscrn`, `st1_0..3`, `st2_0..4`, `st4_0`) has a
  matching `.sym` — direct 1:1 coverage for the modules we already
  extracted from retail.
- **Bonus beyond what emoose's README described**: dozens of additional
  `em*` (enemy behavior), `pl*` (player behavior), `wep*` (weapon), and
  `t_*` (camera/event/light/scene tool) modules also ship symbols on the
  debug disc — ~25,974 symbol lines total across everything. These appear
  to be additional overlay modules used during development (possibly
  debug-only, or stripped/merged differently in the retail RELs) —
  need to check the debug disc's own `files/Rel/` folder to see if these
  exist as loadable `.rel` files there too, separate from the 11 retail
  ones.

**Not yet solved**: the debug `main.dol` is 2.55 MB vs retail's 2.17 MB —
different code layout (extra debug/assert code, presumably), so these
symbol *addresses* don't transfer directly onto the retail DOL. The
*names* should still be highly reusable once functions are matched between
the two builds (same engine, same era) — this reconciliation step is the
next real technical challenge, not a solved problem yet.

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
