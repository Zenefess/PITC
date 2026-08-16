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

**No per-file setting carries a `Condition`.** Both x64 configurations apply all six of them, so `Debug|x64`
compiles the AVX2 unit with `/arch:AVX2` and the AVX-512 unit with `/arch:AVX512` exactly as `Release|x64`
does. They were `Condition="…=='Release|x64'"`, which left both vector units compiling at the SSE2 baseline in
the debug build: MSVC accepts an intrinsic whatever `/arch` says, so the answers were right, but the code
around them could not be VEX-encoded and paid an AVX-to-SSE transition at every boundary (ISSUES.MD H3). The
requirement is now enforced from the sources as well — `CPU_jobs_AVX.cpp` `#error`s unless `__AVX2__` is
defined and `CPU_jobs_AVX512.cpp` unless `__AVX512F__` is, the complement of H1's guard on the baseline unit.
**The AVX2 unit's guard is two-sided**: it also `#error`s *if* `__AVX512F__` is defined, because
`/arch:AVX512` defines `__AVX2__` as well, so a lower bound alone accepts a raised per-file setting — and
`wmain` gates these kernels on `cfg.sys.cpuAVX2`, which dispatches them to every CPU reporting plain AVX2,
where the EVEX encoding the compiler would then use around the intrinsics is an illegal instruction
(ISSUES.MD H3). The AVX-512 unit needs no such upper bound; MSVC has no `/arch` above `/arch:AVX512`.
`CPU_jobs_SSE.cpp` has no such guard because MSVC has no `/arch` for SSE4.1: that unit compiles at the
baseline by construction, and its one SSE4.1 instruction is gated on `cfg.sys.cpuSSE4_1` at run time instead.
`Optimization=Custom` is likewise unconditional; `Debug|x64` sets no `Optimization` of its own, so it emits no
`/O` switch either way and the change is one of spelling rather than of code.

The **`_mm_abs_pd` and `_mm256_abs_pd` macros in the SSE and AVX2 units are `#undef`ined before being
defined**, and must stay that way. Those units include `CPU.h`, which reaches `SIMD management.h`, and that
header defines `_mm_abs_pd` itself for any unit compiled below AVX2. It used to spell it `_mm_and_epi64`, an
AVX-512VL instruction, so the SSE kernels would have carried an EVEX opcode into every CPU this program
dispatches them to; the header's definition is now `_mm_and_pd` over `_mm_castsi128_pd`, which is SSE2 and
computes the same mask, and its `_mm256_abs_pd` is defined only under `__AVX__` (ISSUES.MD I1). The `#undef`s
are therefore no longer load-bearing against an EVEX opcode — but they stay, because both are bare `#define`s
that an `#ifndef` would inherit silently, and the definitions the kernels are built from should be the ones
in the file the kernels are in. **`SIMD management.h` states no fused multiply-add macros at all now**: the
five `#ifndef`-guarded ones over `_mm_fmadd_ps` and its family could never be false — those are functions, not
macros — so they replaced the intrinsic with a two-rounding split form wherever a unit compiled below AVX2,
under the intrinsic's own name. They are `simd::fmadd_ps`/`fmsub_ps`/`fnmadd_ps` overloads on `fl32x4` and
`fl32x8` instead, beside the `simd::fmadd_ps` that header already carried (I2).

Two settings in `Release|x64` are gone and should not come back: `WholeProgramOptimization` (`/GL` and, from
the `Label="Configuration"` property group, `/LTCG`), which defers to link time precisely the optimisation
`/Od` exists to prevent, and `EnableFiberSafeOptimizations` (`/GT`), which does nothing here. `AssemblyDebug`
is gone from both link groups as a managed-code option, and the per-file `FavorSizeOrSpeed=Size` from
`CPU_jobs_standard.cpp` as inert under `/Od` (ISSUES.MD H5). `Release|x64` states `SDLCheck`,
`BufferSecurityCheck` and `ControlFlowGuard=Guard` explicitly, because a tool that parses user input should
not ship without stack cookies (H6); `Debug|x64` carries the first two and not CFG, being an ASAN build that
is never shipped.

Two settings are load-bearing for reproducibility and must stay pinned in **every** configuration:
`<FloatingPointModel>Strict</FloatingPointModel>` and `<FloatingPointExceptions>false</FloatingPointExceptions>`.
`/fp:strict` fixes the rounding and forbids the compiler from contracting `a*b + c` into an FMA — `JobFPU`
ends each outer iteration with exactly that shape — so it is what makes a `cpu.values` written by one build
valid under another. `CPU_build.h`, included by all four kernel units and by `CPU.h`, `#error`s if
`_M_FP_STRICT` is not defined, and folds the model into the build ID stored in the file's header (H2).
That guard is **MSVC-only, and clang is refused outright**. It used to read
`defined(_MSC_VER) && !defined(__clang__) && !defined(_M_FP_STRICT)`, which exempted clang-cl — the one
toolchain a build made outside `CPU.vcxproj` is likely to be made with, and so the one the guard exists to
catch. clang defines none of the `_M_FP_*` macros, so `VALUES_FP_MODEL` was 0 for *every* clang FP mode and
two clang-cl builds under `/fp:fast` and `/fp:precise` shared a `buildID`; and clang publishes no macro
saying it is in strict mode, so no check can be written to admit it. It is refused until the golden values
are generated and validated under it, as is any toolchain that is neither MSVC nor clang, for the same
reason (ISSUES.MD H1). **`VALUES_FP_MODEL` can no longer be 0** — the constant's final arm is unreachable —
which is what stops it being a value two differently-rounding builds could agree through.

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
thread report `!Fail!` as though the silicon were at fault. `W` self-validates by re-running the kernels
`VALUES_SELF_CHECK_ITERATIONS` (65,536) times **for every one of the 512 entries** and refuses to write the
file on any mismatch. It used to run 65,535 times for the first entry of each thread's range and not at all
for the rest, because the counter was a `ui16` declared once for the whole function and never reset — about
2 % of the table, in place of the guarantee the help text gives (ISSUES.MD B4, B8). Delivering that guarantee
costs real time: the check is 33.5 million ladder runs, which is minutes rather than seconds on a CPU of few
cores, and the counter is a `ui32` because a `ui16` can never reach 65,536.

`W` also runs `ValidateKernelFamilies` (`CPU.h`) *before* it generates anything, and refuses with `-22` and the
name of the offending kernel if it fails (ISSUES.MD B5). That check is what holds the eighteen job kernels to
each other: `cpu.values` only ever records what the five register-resident kernels produce, so a `JobMem*` or
`JobALU_*` kernel that had drifted from its counterpart was compared against nothing, and every memory-backed
run reported the difference as a CPU fault. It runs one seed through every kernel the CPU can execute and
requires each memory-array and combined variant to reproduce its register-resident counterpart **byte for
byte** — `memcmp`, never an arithmetic comparison, for the reason the bit-exactness note at the end of this
file gives. Each `JobMem*` kernel is handed four records carrying the same seed and all four must come back
equal, so the record indexing is checked alongside the arithmetic. `ValidateKernelFamilies` is a dispatcher:
the checks themselves are four `ValidateFamily*` functions, one per `CPU_jobs_*.cpp`, because the AVX2 and
AVX-512 halves move values of those widths and may not be compiled at the baseline (ISSUES.MD H4). Each
returns the `wstrKernelName` index of the first kernel that disagreed, and each derives its own ALU reference
rather than being handed one, so it is a complete statement of its own family. **Add a `Job*` family and it
must be added to its unit's `ValidateFamily*`, to `wstrKernelName`, and to `JobCycle`.**

