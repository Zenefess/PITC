# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**PITC — Pulsed Integrity Tests for CPUs** (v1.0.2). A Windows x64 console application that verifies CPU
*computational integrity* rather than merely loading the CPU. Its distinguishing feature is **pulsed** testing:
bursts of compute separated by idle gaps, which stresses the C-state/voltage/frequency transitions that
constant-load stress tests never exercise. Traditional constant stress and a benchmark mode are also provided.

Single author project (David William Bull), MIT licensed, no dependencies beyond the Win32 API and MSVC intrinsics.

## Build

No `.sln` is checked in — build the project file directly with MSVC (VS 2022 / toolset v143):

```
msbuild CPU.vcxproj /p:Configuration=Release /p:Platform=x64   # -> x64\PITC.exe
msbuild CPU.vcxproj /p:Configuration=Debug   /p:Platform=x64   # -> x64\Debug\PITC.exe (ASAN enabled)
```

Build **x64 only**. The `Win32` configurations are vestigial: they lack the `LanguageStandard`, `IncludePath`
and `TargetName` settings the x64 configurations carry, and are not maintained.

Two build settings look like mistakes but are deliberate — **do not "fix" them**:

- `Release|x64` sets `<Optimization>Disabled</Optimization>` globally. The job kernels are deterministic
  arithmetic loops with no observable side effects; letting the optimiser at them collapses or reorders the
  very work being measured. The four `CPU_jobs_*.cpp` files override this with `Optimization=Custom`.
- The global `EnableEnhancedInstructionSet` is `StreamingSIMDExtensions2`, and each ISA translation unit
  raises it individually (`CPU_jobs_AVX.cpp` → AVX2, `CPU_jobs_AVX512.cpp` → AVX-512). This is what keeps
  AVX-512 opcodes out of the baseline code path on CPUs that cannot execute them.

## Running and verifying a change

There is no automated test suite, no CI, and no lint/format tooling in the repo. The functional smoke test is
the program itself. Run with no arguments to print the full option reference.

```
PITC.exe W                 # REQUIRED FIRST: generate the "cpu.values" golden file in the CWD
PITC.exe Ia Tct5.0 Ue      # 5-second ALU-only constant run, one thread per physical core
PITC.exe -1                # preset 1 (10-minute constant stress)
PITC.exe B                 # 60-second benchmark, prints a KUPS score
PITC.exe Iax Mc8 Spt Tfd2.0t60[250]1750 Ua   # AVX-512 + ALU, 8MB/core, synchronised parallel pulses
```

**`cpu.values` must be regenerated (`PITC.exe W`) after any change to a `Job*` kernel in `CPU_jobs_*.cpp`.**
The file stores the expected outputs of those exact kernels; edit a kernel without regenerating and every
thread reports `!Fail!`. `W` self-validates by re-running the kernels 65,535 times and refuses to write the
file on any mismatch.

Exit codes are meaningful and documented in `en-GB.h` (`wstrInstructions_English`): negative = error,
`0` = stability test completed, `1` = values file written, `2` = instructions displayed. `-11` … `-14` are the
pre-flight rejections in `wmain` — an unsupported vector unit, more than one `S` pulse shape, a test duration
of zero or less, and a zero pulse on-time. Add a code and the table in `wstrInstructions_English` and the
message in `wstrMessage_*` have to grow with it.

## Architecture

### The golden-value model

Everything hinges on one idea: a *job* is a fixed-length, fully deterministic arithmetic loop, so the same
input must produce a bit-identical output on every core, every time. Divergence means the silicon erred.

`cpu.values` holds two `RESULTS[MAX_THREADS]` blocks back to back: random seed inputs, then the outputs those
seeds produce. At startup `wmain` loads them into two of four parallel result planes, and worker threads
re-derive the outputs continuously and compare.

The four planes (`value[4][MAX_THREADS]`, declared in `CPU.h`) each carry a fixed role:

| Plane | Role at test time |
|-------|-------------------|
| `value[0]` | Input seeds (block 1 of `cpu.values`) — read-only during the run |
| `value[1]` | Per-thread working scratch; in memory-backed mode its `p0`–`p4` fields instead hold pointers into the RAM arena |
| `value[2]` | Expected outputs (block 2 of `cpu.values`) — the comparison reference |
| `value[3]` | Observed value, written only by `Failed()` on a mismatch; otherwise a copy of `value[2]` so the results table prints `.Pass.` |

