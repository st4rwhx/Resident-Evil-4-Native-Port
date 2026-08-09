# Matching log: CameraControl::CurrentAreaNo / CurrentCameraNo

Status: **MATCHED** (byte-identical) — the project's first verified,
complete, byte-perfect matches.

## Ground truth

```
CurrentAreaNo    80003964-8000396f  12 bytes
  88 63 06 90    lbz r3,0x690(r3)
  7c 63 07 74    extsb r3,r3
  4e 80 00 20    blr

CurrentCameraNo  80003970-8000397b  12 bytes
  88 63 06 92    lbz r3,0x692(r3)
  7c 63 07 74    extsb r3,r3
  4e 80 00 20    blr
```

## Matching source

```cpp
s32 CameraControl::CurrentAreaNo()
{
    return mCurrentAreaNo;   // s8 field @ 0x690
}

s32 CameraControl::CurrentCameraNo()
{
    return mCurrentCameraNo; // s8 field @ 0x692
}
```

## The one thing that mattered: return type

First attempt used `s8` as the return type (matching the field's actual
storage type). That compiled to just `lbz r3,offset(r3); blr` — **no**
`extsb` — because with `-opt all`, the compiler decided the sign-extension
was redundant for an `s8`-returning function and dropped it. Target
*does* have the `extsb`. Changing the return type to `s32` (the field
gets promoted/sign-extended into a full int on return, which is a real,
necessary conversion the optimizer can't drop) immediately produced the
exact match. Lesson: a getter's C++ return type doesn't have to equal the
underlying field's storage type, and picking the wrong one can silently
delete an instruction rather than just relabel it — worth double-checking
whenever a match is "off by exactly one sign/zero-extension instruction."

## Why this one worked immediately (unlike AreaCheckOnOff/Disable)

No branches, no register reuse across multiple live values, no compiler
choice between equivalent instruction encodings — a single value flows
r3 -> r3 the whole way through. Nothing for the scheduler/allocator to
have discretion over. Straight-line, single-value functions like this are
the reliable, fast-to-match category; multi-branch or multi-local-variable
functions (see `AreaCheckOnOff`, `Disable`) are where compiler-internal
non-determinism starts showing up. Worth prioritizing more functions of
this shape to build up the symbol/type database before returning to the
harder ones.
