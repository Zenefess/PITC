# PITC — Known Issues and Defects

**Audit of:** Pulsed Integrity Tests for CPUs v1.0.2, at the tree following the fix series of PRs #1–#19
**Date:** 2026-08-16
**Scope:** every file in the repository — `CPU.cpp`, `CPU.h`, `CPU_methods.h`, `CPU_job_cycles.h`, `CPU_build.h`,
the four `CPU_jobs_*.cpp` kernel units, the six vendored shared headers, `CPU.vcxproj`, and all documentation.
**Method:** static review only, performed as twelve subsystem passes (parsing, output/evaluation, topology,
golden values, timing, the four kernel units, cross-unit bit-exactness, dispatch/concurrency, vendored
headers, build configuration, documentation) followed by an adversarial verification pass: every new finding
was independently re-derived from the source and an attempt made to refute it before inclusion. Two candidate
findings that failed that verification are recorded in Appendix A so they are not rediscovered. The target
platform (Windows x64 / MSVC v143) was not available to the audit, so nothing here was reproduced by
execution; entries whose claim rests on Windows SDK declarations or MSVC codegen say so. Every entry cites
the line it was derived from — re-confirm against the current source before acting.

## Severity legend

| Level | Meaning |
|-------|---------|
| **Critical** | The tool reports a *wrong verdict about CPU health* — a false `.Pass.`, a guaranteed false `!Fail!`, or corruption of its own reference data. For an integrity tester this is the worst class of defect. |
| **High** | Crash, hang, memory corruption, a documented mode that does not work at all, or a build blocker. |
| **Medium** | Wrong behaviour in a reachable configuration, or a latent defect that becomes live as soon as a stated limit is lifted. |
| **Low** | Cosmetic, documentation, or repository-hygiene problems. |

---

## A. Verdict integrity — the tool's core claim

### A1. Cache-targeting options `I1`/`I2`/`I3` are parsed and displayed but implemented by nothing — **Medium** — *carried forward (old A13)*

`CPU.cpp:167–175` parses the three cache letters into `procUnits` bits 5–7 and the banner (`CPU.cpp:899`)
prints `CL1`/`CL2`/`CL3` for them, but no code reads those bits: dispatch masks with `0x1F`
(`CPU_methods.h:29`), the arena switch masks with `0x01F` (`CPU.cpp:791`), and the cache sizes the topology
walk files (`CPU.h:797–809`) have no consumer. The help text discloses "NOT YET IMPLEMENTED", but a saved
results file still carries a banner naming `CL3` as part of the tested configuration. Unchanged from the
previous audit.

## B. Vendored headers

These files are copies of the author's general-purpose library. Defects here are latent for PITC unless
noted — but they are live for any other client, and `CPU.h` includes two of them into every translation unit.

### B1. `refp1a1` expands to a cast to an array type, ill-formed on any use — **Low**

`typedefs.h:577`: the macro produces a C-style cast whose type-id is an array type, which C++ forbids —
every expansion is a compile error on every compiler, unlike its `defpa`/`refpa` companions. No PITC file
uses it.

### B2. `CLASS_TIMERS` never frees its `salloc`'d timer table — **Low**

`class_timers.h:121`: the 2 KB `TIMER_VARIABLES[MAX_TIMERS]` block is allocated by an in-class initialiser
and the struct has no destructor — contrary to GCS p2 and the repo's own destructor-owned-pointer pattern
(`GLOBAL_CFG`, `RESULTS_ARRAYS`). PITC never instantiates `CLASS_TIMERS`.

## C. Job kernels

### C1. The "SSE4.1" unit exercises no SSE4.1 arithmetic, and the "AVX2" unit no AVX2 arithmetic — **Medium** — *carried forward (old J3)*

`JobSSE`/`JobALU_SSE`/`JobMemSSE`/`JobMemALU_SSE` (`CPU_jobs_SSE.cpp:48–247`) use only
`_mm_set1_pd/div/sqrt/add/sub/mul/and/set1_epi64x` — all SSE2. The unit's sole SSE4.1 instruction is `PTEST`
in `ResultsMatch` (:261) and the completion poll — the comparison, not the computation — yet an SSE4.1-less
CPU is refused the whole unit with `-11`. Likewise the AVX2 unit's kernels use only `_mm256_*_pd` forms that
are AVX1; the AVX2-ness is the compiler's encoding, not the arithmetic under test. A user running `Is` or
`Iv` to exercise the instruction class named on the tin is testing less than the name says. Renaming or
extending the kernels both change the golden values, so this stays a documentation-versus-substance defect
until a `cpu.values` version bump is scheduled anyway.

## D. Documentation and localisation

### D1. Failure-report and results-table strings remain outside the translation tables — **Low** — *carried forward (old K9)*

Beyond the disclosed `wstrUnitsCPU`/`wstrSyncCPU`/`wstrPass`, the per-unit value formats inside `Failed()`
(`CPU.cpp:72–91`) and the results-table row labels and separators (`CPU.cpp:1029–1051`) are literals the `L`
mechanism cannot reach — the one output a failing CPU produces is only partly translatable.

### D2. `translations.h` cannot express a second language without code edits, and the console path would garble one — **Low** — *carried forward (old K10)*

Adding a language still means writing `<code>.h`, editing `translations.h` and extending the `L` case — and
the help text's "Recognises ISO 639-1 language codes" oversells a hard-coded en-GB/en-US comparison whose
example is a BCP-47 tag. Additionally, no `_setmode(_O_U16TEXT)`/`SetConsoleOutputCP` call exists anywhere,
so `wprintf` output converts through the ANSI code page: pure-ASCII English survives, but any future
translation with characters outside the console code page will print mojibake through the very mechanism
built to carry it.

### D3. Dead configuration declarations misdescribe the program — **Low** — *carried forward (old K14)*

`GLOBAL_CFG`'s in-class defaults `procUnits = 0x04` / `procSync = 0x02A` (`CPU.h:86–87`) are overwritten
unconditionally by `CPU.cpp:140–141` before any argument is read, so the struct lies about the defaults; and
four `THREAD_CFG` fields (`packetSizeRAM`, `maxTics`, `inactiveTime`, `records32`) are written in the spawn
loop and read by nothing.