`RESULTS` (`CPU.h`) is a 128-byte union whose 16 `ui64` lanes are ordered **widest unit first**:
`raw[0..7]` = AVX-512, `raw[8..11]` = AVX2, `raw[12..13]` = SSE, `raw[14]` = FPU, `raw[15]` = ALU.
`Evaluate(thread, unit)` derives its lane window arithmetically from that ordering
(`unit`: 0=AVX512, 1=AVX2, 2=SSE, 3=FPU, 4=ALU, -1=all) — the layout and the function must change together.

`GenerateValues` (`CPU.h`) fills that table under `W`, and carries two invariants a careless edit will break.
It splits the 512 entries across `vCoreCount` threads as `[t*q, (t+1)*q)` with the last thread absorbing
`512 % vCoreCount`, so the ranges must keep tiling `[0, 512)` exactly — a gap leaves seeds in the "expected
output" block and every healthy core then reports `!Fail!`. And its three ISA ladders (AVX-512, AVX2, SSE)
each have to transform all 16 lanes **exactly once**, in the same pattern in the generation and self-check
halves: `JobSSE`, `JobAVX2` and `JobAVX512` compute the same function element-wise, so one pass per lane is
what makes the golden block the same whichever ladder built it, and any disagreement between the two halves
makes the self-check fail unconditionally and `W` return `-4`.

### Job kernels: one translation unit per ISA

`CPU_jobs_standard.cpp` (ALU/FPU), `CPU_jobs_SSE.cpp`, `CPU_jobs_AVX.cpp`, `CPU_jobs_AVX512.cpp` each define
the same eight-function family, declared `extern` in `CPU.h`:

```
Job<UNIT>(x)              Job ALU_<UNIT>(x, y)         # register-resident
JobMem<UNIT>(ptr)         JobMemALU_<UNIT>(ptr, ptr)   # memory-array variants, 4 records per call
```

Every kernel is `for(i < 16) { UNLOOPx4( ...4 chained ops... ) }` — the `UNLOOPx4` macro is manual unrolling,
and the arithmetic is deliberately chosen to be latency-bound and irreducible (chained `sqrt`/divide for FP,
multiply/shift/xor/divide for integer). The `ALU_` variants interleave integer and FP ops to load both pipes
simultaneously. Adding a new unit means adding all eight functions plus its `JobCycle` entries.

### Dispatch: `JobCycle[2][24]`

`CPU_job_cycles.h` wraps each kernel in a `JobCycle*` function that seeds the working values from `value[0]`,
runs the kernel, compares against `value[2]`, and on mismatch records `value[3]` and calls `Failed()`.
These are indexed by a **function-pointer table** — `JobCycle[hasMemory][procUnits & 0x1F]` — resolved once per
thread in `ComputationPulse`, so the hot loop has no branching on configuration.

The table's shape encodes a real rule: **only the ALU bit and the widest enabled vector unit matter.** Index 6
(FPU+SSE) maps to the same `JobCycleSSE` as index 4; indices 8–15 all map to AVX2 variants; 16–23 to AVX-512.
The FPU path is only reached when no vector unit is selected.

### Thread lifecycle and scheduling

`wmain` (`CPU.cpp`) enumerates topology via `GetLogicalProcessorInformation`, applies the core map and SMT
policy (`SetSMTLoading` in `CPU.h`), allocates the arena, then `_beginthread`s one `ComputationPulse`
(`CPU_methods.h`) per selected virtual core and pins it with `SetThreadAffinityMask`.

Completion is signalled through `threadBits`, a global bitmap with one bit per thread. A thread clears its own
bit via `_InterlockedAnd8` when it exits; `wmain` spins on `ThreadsRunning()` — a reference bound at startup to
the widest of `ThreadsRunningAVX512/AVX/SSE` (`CPU.h`) so the poll itself uses available SIMD.

`ComputationPulse` implements the pulse shapes from the `procSync` bits before entering its loop:

- **Constant** (bit 4): `nextTic` is set to the end time, so the compute branch never yields.
- **Parallel** (bit 1): all threads pulse together.
- **Round-robin** (bit 0): thread *n*'s start is offset by *n* cycles and its cycle stretched by the thread
  count, so exactly one thread is active at a time.
- **Staggered** (bit 2): offset by `1 << (coreNum & 7)` cycles — a doubling ramp across each group of 8 cores.
- **Sweeping** (bit 6): the sleep duration is recomputed each cycle as a linear ramp across the test's total
  duration, continuously sweeping the duty cycle rather than holding it fixed.
- **Time-synchronised** (bit 3): suppresses the per-thread random start/period jitter (`offset[0..1]`) that is
  otherwise applied to deliberately desynchronise threads.

