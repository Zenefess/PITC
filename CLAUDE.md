# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**PITC — Pulsed Integrity Tests for CPUs**: a Windows x64 console application that verifies CPU
*computational integrity* rather than merely loading the CPU. Its distinguishing feature is **pulsed**
testing — bursts of compute separated by idle gaps, which exercises the C-state/voltage/frequency
transitions that constant-load stress tests never touch. Constant stress and a benchmark mode are also
provided. Single-author project (David William Bull), MIT licensed, no dependencies beyond the Win32 API
and MSVC intrinsics. Everything the program computes is graded **bit-exactly** against golden values in a
`cpu.values` file the program generates for itself — any change that alters one bit of kernel output is a
functional change.

## Build

Windows only, MSVC only, x64 only. VS 2022 (toolset v143), Windows 10 SDK. No `.sln` is checked in —
build the project file directly:

```
msbuild CPU.vcxproj /p:Configuration=Release /p:Platform=x64    ->  x64\PITC.exe
msbuild CPU.vcxproj /p:Configuration=Debug   /p:Platform=x64    ->  x64\Debug\PITC.exe  (ASAN enabled)
```

The project file is `CPU.vcxproj` but the binary is `PITC.exe`. `CPU_build.h` hard-`#error`s any other
arrangement: not-x64, clang/clang-cl, non-MSVC, or a build without `/fp:strict` all refuse to compile.
This cannot be built or run in a non-Windows environment; there is no CI, no test suite, no
`.clang-format`/`.editorconfig` — verification is running the program itself (see below).

### Deliberate build settings — do not "fix" them

Several `CPU.vcxproj` settings look like mistakes but are load-bearing:

- **`Release|x64` sets `<Optimization>Disabled</Optimization>` globally.** The job kernels are the work
  being measured; the optimiser must not collapse or reorder them. The four `CPU_jobs_*.cpp` files
  override this with `Optimization=Custom` (no `/O` switch at all), pinning kernel codegen identically in
  both configurations.
- **Global `EnableEnhancedInstructionSet` is `StreamingSIMDExtensions2`** (the x64 baseline), raised
  per-file only on `CPU_jobs_AVX.cpp` (`/arch:AVX`) and `CPU_jobs_AVX512.cpp` (`/arch:AVX512`). This is
  what keeps AVX/AVX-512 opcodes out of code paths dispatched to CPUs that cannot execute them.
  `CPU_jobs_SSE.cpp` and `CPU_jobs_standard.cpp` deliberately carry **no** per-file ISA setting.
- **No per-file setting carries a `Condition`** — Debug must compile the vector units at their own
  `/arch` exactly as Release does.
- **`FloatingPointModel=Strict` is pinned in both configurations** and enforced by `CPU_build.h`
  (`_M_FP_STRICT`). `/fp:strict` (no FMA contraction, no reassociation) is what makes a `cpu.values`
  file from one build valid under another. `WholeProgramOptimization` is explicitly `false`; `/utf-8` is
  passed in both configurations. None of these may be relaxed.

Each ISA unit backstops its required `/arch` with an `#error` guard at the top of the file (testing
`__AVX__`/`__AVX2__`/`__AVX512F__`), so a lost or raised per-file setting is a compile error, not a
silent mis-build. Leave the guards, and leave the `Win32Proj` keyword in the vcxproj (it is a project
type, not a platform).

## Running and verifying changes

Every test grades computed values against `cpu.values`, which is not shipped. Generate it once, in the
directory the executable runs from, before the first test of a build:

```
PITC.exe W
```

`W` cross-checks all kernels against each other first, then verifies every entry it writes over 65,536
iterations — expect minutes, not seconds. The file records the build and kernel identity
(`KernelFingerprint()`), so a file from a different build is refused with exit code `-21`.
**Regenerate `cpu.values` after any change to a job kernel.** The file is *not* machine-specific: one
written on any CPU verifies on any other, whatever its vector width — that portability is a maintained
invariant, not an accident.