**That half holds each unit to itself; a second half holds the units to each other.** `ValidateFamily*`
derives its reference from its own unit's register kernel, so nothing in it can see that `JobSSE`, `JobAVX2`
and `JobAVX512` must also compute `JobFPU` *element-wise* — the property `RunGoldenLadder` rests on, and the
whole of why a `cpu.values` written on one vector width verifies on another. Edit one unit's FP arithmetic
consistently across that unit's family and `W` used to pass and write the file; every machine of a different
width then computed a different `KernelFingerprint` and rejected it with `-21` "generated by a different
build", so the portability silently ended, no check named the kernel that ended it, and the diagnostic blamed
the file (ISSUES.MD B1). `ValidateKernelFamilies` therefore ends by running `LADDER_PROBE_LANES` (8) seeds
through `JobFPU` one lane at a time and handing that reference to `ValidateLadderSSE`, `ValidateLadderAVX2`
and `ValidateLadderAVX512` — one per unit, for the H4 reason again, each loading the same eight lanes into
vectors of its own width and `memcmp`ing all eight back against the scalar result. SSE is always checked,
AVX2 and AVX-512 exactly where `RunGoldenLadder` would use them. Each returns its `wstrKernelName` index as
`KERNEL_NAME_LADDER` and the two entries above it — spelt from the constant rather than as the literals 14–16
the family checks use, so that inserting a *family* entry and moving the constant carries the ladder returns
with it. That boundary is how `wmain` tells the two disagreements apart: below it it prints
`wstrMessage[30]`, at or above it `wstrMessage[41]`, both under `-22`. **A new vector unit needs a
`ValidateLadder*` as well as a `ValidateFamily*`, an entry in each half of `wstrKernelName`, and
`KERNEL_NAME_LADDER` moved past its new family entry.**

**`W` replaces `cpu.values`; it never writes into it.** The header and the two blocks go to `cpu.values.tmp`
— `CREATE_ALWAYS`, share mode **0** — and `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`
moves that over `cpu.values` once the last block is written and `FlushFileBuffers` has returned. Written in
place, as it was, a crash, a full disk or a Ctrl-C anywhere between the open and the last write left a
truncated file where the user's previous one had been: the header hashes make the next run reject the remnant
with `-21` rather than grade a CPU against it, but the good file is gone and another costs the minutes of
ladder runs above (ISSUES.MD B2). Three rules hold that arrangement together. **The flush is part of the
write** — without it the rename can reach the disk ahead of the bytes it renames, publishing the right name
over the wrong contents — so a failed `FlushFileBuffers` is treated as a failed write. **Every failure path
deletes the temporary**, so an error return leaves the directory as it found it. And **the temporary's name
is fixed rather than generated**: with a share mode of 0 that is what makes two concurrent `W` runs collide
at the `CreateFileW` and refuses the second with `-5`, where `FILE_SHARE_WRITE` let both interleave their
blocks into one file. A failed move reports `wstrMessage[42]`, which names the file and says the previous one
is untouched. **A new write between the open and the move needs a delete on its failure path**, and nothing
between them may name `cpu.values` itself.

Exit codes are meaningful and documented in `en-GB.h` (`wstrInstructions_English`): negative = error,
`0` = stability test completed, `1` = values file written, `2` = instructions displayed. `-11` … `-18` are the
pre-flight rejections in `wmain` — an unsupported vector unit, more than one `S` pulse shape, a test duration
of zero or less, a zero pulse on-time, an `I` string naming no processing unit, an `I` string naming more
than one of FPU/SSE4.1/AVX2/AVX-512, a memory request the machine cannot satisfy (`-17`, shared by the
pre-flight size check, a failed `malloc64`, a failed report buffer and — since ISSUES.MD C2 — a failed
`resArray.iter` under `B`; every run-length allocation in the program now answers with it), and a per-thread
slice too small to hold the eight records a
slice is rounded to (`-18`, which is also where a core class given no memory at all arrives — `Mn` and `Ms`
each name one class). `-19` is the one runtime failure that aborts a run: a worker thread that could not be
created or resumed, in either the test or the `W` path. `-20` is a `cpu.values` header that could not be
written; `-21` is a `cpu.values` this build will not read — bad magic, an unreadable or unsupported format
version, a different build or kernel revision, or contents that disagree with the hashes in the header —
one code shared by five messages, the way `-11` and `-17` already share theirs. `-22` is a job kernel that
disagrees with the kernel it is required to reproduce, raised by `ValidateKernelFamilies` under `W` and
shared by two messages: `wstrMessage[30]` for a memory-array or combined kernel that has drifted from the
register-resident kernel of its own unit, and `wstrMessage[41]` for a vector kernel that no longer computes
`JobFPU` element-wise.
`-23` is a processor topology that could not be enumerated — `GetLogicalProcessorInformationEx` failing at
either of its two calls, a buffer that could not be allocated for it, or a walk that named no processor core
— raised by `EnumerateTopology` before anything else in `wmain` and shared by three messages (ISSUES.MD G6,
G3). `-24`, `-25` and `-26` are the command-line rejections: a numeric option with no value, a malformed one
or one outside its documented range; an option letter, argument or preset digit this build does not
recognise; and a `U` core map that selects no core at all, which is refused before `threadCount[2]` is used
as a divisor (ISSUES.MD F4, F9, F2, C8). `-5` is shared by two messages of the `W` path, both about the file
it could not put in place: `wstrMessage[6]` for a temporary it could not create — which is also how a second
concurrent `W` is refused — and `wstrMessage[42]` for one it wrote but could not move over `cpu.values`.
Add a code and the table in `wstrInstructions_English` and the message in `wstrMessage_*` have to grow with
it; a new *message* under an existing code grows the second table alone.
Not every message is an exit code: `wstrMessage[24]` warns
that a thread could not be pinned, `wstrMessage[33]` that the machine carries more virtual cores than
`MAX_THREADS`, `wstrMessage[34]` names the two core classes of a hybrid CPU, because there the split is not
the non-SMT/SMT one the options are documented against (ISSUES.MD G9), and `wstrMessage[39]` reports a
language code this build does not carry; none of the four stops the run.

**An unrecognised or malformed argument is fatal, and deliberately so.** Every one of these used to be
skipped in silence, so a mistyped option ran a configuration other than the one on the command line and
reported `.Pass.` for it (ISSUES.MD F9) — which is the class of outcome the whole program exists to avoid.
The top-level `switch` and the inner switches for `I`, `M`, `O`, `S`, `T` and `U` therefore all carry a
`default` arm, `B` and `W` reject a trailing character, and `-` validates its digit *before* applying the
memory and unit preamble that used to run whatever followed the dash.

