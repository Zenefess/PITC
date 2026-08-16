# PITC — Known Issues and Defects

**Audit of:** Pulsed Integrity Tests for CPUs v1.0.2, at the tree following the fix series of PRs #1–#19
**Date:** 2026-08-15
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

**Relationship to the previous audit:** the prior `ISSUES.MD` (deleted in commit `c8a1497`) recorded 129
defects, of which roughly 95 were fixed across PRs #1–#19. This is a fresh audit of the tree those fixes
produced. Entries carried forward from the previous audit are marked with their old IDs; four previously-open
entries were verified **fixed** in the current tree and do not appear below: old **J4** (the `_mm_abs_pd`
`#undef` guards now precede both unit-local definitions), old **K3** (the `O` option's UTF-16 write now
multiplies by `sizeof(wchar)` and the 8-bit path converts through `WideCharToMultiByte` into a separate
buffer), old **K4** (the `Ux`/`Tx` option descriptions now match the parser), and old **K5** (`wmain` zeroes
`offTime` for every sweep, so presets `-6`/`-7` run the cycles they name). The concurrency machinery
(`threadBits`, the four polls, the join/abort paths, the jitter generator) and the arena-layout switch were
audited and survived adversarial review with **no new findings** — section D of the old audit stays closed.

## Severity legend

| Level | Meaning |
|-------|---------|
| **Critical** | The tool reports a *wrong verdict about CPU health* — a false `.Pass.`, a guaranteed false `!Fail!`, or corruption of its own reference data. For an integrity tester this is the worst class of defect. |
| **High** | Crash, hang, memory corruption, a documented mode that does not work at all, or a build blocker. |
| **Medium** | Wrong behaviour in a reachable configuration, or a latent defect that becomes live as soon as a stated limit is lifted. |
| **Low** | Cosmetic, documentation, or repository-hygiene problems. |

## Summary

| Area | Critical | High | Medium | Low | Total |
|------|---------:|-----:|-------:|----:|------:|
| A — Verdict integrity | 0 | 0 | 2 | 1 | 3 |
| B — Golden values and `cpu.values` | 0 | 0 | 1 | 1 | 2 |
| C — Memory and resource handling | 0 | 0 | 1 | 2 | 3 |
| D — Concurrency | 0 | 0 | 0 | 0 | 0 |
| E — Scheduling and timing | 0 | 0 | 1 | 0 | 1 |
| F — Command-line parsing and output | 0 | 0 | 2 | 2 | 4 |
| G — Topology and scaling | 0 | 1 | 0 | 0 | 1 |
| H — Build configuration | 0 | 0 | 1 | 5 | 6 |
| I — Vendored headers | 0 | 0 | 8 | 10 | 18 |
| J — Job kernels | 0 | 0 | 1 | 1 | 2 |
| K — Documentation and localisation | 0 | 0 | 1 | 11 | 12 |
| L — Project process | 0 | 0 | 0 | 5 | 5 |
| **Total** | **0** | **1** | **18** | **38** | **57** |

**A1, A2, B1, C1, E1, F1, G1, H1 and I1–I6 are fixed** — see the `Fixed` note on each entry. The counts in the
table above are as first audited and have not been decremented.

No Critical finding remains: the false-pass and false-fail families of the previous audit are gone, and the
one High entry was a start-up crash on a narrow class of old hardware rather than a wrong verdict. The Medium
weight sits in the vendored headers (latent for PITC, live for any other client of those files) and in a
handful of reachable configuration defects; the Low tail is documentation and hygiene.

---

## A. Verdict integrity — the tool's core claim

### A1. `Evaluate()` reads and increments its lane index in one unsequenced expression — **Medium** — *Fixed*

`CPU.h:1160`: `if(value[2][thread].raw[index] != value[3][thread].raw[index++])`. The right operand modifies
`index` while the left operand's subscript reads it, and the operands of `!=` are unsequenced (C++17 gave
this expression no ordering), so the behaviour is formally undefined. `Evaluate` is the single verdict
function: it grades every row of the results table (`CPU.cpp:1038–1051`) and gates `W`'s 65,536-iteration
self-check (`CPU.h:1204`). A legal compilation that evaluates the right operand first compares expected lane
*i+1* against observed lane *i* on every iteration — misaligned verdicts in both the test and the generation
paths, plus a one-past-the-window read. Today's MSVC evaluates left-first and the verdicts are right, but
nothing in the source guarantees it, and clang's `-Wunsequenced` flags exactly this pattern.
**Fix:** move the increment out of the expression (`... != value[3][thread].raw[index]) return 1; ++index;`).

> **Fixed** as the entry prescribes. The `while` is now a `for(; index < end; ++index)` and both operands
> subscript with a plain `index`, so the increment is a statement of the loop rather than a side effect of one
> operand: no evaluation order can misalign the lanes or read past the window, and clang's `-Wunsequenced` has
> nothing left to flag. The comment above it names the old expression and what a right-first compilation did
> with it, so the increment is not folded back into the comparison as a tidy-up. The dead
> `//return index >= end ? 0 : 1;` below the loop went with it. Verdicts are unchanged on MSVC, which already
> evaluated left-first — this removes the dependency on that, not a wrong answer.

### A2. The FPU job cycles compare doubles with a floating-point `!=`, not the bit compare the doctrine claims — **Low** — *Fixed*

`JobCycleFPU` (`CPU_jobs_standard.cpp:283`), `JobCycleMemFPU` (:301–316), `JobCycleALU_FPU` (:333) and
`JobCycleMemALU_FPU` (:351–366) compare `fl64` results with `!=`, and `CPU_job_cycles.h:46–48` (echoed by
`CLAUDE.md`) asserts all five units "answer the same question: are these two bit patterns identical". IEEE
`!=` is a numeric predicate: `-0.0 != +0.0` is false, and two bit-identical NaNs compare unequal. Today the
kernels can produce neither a zero nor a NaN as a *golden* value, so the predicate cannot misfire — but any
future edit to the FP chain that lets one in silently reintroduces the A11 defect class in the FPU unit
alone. **Fix:** compare through a `ui64` alias (`(ui64&)value[1][k].fpu != (ui64&)value[2][k].fpu`), which is
what the doctrine says is happening.

> **Fixed**, though not by the spelling the entry proposes. `CPU_jobs_standard.cpp` now carries a fourth
> `ResultsMatch` overload — `cbool ResultsMatch(cfl64 &result, cfl64 &expected)` — beside the three vector
> ones, and all ten `fl64` comparisons in `JobCycleFPU`, `JobCycleMemFPU`, `JobCycleALU_FPU` and
> `JobCycleMemALU_FPU` go through it. The `si64` comparisons in those same cycles are left as `!=`, which
> already examines every bit of both operands.
>
> The overload is `!memcmp(&result, &expected, sizeof(fl64))`, **not** the `(ui64&)` alias: reading a `fl64`
> object through a `ui64` lvalue is exactly the aliasing a compiler is entitled to assume cannot happen, and
> the fix would then be removed by the optimiser rather than performed. That is not theoretical — the
> proposed spelling was transcribed into a harness alongside the `memcmp` one and built at `-O2`, where it
> answers "identical" for `+0.0` against `-0.0` and for `1.0` against `1.0 + 1 ULP`: the A11 defect class,
> reintroduced by the line meant to remove it. The `memcmp` form answers all four probe cases correctly
> (`+0.0` and `-0.0` distinct, two bit-identical NaNs equal, `1.0` equal to itself, `1.0` distinct from
> `1.0 + 1 ULP`), is what `ValidateFamilyScalar` in the same file already uses on the same operands, and is
> expanded in place rather than called: both configurations set `IntrinsicFunctions` and the size is a
> constant. `CPU_job_cycles.h`'s bit-exactness note and `CLAUDE.md` were corrected with it — both recorded
> that the FPU cycles compare "raw scalars with `!=`" as though it were the doctrine rather than the one
> exception to it.

