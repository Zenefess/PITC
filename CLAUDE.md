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

Build **x64 only** — and now that is the only thing the project file offers. The two `Win32` configurations
were deleted (ISSUES.MD H7); they had never carried the `LanguageStandard`, `IncludePath`, `TargetName` or
per-file ISA settings the x64 configurations do. `CPU_build.h`, included by all four kernel units and by
`CPU.h`, `#error`s if it is compiled for anything but x64, so the sources reject an x86 build however it is
configured — the kernels use `_mm_set1_epi64x` and the 512-bit intrinsics, and the topology, affinity and
arena code is written against 64-bit masks throughout. The `Win32Proj` keyword in the globals property
group is a project *type*, not a platform, and is what Visual Studio emits for an x64 console app; leave it.

Two build settings look like mistakes but are deliberate — **do not "fix" them**:

- `Release|x64` sets `<Optimization>Disabled</Optimization>` globally. The job kernels are deterministic
  arithmetic loops with no observable side effects; letting the optimiser at them collapses or reorders the
  very work being measured. The four `CPU_jobs_*.cpp` files override this with `Optimization=Custom`.
- The global `EnableEnhancedInstructionSet` is `StreamingSIMDExtensions2`, and each ISA translation unit
  raises it individually (`CPU_jobs_AVX.cpp` → AVX2, `CPU_jobs_AVX512.cpp` → AVX-512). This is what keeps
  AVX-512 opcodes out of the baseline code path on CPUs that cannot execute them. `CPU_jobs_standard.cpp`
  must never carry such an override — it holds the scalar ALU/FPU path that has to run on any x64 CPU — and
  now `#error`s if one is applied (ISSUES.MD H1).

Two settings are load-bearing for reproducibility and must stay pinned in **every** configuration:
`<FloatingPointModel>Strict</FloatingPointModel>` and `<FloatingPointExceptions>false</FloatingPointExceptions>`.
`/fp:strict` fixes the rounding and forbids the compiler from contracting `a*b + c` into an FMA — `JobFPU`
ends each outer iteration with exactly that shape — so it is what makes a `cpu.values` written by one build
valid under another. `CPU_build.h`, included by all four kernel units and by `CPU.h`, `#error`s under MSVC if
`_M_FP_STRICT` is not defined, and folds the model into the build ID stored in the file's header (H2).

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
The file stores the expected outputs of those exact kernels. This is now enforced rather than remembered: the
file's header carries a fingerprint of the kernels that produced it (`KernelFingerprint`, `CPU.h`), so a stale
file is rejected with `-21` and a message naming the cause, instead of reaching the comparison and making every
thread report `!Fail!` as though the silicon were at fault. `W` self-validates by re-running the kernels 65,535
times and refuses to write the file on any mismatch.

`W` also runs `ValidateKernelFamilies` (`CPU.h`) *before* it generates anything, and refuses with `-22` and the
name of the offending kernel if it fails (ISSUES.MD B5). That check is what holds the eighteen job kernels to
each other: `cpu.values` only ever records what the five register-resident kernels produce, so a `JobMem*` or
`JobALU_*` kernel that had drifted from its counterpart was compared against nothing, and every memory-backed
run reported the difference as a CPU fault. It runs one seed through every kernel the CPU can execute and
requires each memory-array and combined variant to reproduce its register-resident counterpart **byte for
byte** — `memcmp`, never an arithmetic comparison, for the reason the bit-exactness note at the end of this
file gives. Each `JobMem*` kernel is handed four records carrying the same seed and all four must come back
equal, so the record indexing is checked alongside the arithmetic. **Add a `Job*` family and it must be added
to `ValidateKernelFamilies`, to `wstrKernelName`, and to `JobCycle`.**