Running with no arguments prints the full option reference and returns `2`. Exit codes: `0`/`1`/`2` are
outcomes, negatives (`-1`…`-27`) are refusals; the full table lives in `README.md` and in
`wstrInstructions_English` in `en-GB.h` — those two must be kept in sync. Options are applied in order,
last-wins; presets (`-0`…`-9`) and `B` (benchmark) reset units/memory, so they go before `I`/`M` options.

## Architecture

### Source layout

- `CPU.cpp` — global object definitions (order matters, documented at the top), the `JOB_CYCLE` dispatch
  table, `Failed()`, and `wmain` (parse → validate → load `cpu.values` → size memory → spawn threads →
  wait → grade → report).
- `CPU.h` — nearly all logic, as `static` functions: option parsing, topology enumeration, cache-derived
  sizing, pulse timing primitives, the thread-completion bitmap, `cpu.values` I/O, kernel cross-check
  machinery, `GenerateValues`.
- `CPU_methods.h` — `ComputationPulse`, the worker-thread body (pulse shaping, dispatch loop).
- `CPU_job_cycles.h` — extern declarations for the eighteen `JobCycle*` wrappers and `JOB_CYCLE`.
- `CPU_jobs_standard.cpp` / `CPU_jobs_SSE.cpp` / `CPU_jobs_AVX.cpp` / `CPU_jobs_AVX512.cpp` — one
  translation unit per ISA, each compiled at its own `/arch` (see above).
- `CPU_build.h` — compile-time environment guards and the FP-model constants baked into `cpu.values`.
- `translations.h` + `en-GB.h` + `fr-FR.h` + `zh-CN.h` — localization (see below).
- `typedefs.h`, `vector structures.h`, `memory management.h`, `common functions.h`, `class_timers.h`,
  `SIMD management.h` — vendored, header-only support library (own version numbers in their prologs).
  Layering: typedefs → {vector structures, SIMD management} → common functions → memory management →
  class_timers. **Four of these filenames contain spaces** — always quote them in shell commands and
  keep the quoted `#include "memory management.h"` form.

### ISA isolation — the structural rule

The whole program is organised so that no instruction above a CPU's capability can reach it:

- Feature detection (`cfg.sys.cpuSSE2/cpuAVX/cpuAVX512`, via `IsProcessorFeaturePresent`) gates dispatch
  in `wmain`; requesting an unsupported unit exits `-11` before any kernel runs.
- **Anything that moves or compares a value wider than 64 bits belongs in the translation unit of its
  width** — never in `CPU.h`, `CPU.cpp` or `CPU_methods.h`, which must stay at the SSE2 baseline.
  Per-width code in the units includes the `ResultsMatch` overloads and the `ThreadsRunning*` polls.
- Nothing on the path from `wmain` to the ISA gate may execute an instruction outside the x64 baseline:
  the bit helpers (`LowestSetBit64`, `SetBitCount64`, …) are deliberately shift-and-mask, not
  `__popcnt64` (POPCNT is ungated by `/arch` and would crash pre-Nehalem CPUs at the help screen).
- The support headers contain traps for baseline code: `AllTrue`/`AllFalse` over `cui128` in
  `common functions.h` are PTEST (SSE4.1) — the SSE unit uses its own `AllBitsZero128` instead — and
  `memory management.h` carries AVX-512 copy/stream helpers no baseline unit may call.

### Kernels and the golden ladder

All four units implement the *same arithmetic* at different widths: an FP chain of sqrt/divide/add
(with `fabs` as an explicit sign-bit mask) and an integer ALU chain, 16 iterations × `UNLOOPx4`.
Two cross-check layers, both bit-exact and run by `PITC.exe W` before generating anything:

- **Family check** (per unit): combined (`JobALU_*`) and memory (`JobMem*`) kernels must reproduce the
  register-resident kernel of their unit exactly.