### A3. Cache-targeting options `I1`/`I2`/`I3` are parsed and displayed but implemented by nothing — **Medium** — *carried forward (old A13)*

`CPU.cpp:167–175` parses the three cache letters into `procUnits` bits 5–7 and the banner (`CPU.cpp:899`)
prints `CL1`/`CL2`/`CL3` for them, but no code reads those bits: dispatch masks with `0x1F`
(`CPU_methods.h:29`), the arena switch masks with `0x01F` (`CPU.cpp:791`), and the cache sizes the topology
walk files (`CPU.h:797–809`) have no consumer. The help text discloses "NOT YET IMPLEMENTED", but a saved
results file still carries a banner naming `CL3` as part of the tested configuration. Unchanged from the
previous audit.

## B. Golden values and `cpu.values`

### B1. The cross-unit ladder equivalence is verified by no code, though `CLAUDE.md` says `W` proves it — **Medium** — *supersedes old J6* — *Fixed*

Everything `cpu.values` promises about portability rests on `JobSSE`, `JobAVX2` and `JobAVX512` computing
`JobFPU` element-wise, bit for bit (`RunGoldenLadder`, `CPU.h:351–370`). `ValidateKernelFamilies`
(`CPU.h:278–296`) does not check that property: each `ValidateFamily*` derives its reference from *its own
unit's* register kernel (`CPU_jobs_AVX512.cpp:286`, `CPU_jobs_SSE.cpp` and `CPU_jobs_AVX.cpp` likewise), so
the eighteen kernels are held to their own width and never across widths — while `CLAUDE.md` states "`W`'s
`ValidateKernelFamilies` is what proves the eighteen kernels still agree afterwards". Consequence: edit the
FP arithmetic in one vector unit consistently across that unit's family and `W` passes and writes the file;
every machine of a *different* vector width then computes a different `KernelFingerprint` and rejects the
file with `-21` "generated by a different build" — the cross-width portability silently ceases to exist, no
check names the drifted kernel, and the diagnostic blames the file. **Fix:** after `ValidateFamilyScalar`,
run the probe seed's lanes through `JobFPU` per-lane and `memcmp` against `JobSSE` (and, where the CPU
carries them, `JobAVX2`/`JobAVX512`) over the same lanes — the ladder equivalence tested directly, on the
seed already in hand.

> **Fixed** as the entry prescribes. `ValidateKernelFamilies` (`CPU.h`) now ends with a second half: the
> first `LADDER_PROBE_LANES` (8) lanes of the seed already in hand are run through `JobFPU` one lane at a
> time, and that scalar reference is handed to `ValidateLadderSSE`, `ValidateLadderAVX2` and
> `ValidateLadderAVX512`. Each loads the same eight lanes into vectors of its own width — four `fl64x2`, two
> `fl64x4`, one `fl64x8` — runs its own register-resident kernel over them and `memcmp`s all eight lanes back
> against the scalar result. SSE is checked on every CPU and the other two exactly where `RunGoldenLadder`
> would reach for them, so a machine of any width proves every ladder it could itself walk. The three live in
> their own translation units for the same reason every `ValidateFamily*` does (H4); the reference is derived
> once by the caller rather than three times, because it is scalar, identical for all three, and by
> definition the *other* unit's arithmetic — the one thing a per-unit check cannot supply itself.
>
> A disagreement is reported as `-22`, alongside the family checks, but not with their message: `JobSSE`,
> `JobAVX2` and `JobAVX512` were added to `wstrKernelName` as entries 14–16 and `KERNEL_NAME_LADDER` marks
> the boundary, so `wmain` prints `wstrMessage[30]` below it and the new `wstrMessage[41]` at or above it.
> The three functions spell their return from that constant rather than as literals, so a family entry
> inserted later moves the ladder's indices with it.
> The two failures are different diagnoses — one kernel has drifted from its own unit's reference, or one
> unit has drifted from the scalar kernel every unit stands in for — and the second is the one that used to
> reach the next machine disguised as a stale file. `en-GB.h`'s `-22` line was broadened to cover both, its
> `W` description now states the two checks, and `wstrMessage_English` grew to 42 entries.
>
> Verified off-target, the arithmetic being platform-independent: the four register-resident kernels and the
> three new functions were transcribed into a harness and built with `-ffp-contract=off` (the `/fp:strict`
> guarantee `CPU_build.h` enforces). Over the `KernelFingerprint` seed and 20,000 rounds of `W`-style random
> seeds — 160,000 lane comparisons — `JobSSE`, `JobAVX2` and `JobAVX512` agreed with `JobFPU` bit for bit
> with no mismatches, so the check does not reject the tree it was written against. Injecting a
> single-kernel drift then produced 14, 15 and 16 respectively, so it does not pass one either.

### B2. `cpu.values` is written in place, shared for write, with no atomic replace — **Low**

The `W` path opens `cpu.values` with `CreateFileW(..., GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS,
...)` (`CPU.cpp:502`) and writes header and blocks directly. `FILE_SHARE_WRITE` admits a second concurrent
writer (two `W` runs interleave), and a crash or Ctrl-C mid-write leaves a truncated file in place of any
previous good one. The damage is bounded — the header hashes make the next read reject the remnant with
`-21` rather than grade against it — but the previous good file is still lost. **Fix:** write to a temporary
name and `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING)`, and drop the share-write bit.

## C. Memory and resource handling

### C1. The `O` results file is truncated at open, then left empty and its handle leaked on six error exits — **Medium** — *Fixed*

`O[name]` opens the results file with `CREATE_ALWAYS` at `CPU.cpp:740` — destroying any previous file of
that name — before the arena is sized, the memory pre-flight runs, or a thread is spawned. The error exits
after it return without closing the handle or noting the file: `-18` at :821, `-17` at :828, :835 and :883,
`-19` at :990 and :1006. Each leaves a zero-length (or BOM-only) file where the user's previous results
stood, with no message saying the file was touched; a Ctrl-C during the run (no console handler exists) has
the same effect. Example: `PITC.exe -1 Mc999999 O[results.txt]` fails the memory pre-flight with `-17` and
silently replaces an existing `results.txt` with an empty file. **Fix:** open the file after the pre-flight
checks, immediately before results are written — or close and delete it on every error return between :740
and the write at :1087.

> **Fixed** by the first of the two, with the early check kept in a form that destroys nothing. The
> `CREATE_ALWAYS` open and the byte-order-mark write moved down into the results block, immediately above the
> report write, so no handle is held across the run and no error return between the pre-flight and the write
> has a file to leave behind: the six exits the entry lists now touch nothing, and neither does a Ctrl-C,
> which is the case no amount of close-and-delete on the error paths would have covered.
>
> What stands where the open used to is a probe rather than a creation. `O` takes its path from the command
> line, so a typo in it should be reported before a run of hours rather than after one; the probe opens the
> path `OPEN_ALWAYS`, which neither truncates an existing file nor destroys one, closes it at once, and
> `DeleteFileW`s it if `GetLastError` reported that the probe was what created it. `-9` is therefore still
> raised before the test on an unwritable path, and a run that never reaches its results leaves the
> filesystem as it found it. The `ERROR_ALREADY_EXISTS` code is read on the statement after the call, before
> anything else can overwrite it — `OPEN_ALWAYS` reports it through `GetLastError` on *success*.
>
> The residual difference: a path that becomes unwritable *during* the run is now reported at the end rather
> than at the start. The report has already gone to the console by then, and the exit code is the same `-9`.
> `CLAUDE.md`'s parsing section records the arrangement and why the creation should not be moved back up.