**`O[name]`'s file is created after the run, not before it, and nothing between the two touches it.** `wmain`
opens it with `CREATE_ALWAYS` immediately above the write, in the same block; before the run it only *probes*
the path — `OPEN_ALWAYS`, closed at once, and deleted again if the probe was what created it. The file used to
be created with `CREATE_ALWAYS` before the arena was sized, so `-18`, the three `-17`s and the two `-19`s each
left an empty file where the user's previous results had been, said nothing about having done so, and leaked
the handle; `PITC.exe -1 Mc999999 O[results.txt]` destroyed an existing `results.txt` and then failed the
memory pre-flight, and a Ctrl-C mid-run did the same (ISSUES.MD C1). Keep the probe: `O` takes its path from
the command line, and a run of hours should not discover a typo in it once the test is over. **A new error
return between the probe and the write needs no file handling at all** — which is the point of the
arrangement, and the reason not to move the creation back up.

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

The four planes (`value[4][MAX_THREADS]`, declared in `CPU.h` and defined in `CPU.cpp`) each carry a fixed
role:

| Plane | Role at test time |
|-------|-------------------|
| `value[0]` | Input seeds (block 1 of `cpu.values`) — read-only during the run |
| `value[1]` | Per-thread working scratch; in memory-backed mode its `p0`–`p4` fields instead hold pointers into the RAM arena |
| `value[2]` | Expected outputs (block 2 of `cpu.values`) — the comparison reference |
| `value[3]` | Observed value, written by the `JobCycle*` wrapper on a mismatch and read back by `Failed()`; otherwise a copy of `value[2]` so the results table prints `.Pass.` |

`value[3]` is where a failure is *reported from*, and both readers depend on the wrapper having written it:
`Evaluate` grades the results table by comparing it against `value[2]`, and `Failed()` prints it as the
observed value. `Failed()` used to print `value[1]` for the AVX-512, AVX2, SSE and FPU units instead, which
is the working plane — and in memory-backed mode the working plane holds no results at all, its first 40
bytes being the arena pointers the `RESULTS` union overlays on the `avx512` member, so the one place an
AVX-512 fault is surfaced printed eight pointers formatted as doubles (ISSUES.MD A8). Nothing but a
`JobCycle*` wrapper should write `value[3]` during a run.

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

A disagreement is recorded in `generateError` (`CPU.h`), a global flag each thread sets with
`_InterlockedOr8` before it clears its completion bit, and which `wmain` reads once every thread has joined.
It used to be signalled by writing two sentinel values into `value[3][0]` and `value[2][0]` — storage that
belongs to entry 0, and that whichever thread owns entry 0 overwrites wholesale as it passes. A fault found
by a thread that finished first was therefore erased, and `W` wrote the file and reported success on a run
that had already failed (ISSUES.MD B7). **Nothing but a result belongs in a result plane**; a new signal
between the workers and `wmain` needs storage of its own and an interlocked write, so that it is ordered
ahead of the completion bit `wmain` is waiting on.

### Where the globals live

**Every object at namespace scope is declared `extern` in a header and defined once, in `CPU.cpp`.** That is
`timer`, `cfg`, `threadData`, `value`, `threadBits`, `wstrOut`, `resArray`, `generateError` and `wstrLang`
from `CPU.h`, the three language pointers from `translations.h`, and the `JobCycle[2][32]` dispatch table from
`CPU_job_cycles.h`; `Failed` is a function definition in `CPU.cpp` for the same reason. The definitions sit
together at the top of `CPU.cpp`, above `wmain`, and the declaration in the header names the `declare*` macro
that defines each pointer, because the macro carries the allocation with it and the two have to be changed
together.

They were defined in the headers, which is why the program could never grow a second translation unit
(ISSUES.MD H9). Some of them would have been duplicate symbols; the rest — everything the `declare*` macros
produce, being `dataType *const` and so internally linked — would have been *worse than* a link error, giving
each unit a private copy of the result planes, the thread table and the completion bitmap that the workers and
`wmain` compare and signal through. The `L` option's three pointers are the same hazard: they are written at
run time, and a per-unit copy is a language change one half of the program never sees.

The immutable tables are the exception, and are `inline` rather than `extern`: `wstrUnitsCPU`, `wstrSyncCPU`,
`wstrPass`, `wstrKernelName` and the two byte-order marks. `inline` makes them one entity instead of a copy
per unit while leaving them beside what they describe. **A new mutable global goes in `CPU.cpp`; a new
immutable table may stay in a header if it is `inline`.**

### Job kernels: one translation unit per ISA

`CPU_jobs_standard.cpp` (ALU/FPU), `CPU_jobs_SSE.cpp`, `CPU_jobs_AVX.cpp`, `CPU_jobs_AVX512.cpp` each define
the same eight-function family, declared `extern` in `CPU.h`:

```
Job<UNIT>(x)              Job ALU_<UNIT>(x, y)         # register-resident
JobMem<UNIT>(ptr)         JobMemALU_<UNIT>(ptr, ptr)   # memory-array variants, 4 records per call
```

**Everything else of that unit's width lives in the same file**, and that is the point of the split: each unit
also holds its `ResultsMatch` overload, its four (or six, for the scalar unit) `JobCycle*` wrappers, its
`ValidateFamily*` cross-check, its `SeedRecords*` arena pass, and — for the three vector units — its
`ThreadsRunning*` completion-bitmap poll. All of those used to sit in headers included by `CPU.cpp`, which
compiles at the SSE2 baseline in every configuration, so the file holding the option parser and the results
table emitted `_mm512_xor_si512`, `_mm512_test_epi64_mask` and a 512-bit move per AVX-512 job cycle
(ISSUES.MD H4). Nothing failed at run time — the ISA gate in `wmain` keeps those paths off CPUs that cannot
execute them — but the isolation this section describes was true of the kernels alone. It is now true of the
whole program: `CPU.obj` contains no `ymm` or `zmm` operand and no `ptest`. **Anything added that moves or
compares a value wider than 64 bits belongs in the unit for its width, not in `CPU.h`, `CPU.cpp` or
`CPU_methods.h`.**

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
bit — that is what lets a `cpu.values` written on one vector width verify on another, and it is what the
`ValidateLadder*` half of `ValidateKernelFamilies` tests directly (ISSUES.MD B1). Any change to the FP
arithmetic must be made in all four units identically, and `W`'s `ValidateKernelFamilies` is what proves the
eighteen kernels still agree afterwards.

### Dispatch: `JobCycle[2][32]`

Each kernel is wrapped in a `JobCycle*` function that seeds the working values from `value[0]`, runs the
kernel, compares against `value[2]`, and on mismatch records `value[3]` and calls `Failed()`. The wrappers are
defined in the four `CPU_jobs_*.cpp` units (H4) and declared in `CPU_job_cycles.h`, which also states the
rules the table below keeps; the table itself is defined in `CPU.cpp` with the other globals (H9).
**`Failed()`'s third argument must name the unit whose `value[3]` member the wrapper just wrote**
(0=AVX-512, 1=AVX2, 2=SSE, 3=FPU, 4=ALU), because that argument is what selects the format the mismatch is
printed in and the lanes it is read from. `JobCycleMemFPU` passed 4 for all four of its records, so an FPU
fault printed two unrelated ALU integers with `%lld` (ISSUES.MD A12) — the combined `JobCycle*ALU_*`
wrappers, which call `Failed()` twice with different units, are the shape to copy.
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

