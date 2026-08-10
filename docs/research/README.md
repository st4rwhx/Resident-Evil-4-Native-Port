# Matching research log

These files are the pass-by-pass notes written by the AI agents that did the matching work, split
from one chronological notebook into thematic files (each file is chronological on its own). They
are a log, not a manual: every `### ` section is one matching pass on one unit family, and records
what was tried, what the compiler dumps showed, and what was applied. This is the evidence behind
every `// COMPILER-DIFF: <item>` tag in the sources and behind every row of the lever catalogue in
`docs/matching.md`; the section titles are what source comments cite (`docs/research/, <title>`,
`pass N`, `closer`, `sweep`). Scratch-directory names (`~/.cache/<pass>`, `/tmp/<pass>`) refer to
per-pass harnesses that were deleted at the end of each pass; the persistent kit they were built from
is in `tools/research/`.

| file | contents |
|---|---|
| `dol.md` | GCC 2.95 game units: the DOL (`game/`, system units, `esp*`/`Espgen*`, `obj*`, `cam_*`, `db_cam`, ...) and the enemy (`em*`), player (`pl*`) and weapon (`wep*`) REL modules — sweeps, structural passes and the per-unit "closer" passes |
| `rel-rooms.md` | GCC 2.95 stage-room RELs (`st1_*`, `st2_*`, `st4_0`), the room idiom passes, the REL near-miss sweep and the Sscrn notes that were logged as passes |
| `rel-tools.md` | GCC 2.95 debug-tool RELs (`Tools`, `t_esp`, `t_camera`, `t_light`, `t_event`, `t_id`, `t_movie`, `t_sce`, ...): db_light / db_widget / db_mod / t_esp InitTool passes, the cDbgToolMain header layout |
| `cri.md` | MWCC 2.4.7 CRI middleware (`lib/adx_*`, `sfd_*`, `mpv_*`, `mps_*`, `sfx_*`, `dct_*`, `gcci`, `cri_cvfs`, `mwsfd*`): CRI passes 1–81, the SWAR / paired-single kernel passes, the MWCC register-ranking, add-propagation, pool and scheduler models |
| `compiler.md` | the toolchain itself: COMPILER-DIFF #N research/closure passes, the tag audits and structural tag hunt, the adopted `shipped-build-temp-flags` compiler patch, the switch-tree model, the inline-vs-macro and dead-test lever sweeps, the GCC residue sweeps and the 2-word-tie passes |
| `asm-removal.md` | 2026-09-17: replacing every asm-emitted instruction / hard-register `asm { }` pin with C — recipes by compiler pass (GCC) and by Chaitin-graph edit (MWCC), the codeless-pin correction, the remaining `dct_ac` block |
| `asm-removal.md` | 2026-09-17: replacing every asm-emitted instruction / hard-register `asm { }` pin with C — recipes by compiler pass (GCC) and by Chaitin-graph edit (MWCC), the codeless-pin correction, the remaining `dct_ac` block |
| `misc.md` | hazards found on the way (a stale `configure.py` copy, `/tmp` as tmpfs) and the identity audit of every unmatched unit |
