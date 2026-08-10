# Matching log: systemStartInit

Status: **in progress** — 80.7% match (objdiff) on the first serious
attempt. Not chasing further right now (see "Why stopping here" below);
this is real, verified progress, not a finished match.

## Why this function

`main()`'s entry chain is `__main → systemStartInit → systemRestartInit
(main loop)`. `systemStartInit` is the literal top-level engine bootstrap —
it brings up every subsystem (OS, exception handling, video, DVD, memory,
render, sound, FMV/Sofdec, message system) in a fixed order before the game
loop starts. Understanding it is a real prerequisite for anything past
"the executable starts" - not a random pick.

## Ground truth

`801533F4-80153554`, 0x164 bytes / 89 instructions. See
`build/RE4DBG/obj/asm/auto_01_800771E4_text_026.s` for the full disassembly
this was hand-traced from.

## Matching source

`src/misc/SystemInit.cpp` / `src/misc/SystemInit.h`.

## What matches (80.7%)

The entire call sequence, in the exact original order: `memclr_asm →
OSInit → setLanguage → RomFontSetting → OSGetConsoleType → [InitFile →
file_path → file_path fallback, conditionally] → ExceptionInit →
OSInitAlarm → VIInit → systemVISetBlack(1) → LCEnable → VIWaitForRetrace →
VISetPostRetraceCallback → cDvd::Init → CardInit → [GQR2-5 quantization
register setup] → SystemMemInit → [CardDbgCacheSet, conditionally] →
TaskSchedulerInit → systemScreenInit → Render_init → LightSetInit →
RndInit(0xd37) → InitOt → SndInit → SofdecInit → IDSystem::gameInit(0x80) →
IdTexGameInit → EprintfInit → LogInit → init_dbmodule →
cDvd::SizeTableRead → MessageControl::init`. Both conditionals (devkit
console-type flag gating the debug file-path probe and CardDbgCacheSet)
reproduce correctly. The GQR setup (`li/oris/mtspr` x4, values 0x00040004/
0x00050005/0x00060006/0x00070007 into GQR2-5) needed CodeWarrior inline PPC
asm (`asm { ... }` block) since it's raw SPR manipulation, not expressible
in portable C++ - matches exactly.

## What doesn't match yet, and why (via `objdiff-cli`, not raw byte diff)

Raw `.o`-vs-`.o` byte comparison is **not valid** for this function before
linking - every `bl` target and every `@ha`/`@l`/`@sda21` global reference
is an unresolved relocation until link time, so naive byte diffing shows
"different" for dozens of instructions that are actually correct once
linked. `objdiff-cli diff -1 <ground-truth-split-object> -2 <our-compiled-object>
<symbol>` is relocation-aware and is the only valid comparison method for
a function this size - use it, not manual hex diffing (cost real time
re-discovering this).

1. **Global naming (cosmetic, not a real mismatch)**: `lbl_80314BC8` (the
   project's default auto-name) vs `g_systemWork` (this file's descriptive
   name for the same address) show as `DIFF_ARG_MISMATCH` in objdiff purely
   because the symbol *names* differ as text - both correctly compute
   "load pointer at that sda21-relative address." Once linked against the
   real address both resolve identically. Not a real bug, just how the
   diff tool reports it without a name-alias mapping configured.

2. **`cDvd`/`IDSystem`/`MessageControl` instances placed via `@sda21` here,
   but the original uses full `lis+addi` (32-bit address) instead** - i.e.
   the original does NOT treat these globals as small-data-eligible despite
   being small (`cDvd` is only confirmed at 4 bytes touched so far), while
   our stub classes (declared as empty classes with only method
   declarations, no data members) apparently do qualify under mwcc's
   default `-sdatathreshold 8`. Most likely explanation: the real `cDvd`/
   `IDSystem`/`MessageControl` classes are larger than what's been
   recovered so far (only the specific fields/methods matched functions
   touch are known - same "stub struct, offsets only" convention as
   `CameraControl`'s `AreaData`/`MotionBase`) and their true size exceeds
   the small-data threshold. Growing these stub classes' declared size
   (without claiming to know real field layouts) once more of their real
   methods get matched should fix this - not attempted yet.

3. **`OSGetConsoleType() & 0xf0000000` compiles to `clrrwi. r0,r3,28`
   here vs the target's `andis. r0,r3,0xf000`** - logically identical
   (both mask to the top 4 bits and set cr0), different instruction
   selected by the optimizer for the same mask. Same category as
   `CameraControl::AreaCheckOnOff`'s `or.` vs `cmpwi` finding - worth one
   or two quick rephrasings (e.g. comparing against a shifted value
   instead of a masked one) but not chased yet.

4. **Prologue register-save scheduling**: target does
   `stwu; mflr; stw r31,0xc; stw r0,0x14; <sda21 load>; li r4,0; ori
   r4,r4,0x8680` (save both callee-saved regs first, *then* compute the
   memclr size argument); ours computes the size argument (via a different
   two-instruction constant sequence, `li r4,1; subi r4,r4,0x7980`, same
   value 0x8680 via a different encoding) interleaved with saving r31.
   Minor compiler-internal scheduling choice, same general class as
   `Disable()`'s register-numbering-only difference - not chased yet.

## Why stopping here for now

80.7% on the first real attempt at an 89-instruction, dozen-global-
reference function is a strong result to build on - `AreaCheckOnOff`
(14 instructions) took 20+ compiler-version/rephrasing attempts to reach
"Equivalent" and never got byte-exact. All four remaining gaps above are
understood and categorized, not mysteries; #1 isn't a real bug at all,
#2 needs the stub classes fleshed out as more of their methods get matched
(natural incremental progress, not extra work), and #3/#4 are the same
"compiler-internal, not clearly source-controllable" class already
documented for `CameraControl`. Recorded honestly as "in progress" rather
than spending more of this session chasing register-scheduling minutiae on
one function - the bigger-picture goal (native menu) needs hundreds more
functions like this one matched at all, not this one byte-perfect.