Exit codes are meaningful and documented in `en-GB.h` (`wstrInstructions_English`): negative = error,
`0` = stability test completed, `1` = values file written, `2` = instructions displayed. `-11` … `-18` are the
pre-flight rejections in `wmain` — an unsupported vector unit, more than one `S` pulse shape, a test duration
of zero or less, a zero pulse on-time, an `I` string naming no processing unit, an `I` string naming more
than one of FPU/SSE4.1/AVX2/AVX-512, a memory request the machine cannot satisfy (`-17`, shared by the
pre-flight size check and a failed `malloc64`), and a per-thread slice too small to hold one call's four
records (`-18`). `-19` is the one runtime failure that aborts a run: a worker thread that could not be
created or resumed, in either the test or the `W` path. `-20` is a `cpu.values` header that could not be
written; `-21` is a `cpu.values` this build will not read — bad magic, an unreadable or unsupported format
version, a different build or kernel revision, or contents that disagree with the hashes in the header —
one code shared by five messages, the way `-11` and `-17` already share theirs. `-22` is a job kernel that
disagrees with the register-resident kernel for its unit, raised by `ValidateKernelFamilies` under `W`.
Add a code and the table in `wstrInstructions_English` and the message in `wstrMessage_*` have to grow with
it.

## Architecture

### The golden-value model

Everything hinges on one idea: a *job* is a fixed-length, fully deterministic arithmetic loop, so the same
input must produce a bit-identical output on every core, every time. Divergence means the silicon erred.

`cpu.values` holds a 64-byte `VALUES_HEADER` followed by two `RESULTS[MAX_THREADS]` blocks: random seed
inputs, then the outputs those seeds produce. At startup `wmain` loads the two blocks into two of four
parallel result planes, and worker threads re-derive the outputs continuously and compare.

The header (`CPU.h`) is what stops the file's likeliest failure — being stale — from being reported as a CPU
fault. It carries a magic value, a format version, the block geometry, a build ID (the floating-point model
and record size), a fingerprint of the job kernels, and an FNV-1a hash of each block; `wmain` checks all of
them before the blocks are read, and every read and write is checked for length, because `ReadFile` reports
reaching the end of a file as success and `WriteFile` reports a partial write the same way (ISSUES.MD B6).
**Changing `VALUES_HEADER`, `RESULTS`, or `MAX_THREADS` means raising `VALUES_FILE_VERSION` with it.**

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
each have to transform all 16 lanes **exactly once**: `JobSSE`, `JobAVX2` and `JobAVX512` compute the same
function element-wise, so one pass per lane is what makes the golden block the same whichever ladder built
it. The generation half, the self-check half and the header's kernel fingerprint all call one shared
`RunGoldenLadder` (`CPU.h`) rather than repeating the ladder, because a disagreement between the generation
and self-check halves makes the check fail unconditionally and `W` return `-4`.

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

Three properties of that arithmetic are load-bearing, and all three are easy to undo by accident:

- **The ALU chain runs in `ui64`, not `si64`.** Each kernel binds an alias over the value it was given —
  `ui64 &v = (ui64&)x;`, or `ui64ptrc v = (ui64ptrc)x;` in the memory variants — and every step wraps. Only
  unsigned arithmetic is defined to wrap in every language mode; in `si64` the multiply's narrowing
  conversion and the left shift of a negative value were implementation-defined or undefined before C++20,
  so the golden values silently depended on `LanguageStandard` (ISSUES.MD J2). Do not "simplify" the alias
  away by copying the value into a local either — in the memory kernels that would delete the load/store
  traffic those kernels exist to generate.
- **The shift predicate is `i < 8`, against a counter that runs to 15**, so the shift reverses direction
  halfway through the loop. It was `i < 32`, which is always true, and the right-shift arm never executed
  (ISSUES.MD J1).
- **Every FP kernel carries a running accumulator**, `acc`, seeded to 1.0, updated as
  `acc = (acc + x) / (|acc - x| + 0.01)` after *every* step and folded in once at the end with `x *= acc`
  (one accumulator per record in the memory variants). It is not decoration: each step opens with
  `sqrt(sqrt(x / c))`, whose fourth root divides a relative perturbation by four and rounds it away, so
  without a path to the result that avoids a root, four single-ULP faults in five were absorbed and the
  tool reported `.Pass.` (ISSUES.MD J7). Removing it, or moving the fold inside the loop where a root
  follows it, restores the defect.

