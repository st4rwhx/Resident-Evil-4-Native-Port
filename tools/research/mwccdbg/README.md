# mwccdbg — cadmic's mwcc-debugger for the re4 CRI units

Built 2026-09-11 (CRI pass 12). Dumps the MWCC 2.4.7 (GC/2.6 = build 107; codegen identical to our
GC/2.7 build 108) frontend AST, backend PCode after every pass, and the register allocator's
interference graph + priority list for one function, and replays the allocator and the scheduler in
Python models validated against those dumps.

## Layout
- `mwcc-debugger/` — github.com/cadmic/mwcc-debugger (main, shallow clone). `mwcc_debugger.py`. NOT in
  the repo (external checkout, see "Build"); override the location with `MWCCDBG_DEBUGGER=<path to mwcc_debugger.py>`.
- `retrowin32/` — github.com/encounter/retrowin32 branch `gdb-stub` (commit 11dbea5a). Binary:
  `retrowin32/target/lto/retrowin32`. NOT in the repo; override with `MWCCDBG_RW32=<path to the binary>`.
- `ra.py` — wrapper: `ra.py lib/unit Func [--src file.c] [--out DIR] [--ver GC/2.6]` (run from the repo root; pass absolute paths).
- `rasum.py DIR [--nb]` — one line per node of the priority list (`vid -> reg name removed/total [neighbours]`) plus the coalesced ghosts.
- `$MWCCDBG_OUT/<Func>/` (default `/tmp/mwccdbg.$USER/out/<Func>/`) — dumps of the last run per function.
- `RE4_ROOT` — the repo root; default is the git toplevel of this directory.

The compiler itself is `build/compilers/GC/2.6/mwcceppc.exe` of the configured tree (fetched by the
normal build). `mwcc_debugger.py` knows only the GC/1.1 and GC/2.6 offsets; `-ver GC/2.7` is NOT supported.

## Build (exact commands)
```sh
cd tools/research/mwccdbg
git clone --depth 1 https://github.com/cadmic/mwcc-debugger.git
git clone https://github.com/encounter/retrowin32.git && cd retrowin32 && git checkout gdb-stub
RUSTFLAGS="-C link-arg=-latomic" cargo build -p retrowin32 -F x86-unicorn --profile lto   # ~35 s, needs cmake, gdb (x86 host gdb works)
```
(Both directories are git-ignored here.)

## Run
```sh
cd <repo root>
python3 tools/research/mwccdbg/ra.py lib/adx_tsvr adxt_nlp_trap_entry            # unit's ninja flags, GC/2.6
python3 tools/research/mwccdbg/ra.py lib/sfd_hds sfhds_DoProcessHdr --src /abs/path/probe.c
```
Raw form (what ra.py runs; cwd must be the repo root, include paths relative — retrowin32 maps the cwd):
```sh
python3 tools/research/mwccdbg/mwcc-debugger/mwcc_debugger.py \
  -e tools/research/mwccdbg/retrowin32/target/lto/retrowin32 \
  -a "build/compilers/GC/2.6/mwcceppc.exe -nodefaults -proc gekko -fp hard -fp_contract on -Cpp_exceptions off -enum int -char signed -warn pragmas -pragma 'cats off' -O4,p -inline auto -sdata 0 -sdata2 0 -str readonly -use_lmw_stmw on -I- -i include -i include/libc -i src/lib -i src/lib/cri -lang=c -c src/lib/adx_tsvr.c -o /tmp/x.o" \
  adxt_nlp_trap_entry /path/to/outdir
```
~3 s per function. Static functions are found by plain name; the function must be emitted (not
inlined away).

## chaitin.py — the allocator model (CRI mpv pass 3)
`python3 tools/research/mwccdbg/chaitin.py OUTDIR [--pass N] [--check] [--verbose] [--nb] [--k N]` (`--k 28` when the function has one `asm { mr rN, x }` pin, `--k 27` for two: each pinned physical register leaves the colour set, CRI pass 47) replays the 2.4.7
GPR Chaitin allocator on a dump directory: it reads `regalloc-gpr-pass-N-all.txt` (graph, names,
fCoalesced ghosts / fCoalescedInto leaders) and `backend-NN-before-regalloc.txt` (PCode + LOOPWEIGHT
for the spill costs) and prints the predicted colouring order, colours, degree at removal, level
(`L<n>`, `S` = spill-candidate pick) and cost.  `--check` diffs it against the compiler's
`regalloc-gpr-pass-N-assigned.txt` (order, colour, degree, cost) — IDENTICAL on ExecServerSub, ChkBufSiz,
DecodePicAtr (9 spill picks), CreateSofdec, CreateSfd pass 1 (incl. the `width` spill) and pass 2.
The model itself is in the module docstring (scan order, degree < 29 removal, cost/degree spill pick with
ties to the highest vid, lowest free of r0,r3..r12 + handed-out callee-saved, else a new one from r31).
Use it BEFORE a source change: `import chaitin; g = chaitin.load(DIR); chaitin.allocate(g)` after editing
`g.adj` / `g.cost` / `g.order` tells you whether "one more neighbour for X" or "a loop-weighted use of Y"
actually moves the target register; if the prediction does not move, the source change will not either.