### C2. `resArray.iter` is the one run-length allocation never checked for failure — **Low**

`CPU.cpp:154`: `resArray.iter = zalloc1d64(si64, cfg.sys.vCoreCount);` — the arena (`CPU.cpp:833`) and the
report buffer (`CPU.cpp:881`) both return `-17` on null, but a failed `iter` allocation proceeds to spawn
threads, and every benchmark thread then writes through the null pointer (`CPU_methods.h:160`). The
allocation is a few KiB, so the trigger is remote; the asymmetry against the codebase's own rule is the
defect.

### C3. `wstrOutput` is allocated with `zalloc64` and never freed — **Low**

The report buffer (`CPU.cpp:879`, up to ~1 MiB at 512 threads) has no matching `mfree` on any path — not at
the normal `return 0` nor on the later error returns. GCS rule p2 mandates a matching free for every aligned
allocation, and the old C13 entry moved the arena and `iter` frees into `~RESULTS_ARRAYS` for exactly this
reason. Reclaimed by process exit, so hygiene rather than a runtime defect.

## D. Concurrency

No findings. The completion bitmap and its four polls, the interlocked byte-wide accesses from both sides,
the suspended-create → `SetThreadGroupAffinity` → resume ordering, the join and abort paths, `generateError`
ordering, and the per-thread jitter generator were each re-reviewed and survived adversarial verification.

## E. Scheduling and timing

### E1. The round-robin start offset is neither folded down nor clamped, so late threads idle past `endTics` and are graded on a single forced job cycle — **Medium** — *Fixed*

`CPU_methods.h:53–54` offsets thread *n*'s `startTics` by *n* whole cycles with no fold-down — unlike the
staggered arm directly below it, whose ramp is folded until the first window fits the run (the E3 fix) — and
the initial `PulseWaitUntil(pulseTimer, startTics)` at :114 is not clamped to `endTics`. Whenever
`threadCount × cycleTics` exceeds the test duration, late threads wait out their entire offset *after* the
run has ended, execute exactly the one forced job cycle, and `wmain` waits for all of it. Example:
`Ia Sr Tft60[15000]15000 Ua` on 16 virtual cores — cycle 30 s, so thread 15's first window opens at 450 s
while the run ends at ~62 s: the "60-second" run takes ~7.5 minutes and rows for threads 3–15 grade silicon
exercised by one job cycle. This contradicts the documented invariants "no idle phase may end after the run
does" and "the only overshoot left is one job cycle". **Fix:** fold the offset the way the staggered arm
folds its ramp (threads share slots when the run is too short), or at minimum clamp the initial wait to
`min(startTics, endTics)` so a late thread runs its forced cycle at the deadline instead of long after it.

> **Fixed** by both halves, the fold being the one that does the work and the clamp the one that makes the
> invariant unconditional.
>
> The round-robin arm now derives a *slot count* rather than using the thread count directly:
> `slots` opens at `threadCount` and is decremented while
> `cycleTics * (slots - 1) + activeTics > endTics - startTics`, which is the staggered arm's own predicate
> with its power-of-two ramp replaced by a count that need not be one. Thread *n* takes slot `n % slots` and
> the cycle is stretched by `slots`, so the last slot's first window is guaranteed to close on or before the
> deadline. Where the run already holds the whole rotation the loop does not execute, `slots == threadCount`,
> and the arithmetic is the same expression it replaced — a `Sr` run long enough to complete its rotation is
> byte-for-byte the run it was before. Decrementing rather than halving keeps as much of the
> one-thread-at-a-time property as the run can pay for; where it cannot pay for any, `slots` reaches 1 and
> every thread pulses together, which is the same compromise the staggered fold makes.
>
> The entry's example is the check: `Ia Sr Tft60[15000]15000 Ua` on 16 virtual cores folds from 16 slots to
> 2, the last first window closing at 45 s of the 60-second run instead of at 465 s, and no thread starts
> after the deadline where 14 of 16 did. `Sr Tf[250]1750` across 128 threads in a 60-second run folds to 30
> slots (58.2 s) from 254.2 s and 98 late threads; at a one-hour duration the same command folds not at all.
>
> The clamp is `PulseWaitUntil(pulseTimer, min(startTics, tcfg->endTics))`. The two folds leave nothing for
> it to do on the round-robin and staggered shapes, but a parallel thread's start carries a jitter of up to a
> quarter of its shorter phase and no fold, so a run shorter than that window — `Tft0.05[250]250` — could
> still open a wait that outlives the test. Clamping the one wait that is not already `min`-bounded makes
> "no wait may end after the run does" true of every wait in the executor rather than of the pulse boundaries
> alone, which is what `CLAUDE.md` claims for it.

## F. Command-line parsing and output

### F1. The `M` option's per-argument reset clears `allocMem[0]` but never `allocMem[1]`, so `Mn8 Ms8` is refused while `Ms8 Mn8` runs — **Medium** — *Fixed*

Every `M` argument begins with `cfg.memConfig = 0; cfg.allocMem[0] = 0;` (`CPU.cpp:225–226`) and nothing —
not this reset, not `B` (:157–158), not the preset preamble (:545–546) — ever clears `allocMem[1]`. Giving
the two per-class sizes as two arguments therefore composes in one order only: `Mn8 Ms8` ends with
`{0, 8MB}` and the per-class record check (:819) refuses the run with `-18` and the misleading message
"Only 0 bytes of memory per thread", while `Ms8 Mn8` and the single-argument `Mn8s8` both run. The help text
is violated twice over: "where two of them set the same property, the last one wins" (`en-GB.h:43` — these
set *different* properties, yet one erases the other), and "'N' and 'S' each cover one class only: where the
CPU has cores of both, give both" (`en-GB.h:56` — the natural spelling of "give both" is the one refused).
**Fix:** reset both entries, or reset neither — `en-GB.h:44` documents only `B` and the presets as resetting
the memory configuration, so dropping the reset is the spelling that matches the documentation.

> **Fixed** by dropping the reset, which is the half of the choice that makes the documented spelling work:
> clearing `allocMem[1]` as well would have made the two orders agree only by refusing both of them, and
> "where the CPU has cores of both, give both" would still have named a command line that cannot run.
>
> The two lines at the head of the `M` case are gone. Nothing is lost by their going: each of the four
> sub-options assigns `memConfig` together with the size it names — `c`→1, `n`→2, `s`→2, `t`→0 — and the
> `default` arm rejects anything else with `-25`, so a valid `M` argument still settles the configuration
> completely and "the last one to set a property wins" still holds. What no longer happens is one `M`
> argument erasing a *different* property another had set.
>
> The reset the documentation does promise is now complete where it is promised. `B` and the preset preamble
> each clear `allocMem[1]` beside the `allocMem[0]` they were already setting, and the defaults block states
> both entries rather than one, so a preset or a benchmark still discards every memory option given before it
> — including an `Ms` — exactly as `en-GB.h:44` says.
>
> Checked against the arena sizing and the `-18` gate for both orders and for the spellings around them:
> `Mn8 Ms8` now runs `{8MB, 8MB}` where it was refused, and agrees with `Ms8 Mn8` and `Mn8s8`, which already
> ran. `Mc8`, `Mt1024`, `B Mt1024`, `Ms4 -1` and a bare preset are unchanged. `Mn8` alone on a two-class
> machine is still `-18`, which is the outcome `en-GB.h:56` documents for a class left without memory.

### F2. `ParseDecimal` is locale-dependent: every documented decimal spelling is rejected on comma-decimal locales — **Medium** — *needs confirmation on a real Windows system*