`RunGoldenLadder` depends on `JobSSE`, `JobAVX2` and `JobAVX512` computing `JobFPU` element-wise, bit for
bit — that is what lets a `cpu.values` written on one vector width verify on another. Any change to the FP
arithmetic must be made in all four units identically, and `W`'s `ValidateKernelFamilies` is what proves the
eighteen kernels still agree afterwards.

### Dispatch: `JobCycle[2][32]`

`CPU_job_cycles.h` wraps each kernel in a `JobCycle*` function that seeds the working values from `value[0]`,
runs the kernel, compares against `value[2]`, and on mismatch records `value[3]` and calls `Failed()`.
These are indexed by a **function-pointer table** — `JobCycle[hasMemory][procUnits & 0x1F]` — resolved once per
thread in `ComputationPulse`, so the hot loop has no branching on configuration.

The table's shape encodes a real rule: **only the ALU bit and the widest enabled vector unit matter.** Index 6
(FPU+SSE) maps to the same `JobCycleSSE` as index 4; indices 8–15 all map to AVX2 variants; 16–31 to AVX-512.
The FPU path is only reached when no vector unit is selected.

The table's 32 entries cover the whole `procUnits & 0x1F` index domain, and must keep covering it: the index
is not range-checked in `ComputationPulse`, so a short table is an indirect call through whatever follows it
(ISSUES.MD A9). `wmain` separately rejects an `I` string that names two of FPU/SSE4.1/AVX2/AVX-512, which is
what makes 24–31 unreachable in practice — but the entries stay, because that validation is the only thing
standing between a hand-built `I` string and a wild jump.

### Thread lifecycle and scheduling

`wmain` (`CPU.cpp`) enumerates topology via `GetLogicalProcessorInformation`, applies the core map and SMT
policy (`SetSMTLoading` in `CPU.h`), allocates the arena, then creates one `ComputationPulse`
(`CPU_methods.h`) per selected virtual core with `_beginthreadex` — **suspended**, so `SetThreadAffinityMask`
lands before the thread executes anything — and resumes it. `_beginthread` is not usable here: the CRT closes
*its* handle when the thread exits, so the handle must not be retained, let alone passed to an API. The
handle `_beginthreadex` returns belongs to `wmain`, which closes it after resuming. A creation or resume
failure aborts the run with `-19` rather than leaving a completion bit no thread will ever clear. Both thread
entry points are therefore `ui32 __stdcall` and end by returning rather than calling `_endthread`.

Completion is signalled through `threadBits`, a global bitmap with one bit per thread. **Every access to it
is byte-wide and interlocked**: a thread clears its own bit via `_InterlockedAnd8` when it exits, and `wmain`
sets a bit with `SetThreadRunning` (`_InterlockedOr8`) before spawning. A plain `|=` there was a lost-update
race against a worker already clearing a different bit of the same byte, which hung the poll below
(ISSUES.MD D1). `wmain` spins on `ThreadsRunning()` — a reference bound at startup to the widest of
`ThreadsRunningAVX512/AVX/SSE` (`CPU.h`) so the poll itself uses available SIMD. Those three read the *same*
64 bytes: a `ui64` array means a 512-bit view spans 8 elements, a 256-bit view 4 and a 128-bit view 2, so the
AVX2 poll steps `[0], [4]` and the SSE poll `[0], [2], [4], [6]`. Stepping one element per vector re-read
bits already examined and left the tail of the map unchecked (ISSUES.MD D3). `MAX_THREADS_WORDS`, not
`MAX_THREADS_BYTES`, sizes the allocation, because `declare1d64z` counts elements.

A worker must never touch the global `timer` beyond reading `siFrequency`: `CLASS_TIMER::Update` is a
multi-field read-modify-write, so every thread calling it was an unsynchronised race that left `siTotalTics`,
`dTotal` and `dScale` meaningless and made each thread's "now" whatever another thread last wrote
(ISSUES.MD D2). `ComputationPulse` and `PulseWaitUntil` take their timestamps from `CurrentTics()` (`CPU.h`),
a bare `QueryPerformanceCounter` read into a local. A side effect worth keeping: `wmain`'s single
`timer.Update()` before the spawn loop now gives every thread an identical `startTics`, which is what the
time-synchronised shapes need.