- **Ladder check** (across widths): every vector kernel must reproduce `JobFPU` **lane for lane** over
  the same seeds. This is what makes `cpu.values` portable across vector widths.

Consequences:
- Any FP-arithmetic change must be made in **all four units identically**, then `cpu.values`
  regenerated. The ALU chain is textually identical in all four units and must stay `ui64` (defined
  wrap), accessed through the `ui64 &v = (ui64&)y` alias (copying to a local would delete the memory
  traffic the Mem kernels exist to generate). The `i < 8` shift-direction predicate against a counter
  that runs to 15, and the `acc` accumulator folded once at the end (`x *= acc`), are deliberate
  fault-detection features — do not "clean them up".
- Comparisons stay **bit-exact**: scalar `ResultsMatch` is `memcmp` (not a `ui64&` alias — strict
  aliasing breaks it at `-O2`), vector forms are XOR + full-width zero tests. Never substitute a
  floating-point equality (hides ±0.0 flips, false-fails identical NaNs) or a `testc` subset test.
- The `#undef` before the `_mm_abs_pd`/`_mm256_abs_pd` `#define`s in the SSE/AVX units stays —
  `SIMD management.h` defines these names below AVX2 and the units must own their definitions.
- Dispatch is `JOB_CYCLE[2][32]` (`[memory?][procUnits & 0x1F]`), indexed **without range-checking** in
  `ComputationPulse` — all 32 columns must stay populated, including the unreachable ones.
- Wiring a new kernel touches: its ISA unit (behind the guard), `JobCycle*` wrappers in the same unit,
  externs in `CPU_job_cycles.h` and `CPU.h`, both rows of `JOB_CYCLE`, `wstrKernelName` (respecting the
  `KERNEL_NAME_LADDER` split), the `ValidateFamily*`/`ValidateLadder*` chain, and regenerated golden
  values. Changing `VALUES_HEADER`, `RESULTS` or `MAX_THREADS` means raising `VALUES_FILE_VERSION`.

### Data model and threading

- `RESULTS value[4][MAX_THREADS]` — plane 0 = input seeds, 1 = working values (and arena slice pointers),
  2 = expected (golden), 3 = actual/error. Each 128-byte `RESULTS` union carries one lane set:
  AVX-512 `raw[0..7]`, AVX `raw[8..11]`, SSE `raw[12..13]`, FPU `raw[14]`, ALU `raw[15]`.
  **Nothing but a `JobCycle*` wrapper writes `value[3]` during a run**; a new worker→wmain signal needs
  its own storage and an interlocked write (like `generateError`), never a sentinel in a result plane.
- `MAX_THREADS` = 512; `MAX_GROUPS` = 64 is a separate constant with a different meaning. Topology
  handles Windows processor groups, and hybrid CPUs by collapsing all efficiency classes to two (class 1
  = the widest cores); on non-hybrid machines the two classes are non-SMT/SMT. The `Mn`/`Ms` option
  letters keep their spelling on hybrid parts — renaming would break existing command lines.
- Thread completion is one bit per thread, manipulated only with `_InterlockedOr8`/`_InterlockedAnd8`;
  wmain polls through a per-ISA `ThreadsRunning` function reference bound once at startup.
- Timing uses per-thread high-resolution waitable timers. `timeBeginPeriod` is deliberately **not**
  used (a machine-wide tick change would perturb the C-state behaviour under test).
- Locale: `LC_NUMERIC` is pinned to `"C"`; `LC_CTYPE` `".UTF8"` and `SetConsoleOutputCP(CP_UTF8)` move
  together (with Ctrl-C restore); never `LC_ALL`; every console write is wide (`wprintf`).