`wmain` (`CPU.cpp`) calls `EnumerateTopology` (`CPU.h`) first of all, applies the core map and SMT
policy (`SetSMTLoading` in `CPU.h`), allocates the arena, then creates one `ComputationPulse`
(`CPU_methods.h`) per selected virtual core with `_beginthreadex` — **suspended**, so `SetThreadGroupAffinity`
lands before the thread executes anything — and resumes it. `_beginthread` is not usable here: the CRT closes
*its* handle when the thread exits, so the handle must not be retained, let alone passed to an API. The
handle `_beginthreadex` returns belongs to `wmain`, which **keeps it in `threadHandle[]` until the thread has
been waited on**: the completion bitmap says only that a thread has reached its last statement, and `wmain`
reads the result planes and frees the arena as soon as it empties, so `JoinThreads` (`CPU.h`) is what makes
"every thread has finished" true before either happens (ISSUES.MD D8). A creation or resume failure aborts the
run with `-19` rather than leaving a completion bit no thread will ever clear, and releases the handles it
already holds with `ReleaseThreads` rather than waiting on threads that would run to the end of a test nobody
is going to read. Both thread entry points are therefore `ui32 __stdcall` and end by returning rather than
calling `_endthread`.

**Everything about a core is a (group, mask) pair, never a bare mask.** `EnumerateTopology` walks
`GetLogicalProcessorInformationEx(RelationAll, …)`, whose every record carries a `GROUP_AFFINITY`;
`GetLogicalProcessorInformation`, which it replaced, has no group field at all and on a machine of more than
64 logical processors reports only the group the calling thread happens to be in — so the tool silently
tested one group and reported it as the machine (ISSUES.MD G3). The core maps are indexed `[class][group]`,
the thread bitmap in the banner is printed one row per group, and threads are pinned with
`SetThreadGroupAffinity`: `SetThreadAffinityMask` interprets its mask in the thread's *own* group and cannot
reach another. `MAX_GROUPS` (`CPU.h`) is 64, Windows' architectural maximum, and is deliberately **not**
`MAX_THREADS_WORDS` — that is the width of a bitmap indexed by thread, and the two are equal only when every
group holds 64 virtual cores.

The core a thread is pinned to comes from `NextSelectedCore` (`CPU.h`), a cursor over
`cfg.sys.coreMap[j][g] & cfg.coreMap[g]` — the selected cores of the thread's **own class** — not over the
combined map. `threadCount[j]` is the population count of exactly that expression summed over the groups, so
the cursor hands out one distinct core per thread and leaves none of the selected cores idle, and a class-0
thread lands on a class-0 core, which is what makes `packetSizeRAM` and `resArray.records[j]` (both chosen by
class) describe the core the thread is really on. The walk used the combined map and restarted at bit 0 for
each class, so the second class was pinned over cores the first already held: on a hybrid P/E-core part that
left **every E-core untested at every setting**, its efficiency cores being class 0 and its performance cores
class 1 (ISSUES.MD G5, G9). The two class maps are disjoint, which is why
restarting the cursor at group 0, bit 0 per class is still correct. A mask shifted past bit 63 is 0, and that
is the cursor's signal to resume at bit 0 of the next group — do not "fix" the shift.

**The two core classes are not the same two classes on every machine, and `CoreClass` (`CPU.h`) is the one
place that decides which.** Class 1 is the wider core and class 0 the narrower, and everything indexed by
class follows from that: the two core maps, the two cache records, `Mn`/`Ms`, and the two passes of the spawn
loop. A machine reporting more than one `EfficiencyClass` is split by that — performance cores are class 1,
every lower tier class 0, so a three-tier part collapses onto two — and a machine reporting one is split by
sibling count, which is the property that describes it. Deriving the class from the sibling count alone named
a hybrid part's classes "non-SMT" and "SMT" when they are nothing of the kind, and stopped sorting it at all
once SMT was disabled in firmware: every core then carries one virtual core, so P and E cores landed in one
class with one set of cache sizes for both (ISSUES.MD G9). `cfg.sys.hybrid` records which rule applied and
`wstrMessage[34]` reports it, because the `M` and `U` options are documented against the other one.

The enumeration carries seven rules of its own. Both `GetLogicalProcessorInformationEx` calls are checked, as
is the buffer allocated between them, and a walk that names no processor core is refused as well — all
`-23`; an unchecked call left the walk reading an untouched buffer as though it held topology records, and an
empty core map means no thread is ever created, which `wmain` would otherwise report as a successful test of
a CPU it never touched. Records are stepped by the `Size` each one carries, and a `Size` of 0 or one reaching
past the buffer ends the walk rather than spinning or reading past it. **The buffer is walked three times**,
by `WalkTopology`, because each pass needs the one before it complete and the API documents no order for its
records: the efficiency classes decide how a core is filed, the core maps cannot be built until that rule is
known, and a cache is filed by the class of the cores its own mask names, which only the finished maps can
answer. `cfg.sys.SMT[class]` is the widest sibling count *of that class*, and each is normalised to 1 when
nothing set it — a machine-wide maximum described a hybrid part's narrow class as being as wide as its wide
one, and a CPU without SMT left a 0 behind for every later shift and multiply. `cfg.sys.groupCount` is the number of groups
the walk *populated*, counted from the maps themselves over `MAX_GROUPS` — it was an arithmetic expression
that omitted the SMT core count, doubled the non-SMT one and added 1, so a single non-SMT core made it 2
while only group 0 was ever written (ISSUES.MD G1, G4, G6). And **a core that would take the virtual core
total past `MAX_THREADS` is refused rather than recorded**: a thread's index is its ordinal among the
selected cores, so `threadData`, the four result planes and the completion bitmap are all indexed by it, and
the surplus is reported through `wstrMessage[33]` rather than written past the end of them. `cfg.sys.vCoreCount`
is therefore the count of cores actually accepted, not `coreCount[1] * SMT + coreCount[0]`.