`ComputationPulse` implements the pulse shapes from the `procSync` bits before entering its loop:

- **Constant** (bit 4): `nextTic` is set to the end time, so the compute branch never yields.
- **Parallel** (bit 1): all threads pulse together.
- **Round-robin** (bit 0): thread *n*'s start is offset by *n* cycles and its cycle stretched by the thread
  count, so exactly one thread is active at a time.
- **Staggered** (bit 2): offset by `1 << (coreNum & 7)` cycles — a doubling ramp across each group of 8 cores.
- **Sweeping** (bit 6): the sleep duration is recomputed each cycle as a linear ramp across the test's total
  duration, continuously sweeping the duty cycle rather than holding it fixed. The ramp is only meaningful
  *inside* the run's window, so the loop re-checks `endTics` against a fresh reading immediately after
  `timer.Update()`, and the ramp itself is computed in `si64` and clamped to `[0, cycleTime]` before it is
  narrowed. Evaluating it past `endTics` used to make the subtraction negative, and the unsigned delay that
  came out of the cast was ~49 days (ISSUES.MD E1) — do not remove either guard.
- **Time-synchronised** (bit 3): suppresses the per-thread random start/period jitter (`offset[0..1]`) that is
  otherwise applied to deliberately desynchronise threads.

Bits 0–2 select *one* shape, so `wmain` rejects a `procSync` carrying more than one of them and substitutes
Parallel when it carries none. The shape `switch` in `ComputationPulse` accordingly treats every value other
than 1 (R-R) and 4 (Staggered) as Parallel, and adds `startTics` outside the `switch`: `nextTic` starts life as
a *duration* (`activeTics`), so any path that fails to add the start timestamp leaves the thread comparing a
QPC reading against a few million tics, permanently asleep — the whole-run idle of ISSUES.MD A4.

Every wait a worker thread performs — the start-up delay and each pulse boundary — goes through the
pulse-timing group in `CPU.h` (`CreatePulseTimer`, `PulseWait` / `PulseSleep`, `PulseWaitUntil`), never
`Sleep`. Each thread creates **its own** waitable timer with `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` and
closes it before clearing its completion bit; a shared handle would not work, because `SetWaitableTimer`
cancels whatever interval the object is already counting. This is what makes the millisecond-resolution
options mean what they say: Windows' default 15.625 ms scheduler tick quantises `Sleep`, and because
`nextTic` fixes the period, every millisecond an off-phase overruns is taken out of the *following*
on-phase — `Tf[10]10` delivered a 3 ms on-phase at 15 % duty rather than 10 ms at 50 %. `timeBeginPeriod`
is deliberately *not* used, and `<timeapi.h>` is deliberately no longer included: raising the tick rate is
a machine-wide change that would perturb the C-state behaviour the pulse shapes exist to exercise, and it
would add a `winmm.lib` dependency. `CreatePulseTimer` degrades to an ordinary waitable timer on kernels
older than Windows 10 1803, and `PulseWait` to `Sleep` if even that fails.

### Memory-backed mode and the arena

Requesting memory (`M`, or any preset) switches every thread from register-resident to `JobMem*` kernels
working over a RAM arena — this is what exercises load/store units and the cache hierarchy.

`wmain` allocates one 64-byte-aligned block (`malloc64`) for the whole run and hands each thread a slice via
`value[1][k].p0..p4`. The `switch(cfg.procUnits & 0x01F)` is the arena layout. It yields two numbers:
`recSize`, the per-record byte cost of the selected units (8 for ALU-only, 40 for ALU+AVX2, 72 for
ALU+AVX-512, …), and `vecUnits`, the same record's *vector* portion counted in the 8-byte units
`resArray.alu` is indexed by. Whenever the ALU bit is set the two satisfy `recSize == (vecUnits + 1) * 8`,
and that identity is what makes the ALU sub-array — placed *after* the vector sub-array by advancing
`resArray.alu` by `vecRecords * vecUnits` — tile the block exactly. The multiplier applies to the record
count of **both** thread classes: applying it to the SMT term alone dropped the ALU base inside the vector
sub-array on any topology with non-SMT threads (ISSUES.MD C2). Change the unit set or the record size and
this switch must change with it, and keep its `default:` arm — without one an index with no case leaves
`recSize` uninitialised (ISSUES.MD A10).