`wmain` installs the user's regional locale (`setlocale(LC_ALL, "")`, `CPU.cpp:115`) before parsing, and
`ParseDecimal` reads its value with a bare `wcstod` (`CPU.h:1050`), whose decimal-point character follows
`LC_NUMERIC`. On any system using a decimal comma (most of Europe and Latin America), `wcstod` stops at the
`.` in every documented spelling — `Tct5.0`, `Tfd1.0t12.5[100]2400`, `d10.0` — the parse succeeds with the
truncated integer part, the option loop's `++j` lands on the `.`, and the `T` option's `default` arm rejects
the whole command line with `-25` ("Unrecognised '.' option"). The failure is loud, but the documented option
grammar does not work at all on those machines, and `Tt12,5` parses there while failing on `en-US` — the same
command line means different things per machine. `ParseWholeNumber` (`wcstoll`) is unaffected.
**Fix:** parse with an invariant locale — `_create_locale(LC_NUMERIC, "C")` + `_wcstod_l`.

### F3. Numeric output is locale-formatted, so results files are written with locale decimal separators — **Low**

The complement of F2: the same `setlocale` call governs every `%f` conversion the program performs — the
banner's duration (`wstrInterface[5]`), and the observed/expected doubles `Failed()` prints
(`CPU.cpp:72–88`). On a comma-decimal system a saved results file and every failure report print `1,234567`
where the documentation and any parsing consumer expect `1.234567`. **Fix:** format through the same
invariant locale as F2, or confine `setlocale` to the console code page it was installed for.

### F4. Byte and wide output are mixed on one stream — **Low** — *benign under the MSVC CRT*

`GenerateValues` prints its progress dots with `printf(".")` (`CPU.h:1218`), `ComputationPulse` likewise
(`CPU_methods.h:125`), and `CPU.cpp` mixes `printf("\n")` (:886, :933, :1019) between `wprintf` calls on the
same `stdout`. C and C++ forbid byte I/O on a wide-oriented stream (and vice versa); the code works because
the MSVC CRT deliberately does not implement stream orientation. Formally nonportable, and the first thing to
break under any other runtime.

## G. Topology and scaling

### G1. `POPCNT` executes unconditionally in the SSE2-baseline path, crashing pre-Nehalem x64 CPUs at startup — **High** — *Fixed*

`WalkTopology` counts a core's virtual cores with `PopulationCount64` (`CPU.h:735`), and the thread-count
loop does the same (`CPU.cpp:725–726`). On x64 MSVC `PopulationCount64` is winnt.h's macro for `__popcnt64`,
which always emits the `POPCNT` instruction — there is no `/arch` gate on it. `EnumerateTopology` is the
first thing `wmain` calls, so on an x64 CPU predating POPCNT (any Core 2 including Penryn, early
Athlon 64 X2) every invocation — including the bare help screen — dies with an illegal-instruction exception
before printing anything. These are exactly the CPUs the codebase pledges to support: the scalar unit "has to
run on any x64 CPU", `ThreadsRunningScalar` exists as the sub-SSE4.1 baseline, and the old D4 fix was
specifically about not executing an unsupported instruction on "a Core 2 or an early Athlon 64 X2 before any
test began". This reintroduces the same failure class one call earlier. **Fix:** a branchless software
popcount (the 0x5555/0x3333/0x0F0F fold) beside `LowestSetBit64`/`HighestSetBit64`, which are already
deliberately intrinsic-free; three call sites, none hot.

> **Fixed** as the entry prescribes. `SetBitCount64` (`CPU.h:663–684`) is the SWAR fold, placed with
> `LowestSetBit64` and `HighestSetBit64` in the `//--- Processor topology ---//` group and carrying the
> reason it is intrinsic-free in its doc comment, so the next reader does not "simplify" it back into
> `__popcnt64`. The three call sites — `WalkTopology`'s virtual-core count (`CPU.h:758`) and the two
> thread-count accumulations in `wmain` (`CPU.cpp:725–726`) — now call it, and the name
> `PopulationCount64` appears nowhere in the tree outside that comment and this entry. No `#undef` of the
> SDK's macro accompanies it: whether it is a macro or a `FORCEINLINE` differs by SDK version, and
> undefining a name other Windows headers may use to fence off three call sites of our own is the larger
> risk of the two.
>
> The entry's caveat — that its claim rests on winnt.h's declaration and was not confirmed against a real
> build — is retired rather than resolved, because the fix does not depend on which form the SDK uses. Both
> forms reach `__popcnt64`, and that intrinsic is documented to emit `POPCNT` with no `/arch` gate, so the
> only way to be certain the baseline unit stays baseline is not to route through it at all.
>
> Verified off-target, the arithmetic being platform-independent: the fold was transcribed verbatim into a
> harness and agreed with `__builtin_popcountll` on 20,262,274 masks — every single-bit mask, every
> low-run mask, the empty and full masks, every 16-bit pattern in each of the four 16-bit lanes, and 20
> million SplitMix64 draws — with zero mismatches. The codegen premise was checked the same way: compiled
> at `-march=x86-64` the fold emits no `POPCNT`, and at `-march=nehalem` and `-march=x86-64-v3` an
> idiom-recognising compiler substitutes one, which is the behaviour the intrinsic cannot offer. MSVC does
> not recognise the idiom at all, and `CPU.cpp` carries the global `StreamingSIMDExtensions2` setting and
> no `Optimization` override in either configuration — the four per-file overrides are the job units' — so
> no substitution arises on the shipped build path either. The copies of the function in those four units
> are never called: `WalkTopology` and `wmain` are both in `CPU.obj`, and an AVX2 or AVX-512 target carries
> `POPCNT` regardless.

## H. Build configuration

### H1. `CPU_build.h`'s `/fp:strict` guard exempts clang-cl, the build route it exists to police — **Medium** — *Fixed*

The guard is `#if defined(_MSC_VER) && !defined(__clang__) && !defined(_M_FP_STRICT)` (`CPU_build.h:35`).
clang-cl defines both `_MSC_VER` and `__clang__`, so it is exempt from the `#error` — yet a build made
outside `CPU.vcxproj` is precisely what the guard says it exists to catch. clang also defines none of the
`_M_FP_*` macros, so `VALUES_FP_MODEL` is 0 (:48–49) for *every* clang FP mode: two clang-cl builds under
`/fp:fast` and `/fp:precise` share a `buildID` and will exchange `cpu.values` files whose arithmetic rounds
differently — the false-`!Fail!` class the H2 fix exists to prevent, reachable through one unguarded
toolchain. **Fix:** either drop the `__clang__` exemption (clang-cl models `/fp:strict` and could satisfy a
suitable check) or `#error` outright on `__clang__` until the golden values are validated under it.

> **Fixed** by the second of the two. The first is not available: clang-cl does model `/fp:strict`, but it
> publishes no macro that says it did — there is no `__STRICT_FP__`, and `__FAST_MATH__` names only the mode
> at the opposite end — so a "suitable check" cannot be written, and a guard that cannot distinguish
> `/fp:strict` from `/fp:precise` is the defect this entry describes rather than a fix for it.
>
> The single condition is now a three-armed ladder. `__clang__` is refused outright, whatever else it defines,
> with a message saying the model cannot be checked; a toolchain defining neither `__clang__` nor `_MSC_VER`
> is refused for the same reason, that being the other route by which `VALUES_FP_MODEL` reached its `0` arm;
> and MSVC proper keeps the original `_M_FP_STRICT` requirement and the original message. The ladder is
> ordered so that clang-cl — which defines `_MSC_VER` as well — meets the clang arm first rather than being
> graded as an MSVC build.
>
> The consequence worth stating plainly is that `VALUES_FP_MODEL` can no longer be `0`. Its final `#else`
> is now unreachable, so the constant is `1` in every build that compiles, and `0` has stopped being a value
> that two builds with differently-rounding arithmetic could share a `buildID` through. That was the whole of
> the harm: a `cpu.values` written by a `/fp:fast` clang-cl build would have been accepted by a `/fp:precise`
> one, and the arithmetic difference reported to the user as a CPU fault.
>
> The cost is that clang-based tooling can no longer preprocess the tree. That is deliberate, and it is the
> state the entry's own remedy asks for: the golden values have never been generated or validated under
> clang, and the way to lift the restriction is to do that work and then relax the arm, not to leave the hole
> open. Verified across six toolchain/model combinations — only MSVC with `/fp:strict` is accepted, and a
> `static_assert(VALUES_FP_MODEL == 1)` holds under it.

