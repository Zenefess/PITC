# PITC — Design: Cache-targeted memory-block sizing (ISSUES.MD A1)

**Audience:** Claude Code, implementing against `github.com/Zenefess/PITC`
**Baseline commit:** `bd15b433ffd54a798fd3a048b9b0c4ef3b372da0` (2026-08-16 16:48:12 +1000)
**Binding standard:** Guild Coding Standard v1.1.4 (`GDC_GCS_v1_1_4.md`, in-repo). All new code conforms; see §8.
**Design owner:** David William Bull. All six design decisions below are settled by the owner; do not reopen them.

> Every `file:line` anchor in this document was verified against the baseline commit above. **Re-confirm each
> anchor against HEAD before editing** — ISSUES.MD itself carries the same instruction, and line numbers drift.

---

## 1. Objective and scope

ISSUES.MD **A1** (carried forward from old A13): the cache-targeting options `I1`/`I2`/`I3` are parsed into
`procUnits` bits 5–7 (`CPU.cpp:224–232`), printed by the banner as `CL1`/`CL2`/`CL3` (`CPU.cpp:1031`,
`wstrUnitsCPU` at `CPU.h:233`), and read by nothing: dispatch masks with `0x01F` (`CPU_methods.h:37`), the
arena switch masks with `0x01F` (`CPU.cpp:909`), and the cache sizes the topology walk files
(`CPU.h:903–919` into `CPU.h:72–73`) have no consumer.

This design gives bits 5–7 their reader: **a sizing pass that derives each thread's memory-block size from
the requested cache level**, so that an `I1`/`I2`/`I3` run is a memory-backed run whose working sets are
resident in — and only exercisable through — the named level. No new kernels, no dispatch changes: non-zero
record counts already flip every thread onto the `JobCycleMem*` path via `JOB_CYCLE[recCount ? 1 : 0]`
(`CPU_methods.h:146,158`), so sizing alone completes the feature.

**Out of scope:** latency-isolating access patterns (pointer chase), new job kernels, `cpu.values` format
changes, dispatch-table changes, per-thread (rather than per-class) block sizes.

### Settled design decisions

| # | Decision |
|---|---|
| 1 | Multiple cache bits: **the highest level given is used** (mirrors the widest-unit-wins rule of `JOB_CYCLE`). |
| 2 | An explicit `M` **overrides** the derived size, with a warning when it falls outside the level's residency window. `B` **drops its hard-coded 8 MB** and takes the derived level-3 size. |
| 3 | An empty level-3 window: **warn and run** at the smallest size that defeats the level below; the warning prints the largest per-instance thread count that would have been level-resident. |
| 4 | Selected-sharer counts come from a **sizing-time `GetLogicalProcessorInformationEx(RelationCache)` re-query**, packaged as a **callable function** (`QueryCacheDomains`, §5.4) — no persistent domain tables in `GLOBAL_CFG`. |
| 5 | A requested level the system does not report is **refused** with a new exit code **`-27`** — never a silent fallback size. |
| 6 | Policy constants: target occupancy **½**, hard ceiling **¾**, defeat multiple **×2**. |

---

## 2. Behavioural specification

### 2.1 Option semantics

- `I1`/`I2`/`I3` set `procUnits` bits 5/6/7 exactly as today. When more than one is set, **the highest level
  wins** (decision 1). At least one processing unit (bits 0–4) is still required; the `-15`/`-16`/`-11`
  pre-flight checks are unchanged.
- A run whose `procUnits` carries any of bits 5–7 and **no explicit `M`** becomes a memory-backed run with
  per-class block sizes computed by §4, `cfg.memConfig = 3` ("derived from the requested cache level"), and
  `cfg.allocMem[0]` set to the true total so the `-17` pre-flight, `malloc64` and the banner's MB figure stay
  truthful.
- **Explicit `M` overrides** (decision 2): if any `M` sub-option was parsed on the command line since the last
  reset (`B`, a preset, or the defaults), the `M` sizes stand. The sizing pass still computes the level's
  per-class residency window and prints a **warning** (`wstrMessage[45]`) for any class whose per-thread block
  falls outside `[lower, ceiling]`. Tracked by a new `cfg.memExplicit` flag (§6.1): set by every `M` sub-option
  arm; cleared by the defaults block, by `B`, and by the preset preamble. This makes `Mc8 B` a derived-size run
  and `B Mc8` an explicit 8 MB run — exactly the "options are applied in order; `B` resets the memory
  configuration; options after `B` override" contract the help text already states.
- **`B`** keeps bit 7 (level 3) in `procUnits` and **no longer sets 8 MB**: its `allocMem` assignments become
  zeros and its sizes come from the derived level-3 window (decision 2). Behavioural consequences to document:
  `B` now refuses with `-27` on systems reporting no L3 (some VMs), and the KUPS score of `B` changes because
  the working set changes — `CLAUDE.md`'s score-comparability note must say so (§7).
- **Presets** are unchanged (their preamble sets 8 MB/core and no cache bits). Because the preamble clears
  `memExplicit`, a preset followed by a cache-bit `I` (e.g. `-1 I3a`) takes derived sizes — presets provide
  defaults, not explicit requests.
