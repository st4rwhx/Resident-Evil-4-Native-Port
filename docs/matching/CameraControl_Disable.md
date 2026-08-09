# Matching log: CameraControl::Disable

Status: **`Equivalent`** — instruction sequence, opcodes, and order are a
100% exact match. Only the specific register *numbers* the allocator chose
differ. This is closer than `AreaCheckOnOff` (which differed in opcode
choice/instruction order too) — a good data point that "close" comes in
degrees, and this one is nearly there.

## Ground truth (debug build, 80003908-8000391f, 24 bytes, 6 instructions, zero branches)

```
8003002c    lwz r0,0x2c(r3)    ; load mFlags
39200000    li r9,0x0
99230034    stb r9,0x34(r3)    ; mUnk34 = 0
60000008    ori r0,r0,0x8
9003002c    stw r0,0x2c(r3)    ; mFlags |= 0x8
4e800020    blr
```

## Current best-effort source

```cpp
void CameraControl::Disable()
{
    u32 flags = mFlags;
    mUnk34 = 0;
    mFlags = flags | 0x8;
}
```

Compiles (CodeWarrior GC 2.7, `-opt all`, and confirmed identical across
all 20 available versions 1.0-3.0a5.2) to:

```
lwz r4,0x2c(r3)
li  r0,0x0
stb r0,0x34(r3)
ori r0,r4,0x8
stw r0,0x2c(r3)
blr
```

Same 6 instructions, same order, same opcodes. Only difference: the
allocator put the loaded `mFlags` value in **r4** (target: **r0**) and the
temp `0` in **r0** (target: **r9**).

## What was tried to fix the register choice

- `mFlags |= 0x8; mUnk34 = 0;` (no temp var) — wrong *order* too (stb ends
  up last), same r4/r0 choice
- `mUnk34 = 0; mFlags |= 0x8;` (reversed, no temp var) — order also wrong,
  but interestingly reuses r0 for both values sequentially (since they're
  not simultaneously live) - still not a match
- `u32 flags = mFlags; mUnk34 = 0; mFlags = flags | 0x8;` — **this is the
  one with correct order**, tried at `-opt off/on/all/level=2-4` — always
  r4/r0
- `u32 flags = mFlags | 0x8; mUnk34 = 0; mFlags = flags;` (fold the OR into
  the temp init) — no change
- All 20 CodeWarrior versions (1.0 through 3.0a5.2) on the correct-order
  source — 100% identical r4/r0 output across every single one

## Conclusion

Same root cause category as `AreaCheckOnOff` (see that log for the full
research writeup: mwcc's internal register allocator/scheduler makes
choices that aren't reliably reachable by restructuring the C source, and
this is a documented, accepted limitation in real GC/Wii decomp projects,
not something to keep blindly grinding on). Marking `Equivalent` and
moving on. If we ever pin down Capcom's actual build flags/version
(neither of which explains the mismatch among what's tried so far), this
one and `AreaCheckOnOff` might both resolve at once.