**The topology code counts and scans bits without an intrinsic, and that is a requirement rather than an
oversight.** `LowestSetBit64`, `HighestSetBit64` and `SetBitCount64` (`CPU.h`) are shift-and-mask folds
because `EnumerateTopology` is the first thing `wmain` does — before the banner, before the help screen, and
before the pre-flight check that names a missing instruction set. `PopulationCount64`, the obvious spelling
of the third, is winnt.h's route to `__popcnt64`, and no `/arch` setting gates that intrinsic: it emits
`POPCNT` whatever baseline the unit was compiled at, so every invocation of the program — the bare help
screen included — died with an illegal instruction on an x64 CPU predating Nehalem, the Core 2 and early
Athlon 64 X2 parts the scalar job unit and `ThreadsRunningScalar` exist to serve (the POPCNT entry of
ISSUES.MD's G section, and D4 before it, which is the same failure one call later). A compiler that
recognises the fold and substitutes `POPCNT` for it does so only where the ISA it was told to target carries
the instruction, which is the property the intrinsic lacks. **Nothing on the path from `wmain` to the ISA
gate may execute an instruction outside the x64 baseline**, intrinsic or compiler-generated.

`SetSMTLoading` applies the `Ue`/`Uo` policies by masking with `cfg.sys.coreSibling`, two bitmaps the
enumeration builds one record at a time: the lowest set bit of each physical core's sibling mask, and the
highest. That record is the only place the sibling layout is knowable — `cfg.sys.coreMap[0]` and `[1]` are
unions and cannot say afterwards which virtual cores share a core — and a core without SMT contributes the
same bit to both maps, so one thread per physical core keeps it either way. Rebuilding the layout from
`coreCount[1]` and a stride instead is what deselected every non-SMT core on a hybrid part, and what shifted
by 64 on a CPU reporting no SMT at all (ISSUES.MD G1, G2, G7). **A bitmap added to `GLOBAL_CFG` must be
freed in its destructor**, which is what the `mfree` call there is for.

`Ua` reads the same two bitmaps in the other direction, one physical core at a time: each first-sibling bit
is paired with the lowest last-sibling bit at or above it, `(last | (last - first))` is that core's span, and
the span is added to a **separate accumulator** when the map already holds any virtual core of the core. The
separate accumulator is the point — the arm used to be `cfg.coreMap[i] |= (cfg.coreMap[i] << j) & …` over a
stride of `cfg.sys.SMT`, which reads the map it is writing, so at 4-way SMT the accumulated shift selected
the *next* physical core (ISSUES.MD G8). It consults `coreMap[0] | coreMap[1]` rather than the class-1 map
alone, because since G9 a class-0 core can carry SMT. `cfg.sys.SMT[]` has **no reader left in the program**:
it is recorded and reported, never computed with, and no new use of it should reintroduce a stride.

Completion is signalled through `threadBits`, a global bitmap with one bit per thread. **Every access to it
is byte-wide and interlocked**: a thread clears its own bit via `_InterlockedAnd8` when it exits, and `wmain`
sets a bit with `SetThreadRunning` (`_InterlockedOr8`) before spawning. A plain `|=` there was a lost-update
race against a worker already clearing a different bit of the same byte, which hung the poll below
(ISSUES.MD D1). `wmain` spins on `ThreadsRunning()` — a reference bound at startup to the widest poll the CPU
can execute, `ThreadsRunningAVX512/AVX/SSE/Scalar`. All four are declared in `CPU.h`, but only the scalar one
is defined there: each vector poll reads the map with instructions of its own width, so it lives in the
`CPU_jobs_*.cpp` unit built for that width (ISSUES.MD H4). All four read the *same* 64 bytes: a `ui64`
array means a 512-bit view spans 8 elements, a 256-bit view 4 and a 128-bit view 2, so the AVX2 poll steps
`[0], [4]` and the SSE poll `[0], [2], [4], [6]`. Stepping one element per vector re-read bits already
examined and left the tail of the map unchecked (ISSUES.MD D3). `MAX_THREADS_WORDS`, not `MAX_THREADS_BYTES`,
sizes the allocation, because `declare1d64z` counts elements.

**The SSE poll is not the baseline**, and the selection must keep testing `cfg.sys.cpuSSE4_1` for it:
`AllFalse` of two `ui128` is `_mm_testz_si128`, an SSE4.1 instruction, so binding it on nothing but the
absence of AVX2 executed an illegal instruction on a CPU carrying SSE2 and no more — before any test began,
and before the pre-flight check that names a missing instruction set, which an ALU-only run never reaches at
all (ISSUES.MD D4). `ThreadsRunningScalar` is that baseline, and is the only one of the four that reads the
map through the `vui64` declaration: no `_mm*_load` intrinsic takes a volatile pointer, so the three vector
polls take their address from `ThreadBitsView`, whose `std::atomic_signal_fence` is inlined into the wait
loop with the load and stops the map being read once and cached for the rest of the run (D9). Casting the
qualifier away without it compiled the whole wait loop down to a single `ret` under gcc -O2.

A worker must never touch the global `timer` beyond reading `siFrequency`: `CLASS_TIMER::Update` is a
multi-field read-modify-write, so every thread calling it was an unsynchronised race that left `siTotalTics`,
`dTotal` and `dScale` meaningless and made each thread's "now" whatever another thread last wrote
(ISSUES.MD D2). `ComputationPulse` and `PulseWaitUntil` take their timestamps from `CurrentTics()` (`CPU.h`),
a bare `QueryPerformanceCounter` read into a local. A side effect worth keeping: `wmain`'s single
`timer.Update()` before the spawn loop now gives every thread an identical `startTics`, which is what the
time-synchronised shapes need. **That call belongs immediately above the spawn loop**, below the arena
allocation and the seeding pass that writes every byte of it: taken above them, as it was, the reading was
stale by the whole of the seeding — hundreds of milliseconds for `Mc8`, far longer for a multi-gigabyte
request — so the start-up delay was spent on seeding, `startTics` could already be in the past when the
threads read it, and `endTics` moved earlier with it, making the test shorter than the duration asked for
(ISSUES.MD E11). `timer.siCurrentTics` has no other reader in the program.

`ComputationPulse` implements the pulse shapes from the `procSync` bits before entering its loop. **Every
quantity below is a tic count**, including the idle phase, which is `cycleTics - activeTics` *after* the
shape has stretched the cycle. It used to be a `ui32` of milliseconds grown separately from the cycle, and
the two could then disagree — the staggered arm did — as well as overflowing 32 bits at a long cycle on a
wide machine (ISSUES.MD E3, E6).

- **Constant** (bit 4): `nextTic` is set to the end time, so the compute branch never yields.
- **Parallel** (bit 1): all threads pulse together — and, unless the run is time-synchronised, this is the one
  shape that is *jittered*, so that "together" is not lockstep.
- **Round-robin** (bit 0): thread *n*'s start is offset by *n* cycles and its cycle stretched by the thread
  count, so exactly one thread is active at a time — and the rotation is **folded down** to the most slots the
  run can hold, exactly as the staggered ramp below is, thread *n* then taking slot `n % slots`. Without the
  fold a run shorter than `threadCount` cycles never reached its late threads at all: each waited out an
  offset ending after the run had, ran the one job cycle forced before the loop, and `wmain` waited on every
  second of it — `Ia Sr Tft60[15000]15000 Ua` across 16 virtual cores took 7.5 minutes to deliver a
  60-second test and graded thirteen of its sixteen rows on one job cycle apiece (ISSUES.MD E1). The fold
  decrements rather than halving, `threadCount` not being a power of two, so it keeps as much of the
  one-thread-at-a-time property as the run can pay for; where the run already holds the whole rotation it
  does not execute and the arithmetic is what it always was.
- **Staggered** (bit 2): offset by `(1 << (coreNum & 7)) - 1` cycles and its cycle stretched by
  `1 << (coreNum & 7)` — a doubling ramp across each group of 8 cores. The offset is added to `startTics`
  only, `nextTic` taking it from there; adding it to both left the *first* window open one whole stagger
  period longer than the on-time asked for — 7.9 s of unbroken compute at preset `-8`'s 900 ms setting. The
  stretch is by the stagger, not by twice it, or core 0 and every eighth core after it run at half the cycle
  frequency requested. And the ramp is **folded down** (`stagger >>= 1`) until the thread's first window fits
  inside the run, because a run shorter than the full 128-cycle ramp otherwise leaves the top of each group
  of 8 cores executing the one job cycle forced before the loop and nothing else — a `.Pass.` for silicon
  that was never exercised (ISSUES.MD E3).
- **Sweeping** (bit 6): the sleep duration is recomputed each cycle as a linear ramp across the test's total
  duration, continuously sweeping the duty cycle rather than holding it fixed. Each cycle **begins idle** and
  the duty cycle rises in a straight line to 100 % at the end of the run; the help text, the README and the
  `T` option say so, because nothing but the code did (E8). `wmain` clears `cfg.offTime` for any run carrying
  this bit — in a sweep the on-time is the whole cycle — which is what makes presets `-6` and `-7` run the
  cycle they name rather than 900 ms more (E7). The ramp is only meaningful *inside* the run's window, so the
  loop's condition takes a fresh reading and leaves as soon as the deadline has passed, and the ratio is
  taken in `fl64` and the result clamped to `[0, cycleTics]` before it is used. Evaluating it past `endTics`
  used to make the subtraction negative, and the unsigned delay that came out of the cast was ~49 days
  (ISSUES.MD E1) — do not remove either guard. The `fl64` is not decoration either: the `si64` product of a
  cycle and a run overflows at a day-long run of minute-long cycles.
- **Time-synchronised** (bit 3): suppresses the jitter a parallel run is otherwise given, so `Spt`'s threads
  share one pulse edge where `Sp`'s deliberately do not.

**No wait may end after the run does.** Each idle phase is a wait until `min(curTics + pulseTics, endTics)`,
not a relative sleep: a round-robin thread's off-phase is one cycle per thread and a staggered thread's up to
127 of them, so 64 threads at a 2 s cycle issued a single 126-second sleep and `wmain` waited out every second
of it past the end of the test (ISSUES.MD E6). **The start-up wait is bounded the same way**, by
`min(startTics, endTics)`: the two fold-downs keep a round-robin or staggered thread's first window inside the
run, but a parallel thread's start carries a jitter of up to a quarter of its shorter phase and no fold, so a
run shorter than that window — `Tft0.05[250]250` — could still open a wait outliving the test (E1). The only
overshoot left is one job cycle — the one already executing when the deadline passes, which must finish for
its result to be compared.

The jitter is `jitterSpan`, `JitterSeed` and `NextJitter` (`CPU.h`), and four properties of it are
load-bearing. **It is drawn from a per-thread generator**, seeded from `rand_s`, the performance counter and
the thread's index: `rand()`'s state is per-thread storage initialised to the same default seed in the MSVC
CRT and `srand` is never called, so every thread of every run drew the same two values — 41 and 18 467 — and
the offsets desynchronised nothing (ISSUES.MD D6). **One span in tics governs both halves**, a quarter of the
shorter of the on and off phases: the pair it replaces was a millisecond `offset[0]` and a tic-counted
`offset[1]`, one of which was in the wrong unit (D7), and a window wider than that quarter can consume a
pulse whole. **The start offset is drawn once and the period jitter every cycle, centred on zero**, so the
train shifts without distorting the duty cycle and the mean period stays `cycleTics`; adding a fixed
`offset[1]` to every cycle was a permanent change of period, and a fixed `offset[0]` to every sleep took up
to 63 ms out of each *following* on-phase, because `nextTic` fixes the period arithmetically. And **only the
parallel shape is jittered**: round-robin and staggered are arrangements of the threads against each other,
which any perturbation erodes — a jittered `Sr Tf[250]100` across 8 threads spends 194 s of a 30-minute run
with two threads computing at once, against the zero its description promises.

Bits 0–2 select *one* shape, so `wmain` rejects a `procSync` carrying more than one of them and substitutes
Parallel when it carries none. The shape `switch` in `ComputationPulse` accordingly treats every value other
than 1 (R-R) and 4 (Staggered) as Parallel, and adds `startTics` outside the `switch`: `nextTic` starts life as
a *duration* (`activeTics`), so any path that fails to add the start timestamp leaves the thread comparing a
QPC reading against a few million tics, permanently asleep — the whole-run idle of ISSUES.MD A4.

**Bits 4–6 are read the same way, each from its own bit.** The outer `switch(tcfg->procSync & 0x070)` gives
Fixed pulse (`0x020`) and Sweeping pulse (`0x040`) the pulsed arm and Constant (`0x010`) the other, and
`wmain` substitutes Constant when none of the three is set, exactly as it substitutes Parallel above. Bit 5
had no reader at all while "fixed pulse" was inferred from the absence of the other two, so the banner's
`F-P` named a mode no line of the executor implemented (ISSUES.MD E10). The arm that catches a combination
this build does not implement is the **constant** one, deliberately: a thread that never yields cannot be
left idle for a test it was told to compute, where the fixed-pulse arm would run an unknown mode as a
100 ms/900 ms pulse under another mode's name.

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

- **Record counts are rounded down to a multiple of 8** (`& ~0x07ull`). Four of those eight are what the
  cursor needs: every `JobCycleMem*` call processes records `offset … offset+3` and the cursor in
  `ComputationPulse` steps by 4, so an odd count is walked up to three records past the end of the slice
  (ISSUES.MD C4). Eight is what puts every slice on a cache line (C12): a thread's base in a sub-array is its
  record offset scaled by that sub-array's element size, the smallest of which is the ALU sub-array's 8
  bytes, so a multiple of 8 records is a whole number of 64-byte lines in every sub-array — and, since the
  ALU base is the vector records of every thread scaled by 8 bytes or more, for that base as well. At a
  multiple of 4, adjacent threads shared the line at each boundary and the false sharing was measured as the
  cache behaviour this mode exists to exercise.
- **A count of 0 is rejected** with `-18` rather than run. Zero records also drops the thread onto the
  *register* code path (`JobCycle[recCount ? 1 : 0]`) with `p0`–`p4` already overwritten by arena pointers.
  This is also the check that refuses a core class given no memory at all — `Mn8` names the first class and
  leaves the second holding nothing (ISSUES.MD C9) — so a class with threads must be given a size.
- **`bos`, the running per-thread record offset, counts across both thread classes.** It is initialised once,
  outside the `m` loop; restarting it at the first SMT thread handed every SMT thread a slice a non-SMT
  thread already owned (ISSUES.MD C3).

The size of the request is checked against `GlobalMemoryStatusEx` and the result of `malloc64` against null,
both `-17`: every pointer handed to a thread is derived from that one allocation (ISSUES.MD C5).

**The whole block is entered when *either* class has been given a size**, not when the first has. `Ms8` names
the second class, so testing `blockSize[0]` alone meant it allocated nothing at all while the banner reported
the memory it had asked for and every thread ran the register-resident kernels; `allocMem[1]`'s old default of
1 byte was the other half of the same defect, giving a class that was never named a slice of one byte rather
than of nothing (ISSUES.MD C9). Both `allocMem[]` entries now default to 0, and a class that has threads and
no memory is refused by the record check above.

**An `M` argument resets nothing; `B` and the presets reset both class sizes.** Every `M` used to open by
clearing `memConfig` and `allocMem[0]` and leaving `allocMem[1]` alone, so the two per-class sizes composed in
one order only: `Mn8 Ms8` ended as `{0, 8MB}` and was refused with `-18` and "Only 0 bytes of memory per
thread", while `Ms8 Mn8` and the single-argument `Mn8s8` both ran — and "where the CPU has cores of both, give
both" is exactly how the help text spells the two-class case (ISSUES.MD F1). Nothing is lost by dropping the
reset: each of `M`'s four sub-options assigns `memConfig` together with the size it names, so the last option
to set a property still wins. `en-GB.h:44` documents only `B` and the presets as resetting the memory
configuration, and each of those now clears `allocMem[1]` beside the `allocMem[0]` it sets, so **a new
per-class size added to `M` needs a matching zero in the `B` case, the preset preamble and the defaults
block** — and none in `M` itself. The arena and `resArray.iter` are freed by
`~RESULTS_ARRAYS` (`CPU.h`) — `wmain` leaves by more than a dozen returns, so the frees belong to the object
that owns the pointers, exactly as `GLOBAL_CFG`'s destructor owns its bitmaps (C13, GCS p2). A second `B` on
one command line frees `resArray.iter` before allocating it again, and **the result of that allocation is
checked**: `B` refuses with `-17` on a null, where it used to spawn the threads and let every one of them
write its record count through a null pointer as it ended (ISSUES.MD C2).

**The report buffer is the third run-length allocation, and it is owned rather than freed at the point of
use.** `wstrOutput` is a local of `wmain` sized from the group and thread counts, so neither destructor above
can reach it; `REPORT_BUFFER` (`CPU.h`) is the two-line owner that does — one `wchptrc` and a destructor that
`mfree1`s it — and `wmain` declares a `cREPORT_BUFFER` over the pointer immediately below the `declare1d64z`
that allocates it, above the null check, so a failed allocation destructs on the same path as a successful
one. It had no free at all before (ISSUES.MD C3), and seven error returns stand between the allocation and
the end of the function, which is exactly the arrangement C13 rejected for the arena. **Nothing reads the
owner**: every write still goes through `wstrOutput` itself, and a new error return between the two needs no
free of its own. A fourth run-length allocation should follow the same rule — an owner, not a free per
return.

Of the five pointers handed to each thread, `p0`–`p3` are four views of the *same* arena address at different
element strides (only `p4` is advanced past them, and only for the `ALU_` combinations). The seeding pass must
therefore write no more than one of them, which is why `wmain` rejects a unit selection naming more than one
of FPU/SSE4.1/AVX2/AVX-512 (ISSUES.MD C1).

The pass itself is one `SeedRecords<UNIT>` call per selected unit rather than a loop in `wmain`, because
filling a slice is a store of the unit's own width repeated — `p0[os] = value[0][k].avx512` is a 512-bit move
— and `wmain` compiles at the SSE2 baseline. Each lives in its unit's translation unit with everything else of
that width (ISSUES.MD H4). The seed is passed **by reference** for the three vector widths: MSVC passes a
vector of more than 16 bytes by address anyway, and a reference says so without the caller having to form the
value in a register it may not have.

### Configuration bit-fields

`cfg.procUnits` and `cfg.procSync` (`GLOBAL_CFG`, `CPU.h`) are copied verbatim into each `THREAD_CFG` and are
the decoder ring for most of the code:

```
procUnits  bit 0 ALU   1 FPU   2 SSE4.1   3 AVX2   4 AVX512   5 L1$   6 L2$   7 L3$
procSync   bit 0 R-R   1 Par   2 Stag     3 T-Sync 4 Constant 5 Fixed pulse  6 Sweep  7 Benchmark
```

Cache bits 5–7 are parsed and displayed but **not implemented** (the README and help text say so explicitly).
Benchmark mode (bit 7) additionally records each thread's **record** count into `resArray.iter` and prints a
KUPS score weighted by the 64-bit lanes a job cycle updates. Two rules hold that score together, and both
were broken (ISSUES.MD E12). `resArray.iter` counts records, not loop iterations: a memory-backed iteration
is four records and a register-resident one is a single record, and an idle iteration of a pulsed run is no
records at all, so `j` advances by `recStep` inside the compute branch alone. And the weight is derived the
way `JobCycle` dispatches — the widest vector unit selected, plus the ALU's lane where bit 0 is set — which
makes it equal to the arena's own `recSize / 8` for every unit selection `wmain` accepts. It was
`(procUnits & 0x1F) >> 1`, which is a vector width only for the three combinations `B` installs, and even
there was short by the ALU lane. The rate is `fl64` over `cfg.tics`, never over a count of whole seconds:
that count is 0 for `B Tt0.5` (E4). **Changing any of this changes the published score**, and figures from
different builds are not comparable — the current one is 4.5× what the same run reported before E12.

`procUnits` bits 1–4 select *one* unit: `wmain` rejects a selection carrying more than one of them with `-16`,
and one carrying none of bits 0–4 with `-15`, both before the arena is allocated. Bit 0 (ALU) is orthogonal and
stacks with any of them. Dispatch and arena sizing already reduced a wider selection to the ALU bit plus the
widest other unit, so the extra units never ran: they only corrupted the arena seeds of the unit that did —
a false `!Fail!` — while their own results row was graded `.Pass.` against silicon never exercised.

`procSync` bits 4–6 are the mutually exclusive timing modes, and each of `T`'s `C`/`F`/`S` sub-options clears
all three before setting its own — so the last one given wins, and a `T` argument that carries only a duration
or a start-up delay leaves the mode untouched. That is what keeps `B Tt120` and `-1 Tt600` constant-load runs
(ISSUES.MD F1); do not hoist the clear back out to the top of the option's parse loop.

**What each mode implies is applied after the whole command line has been read**, not from the sub-option's
own parse arm, and there are two such implications: a `procSync` naming no mode at all becomes Constant, and
a sweep's `cfg.offTime` becomes 0. `Ts` used to clear the off-time as it was parsed, which made `Ts]500` and
`T]500s` different runs and did nothing whatever for presets `-6` and `-7`, which set the sweep bit without
passing through that arm — so the two ways of asking for a sweep ran cycles 900 ms apart (ISSUES.MD E7).
Both statements sit with the `-12`/`-13`/`-14` checks in `wmain`, above the `-14` check because it reads the
mode they settle.

### Command-line parsing

Four functions in the `//--- Command-line parsing ---//` group of `CPU.h` are the whole of the option
grammar below the level of the letter, and none of what they replaced should be reintroduced inline.

`ParseWholeNumber` and `ParseDecimal` are **the only route a number takes into `cfg`**. Each requires the
value to begin where the option says it does — a digit or a sign, never the whitespace `wcstoll` and `wcstod`
would otherwise skip past to find a value in the next field — compares `stopChar` against that first
character, and range-checks against the `OPT_*` bounds beside them before assigning anything. A failed read
is `-24`, not a silent zero: an option with no value used to produce a zero-length test, a zero pulse
on-time or a zero allocation, all three of which report `.Pass.` (ISSUES.MD F4, A5, A6). They also set the
parse index **absolutely**, `ui32(stopChar - str) - 1`, and `j` in `wmain` is a `ui32` for the same reason —
the old `j += ui8(stopChar - &argv[i][j] - 1)` moved the index backwards on a value field longer than 255
characters, and the option loop then could not terminate.

`CoreMapChar` and `ParseCoreMap` read the `Uc` and `Ut` maps. Three properties are load-bearing:

- **Every index comes from the topology, never from the character's position in the argument.** `Ut`
  character *n* is bit `n & 63` of group `n >> 6`; `Uc` character *n* is the *n*-th physical core, whose span
  of virtual cores is read from `cfg.sys.coreSibling` exactly as `SetSMTLoading` expands one. The old
  expressions — `coreMap[(j - 1) >> 3] |= 0x01ull << j`, and `0x03ull << (j << 1)` for `Uc` — put the first
  character on core 2, addressed a 64-bit word as though it held eight cores, assumed two virtual cores per
  physical core at a fixed stride, and shifted by 64 or more on a long enough map (ISSUES.MD F2, C10).
- **The map is cleared first and masked with the cores the enumeration reported**, so it is the whole of the
  selection and cannot name a core the machine does not have. `wmain` refuses a selection of none with `-26`
  before `threadCount[2]` is divided by (C8).
- **The map ends at the first character that is not part of one**, which is what lets a further `U`
  sub-option follow it — the documented `Uc!.!!...!a` spelling. That is why the enabling characters are an
  explicit set in `CoreMapChar` rather than "any other character": the two cannot both be true. Add a
  character to either set and `en-GB.h` and `README.md` have to say so.

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
only implementation. Adding a language means writing `<code>.h` with `wstrInstructions_*`, `wstrMessage_*[43]`
and `wstrInterface_*[13]`, then extending both `translations.h` and the `L` case in `CPU.cpp`. The three
globals are *declared* in `translations.h` and defined, pointing at English, in `CPU.cpp`: the `L` option
writes them at run time, so a definition in the header would have given each translation unit a copy and left
the four `CPU_jobs_*.cpp` units reading a language nobody selected (ISSUES.MD H9).

The `L` option's selection logic was inverted — `lstrcmpiW` returns 0 when the codes *match*, so testing it
directly selected a language exactly when the argument did not name it, and every input resolved to English
(ISSUES.MD F7). It is now `!lstrcmpiW(...)`, and a code this build does not carry prints `wstrMessage[39]`
and stays in en-GB rather than reaching one by accident. The code itself is copied
into `wstrLang[6]`, and the copy is clamped to that capacity — `lstrcpynW`'s third argument is the size of the
*destination*, so passing the argument's length overran the globals that follow it (ISSUES.MD C6). A longer
code is therefore truncated to five characters; widen `wstrLang` if a language ever needs more. Some strings
are still
hard-coded outside the tables — `wstrUnitsCPU`, `wstrSyncCPU` and `wstrPass` in `CPU.h` (the last is flagged
`///--- Modify for translation ---///`). `wstrKernelName` in `CPU.h` is also outside the tables, but
deliberately: its entries are C++ identifiers naming the job kernels, and are not translated.

**Every write to `stdout` is wide, and a new one must be too.** Both languages give a stream an orientation
at its first use and forbid byte I/O on a wide-oriented one; five writes were `printf` — `GenerateValues`'
progress dot (`CPU.h`), `ComputationPulse`'s (`CPU_methods.h`) and three newlines in `CPU.cpp` — beside the
`wprintf` that carries every message and the report, and worked only because the MSVC CRT deliberately
implements no orientation at all (ISSUES.MD F4). They are `wprintf` now. `_setmode(_fileno(stdout),
_O_U16TEXT)` is deliberately *not* set: that changes how the console renders each of these writes, where the
defect was the mixing alone.

## Known constraints and latent issues

Verify against current source before relying on any of these:

- **`MAX_THREADS` is a real ceiling, at 512.** Enumeration and affinity are now group-aware end to end
  (ISSUES.MD G3), so the 64-virtual-core limit is gone, but the thread-indexed tables are still fixed at 512
  entries and a core beyond that is refused with a warning rather than tested.
- **A `U` core map is the whole of the selection**, and a core it does not name is not utilised (ISSUES.MD
  F2). That is a change from the parser it replaced, whose characters could only modify the full map the
  enumeration produced; a short map therefore selects fewer cores than it used to, and a map of all-disabled
  characters is refused with `-26` rather than reaching a division by zero (C8).
- **On a hybrid CPU, `Mn`/`Ms` and the two cache records mean "efficiency core / performance core"**, not
  "non-SMT / SMT". That is now what the code computes (`CoreClass`, ISSUES.MD G9) rather than what it
  accidentally did, and the run says so through `wstrMessage[34]`, but the option letters are still `N` and
  `S` — renaming them would break every existing command line.
- **A `-19` abort does not wait for the threads it already spawned.** Those threads are running the test they
  were given, which can be hours long, so the handles are released rather than waited on and `~RESULTS_ARRAYS`
  frees the arena under them on the way out. The process is exiting with an error either way; every path that
  goes on to *read* a result joins first (ISSUES.MD D8).
- Cache-targeting (`I1`/`I2`/`I3`) is accepted, displayed, and does nothing.
- **The program is no longer confined to one translation unit** (ISSUES.MD H9), and the five it now has are
  compiled at four different instruction sets. A new file has to be added to `CPU.vcxproj`'s `ClCompile` list
  with the ISA and `Optimization` metadata of the unit it belongs beside; a new *header* still has to be added
  to the `ClInclude` list, which the six vendored headers are still absent from (H8).
- **`memory management.h` carries AVX2 and AVX-512 copy and stream helpers**, and `CPU.h` includes it, so a
  future caller of `Copy512`/`Stream512` from `CPU.cpp` would put an EVEX opcode back into the baseline unit.
  They are `inline` and unused today, so nothing is emitted; the guard belongs upstream (ISSUES.MD I12). The
  same hazard in `SIMD management.h` — `_mm_abs_pd`, `_mm256_abs_pd` and the fused multiply-add macros — has
  been guarded upstream (I1, I2), so that header is no longer a route by which a baseline unit can be handed
  an instruction it cannot execute. `memory management.h` still is.

### Result comparison must stay bit-exact

Each of the three vector units defines one `ResultsMatch` overload — `fl64x2` in `CPU_jobs_SSE.cpp`, `fl64x4`
in `CPU_jobs_AVX.cpp`, `fl64x8` in `CPU_jobs_AVX512.cpp`, each beside the job cycles that call it (ISSUES.MD
H4) — that XORs the computed and expected vectors and tests the difference against zero. **Every** SSE, AVX2 and AVX-512 job
cycle goes through them. `CPU_jobs_standard.cpp` carries a fourth, scalar overload for the FPU cycles, and
the ALU cycles compare `si64` with `!=`, which already examines every bit — so all five units now
answer the same question: are these two bit patterns identical?
The FPU cycles were the last to be brought to it: they compared `fl64` with `!=`, which is the same numeric
predicate the AVX-512 cycles were corrected for below, wrong in the same two directions (ISSUES.MD A2). The
scalar overload is a `memcmp` of `sizeof(fl64)` and **not** the `(ui64&)a != (ui64&)b` alias that entry
proposes: reading a `fl64` through a `ui64` lvalue is precisely the aliasing a compiler may assume cannot
occur, and one that does assume it folds the comparison — built at `-O2`, that spelling answers "identical"
for `+0.0` against `-0.0`, reintroducing the defect it was meant to remove. Both configurations set
`IntrinsicFunctions`, so a constant-size `memcmp` is expanded in place rather than called.
Do not substitute `_mm_testc_si128` or `_mm256_testc_pd` here, and do not reach for `AllTrue` in
`common functions.h`: `PTEST`'s `CF` is a *subset* test (a bit that should be 0 turning 1 passes), and
`VTESTPD` examines only each lane's sign bit — every job output is positive, so it can never fail. Both
spellings were live in the shipped code and made the SSE and AVX2 verdicts partly or wholly blind.
Do not substitute a floating-point predicate either. The AVX-512 cycles used
`_mm512_mask_cmpneq_pd_mask`, which asks a question about *numeric values* rather than bit patterns and so
errs in both directions: `+0.0` and `-0.0` satisfy it as equal, hiding a sign-bit flip in a zero lane,
while two bit-identical NaNs satisfy it as unequal, which would report a fault where the bits agree
(ISSUES.MD A11). The `fl64x8` overload uses `_mm512_test_epi64_mask` on the XOR rather than `testz`, which
AVX-512 does not provide; an empty mask is the match.