- **Missing data** (decision 5): if the requested level — or any input the window needs (the class's L1 data
  size for a level-2 run; the class's L2 instances for a level-3 run) — is not reported by the system, refuse
  with `-27` and `wstrMessage[43]`. This applies whether or not an explicit `M` was given: the *label* on the
  test is unverifiable either way.
- **Empty level-3 window** (decision 3): warn with `wstrMessage[44]` (naming the level and the feasible
  per-instance thread count) and run every class at its defeat bound. The run is then a valid stress whose
  fills spill past the named level; the residency claim is withdrawn by the warning, not by refusing the run.

### 2.2 What is deliberately unchanged

`ComputationPulse` and its `0x01F` mask; the `JOB_CYCLE` table; the record-cursor wrap
(`i = (i >= recCount - 4 ? 0 : i + 4)`, valid at the 8-record floor); the seeding pass; the arena layout,
including the ×8-record rounding (C12) and the per-class `-18` check; the `cpu.values` format, fingerprint and
golden arithmetic (block sizes never enter a kernel); the banner code (it already prints `CL1/CL2/CL3` and the
correct MB figure once `allocMem[0]` is set truthfully).

---

## 3. The sizing model

### 3.1 Access-pattern analysis (why the formulas are what they are)

Per `CPU_methods.h:152`, each thread **cyclically streams its private slice**, 4 records per `JobCycleMem*`
call, wrapping at `recCount - 4`. Each call re-seeds those 4 records from `value[0]`, runs 64 chained
read-modify-write passes, and compares against `value[2]` — a fixed per-thread overhead of roughly eight
64-byte lines (the thread's four 128-byte `value[·][k]` rows) beside the slice itself. Kernel code occupies
L1I, not L1D; the walk already files `L1Code` and `L1Data` separately, and sizing uses **`L1Data`** only.

Two consequences drive every formula:

1. **Residency is an aggregate budget.** A level "holds the test" iff the slices of every *selected* thread
   sharing one instance of that level sum to less than its capacity. Every ceiling therefore divides capacity
   by the largest selected-thread count any single instance serves.
2. **Defeating the level below is also an aggregate budget.** A cyclic sequential stream through a
   (pseudo-)LRU cache misses on essentially every line reuse once the resident set exceeds the cache, so the
   level below is reliably missed when its sharers' combined slices span **twice** it. The defeat bound
   therefore multiplies the **widest per-thread share any instance of the lower level grants** — e.g. four
   E-cores defeating their shared 4 MB cluster L2 need only 2 MB each, while a lone thread on a 2 MB private
   L2 needs 4 MB.

A sequential stream engages the hardware prefetchers, so fills from the target level arrive early. For an
integrity/stress tool that is the desired behaviour — it drives the fill path, tags and data arrays of the
named level at full bandwidth. Latency *isolation* would need a pointer-chase kernel, which is out of scope.

### 3.2 Policy constants (decision 6)

```cpp
constexpr cui64 CACHE_OCCUPANCY_NUM = 1, CACHE_OCCUPANCY_DEN = 2; // Target: the slices fill half the level
constexpr cui64 CACHE_CEILING_NUM   = 3, CACHE_CEILING_DEN   = 4; // Hard cap: never above three quarters
constexpr cui64 CACHE_DEFEAT_MUL    = 2;                          // Slices span twice the level below
```

Rationale: the per-thread fixed overhead is a handful of lines and each slice is one contiguous region of one
`malloc64` block — it maps uniformly across sets with no self-conflict — so ½ with a ¾ ceiling is conservative
without waste. ×2 is the smallest multiple that makes "misses the level below" robust against pseudo-LRU
retention and prefetch.

### 3.3 Measured inputs

Per active class `c` (a class with `threadCount[c] == 0` gets `blockSize[c] = 0` and is skipped):

| Symbol | Meaning | Source |
|---|---|---|
| `L1D_c` | Smallest L1 data size of the class | `cfg.sys.cache[c].L1Data` (`CPU.h:72`) |
| `sibMin_c`, `sibMax_c` | Fewest / most **selected** virtual cores on any one *hosting* physical core of the class | `MinMaxSelectedSiblings` (§5.3) — sibling-bitmap spans ∩ final `cfg.coreMap`, the same pairing `SetSMTLoading` uses |
| L2 instance table | For every L2 instance serving ≥1 selected thread: `{size, n0, n1, coreClass}` | `QueryCacheDomains(2, …)` (§5.4) |
| L3 instance table | Likewise for level 3 | `QueryCacheDomains(3, …)` |

Derived per-thread **shares** (all integer arithmetic, `ui64`):

```
L2_ceilShare(c)   = min over L2 instances filed to c of  size / (n0 + n1)
L2_defeatShare(c) = max over L2 instances filed to c of  size / (n0 + n1)
```

Ceilings always take the **smallest** share (worst-cased thread must fit); defeat bounds always take the
**largest** share (best-cached thread must still be defeated).

### 3.4 Per-level formulas

`floor8 = 8 · recSize` is the structural floor (the ×8-record granule of C12; also keeps the `-18` check and
the cursor's `recCount - 4` wrap well-defined). `recSize` comes from `RecordGeometry` (§5.1).

**Level 1** (per class): needs `L1D_c > 0`, else `-27`.
```
target  = L1D_c / (2 · sibMax_c)
ceiling = 3·L1D_c / (4 · sibMax_c)
lower   = floor8
s_c     = clamp(target, lower, ceiling)
```

**Level 2** (per class): needs `L1D_c > 0` and ≥1 L2 instance filed to the class, else `-27`.
```
lower   = max( 2·L1D_c / sibMin_c , floor8 )
target  = L2_ceilShare(c) / 2
ceiling = 3·L2_ceilShare(c) / 4
s_c     = clamp(target, lower, ceiling)
```

**Level 3** (joint across classes — one L3 hosts both): needs ≥1 L3 instance and, per active class, ≥1 L2
instance, else `-27`.
```
lower_c = max( 2·L2_defeatShare(c) , floor8 )
L3min   = min instance size;   N3max = max over instances of (n0 + n1)
s_c     = max( L3min / (2·N3max) , lower_c )                    — equal share, raised to the defeat bound
verify every L3 instance d:   n0_d·s_0 + n1_d·s_1 ≤ 3·size_d / 4
on any breach (decision 3):   s_c = lower_c for every active class;  status = clamped;
                              feasibleK = min over breaching d of ( (3·size_d/4) / max(lower_c : n_{d,c} > 0) )
```
The uniform pre-raise share can never breach (each instance sees `n_d · L3min/(2·N3max) ≤ ½·size_d`); only the
defeat-bound raise can, which is why the fallback is exactly "run at the defeat bound and warn". The
`lower`/`ceiling` pair reported outward for the explicit-`M` warning uses the uniform-share ceiling
`3·L3min / (4·N3max)`.

**Empty window at level 1 or 2** (`lower > ceiling` — unreachable on known silicon, the formulas keep it
defined anyway): `s_c = lower`, status = clamped, `feasibleK = 1`.

**Rounding, applied last:** `records_c = max(8, (s_c / recSize) & ~7)`; `blockSize_c = records_c · recSize`.
Rounding may undershoot `lower` by at most `8·recSize − 1` bytes, which is noise against the ×2/¾ margins.

---

## 4. Integration flow in `wmain`

All anchors are baseline-commit line numbers in `CPU.cpp`; re-verify before editing.

1. **Hoist record geometry.** The `switch(cfg.procUnits & 0x01F)` at `:909–918` moves verbatim into
   `RecordGeometry` (§5.1). `recSize`/`vecUnits` become `wmain` locals computed once, before the sizing block;
   delete them from the arena block's declaration at `:897` (`bos`, `l`, `m`, `memStatus` stay). Keep the
   switch's `default:` arm and its A10 comment.
2. **Sizing block** — insert after the results-file probe (`:860–873`) and **before** the `memConfig` switch
   (`:876`):

```cpp
   cui8 cacheLevel = HighestCacheLevel(cfg.procUnits);
   ui64 recSize, vecUnits, cacheSize[2], cacheLow[2], cacheHigh[2];
   ui32 feasibleK;

   RecordGeometry(ui8(cfg.procUnits & 0x01F), recSize, vecUnits);

   if(cacheLevel) {
      csi8 sized = CalcCacheBlockSizes(cacheLevel, recSize, threadCount, cacheSize, cacheLow, cacheHigh, feasibleK);

      // A level the system does not report cannot label a test, whether the sizes are derived or explicit
      if(sized < 0) { wprintf(wstrMessage[43], ui32(cacheLevel)); return -27; }

      if(!cfg.memExplicit) { // Derived sizes; an explicit 'M' since the last reset keeps its own (ISSUES.MD A1)
         if(sized > 0) wprintf(wstrMessage[44], ui32(cacheLevel), feasibleK, ui32(cacheLevel));
         resArray.blockSize[0] = cacheSize[0];
         resArray.blockSize[1] = cacheSize[1];
         cfg.allocMem[0] = si64(cacheSize[0]) * threadCount[0] + si64(cacheSize[1]) * threadCount[1];
         cfg.memConfig   = 3;
      }
   }
```

3. **`memConfig` switch** (`:876–888`) gains a pass-through arm — the sizes are already written:

```cpp
   case 3: // Derived from the requested cache level; blockSize and allocMem were written by the sizing block
      break;
```

4. **Explicit-`M` window warning** — insert immediately **after** the switch (the switch is what turns an `Mt`
   total into per-thread sizes):

```cpp
   if(cacheLevel && cfg.memExplicit)
      for(ui8 cc = 0; cc < 2; ++cc)
         if(threadCount[cc] && (resArray.blockSize[cc] < cacheLow[cc] || resArray.blockSize[cc] > cacheHigh[cc]))
            wprintf(wstrMessage[45], ui32(cacheLevel), cacheLow[cc] >> 10, cacheHigh[cc] >> 10, ui32(cc));
```

5. **Defaults block** (`:183–185`): add `cfg.memExplicit = 0;`.
6. **`B` case** (`:209–215`): replace the `memConfig = 1` / `allocMem[0] = 8388608` pair with
   `cfg.memExplicit = 0;  cfg.allocMem[0] = 0;  cfg.allocMem[1] = 0;` and rewrite the F1 comment: `B` still
   resets the memory configuration — the reset value is now "derived", not 8 MB. `procUnits`/`procSync`
   assignments unchanged (bit 7 stays).
7. **Preset preamble** (`:651–656`): add `cfg.memExplicit = 0;` beside the existing resets. Preset digits
   themselves unchanged.
8. **Every `M` sub-option arm** (`:297–330`, four arms): add `cfg.memExplicit = 1;` beside the `memConfig`
   assignment each arm already makes.

Ordering invariants: the sizing block runs after `SetSMTLoading` and the `U` maps (so the sharer counts
describe the selection that will run), after the `-26` check (`:846`, so `threadCount[2] > 0`), and after the
`-15`/`-16` unit checks (`:748–749`, so `RecordGeometry`'s domain is valid). Nothing on this path executes an
instruction outside the x64 baseline — reuse `SetBitCount64`/`LowestSetBit64`, never `__popcnt64` or a
`_BitScan*` (the same rule `EnumerateTopology` documents at `CPU.h:749–760`).

---

## 5. New functions — reference implementations

Placement: a new `//--- Cache-test block sizing ---//` group in **`CPU.h`**, between the close of
`//--- Processor topology ---//` (`CPU.h:1101`) and `//--- Command-line parsing ---//` (`CPU.h:1103`) — the
same static-function-in-header pattern as `EnumerateTopology`. Everything here is scalar (nothing wider than
64 bits moves or compares), so the H4 baseline rule is satisfied. The reference bodies below are normative for
**arithmetic and control flow**; adjust column alignment to taste within GCS r3/r4, and verify against the §10
vectors after transcription. If a new header is preferred instead of `CPU.h`, it needs the full r17 prolog and
a `ClInclude` entry in `CPU.vcxproj` (H8) — staying in `CPU.h` avoids both.

### 5.1 `RecordGeometry`

```cpp
/// Bytes per record of a unit selection, and the record's vector portion in the 8-byte units resArray.alu is
/// indexed by; the two always satisfy recSize == (vecUnits + 1) * 8 whenever the ALU bit is set, which is
/// what makes the two sub-arrays tile the arena exactly. 'default' catches a selection with no processing
/// bit set: wmain rejects that before any caller runs, so the arm is unreachable, but without it recSize
/// would be read uninitialised (ISSUES.MD A10). Change the unit set or a record's layout and this switch
/// must change with it
/// @param unitBits Processing-unit selection, already masked to bits 0~4
/// @param recSize Receives the bytes one record of the selection occupies
/// @param vecUnits Receives the record's vector portion, in 8-byte units; 0 when no ALU offset is needed
static void RecordGeometry(cui8 unitBits, ui64 &recSize, ui64 &vecUnits) {
   switch(unitBits) {
   default: case 1: case 2:                                                 recSize =  8; vecUnits = 0; break;
   case 3:                                                                  recSize = 16; vecUnits = 1; break;
   case 4: case 6:                                                          recSize = 16; vecUnits = 0; break;
   case 5: case 7:                                                          recSize = 24; vecUnits = 2; break;
   case 8: case 10: case 12: case 14:                                       recSize = 32; vecUnits = 0; break;
   case 9: case 11: case 13: case 15:                                       recSize = 40; vecUnits = 4; break;
   case 16: case 18: case 20: case 22: case 24: case 26: case 28: case 30:  recSize = 64; vecUnits = 0; break;
   case 17: case 19: case 21: case 23: case 25: case 27: case 29: case 31:  recSize = 72; vecUnits = 8;
   }
}
```
The body is the switch at `CPU.cpp:909–918`, moved verbatim; carry its full comment block with it and make the
arena block call this function.

### 5.2 `HighestCacheLevel`

```cpp
/// The cache level a unit selection targets: the highest of bits 5~7, mirroring the widest-unit-wins rule
/// JOB_CYCLE applies to bits 1~4. 0 means the selection targets no cache level
/// @param units Processing-unit selection, all 8 bits
/// @return Requested cache level, 1~3; 0 when none of bits 5~7 is set
static cui8 HighestCacheLevel(cui8 units) { return units & 0x080 ? 3 : units & 0x040 ? 2 : units & 0x020 ? 1 : 0; }
```

### 5.3 `MinMaxSelectedSiblings`

```cpp
/// Smallest and largest number of selected virtual cores sharing one physical core of a class. The pair
/// bounds the per-thread share of the caches private to a physical core: a level's ceiling divides by the
/// largest count, and the defeat bound of the level below it by the smallest. A core of the class hosting no
/// selected virtual core hosts no thread and is skipped; a class hosting none at all returns 1 and 1, so no
/// caller inherits a zero divisor. Pairs the two sibling bitmaps exactly as SetSMTLoading does; nothing here
/// reasons about a stride (ISSUES.MD G1, G2, G7)
/// @param threadClass Core class to scan; 0=Non-SMT or efficiency, 1=SMT or performance
/// @param minShare Receives the smallest selected-sibling count of any hosting core of the class, 1~64
/// @param maxShare Receives the largest selected-sibling count of any hosting core of the class, 1~64
static void MinMaxSelectedSiblings(cui8 threadClass, ui32 &minShare, ui32 &maxShare) {
   minShare = maxShare = 0;

   for(ui8 g = 0; g < cfg.sys.groupCount; ++g) {
      ui64 firsts = cfg.sys.coreSibling[0][g] & cfg.sys.coreMap[threadClass][g];

      while(firsts) {
         cui64 first = LowestSetBit64(firsts);
         cui64 last  = LowestSetBit64(cfg.sys.coreSibling[1][g] & ~(first - 1ull));
         cui64 span  = last ? last | (last - first) : first;
         cui32 share = SetBitCount64(span & cfg.coreMap[g]);

         if(share) {
            if(!minShare || minShare > share) minShare = share;
            if(maxShare < share)              maxShare = share;
         }
         firsts ^= first;
      }
   }
   if(!minShare) minShare = maxShare = 1;
}
```

### 5.4 `CACHE_DOMAIN` and `QueryCacheDomains` — decision 4's callable function

```cpp
constexpr cui32 MAX_CACHE_DOMAINS = MAX_THREADS; // One instance per selected physical core is the worst case

al8 struct CACHE_DOMAIN { // 8 bytes
   ui32 size;      // CacheSize of the instance, in bytes
   ui16 n[2];      // Selected virtual cores of each core class the instance serves
   ui8  coreClass; // Class the instance files under, by the WalkTopology pass-2 rule (CPU.h:899~901)
   ///--- 1 byte unused
};

/// Enumerates every data-path cache instance of one level that serves at least one selected virtual core,
/// with the selected-thread count of each core class it serves. Asks the OS afresh rather than keeping the
/// enumeration's buffer alive: the 'U' maps and the SMT policy are applied after EnumerateTopology has
/// returned, so a count taken before them describes a selection that is not running. Instruction and trace
/// caches are skipped -- the arena is data, and sizing against L1I would size against the wrong array.
/// Records are stepped by the Size each one carries, exactly as WalkTopology steps them
/// @param level Cache level to enumerate, 1~3
/// @param dom Receives one entry per qualifying instance
/// @return Number of entries filled; 0 if the system reports no such instance, or the query failed
static cui32 QueryCacheDomains(cui8 level, CACHE_DOMAIN (&dom)[MAX_CACHE_DOMAINS]) {
   DWORD bytes = 0;
   ui32  count = 0;

   SetLastError(ERROR_SUCCESS);
   GetLogicalProcessorInformationEx(RelationCache, 0, &bytes);
   if(GetLastError() != ERROR_INSUFFICIENT_BUFFER || !bytes) return 0;

   ptrc buffer = zalloc64(bytes);

   if(!buffer) return 0;
   if(!GetLogicalProcessorInformationEx(RelationCache, (SLPIEXptrc)buffer, &bytes)) { mfree1(buffer); return 0; }

   for(ui32 os = 0; os + ui32(sizeof(LOGICAL_PROCESSOR_RELATIONSHIP) + sizeof(DWORD)) <= bytes; ) {
      cSLPIEXptrc lpi = (cSLPIEXptrc)&((cchptr)buffer)[os];

      if(!lpi->Size || os + lpi->Size > bytes) break;
      os += lpi->Size;

      if(lpi->Relationship != RelationCache || lpi->Cache.Level != level) continue;
      if(lpi->Cache.Type == CacheInstruction || lpi->Cache.Type == CacheTrace) continue;

      cui64 mask  = lpi->Cache.GroupMask.Mask;
      cui16 group = lpi->Cache.GroupMask.Group;

      if(!mask || group >= MAX_GROUPS) continue;

      cui64 sel0 = mask & cfg.coreMap[group] & cfg.sys.coreMap[0][group];
      cui64 sel1 = mask & cfg.coreMap[group] & cfg.sys.coreMap[1][group];

      if(!(sel0 | sel1)) continue; // Serves no selected core

      if(count >= MAX_CACHE_DOMAINS) { count = 0; break; } // Table overrun reads as a failed query

      dom[count].size      = lpi->Cache.CacheSize;
      dom[count].n[0]      = SetBitCount64(sel0);
      dom[count].n[1]      = SetBitCount64(sel1);
      dom[count].coreClass = (mask & cfg.sys.coreMap[1][group]) ? 1 : 0;
      ++count;
   }
   mfree1(buffer);

   return count;
}
```

### 5.5 `CalcCacheBlockSizes`

```cpp
/// Derives one memory-block size per core class such that the selected threads' blocks together occupy the
/// requested cache level and together exceed the level below it. Both bounds are budgets over selected
/// sharers, taken from the final core map: a ceiling divides an instance's capacity by the most selected
/// threads it serves, and a defeat bound doubles the widest per-thread share any instance of the level below
/// grants. Sizes are rounded down, last, to the 8-record multiple every slice must be (ISSUES.MD C12)
/// @param level Requested cache level, 1~3
/// @param recSize Bytes per record of the selected unit set, from RecordGeometry
/// @param threadCount Selected virtual cores per class, and their total, as wmain counts them
/// @param blockSize Receives each class's per-thread block size in bytes; 0 for a class with no threads
/// @param lower Receives each class's smallest level-resident size, for the explicit-'M' window warning
/// @param ceiling Receives each class's largest level-resident size, for the explicit-'M' window warning
/// @param feasibleK Receives, when the window is empty, the most threads one instance could hold resident
/// @return 0 on success; 1 if the window was empty and the defeat bound was used; -1 if a needed level is unreported
static csi8 CalcCacheBlockSizes(cui8 level, cui64 recSize, csi16 (&threadCount)[3], ui64 (&blockSize)[2],
                                ui64 (&lower)[2], ui64 (&ceiling)[2], ui32 &feasibleK) {
   CACHE_DOMAIN dom[MAX_CACHE_DOMAINS];
   cui64 floor8    = recSize << 3; // The 8-record granule of C12; no block may be smaller
   ui64  target[2] = { 0, 0 };
   ui32  domains, d;
   si8   clamped   = 0;
   ui8   c;

   feasibleK    = 0;
   blockSize[0] = blockSize[1] = 0;
   lower[0]     = lower[1]     = floor8;
   ceiling[0]   = ceiling[1]   = 0;

   switch(level) {
   case 1:
      for(c = 0; c < 2; ++c) {
         ui32 sibMin, sibMax;

         if(!threadCount[c]) continue;
         if(!cfg.sys.cache[c].L1Data) return -1;
         MinMaxSelectedSiblings(c, sibMin, sibMax);
         target[c]  = cui64(cfg.sys.cache[c].L1Data) / (2ull * sibMax);
         ceiling[c] = (cui64(cfg.sys.cache[c].L1Data) * 3ull) / (4ull * sibMax);
      }
      break;
   case 2:
      if(!(domains = QueryCacheDomains(2, dom))) return -1;
      for(c = 0; c < 2; ++c) {
         ui32 sibMin, sibMax;
         ui64 ceilShare = 0;

         if(!threadCount[c]) continue;
         if(!cfg.sys.cache[c].L1Data) return -1;
         MinMaxSelectedSiblings(c, sibMin, sibMax);
         for(d = 0; d < domains; ++d) {
            if(dom[d].coreClass != c) continue;

            cui64 share = cui64(dom[d].size) / cui64(dom[d].n[0] + dom[d].n[1]);

            if(!ceilShare || ceilShare > share) ceilShare = share;
         }
         if(!ceilShare) return -1; // The class has threads and no level-2 instance serving them

         cui64 defeat = (cui64(cfg.sys.cache[c].L1Data) * 2ull) / sibMin;

         if(lower[c] < defeat) lower[c] = defeat;
         target[c]  = ceilShare / 2ull;
         ceiling[c] = (ceilShare * 3ull) / 4ull;
      }
      break;
   case 3: {
      ui64 l3Min = 0;
      ui32 n3Max = 0;

      // Defeat bound: twice the widest per-thread level-2 share any selected thread of the class enjoys
      if(!(domains = QueryCacheDomains(2, dom))) return -1;
      for(c = 0; c < 2; ++c) {
         ui64 defeatShare = 0;

         if(!threadCount[c]) continue;
         for(d = 0; d < domains; ++d) {
            if(dom[d].coreClass != c) continue;

            cui64 share = cui64(dom[d].size) / cui64(dom[d].n[0] + dom[d].n[1]);

            if(defeatShare < share) defeatShare = share;
         }
         if(!defeatShare) return -1;
         if(lower[c] < defeatShare * 2ull) lower[c] = defeatShare * 2ull;
      }
      // Residency budget: every level-3 instance must hold the slices of every selected thread it serves
      if(!(domains = QueryCacheDomains(3, dom))) return -1;
      for(d = 0; d < domains; ++d) {
         cui32 n = ui32(dom[d].n[0]) + ui32(dom[d].n[1]);

         if(!l3Min || l3Min > dom[d].size) l3Min = dom[d].size;
         if(n3Max < n)                     n3Max = n;
      }
      ceiling[0] = ceiling[1] = (l3Min * 3ull) / (4ull * n3Max);
      for(c = 0; c < 2; ++c) if(threadCount[c]) target[c] = max(l3Min / (2ull * n3Max), lower[c]);
      // The uniform pre-raise share cannot breach any instance; only the defeat-bound raise above can
      for(d = 0; d < domains; ++d) {
         cui64 budget = (cui64(dom[d].size) * 3ull) / 4ull;
         cui64 load   = cui64(dom[d].n[0]) * target[0] + cui64(dom[d].n[1]) * target[1];

         if(load <= budget) continue;

         cui64 maxLower = max(dom[d].n[0] ? lower[0] : 0ull, dom[d].n[1] ? lower[1] : 0ull);
         cui32 k        = ui32(budget / maxLower);

         if(!clamped || feasibleK > k) feasibleK = k;
         clamped = 1;
      }
      if(clamped) for(c = 0; c < 2; ++c) target[c] = threadCount[c] ? lower[c] : 0; // Decision 3: warn and run
   }
   }

   for(c = 0; c < 2; ++c) {
      ui64 s = target[c];

      if(!threadCount[c]) continue;
      if(level != 3) { // Level 3 verified its joint budget above; the scalar clamp below is levels 1 and 2
         if(lower[c] > ceiling[c]) { s = lower[c]; feasibleK = 1; clamped = 1; }
         else                      s = min(max(s, lower[c]), ceiling[c]);
      }

      ui64 records = (s / recSize) & ~0x07ull;

      if(records < 8) records = 8;
      blockSize[c] = records * recSize;
   }
   return clamped;
}
```

Notes for the implementer: `min`/`max` are the `windows.h` macros already used throughout `wmain` and
`ComputationPulse`; all arithmetic is `ui64` and cannot overflow (`size` is a `DWORD`, multiplied by at most
3). The policy constants of §3.2 appear in the reference body as their reduced literal forms (`* 3 / 4`,
`/ 2`, `* 2`, `<< 3`); if the named constants are preferred, substitute them consistently — behaviour must be
identical either way.

---

## 6. Data-structure, message and exit-code changes

### 6.1 `GLOBAL_CFG` (`CPU.h:61–105`)

- Add, directly after `memConfig` (`:102`):
  `ui8 memExplicit = 0; // An 'M' argument set the memory sizes; derived cache sizing must not override them`
- Extend the `memConfig` comment: `// 0=Total equally split, 1=Per core, 2=Split per core class, 3=Derived from the requested cache level`
- Correct the struct's byte-count comment at `:61` if `sizeof(GLOBAL_CFG)` changes (verify with a temporary
  `static_assert`, then remove it — the repo convention is an accurate comment, not a permanent assert).

### 6.2 Messages (`en-GB.h`) — table grows `[43]` → `[46]`

The definition `cwchptrc wstrMessage_English[43]` is at `en-GB.h:109`; `translations.h` declares the pointer
without an extent, so only `en-GB.h` and `CLAUDE.md`'s prose carry the number. New entries (wording is the
owner's to polish; the format arguments are normative):

| Index | Purpose | Format arguments | Draft |
|---|---|---|---|
| `[43]` | `-27` refusal | `%u` level | `L"\n\nThe requested level %u cache is not reported by this system; cache-targeted testing is unavailable here.\n\n"` |
| `[44]` | Empty-window warning (decision 3) | `%u` level, `%u` feasibleK, `%u` level | `L"\n\nLevel %u working sets cannot be made cache-resident at this thread count; running at the smallest size that defeats the level below.\n         At most %u threads per level-%u cache instance can be resident with this configuration.\n\n"` |
| `[45]` | Explicit-`M` outside window (decision 2) | `%u` level, `%llu` low KiB, `%llu` high KiB, `%u` class | `L"\n\nThe requested memory per thread lies outside the level %u residency window (%llu ~ %llu KiB per thread for class-%u cores).\n\n"` |

### 6.3 New exit code `-27`

- `wmain` returns `-27` from the sizing block (§4 step 2).
- Exit-code table in the help text: append after the `-26` row at `en-GB.h:47`:
  `"\n-27 : Requested cache level not reported by the system\n"` (move the trailing `\n` from the `-26` row).
- `README.md` return-value table: append the `-27` row after `-26` (`README.md:140`).

### 6.4 Help text and README (K-rule: `en-GB.h` and `README.md` move together)

- **`B` line** (`en-GB.h:57`, `README.md:40`): delete `!!! CACHE USE NOT YET IMPLEMENTED !!!`; the descriptive
  line below it changes from `…level 3 cache, and 8MB memory per thread…` to `…level 3 cache, with memory
  sized to it…` (owner's wording).
- **`I` block** (`en-GB.h:59–60`, `README.md:43–46`): delete `!!! CACHE USE NOT YET IMPLEMENTED !!!`; add to
  the Caches line's column the rule `The highest cache level given is used` (mirrors the `F, S, V and X are
  mutually exclusive` annotation style), and a line stating that block sizes are derived from the selected
  cores' caches unless an `M` option sets them.
- **`M` block** (`en-GB.h:64…`, `README.md:50–54`): add one line: an explicit `M` overrides cache-derived
  sizing, and a size outside the level's residency window is warned about.

### 6.5 `CLAUDE.md`

Update, at minimum: the bit-field section's sentence `Cache bits 5–7 are parsed and displayed but **not
implemented**…`; the Known-constraints bullet `Cache-targeting (I1/I2/I3) is accepted, displayed, and does
nothing.`; the localisation section's `wstrMessage_*[43]` extent (→ 46); the KUPS paragraph (B's working set
changed, so B scores are not comparable across this change); `memConfig`'s documented values; and a short
description of the new `//--- Cache-test block sizing ---//` group and the `memExplicit` flag.

### 6.6 Bookkeeping

- Remove the completed To-Do items: `CPU.h:10` (item 3), `CPU.cpp:8` (item 1), `CPU_methods.h:8` (item 1).
- Update `Last Modified` in every touched file's r17 prolog; `Version:` stays the product version and moves
  only on release, per `CLAUDE.md`.
- `ISSUES.MD` A1: mark resolved by this change (owner's edit; note the `B` behaviour change there too).
- No root `CHANGELOG.md` exists (GCS c2 gap, ISSUES.MD L1); if it is created as part of this work, this change
  is its first `Added`/`Changed` entry — otherwise record nothing inline (c1).

---

## 7. GCS v1.1.4 conformance requirements for this change

- **r8/r7/e2**: 3-space indent, no tabs; ≤150 columns, hard cap 180.
- **r14/r15**: braces attach; one space before `{`.
- **r1/r2/t1/t2/t3**: `ui64`/`cui8`/`csi16` etc. throughout; const/indirection in the alias, never mixed with
  raw `const T*` forms in the same TU.
- **r11/r12**: `RecordGeometry`, `HighestCacheLevel`, `MinMaxSelectedSiblings`, `QueryCacheDomains`,
  `CalcCacheBlockSizes` are PascalCase; `CACHE_OCCUPANCY_*`, `CACHE_CEILING_*`, `CACHE_DEFEAT_MUL`,
  `MAX_CACHE_DOMAINS` are UPPER_SNAKE.
- **r3/r4**: spreadsheet-align the declaration and assignment columns as the reference bodies do.
- **r5/d1**: `///` API docs with `@param`/`@return` on every new function; `//` notes; group headers `//--`.
- **H4**: nothing wider than 64 bits moves or compares in `CPU.h`/`CPU.cpp` — all new code is scalar.
- **Baseline rule**: bit counting via the existing SWAR helpers, never `__popcnt64`/`PopulationCount64`.
- **p2**: `QueryCacheDomains`'s `zalloc64` has its matching `mfree1` on every path, including both failures.
- **en3**: no deviations are expected; if one becomes necessary, tag it `// RULE-DEV:<rule-id> <why>`.

---

## 8. Invariants that must not change

1. `cpu.values` format, `VALUES_FILE_VERSION`, `KernelFingerprint` inputs, and every `Job*` kernel — block
   sizes never enter the golden arithmetic, so `W` output is bit-identical before and after this change.
2. `JOB_CYCLE[2][32]`, its index domain, and `ComputationPulse`'s `& 0x01F` mask — bits 5–7 must never reach
   the dispatch index.
3. The ×8-record rounding, the per-class `-18` check, the `bos` cross-class record offset, the seeding pass,
   and the cursor wrap at `recCount - 4`.
4. The order `timer.Update()` → spawn loop (E11), and everything in the spawn loop.
5. The results-file probe/create split (C1) — the sizing block sits between the probe and the switch and adds
   no file handling.

---

## 9. Known limitations (record these as `//` notes where relevant)

- **Multi-group cache records.** Like `WalkTopology` pass 2, `QueryCacheDomains` reads the single
  `Cache.GroupMask`. On machines where one cache spans processor groups (>64 logical processors per L3 — e.g.
  large single-die servers), sharers are under-counted and per-thread shares over-estimated. The SDK fields
  for a future fix are `Cache.GroupCount`/`Cache.GroupMasks` (Windows SDK 20348+). Inherited limitation; fix
  both walks together or neither.
- **Victim/exclusive L3** (AMD Zen): budgeting residency against L3 capacity alone is conservative — the
  effective capacity is nearer L3 + the L2s feeding it — so derived sizes err toward guaranteed residency.
- **Prefetchers**: the sequential walk exercises bandwidth and fill paths of the named level, not isolated
  load-to-use latency; that is the intended semantics of `I1`/`I2`/`I3` here.
- **Smallest-size-per-class convention**: `L1D_c` inherits pass 2's "smallest instance per class" filing.
  Classes are homogeneous on every known part; on a hypothetical heterogeneous class the ceiling stays safe
  and the L2 defeat bound is exact (it comes from the per-instance table), leaving only the level-2 test's
  L1-defeat bound nominally optimistic.

---

## 10. Acceptance criteria

### 10.1 Arithmetic vectors (verify `CalcCacheBlockSizes` by dry run before wiring `wmain`)

Both vectors use `recSize = 40` (ALU + AVX2, arena index 9). "Selected" = every virtual core.

**Vector A — Ryzen 9 5950X** (16C/32T, SMT2; L1D 32768; L2 524288 private; two L3 instances of 33554432
serving 16 threads each; single class 1):

| Level | lower | target | ceiling | records | blockSize | status |
|---|---:|---:|---:|---:|---:|---|
| 1 | 320 | 8 192 | 12 288 | 200 | **8 000** | 0 |
| 2 | 32 768 | 131 072 | 196 608 | 3 272 | **130 880** | 0 |
| 3 | 524 288 | 1 048 576 | 1 572 864 | 26 208 | **1 048 320** | 0 (budget 16 × 1 MiB = 16 MiB ≤ 24 MiB) |

**Vector B — Core i9-13900K** (class 1 = 8 P-cores SMT2: L1D 49152, L2 2097152 private; class 0 = 16 E-cores:
L1D 32768, L2 4194304 per 4-core cluster; one L3 instance 37748736 serving n0 = 16, n1 = 16):

| Level | class | lower | target | ceiling | blockSize | status |
|---|---|---:|---:|---:|---:|---|
| 1 | 1 (P) | 320 | 12 288 | 18 432 | **12 160** | 0 |
| 1 | 0 (E) | 320 | 16 384 | 24 576 | **16 320** | 0 |
| 2 | 1 (P) | 49 152 | 524 288 | 786 432 | **524 160** | 0 |
| 2 | 0 (E) | 65 536 | 524 288 | 786 432 | **524 160** | 0 |
| 3 | both | 2 097 152 | raised to 2 097 152 | 884 736 (uniform) | **2 096 960** | **1**, `feasibleK = 13` |

Vector B level 3 breaches its instance (32 × 2 MiB = 64 MiB > ¾ × 36 MiB = 27 MiB), so both classes run at the
defeat bound and the warning must print `13` (⌊28 311 552 / 2 097 152⌋).

### 10.2 Behavioural checklist (on the developer machine, after `PITC.exe W`)

1. `PITC.exe I3a Tct5.0` — runs memory-backed; banner shows `ALU CL3` and a truthful MB total; no warning on a
   5950X-class part; per-thread size within Vector-A level-3 bounds for the machine's own topology.
2. `PITC.exe I1a Tct5.0` and `I2a` — slices match the machine's own L1D/L2 windows (spot-check against the
   formulas with the machine's CPUID/coreinfo numbers).
3. `PITC.exe I123a Tct5.0` — behaves as `I3a` (decision 1).
4. `PITC.exe B` — no 8 MB anywhere; derived level-3 sizes; KUPS prints; on a big-private-L2 part the
   `wstrMessage[44]` warning appears with a sensible thread count.
5. `PITC.exe B Mt1024` — explicit `M` wins; `wstrMessage[45]` fires iff the resulting per-thread size is
   outside the level-3 window. `PITC.exe Mc8 B` — derived sizes (the `B` reset cleared `memExplicit`).
6. `PITC.exe -1 I3a` — preset's 8 MB is overridden by derived sizes (preamble cleared `memExplicit`).
7. On a system reporting no L3 (VM): `PITC.exe I3a` and `PITC.exe B` both refuse with `-27` and
   `wstrMessage[43]`.
8. `PITC.exe Ia Tct5.0` (no cache bits) — byte-identical behaviour to the baseline build's register-resident
   run; `PITC.exe -1` unchanged.
9. `-18` remains unreachable through derived sizing (floor is 8 records by construction).

### 10.3 Documentation greps (all must hold after the change)

- `grep -ri "not yet implemented" en-GB.h README.md CLAUDE.md` → no cache-related hits.
- `-27` present in both exit-code tables (`en-GB.h`, `README.md`) and returned by exactly one `wmain` path.
- `wstrMessage_English[46]` defined with 46 initialisers; no reference to a `wstrMessage` index ≥ 46;
  `CLAUDE.md` no longer says `[43]`.
- The three To-Do items of §6.6 are gone; every touched prolog's `Last Modified` is current; no `History:`
  label anywhere (r17/c1).