- File writes follow the crash-safe pattern of the `W` path: write `cpu.values.tmp` (fixed name,
  share-mode 0 — the collision is how a concurrent `W` is refused), treat a failed `FlushFileBuffers`
  as a failed write, delete the temp on every failure path, publish with
  `MoveFileExW(…, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.

### Localization

All user-facing text is reached through six global table pointers (`wstrInstructions`, `wstrMessage`,
`wstrInterface`, `wstrUnitsCPU`, `wstrSyncCPU`, `wstrPass`), repointed together by the `L` option from
the `LANGUAGES[]` registry in `translations.h` (currently `en-GB` and `en-US`, both English, plus
`fr-FR` and `zh-CN`). Strings are addressed by hard positional index — all language headers must move in
lock-step, appending only.
Format-specifier count/order/types, embedded newlines, and layout contracts (4-column unit labels,
10-character ProcUnit cell, no trailing tab on `wstrInterface[7]`) are part of each string's contract;
the label tables are reached through pointer-to-array typedefs precisely so a wrong width fails to
compile. A new language = a new header defining all six tables + one `LANGUAGES[]` row + a `ClInclude`
in `CPU.vcxproj` and `CPU.vcxproj.filters`; the code must fit `wchar[6]` (≤5 characters). `fr-FR.h` and
`zh-CN.h` are the non-ASCII source files, and each carries a `static_assert` that fails the build if
`/utf-8` is ever lost — any further non-ASCII language belongs behind the same guard.
Every column contract above is counted in **rendered console columns**, which stop matching `wchar_t`
once a language is not Latin: `zh-CN.h` writes each of its 3-column labels as one Han character plus one
space, and gives `wstrInterface[7]` a newline of its own because the bitmap's hanging indent is measured
in `wchar_t` and a Han label would indent the later processor groups by half its width.

## Coding conventions

`CONTRIBUTING.md`: all submissions are reviewed against the **Guild Coding Standard v1.1.4**
(`GDC_GCS_v1_1_4.md`). Read it before writing code. The rules you will trip over first:

- 3-space indent, no tabs; ≤150 columns preferred, 180 hard cap.
- Width-encoded types from `typedefs.h`: `ui8…ui64`, `si8…si64`, `fl32`, `fl64`; SIMD as `fl64x4`,
  `ui512`, etc. Const/volatile live in the typedef, not the identifier: leading `c` binds the pointee,
  trailing `c` the pointer (`cui32`, `ui32ptrc`, `cwchptrc`), `v` = volatile.
- Functions are PascalCase; tables, macros and global constants are UPPER_SNAKE.
- Opening brace on the same line (functions: one space before it); `///` with `@param`/`@return` for API
  docs; `//---` / `//==` section banners (this repo opens *and* closes sections with them);
  spreadsheet-style column alignment where it helps; multiple statements on one line separated by
  exactly three spaces.
- Every file carries the r17 prolog block (File/Version/Owner/…/ISA/Thread-safety/License). No history
  in prologs.
- Allocation goes through the alignment-aware wrappers in `memory management.h` (`malloc64`,
  `zalloc1d64`, `declare1d64z`, freed with `mfree`/`mfree1`), per GCS p2.
- Repo style beyond the GCS: hex literals with a leading zero after the prefix (`0x08F`), byte-size
  comments on structs, no exceptions — Win32 return-code checks and one distinct negative exit code per
  failure class, RAII structs only where a destructor must free.

**This project is exempt from the GCS's AVX2 baseline** (CONTRIBUTING.md): a CPU integrity tester cannot
have a hardware baseline above the hardware it tests. Do not "fix" the deviation by raising `/arch`
anywhere; scalar and SSE2 paths are permanent, first-class citizens here.

### Documentation coupling

- A changed default, preset, option letter or exit code is an edit to the instruction text of **every**
  language header (`en-GB.h`, `fr-FR.h`, `zh-CN.h`) **and** `README.md` together.
- A version bump moves `README.md` line 1 and the banner of every language header together —
  PITC-proper files are versioned individually.
