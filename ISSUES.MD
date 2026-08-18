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

### A1. Cache-targeting options `I1`/`I2`/`I3` are parsed and displayed but implemented by nothing — **Medium** — *carried forward (old A13)* — **RESOLVED**

`CPU.cpp:167–175` parsed the three cache letters into `procUnits` bits 5–7 and the banner (`CPU.cpp:899`)
printed `CL1`/`CL2`/`CL3` for them, but no code read those bits: dispatch masks with `0x1F`
(`CPU_methods.h:29`), the arena switch masks with `0x01F` (`CPU.cpp:791`), and the cache sizes the topology
walk files (`CPU.h:797–809`) had no consumer. The help text disclosed "NOT YET IMPLEMENTED", but a saved
results file still carried a banner naming `CL3` as part of the tested configuration.

**Resolved** by the `//--- Cache-test block sizing ---//` group in `CPU.h`, per `DESIGN_cache_block_sizing.md`.
The bits now select a per-class memory-block size — the blocks of every selected thread sharing one instance
of the named level fill it, and together span twice the level below — leaving dispatch, the kernels, the
arena layout and `cpu.values` untouched. A level the system does not report is refused with the new exit code
`-27` rather than given a fallback size; a thread count that makes residency impossible is warned about and
run at the defeat bound; and an explicit `M` still wins, with a warning where it falls outside the level's
window. `B` was deliberately left as it stands at HEAD — no cache bit, 8MB per thread — so its KUPS score is
unchanged and comparable across this change; the design's decision 2 for `B` was superseded by
`9847c69`/`c1af2dc`.

## B. Vendored headers

These files are copies of the author's general-purpose library. Defects here are latent for PITC unless
noted — but they are live for any other client, and `CPU.h` includes two of them into every translation unit.

### B1. `refp1a1` expands to a cast to an array type, ill-formed on any use — **Low** — **RESOLVED**

`typedefs.h:577`: the macro produces a C-style cast whose type-id is an array type, which C++ forbids —
every expansion is a compile error on every compiler, unlike its `defpa`/`refpa` companions. No PITC file
uses it.

### B2. `CLASS_TIMERS` never frees its `salloc`'d timer table — **Low** — **RESOLVED**

`class_timers.h:121`: the 2 KB `TIMER_VARIABLES[MAX_TIMERS]` block is allocated by an in-class initialiser
and the struct has no destructor — contrary to GCS p2 and the repo's own destructor-owned-pointer pattern
(`GLOBAL_CFG`, `RESULTS_ARRAYS`). PITC never instantiates `CLASS_TIMERS`.

**Resolved** by `~CLASS_TIMERS(void) { mfree1(timer); }`, which is the same two-line owner `REPORT_BUFFER`
already is in `CPU.h`. `mdealloc` ignores a null pointer, so an object whose `salloc` failed destructs on
exactly the path a successful one does — the property `GLOBAL_CFG`, `RESULTS_ARRAYS` and `REPORT_BUFFER` all
rest on. `CLASS_TIMER`, the single-timer sibling above it, owns nothing and correctly still has no
destructor.

The destructor could not be added alone. A class that releases an owned pointer and leaves the *implicit*
copy constructor in place hands two objects one address and frees it twice, so the one-line fix would have
traded a leak for heap corruption — and this header's defects are, as the section head says, live for clients
other than PITC. Both copy operations are therefore `= delete`d beside the destructor. Copy assignment was
already implicitly deleted, `timer` being `TIMER_VARIABLES *const`, and is stated anyway so that relaxing
that constness cannot reintroduce it silently; a user-declared destructor suppresses both move operations, so
all four special members are accounted for. Nothing usable is withdrawn: before the destructor existed a copy
duplicated a pointer to one table and leaked both objects' claim on it. The deletions name `cCLASS_TIMERS`,
a new file-scope `typedef const CLASS_TIMERS` above a forward declaration of the struct, because GCS r2/t2
put the qualifier in the typedef and t3 forbids mixing that with a raw `const CLASS_TIMERS &` at the
parameter — the same `c<TYPE>` idiom as `cREPORT_BUFFER` and `cRESULTS`.