### H2. Machine-specific `D:\Programming\include` in both `IncludePath`s; six vendored headers invisible to the project — **Low** — *carried forward (old H8)*

`CPU.vcxproj:49` and `:53` still append the author's local include path, and the six vendored headers remain
absent from the `ClInclude` list (:126–133) and from `CPU.vcxproj.filters`. A clean clone builds only
because `cl` silently ignores the nonexistent directory and quoted includes resolve to the repo copies
first; any future angle-bracket include would resolve against a path that exists on one machine.

### H3. The AVX2 unit's `#error` guard enforces only a lower ISA bound — **Low**

`CPU_jobs_AVX.cpp:19–21` rejects a build *below* AVX2 but not above it: `/arch:AVX512` also defines
`__AVX2__`, so raising this one file's per-file setting passes the guard while the unit's compiler-generated
code becomes EVEX-encoded — and `wmain` dispatches these kernels to any CPU reporting plain AVX2. The scalar
unit's H1 guard is two-sided for exactly this reason. **Fix:** add `#if defined(__AVX512F__) #error` beside
the existing guard.

### H4. `WholeProgramOptimization` is still present (set `false`) in two places `CLAUDE.md` says it is gone from — **Low**

`CLAUDE.md` states the setting "is gone and should not come back"; it is present as an explicit
`<WholeProgramOptimization>false</WholeProgramOptimization>` at `CPU.vcxproj:33` (Configuration group) and
`:96` (ClCompile). Functionally equivalent to absent — `false` emits neither `/GL` nor `/LTCG` — but the
document and the XML disagree, and an editor seeing an explicit `false` may take it for a toggled choice
rather than a removal.

### H5. The global `EnableEnhancedInstructionSet=StreamingSIMDExtensions2` is inert on x64 — **Low** — *unverified: needs a real MSVC build*

On x64, MSVC accepts only `/arch:AVX*`; the SSE2 setting (`CPU.vcxproj:65`, `:86`) maps to no switch, so
what actually keeps AVX-512 out of the baseline units is the *absence* of a higher per-file `/arch`, not
this setting's presence — `CLAUDE.md`'s build section attributes the isolation to it. Harmless either way;
worth confirming against the build log and rewording the documentation.

### H6. `OmitFramePointers=true` maps to `/Oy`, an x86-only switch — **Low** — *unverified: needs a real MSVC build*

`CPU.vcxproj:90`. The x64 compiler rejects `/Oy` with warning D9002 ("ignoring unknown option") on current
toolsets. Cosmetic if so — the switch is ignored — but the project file should not carry a setting the
target platform cannot express.

## I. Vendored headers

These six files are copies of the author's general-purpose library. Defects here are latent for PITC unless
noted — but they are live for any other client, and `CPU.h` includes two of them into every translation unit.

### I1. The pre-AVX2 `_mm_abs_pd` fallback expands to an AVX-512VL instruction — **Medium** — *carried forward (old I1)* — *Fixed*

`SIMD management.h:25`, inside `#if !defined(__FMA__) && !defined(__AVX2__)` — active precisely in units
compiled *below* AVX2 — defines `_mm_abs_pd` as `(fl64x2&)_mm_and_epi64(...)`, and `_mm_and_epi64` is
AVX-512VL+F; the companion `_mm256_abs_pd` (:27) likewise puts VEX opcodes behind an SSE2-era guard. All
five PITC units preprocess this header. The two kernel units that use the macros protect themselves with
`#undef` (`CPU_jobs_SSE.cpp:30`, `CPU_jobs_AVX.cpp:37`), but a future caller in `CPU.cpp` or
`CPU_jobs_standard.cpp` would embed an EVEX `vpandq` in baseline code. The guard belongs upstream.

> **Fixed** upstream, where the entry says the guard belongs. `_mm_abs_pd` is now
> `_mm_and_pd(_mm_castsi128_pd(_mm_set1_epi64x(...)), input)` — SSE2 throughout, and the same mask and the
> same result as the two unit-local definitions the kernels use, so nothing the kernels compute changes. The
> cast intrinsic replaces the `(fl64x2&)` reference cast in passing, which is old I5's hazard (now I17) in
> this one definition; the two unit-local copies the entry names still carry the reference cast and are still
> I17's subject.
>
> `_mm256_abs_pd` is now defined only under `#if defined(__AVX__)`. Its body was already VEX-encoded, so it
> had the same shape of defect as the 128-bit macro one width up: a header block active precisely below AVX2
> offered `vandpd` and `vpbroadcastq` to a unit compiled at the SSE2 baseline. `/arch:AVX` defines `__AVX__`
> without `__AVX2__` and still reaches the block legitimately, which is the case the macro exists for; an
> SSE2 unit now gets nothing rather than something it cannot execute.
>
> The two `#undef`s in `CPU_jobs_SSE.cpp` and `CPU_jobs_AVX.cpp` stay. They are no longer load-bearing
> against an EVEX opcode, but they remain the only thing that stops a bare `#define` in a shared header
> silently deciding which spelling the kernels are built from, and `CLAUDE.md` requires them.
>
> Verified by compiling the header at `-msse2`: the emitted body of `_mm_abs_pd` is `movapd`/`andpd`, with
> zero VEX- or EVEX-encoded instructions in the object. The original does not compile at that baseline at all
> under a compiler that enforces its target — `_mm_and_epi64` is simply not declared — which is the defect in
> its clearest form, MSVC having accepted it and emitted the instruction anyway.

### I2. `#ifndef` guards cannot detect intrinsic functions, so the FMA fallback macros always win — **Medium** — *carried forward (old I2)* — *Fixed*

`SIMD management.h:29–47`: `_mm_fmadd_ps` and friends are *functions* declared by `<immintrin.h>`, never
macros, so every `#ifndef` is true and the fallbacks are defined wherever the enclosing below-AVX2 block is
active. Code in a baseline unit calling `_mm_fmadd_ps` expecting the single-rounded contract silently gets
the two-rounding split form. PITC calls none of them today.

> **Fixed** by removing the macros rather than by repairing their guards, there being no guard that would
> work: the condition an `#ifndef` would have to test is whether a *function* has been declared, which the
> preprocessor cannot see. Any spelling that keeps the intrinsics' own names keeps the substitution invisible
> at the call site, which is the defect.
>
> The five macros are replaced by six `simd::` functions — `fmadd_ps`, `fmsub_ps` and `fnmadd_ps`, each
> overloaded on `fl32x4` and `fl32x8` — beside the `simd::fmadd_ps` the header already carried, which is the
> pattern this header had itself established for exactly this problem and then not followed. Each selects the
> true intrinsic under `#if defined(__FMA__) || defined(__AVX2__)` and the split form otherwise, so a unit
> that can execute `VFMADD` gets one rounding and a unit that cannot gets two — but the caller asked for
> `simd::fmadd_ps`, and `_mm_fmadd_ps` continues to mean `_mm_fmadd_ps` everywhere. The 256-bit overloads
> are guarded on `__AVX__` for I1's reason one width up: their fallbacks are `vmulps`/`vaddps`, which an SSE2
> unit cannot execute either.
>
> PITC calls none of them, before or after, so nothing it computes changes. Compiled clean at `-msse2`,
> `-mavx` and `-mavx2 -mfma`, with each function instantiated at both widths.

