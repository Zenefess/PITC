# PITC — Known Issues and Defects

**Audit of:** Pulsed Integrity Tests for CPUs v1.0.2
**Scope:** every file in the repository — `CPU.cpp`, `CPU.h`, `CPU_methods.h`, `CPU_job_cycles.h`, the four
`CPU_jobs_*.cpp` kernel units, the six vendored shared headers, `CPU.vcxproj`, and all documentation.
**Method:** static review only. The repository has no test suite, no CI and no lint tooling, and the target
platform (Windows x64 / MSVC) was not available to the audit, so nothing here was reproduced by execution and
neither the Windows SDK headers nor MSVC's codegen could be inspected. Claims about SDK-provided declarations
and about which instructions the compiler actually emits are therefore reasoned from the source alone and
should be checked against a real build. Every entry cites the line it was derived from; please re-confirm
against the current source before acting.

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
| A — Verdict integrity | 6 | 4 | 3 | 0 | 13 |
| B — Golden-value generation (`W`) | 2 | 1 | 4 | 1 | 8 |
| C — Memory safety | 1 | 5 | 4 | 3 | 13 |
| D — Concurrency | 0 | 3 | 5 | 1 | 9 |
| E — Scheduling and timing | 0 | 2 | 8 | 2 | 12 |
| F — Command-line parsing and output | 0 | 4 | 5 | 6 | 15 |
| G — Topology and scaling | 0 | 4 | 5 | 0 | 9 |
| H — Build configuration and determinism | 0 | 1 | 4 | 5 | 10 |
| I — Vendored headers | 0 | 0 | 8 | 6 | 14 |
| J — Job kernels | 0 | 1 | 4 | 2 | 7 |
| K — Documentation and localisation | 0 | 0 | 6 | 8 | 14 |
| L — Project process | 0 | 0 | 0 | 5 | 5 |
| **Total** | **9** | **25** | **56** | **39** | **129** |

**A1–A7, A9, A10, B1, B2, B3, B5, B6, C1–C6, D1, D2, D3, D5, E1, E2, F1, G1, G2, G4, G5, G6, G7, H1, H2,
H7, J1, J2 and J7 are fixed** — see the
`Fixed` notes on those entries. The counts in the table above are as first audited and have not been decremented. A1–A3 restored
bit-exact verification on the SSE and AVX2 paths and made the summary table able to report an AVX-512
failure; A4–A6 and F1 were the false-pass family of Appendix B item 2, where the configuration itself
guaranteed that no work was done, or no time was spent doing it. Rejecting those configurations also closed
**E9** and the sweep half of **E4**, made **E10** unobservable, and the documentation they touched closed
**K1** and two bullets of **K4**. B1–B3 made `W` able to produce a correct `cpu.values` at all: the
per-thread ranges now tile the table exactly, the SSE ladders cover each of the 16 lanes once, and a
finishing thread clears only its own completion bit.

E1 and E2 are the scheduling pair. E1's ~49-day sleep is closed at both ends — the executor leaves its loop
as soon as a fresh reading shows the deadline has passed, and the sweep ramp is clamped to `[0, cycleTime]`
before it is narrowed — which also closes the overrun half of **E5**. E2 replaced every `Sleep` in a worker
thread with a per-thread `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` waitable timer, so the pulse edges and the
start-up delay are accurate to well under a millisecond instead of being quantised to the 15.625 ms
scheduler tick; `timeBeginPeriod` was rejected as a machine-wide change to the idle behaviour under test,
and `<timeapi.h>`, whose presence named E2, is gone. Neither entry changes a job kernel, so `cpu.values`
does not need regenerating. E3, E4's benchmark half, and E6–E12 are untouched.

C2–C5 are the arena-layout family, and they are one edit: the `switch` that used to compute `records[]` and
four ALU offsets now yields only a record size and a record's vector width, and the sizing, the rounding,
the guards and the one ALU-base expression follow it. C2's missing multiplier turns out to fire on *any*
topology with non-SMT threads — an SMT-disabled or non-SMT CPU is the worst case, not a safe one — while
C3 needs both classes populated; C4's round-down to a multiple of 4 stops the one-to-three-record overrun
that every memory-backed run on a CPU below AVX-512 performed, preset `-0` included on every CPU; C5 adds a
pre-flight size check and a null check on one shared exit code. Together they make the arena tile exactly
for every unit set, thread-class mix and `M` mode tested — 31 hand-written configurations and a 1 341-case
sweep, of which the pre-fix code fails 23 and 856 respectively.
Two new exit codes, `-17` and `-18`, are documented in `wstrInstructions_English`, and `wstrMessage_*` grew
from 20 entries to 23. C6 is unrelated to those and is the clamp its entry recommends. **C7–C13 are
untouched**, and C9 in particular is now visible rather than silent: `Mn8` with no `Ms` exits with `-18`
instead of quietly running the register code path.

A9, A10 and C1 are the malformed-`I`-string family, and one pre-flight rule closes all three: a unit
selection must name at least one processing unit (`-15`) and at most one of FPU/SSE4.1/AVX2/AVX-512 (`-16`).
That removes the `JobCycle` index overrun, the arena sized in bytes rather than records, the empty results
table, and the overlapping sub-array seeds together. `JobCycle` was also widened to the full 0–31 index
domain so the overrun cannot return by another route. The two new exit codes are documented in
`wstrInstructions_English`, and `wstrMessage_*` grew from 18 entries to 20.

A7, D1, D2, D3 and D5 are the concurrency batch, and four of the five are one story: nothing that touched
`threadBits` or the clock was safe to do the way it was being done. Every write to the completion bitmap is
now byte-wide and interlocked from both sides — `SetThreadRunning`/`ClearThreadRunning` in `wmain` (D1),
`Failed()` clearing one bit rather than eight (A7) — and every read of it examines the whole 64 bytes once
(D3). Workers no longer touch the global `CLASS_TIMER` at all beyond `siFrequency`, taking their timestamps
from `CurrentTics()` instead (D2), which as a side effect gives every thread the identical `startTics` the
`T-Sync` shapes were always supposed to have. D5 replaced `_beginthread` with `_beginthreadex` at both spawn
sites, created suspended so the affinity mask lands before the thread runs, and added one exit code, `-19`,
for a thread that could not be created or resumed; `wstrMessage_*` grew from 23 entries to 25, the second of
the two being the warning printed when `SetThreadAffinityMask` fails. No job kernel is touched, so
`cpu.values` does not need regenerating. The bitmap and the three polls were exercised off-target, against a
stand-in harness that walks all 512 thread indices: the pre-fix forms fail 327 of its checks — 192 SSE poll
misses (threads 320–511), 128 AVX2 poll misses (threads 384–511) and the seven neighbours one `Failed()`
clears — and the current forms fail none. The rest of the batch is Windows-specific and was reviewed, not
executed; the MSVC build still has to confirm it.

**D5 has a consequence worth acting on: affinity now actually reaches the scheduler**, where a
`_beginthread` handle meant no mask ever did. That promotes **G5** — the mask restarting at bit 1 for the
SMT thread class — from a latent defect to a live one on any topology carrying both non-SMT and SMT threads.
Uniform SMT CPUs are unaffected, and **G5 should be the next entry taken** — it was, in the topology batch
below, where measuring it showed the "uniform SMT CPUs are unaffected" qualifier to be the whole of the
story only for CPUs that are not hybrid. **D4 and D6–D9 are untouched**,
and D8 in particular now stands alone: with A7 fixed, the only completion bit cleared ahead of a thread's
last write is that thread's own.

Note that B1–B3 shifted every line in `CPU.h` after 231 down by two, so line citations below that point in
`CPU.h` — B4, B5, B7 and B8 among them — read two low against the current source. A9, A10 and C1 shifted
three more files: `CPU.cpp` by twelve lines (nine of them ahead of the memory-allocation block, so C2–C5 and
C8–C13 are all below the edits), `CPU_job_cycles.h` by six at the dispatch table, and `en-GB.h` by five.
Citations into those files read correspondingly low, and several were already stale before these fixes — as
the preamble says, re-confirm every line number against the current source before acting on an entry.

C2–C6 shifted two files again. In `CPU.cpp` the `L` option gained two comment lines at `:141`, so every
citation past it — C2–C13, D1, E11, F2–F15 among them — reads two lines low on top of the twelve A9/A10/C1
already added; the arena block itself grew from 76 lines to 93, all of it between `:518` and `:611`, so
citations *below* the arena read nineteen lines low in total. `en-GB.h` gained five lines: two exit codes in
the instruction table and three messages, the latter at the end of the array. `CLAUDE.md` was updated to
match, so its arena, exit-code and localisation sections describe the code as it now stands.

E1 and E2 shifted two more. `CPU.h` lost its `<timeapi.h>` include at line 15 and gained a 62-line
pulse-timing group before `SetSMTLoading`, so citations between those two points read one line high and
everything below `SetSMTLoading` — A7's `Failed()`, and B4, B5, B7 and B8 in `GenerateValues` — reads
sixty-one lines low on top of the two B1–B3 already added. `CPU_methods.h` grew by twelve lines, all of them
at or below its line 18, so every citation into it reads low: the loop cited at `:63–71` now begins at `:67`.