Bits 0–2 select *one* shape, so `wmain` rejects a `procSync` carrying more than one of them and substitutes
Parallel when it carries none. The shape `switch` in `ComputationPulse` accordingly treats every value other
than 1 (R-R) and 4 (Staggered) as Parallel, and adds `startTics` outside the `switch`: `nextTic` starts life as
a *duration* (`activeTics`), so any path that fails to add the start timestamp leaves the thread comparing a
QPC reading against a few million tics, permanently asleep — the whole-run idle of ISSUES.MD A4.

### Memory-backed mode and the arena

Requesting memory (`M`, or any preset) switches every thread from register-resident to `JobMem*` kernels
working over a RAM arena — this is what exercises load/store units and the cache hierarchy.

`wmain` allocates one 64-byte-aligned block (`malloc64`) for the whole run and hands each thread a slice via
`value[1][k].p0..p4`. The `switch(cfg.procUnits & 0x01F)` that computes `resArray.records[]` is the arena
layout: the divisor is the per-record byte cost of the selected units (8 for ALU-only, 40 for ALU+AVX2, 72 for
ALU+AVX-512, …), and for the `ALU_` combinations the ALU sub-array is placed *after* the vector sub-array by
advancing `resArray.alu`. Change the unit set or the record size and this switch must change with it.

### Configuration bit-fields

`cfg.procUnits` and `cfg.procSync` (`GLOBAL_CFG`, `CPU.h`) are copied verbatim into each `THREAD_CFG` and are
the decoder ring for most of the code:

```
procUnits  bit 0 ALU   1 FPU   2 SSE4.1   3 AVX2   4 AVX512   5 L1$   6 L2$   7 L3$
procSync   bit 0 R-R   1 Par   2 Stag     3 T-Sync 4 Constant 5 Fixed pulse  6 Sweep  7 Benchmark
```

Cache bits 5–7 are parsed and displayed but **not implemented** (the README and help text say so explicitly).
Benchmark mode (bit 7) additionally records each thread's iteration count into `resArray.iter` and prints a
KUPS score weighted by the vector width.

`procSync` bits 4–6 are the mutually exclusive timing modes, and each of `T`'s `C`/`F`/`S` sub-options clears
all three before setting its own — so the last one given wins, and a `T` argument that carries only a duration
or a start-up delay leaves the mode untouched. That is what keeps `B Tt120` and `-1 Tt600` constant-load runs
(ISSUES.MD F1); do not hoist the clear back out to the top of the option's parse loop.

## Shared headers vendored from an external library

`typedefs.h`, `memory management.h`, `common functions.h`, `vector structures.h`, `class_timers.h` and
`SIMD management.h` are copies of the author's general-purpose C++ library, not PITC-specific code. Two
consequences:

- They are **absent from `CPU.vcxproj`'s `ClInclude` list**, so they do not appear in Solution Explorer. The
  project's `IncludePath` also references `D:\Programming\include`, where the upstream originals live. Quoted
  includes resolve to the repo copies first, so local edits do take effect — but changes made here are local
  forks unless carried upstream.
- They carry the newer GCS-compliant file prolog (`Version:`/`Owner:`/`To Do:`/`ISA:`/`Thread-safety:`), while
  the PITC-proper files still use the older boxed-comment prolog. Both styles are live in the tree.

What they provide: the `ui8`/`si64`/`fl64x8` type-alias lattice and `defpa`/`refpa` pointer-array macros
(`typedefs.h`); aligned allocation and the `declare1d64z`/`zalloc64`/`mfree` family plus SIMD copy/stream
primitives (`memory management.h`); `UNLOOPx*`, `AllTrue`/`AllFalse`, `RoundUpToNearest*` and the `null128`/
`max512` constants (`common functions.h`); and the `QueryPerformanceCounter`-based `CLASS_TIMER` used for all
tic arithmetic (`class_timers.h`).

## Coding standard

`CONTRIBUTING.md` mandates **Guild Coding Standard v1.1.4**, specified in full in `GDC_GCS_v1_1_4.md`. All
submissions are reviewed against it. The standard describes pre-commit hooks, CI gates and `.clang-format`/
`.editorconfig` files (`tc1`, `tc2`, `en1`, `en2`) — **none of which exist in this repo**; conformance is
manual. The rules that bite most often when editing here:

- **3-space indent, no tabs** (r8). Lines ≤150 columns, hard cap 180 (e2, r7).
- **Braces attach**: `{` on the same line as the control statement *and* the function signature, exactly one
  space before it; `}` on its own line (r14, r15).
- **Encode width and sign in the type**: `ui32`, `si64`, `fl64` — never bare `int`/`float`/`double` (r1). New
  `f32`/`f64` aliases are banned outright (en2).