### I3. `CLASS_TIMER::dGrandTotal` accumulates the running total, not the elapsed increment — **Medium** — *carried forward (old I3)* — *Fixed*

`class_timers.h:60`: `Update()` executes `dGrandTotal += (dTotal += dElapsed)`, so `dGrandTotal` grows
quadratically with update count (1+2+…+n seconds after n one-second updates). `CLASS_TIMERS` repeats the
pattern (:170, :186). Latent for PITC — `wmain` calls `timer.Update()` once and reads only `siFrequency` and
`siCurrentTics`.

> **Fixed** in all three: each of `CLASS_TIMER::Update`, `CLASS_TIMERS::Update` and `CLASS_TIMERS::UpdateAll`
> now assigns `dElapsed` once and then adds *it* to each of the two totals, rather than nesting the
> assignments so that the grand total receives the running one. `dTotal` is unchanged in meaning; what
> changes is that `dGrandTotal` becomes what its name and its exemption from `Reset` say it is — the time the
> timer has run across resets of `dTotal`.
>
> Measured against a deterministic counter: after ten one-second updates `dGrandTotal` is 10.0 where it was
> 55.0, and three updates followed by a `Reset` and two more leave `dTotal` at 2.0 with `dGrandTotal` at 5.0,
> where the old form gave 9.0. Nothing in PITC reads either member, so the program's behaviour is unchanged.

### I4. `CLASS_TIMERS::Update` double-counts elapsed time; `UpdateAll` never advances `siPrevTics` — **Medium** — *carried forward (old I4)* — *Fixed*

`class_timers.h:169–170` runs the identical `dElapsed` computation twice back to back, so scaled totals
advance at twice real time; `UpdateAll` (:175–190) has the same doubled pair, assigns `siCurrentTics` twice,
and never writes `siPrevTics`, so its `siElapsedTics` measures from zero — roughly the machine's QPC epoch —
on every call. PITC never instantiates `CLASS_TIMERS`.

> **Fixed**, both halves. `CLASS_TIMERS::Update` performs the `dElapsed` computation once; the discarded
> first copy is gone, and with it the doubling of every scaled total the class reports.
>
> `UpdateAll` takes its counter reading into a local rather than into `timer[0].siCurrentTics`, which is a
> slot's own state and was therefore destroyed before slot 0 could record it as its previous reading; each
> slot then does `siPrevTics = siCurrentTics` before taking the new value, which is what `Update` had always
> done and what this function never did. The duplicated `siCurrentTics` assignment — two spellings of one
> value — is gone with it, as is the doubled `dElapsed`.
>
> `ReinitialiseAll` now calls `Reinitialise(index)` instead of holding a second copy of its body, that
> duplication being how the two came to seed a slot differently in the first place.
>
> Measured against a deterministic counter, across two slots updated by both routes: `dElapsed` is the time
> since that slot's own previous update rather than since the epoch, `dTotal` advances at real time rather
> than twice it, and two slots created together agree on their total elapsed time to the tic — where the old
> code had them 718 seconds apart after seven seconds of test.

### I5. `CLASS_TIMER::Reinitialise` leaves six members uninitialised, and the `dScale` clamp discards the acceleration — **Medium** — *carried forward (old I6)* — *Fixed*

`class_timers.h:46–53` never assigns `dGrandTotal`, `dScaleAccel`, `_dScaleAccelTemp`, `siPrevTics`,
`siElapsedTics` or `siPauseTics`, so a non-static `CLASS_TIMER` reads indeterminate values in its first
`Update` (`dScale += dScaleAccel` on garbage). Sharper than previously recorded: :59 reads
`(dScale += dScaleAccel) = clamp(...)` — under C++17 sequencing the assignment overwrites the compound
result, so the acceleration is added and then unconditionally discarded. PITC's one instance is a
zero-initialised global that never uses scaling.

> **Fixed**, both halves, and in `CLASS_TIMERS::Reinitialise` as well as `CLASS_TIMER::Reinitialise` — the
> slot form left the same six members unassigned, `salloc`'s bit pattern being all that stood behind them.
> Each function now states every member of its own object: `dGrandTotal`, `dScaleAccel`, `_dScaleAccelTemp`,
> `siPrevTics`, `siElapsedTics` and `siPauseTics` beside the ones that were already there. `siPrevTics` is
> seeded from the origin reading so that a first `Update` measures from construction, and the rest are zero.
>
> The clamp is now two statements — `dScale += dScaleAccel;` then
> `dScale = (dScale < 0.0 ? max(...) : min(...));` — which is the arithmetic the single expression was
> written to perform. Spelt as `(dScale += dScaleAccel) = clamp(dScale…)` C++17 evaluates the right operand
> first, so the clamp read the *unaccelerated* `dScale` and the store then overwrote the compound assignment's
> result: the acceleration was added and discarded on every update, and `Pause`, `UnPause` and `Reset`'s time
> scaling did nothing at all. The same spelling in `CLASS_TIMERS::Update` and `UpdateAll` is corrected with
> it.
>
> Measured: with `dScaleConst` 2.0 and an acceleration of +0.5 per update, `dElapsed` for successive
> one-second updates is now 1.5, then 2.0, then 2.0 — ramping to the ceiling and holding there — where the
> old form returned 1.0 every time. PITC's one instance is the zero-initialised global the entry describes
> and uses no scaling, so nothing it reports changes.

### I6. `GetTotalTimeUnscaled` reports QPC-epoch uptime: `siTotalTics` is seeded with `siOriginTics` instead of 0 — **Medium** — *new; same family as I3/I4* — *Fixed*

`class_timers.h:51`: `Reinitialise` seeds `siTotalTics` with the raw counter reading while its
floating-point twin `dTotal` is seeded 0.0, so `siTotalTics` permanently equals `siCurrentTics` and
`GetTotalTimeUnscaled` (:105) returns time since boot epoch, disagreeing with `GetTotalTimeScaled` by the
machine's uptime. Latent for PITC.

> **Fixed** in both `Reinitialise` functions: `siStartTics` and `siCurrentTics` keep the origin reading,
> because they are timestamps and an origin is what they mean, while `siTotalTics` is seeded 0 beside
> `siElapsedTics` and `siPauseTics` — it is a duration, and `dTotal`, its floating-point twin, was already
> seeded 0.0 two lines away.
>
> Measured: after ten one-second updates `GetTotalTimeUnscaled` returns 10.0 against `GetTotalTimeScaled`'s
> 10.0, the two agreeing at unit scale as they should. The old form returned 110.0 for the same run, the
> extra 100 seconds being the fake counter's epoch — on a real machine it would be the uptime, and would grow
> with every day since boot.

### I7. `malloc` is overloaded in the global namespace — **Low** — *carried forward (old I7)*

`memory management.h:214`: `inline ptrc malloc(csize_t, csize_t)` adds an overload of a reserved name —
formally undefined, tolerated by MSVC beside the CRT declaration. Reachable in every PITC run through the
`malloc64` macro.

### I8. `min` and `max` are lowercase function-like macros in a public header — **Low** — *carried forward (old I8)*

`class_timers.h:16, :20`. Every PITC unit inherits them via `CPU.h`; `CPU.cpp:209` relies on the `min`
macro. Any later `std::min`/`std::max` use, or an `<algorithm>` include, collides.

### I9. `va_arg` with a const-qualified type — **Low** — *carried forward (old I9)*

`memory management.h:370`: `mdealloc_` steps with `va_arg(val, ptrc)` where `ptrc` is `void *const` —
formally UB, benign on MSVC, and executed on every PITC exit path that frees the arena or the config
bitmaps.