A7, D1–D3 and D5 shifted four files again, `CPU.h` the most. It gained 48 lines in four places: four at
`:21` (the `MAX_THREADS_WORDS` constant), 24 at `:143` (the interlocked bitmap helpers, the rewritten
`ThreadsRunning*` group, and the second line the SSE poll now needs), 14 at `:179` (`CurrentTics`) and six
at `:243` (`Failed`'s mask). A citation therefore reads 4 lines low between `:21` and `:142`, 27 or 28
through the `ThreadsRunning*` group, 42 between `:179` and `:242`, and 48 below that — `GenerateValues`, and
so B4, B5, B7 and B8, on top of every earlier shift.
`CPU.cpp` gained 20: eight in the `W` spawn loop (now `:339`) and twelve in the test spawn loop (now
`:641`), so citations between the two read 8 low and citations below the second read 20 low.
`CPU_methods.h` gained five, two of them at its line 14, so `ComputationPulse`'s loop moves down again. `en-GB.h` gained three: one exit code
in the instruction table and two messages at the end of the array. `CLAUDE.md` was updated to match — its
exit-code, thread-lifecycle and known-constraints sections now describe the code as it stands, and the two
bullets that named A7 and D3 as live constraints are replaced by one naming G5.

B6, H1 and H2 are the provenance batch, and they are one story from three directions: nothing in the build
or in `cpu.values` recorded what had produced the file, so the tool's likeliest failure — a file left over
from an earlier kernel revision or a differently-rounded build — arrived at the comparison intact and was
reported as a CPU fault. H1 and H2 remove the two ways two builds of the same source could disagree about
what a correct result is (an `/arch` override on the baseline unit, and an unpinned floating-point model),
and each is now enforced by an `#error` rather than by the project file alone: `CPU_jobs_standard.cpp`
refuses to compile with any `/arch` above the SSE2 baseline, and the new `CPU_build.h` refuses to compile
any kernel unit without `/fp:strict`. B6 then makes the file say what produced it — magic value, format
version, block geometry, build ID, kernel fingerprint and a hash of each block, in a 64-byte header — and
checks every read and write for length, which none of the four calls did. Two exit codes are added, `-20`
and `-21`, and `wstrMessage_*` grew from 25 entries to 30.

**Any `cpu.values` generated before this batch must be regenerated with `W`**: it has no header. That is the
one user-visible cost, and it is the point — the same check is what will reject a file after the next kernel
edit, which `CLAUDE.md` previously had to ask the author to remember. The kernel fingerprint is computed by
running the golden ladder over a fixed seed, so it tracks the kernels rather than a revision constant
somebody has to bump; the ladder itself is now a single `RunGoldenLadder` shared by both halves of
`GenerateValues` and by the fingerprint, which makes the "same pattern in both halves" invariant structural.
`RESULTS` is untouched, and the header is verified 64 bytes with no padding. **B4, B7, B8 and H3–H6 are
untouched**; H3 in particular is now the visible remainder of H1's area, and H5 and H6 were already largely
overtaken by commit `39f0d4b`. B5 and H7 are closed by the batch below.

`CPU.h` is 101 lines longer than it was — 410 now 511 — and moved in three steps: one include at `:18`, a
132-line `"cpu.values" file format` group between the `Job*` externs and the thread-completion bitmap, and
32 lines *removed* from `GenerateValues` when its two ISA ladders collapsed onto `RunGoldenLadder`. A
citation into `CPU.h` therefore reads 1 line low between `:18` and `:146`, 133 low from there to
`GenerateValues`, and 101 low below it — B4, B7 and B8 among them — on top of every earlier shift.
`CPU.cpp` gained 49 — 741 now 790 — one declaration at `:19`, fourteen in the `W` path and 34 at the file
read, so a citation reads 1 low above the `W` case, 15 low between it and the read, and 49 low below the
read — every C, E and F entry in `wmain` past that point. `en-GB.h` gained seven, two exit codes in the
instruction table and five messages at the end of the array. `CLAUDE.md` was updated to match: its build,
golden-value-model, `GenerateValues` and exit-code sections now describe the format and the two guards as
they stand.

J1, J2, J7, B5 and H7 are the job-kernel batch, and four of the five are one rewrite. J1 and J2 had to be
taken together because either one alone changes every golden value: the shift predicate becomes `i < 8`, so
the right-shift arm executes for the first time, and the chain moves into `ui64` so that its wraparound is
defined by the language rather than by a project setting. J7 adds the running accumulator its entry
proposes, which raises single-ULP fault detection in the FP kernels from 21 % to 62 %; the measurement that
drove the design is that the sensitivity is lost at *one* rounding — the fourth root that opens every step —
and not gradually, so the accumulator's value is that it reaches the result without passing through one.
B5 is the check that keeps all of this honest: `ValidateKernelFamilies` requires the thirteen memory-array
and combined kernels to reproduce their five register-resident counterparts byte for byte, and `W` runs it
before generating anything. That mattered immediately — the eighteen kernels were edited together, and the
check is what establishes that they still agree. H7 deletes the two `Win32` configurations and adds an
x64 `#error` to `CPU_build.h`, which is also what makes J2's guarantee unconditional.

The four `CPU_jobs_*.cpp` files were regenerated from a single description of the arithmetic rather than
edited eighteen times by hand, which is the practical answer to **J6** even though that entry stays open:
the generator is not in the repository, but its output is what B5's check now verifies at run time. Two
knock-on tidies came with it — the SSE unit's longest line drops from 195 columns to 121, inside the GCS
180 hard cap it had been breaking, and same-line statements are separated by three spaces per `r4`. One
exit code is added, `-22`, and `wstrMessage_*` grew from 30 entries to 31.

**Any `cpu.values` from before this batch must be regenerated with `W`.** Every golden value changes. The
header's `KernelFingerprint` detects that on its own and reports `-21`, so a stale file is refused rather
than blamed on the CPU — which is the guard doing exactly what B6 added it for. **J3, J4, J5, J6 and B4,
B7, B8 remain open.**

G1, G2, G4, G5 and G6 are the topology batch, and three of them are one story: the code knew how many cores
there were and never recorded how they were arranged. `SetSMTLoading` rebuilt the sibling layout from a core
count and a stride every time it ran, which on a CPU reporting no SMT meant shifting by `ui64(0 - 1)` and by
64 (G1), on a hybrid part meant a mask describing the P-cores that cleared every E-core (G2), and in either
case leaned on a `groupCount` that omitted the SMT core count, doubled the non-SMT one and added 1 (G4). The
enumeration has the layout in its hand, one record at a time, and now keeps it: `cfg.sys.coreSibling` holds
the lowest and the highest sibling of every physical core, `SetSMTLoading` masks with one of the two, and
`groupCount` counts the groups the walk populated rather than predicting them. `cfg.sys.SMT` is normalised
to 1 when nothing set it. **G7 goes with them** — the arm no longer computes an offset, so the scan that
could only return 0 or 64 has nothing left to be wrong about — and half of **G8**, whose shadowed loop
variable disappeared with the arm that needed the outer one. G6 is separate: `GetLogicalProcessorInformation`
is checked, a walk that names no core is refused alongside it, and both raise the new exit code `-23`;
`wstrMessage_*` grew from 31 entries to 33. No job kernel is touched, so **`cpu.values` does not need
regenerating**.

The batch was verified off-target, in the same way the concurrency batch was: both `SetSMTLoading` arms, the
walk, the `groupCount` derivation and the spawn loop's affinity assignment were transcribed pre- and
post-fix into a harness and run over seven synthetic topologies — uniform 2-way and 4-way SMT, a 6-core and
a 1-core non-SMT part, a 32-core 2-way part, and two hybrid P/E mixes — with the pre-fix code's two
undefined shifts emulated as x86 executes them. The pre-fix forms fail 13 of 35 checks (0 threads selected
on either non-SMT part, every E-core dropped on both hybrid parts, `groupCount` 2 on all four) and the
current forms fail none of 205, which include, for all four `U` policies on every topology: one thread per
selected core, no two threads sharing a core, no selected core left without one, and every thread on a core
of its own class. The uniform SMT topologies pass both, which is what the entries predicted — this is a
class of defect that never appears on the machines most likely to be used for testing.

**G5 came with them**, because without it the core map G2 now builds correctly is still pinned onto too few
cores. It is not the one-line hoist D5's note assumed: the affinity walk now consults the selected cores of
the thread's *own* class, `cfg.sys.coreMap[j] & cfg.coreMap`, which both makes the assignment one-to-one and
keeps `packetSizeRAM` and `resArray.records[j]` describing the core the thread is on. Extending the harness
to the spawn loop measured what the entry's "masked on a uniform SMT CPU" qualifier was hiding: on a hybrid
part **every E-core in the machine went untested at every setting**, `U` argument or not — 20 threads pinned
onto 12 distinct cores on 6 P + 8 E, 32 onto 16 on 8 P + 16 E — because `coreType` puts the E-cores in the
non-SMT class and the P-core threads then restarted over them. Post-fix both cover every core.

**G3 and G9 remain open**, and G9 is worth reading alongside G2 and G5: the "non-SMT / SMT" split these
fixes now handle correctly is still *called* something it does not mean on a hybrid part, and that
misnomer is why G5's severity was mis-scoped when it was first written down.

Two stale cross-references were corrected while the section was open: C7 and C11 both cite "the
64-virtual-core ceiling (G2)", which is **G3**.

Three files moved again. `CPU.cpp` gained 60 — 797 now 857 — 24 of them in the topology walk at `:30`, 24
more between the walk and `cfg.coreMap`'s assembly at `:96`, and twelve at the spawn loop (now `:746`), so a
citation into `wmain` reads 24 lines low above the `groupCount` derivation, 48 low from there to the spawn
loop and 60 low below it, on top of every earlier shift; **every C, E and F entry in `wmain` is below the
first two**. `CPU.h` gained 35 — 625 now 660 — six at `:33` for the sibling bitmaps
and 29 at `SetSMTLoading`, so a citation reads 6 low from `:33` to `:512` and 35 low below that:
`Failed()`, `Evaluate` and `GenerateValues`, and so A7, B4, B5, B7 and B8. `en-GB.h` gained three, one exit
code in the instruction table and two messages at the end of the array. `CLAUDE.md` was updated to match —
its exit-code and thread-lifecycle sections describe the enumeration's four rules, the sibling bitmaps and
the class-map affinity walk, and the known-constraints bullet that named G5 is gone, leaving one that names
G9 — the misnomer that outlived it.

Every other entry below still stands.

---

## A. Verdict integrity — the tool's core claim

### A1. `_mm256_testc_pd` compares only sign bits, so AVX2 verification never fails — **Critical** — *Fixed*

> **Fixed.** All ten call sites now use the `ResultsMatch(cfl64x4, cfl64x4)` overload added at the top of
> `CPU_job_cycles.h`, which XORs the two vectors and tests the difference with `_mm256_testz_si256`.

`CPU_job_cycles.h:134` (and `:148,153,158,163` in `JobCycleMemAVX2`, `:360` in `JobCycleALU_AVX2`,
`:378,383,388,393` in `JobCycleMemALU_AVX2`):

```cpp
if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
```

`VTESTPD` — what `_mm256_testc_pd` emits — does not compare its operands. It computes `~a & b` and returns
`CF = 1` if **the sign bit of every 64-bit lane** of that intermediate is zero. In other words it returns 1
unless some lane has `sign(expected) == 1` while `sign(computed) == 0`. The 52-bit mantissa and the 11-bit
exponent are never examined.

Every value these kernels produce is strictly positive: each step is `sqrt(k) / (abs(1 - …) + positive)`, and
the trailing `x = x * (x*1.0101… + 0.00021)` preserves the sign. The expected values therefore always have
sign bit 0, the condition is never satisfied, `CF` is always 1, and the `if` body is unreachable.

* **Trigger:** any run selecting AVX2 — `PITC.exe Iv`, `Iav`, `B` on a non-AVX-512 CPU, or any `-1`…`-9`
  preset on a non-AVX-512 CPU (`CPU.cpp:99,372` select `0x089`/`0x09` = ALU + AVX2 there).
* **Impact:** the AVX2 integrity check is dead code. Combined with A3 (the summary reports `.Pass.` whenever
  `value[3]` was never written), an AVX2 run prints `.Pass.` for every thread regardless of the hardware's
  behaviour. The tool's headline feature is silently not working.
* **Fix:** compare bit patterns, e.g.
  `_mm256_testz_si256(d, d)` on `d = _mm256_xor_si256((__m256i&)a, (__m256i&)b)`, or
  `_mm256_movemask_pd(_mm256_cmp_pd(a, b, _CMP_NEQ_UQ))`. `common functions.h:162` already provides a correct
  `AllTrue(cui256, cui256)` built on `_mm256_testc_si256` — the `_pd` spelling here looks like a slip.

### A2. `_mm_testc_si128` is a subset test, not equality — SSE misses every 0→1 bit flip — **Critical** — *Fixed*

> **Fixed.** All ten call sites now use the `ResultsMatch(cfl64x2, cfl64x2)` overload added at the top of
> `CPU_job_cycles.h`, which XORs the two vectors and tests the difference with `_mm_testz_si128`.

`CPU_job_cycles.h:94` (and `:108,113,118,123,290,308,313,318,323`):

```cpp
if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
```

`PTEST` sets `CF = ((~a & b) == 0)`, i.e. "every bit set in `b` is also set in `a`". This detects a bit that
*should* be 1 becoming 0, but a bit that should be 0 becoming 1 passes silently — roughly half of all
single-bit corruptions in the 128-bit SSE result are invisible.

* **Trigger:** any run selecting SSE — `Is`, `Ias`, preset `-0`, or `B` on an SSE-only CPU.
* **Impact:** a genuine class of silicon faults produces `.Pass.`.
* **Fix:** `_mm_testz_si128(d, d)` on `d = _mm_xor_si128(a, b)`.

### A3. The results table can never report an AVX-512 failure — **Critical** — *Fixed*

> **Fixed.** `case 10:` is now `case 16:` (`CPU.cpp:642`), so AVX-512 rows reach their own arm, print all
> eight lanes and are graded by `Evaluate(i, 0)`. This also squares the row width with the header rulers.

`CPU.cpp:630–646`:

```cpp
switch(cui8 bit = threadData[i].procUnits & mask) {
default:  … value[2][i].raw[16 - bit] … wstrPass[Evaluate(i, 5 - bit)]
case 4:   … case 8:   … case 10:  …
```

`mask` is `1 << j` for `j` in `0…4`, so `bit` can only be 1, 2, 4, 8 or 16. **`case 10:` is unreachable** — it
should be `case 16:`. AVX-512 therefore falls into `default`, which prints a single lane
(`raw[16 - 16]` = `raw[0]`) and evaluates `Evaluate(i, 5 - 16)` = `Evaluate(i, -11)`.

In `Evaluate` (`CPU.h:213–223`) an out-of-domain unit index produces `index = ui8(16 - (1 << 15)) = 16` and
`end = ui8((1 << 14) + 16) = 16`, so the comparison loop body never executes and the function returns 0 —
`.Pass.`, unconditionally.

The header rulers at `CPU.cpp:623,625,627` *do* carry a correct `case 16:`, so the table is drawn 128 hex
digits wide while the rows print 16, leaving the output visibly misaligned as well.

* **Trigger:** any AVX-512 run (`Ix`, `Iax`, `B` or any preset on an AVX-512 CPU).
* **Impact:** the end-of-run summary is blind to AVX-512 failures. Failures *are* still caught in-flight by
  `JobCycleAVX512` → `Failed()`, which prints to the console — but any user reading only the summary table
  (or the `O[…]` results file, which contains only the table) sees an unqualified pass.
* **Fix:** change `case 10:` to `case 16:`.

### A4. Two stacked `S` flags make every thread idle for the whole run and report `.Pass.` — **Critical** — *Fixed*

> **Fixed** in two places. `wmain` (`CPU.cpp:455–462`) now rejects a `procSync` carrying more than one of
> bits 0–2 with `-12`, and substitutes Parallel when it carries none, so `S`, `St` and `Sq` stay usable while
> `Spr`, `Sps`, `Srs` and `Sprs` are refused before any thread is created. `ComputationPulse` was also
> restructured so that the fall-through `case 2` became a `default`, and the `nextTic += startTics` that
> converts the duration into an absolute timestamp moved *out* of the `switch` — no value of `procSync` can
> now leave `nextTic` holding a bare duration. The round-robin, parallel and staggered arms are otherwise
> unchanged (E3's double-counted stagger offset is preserved verbatim, and is still open).

`CPU_methods.h:36–50`:

```cpp
switch(tcfg->procSync & 0x07) {
case 1: … // Round-robin (falls through)
case 2: nextTic += startTics; break;
case 4: … // Staggered
}
```

The mask yields 0–7 but only 1, 2 and 4 have labels and there is no `default`. For the other five values
`nextTic` keeps the value assigned at `CPU_methods.h:28` — `tcfg->activeTics`, a small *duration* in tics
(e.g. 2 500 000) rather than an absolute timestamp. `timer.siCurrentTics` is a QPC reading since boot, of
order 10^13, so `timer.siCurrentTics < nextTic` at `CPU_methods.h:66` is false on the first iteration and
every subsequent one. The thread takes the `Sleep` branch forever and performs no work after the mandatory
first cycle.

Reachable masked values with no case: 0 (`S` with no letters), 3 (`Spr`), 5 (`Srs`), 6 (`Sps`), 7 (`Sprs`).

* **Trigger:** `PITC.exe Spr Tf[250]1750`, `PITC.exe Sps …`, `PITC.exe S …`. The help text at `en-GB.h:37`
  explicitly advertises stacking — *"The first three options (P,R,S) can be stacked"*.
* **Impact:** the run idles for its full duration and then prints `.Pass.` for every thread, exit code 0. A
  user who stacks flags as documented gets a fabricated pass.
* **Fix:** add a `default:` that errors out, and validate at parse time that at most one of P/R/S is set.

### A5. A zero on-time produces a run that does no work and passes — **Critical** — *Fixed*

> **Fixed.** `CPU.cpp:460` rejects a zero `onTime` with `-14` whenever the run is not in constant mode, which
> covers the fixed-pulse path described below and the sweeping path, where a zero `onTime` collapses
> `cycleTics` to zero instead (`offTime` having been zeroed by the `S` sub-option). Constant runs never read
> `activeTics`, so `Tc T[0` is still accepted. An `onTime` of at least 1 ms also guarantees `activeTics` and
> `cycleTics` are non-zero, which removes E9's zero-length cycle (`Tf[0]0` being one of the rejected forms).

`CPU.cpp:240` accepts any `T[` value without validation, and `CPU.cpp:598` turns it into
`activeTics = timer.siFrequency * cfg.onTime / 1000`. With `onTime == 0`, `nextTic` equals `startTics` exactly
(`CPU_methods.h:28,42`); the loop takes the sleep branch, advances `nextTic` by a full `cycleTics`, and the
sleep itself consumes that same `cycleTics` of wall time — so `timer.siCurrentTics < nextTic` is never true
again and the compute branch is never entered.

* **Trigger:** `PITC.exe Tf[0]1000`, or `T[` with a missing/non-numeric value (`wcstol` returns 0 — see F4).
* **Impact:** a full-duration run that executes one job cycle and then idles, reporting `.Pass.` for every
  thread with exit code 0.

### A6. A zero or negative test duration reports `.Pass.` immediately — **Critical** — *Fixed*

> **Fixed.** `CPU.cpp:459` rejects `cfg.tics <= 0` with `-13`, after the whole command line has been parsed
> so that a later preset can still legitimately overwrite an earlier `Tt`. This also removes the sweep-ramp
> divide-by-zero half of E4 (`Tst0`), since `endTics - startTics` is now always positive. The KUPS half of E4
> (`B Tt0.5`, where `cfg.tics / timer.siFrequency` truncates to zero) is untouched and still open.

`CPU.cpp:267` — `cfg.tics = si64((fl64)timer.siFrequency * wcstod(&argv[i][j + 1], &stopChar));` — accepts 0
and negative values. `endTics = startTics + cfg.tics` (`CPU.cpp:596`), so the pulse loop's condition
`timer.siCurrentTics < tcfg->endTics` is false on entry and every thread exits after the single mandatory
job cycle at `CPU_methods.h:61`.

* **Trigger:** `PITC.exe Tt0`, `PITC.exe Tt-5`, `PITC.exe Tt` (missing value).
* **Impact:** the program prints a full `.Pass.` table and returns 0 having tested essentially nothing. A
  typo in a script becomes a clean bill of health.
* **Fix:** reject durations ≤ 0 (and see E4 — `Tt0` also divides by zero in sweep mode).

### A7. `Failed()` clears eight threads' completion bits, not one — **High** — *Fixed*

> **Fixed.** `Failed()` now clears only the failing thread's bit. `threadBit` did not have to be threaded
> through the sixty-odd call sites in `CPU_job_cycles.h`: `coreNum` is `(threadByte << 3) + threadBit` by
> construction in `ComputationPulse`, so the bit within the byte `threadByte` addresses is `coreNum & 0x07`,
> and the mask is built from that. The clear is now idempotent with the one `ComputationPulse` performs on
> its way out, and a failure on one thread no longer releases `wmain` for the other seven in its byte.
> The residual — `Failed()` clearing the *failing* thread's own bit before that thread writes
> `resArray.iter[coreNum]` — is **D8**, which is untouched.

`CPU.h:206`:

```cpp
_InterlockedAnd8(threadByte, 0x0);
```

`threadByte` addresses the byte holding eight threads' bits. Clearing the whole byte tells `wmain` that all
eight have finished. Contrast the correct form at `CPU_methods.h:76`:
`_InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));`

* **Trigger:** any single computational failure on a machine with more than one thread.
* **Impact:** `while(ThreadsRunning())` at `CPU.cpp:614` can return while up to seven threads are still
  writing `value[3][i]` and `resArray.iter[i]`. The summary table and the KUPS score are then read from data
  being concurrently mutated, and the process exits mid-test. Precisely when the tool has found something,
  its report becomes unreliable.
* **Fix:** clear only the failing thread's bit; pass `threadBit` through to `Failed()`.

### A8. `Failed()` prints the wrong value plane, and prints raw pointers in memory-backed mode — **High**

`CPU.h:181–208`. Cases 0–3 (AVX-512, AVX2, SSE, FPU) print `value[1][coreNum]` as the observed value, while
case 4 (ALU) prints `value[3][coreNum].alu`. Every caller in `CPU_job_cycles.h` records the observed value
into `value[3]` before calling `Failed()`, so cases 0–3 should read `value[3]` too.

Worse, in memory-backed mode `value[1][k]` holds the arena pointers `p0`–`p4`, which the `RESULTS` union
overlays at byte offsets 0–39 — i.e. *inside* the `avx512` member (`CPU.h:81–100`). Cases 0 and 1 therefore
reinterpret pointer values as doubles and print nonsense.

* **Trigger:** any vector/FPU failure; the pointer-garbage variant needs any `M`/`B`/`-N` run.
* **Impact:** the error report — the only place an AVX-512 failure is ever surfaced (A3) — shows meaningless
  numbers, making a real fault impossible to characterise.

### A9. `procUnits & 0x1F` indexes a 24-entry table — **High** — *Fixed*

> **Fixed** in two places. `wmain` (`CPU.cpp:450–457`) now rejects a unit selection naming more than one of
> FPU/SSE4.1/AVX2/AVX-512 with `-16`, so `Ivx` and its relatives are refused before any thread is created.
> And `JobCycle` is now declared `[2][32]` (`CPU_job_cycles.h:521`), which makes the table total over the
> `procUnits & 0x1F` domain `ComputationPulse` indexes it with, so no future path that skips the validation
> can call through the bytes following the table. Entries 24–31 follow the same "only the ALU bit and the
> widest selected unit matter" rule as the rest of the table and therefore mirror 16–23 — which is what the
> arena-sizing switch already assumed for those indices, so the two tables now agree on the valid domain.

`CPU_job_cycles.h:492` declares `JobCycle[2][24]`; `CPU_methods.h:24,61,67` index it with
`tcfg->procUnits & 0x01F`, domain 0–31. Selecting AVX2 *and* AVX-512 together yields index 24–31, reads past
the end of the table and calls through whatever bytes follow it.

* **Trigger:** `PITC.exe Ivx`, `Iavx`, `Ifvx`, … Every shipped preset and `B` set exactly one vector bit, so
  this is reachable only through a hand-built `I` string — but nothing rejects it.
* **Impact:** undefined behaviour; in practice a jump to an arbitrary address.
* **Fix:** reject `I` strings with more than one vector bit, or reduce the index to the widest selected unit
  before dispatch. Note the arena-sizing switch at `CPU.cpp:544–549` *does* handle 24–31, so the two tables
  disagree about the valid domain.

### A10. Selecting no processing unit silently runs the ALU kernel with an un-divided record count — **High** — *Fixed*

> **Fixed.** `CPU.cpp:456` rejects a `procUnits` with no processing bit set with `-15`, after the whole
> command line has been parsed and before the arena is sized or any thread is created. That closes all three
> consequences at once: no run can be silently promoted to an ALU test, `resArray.records[]` can no longer
> keep the block size in bytes, and the results table can no longer come out empty. The sizing `switch` also
> gained the `default:` arm asked for below (`CPU.cpp:542–545`), grouped with the two 8-byte-record cases
> because index 0 dispatches to `JobCycleMemALU`; it is unreachable while the validation stands, but an
> index that ever escapes it now still yields a record count rather than a byte count.

`I1`, `I2` or `I3` alone sets only the cache bits, so `procUnits & 0x1F == 0`.

* `CPU_job_cycles.h:493` maps index 0 to `JobCycleALU` / `JobCycleMemALU` — the run silently becomes an ALU
  test the user did not ask for.
* Far worse, the arena-sizing `switch(cfg.procUnits & 0x01F)` at `CPU.cpp:519–550` **has no `case 0` and no
  `default`**, so `resArray.records[]` keeps the value assigned at `CPU.cpp:515` — the block size in *bytes*,
  not in records. `JobCycleMemALU` then walks `p4[offset … offset+3]` over a range eight times larger than
  the slice.

* Finally, the results loop at `CPU.cpp:618–620` iterates `j` over 0…4 and skips every unit whose bit is
  clear, so the run finishes with a completely empty results table — no rows, no verdict, exit code 0.

* **Trigger:** `PITC.exe I3 Mc8 Tct10`.
* **Impact:** heap corruption across the whole arena; with `Mc8` each thread writes 64 MB into an 8 MB slice,
  and the run then reports nothing at all.
* **Fix:** reject a `procUnits` with no processing bit set, and add a `default:` to the sizing switch.

### A11. AVX-512 uses floating-point comparison semantics while every other unit compares bits — **Medium**

`CPU_job_cycles.h:174,188,193,198,203,430,448,…`:

```cpp
if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512))
```

This is an FP predicate, not a bit comparison. `+0.0` and `-0.0` compare equal, so a sign-bit corruption in a
zero lane is missed, and if a fault produced NaN in both planes the comparison would report a difference. The
ALU and FPU paths use `!=` on raw scalars and the SSE/AVX2 paths use integer `PTEST`, so the four units do not
agree on what "identical" means. For a bit-exactness tester the comparison should be bitwise everywhere.

### A12. `JobCycleMemFPU` reports FPU failures using the ALU format — **Medium**

`CPU_job_cycles.h:70,75,80,85` pass unit `4` (ALU) to `Failed()` from inside the FPU function:

```cpp
value[3][coreNum].fpu = value[1][coreNum].p3[offset];
Failed(coreNum, threadByte, 4);
```

`Failed`'s `case 4` prints `value[2][coreNum].alu` / `value[3][coreNum].alu` with `%lld`, so an FPU mismatch
prints two unrelated ALU integers. The sibling `JobCycleMemALU_FPU` uses `3` correctly (`:240,245,250,255`).

* **Fix:** change the four `4`s to `3`.

### A13. Cache-targeting options are accepted, displayed, and do nothing — **Medium**

`CPU.cpp:106–114` parses `I1`/`I2`/`I3` into bits 5–7 and `CPU.cpp:572` prints `CL1`/`CL2`/`CL3` in the
configuration banner, but no code reads those bits; `cfg.sys.cache[]` and `cfg.sys.cacheL3`, populated at
`CPU.cpp:47–63`, are never used. This is disclosed in the help text and README
(`!!! CACHE USE NOT YET IMPLEMENTED !!!`), so it is a known gap rather than a surprise — but the banner still
reports a configuration the run does not implement, which is misleading in a saved results file. Consider
suppressing the `CLn` tokens until the feature exists.

---

## B. Golden-value generation (`W`)

### B1. `W` silently writes a corrupt `cpu.values` on most real CPUs — **Critical** — *Fixed*

> **Fixed.** Thread *t* now owns `[t * entries[0], (t + 1) * entries[0])` and the final thread absorbs the
> remainder (`CPU.h:233–236`), so the ranges tile `[0, MAX_THREADS)` exactly: every entry is generated by
> one thread, and no entry is generated by two. The remainder is no longer used as thread 0's starting
> offset.
>
> One detail below was optimistic: the entries two threads shared were **not** "benign in value". Each
> thread copies `value[3][coreNum]` into `value[2][coreNum]` and then transforms `value[3][coreNum]` in
> place, so whichever thread arrives second transforms an already-transformed entry and re-seeds `value[2]`
> from it. The shared entries came out doubly computed, and on some core counts the self-check saw the
> divergence and returned `-4` rather than writing the file at all.

`CPU.h:230–236`:

```cpp
cui32 entries[2] = { MAX_THREADS / (cui32)cfg.sys.vCoreCount, MAX_THREADS % (cui32)cfg.sys.vCoreCount };
si16  coreNum   = ((cui64(tcfg->threadByte) << 3) + tcfg->threadBit);
      coreNum   = coreNum * entries[0] + (coreNum ? 0 : entries[1]);
csi16 range     = coreNum + entries[0];
```

The remainder `entries[1]` is used to *offset thread 0's start* instead of extending the last thread's range.
With `vCoreCount = 12` (a Ryzen 5 5600X): `entries = {42, 8}`; thread 0 covers `[8, 50)`, thread 1 covers
`[42, 84)`, … thread 11 covers `[462, 504)`.

* Indices `[0, 8)` and `[504, 512)` are **never computed**. `value[3]` for those entries still holds the seed
  copied at `CPU.cpp:333`, so the file's "expected output" block contains the *inputs* for those threads.
* Indices `[42, 50)`, `[84, 92)`, … are computed concurrently by two threads — benign in value, since the
  computation is deterministic, but a formal data race.
* The self-validation loop only inspects indices inside a thread's own range, so the corruption is never
  detected and `W` returns 1 ("success").

* **Trigger:** any machine whose virtual-core count does not divide 512 — 6, 12, 20, 24, 28, 40, 48, 56, 96 …
  i.e. most consumer CPUs (Ryzen 5 5600X = 12, Core i7-13700K = 24, Ryzen 9 7900X = 24).
* **Impact:** every subsequent test run compares thread 0…7's real output against a stored *seed* and reports
  `!Fail!` on healthy silicon. The tool is unusable on those machines and, worse, accuses the CPU.
* **Fix:** give thread `t` the range `[t*entries[0], (t+1)*entries[0])` and add the remainder to the last
  thread.

### B2. `W` can never succeed on a CPU without AVX2 — **Critical** — *Fixed*

> **Fixed.** Both SSE ladders are now `_fl64[0], [2], [4], [6], [8], [10]` (`CPU.h:251–256` and `:272–277`),
> which covers lanes 0–11 once each; the `JobSSE(value[…].sse)` following the `if` chain covers 12–13,
> `JobFPU` covers 14 and `JobALU` 15. The ladder's duplicate `_fl64[12]` — the pass that made lanes 12–13
> doubled — is gone, and so is the validation ladder's repeated `_fl64[8]`. Generation and validation are
> now identical, so `Evaluate(coreNum, -1)` no longer reports a mismatch on healthy silicon and `W` can
> complete on an SSE-only CPU. The three ladders were confirmed to agree lane for lane: `JobSSE`, `JobAVX2`
> and `JobAVX512` compute the same function element-wise, so one pass per lane is what makes the golden
> block independent of which ladder built it.

`CPU.h:249–254` (generation ladder) versus `CPU.h:270–275` (validation ladder):

```cpp
// generation                              // validation
JobSSE((fl64x2&)value[3][…]._fl64[0]);      JobSSE((fl64x2&)value[2][…]._fl64[0]);
JobSSE((fl64x2&)value[3][…]._fl64[2]);      JobSSE((fl64x2&)value[2][…]._fl64[2]);
JobSSE((fl64x2&)value[3][…]._fl64[4]);      JobSSE((fl64x2&)value[2][…]._fl64[4]);
JobSSE((fl64x2&)value[3][…]._fl64[8]);      JobSSE((fl64x2&)value[2][…]._fl64[8]);
JobSSE((fl64x2&)value[3][…]._fl64[10]);     JobSSE((fl64x2&)value[2][…]._fl64[8]);   // ← duplicate
JobSSE((fl64x2&)value[3][…]._fl64[12]);     JobSSE((fl64x2&)value[2][…]._fl64[10]);
```

Against the `RESULTS` lane layout (`raw[0..7]` AVX-512, `raw[8..11]` AVX2, `raw[12..13]` SSE) the two ladders
disagree, and both are wrong:

| Lanes | Generation | Validation | Correct |
|-------|-----------|-----------|---------|
| 0–5 | 1× | 1× | 1× |
| **6–7** | **0×** | **0×** | 1× |
| **8–9** | 1× | **2×** | 1× |
| 10–11 | 1× | 1× | 1× |
| **12–13** | **2×** (here + line 256) | 1× (line 277) | 1× |

* Lanes 6–7 are never processed, so the AVX-512 half of the golden block keeps two raw seed lanes.
* Lanes 8–9 and 12–13 receive different pass counts in the two ladders, so `Evaluate(coreNum, -1)` at
  `CPU.h:281` **always** reports a mismatch, the sentinel at `CPU.h:283–284` is written, and `wmain`
  (`CPU.cpp:345`) returns `-4` — *"Computational errors detected while generating correct values"*.

* **Trigger:** `PITC.exe W` on any CPU without AVX2 (pre-Haswell Intel, pre-Excavator AMD, or a VM masking
  AVX2).
* **Impact:** `cpu.values` can never be created on such a machine, so the entire program is unusable there —
  and the error message blames the CPU.
* **Fix:** make both ladders `_fl64[0], [2], [4], [6], [8], [10]` and drop the redundant
  `JobSSE(value[…].sse)`, identically in both places.

### B3. `GenerateValues` clears the wrong bits when a thread finishes — **High** — *Fixed*

> **Fixed.** The value is inverted at its declaration — `cui8 threadMask = ui8(~(1u << tcfg->threadBit));`
> (`CPU.h:238`) — and `_InterlockedAnd8` at `CPU.h:297` now ANDs with that mask, matching the polarity
> `ComputationPulse` has always used (`CPU_methods.h:81`). A finishing thread clears its own bit and leaves
> the other seven alone, so `wmain` waits for every generating thread and neither writes an incomplete
> `cpu.values` nor hangs when a byte holds a single thread.

`CPU.h:236,295`:

```cpp
cui8 threadBit = 1u << tcfg->threadBit;
…
_InterlockedAnd8(&((chptr)threadBits)[tcfg->threadByte], threadBit);
```

`AND`ing with `threadBit` **keeps** the finishing thread's own bit and **clears the other seven**. The correct
polarity is used at `CPU_methods.h:76` (`~cui8(1u << tcfg->threadBit)`). Consequences depend on how many
threads share the byte:

* **≥ 2 threads in the byte:** thread A leaves `bitA`; thread B then `AND`s with `bitB`, and
  `bitA & bitB == 0`, so the byte clears as soon as the *second* thread finishes — `wmain` proceeds while up
  to six threads are still generating and writes an incomplete `cpu.values`.
* **Exactly 1 thread in the byte** (`vCoreCount % 8 == 1`, e.g. 9, 17, 25, 33 virtual cores): the bit is never
  cleared and `while(ThreadsRunning()) Sleep(100)` at `CPU.cpp:342` **hangs forever**.

* **Fix:** `_InterlockedAnd8(&((chptr)threadBits)[tcfg->threadByte], ~threadBit);`

### B4. The advertised 65 535-iteration self-check runs for only one index per thread — **Medium**

`CPU.h:235` declares `ui16 i = 0;` *outside* the per-core loop and `CPU.h:261` uses it as the validation
counter without resetting it. After the first core in a thread's range, `i == 65535` and the inner loop never
runs again.

* **Impact:** with 12 virtual cores each thread covers ~42 indices but validates one — roughly **2.4 % of the
  generated table is checked** rather than 100 %. The guarantee in the help text (*"File will only be created
  if the integrity of the results pass 65,536 iterations"*) is not delivered.
* **Fix:** declare `i` inside the outer loop.

### B5. `W` validates only 5 of the 16 job kernels — never the ones the test actually uses — **Medium** — *Fixed*

> **Fixed.** `ValidateKernelFamilies` (`CPU.h`) runs one seed through every kernel the CPU can execute and
> requires each memory-array and combined variant to reproduce its register-resident counterpart byte for
> byte. `W` calls it before it generates anything and refuses with `-22`, naming the kernel that
> disagreed, so a divergence is reported as the defect it is instead of being written into `cpu.values` as
> though it were correct. Each `JobMem*` kernel is handed four records carrying the same seed and all four
> must come back equal to the single register-resident result, which checks the record indexing as well as
> the arithmetic. The AVX2 and AVX-512 families are gated exactly as `RunGoldenLadder` gates them.
>
> The comparison is `memcmp`, not any arithmetic test, for the reason **A1**–**A3** and **A11** give: a
> golden value is a bit pattern, and every floating-point spelling of "equal" this codebase has reached for
> has at some point compared something other than every bit.
>
> Checked for vacuity rather than assumed to work: a divergence injected into each of the thirteen
> non-reference kernels in turn — separately in the ALU half and the vector half, and in records other than
> the first — is caught in all thirteen cases and named correctly. A divergence in one of the five
> reference kernels is caught too, as a disagreement reported against whichever variant it is compared
> with.
>
> This checks the eighteen kernels against *each other*; what pins them to the file is still
> `KernelFingerprint`. The two are complementary, and neither replaces the other.

`CPU.h:241–258` exercises `JobAVX512`, `JobAVX2`, `JobSSE`, `JobFPU` and `JobALU` — the register-resident,
single-unit kernels. It never runs any `JobMem*` variant and never runs any `Job ALU_*` variant, yet those are
what every memory-backed run (`B`, every preset, any `M`) actually dispatches through `JobCycle[1][…]`.

* **Impact:** the golden values are only ever proven self-consistent for the register path. A defect
  introduced into a `JobMem*` or `JobALU_*` kernel — a divergence from its register counterpart — would not be
  caught by `W` at all, and would surface as a fleet of `!Fail!` reports blamed on hardware. Given that these
  eight function families must stay arithmetically identical by hand (there is no shared implementation), this
  is exactly the invariant that most needs a check.

### B6. `cpu.values` is untagged and its reads and writes are unvalidated — **Medium** — *Fixed*

> **Fixed** with both halves this entry asks for, and one leak it does not mention.
>
> Every read and every write of the file now goes through `ReadBlock` / `WriteBlock` (`CPU.h`), which treat a
> short transfer as failure: the byte count is the only evidence either way, and it was being discarded on
> all four calls. Each failure path closes `outFile` before returning — the two *read* paths leaked it too,
> not just the writes.
>
> The file gained a 64-byte `VALUES_HEADER` ahead of the two blocks, holding a magic value (`"PITCVALS"`), a
> format version, the block geometry (`RESULTS_BUF_SIZE`, so a change to `MAX_THREADS` or to `RESULTS` is
> caught), a build ID, a fingerprint of the job kernels, and an FNV-1a hash of each block. `wmain` validates
> the header before reading the blocks it describes and the hashes immediately after, and `W` writes it
> ahead of them. A file that fails any of those checks exits `-21` with a message naming which check failed,
> instead of being loaded and turned into a fleet of `!Fail!` reports blamed on the silicon. `-20` covers a
> header that could not be written. Both are documented in `wstrInstructions_English`, and `wstrMessage_*`
> grew from 25 entries to 30.
>
> **Any existing `cpu.values` is rejected by this change and must be regenerated with `W`** — it has no
> header, so it does not carry the magic value.
>
> Two design points worth recording, because the obvious alternatives are worse:
>
> * The kernel identifier is computed, not declared. `KernelFingerprint` (`CPU.h`) runs the same ladder that
>   generates the file over a fixed seed and hashes the 16 lanes it produces, so it changes whenever the
>   arithmetic of any kernel that ladder walks changes. A hand-maintained revision constant would have been
>   simpler and would have relied on exactly the discipline `CLAUDE.md` says the format should enforce. The
>   ladder is `RunGoldenLadder`, shared with both halves of `GenerateValues`, so a fingerprint can never
>   describe a different function from the one that wrote the block. It covers the register-resident kernels
>   — which *are* what produces the file; the `JobMem*` and `Job ALU_*` variants are outside it, and remain
>   B5's subject.
> * The compiler version is deliberately *not* part of the build ID. Under the `/fp:strict` that H2 pins, a
>   toolset upgrade cannot change a result, and folding `_MSC_FULL_VER` in would force a regeneration after
>   every Visual Studio update. The floating-point model and record size are in it, because they can.
>
> The entry's remark about a corrupt file introducing a negative or NaN seed is covered by the block hashes
> rather than by a range check on the seeds: a hash mismatch rejects the file whatever the corruption did to
> it, whereas a range check would pass any corruption that happened to land inside the range. Nothing here
> defends the file against deliberate tampering, and it is not meant to.
>
> One residual, in exchange: the fingerprint is computed on the machine reading the file, so a CPU faulty
> enough to miscompute one ladder run is told to regenerate its `cpu.values` rather than being tested. That
> is a refusal to run rather than a false verdict, it is far less likely than an error during a full run,
> and `W` on such a machine fails with `-4` and says so.

`CPU.cpp:458,463` read two 64 KiB blocks and never check `bytesProc`:

```cpp
if(!ReadFile(outFile, value[0], RESULTS_BUF_SIZE, &bytesProc, 0)) { … return -2; }
```

`ReadFile` returns `TRUE` for a short read, so a truncated, empty or unrelated file passes both checks and
leaves `value[0]`/`value[2]` partly holding stale zeroes. The writes at `CPU.cpp:355,359` have the same problem
in reverse — a partial write is reported as success — and leak `outFile` on the failure paths.

The file also carries no magic number, version, kernel hash or build identifier, so nothing detects the far
more likely failure mode: a `cpu.values` produced by a different build or a different kernel revision.
`CLAUDE.md` warns that the file must be regenerated after any kernel edit — the format should enforce that
rather than rely on discipline. A corrupt file could also introduce a negative or NaN seed, which would drive
the FP chains permanently to NaN and be reported as a hardware failure.

* **Impact:** the most common user error (a stale or mismatched `cpu.values`) presents as every thread
  reporting `!Fail!`, which reads as a hardware fault.
* **Fix:** verify `bytesProc == RESULTS_BUF_SIZE` on both reads and both writes; prepend a header with a magic
  value, a format version, and a hash of the kernel constants and build settings.

### B7. A detected generation error can be overwritten and lost — **Medium**

`CPU.h:283–284` signals failure by writing sentinels into thread 0's slot:

```cpp
value[3][0].raw[0] = 0x05555555555555555;
value[2][0].raw[0] = 0x0AAAAAAAAAAAAAAAA;
```

Whichever thread owns index 0 later writes `value[3][0]` wholesale at `CPU.h:242–258`, overwriting `raw[0]`.
If the failing thread runs first, the sentinel is erased and `CPU.cpp:345` sees no error — a real computational
error is silently discarded and a corrupt `cpu.values` is written and reported as successful.

* **Fix:** use a dedicated `volatile` flag with an interlocked write rather than repurposing result storage.

### B8. Help text says 65 536 iterations; the code performs 65 535 — **Low**

`en-GB.h:50` / `README.md:41` versus `CPU.h:261` (`i < 65535`).

---

## C. Memory safety

### C1. Selecting more than one non-ALU unit corrupts the arena seeds — **Critical** — *Fixed*

> **Fixed.** `CPU.cpp:457` rejects a unit selection naming more than one of FPU/SSE4.1/AVX2/AVX-512 with
> `-16` — the same check that closes A9 — so at most one of `p0`–`p3` is ever seeded and the aliased
> sub-arrays can no longer overwrite one another. `Iaf` (ALU + FPU) stays legal because `p3` and `p4`
> address the arena in the same 8-byte units and `case 3` advances `resArray.alu` past the FPU sub-array;
> every shipped preset, `B` and the default configuration name exactly one non-ALU unit and are unaffected.
>
> The other candidate fix — seeding only the sub-array the dispatcher will use — was rejected. It leaves the
> configuration banner and the results table reporting units the dispatcher never runs, and an un-run unit's
> row is graded against `value[3]`, which is still the untouched copy of `value[2]` made at `CPU.cpp:487`.
> That trades a false failure for a false `.Pass.` on silicon that was never exercised, which is the worse
> of the two defects for an integrity tester. The same reasoning rules out normalising `procUnits` down to
> the ALU bit and the widest selected unit.

`CPU.cpp:554–565` hands every thread five pointers derived from the *same* arena base with *different*
element strides, then seeds each sub-array whose bit is set:

```cpp
value[1][k].p1 = &resArray.avx[bos];   // fl64x4* → byte offset bos*32
value[1][k].p2 = &resArray.sse[bos];   // fl64x2* → byte offset bos*16
…
if(cfg.procUnits & 0x08) value[1][k].p1[os] = value[0][k].avx;
if(cfg.procUnits & 0x04) value[1][k].p2[os] = value[0][k].sse;
```

`resArray.avx`, `resArray.sse`, `resArray.fpu` and `resArray.avx512` are all the same address
(`CPU.cpp:517–518`), so `p1`'s region `[bos*32, (bos+records)*32)` and `p2`'s region
`[bos*16, (bos+records)*16)` overlap. Dispatch, however, follows the documented rule that "only the ALU bit
and the widest enabled vector unit matter" (`CPU_job_cycles.h:492`) — index 13 (`ALU|SSE|AVX2`) runs
`JobCycleMemALU_AVX2`, which reads `p1`. The SSE seeding has already overwritten the first half of `p1`'s
region with SSE seed values.

* **Trigger:** any memory-backed run whose `I` string names two or more of FPU/SSE/AVX2/AVX-512 —
  `PITC.exe Iasv Mc8`, `Iafs Mc8`, `Ifsv Mc8`, …
* **Impact:** every thread starts the widest unit's records from the wrong seed and reports `!Fail!` on
  healthy hardware — a false accusation against the CPU, from a command line the parser accepts without
  complaint.
* **Fix:** seed only the sub-array the dispatcher will actually use, or reject multi-vector `I` strings (see
  also A9).

### C2. The ALU sub-array offset drops the element-size factor on the non-SMT term — **High** — *Fixed*

> **Fixed.** None of the four arms that placed the ALU sub-array computes the offset itself now. The `switch`
> yields only two numbers — `recSize`, the per-record byte cost, and `vecUnits`, the vector portion of a
> record in the 8-byte units `resArray.alu` is indexed by — and one expression after it advances the base:
> `resArray.alu = (si64ptrc)resArray.p + vecRecords * vecUnits`, where `vecRecords` is
> `records[0] * threadCount[0] + records[1] * threadCount[1]`. The multiplier now applies to the whole count,
> as this entry asks, and it cannot be dropped from one term again because no term is written out separately.
> `case 3` (ALU + FPU, N = 1) is unchanged in effect.
>
> **The trigger is wider than stated below.** The multiplier was missing from the *non-SMT* term, so the
> defect fires whenever `threadCount[0] > 0` — it does not need both classes populated. A CPU with no SMT at
> all, or one with SMT disabled in firmware, puts **every** thread in class 0 and is therefore the *worst*
> case, not a safe one: with `Iax Mc8` on 16 non-SMT threads the ALU base landed at 128 × `records` bytes
> where the vector sub-array alone needs 1 024 × `records`, so the ALU sub-array sat almost entirely inside
> it. Only a uniform-SMT topology (`threadCount[0] == 0`), which is what the author's own machine reports,
> masks it — which is presumably why it survived.
>
> Verified by compiling this block verbatim over a shimmed Win32 layer and checking the layout it produces:
> 31 hand-written configurations covering the shipped command lines, plus a sweep of 1 341 combinations of
> the 9 reachable unit sets, 12 topologies (uniform SMT, uniform non-SMT and ten mixes) and the three `M`
> modes at several sizes. The pre-fix code fails 23 of the 31 and 856 of the 1 341 — 777 hand two threads
> overlapping slices (C2 and C3) and 347 produce a record count the cursor walks past the end of (C4), the
> two sets overlapping — including 79 uniform-SMT configurations, so even the topology that masks C2 is not
> safe once C4 is counted. The fixed code passes all 31 and all 1 341: no slice leaves the allocation, no
> two slices overlap, every record count is a usable multiple of 4, and every thread's seed values are still
> intact after the seeding pass — that read-back being exactly what the false `!Fail!` came from.

`CPU.cpp:535,542,549`:

```cpp
case 5: case 7:   // ALU + SSE
   resArray.alu = (si64ptrc)resArray.p + (resArray.records[0] * threadCount[0] + resArray.records[1] * threadCount[1] * 2);
case 9: … 15:     // ALU + AVX2   … * 4
case 17: … 31:    // ALU + AVX-512 … * 8
```

`resArray.alu` is an `si64ptr`, so the offset must be in 8-byte units. The vector sub-array occupies
`records × N` such units per thread (N = 2 for SSE, 4 for AVX2, 8 for AVX-512), but the multiplier is applied
only to the SMT term. The correct expression is
`(records[0] * threadCount[0] + records[1] * threadCount[1]) * N`. `case 3` (ALU + FPU) is correct because
N = 1 there.

* **Trigger:** any `ALU + vector` memory-backed run on a topology with **both** non-SMT and SMT physical cores
  — hybrid P/E-core CPUs, or any machine where `SetSMTLoading` leaves both classes populated. On a uniform SMT
  CPU `threadCount[0]` is 0 and the bug is masked.
* **Impact:** the ALU sub-array base lands inside the vector sub-array; the two overlap and corrupt each
  other, producing `!Fail!` on healthy hardware.

### C3. Arena slices for non-SMT and SMT threads overlap — **High** — *Fixed*

> **Fixed** by the first of the two remedies below: `bos` is initialised once, in the outer loop's init
> clause, and runs across both passes — `for(k = 0, m = 0, bos = 0; m < 2; ++m)` with the inner loop reduced
> to `for(l = 0; l < threadCount[m]; ++k, ++l, bos += resArray.records[m])`. The two remedies this entry
> offers are the same thing — after the non-SMT pass a running `bos` holds exactly
> `records[0] * threadCount[0]` — and hoisting was chosen because it needs no second expression to keep in
> step with the loop. With C2 fixed as well, the arena tiles exactly: for every unit set, both sub-arrays and
> both thread classes, the slices partition the block with no gap and no overlap.

`CPU.cpp:552–553`:

```cpp
for(k = 0, m = 0; m < 2; ++m)
   for(l = 0, bos = 0; l < threadCount[m]; ++k, ++l, bos += resArray.records[m]) {
```

`bos` — the running per-thread record offset into the arena — is reset to 0 when `m` advances from the non-SMT
pass to the SMT pass, while `k` (the thread index) keeps counting. Every SMT thread is handed a slice starting
at an offset already given to a non-SMT thread.

* **Trigger:** any memory-backed run where `threadCount[0] > 0` **and** `threadCount[1] > 0`.
* **Impact:** pairs of threads write the same arena bytes concurrently, each observes the other's writes, and
  both report `!Fail!` — a false positive that looks exactly like a real memory-subsystem fault.
* **Fix:** hoist `bos` out of the inner loop, or seed the SMT pass with `records[0] * threadCount[0]`.

### C4. `records[]` is never rounded to a multiple of 4, so the kernels write past the slice — **High** — *Fixed*

> **Fixed** exactly as recommended: `resArray.records[m] = (resArray.blockSize[m] / recSize) & ~0x03ull`,
> and a resulting count of 0 is rejected — but only for a thread class that actually has threads, since
> `records[1]` is meaningless when `threadCount[1]` is 0. The rejection is a new exit code, `-18`, with a
> message naming both the bytes per thread available and the `4 × recSize` the selected units need.
>
> This is what the round-down is worth in practice. The default `Mc8` gives 116 508 records for ALU+AVX-512,
> which is already a multiple of 4 and walks exactly to the end of the slice; but it gives 209 715 for
> ALU+AVX2 and 349 525 for ALU+SSE, and preset `-0`'s `Mc2` gives 87 381 — so every memory-backed run on a
> CPU below AVX-512, `B` and presets `-1`–`-9` alike, walked 1 record (AVX2) or 3 records (SSE) past the end
> of every thread's slice on every pass, as did preset `-0` on every CPU. The overrun is
> `(4 - recCount mod 4) mod 4` records: the cursor's last value is the smallest multiple of 4 that is at
> least `recCount - 4`, which lands *beyond* `recCount - 4` whenever the count is not a multiple of 4. The
> degenerate `recCount < 4` case is now unreachable too: it is rejected rather than pinned at `i = 0` writing
> four records into a shorter slice.
>
> One reachable configuration changes verdict rather than behaviour. `Mn8` with no `Ms` on an all-SMT
> machine leaves `allocMem[1]` at its 1-byte default (C9), so the SMT threads get 0 records; that run used
> to allocate an arena of `1 × threadCount[1]` bytes and hand every thread the *register* code path with its
> pointers already overwritten, and now exits with `-18` instead. C9 itself — the 1-byte default and `Ms`
> alone allocating nothing — is untouched.

Every `JobCycleMem*` function touches `offset … offset+3` (`CPU_job_cycles.h:23–27`), and the cursor at
`CPU_methods.h:63` advances in steps of four:

```cpp
for(i = j = 0; …; i = (i >= recCount - 4 ? 0 : i + 4), ++j)
```

When `recCount` is not a multiple of 4 the cursor reaches `recCount - (recCount mod 4)` and the kernel writes
up to three records beyond the end of the thread's slice. `records` comes from plain integer division at
`CPU.cpp:521–549` with no rounding: 8 MiB / 24 = 349 525, not a multiple of 4. The degenerate cases are worse:
`recCount` of 1, 2 or 3 makes `i >= recCount - 4` true immediately, pinning `i` at 0 while the kernel still
writes four records.

* **Trigger:** any memory-backed run whose per-thread record count is not a multiple of 4 — the default `Mc8`
  with ALU + SSE, ALU + AVX2 or ALU + AVX-512, and therefore `B` and every preset.
* **Impact:** each thread overruns its slice by up to 3 records (24–192 bytes) into the next thread's data; on
  the last thread the overrun leaves the allocation entirely.
* **Fix:** `records[m] &= ~3ull;` after the division, and reject a resulting count of 0.

### C5. `malloc64` is never checked for failure — **High** — *Fixed*

> **Fixed** with both halves this entry asks for, sharing one exit code, `-17`, the way `-11` already serves
> three unsupported-unit messages. Before the allocation is attempted, a request that is zero or negative, or
> larger than `GlobalMemoryStatusEx`'s `ullAvailPhys`, is rejected; afterwards, `resArray.p` is tested
> against null before the pointer-seeding loop can dereference it. The size check is skipped if
> `GlobalMemoryStatusEx` itself fails, so an unavailable API cannot refuse a valid run.
>
> That covers the three unchecked inputs the entry lists. `Mc65536` (4 TiB on 64 threads) and `Mc-1` are both
> refused before `malloc64` is called — the negative value because `<<= 20` and the multiply by the thread
> count keep it negative, which the `<= 0` test catches. The `si64` overflow the entry raises is not
> reachable in the current parser: `wcstol` caps a 32-bit `long` at 2 147 483 647, so `<< 20` gives at most
> 2.25 × 10^15 bytes and ×512 threads at most 1.15 × 10^18, inside `si64`. It becomes reachable if `M` ever
> moves to `wcstoll`, and the checks above bound it either way. Verified by driving this block over a shimmed
> allocator: all three cases now return `-17`, where the pre-fix code was killed by a null-pointer write in
> the seeding loop in each of them.
>
> Only the arena is covered. `B`'s `resArray.iter = zalloc1d64(...)` (`CPU.cpp:93`) is a different allocator
> and a different (4 KiB) allocation; it is still unchecked and is still leaked on a repeated `B` (C13).

`CPU.cpp:517`:

```cpp
resArray.avx = (fl64x4ptrc)(resArray.avx512 = (fl64x8ptrc)(resArray.p = malloc64(cfg.allocMem[0])));
```

`malloc64` is `_aligned_malloc` (`memory management.h:46,214`), which returns `NULL` on failure. Nothing checks
it; the initialisation loop at `CPU.cpp:559–565` then dereferences the null-derived pointers. There is also no
sanity check on the requested size: `Mc65536` on a 64-thread machine asks for 4 TiB, `wcstol` happily accepts a
negative value (F4), and `blockSize[m] * threadCount[m]` at `CPU.cpp:503,508` can overflow `si64`.

* **Trigger:** `PITC.exe Mc65536`, `PITC.exe Mc-1`, or any `M` value exceeding available memory.
* **Impact:** null-pointer write, immediate crash with no diagnostic.
* **Fix:** check the result and return a documented error code; validate the request against
  `GlobalMemoryStatusEx` first.

### C6. A long `L` language code overflows `wstrLang[6]` — **High** — *Fixed*

> **Fixed** with the one-line clamp this entry gives:
> `lstrcpynW(wstrLang, &argv[i][1], min(c, si32(_countof(wstrLang))))`. A longer code is now truncated to the
> five characters the buffer holds rather than written past it — `Lde-DE-formal` leaves `de-DE` in
> `wstrLang`, `Lzh-Hans-CN-x-private` leaves `zh-Ha`. Both still resolve to English, because the option's
> comparison is inverted (F7) and English is the only table in the tree; truncation therefore changes no
> observable behaviour today, and the note in `CLAUDE.md` says to widen `wstrLang` if a language code ever
> needs more room. Verified by compiling the option's parse arm verbatim against a shim with `lstrcpynW`'s
> documented semantics and `wstrLang` as a real global: ASan reports a global-buffer-overflow on the pre-fix
> arm for both of those arguments and none on the fixed one.

`CPU.cpp:140–141`:

```cpp
for(c = 1; argv[i][c] && argv[i][c] != L' ' && c < 1024; ++c);
lstrcpynW(wstrLang, &argv[i][1], c);
```

`wstrLang` is `wchar wstrLang[6]` (`CPU.h:131`). `c` is the argument length, capped only at 1024, and
`lstrcpynW`'s third parameter is the *destination* buffer size in characters. `Len-GB` fits exactly (c = 6, 5
characters + NUL); anything longer overruns the global.

* **Trigger:** `PITC.exe Lde-DE-formal` writes 13 wide characters into a 6-wide-character global, past the end
  of `wstrLang` and into the adjacent `outUTF16header` / `outUTF8header` constants (`CPU.h:132–133`).
* **Impact:** global data corruption, possibly an access violation if those constants land in read-only
  storage. `/GS-` in `Release|x64` (`CPU.vcxproj:149`) removes the usual mitigation.
* **Fix:** `lstrcpynW(wstrLang, &argv[i][1], min(c, (int)_countof(wstrLang)));`

### C7. The results buffer and the `wcstombs` in-place conversion overflow at high thread counts — **Medium**

`wstrOutput` is 32 768 wide characters (`CPU.cpp:17`) and every `swprintf` into it (`CPU.cpp:571–655`) uses the
legacy two-argument form with no size limit. The conversion at `CPU.cpp:667` writes the narrow string *into
the same buffer, starting at wide-character index `c`*:

```cpp
wcstombs((chptr)&wstrOutput[c], wstrOutput, c);
```

That needs `2c + c ≤ 65 536` bytes, i.e. `c ≤ 21 845`. An ALU + AVX2 table costs roughly 60 characters per ALU
row plus 160 per AVX2 row, so `c` passes 21 845 at about 100 threads and 32 768 at about 150.
`wcstombs`'s return value (which can be `(size_t)-1`) is also unchecked.

* **Trigger:** currently unreachable because of the 64-virtual-core ceiling (G3), but `MAX_THREADS` is 512 and
  all buffers claim to support it. Lifting the core limit makes this immediately live.
* **Fix:** size `wstrOutput` from `threadCount[2]`, use the bounded `swprintf_s`, and convert into a separate
  allocation.

### C8. Division by zero when the core map selects nothing — **Medium**

`CPU.cpp:499` — `resArray.blockSize[0] = resArray.blockSize[1] = cfg.allocMem[0] / (cui64)threadCount[2];`.
`threadCount[2]` is computed at `CPU.cpp:473–476` by popcount over the user-modifiable core map, and nothing
guarantees it is non-zero. The same variable is used unchecked at `CPU.cpp:503,508`.

* **Trigger:** a `Uc`/`Ut` map that disables every core. (The entry also cited a `SetSMTLoading` mask that
  resolves to 0; that half is closed — the mask is now a bitmap of cores that exist, so it can only empty a
  map the `U` argument had already emptied.)
* **Impact:** integer divide-by-zero exception.

### C9. `allocMem[1]` defaults to 1 byte, and `Ms` alone allocates nothing — **Medium**

`CPU.h:42` initialises `si64 allocMem[2] = { 0, 1 };`. Two consequences:

* `Mn8` alone (`CPU.cpp:166–171`) sets `memConfig = 2` and `allocMem[0]` but leaves `allocMem[1]` at 1, so
  `blockSize[1] = 1` and `records[1]` divides down to 0. Those threads take `JobCycle[0][…]` — the *register*
  path (`CPU_methods.h:61,67`) — despite `value[1][k].p0…p4` having been overwritten with arena pointers. For
  AVX-512 the register kernel writes `value[1][coreNum].avx512`, which the union overlays on `p0`–`p4`,
  silently clobbering them.
* `Ms8` alone leaves `allocMem[0]` at 0, so `blockSize[0]` is 0 and the guard at `CPU.cpp:511`
  (`if(resArray.blockSize[0])`) skips the entire allocation — while the banner still reports memory in use.

* **Trigger:** `PITC.exe Mn8 Ua`, `PITC.exe Ms8 Ua`.
* **Impact:** the run silently differs from the reported configuration; some or all threads drop out of
  memory-backed mode.

### C10. `Uc` / `Ut` can shift by 64 or more — **Medium**

`CPU.cpp:288,291,311,314` — `0x03ull << (j << 1)` and `0x01ull << j`, with `j` walking the map characters
unbounded. `Uc` reaches an undefined shift count at the 32nd character and `Ut` at the 64th. See F1 for the
accompanying indexing errors.

### C11. The arena-slice loop uses `ui8` counters against `si16` thread counts — **Low**

`CPU.cpp:513,552–553` declare `ui8 k, l, m` and compare `l` against `threadCount[m]` (`si16`). Above 255
threads the counters wrap and the loop cannot terminate. Latent behind the 64-core ceiling (G3), but the
buffers are sized for 512.

### C12. Per-thread slices are not cache-line aligned — **Low**

`CPU.cpp:521–549` divides the block size by the record size with no rounding to 64 bytes, so adjacent threads
share a cache line at every slice boundary. For a tool whose memory mode exists specifically to exercise the
cache hierarchy, the resulting false sharing is a measurement artefact.

### C13. The arena is never freed — **Low**

`resArray.p` (`CPU.cpp:517`) and `resArray.iter` (`CPU.cpp:93`) have no matching `mfree`. Harmless at process
exit, but GCS rule p2 requires explicit alignment-aware allocators *with matching frees*. Repeating `B` on the
command line allocates `resArray.iter` again each time, leaking the earlier blocks.

---

## D. Concurrency

### D1. The main thread's non-atomic `|=` races with the workers' interlocked clears — **High** — *Fixed*

> **Fixed.** Both `|=` sites now call `SetThreadRunning` (`CPU.h`), which sets the bit with
> `_InterlockedOr8` on the same byte the workers clear with `_InterlockedAnd8` — the two are then atomic
> with respect to each other and no clear can be lost. The companion `ClearThreadRunning` is used by the
> thread-creation failure paths added for **D5**. Setting all bits before spawning was the other option the
> entry offers; the interlocked set was taken instead because it keeps the bit's lifetime beside the
> `_beginthreadex` call that owns it, and because **D5** needs an atomic *clear* from `wmain` regardless.

`CPU.cpp:607` (and `:336` in the `W` path):

```cpp
threadBits[d >> 6] |= 1uLL << (d & 0x03F);
```

A plain read-modify-write on a `vui64` word. Worker threads clear their bits in the *same words* with
`_InterlockedAnd8` (`CPU_methods.h:76`). `volatile` orders the accesses but does not make the RMW atomic.

* **Impact:** a worker's clear can be lost, leaving a bit set for a thread that has exited —
  `while(ThreadsRunning())` at `CPU.cpp:614` then hangs forever. With the default 2 s start-up delay the
  window is narrow, because no worker reaches its clear until long after the spawn loop finishes. But
  `Td0` removes the delay, and a short or zero-length test (`PITC.exe Td0 Tt0`, see **A6**) makes workers
  exit *while the spawn loop is still running* — at which point the lost update is not a rare race but the
  expected outcome, and the program hangs instead of printing its (already meaningless) pass.
* **Fix:** set all bits before spawning any thread, or use `_InterlockedOr8` on the byte.

### D2. Every worker thread mutates one shared `CLASS_TIMER` — **High** — *Fixed*

> **Fixed** by the entry's second suggestion. `CurrentTics()` (`CPU.h`) reads `QueryPerformanceCounter` into
> a local, and it is now the only clock a worker consults: `PulseWaitUntil` uses it in place of
> `timer.Update()`, and `ComputationPulse` keeps its own `curTics` instead of reading `timer.siCurrentTics`.
> No worker writes any field of the global timer. `timer.siFrequency` is still read from worker threads —
> that one is written once, by the `CLASS_TIMER` constructor, before `wmain` runs, so it is not a race.
> This also does what the entry's last paragraph says it could not: `wmain`'s single `timer.Update()` before
> the spawn loop is no longer overwritten by threads that have already started, so every thread's
> `startTics` is computed from an identical timestamp and the `T-Sync` shapes get the common pulse edge they
> promise. A per-thread `CLASS_TIMER` was rejected because nothing in the pulse loop reads the scaled,
> accumulated fields that make `Update()` expensive in the first place.

`CPU.h:117` declares a single global `timer`; `CPU_methods.h:56,64` call `timer.Update()` from every worker.
`CLASS_TIMER::Update` (`class_timers.h:55–63`) is a multi-field read-modify-write over `siPrevTics`,
`siTotalTics`, `siElapsedTics`, `dTotal`, `dScale` and `dGrandTotal` — an unsynchronised data race, formally
undefined behaviour, and in practice those fields hold meaningless values for the whole run.

Only `siCurrentTics` is consumed by the pulse loop, and being a naturally aligned 64-bit store it does not tear
on x64, which is why the tool appears to work. But each thread's notion of "now" is whatever another thread
last wrote, which is also why the per-thread `startTics` computed at `CPU.cpp:595` cannot deliver the
synchronised pulse edges the `T-Sync` modes promise.

* **Fix:** give each thread its own timer, or call `QueryPerformanceCounter` directly in the pulse loop.

### D3. `ThreadsRunningAVX` / `ThreadsRunningSSE` use the wrong stride *and* form misaligned vector references — **High** — *Fixed*

> **Fixed**, both defects and the sizing note. `ThreadsRunningAVX` now reads `threadBits[0]` and `[4]`, and
> `ThreadsRunningSSE` reads `[0]`, `[2]`, `[4]` and `[6]`, so all three polls examine the same 64 bytes —
> exactly `MAX_THREADS` bits, each once. Correcting the strides also removes every misaligned address on its
> own (0, 16, 32 and 48 are all 16- and 32-byte aligned against a 64-byte-aligned base), but the reference
> casts were replaced with `_mm_loadu_si128` / `_mm256_loadu_si256` regardless: an explicit unaligned load
> cannot be lowered to `movdqa`/`vmovdqa`, so no future change to the map's size or alignment can fault the
> poll, and on current hardware the unaligned form costs nothing at an address that happens to be aligned.
> `ThreadsRunningAVX512` is unchanged — it already viewed offset 0 only. The related sizing confusion is
> closed too: `threadBits` is declared with a new `MAX_THREADS_WORDS` (8) rather than `MAX_THREADS_BYTES`
> (64), because `declare1d64z`'s third parameter is an element count, so the buffer is now the 64 bytes the
> structure actually needs and its capacity agrees with `MAX_THREADS`.

`CPU.h:148–153`:

```cpp
inline bool ThreadsRunningAVX(void) {
   return !(AllFalse((si256&)threadBits[0], max256) && AllFalse((si256&)threadBits[2], max256));
}
inline bool ThreadsRunningSSE(void) {
   return !(AllFalse((ui128&)threadBits[0], max128) && AllFalse((ui128&)threadBits[1], max128) &&
            AllFalse((ui128&)threadBits[2], max128) && AllFalse((ui128&)threadBits[3], max128));
}
```

`threadBits` is an array of `ui64`, so a 32-byte `si256` view spans four elements and a 16-byte `ui128` view
spans two. Two independent defects follow:

1. **Coverage.** `ThreadsRunningAVX` reads bytes 0–31 and 16–47 — bytes 16–31 twice, and **bytes 48–63
   (threads 384–511) not at all**; it should use `threadBits[0]` and `threadBits[4]`. `ThreadsRunningSSE`
   reads bytes 0–15, 8–23, 16–31 and 24–39 — **bytes 40–63 (threads 320–511) not at all**; it should use
   `threadBits[0], [2], [4], [6]`.
2. **Alignment.** `threadBits` is 64-byte aligned, so `threadBits[1]` sits at +8, `threadBits[2]` at +16 and
   `threadBits[3]` at +24. Binding a `ui128&` (16-byte alignment) to +8 and +24, or a `si256&` (32-byte
   alignment) to +16, is undefined behaviour, and `AllFalse` takes its arguments **by value** — so the
   compiler must materialise a load from those addresses. If MSVC selects an aligned move (`movdqa` /
   `vmovdqa`), every non-AVX-512 machine faults on the very first poll.

* **Impact:** on any CPU without AVX-512 this is at best silently incomplete coverage and at worst an
  immediate general-protection fault in `wmain`'s wait loop. `ThreadsRunningAVX512` is unaffected — it only
  views offset 0, which is correctly aligned.
* **Related sizing confusion:** `CPU.h:122` declares the array as
  `declare1d64z(vui64, threadBits, MAX_THREADS_BYTES)`. `MAX_THREADS_BYTES` is a *byte* count (64), but the
  macro's third parameter is an *element* count, so the allocation is 64 × `ui64` = 512 bytes for a structure
  that needs 64. Harmless over-allocation, but it is the same units mix-up that produces the stride bug above,
  and it means the buffer's true capacity (4096 thread bits) silently disagrees with `MAX_THREADS`.

### D4. The SSE fallback poll requires SSE4.1 but is selected without checking for it — **Medium**

`CPU.cpp:79` binds `ThreadsRunning` to `ThreadsRunningSSE` whenever the CPU has neither AVX-512 nor AVX2, and
never consults `cfg.sys.cpuSSE4_1`. `ThreadsRunningSSE` calls `AllFalse(cui128, cui128)`, which is
`_mm_testz_si128` (`common functions.h:173`) — an **SSE4.1** instruction. A CPU with neither AVX2 nor SSE4.1
(Core 2, early Athlon 64 X2) executes an illegal instruction before any test begins, and before the friendly
"does not support the SSE4.1 instruction set" message at `CPU.cpp:449` can be reached.

### D5. The `_beginthread` handle is not valid to pass to `SetThreadAffinityMask` — **Medium** — *Fixed*

> **Fixed** exactly as the entry prescribes: `_beginthreadex`, checked for failure, created suspended,
> affinity applied, then resumed. Both spawn sites are converted — the test path and the `W` path — and both
> close the handle they now own. `ComputationPulse` and `GenerateValues` became `ui32 __stdcall` and end by
> returning rather than calling `_endthread`, which is what `_beginthreadex` requires. A creation or resume
> failure clears the thread's completion bit, reports it, and aborts with the new exit code `-19`; without
> that, a thread that never started left a bit no one would ever clear and `while(ThreadsRunning())` spun
> forever. `SetThreadAffinityMask`'s return value is now checked, and a failure prints a warning naming the
> thread rather than being discarded — the run continues, since an unpinned thread still verifies its own
> results, it simply cannot be attributed to a core.
>
> **This makes affinity effective for the first time**, which promotes **G5** from latent to live: `mask`
> restarts at bit 1 when the spawn loop advances to the SMT thread class, so a topology carrying *both*
> non-SMT and SMT threads now really does pin two threads to one virtual core while others idle. Uniform SMT
> CPUs (`threadCount[0] == 0`) are unaffected. **G5 should be taken next.** *(It was, with the topology
> batch — and it was live on every hybrid part rather than latent, because those carry both thread classes.)*

`CPU.cpp:609–612`:

```cpp
ptrc handle = (ptr)_beginthread(ComputationPulse, 0, &threadData[d]);
while(mask & ~cfg.coreMap[procGroup]) mask <<= 1;
SetThreadAffinityMask(handle, mask);
```

MSDN is explicit that the CRT closes the `_beginthread` handle when the thread exits, so it must not be
retained. `_beginthread` also returns `-1L` on failure, which is cast to a pointer and used unchecked, and
`SetThreadAffinityMask`'s return value is discarded. Affinity is additionally applied *after* the thread has
started running. The same unchecked `_beginthread` appears at `CPU.cpp:340` in the `W` path.

* **Impact:** affinity may be applied to an unrelated object or silently fail; a thread-creation failure goes
  unnoticed and its `threadBits` bit is never cleared, hanging `wmain` forever.
* **Fix:** use `_beginthreadex`, check for 0/-1, create suspended, set affinity, then resume.

### D6. `rand()` is unseeded and per-thread, so every thread gets identical "jitter" — **Medium**

`CPU_methods.h:21`:

```cpp
cDWORD offset[2] = { tcfg->procSync & 0x08 ? 0 : cDWORD(rand() & 0x03F),
                     tcfg->procSync & 0x08 ? 0 : cDWORD(rand() & 0x0FFFF) };
```

The MSVC CRT keeps `rand()`'s state in per-thread storage initialised to the same default seed, and `srand` is
never called. Every thread draws the *same* first two values, and both are computed once at thread start
rather than per cycle.

* **Impact:** the offsets exist specifically to *desynchronise* threads (`CLAUDE.md`: "per-thread random
  start/period jitter … applied to deliberately desynchronise threads"). Identical offsets desynchronise
  nothing, so the non-`T-Sync` modes behave like the `T-Sync` modes and the distinction between `Sp` and `Spt`
  is inoperative.
* **Fix:** derive the offsets from `coreNum` plus a seeded per-thread PRNG, or use `rand_s` (already enabled
  by `_CRT_RAND_S` at `CPU.h:11`).

### D7. `offset[1]` is a permanent period offset in the wrong units, not jitter — **Medium**

`CPU_methods.h:70` — `nextTic += cycleTics + offset[1];` — adds the *same* value every cycle, so the thread's
period is permanently `cycleTics + offset[1]` rather than jittered around `cycleTics`. The magnitude is also
inconsistent with its sibling: `offset[1]` is at most 65 535 **tics** (≈ 6.5 ms on a 10 MHz QPC, ~0.3 % of a
2 s cycle) while `offset[0]` is added to a `Sleep` argument in **milliseconds** (up to 63 ms). One of the two
is in the wrong unit.

### D8. Threads are never joined — **Medium**

`wmain` exits its wait loop as soon as `ThreadsRunning()` returns false (`CPU.cpp:614`), but a thread clears
its bit at `CPU_methods.h:76` *before* calling `_endthread()`. `wmain` then reads `value[3][i]` and
`resArray.iter[i]` for every thread. Combined with A7 (`Failed()` clearing eight bits at once) this becomes a
real correctness problem rather than a theoretical one.

### D9. `ThreadsRunning*` cast away the `volatile` qualifier — **Low**

`CPU.h:145–153` casts `vui64` storage to non-volatile vector references, permitting the compiler to cache the
poll result. In practice the `Sleep(100)` and the non-inlinable call boundary prevent hoisting, but the
guarantee is not there in the source.

---

## E. Scheduling and timing

### E1. Sweeping-pulse mode can `Sleep` for ~49 days at the end of a run — **High** — *Fixed*

> **Fixed** in both of the ways this entry recommends. `ComputationPulse` now re-tests `endTics` against a
> fresh reading immediately after `timer.Update()` and leaves the loop when the run is over, so the ramp is
> only ever evaluated inside the window it is defined on; and the ramp itself is computed in `si64`, clamped
> to `[0, cycleTime]`, and only then narrowed, so no timestamp can produce a wrapped delay even if one
> reached it. The two halves were verified separately: with the re-check removed, the clamp alone still
> bounds every delay to one cycle. The `Sleep` became E2's `PulseSleep`, which does not change this
> arithmetic.
>
> **One correction to the analysis below.** The delay is `DWORD(cycleTime - ratio) + offset[0]`, and that
> addition is unsigned 32-bit, so a small negative is *absorbed* by the thread's random 0–63 ms jitter
> (`-1` plus an `offset[0]` of 14 gives a 13 ms sleep, not a hang). The hang needs the sleep branch to be
> entered more than `offset[0]` ramp steps past `endTics`, one step being `(endTics - startTics) / cycleTime`
> tics — 900 ms for preset `-6`, 529 ms for `-7` (whose cycle is 3 400 ms, not 2 500 — see E7), 15 ms for a
> 30-second `Ts` run. Two consequences: with **T-Sync** (`-7`, `Ts St`) `offset[0]` is 0 and a single step is
> enough, so that preset hangs whenever a thread reaches the sleep branch one step late; without it (`-6`)
> the overshoot must beat the jitter, which for a one-step overshoot happened on 5 of 512 seeds and for a
> 72-step overshoot on all 512. Because `rand()` is unseeded and per-thread (D6), every thread of a run
> draws the *same* `offset[0]`, so this is a per-run rather than a per-thread lottery. The "coin flip" is
> real — which branch the post-deadline iteration takes is unpredictable — but on the 30-minute presets
> getting far enough past `endTics` needs the thread to be descheduled for most of a second, not merely to
> lose the toss.

`CPU_methods.h:63–71`:

```cpp
for(i = j = 0; timer.siCurrentTics < tcfg->endTics; …) {
   timer.Update();
   …
   if(timer.siCurrentTics < nextTic) { … }
   else {
      Sleep(sweepSync ? DWORD(cycleTime - ((timer.siCurrentTics - tcfg->startTics) * cycleTime
                                           / (tcfg->endTics - tcfg->startTics))) + offset[0]
                      : sleepDelay + offset[0]);
```

The loop condition tests the value of `siCurrentTics` from the *previous* iteration; `timer.Update()` at the
top of the body then refreshes it. The body therefore regularly executes with `siCurrentTics > endTics`. When
that happens on the sleep branch the ratio exceeds 1, the subtraction goes negative, and the negative `si64` is
converted to `DWORD` — roughly 4.29 × 10^9 milliseconds.

* **Trigger:** presets `-6` and `-7`, or any `Ts` run. Which branch a thread takes on its final iteration is
  effectively a coin flip, so on a multi-core machine at least one thread will usually hit it.
* **Impact:** the thread never clears its `threadBits` bit, so `while(ThreadsRunning())` at `CPU.cpp:614` never
  returns. **The program hangs at the end of the test and prints no results.**
* **Fix:** clamp the delay to `[0, cycleTime]` and re-check `endTics` after `timer.Update()`.

### E2. `timeapi.h` is included but the timer resolution is never raised — **High** — *Fixed*

> **Fixed** with the waitable timer this entry recommends over `timeBeginPeriod`. `CPU.h` gained a
> pulse-timing group: `CreatePulseTimer` (one `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` timer per thread,
> degrading to an ordinary waitable timer on kernels older than Windows 10 1803 and to `Sleep` if neither
> can be created), `PulseWait` / `PulseSleep` (a relative due time and one `WaitForSingleObject`), and
> `PulseWaitUntil`, which replaces the `do { Sleep(1); … }` start-up poll with a wait for the exact
> remaining interval, re-armed for any early wake-up. Each thread owns its timer and closes it before
> clearing its completion bit — `SetWaitableTimer` on a shared handle would cancel the interval another
> thread was already waiting on. `timeBeginPeriod` was deliberately not used: it raises the tick rate for
> every process on the machine, which would itself perturb the C-state residency the pulse shapes exist to
> exercise, and it would add a `winmm.lib` dependency the project does not otherwise carry. The include
> that named this entry is now genuinely unused and was removed.
>
> Measured by driving the real executor over a virtual clock with a 15.625 ms tick: preset `-4`'s on-phase
> was 236 ms against a requested 250 ms and is now 250.0 ms; `Tf[10]10` delivered a 3.0 ms on-phase at 15 %
> duty and now delivers 10.0 ms at 50 %; the start-up wait overshot by up to 14.8 ms and now lands within
> 0.5 ms. One refinement to the analysis below: the pulse *period* was never quantised — `nextTic` fixes it
> arithmetically, and the following on-phase absorbs whatever the off-phase overran — so what the tick
> distorted was the on/off split inside each cycle, which is what the `[on]off` arguments actually name.

`CPU.h:15` includes `<timeapi.h>`, yet `timeBeginPeriod` is never called anywhere in the tree. Windows'
default scheduler tick is 15.625 ms unless a process requests otherwise, and every pulse boundary in this
program is a `Sleep()` (`CPU_methods.h:55,69`):

* Preset `-4`'s 250 ms on / 1750 ms off quantises to multiples of ~15.6 ms with up to 15.6 ms of jitter per
  edge.
* The start-up loop `do { Sleep(1); timer.Update(); } while(startTics > …)` overshoots the intended start time
  by up to 15.6 ms *per thread, independently* — directly undermining the `T-Sync` and parallel modes whose
  entire purpose is coincident pulse edges.
* Short pulses (`Tf[10]10`) are not representable at all.

* **Impact:** the pulse shapes that distinguish PITC from a constant-load stress test are far less precise than
  the millisecond-resolution options imply, and the "synchronised" modes are not synchronised.
* **Fix:** call `timeBeginPeriod(1)` at start-up and `timeEndPeriod(1)` before returning; better still, use
  `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`.

### E3. Staggered mode double-counts the stagger offset and doubles the cycle at stagger 1 — **Medium**

`CPU_methods.h:44–49`:

```cpp
case 4: // Staggered
   csi64 cycleStag = cycleTics * (coreStag - 1);
   startTics  += cycleStag;
   sleepDelay += cycleTime * (coreStag);
   cycleTics  *= si64(coreStag) << 1;
   nextTic    += startTics + cycleStag;
```

1. `startTics` has already had `cycleStag` added, so `nextTic += startTics + cycleStag` adds it **twice** — the
   first active window opens one full stagger period later than intended. The parallel branch correctly uses
   `nextTic += startTics`.
2. `cycleTics *= coreStag << 1` multiplies by 2 even when `coreStag == 1` (core 0 and every eighth core), so
   those threads run at *half* the requested cycle frequency.
3. `startTics` is delayed by up to 127 cycles without any matching extension of `endTics`, so the highest-index
   core of each group of 8 may barely run before the test ends.

* **Trigger:** presets `-8` and `-9`, or any `Ss` run.

### E4. Integer division by zero in two places — **Medium** — *Partly fixed*

* `CPU.cpp:653` — the benchmark score divides by `(cfg.tics / timer.siFrequency)`, an integer count of whole
  seconds, which is 0 for any run shorter than one second. **Trigger:** `PITC.exe B Tt0.5`. **Still open** —
  A6 rejects only a duration of zero or less, and 0.5 s is a legitimate request.
* ~~`CPU_methods.h:69` — the sweep ramp divides by `(tcfg->endTics - tcfg->startTics)`, i.e. `cfg.tics`.
  **Trigger:** `PITC.exe Tst0`.~~ **Fixed** by A6's `cfg.tics <= 0` rejection.

`Tt` and `T[` are now range-checked against zero (A5, A6) but are otherwise still unvalidated (F4).

### E5. The main loop condition reads a stale timestamp — **Medium** — *Partly fixed*

> **Partly fixed** by E1's re-check. The condition still reads the previous iteration's timestamp, and still
> reads a value another thread may have written (D2), but the body now re-tests `endTics` against a fresh
> reading before acting on it, so a run no longer starts a pulse or a sleep after its deadline: a 60-second
> constant-mode run used to exit one `offTime` (900 ms) late and now exits at the deadline, having executed
> the same number of job cycles. E6's overrun — a long round-robin or staggered sleep already in progress
> when the deadline passes — is untouched and still open.

`CPU_methods.h:63` evaluates `timer.siCurrentTics < tcfg->endTics` before `timer.Update()` runs at
`CPU_methods.h:64`, so the loop always executes one iteration past the end time — and reads a value another
thread may have written (D2). This is the proximate cause of E1 and makes every run overshoot its requested
duration by one pulse.

### E6. Long round-robin and staggered sleeps overrun the requested duration — **Medium**

`CPU_methods.h:39,47` grow `sleepDelay` by `cycleTime * (threadCount - 1)` and `cycleTime * coreStag`. With 64
threads and a 2 s cycle, a round-robin thread issues a single `Sleep(126000)`. Because the loop can only
observe `endTics` between sleeps, the run overshoots by up to one full sleep — over two minutes — and `wmain`
waits for it.

* **Fix:** sleep in bounded slices, or compute the delay as `min(sleepDelay, endTics - now)`.

### E7. Sweep presets never zero the off-time, so their cycle is 900 ms longer than stated — **Medium**

The `Ts` option explicitly sets `cfg.offTime = 0` (`CPU.cpp:263`) because in sweep mode `onTime` *is* the whole
cycle duration. Presets `-6` and `-7` (`CPU.cpp:405–416`) set the sweep bit directly and set `onTime`, but
never touch `offTime`, which keeps its default of 900 ms (`CPU.h:44`). `cycleTics` at `CPU.cpp:599` is
therefore `onTime + 900`.

* **Impact:** `-6` runs a 2 900 ms cycle where its `onTime = 2000` implies 2 000 ms, and `-7` runs 3 400 ms
  rather than 2 500 ms. The two ways of requesting a sweep behave differently.

### E8. The shape of the sweep is undocumented — **Low**

`CPU_methods.h:28` starts `nextTic` at 0, so after the parallel branch adds `startTics` the first iteration
takes the sleep branch for a full cycle — 0 % duty — and the ramp then raises the duty cycle monotonically to
100 % at the end of the run. This is a design choice, not a defect: `CLAUDE.md` describes bit 6 as
"continuously sweeping the duty cycle rather than holding it fixed", which is what the code does. It is
recorded here only because neither `CLAUDE.md` nor the help text (`en-GB.h:43`) says the sweep is
one-directional or that it begins idle, so a user cannot predict from the documentation which duty cycles a
`-6` or `-7` run will actually visit, or in what order.

### E9. A zero-length cycle spins — **Low** — *Fixed*

> **Fixed** by A5. `cycleTics` is `onTime + offTime`, so a zero-length cycle needs `onTime == 0`, which is
> now rejected with `-14` in every mode that reads it.

`Tf[0]0` gives `cycleTics == 0` and `cycleTime == 0` (`CPU_methods.h:22`), so `nextTic` advances only by the
jitter and the thread busy-loops on `Sleep(offset[0])` for the whole test.

### E10. The "Fixed pulse" flag is never read by the executor — **Medium**

> **Still open, but no longer observable.** Since F1's fix, bits 4–6 always carry exactly one mode — the
> default and every preset set one, and each of `T`'s `C`/`F`/`S` replaces it — so "neither Constant nor
> Sweep" and "Fixed pulse" have become the same state, and the banner can no longer claim a mode the
> executor is not running. The flag is still never read, so the impact below survives as a latent one:
> add a fourth mode and the executor will silently treat it as a fixed pulse.

`procSync` bit 5 is parsed from `Tf` (`CPU.cpp:258`) and printed in the banner (`CPU.cpp:575,580`), but a
search of `CPU_methods.h`, `CPU_job_cycles.h` and `CPU.h` finds no read of `0x020` anywhere. `ComputationPulse`
consults bits 0–3 (the sync switch), 4 (Constant), 6 (Sweep) and 7 (Benchmark) only.

* **Impact:** `Tf[250]1750` and a bare `T[250]1750` produce byte-identical runs, yet only the first prints
  `F-P` in the sync column — the banner and the saved results file assert a mode the executor does not
  implement. Conversely, dropping `Tf` from a command line changes nothing, so a user debugging why their
  pulse settings "did not apply" has no way to tell from the output that the flag is inert. Fixed-pulse
  behaviour is really just "neither Constant nor Sweep", which nothing states.

### E11. Thread deadlines are computed from a clock snapshot taken before the arena is allocated — **Medium**

`CPU.cpp:478` calls `timer.Update()`, and it is the last such call before the worker threads are configured.
Between that snapshot and `CPU.cpp:595`, `wmain` allocates the arena (`:517`) and runs the seeding loop
(`:552–566`), which writes every byte of it. Then:

```cpp
threadData[d].startTics = timer.siFrequency * cfg.delayTime / 1000 + timer.siCurrentTics;
threadData[d].endTics   = threadData[d].startTics + cfg.tics;
```

`timer.siCurrentTics` is stale by however long the allocation and seeding took — hundreds of milliseconds for
`Mc8` across many threads, and far longer for a multi-gigabyte request.

* **Impact:** the start-up delay is silently consumed by the seeding pass, and once seeding exceeds
  `delayTime` the computed `startTics` is already in the past, so the wait loop at `CPU_methods.h:54–57` falls
  through immediately and threads begin unsynchronised. `endTics` shifts earlier by the same amount, so the
  test also runs *shorter* than the requested duration by the seeding time.
* **Fix:** call `timer.Update()` immediately before the spawn loop.

### E12. The benchmark score's unit weight and iteration accounting are both wrong — **Medium**

`CPU.cpp:653`:

```cpp
c += swprintf(&wstrOutput[c], wstrInterface[10],
              accum * max(si64((cfg.procUnits & 0x01F) >> 1), 1) / (cfg.tics / timer.siFrequency) >> 10);
```

Two independent accounting errors, on top of the division by zero already recorded as **E4**:

* The weight `(procUnits & 0x1F) >> 1` is an arithmetic accident of the bitfield, not a vector width. It
  yields the intended 2 / 4 / 8 only for the exact combinations `B` installs (`0x05`, `0x09`, `0x11`). Any
  other selection produces a meaningless multiplier — `Iasv` gives 6, a width no unit has, while the run
  actually executes the AVX2 kernel that `JobCycle` selected. The score is derived from what was *requested*
  rather than from what was *dispatched*.
* `accum` sums `j`, the loop-iteration counter from `CPU_methods.h:63`. One iteration is **four** records in
  memory-backed mode (every `JobCycleMem*` processes `offset … offset+3`) but **one** in register mode, so
  scores from `B` (memory-backed) and from a register-mode run differ by a factor of four in units, not in
  performance. `j` also counts sleep iterations, which is harmless for `B` because it sets the Constant bit,
  but wrong for any pulsed run that reaches this code.

* **Impact:** KUPS figures are not comparable between configurations, and the published benchmark number is
  only meaningful for the exact default `B` invocation.

---

## F. Command-line parsing and results output

### F1. Any `T` option silently cancels Constant / Fixed / Sweep mode — **High** — *Fixed*

> **Fixed.** The blanket `cfg.procSync &= 0x08F;` is gone from the head of the option; each of the `C`, `F`
> and `S` sub-options now clears bits 4–6 immediately before setting its own. A `T` argument therefore
> replaces the timing mode only when it names one, and two mode letters in one command line resolve
> last-wins instead of setting two contradictory bits (`Tcf` was Constant+Fixed, and is now Fixed).
>
> One consequence is worth knowing: pulse *timings* no longer imply pulse *mode*. `T[250]1750` with no
> preset and no `F` now runs in the default Constant mode rather than silently switching to pulses — the
> banner reports `Con`, and `Tf[250]1750` gives the pulsed run. Presets are unaffected, since each sets a
> mode of its own.

`CPU.cpp:235`:

```cpp
case L't': // Set timing options
case L'T':
   cfg.procSync &= 0x08F;
```

`0x8F` keeps bits 0–3 and 7 and clears bits **4, 5 and 6** — Constant, Fixed-pulse and Sweeping-pulse. The
clear happens before the sub-options are read, so it fires even when the argument only carries a *global*
sub-option such as a duration or a start-up delay.

* **Trigger:** `PITC.exe B Tt120`, `PITC.exe -1 Tt600`, `PITC.exe -4 Td5.0` — the natural way to adjust a
  preset. Reversing the order (`Tt120 B`) behaves differently, because the preset then rewrites `procSync`.
* **Impact:** `B Tt120` and `-1 Tt600` silently convert a *constant-load* test into a pulsed one running the
  default 100 ms on / 900 ms off — a 10 % duty cycle instead of 100 %. The banner reports the changed sync
  flags, but nothing warns that the requested mode was discarded, and a user extending a stress test's
  duration gets a tenth of the load they asked for.
* **Fix:** clear bits 4–6 only when the argument actually contains `C`, `F` or `S`.

### F2. The `Uc` and `Ut` core-map options are unusable — **High**

`CPU.cpp:283–318`. Three independent errors:

```cpp
case L't':
   for(++j; argv[i][j] && argv[i][j] != L' '; ++j) {
      …
      cfg.coreMap[(j - 1) >> 3] |= 0x01ull << j;
```

1. **Off by two cores.** When the inner loop starts, `j == 2` for the first map character (1 was the `c`/`t`
   selector; `++j` moves past it). The bit index used is `j`, so the first character controls virtual core 2,
   not core 0 — cores 0 and 1 cannot be addressed at all. `Uc` is off by two in the same way, its first
   character controlling core pair 2 (`0x03ull << (j << 1)` = bits 4–5).
2. **Wrong word index.** `(j - 1) >> 3` selects the `coreMap` element as though a 64-bit word held 8 cores,
   and the bit index is never reduced modulo 64. From the eighth map character on, the code writes to
   `coreMap[1]` while still shifting by the absolute `j` — the two disagree completely.
3. **Undefined shift.** Nothing bounds `j`, so `Ut` shifts by ≥ 64 at the 64th character and `Uc` at the 32nd
   (C10).

The inner loop also consumes the remainder of the argument, so no further `U` sub-option can follow a map —
which makes the documented examples `Uc!.!!...!s` (`en-GB.h:44`) and `Uc!.!!...!a` (`README.md:33`)
impossible — and it exits with `j` on the terminating NUL, after which the *outer* loop's `++j` steps past the
end of the argument and dereferences it (`CPU.cpp:275`). Neither option clears the map first, so the
characters only modify whatever topology enumeration produced, and `SetSMTLoading()` (called later at
`CPU.cpp:471`) may overwrite the result entirely.

* **Impact:** manual core selection — one of the six advertised option groups — does not work, and silently
  produces a different core set from the one the user wrote.

### F3. The UTF-16 results file is truncated to half its length — **High**

`CPU.cpp:661`:

```cpp
if(!WriteFile(outFile, wstrOutput, c, &bytesProc, 0)) {
```

`c` is the accumulated `swprintf` **character** count; `WriteFile`'s third parameter is a **byte** count. A
UTF-16 results file therefore contains only the first half of the report, ending mid-character. The 8-bit path
at `CPU.cpp:668` is correct, because there `c` narrow characters really are `c` bytes.

* **Trigger:** `PITC.exe O16[results.txt] …` (any run writing a UTF-16 results file).
* **Fix:** `WriteFile(outFile, wstrOutput, c * sizeof(wchar), &bytesProc, 0);`

### F4. Numeric arguments are never validated and the index advance can wrap — **High**

Every numeric option follows the same pattern, e.g. `CPU.cpp:240–241`:

```cpp
cfg.onTime = ui32(wcstol(&argv[i][j + 1], &stopChar, 10));
j += ui8(stopChar - &argv[i][j] - 1);
```

* `stopChar` is never compared against the start pointer, so a missing or non-numeric value silently yields 0.
  `Tt` gives a zero-length test (A6); `T[` gives a zero on-time (A5) — both of which report `.Pass.`.
* Negative values are accepted. `Tt-5` makes `cfg.tics` negative; `T[-100` makes `onTime` a huge `ui32`
  (≈ 4.29 × 10^9 ms of "on" time); `Mc-1` produces a negative allocation size (C5).
* The advance is truncated to `ui8`, so a numeric field longer than 255 characters wraps the delta and moves
  `j` backwards, producing an infinite parse loop. `j` is `ui8` throughout, so any argument longer than 255
  characters has the same effect.
* `wcstol` returns a 32-bit `long` on Windows, so `Mt3000000` overflows before the `<< 20`.

* **Fix:** validate `stopChar != &argv[i][j + 1]`, range-check every value, widen `j`, and use `wcstoll` for
  byte counts.

### F5. `O`'s filename length is miscalculated and trailing options are swallowed — **Medium**

`CPU.cpp:200–205`:

```cpp
case L'[':
   for(d = ++c; argv[i][c] && argv[i][c] != L']' && c < 1024; ++c);
   if(!lstrcpynW(wstrOut, &argv[i][d], c++ - 1)) { … }
```

The character count should be `c - d + 1`; `c - 1` is correct only when `d == 2`, i.e. when `[` immediately
follows `O`. And `c++` inside the call, followed by the enclosing loop's `++c`, skips the character after `]`.
Both documented spellings misbehave:

| Command | Filename produced | Encoding produced | Expected |
|---------|-------------------|-------------------|----------|
| `O[results.txt]16` (documented, `en-GB.h:35`) | `results.txt` ✔ | ASCII ✘ | UTF-16 |
| `O16[results.txt]` | `results.txt]` ✘ | UTF-16 ✔ | `results.txt` |

* **Impact:** it is not currently possible to write a correctly named UTF-16 results file — and even when the
  encoding is selected, F3 truncates it.
* **Fix:** use `c - d + 1` as the count and do not advance `c` past `]` inside the call.

### F6. The UTF-8 option writes locale ANSI bytes behind a UTF-8 BOM — **Medium**

`CPU.cpp:488–490` emits a UTF-8 BOM for `outUTF == 1`, but the conversion at `CPU.cpp:667` is `wcstombs`,
which uses the locale set by `setlocale(LC_ALL, "")` at `CPU.cpp:29` — on most Windows installations the
system ANSI code page, not UTF-8. The file therefore claims UTF-8 and contains something else; any non-ASCII
character (a future translation, or a path in an error line) is mojibake.

* **Fix:** use `WideCharToMultiByte(CP_UTF8, …)` for the UTF-8 path.

### F7. The `L` option always selects English — **Medium**

`CPU.cpp:142,147` — `if(lstrcmpiW(wstrLang, L"en-US"))` — `lstrcmpiW` returns 0 when the strings are **equal**,
so both conditions are true exactly when the code is *not* the one named. Every input lands on English. Also
note the stray `;` after the second block's closing brace at `CPU.cpp:151`. Already flagged in `CLAUDE.md`;
must be fixed before a second language is added.

### F8. Reported "Memory allocated" double-counts in the split configuration — **Medium**

`CPU.cpp:574` prints `(cfg.allocMem[0] + cfg.allocMem[1]) >> 20`. In `memConfig == 2` (`CPU.cpp:508`)
`allocMem[0]` has already been overwritten with the *total* while `allocMem[1]` still holds the per-SMT-core
size, so `Mn8 Ms4` reports the true total plus 4 MB. In the other modes `allocMem[1]` is its default 1 byte,
which vanishes under `>> 20`.

### F9. Unknown options, letters and presets are silently ignored — **Medium**

The top-level `switch(argv[i][0])` at `CPU.cpp:90` has no `default`, and neither do the inner switches for
`I`, `M`, `S`, `T`, `U` or `-`. `PITC.exe -5x`, `PITC.exe Iq`, `PITC.exe Zzz` and a mistyped `PITC.exe -a` all
run with defaults and no warning. `case L'-'` additionally applies its memory/unit preamble
(`CPU.cpp:370–372`) *before* testing the digit, so an invalid preset still changes the configuration.

### F10. `O1` reads one character past the argument's terminator — **Low**

`CPU.cpp:190–192`:

```cpp
case L'1':
   if(argv[i][++c] == L'6') outUTF = 2;
```

For the argument `O1`, `++c` lands on the terminating NUL (in bounds), but the enclosing loop's `++c` then
advances past it and the loop condition `argv[i][c]` reads out of bounds.

### F11. `wprintf` is called with a runtime-built buffer as the format string — **Low**

`CPU.cpp:588,658` pass `wstrOutput` directly as the format. No `%` can reach that buffer today (it holds only
`!`/`.` core-map characters, hex digits and fixed text), but the pattern is one edit away from a format-string
bug — and `wstrOut`, the user-supplied output filename, is already interpolated elsewhere.

### F12. `W` reuses the enclosing loop variable — **Low**

`CPU.cpp:326` runs `for(i = 0; i < MAX_THREADS; ++i)` using `i`, the argument-loop index. Harmless only because
the case always returns, but it means `W` can never be combined with any other option.

### F13. Option order changes the result in undocumented ways — **Low**

Options are applied in argv order with no precedence rules. `B Tt120` and `Tt120 B` differ; `-1 Iav` and
`Iav -1` differ; `Ts]500` and `T]500s` differ (`Ts` zeroes `offTime`, so whichever comes last wins). `case L'-'`
unconditionally overwrites `procUnits`, `memConfig` and `allocMem`, so a preset always discards an earlier `I`
or `M`. Worth documenting, or worth defining a precedence.

### F14. The invalid-filename error prints the wrong message and an empty name — **Low**

`CPU.cpp:202–205`:

```cpp
if(!lstrcpynW(wstrOut, &argv[i][d], c++ - 1)) {
   wprintf(wstrMessage[9], wstrOut);
   return -8;
}
```

`wstrMessage[9]` is *"Unable to create file \"%s\"."* (`en-GB.h:73`) — but this branch is the **invalid
filename** path, which `en-GB.h:22` documents as a distinct condition, and no file has been attempted yet.
The `%s` argument is `wstrOut`, which `lstrcpynW` has just failed to populate — as a zero-initialised global it
prints as an empty string. The message the user sees names neither the problem nor the input that caused it.

The branch is also unreachable in practice: `lstrcpynW` returns NULL only on failure, and an empty filename is
not a failure. `PITC.exe O[]` gives `d == c == 2`, so the call copies zero characters, null-terminates, and
returns a valid pointer. `wstrOut[0]` is then 0, the guard at `CPU.cpp:481` is false, and the run completes
with **file output silently disabled** — no file, no warning, exit code 0.

### F15. `system("pause")` makes the no-argument path hang outside an interactive console — **Low**

`CPU.cpp:444`:

```cpp
wprintf(wstrInstructions);
system("pause");
return 2;
```

Running `PITC.exe` with no arguments — the documented way to display the option reference — blocks on a
keypress. In a CI job, a scheduled task, a piped invocation (`PITC.exe | more`) or any non-interactive session
the process hangs until killed. `system()` also spawns `cmd.exe` through the `PATH`, which is an unnecessary
dependency and an unnecessary attack surface for printing help text.

* **Fix:** drop the `system("pause")`; a console program that prints usage should exit.

---

## G. Topology and scaling

### G1. `SetSMTLoading` invokes undefined behaviour on any CPU without SMT — **High** — *Fixed*

> **Fixed** at both ends. `cfg.sys.SMT` is normalised to 1 immediately after the topology walk, because
> `ProcessorCore.Flags` is set only for a core carrying more than one virtual core and "no SMT" means one
> virtual core per physical core — that removes `ui64(cfg.sys.SMT - 1)` as a shift count. The scan that ran
> to 64 is gone along with the arithmetic that needed it: `SetSMTLoading`'s two one-virtual-core policies now
> mask with `cfg.sys.coreSibling`, a pair of bitmaps the enumeration records one core at a time (see G2).
> Neither undefined shift has an expression left to appear in.
>
> The impact this entry describes is closed twice over: an enumeration naming no processor core is refused
> with `-23` before the spawn loop (G6), so the "empty results table, exit code 0" path now needs a `U` map
> that disables every core to reach — which is **C8**, still open.
>
> Verified off-target. The walk, the `groupCount` derivation and both `SetSMTLoading` arms were transcribed
> pre- and post-fix into a harness and run over seven synthetic topologies — uniform 2-way and 4-way SMT, a
> 6-core and a 1-core non-SMT part, a 32-core 2-way part, and two hybrid P/E mixes — with the two undefined
> shifts emulated as x86 executes them (count masked to 6 bits, which is what the shipped binary does). The
> pre-fix arms fail 13 of 35 checks and the post-fix code none of 93. On both non-SMT topologies a `-1`
> preset selected **0** virtual cores before the fix and selects one per physical core after it.

`CPU.h:168–176`:

```cpp
default: // Use one virtual core
   ui64  mask;
   cui64 shift = cfg.SMTLoad == 2 ? ui64(cfg.sys.SMT - 1) : 0;
   cui64 mask0 = mask = 0x01ull << shift;
   while(i < cfg.sys.groupCount) {
      for(j = 1; j < cfg.sys.coreCount[1]; ++j) mask = (mask << cfg.sys.SMT) + mask0;
      for(k = 0; k < 64 && cfg.sys.coreMap[1][i] >> k == 0; ++k);
      cfg.coreMap[i++] &= (mask <<= k);
   }
```

On a CPU with no SMT, `cfg.sys.SMT` is never assigned (`CPU.cpp:38–42` only writes it when
`ProcessorCore.Flags` is set) and stays 0. Then:

* `ui64(cfg.sys.SMT - 1)` is `ui64(-1)`, so `0x01ull << shift` shifts by 2^64−1 — **undefined behaviour**.
* `cfg.sys.coreCount[1]` is 0, so the mask-building loop never runs.
* `cfg.sys.coreMap[1][i]` is 0, so `0 >> k == 0` holds for every `k` and the scan runs to `k == 64`, giving
  `mask <<= 64` — **undefined behaviour again**.

* **Trigger:** `Uo` or `Ue`, and presets `-1`, `-3`, `-4`, `-6`, `-8` (all set `cfg.SMTLoad = 2`), on any
  non-SMT CPU — most Intel i5 desktop parts of several generations, all E-core-only parts, and many VMs.
* **Impact:** the core map is computed from undefined values. With a memory request the empty map divides by
  zero at `CPU.cpp:499`; without one, `threadCount[2]` is 0, the spawn loop at `CPU.cpp:592` runs zero times,
  `ThreadsRunning()` is immediately false, and `wmain` prints an empty results table and returns **0** —
  documented in `en-GB.h:15` as "Successul completion of stability test". A CPU that was never tested at all
  reports success.

### G2. `SetSMTLoading` deselects every non-SMT core — **High** — *Fixed*

> **Fixed** by recording the sibling layout rather than reconstructing it. The topology walk ORs the lowest
> set bit of each processor-core record's `ProcessorMask` into `cfg.sys.coreSibling[0][group]` and the
> highest into `[1][group]`, and `SetSMTLoading`'s `Ue`/`Uo` arms mask with one of those. That record is the
> only place the layout is knowable — `cfg.sys.coreMap[0]` and `[1]` are unions over a whole core class and
> cannot say afterwards which virtual cores share a physical core — and a core without SMT contributes the
> same bit to both sibling maps, so it survives either policy instead of being cleared by a mask built from
> the SMT core count alone.
>
> On the 6 P-core + 8 E-core topology in the G1 harness, `Uo` selected 6 virtual cores before the fix and
> selects 14 after: every physical core, one thread each. On 8 P + 16 E it is 8 before and 24 after.
>
> **G5 had to be taken with it** for those threads to reach the cores this now selects — the spawn loop was
> pinning the second thread class over cores the first already held. It is fixed in the same batch; see G5.

`CPU.h:173` builds the mask from `cfg.sys.coreCount[1]` (SMT physical cores) only. `cfg.coreMap[i] &= mask` at
`CPU.h:175` then clears every bit belonging to a non-SMT core.

* **Trigger:** `Ue`/`Uo` or presets `-1`, `-3`, `-4`, `-6`, `-8` on a hybrid CPU.
* **Impact:** on an Intel P/E-core part, "one thread per physical core" silently tests the P-cores only and
  ignores every E-core — while the banner still reports the full thread bitmap.

### G3. Hard 64-virtual-core ceiling despite `MAX_THREADS == 512` — **High**

All buffers are sized for 512 threads (`CPU.h:20`, `CPU.cpp:18`), but the enumeration and affinity code assume
a single 64-bit mask. The author's own `///--- Modify to account for >64 virtual cores !!!` markers appear at
`CPU.cpp:31,71,611` and it is the top item on `CPU.cpp`'s to-do list.

Specific blockers: `GetLogicalProcessorInformation` (`CPU.cpp:30`) does not report processor groups at all —
`GetLogicalProcessorInformationEx` is required; the affinity walk at `CPU.cpp:611` only ever consults
`cfg.coreMap[procGroup]` with `procGroup` fixed at 0 (`CPU.cpp:26`); and `SetThreadAffinityMask` cannot cross
groups (`SetThreadGroupAffinity` is needed).

* **Impact:** on a machine with more than 64 logical processors, `while(mask & ~cfg.coreMap[procGroup]) mask <<= 1;`
  shifts `mask` to 0 and `SetThreadAffinityMask(handle, 0)` fails, so threads run wherever the scheduler puts
  them. Together with D3's unchecked high bits, results become unreliable rather than merely unpinned.

### G4. `groupCount` adds `coreCount[0]` to itself — **High** — *Fixed*

> **Fixed.** `groupCount` is now the number of groups the walk populated, counted from
> `cfg.sys.coreMap[0][j] | cfg.sys.coreMap[1][j]` across the eight-word span a 512-thread bitmap covers,
> instead of being derived arithmetically from a core count. Under `GetLogicalProcessorInformation` that is
> always 1 — the writer indexes with `procGroup`, which is 0 and never assigned again — and that is exactly
> the point: the five loops over an empty group, the banner's second all-dots row, and the all-zero
> `coreMap[1][1]` that `SetSMTLoading` scanned to 64 all go with the second group. Counting the data rather
> than predicting it also means the derivation stays correct when `GetLogicalProcessorInformationEx`
> replaces the walk (**G3**).
>
> In the G1 harness `groupCount` was 2 on all four non-SMT and hybrid topologies before the fix, and is 1 on
> all seven after it.

`CPU.cpp:71`:

```cpp
cfg.sys.groupCount = ((cfg.sys.coreCount[0] + cfg.sys.coreCount[0] + 63) >> 6) + 1;
```

The second term should be `coreCount[1]` (SMT physical cores); as written the SMT count is excluded entirely
and the non-SMT count doubled. `groupCount` drives the loops at `CPU.cpp:72,472,582` and `SetSMTLoading`
(`CPU.h:164,172`).

On a conventional SMT CPU `coreCount[0]` is 0, so the expression yields `(63 >> 6) + 1` = 1 and everything
works by accident. But the trailing `+ 1` means that **as soon as `coreCount[0]` reaches 1, `groupCount`
becomes 2** — while only `cfg.coreMap[0]` and `cfg.sys.coreMap[*][0]` are ever populated, because the writer
at `CPU.cpp:36–37` indexes them with `procGroup`, which is initialised to 0 at `CPU.cpp:26` and never
assigned again. Five loops then iterate over a second group that is entirely zero.

That is what makes **G1**'s shift-by-64 reachable on hardware that *does* have SMT: on a hybrid P/E-core part
`coreCount[0]` counts the E-cores, so `groupCount` is 2, and `SetSMTLoading`'s second iteration scans an
all-zero `coreMap[1][1]`, runs `k` to 64, and executes `mask <<= 64`. The banner at `CPU.cpp:582–586` also
prints a second, all-dots thread-bitmap row for a processor group that does not exist.

### G5. The affinity mask restarts at bit 0 for the SMT pass — **Medium** — *Fixed*

> **Fixed**, and the entry understates it. Hoisting `mask` out of the class loop would stop the collision,
> but it would also let a class-0 thread be pinned to an SMT core and a class-1 thread to a non-SMT one,
> while `packetSizeRAM` and `resArray.records[j]` — chosen by class, a few lines above — still described the
> other kind. The walk now consults `cfg.sys.coreMap[j][procGroup] & cfg.coreMap[procGroup]`, the selected
> cores of the thread's *own* class. `threadCount[j]` is the population count of exactly that expression, so
> there is one core in it for every thread of the class: the assignment is one-to-one, no selected core is
> left idle, and the per-class block sizes describe the core the thread is really on. The two class maps are
> disjoint, so `mask` restarting at bit 0 for each class is now correct rather than merely harmless, and it
> stays as it was.
>
> **The qualifier below is wrong, and that is the part worth reading.** "Masked whenever
> `threadCount[0] == 0`, i.e. on a uniform SMT CPU" is true of uniform CPUs and of nothing else: `coreType`
> is inferred from a core's sibling count (**G9**), so on every hybrid P/E-core part the E-cores *are* the
> non-SMT class and both classes are populated at every setting — with no `U` argument at all, not just
> under `Ue`/`Uo`. Extending the G1 harness to the spawn loop's walk puts numbers on it: on 6 P-cores + 8
> E-cores a default run created 20 threads and pinned them onto **12** distinct cores, and on 8 P + 16 E it
> created 32 and pinned them onto **16** — in both cases every E-core in the machine went untested, at every
> setting, while its P-cores carried two threads each. Post-fix the same runs cover 20 of 20 and 32 of 32.
> The five non-hybrid topologies cover every selected core before and after, which is why this never showed
> on the machines most likely to be used for testing.

`CPU.cpp:592–593`:

```cpp
for(d = 0, j = 0; j < 2; ++j)
   for(i = 0, mask = 1; i < threadCount[j]; ++d, ++i, mask <<= 1) {
```

`mask` is re-initialised to 1 when `j` advances, so the SMT threads are pinned starting from the same low bits
already assigned to the non-SMT threads. Two threads land on the same virtual core while other cores idle.
Masked whenever `threadCount[0] == 0`, i.e. on a uniform SMT CPU.

### G6. `GetLogicalProcessorInformation`'s return value is ignored — **Medium** — *Fixed*

> **Fixed.** The call is checked, and on `FALSE` the run stops with the new exit code `-23` and a message
> carrying `GetLastError()`. There is deliberately no retry at the size the API asks for: the buffer holds
> `MAX_THREADS * 4` records, several times what a processor group of 64 virtual cores can describe, so
> `ERROR_INSUFFICIENT_BUFFER` here would mean a machine this build cannot describe rather than one that needs
> a second attempt. The `(si32&)` reinterpretation is gone with the decrement it guarded — the walk divides
> the byte count the API wrote back into a record count and iterates that, in `ui32`.
>
> A call that succeeds but names no processor core is refused the same way, for the reason this entry gives
> for the failure case taken one step further: the core map is empty, so no thread is created, and `wmain`
> would print an empty results table and return 0 — "successful completion of stability test" — for a CPU it
> never touched. The same zero is the divisor in `GenerateValues`' `W` path. One exit code, `-23`, shared by
> two messages, the way `-11`, `-17` and `-21` already share theirs; `wstrMessage_*` grew from 31 entries to
> 33.

`CPU.cpp:30–31`:

```cpp
GetLogicalProcessorInformation(sysLPI, &(bytesProc = sizeof(…) * MAX_THREADS * 4));
for(i = 0; (si32&)bytesProc > 0; ++i, bytesProc -= sizeof(…)) {
```

On failure the API sets `bytesProc` to the *required* size and returns FALSE. Since the return value is
discarded, the loop then walks uninitialised memory as though it were topology records, building a nonsense
core map. The `(si32&)` reinterpretation of a `DWORD` also makes a required size above 2 GiB read as negative.

### G7. `SetSMTLoading`'s "first set bit" scan is a no-op — **Medium** — *Fixed*

> **Fixed as a consequence of G1 and G2, not as separate work.** `SetSMTLoading` no longer computes an
> offset at all: the scan, the `cfg.sys.SMT` stride and the `mask0` ladder are replaced by a mask taken from
> the per-core sibling bitmaps the enumeration records, which is correct whatever order the OS numbers
> virtual cores in — including the hybrid layout this entry describes, where the E-cores hold the low bits.

`CPU.h:174` — `for(k = 0; k < 64 && cfg.sys.coreMap[1][i] >> k == 0; ++k);` — for any non-zero map,
`map >> 0 != 0`, so the loop exits immediately with `k == 0` and `mask <<= 0` does nothing. The intent was
evidently to skip to the first SMT core; the loop as written can only ever return 0 or 64 (the latter being the
UB case in G1). On a hybrid CPU where E-cores occupy the low bits, this silently applies the SMT mask at the
wrong offset.

### G8. `SetSMTLoading`'s "use all virtual cores" arm shadows its loop variable and over-smears — **Medium**

> **Half overtaken, not fixed.** The inner `ui8 i` is gone: the `default` arm that needed the outer `i`
> pre-initialised was replaced by G1 and G2, so `case 3` declares nothing of its own and uses the outer
> variable. The substantive half is untouched — the statement still reads `cfg.coreMap[i]` while writing it,
> so a 4-way SMT part still accumulates shifts past the intended sibling span, and the `j == 0` iteration is
> still a no-op. The per-core sibling bitmaps G2 added would let this arm be written as an exact expansion
> too, which is the obvious way to take it.

`CPU.h:163–167`:

```cpp
case 3: // Use all virtual cores
   for(ui8 i = 0; i < cfg.sys.groupCount; ++i)
      for(j = 0; j < cfg.sys.SMT; ++j)
         cfg.coreMap[i] |= (cfg.coreMap[i] << j) & cfg.sys.coreMap[1][i];
```

The inner `ui8 i` shadows the outer `i` declared at `CPU.h:158` (which the `default` arm relies on). More
substantively, the statement reads `cfg.coreMap[i]` while writing it, so each iteration smears the *already
smeared* map: for `SMT == 4` the accumulated shift exceeds the intended sibling span. The `j == 0` iteration is
also a no-op.

### G9. Hybrid P/E-core CPUs are misclassified — **Medium**

`CPU.cpp:32` — `cui8 coreType = (PopulationCount64(sysLPI[i].ProcessorMask) > 1 ? 1 : 0);` — infers
"SMT-ness" from the sibling count. On an Intel hybrid part the E-cores (1 thread) become `coreType 0` and the
P-cores (2 threads) `coreType 1`, so the "non-SMT / SMT" split that `Mn`/`Ms` and `SetSMTLoading` operate on
actually means "E-core / P-core" — with wildly different cache sizes recorded in `cfg.sys.cache[0]` and `[1]`.
Neither the code nor the documentation acknowledges this, and `Ue`/`Uo` ("first"/"last" virtual core of each
physical core) have no meaningful interpretation for single-threaded E-cores. `cfg.sys.SMT` being set to the
*maximum* observed sibling count (`CPU.cpp:40–41`) compounds every downstream calculation.

---

## H. Build configuration and determinism

### H1. `Debug|x64` compiles the *baseline* kernel unit with `/arch:AVX512` — **High** — *Fixed*

> **Fixed**, in two steps by two hands. The override itself was already gone from the tree before this batch
> — commit `39f0d4b` deleted the `AdvancedVectorExtensions512` line from `CPU_jobs_standard.cpp`'s item, so
> that unit now compiles at the global `StreamingSIMDExtensions2` baseline in both x64 configurations, which
> is impact (1) closed. The same commit pinned `<FloatingPointModel>Strict</FloatingPointModel>` on both x64
> configurations, which closes impact (2): `/fp:strict` forbids contraction outright, so `JobFPU`'s trailing
> `x *= x * 1.01010101010101 + 0.00021` cannot become an FMA under any `/arch` setting.
>
> What this batch adds is the guard that keeps it fixed. `CPU_jobs_standard.cpp` now `#error`s if `__AVX__`,
> `__AVX2__` or `__AVX512F__` is defined when it is compiled, so a per-file `/arch` override cannot be
> reapplied to the baseline unit — from the IDE's property pages or otherwise — without the build failing
> and saying why. The complementary requirement, that this unit and the other three compile under
> `/fp:strict`, is enforced by `CPU_build.h`; see H2.
>
> Note this is a guard on the *baseline* unit only. The AVX2 and AVX-512 units deliberately carry no
> matching "must be raised" assertion, because in `Debug|x64` they legitimately compile at the SSE2
> baseline today — that is **H3**, which is untouched.

`CPU.vcxproj:175–179`:

```xml
<ClCompile Include="CPU_jobs_standard.cpp">
  <EnableEnhancedInstructionSet Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">AdvancedVectorExtensions512</EnableEnhancedInstructionSet>
```

`CPU_jobs_standard.cpp` holds the scalar ALU and FPU kernels — the path that must run on *any* x64 CPU.
Compiling it with AVX-512 enabled lets the compiler emit EVEX-encoded scalar instructions there.

* **Impact (1):** a `Debug|x64` build faults with an illegal instruction in the ALU/FPU path on any CPU without
  AVX-512 — the debug build is unusable on exactly the hardware most likely to need debugging.
* **Impact (2):** `/arch:AVX2` and above permit MSVC to contract `a*b + c` into FMA under the default
  `/fp:precise`. `JobFPU` ends each outer iteration with `x *= x * 1.01010101010101 + 0.00021;`
  (`CPU_jobs_standard.cpp:39`), a textbook contraction candidate. FMA's single rounding differs from
  multiply-then-add, so **`cpu.values` generated by the Debug build may not validate under the Release build,
  and vice versa** — with the mismatch presented to the user as a CPU fault.
* **Fix:** remove the override, and see H2.

### H2. No floating-point model is pinned, so the golden values are not reproducible by construction — **Medium** — *Fixed*

> **Fixed** with the entry's first option, `/fp:strict`, in all four places rather than two, and enforced
> from the source as well as from the project file.
>
> Commit `39f0d4b` had already set `<FloatingPointModel>Strict</FloatingPointModel>` on both x64
> configurations. This batch adds it to both `Win32` configurations as well — they are vestigial and remain
> broken for the reasons **H7** gives, but the entry's complaint is that *no* configuration pinned the
> model, and leaving two unpinned would leave that half true. It also adds
> `<FloatingPointExceptions>false</FloatingPointExceptions>` to `Debug|x64`, which is the divergence the
> entry's closing sentence names: `Release|x64` set it while `Debug|x64` inherited `/fp:strict`'s default of
> `/fp:except`, so the two builds were compiling the same kernels under different exception semantics.
>
> Enforcement is `CPU_build.h`, a new header included by all four `CPU_jobs_*.cpp` units and by `CPU.h`. It
> `#error`s under MSVC when `_M_FP_STRICT` is not defined, so a build made outside the project file cannot
> quietly inherit `/fp:precise` and produce a `cpu.values` that no `/fp:strict` build will validate. The
> check is skipped for non-MSVC front ends, which do not define `_M_FP_*`.
>
> The model is also recorded in the file, as the entry asks: `VALUES_FP_FLAGS` (`CPU_build.h`) encodes the
> model and whether contraction is permitted, `CPU.h` folds it into `VALUES_BUILD_ID`, and B6's header
> carries that. `/fp:except` is deliberately *not* encoded — it decides whether an exception is raised, not
> what a result is, so a build differing only in it produces identical golden values and should not have its
> file rejected. `#pragma fp_contract(off)` was not added: it is the entry's fallback for a `/fp:precise`
> build, and `/fp:strict` already disables contraction, so the pragma would only restate what the guard
> above now guarantees.

No configuration in `CPU.vcxproj` sets `<FloatingPointModel>`, so all four inherit MSVC's default
`/fp:precise`. For a tool whose entire premise is bit-identical FP results across builds and machines, the
model, contraction behaviour and denormal handling should be nailed down explicitly (`/fp:strict`, or at
minimum `/fp:precise` plus `#pragma fp_contract(off)` in the kernel units) and recorded in the `cpu.values`
header (B6). Related: `Release|x64` sets `<FloatingPointExceptions>false</FloatingPointExceptions>`
(`CPU.vcxproj:151`) while `Debug|x64` leaves it at the default — another way the two builds diverge.

### H3. Per-file ISA and optimisation settings apply only to `Release|x64` — **Medium**

The `EnableEnhancedInstructionSet` overrides at `CPU.vcxproj:165,169` and every `Optimization=Custom` carry
`Condition="…=='Release|x64'"`. In `Debug|x64` the AVX2 and AVX-512 kernel units compile at the global
`StreamingSIMDExtensions2` baseline. MSVC still accepts the intrinsics, but it will not use VEX encoding for
the surrounding code (producing AVX↔SSE transition penalties), and the debug build does not honour the
documented "kernels are not optimised" contract in the same way as release.

### H4. The AVX-512 and AVX2 comparison intrinsics live in the SSE2-baseline translation unit — **Medium**

`CLAUDE.md` states that the global `StreamingSIMDExtensions2` setting "keeps AVX-512 opcodes out of the
baseline code path". That holds for the *kernels*, but `CPU_job_cycles.h` — containing `_mm_testc_si128`
(SSE4.1), `_mm256_testc_pd` (AVX) and `_mm512_mask_cmpneq_pd_mask` (AVX-512) — is included via
`CPU_methods.h` into `CPU.cpp`, which compiles at the SSE2 baseline in every configuration.

No fault occurs at runtime because the ISA gate at `CPU.cpp:449–451` prevents those functions from being called
on unsupported hardware. But the isolation story is weaker than documented: `CPU.cpp` emits AVX-512 opcodes,
and MSVC is compiling ZMM-using code without being told the target supports it, which affects `vzeroupper`
placement and register allocation. Move the comparison wrappers into their per-ISA translation units, or
document the reliance explicitly.

### H5. `Release|x64` combines mutually contradictory optimisation switches — **Low**

`CPU.vcxproj:143–147` sets `<Optimization>Disabled</Optimization>` (deliberate, per `CLAUDE.md`) alongside
`<WholeProgramOptimization>true</WholeProgramOptimization>` (`/GL`), `<FavorSizeOrSpeed>Size</FavorSizeOrSpeed>`
(`/Os`) and `<InlineFunctionExpansion>AnySuitable</InlineFunctionExpansion>` (`/Ob2`). `/Os` and `/Ob2` have no
effect under `/Od`, and `/GL` defers optimisation to link time — precisely what the `/Od` choice exists to
prevent. Whether LTCG actually reaches the kernels should be confirmed from the generated assembly; if it does,
the "kernels are not optimised" invariant is not being enforced. `<EnableFiberSafeOptimizations>` (`/GT`) and
`<AssemblyDebug>` (a managed-code linker option) are inert here.

### H6. Security mitigations are disabled in the shipping configuration — **Low**

`CPU.vcxproj:136,149,150` set `SDLCheck=false`, `BufferSecurityCheck=false` (`/GS-`) and
`ControlFlowGuard=false` in `Release|x64`, while `Debug|x64` leaves `SDLCheck=true`. Given the unbounded
`swprintf` calls (C7) and the `lstrcpynW` overflow (C6), removing stack cookies from the released binary is the
wrong trade for a tool that parses user input.

### H7. The `Win32` configurations are broken — **Low** — *Fixed*

> **Fixed** by deletion, as this entry recommends. Both `Win32` configurations are gone in full — their
> `ProjectConfiguration` entries, their `Label="Configuration"` property groups, their property-sheet
> imports and their item-definition groups — leaving `CPU.vcxproj` offering `Debug|x64` and `Release|x64`
> only.
>
> `CPU_build.h`, which all four kernel units and `CPU.h` include, additionally `#error`s when it is
> compiled for anything but x64, so the configurations cannot be reintroduced by accident and the sources
> cannot be driven from another build system on x86 either. That closes the J2 half of this entry
> structurally rather than by leaving it to the project file.
>
> The `Win32Proj` keyword in the globals property group is unrelated to the platform — it identifies the
> project *type*, and is what Visual Studio emits for an x64 console application — so it stays.

`Debug|Win32` and `Release|Win32` (`CPU.vcxproj:85–112`) lack `LanguageStandard`, `IncludePath`, `TargetName`,
`EnableEnhancedInstructionSet` and every per-file override. Selecting either in the IDE produces a cascade of
confusing errors (`_mm_set1_epi64x` and the 512-bit intrinsics are x64-only) and, if it linked, would emit
`CPU.exe` rather than `PITC.exe`. The missing `LanguageStandard` also drops the C++20 guarantees the ALU
kernels now depend on (J2). `CLAUDE.md` documents them as vestigial — they should simply be deleted.

### H8. Machine-specific include path and missing header entries — **Low**

`CPU.vcxproj:76,80` hard-code `D:\Programming\include`, a path that exists only on the author's machine. It is
not load-bearing — an identifier sweep of the PITC-proper sources resolves every symbol against either the six
vendored headers or the Windows SDK and CRT (`PopulationCount64`, for instance, is `winnt.h`'s macro for
`__popcnt64`) — so the entry is a stale leftover that silently makes the build look machine-dependent when it
is not. Removing it would prove the repository copies are self-sufficient. The six vendored files (`typedefs.h`,
`memory management.h`, `common functions.h`, `vector structures.h`, `class_timers.h`, `SIMD management.h`) are
absent from both the `ClInclude` list (`CPU.vcxproj:182–188`) and `CPU.vcxproj.filters`, so they do not appear
in Solution Explorer and are easy to edit by accident or miss in review.

Note also that `<EnableEnhancedInstructionSet>StreamingSIMDExtensions2</EnableEnhancedInstructionSet>`
(`CPU.vcxproj:119,139`) emits `/arch:SSE2`, which MSVC rejects as unknown on x64 (SSE2 is the unconditional
x64 baseline). The setting is therefore a no-op that produces a build warning; the per-file *raises* to
AVX2/AVX-512 are what actually do the work described in `CLAUDE.md`.

### H9. The project can never grow a second translation unit — **Medium**

`CPU.h:117–124` defines six objects at namespace scope in a header, five of them through `declare1d64z` /
`declare2d64z` (which expand to `dataType *const name = (dataType *)salloc(…)`), plus `al64 CLASS_TIMER timer`
and `GLOBAL_CFG cfg`. `translations.h:13–15` defines three more. None is `inline`, `static` or `extern`.

Including `CPU.h` from any second `.cpp` therefore either fails to link with duplicate-symbol errors or — for
the `*const` cases, which have internal linkage — silently gives each translation unit its *own* copy of
`value[][]`, `threadData[]` and `threadBits[]`, so worker threads and `wmain` would compare and signal through
different arrays.

* **Impact:** the entire program is locked into a single translation unit. Any refactor that moves the
  `JobCycle` wrappers into their per-ISA units (the fix suggested for H4) hits this first.
* **Fix:** declare the globals `extern` in `CPU.h` and define them once in `CPU.cpp`, or mark them `inline`.

### H10. `#pragma warning(disable:4996)` suppresses every deprecation diagnostic in `CPU.cpp` — **Low**

`CPU.cpp:12` disables C4996 for the whole translation unit. That is what allows the legacy two-argument
`swprintf` (C7), `wcstombs` (F6) and `setlocale` to compile quietly — the three call sites that this audit
finds to be genuinely unsafe. The suppression should be narrowed to the specific calls that need it, with a
`// RULE-DEV:` comment per GCS en3, so that the compiler can keep warning about the rest.

---

## I. Vendored shared headers

These are copies of the author's general-purpose library rather than PITC-specific code, so fixes may need to
be carried upstream. Only defects affecting PITC, or that PITC's build would expose, are listed.

### I1. The pre-AVX2 `_mm_abs_pd` fallback uses an AVX-512 instruction — **Medium**

`SIMD management.h:24–26`:

```cpp
#if !defined(__FMA__) && !defined(__AVX2__)
#define _mm_abs_pd(input)  (fl64x2&)_mm_and_epi64(_mm_set1_epi64x(0x07FFFFFFFFFFFFFFF), (si128&)(input))
```

The guard selects this definition precisely when AVX2 is *not* available, yet `_mm_and_epi64` is an AVX-512VL
intrinsic. Any translation unit including this header and using `_mm_abs_pd` on a pre-AVX2 build would emit an
AVX-512 instruction. `CPU_jobs_SSE.cpp:16–18` defines its own with `_mm_and_pd` and does not include this
header, so PITC dodges it — but the two definitions differ and which applies depends on include order (J4).

### I2. `#ifndef` guards cannot detect an intrinsic, so the FMA macros always win — **Medium**

`SIMD management.h:29–45`:

```cpp
#ifndef _mm_fmadd_ps
#define _mm_fmadd_ps(a, b, c) _mm_add_ps(_mm_mul_ps((a), (b)), (c))
#endif
```

`_mm_fmadd_ps` is a *function* declared by `<immintrin.h>`, not a macro, so `#ifndef` is always true and the
macro is always defined. Every subsequent use in the translation unit — including a genuine call intended to
emit `VFMADD` — silently expands to a separate multiply and add with different rounding. PITC does not call
these today, but `memory management.h` → `common functions.h` → `SIMD management.h` pulls the macros into
`CPU.cpp`'s preprocessor state.

### I3. `CLASS_TIMER::dGrandTotal` accumulates the running total, not the elapsed time — **Medium**

`class_timers.h:60`:

```cpp
dGrandTotal += (dTotal += (dElapsed = (fl64(siElapsedTics) * dScale) / fl64(siFrequency)));
```

`dTotal` is already a running total, so adding it to `dGrandTotal` on every update makes the latter grow
quadratically in the number of updates. It should be `dGrandTotal += dElapsed`. `CLASS_TIMERS` repeats the
pattern at `:170,186`.

### I4. `CLASS_TIMERS::Update` accumulates elapsed time twice — **Medium**

`class_timers.h:169–170` runs the identical computation twice and increments `dTotal` twice per update, so it
advances at double real time. `UpdateAll` has the same doubled pair at `:185–186`, never updates `siPrevTics`
(so `siElapsedTics` grows without bound), and redundantly assigns `siCurrentTics` twice at `:180–181`. PITC
uses `CLASS_TIMER`, not `CLASS_TIMERS`, so it is unaffected today.

### I5. `(fl64x2&)_mm_set1_epi64x(...)` binds a mutable reference to a temporary — **Medium**

`CPU_jobs_SSE.cpp:17`, `CPU_jobs_AVX.cpp:17` and `SIMD management.h:26,28` cast an rvalue intrinsic result to a
non-const lvalue reference. MSVC permits this as a language extension; it is ill-formed under `/permissive-`,
which `ConformanceMode=true` (`CPU.vcxproj:118,138`) requests. Use a named local or `_mm_castsi128_pd`.

### I6. `CLASS_TIMER::Reinitialise` leaves several members uninitialised — **Medium**

`class_timers.h:46–53` never assigns `dScaleAccel`, `dGrandTotal`, `siPrevTics`, `siElapsedTics`, `siPauseTics`
or `_dScaleAccelTemp`. PITC's `timer` is a namespace-scope object, so it is zero-initialised first and the
omission is invisible; a stack-allocated `CLASS_TIMER` would read indeterminate values in `Update`
(`dScale += dScaleAccel`) on its first call. `Reset` (`:89`) additionally assigns the *time scale* to
`dScaleAccel`, which is almost certainly wrong, and the `dScale` clamp at `:59` snaps any negative scale back
to `dScaleConst`, making `Pause`/`Freeze` ineffective.

### I7. `malloc` is overloaded in the global namespace — **Low**

`memory management.h:214` declares `inline ptrc malloc(csize_t numBytes, csize_t alignment)`. Adding an
overload of a reserved C library name at global scope is undefined behaviour by the standard; it works on MSVC
but will not survive a toolchain change. A distinct name (`AlignedAlloc`) would cost nothing.

### I8. `max` and `min` are function-like macros in a public header — **Low**

`class_timers.h:15–21` defines them, and `CPU.cpp:653` relies on `max`. Any later `std::max` / `std::min` use,
or an `<algorithm>` include after this header, breaks. GCS r12 also reserves `UPPER_SNAKE` for macros.

### I9. `va_arg` with a const-qualified type — **Low**

`memory management.h:369` — `pointer = va_arg(val, ptrc)` where `ptrc` is `void *const`. Passing a
const-qualified type to `va_arg` is undefined behaviour.

### I10. `AllFalse(cui512&, cui512&)` is implemented with AVX2 — **Low**

`common functions.h:185–188` splits the 512-bit operand into two `_mm256_testz_si256` calls, so
`ThreadsRunningAVX512` (`CPU.h:145`) — selected specifically because the CPU supports AVX-512 — executes AVX2.
Functionally correct, but it contradicts the stated intent that "the poll itself uses available SIMD".

### I11. Every `#ifdef __AVX512__` branch in `memory management.h` is dead code — **Medium**

`memory management.h` forks on `__AVX512__` in nine places (`:176,385,410,422,447,486,538,584,651`). **No
compiler defines that macro.** MSVC's `/arch:AVX512` defines `__AVX512F__`, `__AVX512BW__`, `__AVX512DQ__`,
`__AVX512VL__` and `__AVX512CD__`; GCC and Clang define the same per-feature macros. The correct spelling is
`__AVX512F__`.

* **Impact:** every AVX-512 path in the allocator, `mzero`, `mset` and the copy/stream families is unreachable,
  so the header silently runs its AVX2 (or SSE) arm even on AVX-512 builds. The file's own to-do list
  (`memory management.h:14`) plans to replace these forks with run-time dispatch, which is the right answer —
  but until then they should at least be spelled correctly, or removed as documented dead code.
* Related: the surrounding `#ifdef __AVX__` forks (e.g. `:152,188,388`) select *different bodies for the same
  inline functions* depending on each translation unit's `/arch` setting. PITC is safe today only because
  `memory management.h` reaches exactly one translation unit (H9); the moment a second one includes it with a
  different `/arch`, those functions violate the one-definition rule.

### I12. Latent defects in the parts of `memory management.h` that PITC does not call — **Medium**

None of `Copy*`, `Stream*`, `mzero`, `mset`, `LockedCopy`, `LockedSwap` or `LockedMoveAndClear` is referenced
by PITC, so nothing here is live today — but the header is shared, and these are worth carrying upstream:

* `Stream()` (`:724`) rounds the byte count **up** before delegating to `Stream16/32/64`, overrunning the
  destination by up to 63 bytes. `Copy64`'s volatile overload rounds up while its `const` overload rounds down
  (`:668`) — the header's own to-do item 4 acknowledges the divergence is "documented but unresolved".
* `salloc`'s wide-pattern overloads (`:296,312,328`) issue 128/256/512-bit stores regardless of the alignment
  they were asked to allocate at, so `salloc(n, 16u, null512)` faults. PITC's `declare*z` macros happen to pair
  a 64-byte alignment with a 128-bit pattern, which is safe.
* `mzero` (`:388`) picks its store width from the byte *count* rather than the address alignment, so a
  64-byte-multiple length at a 16-byte-aligned address selects an aligned 512-bit store.
* `RoundUpToNearest*` (`common functions.h:191–210`) wraps to 0 for inputs above `2^64 - 64`, with no guard,
  and it sizes every `declare*`/`zalloc*` allocation — an overflow there yields a zero-byte buffer the caller
  treats as full size. Note this does **not** reach the arena: `malloc64` (`memory management.h:46`) passes
  its byte count straight to `_aligned_malloc`, so `cfg.allocMem[0]` never touches this code. The only PITC
  call that does is `zalloc1d64(si64, cfg.sys.vCoreCount)` at `CPU.cpp:93`, about 2^61 short of wrapping.
* `mfree(pointer, ...)` (`:350`) expands to `mdealloc_(p, , -1ll)` when given a single pointer — a syntax error
  under a conforming preprocessor. PITC's only call passes two.
* `mdealloc_` (`:366`) stops reporting results past 64 pointers as `ptrBit` shifts out, and the
  `#pragma intrinsic(_InterlockedExchange64)` at `:31` precedes any declaration of that function.
* `_mm_PB64_mm_` (`:539`) is an unhygienic macro that captures `bitPattern64` from the caller's scope.
* The `DATA_TRACKING` paths reference `MemTrack`, `MemUntrack` and `sysData` with no corresponding include, so
  that configuration cannot compile as shipped — and the prolog's `Thread-safety: Reentrant` claim does not
  hold for the tracking table.

### I13. Assorted defects in `typedefs.h` and `vector structures.h` — **Low**

* `typedefs.h:36` — `#define al1 __declspec(align(2))`. `al1` should be `align(1)`; as written it is a
  duplicate of `al2`. Not used by PITC.
* `typedefs.h:44` — `#define vol volatile` leaks a lowercase object-like macro into every translation unit that
  includes it, which is all five.
* `typedefs.h:117` — eleven derived `fl80` aliases exist with no base `fl80` alias; `:193` has a comment
  describing the wrong type; `:551` declares size constants as bare `int`, contrary to GCS r1.
* `vector structures.h` — several unions do not have the size or lane coverage their names imply
  (`SSE8Df16` at `:628` mixes IEEE fp16 with bfloat16 at twice the named width; `AVX8Ds64` at `:780` exposes
  `ymm`/`xmm` members covering half the `zmm`; `AVX64Du8`/`AVX64Ds8` at `:864` name only 32 of 64 bytes; four
  signed unions at `:532` expose their payload through unsigned `VEC` members). None is used by PITC.

### I14. Inline changelog blocks remain in two headers — **Low**

GCS rule c1 forbids history in file prologs and c2 requires a root `CHANGELOG.md`. `typedefs.h` and
`vector structures.h` still carry inline changelogs, and no `CHANGELOG.md` exists (L1). The vendored headers
also use the newer GCS prolog format while the PITC-proper files use the older boxed-comment style, so two
conventions are live in one tree.

---

## J. Job kernels

### J1. The right-shift arm of every ALU chain is dead code — **High** — *Fixed*

> **Fixed** with the predicate `i < 8`, so the shift reverses direction halfway through the loop and both
> arms execute. Every ALU variant in all four `CPU_jobs_*.cpp` files is affected. This changes every golden
> value, so `cpu.values` must be regenerated — which the header's `KernelFingerprint` now enforces rather
> than trusts: a file left over from the old kernels is rejected with `-21`.
>
> Verified behaviourally rather than by reading the source. The kernels were compiled off-target and their
> output compared against two reference models of the chain, one carrying the intended `i < 8` bound and
> one the old `i < 32`. `JobALU` and `JobMemALU` match the first on 500 of 500 seeds and the second on 0 of
> 500, which is the evidence that the right-shift arm now executes. Reading the disassembly is *not*
> sufficient here: unsigned division by a constant lowers to a multiply-high and a `shr`, and signed
> division to a `sar`, so both spellings show shift instructions whichever arm is live.

Every ALU kernel contains, inside `for(ui8 i = 0; i < 16; ++i)`:

```cpp
y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
```

`i` never exceeds 15, so `i < 32` is always true and the `y >> 1` branch is unreachable. This appears in
`CPU_jobs_standard.cpp:21–24,48–54,65–75,101–116`, `CPU_jobs_SSE.cpp:38–44,81–111`, `CPU_jobs_AVX.cpp` and
`CPU_jobs_AVX512.cpp:39–48,102–147`.

* **Impact:** for a tool whose purpose is to *exercise* the integer unit, an entire instruction class (`SAR`)
  is never executed; the compiler folds the ternary away, so the loop also does less work per iteration than
  the source implies, which skews the benchmark. The evident intent was to alternate shift directions across
  the loop — the constant 32 suggests the bound was once 64.
* **Fix:** decide the intended predicate (e.g. `i < 8`) and regenerate `cpu.values` — this changes every golden
  value, so do it together with J2.

### J2. The ALU chains' determinism rests on C++20 semantics that one configuration does not request — **Medium** — *Fixed*

> **Fixed** as this entry asks: the chain arithmetic is performed in `ui64` and reinterpreted only for
> output. Each kernel binds an alias over the value it was handed — `ui64 &v = (ui64&)x;` in the register
> variants, `ui64ptrc v = (ui64ptrc)x;` in the memory ones — so the multiply, both shifts, the XOR and the
> closing subtraction are modular by definition rather than by language mode. Aliasing is what the memory
> variants need: re-typing the arena in place keeps every operand a load and a store, where copying the
> records into locals would have quietly removed the memory traffic those kernels exist to generate.
> Accessing a `si64` object through a `ui64` lvalue is permitted — they are the signed and unsigned
> variants of one type — so the alias is not a strict-aliasing violation.
>
> The XOR operand becomes `~0ull` with the type change; it is the same bit pattern `-1` was.
> `LanguageStandard` stays `stdcpplatest` on both surviving configurations, but nothing in the ALU chain
> now depends on it, and the `Win32` configurations that did not set it are gone (H7). The signed
> right-shift the entry does not mention — also implementation-defined before C++20 — is a logical shift
> now that the value is unsigned, and J1 made that arm reachable for the first time.

`x` and `y` are `si64`, and the chains rely on wraparound behaviour throughout:

* `y *= 789ull / 13 + 501;` — the `ull` suffix makes the right operand `unsigned long long`, so `y` is
  converted to `ui64`, the multiply wraps (well-defined), and the result is converted back to `si64`. That
  narrowing conversion was *implementation-defined* before C++20 and is only guaranteed modular from C++20 on.
* `y << 1` on a negative `si64` was *undefined behaviour* before C++20; C++20 defines it as modular.
* `… / 7 - 294939` — the final signed subtraction can still overflow, which is undefined in every version.

`LanguageStandard` is set to `stdcpplatest` for `Debug|x64` and `Release|x64` (`CPU.vcxproj:120,140`), which
supplies the needed guarantees — but the `Win32` configurations do not set it at all (H7), and the whole
golden-value model silently depends on a project setting that is not documented as load-bearing.

* **Fix:** perform the chain arithmetic in `ui64` throughout and reinterpret only for output, so the result is
  well-defined regardless of language mode; state the dependency in `CLAUDE.md`.

### J3. The "SSE4.1" and "AVX2" units execute no instruction above SSE2 and AVX1 — **Medium**

`CPU_jobs_SSE.cpp` uses only `_mm_div_pd`, `_mm_sqrt_pd`, `_mm_set1_pd`, `_mm_add_pd`, `_mm_sub_pd`,
`_mm_and_pd`, `_mm_mul_pd` and `_mm_set1_epi64x` — every one of them SSE2. `CPU_jobs_AVX.cpp` uses only the
`_mm256_*_pd` equivalents, all AVX1; it contains no AVX2-only (integer or gather) instruction at all. Only
`CPU_jobs_AVX512.cpp` genuinely uses its declared ISA (`_mm512_abs_pd` is AVX-512F).

* **Impact:** `I s` is documented and gated as "SSE4.1" (`en-GB.h:30`, `CPU.cpp:449`) and `I v` as "AVX2"
  (`CPU.cpp:450`), but neither kernel exercises those instruction sets — the only SSE4.1/AVX instructions in
  the program are the `PTEST`/`VTESTPD` used for result comparison, which A1 and A2 show are themselves wrong.
  An AVX1-only CPU (Sandy Bridge, Ivy Bridge, Bulldozer) is refused by the `PF_AVX2_INSTRUCTIONS_AVAILABLE`
  gate even though the kernel would run on it unchanged.
* **Fix:** either exercise the advertised instruction sets in the kernels, or relabel the units and relax the
  feature gates to match what the code actually needs.

### J4. `_mm_abs_pd` / `_mm256_abs_pd` are defined in three places with `#ifndef` guards — **Medium**

`CPU_jobs_SSE.cpp:16`, `CPU_jobs_AVX.cpp:16` and `SIMD management.h:25,27` each define these macros under
`#ifndef`, and the `SIMD management.h` versions differ from the kernel-local ones (I1). Which definition wins
depends on include order, so the same source can compile to different arithmetic in different translation
units. `UNLOOPx4` has the same triple-definition pattern (`common functions.h:70` plus each kernel unit) —
benign there because the definitions agree, but the pattern is fragile in a codebase whose correctness is
defined by bit-exact reproducibility.

### J5. `abs()` on a `double` depends on the global `<cmath>` overloads — **Low**

`CPU_jobs_standard.cpp:34–37,47–54,84–116` call `abs(1.0 - …)` with only `<cmath>` included and no
`using namespace std`. MSVC's `<cmath>` does inject the floating-point overloads into the global namespace, so
the right one is selected — but if the include set ever changes such that only `<stdlib.h>`'s `int abs(int)` is
visible, the argument is truncated to an integer, every FPU result changes silently, and the failure presents
as a CPU fault. Use `fabs` or `std::abs`.

### J6. `Job*` and `JobMem*` correctness is maintained entirely by hand — **Low**

The eight function families are duplicated across four translation units with no shared implementation — 16
functions whose arithmetic must stay bit-identical, spelled out operation by operation (a diff of the current
sources shows they *are* consistent today: the four ALU constant groups `789/13+501 … /7 −294939`,
`791/14+502 … /9 −294941`, `789/13+501 … /7 −294939`, `787/11+500 … /5 −294937` appear in the same order in
every variant, and the trailing post-loop multiply matches). Nothing enforces that, and B5 shows `W` does not
check it either. A single shared macro or template would remove the class of defect entirely.

### J7. The floating-point kernels absorb most single-ULP transient faults — **Medium** — *Fixed*

> **Fixed** by the mechanism this entry proposes — every intermediate folded into a running accumulator —
> after re-measuring the chain to find out where the sensitivity is actually lost.
>
> It is not lost gradually. It is lost at the *first* rounding after the fault. Each step opens with
> `sqrt(sqrt(x / c))`, and the fourth root divides a relative perturbation by four, so a one-ULP fault is
> quantised away before anything downstream can act on it. Everything after that root is strongly
> *expansive*: the `1.0 - t` cancellation gives the settled orbit a measured relative gain of about 5.7 per
> four lines, and a perturbation that survives the first root is never absorbed afterwards — it grows
> monotonically to the end of the run. So the 4-in-5 absorption is one rounding, not a slow decay, and no
> amount of downstream amplification can recover it.
>
> The accumulator therefore has to reach the result without passing through a root. `acc` starts at 1.0,
> takes `acc = (acc + x) / (|acc - x| + 0.01)` after every step, and is folded into the value once, at the
> end, with `x *= acc`. Its own shape is expansive rather than contracting — the `|acc - x|` cancellation
> is the same amplifier the value chain already relies on — so a perturbation entering it grows instead of
> decaying, and it issues no instruction class the kernels did not already use (add, subtract, abs,
> divide). It is applied identically in all eighteen kernels, and the memory variants carry one accumulator
> per record.
>
> Re-measured with this entry's own experiment — one bit flipped at each of the 256 chain steps, over many
> seeds of the form `W` generates:
>
> | Kernel | Flipped bit | Before | After |
> |--------|-------------|-------:|------:|
> | FPU | mantissa LSB (1 ULP) | 21.2 % | **61.8 %** |
> | FPU | mantissa bit 1 | 41.1 % | **79.4 %** |
> | FPU | mid mantissa (bit 26) | 100 % | 100 % |
> | FPU | mantissa MSB (bit 51) | 100 % | 100 % |
> | FPU | exponent (bit 62) | 100 % | 100 % |
>
> The 21.2 % baseline is this entry's 18.9 %, re-measured over a different seed set. The remaining 38 % is
> not reachable while the nested root stays: the perturbation has to survive one gain-¼ rounding before any
> accumulator can see it, and eight other accumulator shapes — reciprocal, multiplicative, ratio-coupled,
> cascaded — measured between 21 % and 39 %. Removing the root would buy the rest at the cost of the
> latency-bound property this entry warns against trading away.
>
> Numerical safety decided the constant. Over 5,000,000 seeds `acc` stays within `2^0 … 2^10.8` and the
> result within `2^-1.9 … 2^17.1`, roughly 60x clear of the double exponent range, and no seed yields a
> zero, an infinity or a NaN — any of which the AVX-512 path's floating-point comparison (**A11**) would
> mishandle. Detection is flat at 58–62 % for guard terms from `1e-5` to `1e-1`, so the choice is not
> perched on a tuned value; `0.01` gives the tightest magnitude bound of those.
>
> The SSE, AVX2 and AVX-512 kernels still reproduce the scalar `JobFPU` chain element-wise, bit for bit, so
> a `cpu.values` written on one vector width remains valid on another. This changes every golden value:
> regenerate with `W`.

Each FP step is `x = sqrt(k) / (abs(1.0 - sqrt(sqrt(x / c))) + e)` (`CPU_jobs_standard.cpp:34–37`, and the
same arithmetic in the SSE, AVX2 and AVX-512 units). The nested `sqrt(sqrt(...))` is a fourth root, whose
derivative damps a *relative* perturbation by roughly 4× each time it is applied. A one-ULP error injected
mid-chain is therefore usually driven back below the rounding threshold by the steps that follow it, and
vanishes before the comparison ever sees it.

I transcribed `JobFPU` and `JobALU` into IEEE-754 double arithmetic and injected a single bit flip at each of
the 256 chain steps, across 12 random seeds of the form `W` generates, measuring how often the flip changes
the final compared value:

| Kernel | Flipped bit | Faults detected | Faults silently absorbed |
|--------|-------------|----------------:|-------------------------:|
| FPU | mantissa LSB (1 ULP) | **18.9 %** | **81.1 %** |
| FPU | mid mantissa (bit 26) | 100 % | 0 % |
| FPU | mantissa MSB (bit 51) | 100 % | 0 % |
| FPU | exponent (bit 62) | 100 % | 0 % |
| ALU | integer LSB | 99.7 % | 0.3 % |
| ALU | mid word (bit 31) | 100 % | 0 % |

* **Scope:** this affects only the smallest perturbation class. Any error beyond one ULP is caught reliably,
  and a *persistent* fault (a stuck bit, a bad divider) corrupts every iteration and will show. What is
  largely invisible is the single-event upset in the low mantissa bit — which is precisely the silent-data-
  corruption signature that idle-stability testing exists to find, and the ALU kernel catches it 5× more
  often than the FP kernels do.
* **Interaction:** this is the *floor* on FP sensitivity after **A1** and **A2** are fixed. Until they are,
  the AVX2 figure is 0 % for every bit position, not 18.9 %.
* **Caveat:** measured in Python, whose `sqrt` and division are IEEE-754 correctly rounded as MSVC's are; the
  trailing `x * (x * 1.0101… + 0.00021)` was not FMA-contracted, which does not affect sensitivity. Worth
  re-measuring against the real binary before acting.
* **Fix:** if transient-fault sensitivity matters more than the current arithmetic's latency-bound property,
  add a step that is expansive rather than contracting — accumulating each intermediate into a running
  checksum lane would preserve every perturbation without changing the instruction mix.

---

## K. Documentation and localisation

### K1. The exit-code table is wrong — **Medium** — *Fixed*

> **Fixed** alongside A4–A6, which added three codes of their own. The second `-8` is now `-9`, and `-11`
> (unsupported vector unit), `-12` (more than one thread synchronisation option), `-13` (test duration of
> zero or less) and `-14` (pulse on-time of zero) are documented. `README.md` still carries no table (K6).

`en-GB.h:13–24` lists `-8` twice and never mentions `-9` or `-11`:

| Documented | Actual (`CPU.cpp`) |
|-----------|--------------------|
| `-8 : Invalid filename for results file` | `-8` ✔ (`:204`) |
| `-8 : Unable to create results file` | **`-9`** (`:485`) |
| `-10 : Failed to write results file` | `-10` ✔ (`:664,671`) |
| *(absent)* | **`-11` : requested vector unit unsupported** (`:449–451`) |

`README.md` omits the table entirely.

### K2. Presets 8 and 9 have "Synchronised" on the wrong one — **Medium**

`en-GB.h:59–60` and `README.md:51–52` say preset 8 is "Synchronised staggered" and preset 9 is plain
"Staggered". The code is the other way round: `case L'8'` sets `procSync = 0x024` (staggered + fixed pulse, no
`T-Sync` bit) and `case L'9'` sets `0x02C` (staggered + `T-Sync` + fixed pulse) — `CPU.cpp:417–430`. The inline
comments in `CPU.cpp` are correct; the user-facing text is not.

### K3. The documented `O[…]` example does not do what it says — **Medium**

`en-GB.h:35` / `README.md:21` give `O[results.txt]16` as the way to write a UTF-16 file. Because of F5 the `16`
is skipped and the file is written as ASCII; and if the encoding *is* selected, F3 truncates the file to half
its length. Every documented spelling of this option is broken in one way or another.

### K4. The `Ux` and `Tx` option letters are described inconsistently — **Medium** — *Partly fixed*

> **Fixed:** the second and fourth bullets. `en-GB.h` and `README.md` now both read `(C,F,S)` and note that
> the last of `C`, `F` and `S` given is the one used, and the `Sx` line reads *"One of the first three
> options (P,R,S) can be stacked with the last (T)"* with `P, R and S are mutually exclusive` beside it —
> matching the parser, which now rejects the stack (A4). The first and third bullets (the `Ux` sub-option
> list and the unusable `Uc` examples) still stand, and depend on F2.

* `en-GB.h:44` — *"One of the first two options (C,T) can be stacked with the one of the remaining (S, O)"*.
  There is no `S` sub-option; the code implements `A`, `C`, `E`, `O`, `T` (`CPU.cpp:276–318`). `README.md:33`
  gets this right (`(A,E,O)`), so the two documents disagree with each other.
* `en-GB.h:39` / `README.md:27` — *"One of the first three options (C,F,T)"* immediately followed by
  *"C==Constant, F==Fixed-length pulses, S==Sweeping-length pulses"*. The list should read `(C,F,S)`.
* Both documents' `Uc` examples (`Uc!.!!...!s`, `Uc!.!!...!a`) cannot work, because the map consumes the rest of
  the argument (F2).
* `en-GB.h:37` advertises stacking `P`, `R` and `S`, which produces the silent full-idle run of **A4**.

### K5. Preset `-6` and `-7` cycle times do not match their `onTime` values — **Medium**

The help text describes the sweep option's `[x` as "Cycle duration" (`en-GB.h:43`), and presets `-6`/`-7` set
`onTime` to 2000 and 2500 ms as though those were the cycle. Because they never zero `offTime` (E7), the actual
cycles are 2900 and 3400 ms.

### K6. README omits the mandatory first step, the exit codes and how to build — **Medium**

`README.md` never mentions that `PITC.exe W` must be run before any test (without `cpu.values` every invocation
returns `-1`), carries no return-value table, and gives no build instructions — no toolchain version, no
`msbuild` line and no note that only `x64` is supported (the `Win32` configurations are selectable and broken —
H7). For a repository with no CI and no `.sln`, those are the first things a new contributor needs.

### K7. Version `1.0.2` appears only in prose — **Low**

`README.md:1` and `en-GB.h:12` state the version; there is no version constant, no `.rc` file and no
`VERSIONINFO` resource, so the built binary reports nothing. `CLAUDE.md` repeats the number, giving three
places to update by hand — and nothing ties a `cpu.values` file to the version that produced it (B6).

### K8. Typographical errors in user-facing text — **Low**

`en-GB.h:15` "Successul" → "Successful"; `en-GB.h:29` and `README.md:12` "intruction" → "instruction";
`CPU.cpp:411` preset 7's comment reads "30 minute duration. 10 minute duration"; `CPU.cpp:52` comments the L1
*data* cache branch as "Set (smallest) L1 code size".

### K9. Strings outside the translation tables — **Low**

`CPU.h:128–130` hard-codes `wstrUnitsCPU`, `wstrSyncCPU` and `wstrPass` (the last already flagged
`///--- Modify for translation ---///`). The failure-detail format strings inside `Failed()`
(`CPU.h:185–204`) and the table separators built at `CPU.cpp:622–628` are also literal. A second language
cannot be added without moving these into the tables.

### K10. `translations.h` cannot express a second language — **Low**

`translations.h` unconditionally includes `en-GB.h` and points the three globals at its tables. Adding a
language requires editing this header, adding a `<code>.h`, and extending the `L` case in `CPU.cpp` — and
fixing F7 first.

### K11. `CLAUDE.md` propagates two claims this audit contradicts — **Low**

* *"the global `EnableEnhancedInstructionSet` is `StreamingSIMDExtensions2` … This is what keeps AVX-512
  opcodes out of the baseline code path"* — see H4; the comparison intrinsics are in the baseline TU.
* Its "Known constraints and latent issues" list omits the false-pass defects (A1–A6), which are more severe
  than every item it does list. Worth updating once those are fixed.

### K12. The effective defaults are documented nowhere — **Low**

`CPU.cpp:82–86` establishes the configuration used when an option is omitted — a 15-minute duration, ALU + FPU,
parallel constant execution, a 2 000 ms start-up delay, and (from `CPU.h:43–45`) a 100 ms / 900 ms pulse if the
constant bit is later cleared. Neither `README.md` nor `en-GB.h` states any of these, so a user running
`PITC.exe Ia Tct5.0` cannot tell which settings they have implicitly accepted. The `-x` help line does say
presets default to "the ALU & the largest vector unit, and 8MB memory per core" — that much is documented; the
no-preset defaults are not. Given F1, knowing the default pulse shape matters: it is what a stray `T` option
silently falls back to.

### K13. Guild Coding Standard deviations — **Low**

`CONTRIBUTING.md` makes GCS v1.1.4 the review standard, and conformance is entirely manual (L1–L5). Observed
deviations, none of which affects behaviour:

* The 180-column hard cap (e2/r7) is exceeded in `CPU.cpp` (the AVX-512 results row at `:643`),
  `CPU_job_cycles.h` and `CPU_jobs_SSE.cpp`.
* r12 reserves `UPPER_SNAKE` for macros, tables and global constants; `JobCycle` (`CPU_job_cycles.h:492`),
  `RoundUpToNearest*` and `UNLOOPx*` (`common functions.h:70,190`) are PascalCase. `common functions.h:9`
  already carries this as a to-do.
* r4 requires exactly three spaces between same-line statements; single spaces are common in the kernel units.
* c1 forbids history in file prologs — see I14 — and all PITC-proper files still use the older boxed-comment
  prolog rather than the `Version:`/`Owner:`/`ISA:`/`Thread-safety:` form the vendored headers have adopted.
* Marking intentional deviations `// RULE-DEV:<rule-id> <why>` (en3) is not done anywhere, including for the
  two deliberate build settings `CLAUDE.md` documents.
* GCS mandates an AVX2 baseline with run-time dispatch one step above it (a8/a11); PITC supports an SSE-only
  path and forks at compile time per translation unit. That is a reasonable choice for this program, but it is
  an unmarked deviation from the standard the project claims to follow.

### K14. Declarations that misdescribe the program's actual behaviour — **Low**

* `CPU.h:46–47` gives `GLOBAL_CFG` in-class defaults of `procUnits = 0x04` (SSE) and `procSync = 0x02A`
  (parallel + synchronised + fixed pulse). `CPU.cpp:83–84` overwrites both before any argument is read, with
  `0x012` (parallel + constant) and `0x03` (ALU + FPU). The header's values are dead and describe a different
  program from the one that runs — a reader checking "what does PITC do by default" from the struct gets the
  wrong answer for both fields.
* `THREAD_CFG::packetSizeRAM` (`CPU.h:55`) is commented "Amount of RAM (in bytes) to utilise per pulse" but is
  written with the whole-run per-thread block size (`CPU.cpp:594`) and never read by anything.
  `THREAD_CFG::maxTics` (`:58`) is likewise written (`CPU.cpp:597`) and never read, and `records32` (`:67`)
  aliases only the low 32 bits of a 48-bit record count and is unused.

---

## L. Project process

Everything in this section is mandated by `CONTRIBUTING.md` → `GDC_GCS_v1_1_4.md` and is simply absent.

| # | Missing | Required by | Severity |
|---|---------|-------------|----------|
| **L1** | `CHANGELOG.md` at the repository root | GCS c2 | Low |
| **L2** | `.clang-format` and `.editorconfig` (the standard names `tc1`, `tc2`, `en1`, `en2`) | GCS | Low |
| **L3** | Pre-commit hooks and CI gates described by the standard | GCS | Low |
| **L4** | Any automated test — there is no `tests/` directory and no test target | GCS; `common functions.h:10` to-do | Low |
| **L5** | `.gitignore` — `x64/`, `*.obj`, `*.pdb`, `*.map` and `cpu.values` will all be offered for commit | — | Low |

Conformance to GCS v1.1.4 is therefore entirely manual, which is worth stating plainly in `CONTRIBUTING.md` so
contributors do not go looking for tooling that does not exist. A minimal CI job that builds both `x64`
configurations from a clean clone would be the cheapest addition here: it would keep the vendored headers
provably sufficient, catch the `Win32` breakage (H7), and give every finding in this document a way to be
verified by execution rather than by reading.

---

## Appendix A — command lines and configurations that misbehave today

| Command / configuration | Result |
|---------|--------|
| `PITC.exe W` on a 12- or 24-thread CPU | Writes a corrupt `cpu.values`; every later run reports `!Fail!` on threads 0–7 (**B1**) |
| `PITC.exe W` on a CPU without AVX2 | Always returns `-4`, "computational errors detected"; the file can never be created (**B2**) |
| `PITC.exe W` on a 9/17/25-thread CPU | Hangs forever (**B3**) |
| `PITC.exe Iv Tct60` (any AVX2 run) | Cannot fail, whatever the hardware does (**A1**) |
| `PITC.exe Ix Tct60` (any AVX-512 run) | Summary always prints `.Pass.` (**A3**) |
| ~~`PITC.exe Spr Tf[250]1750`~~ | ~~Threads never compute; prints `.Pass.` after idling~~ — rejected with `-12` (**A4**, fixed) |
| ~~`PITC.exe Tf[0]1000`~~ | ~~Zero work performed; prints `.Pass.`~~ — rejected with `-14` (**A5**, fixed) |
| ~~`PITC.exe Tt0` / `Tt-5` / `Tt`~~ | ~~Test ends before it starts; prints `.Pass.`, exit code 0~~ — rejected with `-13` (**A6**, fixed) |
| `PITC.exe Td0 Tt0.001` | Workers exit while the spawn loop is still running; the lost-update race hangs `wmain` (**D1**) — `Tt0` itself is now rejected (A6) |
| `PITC.exe O[]` | File output silently disabled — no file, no warning, exit code 0 (**F14**) |
| ~~`PITC.exe Tf[250]1750` vs `T[250]1750`~~ | ~~Identical runs; only the first claims `F-P` in the banner~~ — the second is now a Constant run, as its banner says (**E10** no longer observable, F1) |
| ~~`PITC.exe B Tt120` / `PITC.exe -1 Tt600`~~ | ~~Constant stress silently becomes a 10 % duty pulse~~ — mode preserved (**F1**, fixed) |
| `PITC.exe Iasv Mc8` (two vector units) | Wrong seeds; `!Fail!` on healthy hardware (**C1**) |
| `PITC.exe Ivx` | Reads past the end of `JobCycle[2][24]` (**A9**) |
| `PITC.exe I3 Mc8` | Writes ~64 MB into an 8 MB slice, then prints an empty results table (**A10**) |
| `PITC.exe` (no arguments) | Prints the help text, then blocks on `system("pause")` forever in any non-interactive context (**F15**) |
| `PITC.exe -6` / `-7` / any `Ts` run | Frequently hangs at the end of the test (**E1**) |
| `PITC.exe B Tt0.5` | Integer divide-by-zero in the KUPS calculation (**E4**) |
| ~~`PITC.exe Tst0`~~ | ~~Integer divide-by-zero in the sweep ramp~~ — rejected with `-13` (**E4**'s sweep half, fixed by A6) |
| `PITC.exe Lde-DE-formal` | Overflows `wstrLang[6]` (**C6**) |
| `PITC.exe O[results.txt]16` | Writes ASCII, not UTF-16 (**F5**) |
| `PITC.exe O16[results.txt]` | Creates a file named `results.txt]`, half-truncated (**F3**, **F5**) |
| `PITC.exe O8[results.txt]` | UTF-8 BOM followed by ANSI code-page bytes (**F6**) |
| `PITC.exe Uc!.!!...!` | Selects a different core set than written; cores 0–1 unreachable (**F2**) |
| `PITC.exe Mc65536` / `Mc-1` | Null-pointer crash with no diagnostic (**C5**) |
| `PITC.exe Mn8 Ua` / `Ms8 Ua` | Threads silently drop out of memory-backed mode (**C9**) |
| Any `Mc*` run with ALU + a vector unit | Each thread overruns its arena slice by up to 3 records (**C4**) |
| ~~`-1`/`-3`/`-4`/`-6`/`-8` on a CPU without SMT~~ | ~~Undefined shift in `SetSMTLoading`; zero threads, empty table, **exit code 0**~~ — one thread per physical core (**G1**, fixed) |
| ~~Any run on a hybrid P/E-core CPU~~ | ~~`groupCount` becomes 2 for a single populated group, so `SetSMTLoading` shifts by 64~~ — `groupCount` counts populated groups (**G4**, fixed) |
| ~~`-1`/`-3`/`-4`/`-6`/`-8` on a hybrid P/E CPU~~ | ~~Every E-core silently deselected~~ — every physical core selected, one thread each (**G2**, fixed) |
| ~~Any run on a hybrid P/E-core CPU~~ | ~~The SMT thread class re-uses the affinity mask of the non-SMT class, so every E-core goes untested while each P-core carries two threads~~ — one thread per core, on a core of its own class (**G5**, fixed) |
| Any run on a CPU with SSE2 but not SSE4.1 | Illegal instruction in `ThreadsRunningSSE` before the test starts (**D4**) |
| Debug build on a non-AVX-512 CPU | Illegal instruction in the scalar ALU/FPU path (**H1**) |

## Appendix B — suggested order of work

1. ~~**A1, A2, A3** — replace all three vector comparisons with true bitwise equality and fix `case 10:`.
   Nothing else matters until the tool can actually detect a fault.~~ **Done.**
2. ~~**A4, A5, A6, F1** — the false-pass family. Every one of these returns exit code 0 on an untested CPU;
   together they are the reason a green PITC run currently cannot be trusted.~~ **Done.** The three
   configuration faults are refused before any thread is created, and a `T` argument no longer discards the
   requested timing mode.
3. ~~**B1, B2, B3** — make `W` produce a complete, correct `cpu.values` on every topology and every ISA level.~~ **Done.**
4. ~~**A9, A10, C1** — reject or handle every `procUnits` combination the parser can produce.~~ **Done.**
5. ~~**E1, E2** — fix the sweep-mode hang and raise the timer resolution, so pulse timing means what it says.~~ **Done.**
6. ~~**C2, C3, C4, C5, C6** — the memory-safety set.~~ **Done.**
7. ~~**A7, D1, D2, D3, D5** — the concurrency set; A7 corrupts the report exactly when it matters most.~~ **Done.**
8. ~~**H1, H2, B6** — pin the floating-point model, stop the Debug/Release divergence, and version the
   `cpu.values` format so a toolchain change can never be mistaken for a hardware fault.~~ **Done.**
9. ~~**J1, J2, J7, B5** — decide the intended ALU shift behaviour, move the chains to unsigned arithmetic,
   consider whether the FP kernels' 1-ULP blindness is acceptable, and
   extend `W` to validate the `JobMem*` and `JobALU_*` kernels; regenerate golden values once, together.~~ **Done.**
10. ~~**G1, G2, G4, G6** — topology correctness~~ **Done**, with **G5** and **G7**. A hybrid CPU now runs one
    thread on each of its cores, which no release has done. Next: **G3** to lift the 64-core ceiling, then
    **G9** to stop calling E-cores "non-SMT" — the misnomer that hid G5's real severity — and **G8**, whose
    remaining half the sibling bitmaps G2 added make straightforward.
11. **F2–F15** — the parser and output set.
12. Documentation (**K**) and process (**L**) alongside the code changes they describe. A CI job that builds a
    clean clone is the highest-value single addition.