Verified by compiling the struct's real text against the real `typedefs.h` under clang (`-fms-extensions
-fdeclspec`, an allocator shim standing in for `memory management.h`, whose MSVC vector-union initialisers
clang cannot parse): the 2 KB figure and `alignof(CLASS_TIMERS) == 16` both hold across the new forward
declaration, and the four suppressions and `CLASS_TIMER`'s continued trivial destructibility and copyability
are `static_assert`ed. Under g++ with AddressSanitizer the same text exercising `Create`, `Update`, `UpdateAll`
and both constructors reports no leak and no double free, where `HEAD`'s copy of the header reports
"4096 byte(s) leaked in 2 allocation(s)" for the same two objects. The target toolchain (MSVC v143) was not
available, so this is evidence about the C++, not about the shipped build.

## C. Job kernels

### C1. The "SSE4.1" unit exercises no SSE4.1 arithmetic, and the "AVX2" unit no AVX2 arithmetic — **Medium** — *carried forward (old J3)* — **RESOLVED**

`JobSSE`/`JobALU_SSE`/`JobMemSSE`/`JobMemALU_SSE` (`CPU_jobs_SSE.cpp:48–247`) use only
`_mm_set1_pd/div/sqrt/add/sub/mul/and/set1_epi64x` — all SSE2. The unit's sole SSE4.1 instruction is `PTEST`
in `ResultsMatch` (:261) and the completion poll — the comparison, not the computation — yet an SSE4.1-less
CPU is refused the whole unit with `-11`. Likewise the AVX2 unit's kernels use only `_mm256_*_pd` forms that
are AVX1; the AVX2-ness is the compiler's encoding, not the arithmetic under test. A user running `Is` or
`Iv` to exercise the instruction class named on the tin is testing less than the name says.

**Resolved** by lowering the requirement to the substance rather than raising the substance to the
requirement, which is what leaves `cpu.values` untouched: no kernel's arithmetic changed, so
`KernelFingerprint` — which hashes the lanes the golden ladder produced, not the route it took or the names
of the kernels that produced them — is unchanged, and a file generated before this change is still read by a
build made after it.

Three instructions carried the whole of the discrepancy, and each is replaced by the same predicate at the
unit's own instruction set. `ResultsMatch` in `CPU_jobs_SSE.cpp` and the `AllFalse` in `ThreadsRunningSSE`
were `PTEST`; both now go through one `AllBitsZero128` fold — `PCMPEQD` against a zeroed vector read back
through `PMOVMSKB`, all sixteen mask bits set exactly when all 128 bits are zero, which is neither more nor
less than `PTEST` examined. `ResultsMatch` in `CPU_jobs_AVX.cpp` formed its difference with `VPXOR` on `ymm`
operands, the one AVX2 instruction in that unit, and now forms it with `VXORPD`; `VPTEST` on `ymm` operands
is AVX1, so the test itself needed no change. The AVX-512 unit is untouched.

With the instructions gone the gates follow: `cfg.sys.cpuSSE4_1` is `cpuSSE2` and `cpuAVX2` is `cpuAVX`, read
from `PF_XMMI64_INSTRUCTIONS_AVAILABLE` and `PF_AVX_INSTRUCTIONS_AVAILABLE`; `CPU_jobs_AVX.cpp` builds at
`/arch:AVX` and its guard gained an `__AVX2__` upper arm, because H3's hazard — a unit compiled above the ISA
`wmain` tests for before dispatching it — now applies to the Sandy Bridge, Ivy Bridge and Bulldozer parts
this unit reaches. The kernels, job cycles, validators and seeding pass of that unit are spelt `*AVX` rather
than `*AVX2`, so `wstrKernelName` and the `-22` messages name what the unit is. The help text, `README.md`
and `wstrMessage[12]`/`[13]` say `SSE2` and `AVX`.

Two consequences worth stating. `RunGoldenLadder`'s middle branch is now entered on AVX1, so a pre-Haswell
AVX machine walks the 256-bit ladder where it walked the 128-bit one — the element-wise equivalence
`ValidateLadderAVX` proves is exactly what makes the two interchangeable, and `W` proves it there before
writing anything. And the `-11` check for `Is` can no longer fire on any machine this build runs on, SSE2
being part of the x64 architecture `CPU_build.h` requires; it is kept for symmetry with the other two units
and because `ThreadsRunningScalar` is selected by the same flag.

What C1 named as a *substance* gap is closed only in the sense that the name now matches the arithmetic. The
kernels still exercise no instruction above SSE2 and AVX1, so exercising SSE4.1 or AVX2 arithmetic
specifically remains future work — and it remains work that would change the golden values.

## D. Documentation and localisation

### D1. Failure-report and results-table strings remain outside the translation tables — **Low** — *carried forward (old K9)* — **RESOLVED**

Beyond the disclosed `wstrUnitsCPU`/`wstrSyncCPU`/`wstrPass`, the per-unit value formats inside `Failed()`
(`CPU.cpp:72–91`) and the results-table row labels and separators (`CPU.cpp:1029–1051`) are literals the `L`
mechanism cannot reach — the one output a failing CPU produces is only partly translatable.

**Resolved** by moving all three of the disclosed tables and nine format strings into `en-GB.h`, and by
measuring the one remaining hard-coded width from the translated string it belongs to. `wstrUnitsCPU`,
`wstrSyncCPU` and `wstrPass` are per-language tables now, reached through three more pointers in
`translations.h`; `wstrInterface` grows from 13 entries to 22, taking `Failed()`'s five per-unit value lines
as `[13]`–`[17]` — in the order of its own `unit` argument, so `[13 + unit]` is the entry — and the four
results-table rows as `[18]`–`[21]`, one per value width. The `L` case repoints all six pointers together.

Three things this deliberately did **not** move, each because translating it would break something the
program depends on: `wstrKernelName`, whose entries are C++ identifiers a user greps the source for; the
`'!'`/`'.'` bitmap alphabet, which mirrors the `Uc`/`Ut` map characters the parser accepts, so that a printed
bitmap can be pasted back into a `Ut` map; and the `"cpu.values"` filenames, which are a cross-locale file
contract and are already interpolated into the messages that name them. The table-rule geometry
(`"----"`, `"+--"`, the four-space pads) stays as well: each extra `ui64` lane is 16 locale-invariant hex
characters matched by exactly four rule units, and the part of that line a language does own already sits
inside `wstrInterface[9]`.

Two couplings the move exposed are now held by the code rather than by memory. The label tables are pointed
at by array type (`cwchar4ptr`, `cwchar8ptr`) rather than by `cwchptrcptr`, so a language whose unit label is
four characters wide is a compile error at the line that wrote it instead of a report whose columns silently
shift — the banner pads in whole four-column slots and the results table gives the ProcUnit column a fixed
ten-character cell. And the thread bitmap's continuation indent, a literal 15 spaces with a matching
`c -= 15`, is measured from `wstrInterface[7]` — the characters after its last newline — because 15 was a
property of the English label `"Thread bitmap: "` and of nothing else; `outChars` adds the measured indent to
its per-group term for the same reason.

The en-GB report is byte-for-byte what it was: every moved literal was relocated unchanged, and the measured
indent evaluates to the 15 it replaced.

### D2. `translations.h` cannot express a second language without code edits, and the console path would garble one — **Low** — *carried forward (old K10)* — **RESOLVED**

Adding a language still means writing `<code>.h`, editing `translations.h` and extending the `L` case — and
the help text's "Recognises ISO 639-1 language codes" oversells a hard-coded en-GB/en-US comparison whose
example is a BCP-47 tag. Additionally, no `_setmode(_O_U16TEXT)`/`SetConsoleOutputCP` call exists anywhere,
so `wprintf` output converts through the ANSI code page: pure-ASCII English survives, but any future
translation with characters outside the console code page will print mojibake through the very mechanism
built to carry it.

**Resolved** in both halves. Selection is data-driven: `translations.h` carries a `LANGUAGES` registry — one
row per language, each row the code the `L` option matches and all six tables a match repoints together — and
the `L` case walks it naming no language of its own, so a language is a header and a row rather than an edit
to `CPU.cpp`. The candidate code is parsed into a local, so a code no row carries now leaves both the
selection and `wstrLang` naming the language actually active (`wstrMessage[39]` reworded to match), and the
help text and `README.md` state the real contract — a case-insensitive match against the codes the build
carries — in place of the "ISO 639-1" claim neither the parser nor the `Len-GB` example ever honoured. The
registry is an `inline` table, so the three prose tables in `en-GB.h` became `inline` alongside the three
label tables that already were: an inline definition must mean one entity in every translation unit, which an
internally-linked const table is not.

The console path is UTF-8 where the CRT can match it: `wmain` asks for the `.UTF8` `LC_CTYPE` locale and,
only where that grant succeeds, sets the console output code page to UTF-8 beside it — the two must agree,
because `wprintf` converts wide text through the former and the console decodes the bytes through the
latter — restoring the original code page through a destructor-owned `CONSOLE_CODE_PAGE` (`CPU.h`) on every
return, the console being machine state that outlives the process. A UCRT too old for `.UTF8` (pre-1803)
gets the regional locale exactly as before and an untouched console: ASCII English behaves as it always did.
Two neighbouring gaps the same defect would have surfaced through are closed with it. Both configurations
now compile with `/utf-8`, so a language header's `L"..."` literals mean the same wide characters whatever
ANSI code page the build machine uses — every current source is pure ASCII, so today's binaries are
unchanged. And the `Oa` results path, whose `CP_ACP` conversion silently best-fitted any character the ANSI
code page cannot carry (its comment claimed the conversion "reports a character it cannot represent as a
failure", which is true only of `CP_UTF8` under `WC_ERR_INVALID_CHARS` — now passed on the `O8` path),
converts under `WC_NO_BEST_FIT_CHARS` and reports a substitution through the new `wstrMessage[46]` warning
while still writing the file: the report of a run of hours is not discarded over its prose, and `O8`/`O16`
carry every translation intact.

### D3. Dead configuration declarations misdescribe the program — **Low** — *carried forward (old K14)*

`GLOBAL_CFG`'s in-class defaults `procUnits = 0x04` / `procSync = 0x02A` (`CPU.h:86–87`) are overwritten
unconditionally by `CPU.cpp:140–141` before any argument is read, so the struct lies about the defaults; and
four `THREAD_CFG` fields (`packetSizeRAM`, `maxTics`, `inactiveTime`, `records32`) are written in the spawn
loop and read by nothing.