## Reading the dumps
- `regalloc-gpr-pass-1-assigned.txt`: priority order (top = coloured first), virtual id, assigned
  register, variable name, degree at removal / total degree. `-all.txt` adds neighbour lists and the
  coalesced aliases (ghost nodes).
- `backend-09-before-regalloc.txt`: the PCode with virtual registers — cross-reference the ids.
- `frontend-01-ast-after-optimizations.txt`: what the frontend kept (copies, range splits `@N`).
- See docs/research/cri.md "CRI pass 11: MWCC register-ranking model" and "CRI pass 12" for the model.

## blkflags.py — per-block "scheduled" bit across the backend passes (CRI pass 46)
`python3 tools/research/mwccdbg/blkflags.py DUMPDIR` prints one row per block with the `:{xxxx}` flag word after each
backend pass >= 11. Bit 0x8 = scheduled (set by the pre-RA scheduler); the post-RA scheduler re-runs only blocks
whose 0x8 is clear. Known clearers: peephole-forward `addi rX,rX,K` sink (pass 41), the RA deleting a dead def
(mps_lib MPS_Create: a backend-unrolled loop's dead `li i,0`, pass 46) or a coalesced copy, the post-RA peephole
record-form merge `rlwinm; cmpi` -> `rlwinm.` (pass 46). Read it before reasoning about a block's final order:
if the block is dirty in ours, the target's order is a post-RA schedule too unless its dirtying event is absent.

## sched.py — the PCode list scheduler (pre-RA = post-RA routine), RE'd from GC/2.6 (2026-09-12)
`scheddump.sh lib/unit Func [--src f.c] [--out DIR]` dumps every block's raw PCode (address, flags, `R<class>:<reg>:<rw>`
operands, alias record) right before/after the pre-RA (`sched-pre1/post1.txt`) and post-RA (`pre2/post2`) scheduler runs into
`$MWCCDBG_OUT/Func/`; `python3 sched.py DIR [--post] [--block N] [--verbose]` replays the scheduler on it and diffs against the
compiler's order (IDENTICAL on 497/497 pre-RA and 172/172 post-RA blocks of 28 CRI functions). `schedcheck.sh "lib/unit Func" ..`
= dump + both checks per function; `schedtrace.sh` = the compiler's own (cycle, pcode, height, deadline) picks (breakpoint
0x507e9b) for a cycle-by-cycle comparison with `--verbose`; `dis26.sh START END` = objdump of one GC/2.6 routine. The
algorithm (DAG rules, priority, gekko machine model) is in sched.py's docstring and docs/research/cri.md "MWCC pre-RA scheduler (RE)".
Use it to answer "why is X before Y": `--verbose` prints height/deadline per pick; the priority is urgent (deadline <= cycle)
> frees more successors > greater height > (pre-RA) lower opcode class byte > earlier in the input order.
The gdb scripts (`scheddump_gdb.py`, `schedtrace_gdb.py`) need an x86 host `gdb` with Python support.

## ghostwhatif.py — "what if X had a coalesced ghost" (CRI pass 52)
`python3 tools/research/mwccdbg/ghostwhatif.py RA_DIR [--k N] +ghost:REG[:phys=REG|:like=VID|:names=a,b|:all].. [--target name=reg,..]`
adds never-removed ghost nodes aliased to physical REG to a chaitin.py graph and prints the colouring (+ a score against
`--target`). `phys=P` = adjacent to exactly the nodes that already interfere with physical P — this is what a coalesced
parameter copy `mr rV, rP` looks like (validated: a7 + `phys=6,8,9` = the Ste4AsSte target 18/18, and the compiler's own
histl `?:` pair reproduces the (2,0,0,0) prediction). Use two `+ghost` for a `?:` pair (@temp + backend temp).

## schedwhatif.py — sched.py on ONE block with DAG edits (CRI pass 49)
`python3 tools/research/mwccdbg/schedwhatif.py DUMPDIR BLOCK [--post] [edits..]` replays the scheduler on one block of a scheddump
directory after editing its pcode list and prints the cycle-by-cycle issue (height/deadline per pick). Edits: `mv=i:j` (move raw
index i to position j), `del=i`, `dup=i`, `ins=i:P <pcode line>` (insert a raw `P ...` line, e.g. a copy `P 00000001 MR
flags=88004000 R4:99:2 R4:3:1`), `reg=i:arg:cls:reg:rw` (retarget one operand). Use it to ask "which DAG change puts X before
Y" before hunting the C: pass 49 read sfd_cre AnalyMpv's `addi ofs+1` slot (a c1 int pick that forwarding-blocks the `add`) and
mpv_umc OneReadMb's `rlwinm yhx` slot (c33 tie against `rlwinm chx` on frees 1 / height 6 vs 3) with it.