Three rules follow the switch and must survive any edit to it:

- **Record counts are rounded down to a multiple of 4** (`& ~0x03ull`). Every `JobCycleMem*` call processes
  records `offset … offset+3` and the cursor in `ComputationPulse` steps by 4, so an odd count is walked up
  to three records past the end of the slice (ISSUES.MD C4).
- **A count of 0 is rejected** with `-18` rather than run. Zero records also drops the thread onto the
  *register* code path (`JobCycle[recCount ? 1 : 0]`) with `p0`–`p4` already overwritten by arena pointers.
- **`bos`, the running per-thread record offset, counts across both thread classes.** It is initialised once,
  outside the `m` loop; restarting it at the first SMT thread handed every SMT thread a slice a non-SMT
  thread already owned (ISSUES.MD C3).

The size of the request is checked against `GlobalMemoryStatusEx` and the result of `malloc64` against null,
both `-17`: every pointer handed to a thread is derived from that one allocation (ISSUES.MD C5).

Of the five pointers handed to each thread, `p0`–`p3` are four views of the *same* arena address at different
element strides (only `p4` is advanced past them, and only for the `ALU_` combinations). The seeding loop must
therefore write no more than one of them, which is why `wmain` rejects a unit selection naming more than one
of FPU/SSE4.1/AVX2/AVX-512 (ISSUES.MD C1).

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

`procUnits` bits 1–4 select *one* unit: `wmain` rejects a selection carrying more than one of them with `-16`,
and one carrying none of bits 0–4 with `-15`, both before the arena is allocated. Bit 0 (ALU) is orthogonal and
stacks with any of them. Dispatch and arena sizing already reduced a wider selection to the ALU bit plus the
widest other unit, so the extra units never ran: they only corrupted the arena seeds of the unit that did —
a false `!Fail!` — while their own results row was graded `.Pass.` against silicon never exercised.

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
only implementation. Adding a language means writing `<code>.h` with `wstrInstructions_*`, `wstrMessage_*[23]`
and `wstrInterface_*[13]`, then extending both `translations.h` and the `L` case in `CPU.cpp`.

Note the `L` option's selection logic is inverted (`if(lstrcmpiW(...))` is truthy when the codes *differ*), so
every input currently resolves to English. Fix that when adding a second language. The code itself is copied
into `wstrLang[6]`, and the copy is clamped to that capacity — `lstrcpynW`'s third argument is the size of the
*destination*, so passing the argument's length overran the globals that follow it (ISSUES.MD C6). A longer
code is therefore truncated to five characters; widen `wstrLang` if a language ever needs more. Some strings
are still
hard-coded outside the tables — `wstrUnitsCPU`, `wstrSyncCPU` and `wstrPass` in `CPU.h` (the last is flagged
`///--- Modify for translation ---///`). `wstrKernelName` in `CPU.h` is also outside the tables, but
deliberately: its entries are C++ identifiers naming the job kernels, and are not translated.

## Known constraints and latent issues

Scattered `///--- Modify to account for >64 virtual cores !!!` markers flag the main one. Verify against
current source before relying on any of these:

- **Single processor group / 64 virtual cores.** `MAX_THREADS` is 512 and the buffers are sized for it, but
  topology enumeration, the `U` core-map parsing and the affinity mask in `wmain` all assume one 64-bit mask.
  This is the top item on `CPU.cpp`'s To-do list.
- **The affinity mask restarts at bit 0 for the SMT pass** (ISSUES.MD G5). `mask` is re-initialised to 1 when
  the spawn loop advances from the non-SMT thread class to the SMT one, so on a topology carrying *both*
  classes the SMT threads are pinned to virtual cores the non-SMT threads already hold, while other cores
  idle. Harmless while `threadCount[0] == 0` — a uniform SMT CPU, which is the common case — but it now has
  teeth: until ISSUES.MD D5 was fixed, `SetThreadAffinityMask` was handed a `_beginthread` handle and pinned
  nothing at all, so no mask, right or wrong, ever reached the scheduler.
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