- **Put const/volatile and indirection in the typedef, not the identifier**: write `cui32ptrc x`, not
  `const ui32 *const x`. Leading `c` binds the pointee, trailing `c` binds the pointer (r2, t2). Do not mix
  raw `const T*` style with alias forms in one translation unit (t3).
- **Vector alias names are fixed**: `ui256`=`__m256i`, `fl64x4`=`__m256d`, `fl64x8`=`__m512d`, etc. (t1).
- `PascalCase` functions (r11); `UPPER_SNAKE` macros, tables and global constants (r12).
- **Spreadsheet-style column padding is required where it aids readability** (r3) — this is why declarations
  and assignments throughout the codebase are aligned into columns. Same-line statements are separated by
  exactly three spaces (r4).
- `///` for API docs with `@param`/`@return` tags, `//` for notes, `//==`/`//--` for grouping headers (r5).
  Disable >5 lines of code with `/* */`, fewer with `//` (r6).
- **No history in file prologs** (c1) — a root `CHANGELOG.md` is required (c2) but has not been created yet;
  `typedefs.h` and `vector structures.h` still carry inline changelog blocks awaiting migration.
- Mark intentional deviations `// RULE-DEV:<rule-id> <why>` (en3).
- Performance rules that shape this codebase specifically: explicit alignment-aware allocators with matching
  frees (p2); SIMD preferred with a scalar baseline retained, compile-time specialisation plus run-time CPUID
  dispatch, thread status exposed via atomics (p3); run-time dispatch only one step above the AVX2 baseline,
  no compile-time forks (a8, a11).

## Localisation

`translations.h` selects a language by pointing three globals at one header's string tables; `en-GB.h` is the
only implementation. Adding a language means writing `<code>.h` with `wstrInstructions_*`, `wstrMessage_*[18]`
and `wstrInterface_*[13]`, then extending both `translations.h` and the `L` case in `CPU.cpp`.

Note the `L` option's selection logic is inverted (`if(lstrcmpiW(...))` is truthy when the codes *differ*), so
every input currently resolves to English. Fix that when adding a second language. Some strings are still
hard-coded outside the tables — `wstrUnitsCPU`, `wstrSyncCPU` and `wstrPass` in `CPU.h` (the last is flagged
`///--- Modify for translation ---///`).

## Known constraints and latent issues

Scattered `///--- Modify to account for >64 virtual cores !!!` markers flag the main one. Verify against
current source before relying on any of these:

- **Single processor group / 64 virtual cores.** `MAX_THREADS` is 512 and the buffers are sized for it, but
  topology enumeration, the `U` core-map parsing and the affinity mask in `wmain` all assume one 64-bit mask.
  This is the top item on `CPU.cpp`'s To-do list.
- **`JobCycle` index overrun.** The table has 24 entries but is indexed by `procUnits & 0x1F` (0–31). Selecting
  AVX2 *and* AVX-512 together (`Ivx`, index ≥24) reads past the end. All shipped presets and `B` stay in
  range because they set exactly one vector bit; hand-built `I` strings can escape it.
- **`ThreadsRunningAVX`/`ThreadsRunningSSE` stride bug.** They index `threadBits[0],[2]` and `[0],[1],[2],[3]`
  where a 4-`ui64` (`si256`) and 2-`ui64` (`ui128`) stride requires `[0],[4]` and `[0],[2],[4],[6]`. Windows
  above the first ~320 thread bits go unchecked on non-AVX-512 CPUs. Masked today by the 64-core limit.
- **`Failed()` clears a whole byte** (`_InterlockedAnd8(threadByte, 0x0)`), not just the failing thread's bit,
  so one failure can make `wmain` believe up to 8 threads have finished while they are still running.
- Cache-targeting (`I1`/`I2`/`I3`) is accepted, displayed, and does nothing.

### Result comparison must stay bit-exact

`CPU_job_cycles.h` defines two `ResultsMatch` overloads (`fl64x2`, `fl64x4`) that XOR the computed and
expected vectors and test the difference against zero. Every SSE and AVX2 job cycle goes through them.
Do not substitute `_mm_testc_si128` or `_mm256_testc_pd` here, and do not reach for `AllTrue` in
`common functions.h`: `PTEST`'s `CF` is a *subset* test (a bit that should be 0 turning 1 passes), and
`VTESTPD` examines only each lane's sign bit — every job output is positive, so it can never fail. Both
spellings were live in the shipped code and made the SSE and AVX2 verdicts partly or wholly blind.
The AVX-512 path still compares with `_mm512_mask_cmpneq_pd_mask`, which is a floating-point rather than a
bitwise comparison — a separate known gap (ISSUES.MD A11), not the same defect.
