# Matching log: AreaNum / setMotionBaseMatPtr / getMotionInfoPtr

Status: `getMotionInfoPtr` **MATCHED** (byte-identical). `AreaNum` and
`setMotionBaseMatPtr` **Equivalent** (same instructions/order/opcodes,
different register number for the loaded pointer).

## Ground truth

```
AreaNum               80003958-80003963  12 bytes
  81 23 00 5c   lwz r9,0x5c(r3)
  88 69 00 05   lbz r3,0x5(r9)
  4e 80 00 20   blr

setMotionBaseMatPtr    80008fd0-80008fdb  12 bytes
  81 23 06 8c   lwz r9,0x68c(r3)
  90 89 01 d0   stw r4,0x1d0(r9)
  4e 80 00 20   blr

getMotionInfoPtr       80008fdc-80008fe7  12 bytes
  80 63 06 8c   lwz r3,0x68c(r3)
  38 63 01 00   addi r3,r3,0x100
  4e 80 00 20   blr
```

## Source

```cpp
u8 CameraControl::AreaNum()
{
    return mAreaDataPtr->num;      // AreaData* @ 0x5c, .num @ +5
}

void CameraControl::setMotionBaseMatPtr(void* mat)
{
    mMotionBase->matPtr = mat;      // MotionBase* @ 0x68c, .matPtr @ +0x1d0
}

void* CameraControl::getMotionInfoPtr()
{
    return mMotionBase->unk_100;    // same MotionBase*, address of +0x100
}
```

`getMotionInfoPtr` compiles byte-identical. `AreaNum` and
`setMotionBaseMatPtr` compile to the exact same 3 instructions in the
exact same order — just keeping the loaded pointer in **r3** (reused)
where the target moves it to **r9** (a fresh register) before the second
memory access.

## A real, reproducible pattern (not just noise)

This is the third time we've seen exactly this shape of near-miss
(`Disable` was the first, `AreaNum`/`setMotionBaseMatPtr` here), and a
pattern is visible across all of them:

- **When a loaded value is immediately reused only as an arithmetic
  operand** (`getMotionInfoPtr`'s `addi r3,r3,0x100`) - the compiler
  reuses the same register. **Matches.**
- **When a loaded value is immediately reused as the *base address* of a
  second, separate memory access** (`AreaNum`'s `lbz ...,5(r9)`,
  `setMotionBaseMatPtr`'s `stw ...,0x1d0(r9)`) - the target moves it to a
  fresh register (r9) first. Every attempt here keeps it in r3 instead.

Tried explicitly introducing a local pointer variable (`AreaData* p =
mAreaDataPtr; return p->num;`) to hint the allocator toward a fresh
register - no effect, still r3. Not chasing this further per the
established `Equivalent`-status policy (see `CameraControl_AreaCheckOnOff.md`
for why), but recording the hypothesis clearly since it's the most
concrete, testable lead we have for the general "second-memory-access
gets a fresh register" register-allocation behavior - worth checking
against future matched/near-matched functions to see if it holds up.
