# Matching log: CameraControl::AreaCheckOnOff

Status: **NOT YET MATCHED** — closest attempt is 52/56 bytes, same logic,
same instruction count in the tail, differs only in two spots. Documenting
what's been tried so nobody re-runs the same dead ends.

## Ground truth (debug build, 80003920-80003957, 56 bytes)

```
7c842379    or. r4,r4,r4
41820010    beq 0x80003934
2c040001    cmpwi r4,0x1
41820018    beq 0x80003944
4e800020    blr
80030-02c   lwz r0,0x2c(r3)     ; body @ 0x80003934 ("mode==0" case)
60000008    ori r0,r0,0x8
9003002c    stw r0,0x2c(r3)
4e800020    blr
8003002c    lwz r0,0x2c(r3)     ; body @ 0x80003944 ("mode==1" case)
54000776    rlwinm r0,r0,0x0,0x1d,0x1b   ; clears exactly bit 0x8 (mb=29>me=27 wraps: kept=all but bit28)
60000010    ori r0,r0,0x10
9003002c    stw r0,0x2c(r3)
4e800020    blr
```

Logic (confirmed, not in question): `this->mFlags` is a `u32` at offset
`0x2c`. `mode==0` sets bit `0x8`. `mode==1` clears bit `0x8` and sets bit
`0x10`. Anything else: no-op.

## Toolchain

- Compiler: `tools/compilers/GC/2.7/mwcceppc.exe` — "Metrowerks C/C++
  Compiler for Embedded PowerPC... Version 2.4.7 build 108" (from
  `files.decomp.dev/compilers_20251118.zip`, the same archive PrimeDecomp/
  zeldaret/doldecomp use). Chosen as closest era match to Twilight
  Princess GC's confirmed 2.4.7 — **not confirmed as RE4's actual
  version**, just the best starting guess.
- **Gotcha**: `-O0`/`-O4,p`-style GCC-ish flags are **not valid mwcc
  syntax** and get silently ignored — real flag is `-opt
  off|on|all|space|speed|level=N[,peephole][,schedule]...` (default is
  `off`). Wasted several iterations before catching this; if a flag
  doesn't change the output at all, suspect it's not being parsed rather
  than concluding "optimization doesn't matter here."

## What's been tried (all ruled out as exact matches)

| Source shape | Param type | `-opt` | Result |
|---|---|---|---|
| `if(mode==0){...return;} if(mode==1){...}` | `s32` | off/on/all (no difference) | 52 bytes: `cmpwi r4,0`/`bne`/body0/`blr` then `cmpwi r4,1`/**`bnelr`**/body1/`blr` — logic identical, but opens with `cmpwi` not `or.`, and folds the "mode!=1 -> return" into `bnelr` instead of a separate trailing `blr` |
| `if(mode==0){...} else if(mode==1){...}` | `s32` | any | identical to above |
| nested `if(mode==0){...} else { if(mode==1){...} }` | `s32` | any | identical to above |
| `if(!mode){...return;} if(mode==1){...return;}` | `s32` | any | identical to above |
| `switch(mode){case 0: ...break; case 1: ...break;}` | `s32` | off (=default/no flag) | 60 bytes — compiles as a full bounds-check chain (`cmpwi 1`/`beq`/`bge`/`cmpwi 0`/`bge`/`b`/bodies), wrong shape entirely |
| same switch | `s32` | on/all/level=1+ | 52 bytes but range-test shape (`cmpwi r4,1`/`beq`/**`bgelr`**), checks against 1 *first* — wrong order and wrong opcodes |
| `if(mode==0){...return;}` | `u32` (unsigned) | any | 52 bytes but uses `cmplwi` not `or.`/`cmpwi`, and both branches funnel to one shared trailing `blr` (no separate per-branch `blr`) — also changes the mangled name (`FUi` vs `Fi`), so probably wrong type anyway since we have no confirmation of the real param type |
| Compiler versions 1.3.2, 2.0, 2.5, 2.6, 2.7, 3.0a3, 3.0a3.2, 3.0a5 | `s32`, sequential-if | any | **all produce byte-identical output to each other** for this source — the `or.` vs `cmpwi` choice isn't a compiler-version thing, at least not across this range |

## What's confirmed vs. still a guess

- Confirmed: the mFlags/offset-0x2c logic, the bit manipulation, the
  overall 3-way branch structure (0 / 1 / else-noop).
- Unconfirmed: the `re4-research`/`re4sym` tool doesn't appear to preserve C++ mangled
  parameter-type signatures — it stores `Class::Method`-shaped names
  without parameter types (checked: no demangling-related strings in
  `re4sym.exe` at all, suggesting the SYM format itself only carries
  human-readable class/method names, no full prototype). So **the exact
  parameter type (`int`? `bool`? `s8`?) is not recoverable from the symbol
  data alone** — has to be inferred from the disassembly/calling
  convention, same as any other decomp target.
- Still unexplained: why the target opens with `or. r4,r4,r4` (self-OR
  zero-test idiom) instead of `cmpwi r4,0x0`, and why its "no match ->
  return" case is a standalone `blr` rather than fused into `bnelr`/
  `bgelr`. Neither source restructuring nor compiler version explains it
  so far. Worth trying next: different `-fp`/`-schedule`/`-rostr` flags,
  a `bool`-typed parameter, or checking whether this function is actually
  reached via a vtable/virtual dispatch (which sometimes changes prologue
  codegen in ways that could ripple into this pattern) — CameraControl may
  have virtual methods elsewhere that would confirm/rule this out.

## Files

- `build/areacheckonoff_ref.txt` — full Ghidra-dumped reference
  disassembly + raw bytes (gitignored, regenerate via
  `tools/scripts/DumpFunctionInfo.java`)
- `src/camera/CameraControl.cpp` / `.h` — current best-effort source
  (52/56 bytes, marked UNVERIFIED in a comment)