### I10. `ThreadsRunningAVX512` executes AVX2 instructions, contradicting its own comment and the design it justifies — **Low** — *carried forward (old I10)*

`CPU_jobs_AVX512.cpp:269–273` delegates to `AllFalse(cui512&, cui512&)` (`common functions.h:185–188`),
implemented as two `_mm256_testz_si256` calls — so the poll selected because the CPU has AVX-512 issues only
AVX2 `vptest`s. Functionally correct (the two 256-bit tests cover all 64 bytes); the contradiction is with
the comment above it, `CPU.h:475–478`, and `CLAUDE.md`, all of which describe a genuinely 512-bit read.

### I11. Every `#ifdef __AVX512__` fork is dead code, and the `/arch`-forked inline functions are a live multi-TU ODR hazard — **Medium** — *carried forward (old I11)*

Eleven conditionals in `memory management.h` (:176, :385, :410, :422, :447, :486, :538, :584, :651, :667,
:705) fork on `__AVX512__`, a macro no compiler defines — MSVC's is `__AVX512F__` — so the 512-bit arms
(`Copy`, both `Copy64`s, `Stream64`, `mzero`, `mset`, `zalloc64`'s wide path) are unreachable even in the
AVX-512 unit. New since the program grew five translation units: these `inline` functions now compile to
*different bodies* per unit (the below-AVX2 units get the SSE arm, the AVX units the AVX arm), so a future
call to the same function from two units is an ODR violation where the linker keeps whichever body it saw
first.

### I12. Latent defects in the parts of `memory management.h` PITC does not call — **Medium** — *carried forward (old I12)*

Still present, none reachable from PITC's live call set: `Stream()` (:722–727) rounds the byte count *up*
before delegating, overrunning source and destination by up to 63 bytes; the volatile `Copy64` (:666–679)
ceils likewise while the const overload floors; `salloc` performs stores wider than the alignment it was
asked for; the `DATA_TRACKING` accounting block does not compile if enabled; `mfree`'s arity/terminator
convention is fragile. Flagged so an upstream sync does not import a caller.

### I13. Assorted defects in `typedefs.h` and `vector structures.h` — **Low** — *carried forward (old I13)* — *Fixed*

All bullets remain at current lines: `al1` is `__declspec(align(2))` (`typedefs.h:36`), the lowercase object
macro `vol` (:44) leaks into every unit, eleven derived `fl80` aliases reference a base alias that does not
exist, and several unions in `vector structures.h` declare member arrays narrower than their siblings
(`AVX8Ds64.ymm[1]`, `AVX64Ds8.vector[9]` read past the declared member).

### I14. Inline changelog blocks remain in two headers — **Low** — *carried forward (old I14)* — *Fixed*

`typedefs.h:20–29` (self-annotated "&lt;- Move to CHANGELOG.md") and `vector structures.h:5–7`; GCS c1
forbids history in prologs and the CHANGELOG.md it should migrate to still does not exist (L1).

### I15. `SIMD management.h` is not self-contained — **Low** — *Fixed*

Its only include is `<immintrin.h>` (:11), yet its function signature uses `fl32x4` (:14) and its macros use
`fl64x2`/`fl64x4`/`si128` (:25–27) — aliases from `typedefs.h`. It compiles in PITC only because
`common functions.h` happens to include `typedefs.h` first; included in isolation it is a build blocker two
files removed from its cause.

### I16. `refp1a1` expands to a cast to an array type, ill-formed on any use — **Low**

`typedefs.h:577`: the macro produces a C-style cast whose type-id is an array type, which C++ forbids —
every expansion is a compile error on every compiler, unlike its `defpa`/`refpa` companions. No PITC file
uses it.

### I17. The unit-local `_mm_abs_pd` binds a non-const reference to an intrinsic rvalue — **Low** — *carried forward (old I5)*

`CPU_jobs_SSE.cpp:31` (and the upstream original, `SIMD management.h:25`): `(fl64x2&)_mm_set1_epi64x(...)`
reinterpret-casts a prvalue to a non-const lvalue reference — ill-formed in standard C++, accepted only as
an MSVC extension (C4238) despite both configurations setting `ConformanceMode`. clang-cl rejects it
outright, at every kernel line.

### I18. `CLASS_TIMERS` never frees its `salloc`'d timer table — **Low**

`class_timers.h:121`: the 2 KB `TIMER_VARIABLES[MAX_TIMERS]` block is allocated by an in-class initialiser
and the struct has no destructor — contrary to GCS p2 and the repo's own destructor-owned-pointer pattern
(`GLOBAL_CFG`, `RESULTS_ARRAYS`). PITC never instantiates `CLASS_TIMERS`.

## J. Job kernels

### J1. The "SSE4.1" unit exercises no SSE4.1 arithmetic, and the "AVX2" unit no AVX2 arithmetic — **Medium** — *carried forward (old J3)*

`JobSSE`/`JobALU_SSE`/`JobMemSSE`/`JobMemALU_SSE` (`CPU_jobs_SSE.cpp:48–247`) use only
`_mm_set1_pd/div/sqrt/add/sub/mul/and/set1_epi64x` — all SSE2. The unit's sole SSE4.1 instruction is `PTEST`
in `ResultsMatch` (:261) and the completion poll — the comparison, not the computation — yet an SSE4.1-less
CPU is refused the whole unit with `-11`. Likewise the AVX2 unit's kernels use only `_mm256_*_pd` forms that
are AVX1; the AVX2-ness is the compiler's encoding, not the arithmetic under test. A user running `Is` or
`Iv` to exercise the instruction class named on the tin is testing less than the name says. Renaming or
extending the kernels both change the golden values, so this stays a documentation-versus-substance defect
until a `cpu.values` version bump is scheduled anyway.

### J2. The scalar FPU kernels call bare `abs()` on doubles — **Low** — *carried forward (old J5)*

`CPU_jobs_standard.cpp:66–69` and every FP chain in that unit rely on `<cmath>` injecting the floating-point
`abs` overloads into the global namespace — guaranteed by MSVC, not by the C standard, where global `abs` is
`int abs(int)`. An include reshuffle that dropped the overload would truncate every FPU intermediate to an
integer and silently change every golden value. One `std::` qualification (or `fabs`) per site removes the
dependency; note the change is spelling-only under MSVC today, so `cpu.values` is unaffected.

## K. Documentation and localisation

### K1. Preset 8/9 "Synchronised" labels are swapped in the help text and README — **Medium** — *carried forward (old K2)*

`en-GB.h:86–87` and `README.md:61–62` call preset 8 "Synchronised staggered" and preset 9 plain "Staggered",
but the code sets preset 8 to `0x024` — staggered fixed pulse, *no* time-sync bit — and preset 9 to `0x02C`,
which carries it (`CPU.cpp:593, :600`; the inline comments there agree with the code). A user choosing `-8`
for the synchronised test gets the unsynchronised one and vice versa.

### K2. README omits the mandatory `W` first step, the exit codes, and any build instructions — **Low** — *carried forward (old K6)*

`README.md` never states that `PITC.exe W` must run before any test (without `cpu.values` every test returns
`-1`), carries no return-value table, and gives no build line, toolchain version or x64-only note for a repo
with no `.sln` and no CI.

### K3. Version 1.0.2 exists only in prose, and the README fronts a benchmark screenshot from before the score rescale — **Low** — *carried forward (old K7)*

Three hand-maintained prose sites (`README.md:1`, `en-GB.h:12`, `CLAUDE.md`), no `VERSIONINFO` resource, and
the number has not moved across ~95 behavioural fixes — including the E12 rewrite that multiplied the KUPS
scale by ~4.5×, which makes the 5950X screenshot at `README.md:69–70` misleading for anyone comparing a
current run against it.

### K4. Typographical errors persist in the user-facing text — **Low** — *carried forward (old K8)*

"Successul completion" (`en-GB.h:15`), "intruction" (`en-GB.h:48` and `README.md:14`), and the preset-7
comment "30 minute duration. 10 minute duration" (`CPU.cpp:586`), of which the code implements the first.

### K5. Failure-report and results-table strings remain outside the translation tables — **Low** — *carried forward (old K9)*

Beyond the disclosed `wstrUnitsCPU`/`wstrSyncCPU`/`wstrPass`, the per-unit value formats inside `Failed()`
(`CPU.cpp:72–91`) and the results-table row labels and separators (`CPU.cpp:1029–1051`) are literals the `L`
mechanism cannot reach — the one output a failing CPU produces is only partly translatable.

### K6. `translations.h` cannot express a second language without code edits, and the console path would garble one — **Low** — *carried forward (old K10)*

Adding a language still means writing `<code>.h`, editing `translations.h` and extending the `L` case — and
the help text's "Recognises ISO 639-1 language codes" oversells a hard-coded en-GB/en-US comparison whose
example is a BCP-47 tag. Additionally, no `_setmode(_O_U16TEXT)`/`SetConsoleOutputCP` call exists anywhere,
so `wprintf` output converts through the ANSI code page: pure-ASCII English survives, but any future
translation with characters outside the console code page will print mojibake through the very mechanism
built to carry it.

### K7. The no-option defaults are documented nowhere — **Low** — *carried forward (old K12)*

15-minute duration, ALU+FPU, parallel constant execution, 2000 ms start-up delay, and the 100 ms/900 ms
pulse a later mode change inherits (`CPU.cpp:139–142`, `CPU.h:83–85`) appear in no document; only the preset
defaults are stated.

### K8. GCS v1.1.4 MUST-rules the repository reviews against are violated in the tree — **Low** — *carried forward (old K13)*

Five lines exceed the 180-column hard cap (`CPU.cpp:74, :1038, :1046, :1049, :1051`); every PITC-proper file
keeps the boxed prolog instead of the r17 template; the `JobCycle` dispatch table is lowercase against r12's
UPPER_SNAKE for global tables.

### K9. Dead configuration declarations misdescribe the program — **Low** — *carried forward (old K14)*

`GLOBAL_CFG`'s in-class defaults `procUnits = 0x04` / `procSync = 0x02A` (`CPU.h:86–87`) are overwritten
unconditionally by `CPU.cpp:140–141` before any argument is read, so the struct lies about the defaults; and
four `THREAD_CFG` fields (`packetSizeRAM`, `maxTics`, `inactiveTime`, `records32`) are written in the spawn
loop and read by nothing.

### K10. `CLAUDE.md` says exit code `-21` is shared by five messages; the code has four — **Low**

Exactly four `return -21` sites exist, printing `wstrMessage[26]`–`[29]` (`CPU.cpp:682, :690, :700, :717`).
A maintainer auditing the table against the document hunts for a fifth rejection path that does not exist.

### K11. The help claims every efficiency core of a hybrid CPU carries a single virtual core — **Low**

`en-GB.h:74` / `README.md:47`: "A physical core carrying a single virtual core is kept by both; on a hybrid
CPU that is every efficiency core." True of Intel hybrids only; the G9 rework explicitly supports parts
whose efficiency class carries SMT (`CPU.h:951–953` names Zen 4 + Zen 4c). On such a part `Ue` keeps one
thread per physical efficiency core — half that class's virtual cores — against the documented expectation.

### K12. `Ut`'s 64-characters-per-group numbering is documented nowhere users look — **Low**

`Ut` character *n* is deliberately bit `n & 63` of group `n >> 6` (`CPU.h:1128–1134`; the design is
documented in the source and `CLAUDE.md`). But `en-GB.h:69` and `README.md:42` present `Ut` as a plain
binary sequence, and the banner prints each group's row only to its highest core — so on a machine whose
groups hold fewer than 64 virtual cores (Windows commonly splits large machines evenly), nothing tells the
user that characters for a group's unpopulated high bits are dead padding they must write anyway to reach
the next group.

## L. Project process

Everything here is mandated by `CONTRIBUTING.md` → `GDC_GCS_v1_1_4.md` and remains absent, unchanged from
the previous audit.

| # | Missing | Required by | Severity |
|---|---------|-------------|----------|
| **L1** | `CHANGELOG.md` at the repository root — ~95 behavioural fixes, a golden-file format change and a 4.5× benchmark rescale are recorded nowhere a user will look | GCS c2 | Low |
| **L2** | `.clang-format` and `.editorconfig` (GCS `tc1`, `tc2`) — which is how the five over-180-column lines of K8 entered | GCS | Low |
| **L3** | Pre-commit hooks and CI gates — a codespell hook alone would have caught the K4 typos; a clean-clone build job would prove the vendored headers sufficient (H2) | GCS en1/en2 | Low |
| **L4** | Any automated test — no `tests/` directory, no test target, for a program whose correctness story is 18 hand-duplicated kernels and a golden-file format | GCS; `common functions.h:10` to-do | Low |
| **L5** | `.gitignore` — `x64/`, `*.obj`, `*.pdb`, `*.map` and `cpu.values` are all offered for commit, and a committed `cpu.values` is machine- and revision-specific | — | Low |

---

## Appendix A — candidate findings rejected under verification

Recorded so they are not rediscovered. Both were submitted as defects and refuted against the source and its
design documentation; each leaves only the small documentation residue noted.

**`Ut` "assumes 64 cores per processor group".** The mechanical observation is accurate — character *n* maps
to bit `n & 63` of group `n >> 6`, so on groups smaller than 64 some characters are dead — but this is the
explicitly documented, deliberate design (`CPU.h:1086–1094`, `CLAUDE.md`'s parsing rules), chosen over
ordinal packing in the same function that implements ordinal walking for `Uc`. The surviving residue is the
user-documentation gap recorded as K12.

**"`KernelFingerprint` covers only 5 of the 18 kernels, so a `JobMem*` edit does not invalidate an existing
`cpu.values`".** Factually accurate but not a missing check: the file's contents depend only on the five
register-resident kernels, so after a `JobMem*`/`JobALU_*` edit the existing file is *not* stale — the
drifted kernel is in the binary, which no file-side fingerprint can detect, and `W`'s `-22` family check is
the layer that owns it (`CPU.cpp:449–452` states the division of labour verbatim). The genuine gap in that
layer — no check of the *cross-unit* ladder equivalence — is recorded as B1.

## Appendix B — command lines that misbehave today

| Command line | Machine | Behaviour |
|---|---|---|
| ~~any, including none~~ | ~~x64 CPU without POPCNT (Core 2, early Athlon 64 X2)~~ | ~~Illegal-instruction crash before any output~~ — **fixed** (G1) |
| `Mn8 Ms8` | any hybrid or two-class machine | Refused `-18` "Only 0 bytes of memory per thread"; `Ms8 Mn8` runs (F1) |
| `Ia Tct5.0 Ue` (the documented smoke test) | Windows set to a comma-decimal locale | Refused `-25` "Unrecognised '.' option" (F2) |
| `Ia Sr Tft60[15000]15000 Ua` | 16+ virtual cores | "60-second" run takes ~7.5 minutes; threads 3+ graded on one job cycle (E1) |
| ~~`-1 Mc999999 O[results.txt]`~~ | ~~any, with an existing `results.txt`~~ | ~~Exits `-17` having silently truncated the previous results file~~ — **fixed** (C1) |
| `-8` | any | Runs the *unsynchronised* staggered preset the help text calls synchronised (K1) |
