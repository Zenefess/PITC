/************************************************************
 * File: CPU.h                          Created: 2025/01/25 *
 *                                    Last mod.: 2026/08/15 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

#define _CRT_RAND_S

#include <iostream>
#include <atomic>   // atomic_signal_fence, the compiler barrier the completion-bitmap polls read through
#include <stdlib.h> // rand_s, which is declared only for a translation unit that defined _CRT_RAND_S above
#include <string.h> // memcmp, for the byte-exact comparisons in each unit's ValidateFamily*
#include <windows.h>
#include <process.h>
#include "memory management.h"
#include "class_timers.h"
#include "CPU_build.h"

constexpr auto MAX_THREADS       = 512;
constexpr auto MAX_THREADS_BYTES = (MAX_THREADS + 7) >> 3;
// declare1d64z's third argument is an element count, not a byte count. Passing MAX_THREADS_BYTES reserved
// 64 ui64 -- 4096 thread bits -- for a map that needs 8, so the buffer's true capacity silently disagreed
// with MAX_THREADS. It is the same units mix-up that produced the stride bug in the ThreadsRunning* polls
constexpr auto MAX_THREADS_WORDS = (MAX_THREADS_BYTES + 7) >> 3;
// Processor groups a topology may name. Windows numbers groups from zero and defines at most 64 of them, so
// this bounds the API's numbering rather than the storage behind it: the maps below are indexed by group and
// hold MAX_THREADS entries each. It is deliberately not MAX_THREADS_WORDS -- that is the width of a bitmap
// indexed by *thread*, and the two were only ever equal because 512 threads and 64-processor groups both
// divide by 64. A group carrying fewer than 64 virtual cores makes them differ (ISSUES.MD G3)
constexpr auto MAX_GROUPS = 64;

//--- Core classes ---//
// Everything the program records about a core, and every choice it makes per core, is split two ways: two
// core maps, two sibling bitmaps, two cache records, two physical core counts, two memory sizes ('Mn'/'Ms')
// and two thread classes in the spawn loop. Class 1 is the wider core of the two, class 0 the narrower.
//
// What "wider" means depends on the machine, and until now the code assumed it always meant the same thing:
// the class came from the core's own sibling count, so it read "non-SMT / SMT" and nothing said that on a
// hybrid part it actually means "efficiency / performance" -- nor that the same part with SMT disabled in
// firmware puts every core, P and E alike, into class 0, with one set of cache sizes standing in for both
// (ISSUES.MD G9). The rule is now the OS's own EfficiencyClass wherever the machine reports more than one of
// them, and the sibling count only for the machines that description fits; cfg.sys.hybrid says which applied

al32 struct GLOBAL_CFG { // 112 bytes
   struct _C_D_ { ui32 L1Code, L1Data, L2; };
   struct {
      struct {
         declare2d64z(ui64, coreMap, 2, MAX_THREADS); // Bitmaps of available virtual cores, per core class
         // One bit per physical core, taken from the sibling mask the enumeration reports for that core: its
         // lowest set bit, and its highest. A physical core without SMT sets the same bit in both maps, so
         // "one virtual core per physical core" keeps it either way. SetSMTLoading masks with one of these
         // rather than rebuilding the sibling layout from a core count and a stride, which is what excluded
         // every non-SMT core and shifted by 64 on a CPU reporting no SMT at all (ISSUES.MD G1, G2)
         declare2d64z(ui64, coreSibling, 2, MAX_THREADS); // Bitmaps of one virtual core per physical core; 0=First, 1=Last
         _C_D_ cache[2];     // Sizes of L1 code & data and L2 caches, per core class
         ui32  cacheL3;      // Size of L3 cache per core complex
         si16  coreCount[2]; // Number of physical cores, per core class
      };
      si16 vCoreCount = 0;     // Total number of virtual processors
      ui8  groupCount = 0;     // Number of virtual processor groups
      // Virtual cores per physical core, of each core class. A machine's two classes need not share one SMT
      // width -- a hybrid part's performance cores carry two virtual cores each and its efficiency cores one
      // -- and a single maximum described neither of them (ISSUES.MD G9)
      ui8  SMT[2]     = { 0, 0 };
      bool hybrid     = false; // Classes are efficiency/performance cores rather than non-SMT/SMT cores
      bool cpuSSE4_1  = false; // CPU supports the SSE 1.0~4.1 instruction sets
      bool cpuAVX2    = false; // CPU supports the AVX 1.0~2.0 instruction sets
      bool cpuAVX512  = false; // CPU supports the AVX512F instruction set
      ///--- 7 bytes unused
   } sys;
   declare1d64z(ui64, coreMap, MAX_THREADS); // Bitmap of available virtual cores
   si64 tics        = 0;        // Global time limit
   // Both classes default to nothing requested. The second used to default to 1 byte, which is neither zero
   // nor a usable size: 'Mn8' alone left it there, so the class-1 threads were given a 1-byte slice, divided
   // down to zero records, and dropped onto the register code path with their arena pointers already written
   // over their results (ISSUES.MD C9). A class that is given no memory now holds 0, which the per-class
   // record check in wmain refuses with -18 instead of running a configuration nobody asked for
   si64 allocMem[2] = { 0, 0 }; // Amount(s) of memory allocated for threads, per core class
   ui32 onTime      = 100;      // Computation duration (in ms)
   ui32 offTime     = 900;      // Sleep duration (in ms)
   ui32 delayTime   = 2000;     // Start-up delay duration (in ms)
   ui8  procUnits   = 0x04;     // 0==ALU, 1==FPU, 2==SSE4.1, 3==AVX, 4==AVX512, 5==L1 cache, 6==L2 cache, 7==L3 cache
   ui8  procSync    = 0x02A;    // 0==Round-robin, 1==Parallel 2==Staggered, 3==Synchronised, 4==Constant, 5==Fixed pulse, 6==Sweeping pulse, 7==Benchmark
   ui8  SMTLoad     = 0;        // Only utilise the specified virtual core(s) of each active physical core; 0=Unchanged, 1=First, 2=Last, 3=All
   ui8  memConfig   = 0;        // 0=Total equally split, 1=Per core, 2=Split per core class

   ~GLOBAL_CFG(void) { mfree(coreMap, sys.coreMap, sys.coreSibling); }
}; typedef GLOBAL_CFG *const GLOBAL_CFGptrc;

al64 struct THREAD_CFG { // 64 bytes
   si64 packetSizeRAM; // Amount of RAM (in bytes) to utilise per pulse
   si64 startTics;     // Thread start time (in tics)
   si64 endTics;       // Thread shutdown time (in tics)
   si64 maxTics;       // Maximum duration (in tics)
   si64 activeTics;    // Computation duration (in tics)
   si64 cycleTics;     // Cycle duration (in tics)
   union {
      struct {
         ui16 records48[3]; // Total number of memory-bound records to process (48-bit value)
         ui16 threadCount;  // Total number of computation threads
      };
      ui64 rc_tc;
      ui32 records32;
   };
   // Reported rather than executed against: ComputationPulse derives its idle phase from the cycle it has
   // stretched, in tics, because a millisecond copy of the off-time cannot follow that stretch (ISSUES.MD E6)
   ui32 inactiveTime;  // Sleep duration (in ms)
   union {
      ui32 flags;
      struct {
         ui8 procUnits;  // 0==ALU, 1==FPU, 2==SSE4.1, 3==AVX, 4==AVX512, 5==L1 cache, 6==L2 cache, 7==L3 cache
         ui8 procSync;   // 0==Round-robin, 1==Parallel, 2==Staggered, 3==Synchronised, 4==Constant, 5==Fixed pulse, 6==Sweeping pulse, 7==Benchmark
         ui8 threadByte;
         ui8 threadBit;
      };
   };
}; typedef THREAD_CFG *const THREAD_CFGptrc; typedef const THREAD_CFG *const cTHREAD_CFGptrc;

al64 union RESULTS { // 128 bytes
   ui64 raw[16];
   fl64 _fl64[16];
   ui32 raw32[32];
   struct {
      fl64x8 avx512;
      fl64x4 avx;
      fl64x2 sse;
      fl64   fpu;
      si64   alu;
   };
   struct {
      fl64x8ptr p0;
      fl64x4ptr p1;
      fl64x2ptr p2;
      fl64ptr   p3;
      si64ptr   p4;
   };
   ui64ptr pr[5];
}; typedef RESULTS *const RESULTSptrc; typedef const RESULTS cRESULTS;

constexpr csi64 RESULTS_BUF_SIZE = sizeof(RESULTS) * MAX_THREADS;

al32 struct RESULTS_ARRAYS { // 96 bytes
   fl64x8ptr avx512;
   fl64x4ptr avx;
   fl64x2ptr sse;
   fl64ptr   fpu;
   si64ptr   alu;
   // The two owned allocations, and the only two members the destructor below frees; they are initialised
   // here rather than relying on the static zero-initialisation resArray happens to receive
   ptr       p    = 0; // Master pointer
   // Records processed per thread, not loop iterations: a memory-backed iteration is four records and a
   // register-resident one is a single record, and the idle iterations of a pulsed run are no records at
   // all, so a count of iterations was not a count of work the benchmark could weigh (ISSUES.MD E12)
   si64ptr   iter = 0; // Total records processed per thread
   ui64 blockSize[2] = { 0, 0 }; // Memory per thread, per core class
   ui64 records[2]   = { 0, 0 }; // Memory records per thread, per core class

   // The arena and the benchmark's iteration counters are the program's two run-length allocations, and
   // neither had a matching free (ISSUES.MD C13); GCS rule p2 requires one for every aligned allocation.
   // wmain leaves by more than a dozen returns, so the frees belong to the object that owns the pointers --
   // the same reason GLOBAL_CFG's destructor owns its bitmaps. mdealloc ignores a null pointer, so a run
   // that allocated neither is as safe as one that allocated both
   ~RESULTS_ARRAYS(void) { mfree(p, iter); }
};

// Owner of the report buffer wmain builds its banner, results table and benchmark score in -- the third
// run-length allocation, and the one the C13 pair left behind. Its size is derived from the processor group
// count and the thread count, so it cannot be a fixed array; it is a local of wmain rather than a global, so
// it cannot be freed by either of the destructors above; and seven further returns stand between its
// allocation and the end of the function, so freeing it where it is used would mean seven frees and a leak
// the next time an error return is added between them. An owning object frees it on every one of those paths
// and on any path added later, which is the whole of why the arena belongs to RESULTS_ARRAYS rather than to
// wmain. GCS rule p2 requires a matching free for every aligned allocation; this is that free, and mdealloc
// ignores a null, so the failed-allocation path destructs exactly as safely as a successful one
// (ISSUES.MD C3, C13)
struct REPORT_BUFFER {
   wchptrc text; // The buffer, as the declare1d64z that allocates it produced; freed however its scope ends

   ~REPORT_BUFFER(void) { mfree1(text); }
}; typedef const REPORT_BUFFER cREPORT_BUFFER;

//--- Global variables ---//
// Declared here, defined once in CPU.cpp. A header that *defines* an object at namespace scope hands every
// translation unit that includes it either a duplicate symbol at link time or -- for the `dataType *const`
// the declare* macros expand to, which is const and therefore internally linked -- a private copy of it. The
// four result planes, the thread table and the completion bitmap are the storage wmain and the worker threads
// compare and signal through, so a second copy is not a link error but a program whose two halves cannot see
// each other. That is what confined the whole program to a single translation unit, and it is what the
// per-ISA job cycles of H4 had to be able to reach from CPU_jobs_*.cpp (ISSUES.MD H9).
//
// The declaration of a pointer the declare* macros produce is spelt out rather than macro-generated, because
// the macros carry the allocation with them; the comment beside each names the macro that defines it in
// CPU.cpp, and the two must be changed together
extern al64 CLASS_TIMER timer;
extern      GLOBAL_CFG  cfg;

extern THREAD_CFG *const threadData;        // declare1d64z(THREAD_CFG, threadData, MAX_THREADS)
extern RESULTS (*const value)[MAX_THREADS]; // declare2d64z(RESULTS, value, 4, MAX_THREADS)
                                            // Result values: 0==Input, 1=Processed, 2=Output, 3=Error
extern vui64 *const threadBits;             // declare1d64z(vui64, threadBits, MAX_THREADS_WORDS)
extern wchar *const wstrOut;                // declare1d64z(wchar, wstrOut, 1024)
extern RESULTS_ARRAYS resArray;
// Set by any GenerateValues thread whose self-check disagrees, and read by 'W' once every thread has joined.
// The failure used to be signalled by writing two sentinel values into value[2][0] and value[3][0], which
// whichever thread owns entry 0 then overwrote wholesale with its own results -- so a real computational
// error was discarded and "cpu.values" written as though the run had passed (ISSUES.MD B7). A flag of its own
// cannot be overwritten by anybody's results, and the interlocked write orders it ahead of the completion bit
// the reader is waiting on
extern vsi8 generateError;

#include "translations.h"

// Immutable tables, so one entity shared by every translation unit rather than a definition each of them owns
// a private copy of. 'inline' is what makes them one entity in C++17; wstrLang below is written to by the 'L'
// option, so it is a global like any other and is defined in CPU.cpp (ISSUES.MD H9)
inline cwchar wstrUnitsCPU[8][4]  = { L"ALU", L"FPU", L"SSE", L"AVX", L"512", L"CL1", L"CL2", L"CL3" };
inline cwchar wstrSyncCPU[8][4]   = { L"R-R", L"Par", L"Sta", L"T-S", L"Con", L"F-P", L"S-P", L"Ben" };
inline cwchar wstrPass[2][8]      = { L".Pass.", L"!Fail!" }; ///--- Modify for translation ---///
inline cwchar outUTF16header      = 0x0FEFF;
inline cchar  outUTF8header[3]    = { char(0x0EF), char(0x0BB), char(0x0BF) };
extern wchar  wstrLang[6];
//--- Global variables ---//

extern void JobALU(si64&);            extern void JobFPU(fl64&);                          extern void JobALU_FPU(fl64&, si64&);
extern void JobSSE(fl64x2&);          extern void JobALU_SSE(fl64x2&, si64&);
extern void JobAVX2(fl64x4&);         extern void JobALU_AVX2(fl64x4&, si64&);
extern void JobAVX512(fl64x8&);       extern void JobALU_AVX512(fl64x8&, si64&);
extern void JobMemALU(si64ptrc);      extern void JobMemFPU(fl64ptrc);                    extern void JobMemALU_FPU(fl64ptrc, si64ptrc);
extern void JobMemSSE(fl64x2ptrc);    extern void JobMemALU_SSE(fl64x2ptrc, si64ptrc);
extern void JobMemAVX2(fl64x4ptrc);   extern void JobMemALU_AVX2(fl64x4ptrc, si64ptrc);
extern void JobMemAVX512(fl64x8ptrc); extern void JobMemALU_AVX512(fl64x8ptrc, si64ptrc);

//--- Arena seeding ---//
// One record of the arena is one input to the job kernel the run has selected, so seeding a slice is a store
// of the unit's own width, repeated: 'value[1][k].p0[os] = value[0][k].avx512' is a 512-bit move, and it was
// written in wmain, which compiles at the SSE2 baseline. Each unit's seeding pass therefore sits in the
// translation unit built for that unit, beside the kernels that read what it writes, exactly as the job
// cycles do -- so that no vector store above the baseline is emitted from CPU.cpp (ISSUES.MD H4). The seed is
// taken by reference: a vector wider than 16 bytes is passed by address under the x64 calling convention
// either way, and a reference says so without the caller ever forming the value in a register it may not have
/// @param records First record of the thread's slice of the arena
/// @param count Number of records in the slice
/// @param seed Value every record is to be given
extern void SeedRecordsALU   (si64ptrc   records, cui64 count, csi64    seed);
extern void SeedRecordsFPU   (fl64ptrc   records, cui64 count, cfl64    seed);
extern void SeedRecordsSSE   (fl64x2ptrc records, cui64 count, cfl64x2 &seed);
extern void SeedRecordsAVX2  (fl64x4ptrc records, cui64 count, cfl64x4 &seed);
extern void SeedRecordsAVX512(fl64x8ptrc records, cui64 count, cfl64x8 &seed);
//--- Arena seeding ---//

//--- Job kernel cross-check ---//
// The eighteen job kernels are written out by hand across four translation units, with no shared
// implementation, and until now nothing required them to agree. "cpu.values" records only what the five
// register-resident kernels produce, so a JobMem* or JobALU_* kernel that had drifted from its counterpart
// was never compared against anything: it was graded against a reference for arithmetic it does not
// perform, and every memory-backed run -- which is to say every preset, every 'B' and every 'M' -- reported
// the difference as silicon at fault (ISSUES.MD B5). 'W' now proves the whole family agrees before it
// writes a file, which is where the invariant is established and the only place it can still be corrected.
//
// Two invariants are proved here, not one. Within a unit, every JobMem* and Job*ALU_* kernel must reproduce
// its register-resident counterpart -- that is the ValidateFamily* half. Across units, every vector kernel
// the golden ladder can walk must reproduce JobFPU element-wise, or a "cpu.values" stops being readable on a
// CPU of another vector width -- that is the ValidateLadder* half (ISSUES.MD B1)

// Names of the kernels the check walks, in the order it walks them; index 0 is "nothing disagreed".
// These are identifiers rather than prose, so the table is not part of the translated strings
inline cwchar wstrKernelName[17][20] = {
   L"",
   L"JobALU_FPU",    L"JobMemALU",    L"JobMemFPU",    L"JobMemALU_FPU",
   L"JobALU_SSE",    L"JobMemSSE",    L"JobMemALU_SSE",
   L"JobALU_AVX2",   L"JobMemAVX2",   L"JobMemALU_AVX2",
   L"JobALU_AVX512", L"JobMemAVX512", L"JobMemALU_AVX512",
   L"JobSSE",        L"JobAVX2",      L"JobAVX512"
};

// Where the table divides. Entries 1~13 name a memory-array or combined kernel, and the reference each of
// them failed to reproduce is the register-resident kernel of its own unit; entries from here on name a
// register-resident *vector* kernel, whose reference is JobFPU one lane at a time. The two failures mean
// different things to whoever reads the message, so wmain selects the message from this boundary
constexpr cui8 KERNEL_NAME_LADDER = 14;

// Lanes the cross-width ladder check runs through every vector kernel the CPU carries: one AVX-512 vector,
// two AVX2 vectors, four SSE vectors, and eight separate JobFPU calls, all over the same eight seeds
constexpr cui8 LADDER_PROBE_LANES = 8;

/// Runs one seed through every job kernel of one processing unit, and requires each memory-array and combined
/// variant to reproduce its register-resident counterpart exactly. Each JobMem* kernel is handed four records
/// carrying the same seed, so all four must come back equal to the single register result -- which checks the
/// record indexing as well as the arithmetic. The comparison is a byte compare: a golden value is a bit
/// pattern, and every floating-point spelling of "equal" this codebase has reached for has at some point
/// compared something other than every bit (ISSUES.MD A1~A3, A11).
///
/// Each of the four lives in the translation unit of the kernels it checks, because the check reads and
/// writes values of that unit's width: the AVX-512 half of the one function these replace performed 512-bit
/// moves in a file compiled at the SSE2 baseline (ISSUES.MD H4). Every one of them re-derives the ALU
/// reference itself rather than being handed one, so that each is a complete statement of its own unit
/// @param seed The one seed every kernel of the family is run over
/// @return 0 if every kernel of the family agreed; otherwise the wstrKernelName index of the first that did not
extern cui8 ValidateFamilyScalar(cRESULTS &seed); // JobALU_FPU, JobMemALU, JobMemFPU, JobMemALU_FPU
extern cui8 ValidateFamilySSE   (cRESULTS &seed); // JobALU_SSE, JobMemSSE, JobMemALU_SSE
extern cui8 ValidateFamilyAVX2  (cRESULTS &seed); // JobALU_AVX2, JobMemAVX2, JobMemALU_AVX2
extern cui8 ValidateFamilyAVX512(cRESULTS &seed); // JobALU_AVX512, JobMemAVX512, JobMemALU_AVX512

/// Runs LADDER_PROBE_LANES seeds through one register-resident vector kernel, in vectors of that kernel's
/// own width, and requires every lane to reproduce what JobFPU produced from the same seed. That property --
/// each vector kernel computing JobFPU element-wise, bit for bit -- is the whole of what makes a "cpu.values"
/// generated on one vector width verifiable on another, and it is the one thing the ValidateFamily* checks
/// above cannot see: each of them derives its reference from its own unit's register kernel, so the eighteen
/// kernels were held to their own width and never across widths (ISSUES.MD B1).
///
/// Each lives in the translation unit of the kernel it checks, for the reason every ValidateFamily* does: the
/// loads and the comparison are of that unit's width (ISSUES.MD H4). The reference is derived once, by the
/// caller, because it is scalar and identical for all three
/// @param probe The lanes to transform, one seed per lane
/// @param reference The same lanes after JobFPU, one call per lane
/// @return 0 if every lane agreed; otherwise the wstrKernelName index of the vector kernel that did not
extern cui8 ValidateLadderSSE   (cfl64ptrc probe, cfl64ptrc reference); // JobSSE
extern cui8 ValidateLadderAVX2  (cfl64ptrc probe, cfl64ptrc reference); // JobAVX2
extern cui8 ValidateLadderAVX512(cfl64ptrc probe, cfl64ptrc reference); // JobAVX512

/// Runs one seed through every job kernel the CPU can execute, one family at a time, and then holds the
/// vector kernels of every width the CPU carries to the scalar kernel they must reproduce lane for lane
/// @return 0 if every kernel agreed; otherwise the wstrKernelName index of the first that did not
static cui8 ValidateKernelFamilies(void) {
   RESULTS seed = {}; // Zeroed first, so every lane stays defined if the seeding below is ever narrowed
   fl64    probe[LADDER_PROBE_LANES], reference[LADDER_PROBE_LANES];
   ui8     badKernel;

   // The seed KernelFingerprint probes with: a magnitude the FP chains settle from within two steps, and a
   // plain integer for the ALU lane
   for(ui8 k = 0; k < 15; ++k) seed._fl64[k] = fl64(0x0123456789ABCull >> (k & 0x03)) + fl64(k);
   seed.raw[15] = 0x0123456789ABCDEF;

   //--- ALU, FPU and SSE: the paths every x64 CPU carries, SSE being the golden ladder's fallback ---//
   if((badKernel = ValidateFamilyScalar(seed)) != 0) return badKernel;
   if((badKernel = ValidateFamilySSE(seed))    != 0) return badKernel;

   //--- AVX2 and AVX-512: gated exactly as RunGoldenLadder gates them ---//
   if(cfg.sys.cpuAVX2   && (badKernel = ValidateFamilyAVX2(seed))   != 0) return badKernel;
   if(cfg.sys.cpuAVX512 && (badKernel = ValidateFamilyAVX512(seed)) != 0) return badKernel;

   //--- The ladder itself: every vector width against the scalar kernel it stands in for ---//
   // RunGoldenLadder transforms the same 16 lanes with whichever vector kernel the CPU provides, so a file
   // written on an AVX-512 machine is only readable on an SSE one while all three compute JobFPU element-wise.
   // Nothing checked that. An edit made consistently across one unit's family passed the checks above, 'W'
   // wrote the file, and every machine of a different width then computed a different KernelFingerprint and
   // rejected it with -21 "generated by a different build": the portability had ceased to exist, no check
   // named the kernel that ended it, and the diagnostic blamed the file (ISSUES.MD B1).
   // The scalar reference is derived here, one JobFPU call per lane, and handed to all three: the reference
   // is the *other* unit's arithmetic by definition, which is the one thing a family check cannot supply
   // itself, and JobFPU is a baseline scalar call from a header every unit already compiles
   for(ui8 k = 0; k < LADDER_PROBE_LANES; ++k) {
      probe[k] = reference[k] = seed._fl64[k];
      JobFPU(reference[k]);
   }

   if((badKernel = ValidateLadderSSE(probe, reference)) != 0) return badKernel;

   if(cfg.sys.cpuAVX2   && (badKernel = ValidateLadderAVX2(probe, reference))   != 0) return badKernel;
   if(cfg.sys.cpuAVX512 && (badKernel = ValidateLadderAVX512(probe, reference)) != 0) return badKernel;

   return 0;
}
//--- Job kernel cross-check ---//

//--- "cpu.values" file format ---//
// A 64-byte header followed by the two RESULTS[MAX_THREADS] blocks: the input seeds, then the golden
// outputs those seeds produce. Before the header existed nothing in the file identified what had written
// it, so its most likely failure -- a file left over from an earlier kernel revision, or from a build that
// rounds differently -- reached the comparison intact and every thread reported "!Fail!" against a
// reference for arithmetic this build no longer performs, which reads as a fleet-wide hardware fault.
// Neither read was checked for length either: ReadFile reports reaching the end of the file as success, so
// a truncated, empty or unrelated file was accepted with value[0] and value[2] left holding stale zeroes

constexpr cui64 VALUES_FILE_MAGIC   = 0x0534C415643544950; // "PITCVALS", little-endian
constexpr cui32 VALUES_FILE_VERSION = 1;                   // Raised whenever VALUES_HEADER changes
constexpr cui64 VALUES_HASH_BASIS   = 0x0CBF29CE484222325; // FNV-1a 64-bit offset basis
constexpr cui64 VALUES_HASH_PRIME   = 0x0100000001B3;      // FNV-1a 64-bit prime

// Every build setting that changes what a correct golden value is. The compiler version is deliberately
// absent: under the /fp:strict CPU_build.h requires, a toolset upgrade cannot change a result, and folding
// it in would demand a fresh "cpu.values" after every Visual Studio update
constexpr cui64 VALUES_BUILD_ID = (cui64(VALUES_FP_FLAGS) << 32) | cui64(sizeof(RESULTS));

al64 struct VALUES_HEADER { // 64 bytes
   ui64 magic;      // VALUES_FILE_MAGIC; identifies the file, and its byte order
   ui32 version;    // VALUES_FILE_VERSION; the layout of the fields below
   ui32 headerSize; // sizeof(VALUES_HEADER)
   ui64 blockSize;  // RESULTS_BUF_SIZE; the MAX_THREADS by sizeof(RESULTS) geometry of each block
   ui64 buildID;    // VALUES_BUILD_ID; the build settings the values depend upon
   ui64 kernelID;   // KernelFingerprint(); the arithmetic the job kernels actually perform
   ui64 seedHash;   // Hash of the input block that follows this header
   ui64 valueHash;  // Hash of the golden output block that follows the input block
   ui64 reserved;   // 0
};

/// Hashes a byte range with the 64-bit FNV-1a function. It detects a truncated or altered "cpu.values", it
/// does not defend one: the file is local data, and every change to it that matters here is an accident
/// @param data First byte of the range
/// @param bytes Length of the range, in bytes
/// @param seed Starting value, so that several ranges can be chained into one hash
/// @return Hash of the range
static cui64 HashBytes(cptrc data, csi64 bytes, cui64 seed) {
   cui8ptrc input = (cui8ptrc)data; // Not named 'byte': <rpcndr.h>, via windows.h, declares that as a type
   ui64     hash  = seed;

   for(si64 i = 0; i < bytes; ++i) hash = (hash ^ cui64(input[i])) * VALUES_HASH_PRIME;

   return hash;
}

/// Transforms all 16 lanes of one result set exactly once, using the widest vector unit the CPU provides.
/// JobSSE, JobAVX2 and JobAVX512 compute the same function element-wise, so each ladder below produces the
/// same 16 lanes from the same input -- which is what lets a "cpu.values" generated on one CPU be verified
/// on another of a different vector width; the ValidateLadder* checks above are what hold the three kernels
/// to that, and 'W' runs them before it generates anything. The generating half of GenerateValues, its
/// self-checking half and the fingerprint stored in the file's header must all walk the same ladder, so
/// there is only one
/// @param result The 16 lanes to transform, in place
static void RunGoldenLadder(RESULTS &result) {
   if(cfg.sys.cpuAVX512) {
      JobAVX512(result.avx512);
      JobAVX2(result.avx);
   } else if(cfg.sys.cpuAVX2) {
      JobAVX2((fl64x4&)result._fl64[0]);
      JobAVX2((fl64x4&)result._fl64[4]);
      JobAVX2(result.avx);
   } else { // Lanes 0~11 are the AVX-512 and AVX2 windows; lanes 12~15 are covered by the 3 calls below
      JobSSE((fl64x2&)result._fl64[0]);
      JobSSE((fl64x2&)result._fl64[2]);
      JobSSE((fl64x2&)result._fl64[4]);
      JobSSE((fl64x2&)result._fl64[6]);
      JobSSE((fl64x2&)result._fl64[8]);
      JobSSE((fl64x2&)result._fl64[10]);
   }
   JobSSE(result.sse);
   JobFPU(result.fpu);
   JobALU(result.alu);
}

/// Runs the golden-value ladder over a fixed seed and hashes the 16 lanes it returns, giving a value that
/// changes whenever the arithmetic of any kernel that ladder walks changes. Stored in the "cpu.values"
/// header, it makes "regenerate the file after editing a kernel" a rule the format enforces rather than one
/// the author has to remember. The seed is of the same sign and magnitude as those 'W' generates, so the
/// chains settle exactly as they do during a run: the first outer iteration of the FP kernels collapses an
/// input of any magnitude, no divisor in either kernel can reach zero, and no lane can leave the reals
/// @return Fingerprint of the job kernels, folded together with the settings they were built under
static cui64 KernelFingerprint(void) {
   RESULTS probe = {}; // Zeroed first: every byte of it is hashed, so none of them may be indeterminate

   for(ui8 i = 0; i < 15; ++i) probe._fl64[i] = fl64(0x0123456789ABCull >> (i & 0x03)) + fl64(i);
   probe.raw[15] = 0x0123456789ABCDEF; // Lane 15 is the ALU lane, and is a plain integer

   RunGoldenLadder(probe);

   return HashBytes(&probe, csi64(sizeof(RESULTS)), VALUES_BUILD_ID);
}

/// Reads an entire block from a file. ReadFile reports reaching the end of the file as success, so the byte
/// count is the only evidence that the whole block was there to be read
/// @param file Handle opened for reading
/// @param data First byte of the destination
/// @param bytes Length of the block, in bytes
/// @return true only if every byte was read
static cbool ReadBlock(cHANDLE file, ptrc data, cui32 bytes) {
   DWORD bytesRead = 0;

   return ReadFile(file, data, bytes, &bytesRead, 0) && bytesRead == bytes;
}

/// Writes an entire block to a file. WriteFile reports a partial write as success, so the byte count is the
/// only evidence that the whole block reached the file
/// @param file Handle opened for writing
/// @param data First byte of the source
/// @param bytes Length of the block, in bytes
/// @return true only if every byte was written
static cbool WriteBlock(cHANDLE file, cptrc data, cui32 bytes) {
   DWORD bytesWritten = 0;

   return WriteFile(file, data, bytes, &bytesWritten, 0) && bytesWritten == bytes;
}

/// Fills the header that precedes the two result blocks in "cpu.values"
/// @param header Header to populate
/// @param seeds Input block, written directly after the header
/// @param values Golden output block, written last
static void FillValuesHeader(VALUES_HEADER &header, cptrc seeds, cptrc values) {
   header.magic      = VALUES_FILE_MAGIC;
   header.version    = VALUES_FILE_VERSION;
   header.headerSize = ui32(sizeof(VALUES_HEADER));
   header.blockSize  = cui64(RESULTS_BUF_SIZE);
   header.buildID    = VALUES_BUILD_ID;
   header.kernelID   = KernelFingerprint();
   header.seedHash   = HashBytes(seeds, RESULTS_BUF_SIZE, VALUES_HASH_BASIS);
   header.valueHash  = HashBytes(values, RESULTS_BUF_SIZE, VALUES_HASH_BASIS);
   header.reserved   = 0;
}
//--- "cpu.values" file format ---//

//--- Thread completion bitmap ---//
// One bit per thread, cleared by each thread as it exits. Every write is interlocked and byte-wide: a worker
// clears its own bit with _InterlockedAnd8, so a plain read-modify-write from wmain over the same byte can
// drop that clear and leave the wait loop below polling for a thread that has already exited

/// Marks a thread as running, leaving the other seven bits of its byte untouched
/// @param thread Index of the thread whose completion bit is to be set
static inline void SetThreadRunning(csi32 thread) {
   _InterlockedOr8(&((vchptr)threadBits)[thread >> 3], ui8(1u << (thread & 0x07)));
}

/// Marks a thread as finished, leaving the other seven bits of its byte untouched
/// @param thread Index of the thread whose completion bit is to be cleared
static inline void ClearThreadRunning(csi32 thread) {
   _InterlockedAnd8(&((vchptr)threadBits)[thread >> 3], ui8(~(1u << (thread & 0x07))));
}

/// Hands the vector polls below an address the load intrinsics will accept, with the guarantee the map's
/// volatile qualifier is there to give restored by a compiler barrier. The map changes under the reader's
/// feet, but no _mm*_load intrinsic takes a volatile pointer, so every vector poll casts the qualifier away
/// and the compiler is formally free to load the map once and spin on that copy for the rest of the run
/// (ISSUES.MD D9). No load may be carried across the barrier, and the barrier is inlined into wmain's wait
/// loop along with the load that follows it, so each poll reads the map afresh; it emits no instruction
/// @return Address of the completion bitmap, without the volatile qualifier
static inline ptr ThreadBitsView(void) {
   std::atomic_signal_fence(std::memory_order_acq_rel);

   return (ptr)threadBits;
}

///--- Expand beyond 512 cores ---///
// threadBits is an array of ui64, so a 512-bit view of it spans 8 elements, a 256-bit view 4 and a 128-bit
// view 2. Advancing by one element per vector step re-read bits already examined and left the tail of the
// map unexamined altogether -- bytes 48~63 for the AVX2 poll, 40~63 for the SSE one -- and bound vector
// references to addresses their alignment does not permit. The loads are unaligned forms because the poll
// must stay well-defined for any future change to the map's size or alignment; the addresses below are all
// naturally aligned today, so no instruction is added on any current CPU
//
// The scalar poll is the baseline the other three are selected over, and it is not decoration: AllFalse of
// two ui128 is _mm_testz_si128, an SSE4.1 instruction, so binding the SSE poll on nothing but the absence of
// AVX2 executed an illegal instruction on a Core 2 or an early Athlon 64 X2 before any test began -- and
// before the pre-flight check that names a missing instruction set could be reached, which an ALU-only run
// never reaches at all (ISSUES.MD D4). Its reads keep the volatile qualifier, so it needs no barrier.
//
// It is also the only one of the four that can be defined here: the three vector polls read the map with
// SSE4.1, AVX and AVX-512 instructions, and this header is included by CPU.cpp, which is compiled at the
// SSE2 baseline. Each of those therefore lives in the translation unit built for its own instruction set,
// beside the job kernels of the same width (ISSUES.MD H4)
inline bool ThreadsRunningScalar(void) {
   ui64 bits = 0;

   for(ui32 i = 0; i < MAX_THREADS_WORDS; ++i) bits |= threadBits[i];

   return bits ? true : false;
}
extern bool ThreadsRunningSSE(void);    // CPU_jobs_SSE.cpp
extern bool ThreadsRunningAVX(void);    // CPU_jobs_AVX.cpp
extern bool ThreadsRunningAVX512(void); // CPU_jobs_AVX512.cpp
///--- Expand beyond 512 cores ---///

typedef HANDLE *const HANDLEptrc;

/// Waits for every worker thread to actually end, then releases the handles. The completion bitmap cannot
/// stand in for this: a thread clears its own bit from inside its body, with its timer close, its return and
/// the CRT's own thread shutdown all still ahead of it, so the poll above releases wmain while the workers
/// are still executing -- over the result planes wmain is about to read and the arena it is about to free
/// (ISSUES.MD D8). Waiting on the handles is what makes "every thread has finished" true rather than
/// "every thread is about to"
/// @param handle Thread handles from _beginthreadex, in thread order; a null entry is skipped
/// @param count  Number of handles to wait on
static void JoinThreads(HANDLEptrc handle, csi32 count) {
   for(si32 i = 0; i < count; ++i) {
      if(!handle[i]) continue;

      WaitForSingleObject(handle[i], INFINITE);
      CloseHandle(handle[i]);

      handle[i] = 0;
   }
}

/// Releases the handles of the threads already spawned, without waiting on any of them. This is the abort
/// path's counterpart to JoinThreads: those threads would run to the end of the test they were given, and
/// wmain is returning an error rather than reading their results. Closing a handle does not end the thread
/// it names, it gives up only this thread's claim on the object
/// @param handle Thread handles from _beginthreadex, in thread order; a null entry is skipped
/// @param count  Number of handles to release
static void ReleaseThreads(HANDLEptrc handle, csi32 count) {
   for(si32 i = 0; i < count; ++i)
      if(handle[i]) { CloseHandle(handle[i]); handle[i] = 0; }
}
//--- Thread completion bitmap ---//

//--- High-resolution pulse timing ---//
// Windows' scheduler tick is 15.625ms unless a process asks for better, so a Sleep()-driven pulse boundary
// lands up to a tick late and a pulse shorter than a tick is not representable at all. A waitable timer
// created with CREATE_WAITABLE_TIMER_HIGH_RESOLUTION is serviced by the kernel's high-resolution timer
// instead, which is accurate to well under a millisecond, and unlike timeBeginPeriod it does not raise the
// tick rate for every process on the machine - which would itself alter the idle behaviour under test

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION // Declared by the Windows 10 1803 SDK and later
   #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x02
#endif

constexpr csi64 UNITS_PER_SECOND = 10000000; // Waitable timer due-time units (100ns) per second
constexpr csi64 UNITS_PER_MS     = 10000;    // Waitable timer due-time units (100ns) per millisecond

/// Creates the object a computation thread waits on at each pulse boundary. Every thread must own one:
/// SetWaitableTimer on a shared handle would cancel the interval another thread is already waiting on
/// @return Timer handle, to be closed by the calling thread, or 0 if no waitable timer could be created
static cHANDLE CreatePulseTimer(void) {
   cHANDLE hTimer = CreateWaitableTimerExW(0, 0, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);

   // Kernels older than Windows 10 1803 reject the flag; the tick-quantised timer is then the best available
   return hTimer ? hTimer : CreateWaitableTimerExW(0, 0, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
}

/// Reads the performance counter. The global timer is shared by every thread and CLASS_TIMER::Update is a
/// multi-field read-modify-write over siPrevTics, siTotalTics, siElapsedTics, dTotal, dScale and
/// dGrandTotal, so calling it from a worker is an unsynchronised data race that leaves those fields
/// meaningless and makes each thread's notion of "now" whatever another thread last wrote. Only
/// timer.siFrequency may be read from a worker: it is written once, by the constructor, before wmain runs
/// @return The current performance-counter reading, in tics
static inline csi64 CurrentTics(void) {
   si64 tics;

   QueryPerformanceCounter((LARGE_INTEGER *)&tics);

   return tics;
}

/// Converts a tic count into the 100ns units a waitable timer's due time is measured in
/// @param tics Duration in performance-counter tics
/// @return The same duration in 100ns units; the split keeps multi-hour durations from overflowing
static inline csi64 TicsTo100ns(csi64 tics) {
   return (tics / timer.siFrequency) * UNITS_PER_SECOND + ((tics % timer.siFrequency) * UNITS_PER_SECOND) / timer.siFrequency;
}

/// Suspends the calling thread for the given number of 100ns units
/// @param hTimer Timer from CreatePulseTimer, or 0 to fall back to the tick-quantised Sleep
/// @param units Delay in 100ns units; zero or less returns immediately
static void PulseWait(cHANDLE hTimer, csi64 units) {
   LARGE_INTEGER dueTime;

   if(units <= 0) return;

   dueTime.QuadPart = -units; // A negative due time is relative to now; a positive one is an absolute date

   if(hTimer && SetWaitableTimer(hTimer, &dueTime, 0, 0, 0, false))
      WaitForSingleObject(hTimer, INFINITE);
   else // Rounded up, so a sub-millisecond delay never becomes a Sleep(0) that merely yields the time slice
      Sleep(DWORD((units + (UNITS_PER_MS - 1)) / UNITS_PER_MS));
}

/// Suspends the calling thread for the given number of milliseconds
/// @param hTimer Timer from CreatePulseTimer, or 0 to fall back to the tick-quantised Sleep
/// @param milliseconds Delay in milliseconds
static inline void PulseSleep(cHANDLE hTimer, cui32 milliseconds) { PulseWait(hTimer, csi64(milliseconds) * UNITS_PER_MS); }

/// Suspends the calling thread until the global timer reaches targetTics, re-arming for any early wake-up.
/// The residual error is the timer's wake-up accuracy rather than the scheduler tick a Sleep(1) poll costs
/// @param hTimer Timer from CreatePulseTimer, or 0 to fall back to the tick-quantised Sleep
/// @param targetTics Absolute tic count to wait for; a count already reached returns immediately
static void PulseWaitUntil(cHANDLE hTimer, csi64 targetTics) {
   for(si64 curTics = CurrentTics(); targetTics > curTics; curTics = CurrentTics())
      PulseWait(hTimer, TicsTo100ns(targetTics - curTics));
}
//--- High-resolution pulse timing ---//

//--- Pulse jitter ---//
// A parallel thread that is not time-synchronised offsets its pulse train by a random amount and jitters its
// period, so that the threads do not all step at one instant: that difference is the whole of what 'Spt'
// promises over 'Sp'. Both offsets used to be drawn from rand(), which cannot deliver it -- the MSVC CRT
// keeps rand()'s state in per-thread storage initialised to the same default seed and srand is never called,
// so every thread of a run drew the same two values and the offsets desynchronised nothing (ISSUES.MD D6).
// They were also drawn once per thread and then applied to every cycle, which is a permanent change of
// period and of duty cycle rather than jitter (D7); the generator below is cheap enough to be read at every
// pulse boundary, which is what makes the per-cycle half of that pair an offset that averages out

constexpr cui64 JITTER_GAMMA = 0x09E3779B97F4A7C15; // SplitMix64's increment, and the seed's per-thread stride
constexpr cui64 JITTER_MIX_1 = 0x0BF58476D1CE4E5B9; // First multiplier of SplitMix64's finaliser
constexpr cui64 JITTER_MIX_2 = 0x094D049BB133111EB; // Second multiplier of SplitMix64's finaliser

/// Seeds one thread's jitter generator. rand_s reads the OS generator, needs no seeding and is safe to call
/// from any thread; the performance counter and the thread's own index are mixed in regardless, so that a
/// CRT which cannot reach the generator still leaves every thread of every run with a sequence of its own
/// @param coreNum Index of the calling thread
/// @return A seed for NextJitter
static inline cui64 JitterSeed(cui32 coreNum) {
   ui32 entropy = 0;

   if(rand_s(&entropy)) entropy = 0; // Non-zero is a failure; the two terms below still differ per thread

   return (cui64(entropy) << 32) ^ cui64(CurrentTics()) ^ ((cui64(coreNum) + 1) * JITTER_GAMMA);
}

/// Draws the next value of a thread's jitter sequence. SplitMix64: a counter through a multiply-xor-shift
/// finaliser, which is uniform over the whole of its period and costs a handful of cycles beside a pulse
/// @param state Generator state, advanced in place
/// @param span  Width of the window to draw from, in tics
/// @return A value in [0, span), or 0 if span is not positive
static inline csi64 NextJitter(ui64 &state, csi64 span) {
   ui64 z = (state += JITTER_GAMMA);

   z = (z ^ (z >> 30)) * JITTER_MIX_1;
   z = (z ^ (z >> 27)) * JITTER_MIX_2;
   z ^= z >> 31;

   return span > 0 ? si64(z % cui64(span)) : 0;
}
//--- Pulse jitter ---//

//--- Processor topology ---//

/// Isolates the lowest set bit of a bitmap
/// @param mask Bitmap to scan
/// @return The lowest set bit of mask; 0 if mask is empty
static inline cui64 LowestSetBit64(cui64 mask) { return mask & (~mask + 1ull); }

/// Isolates the highest set bit of a bitmap. The smear-down-and-subtract form is branchless and defined for
/// every input, an empty mask included, where a bit-scan intrinsic leaves its index operand untouched
/// @param mask Bitmap to scan
/// @return The highest set bit of mask; 0 if mask is empty
static inline cui64 HighestSetBit64(cui64 mask) {
   ui64 smear = mask;

   smear |= smear >> 1;   smear |= smear >> 2;    smear |= smear >> 4;
   smear |= smear >> 8;   smear |= smear >> 16;   smear |= smear >> 32;

   return smear - (smear >> 1);
}

/// Counts the set bits of a bitmap. Intrinsic-free for the same reason the two above are, and load-bearingly
/// so: on x64 MSVC winnt.h's PopulationCount64 resolves to __popcnt64, and no /arch setting gates that
/// intrinsic -- it emits POPCNT whatever baseline the translation unit was compiled at. This is called from
/// EnumerateTopology, which is the first thing wmain does, so on an x64 CPU predating POPCNT (any Core 2
/// including Penryn, an early Athlon 64 X2) every invocation of the program -- the bare help screen included
/// -- died with an illegal-instruction exception before printing anything. Those are precisely the CPUs the
/// scalar job unit and ThreadsRunningScalar exist to serve (ISSUES.MD G1, and D4 before it).
///
/// The SWAR fold below carries no such promise in the other direction: a compiler that recognises the idiom
/// substitutes POPCNT for it only where the ISA it was told to target carries the instruction, which is the
/// property the intrinsic lacks. None of the three call sites is hot -- one per topology record, and one per
/// processor group in each of two counting loops
/// @param mask Bitmap to count
/// @return The number of set bits in mask, 0 to 64
static inline cui8 SetBitCount64(cui64 mask) {
   ui64 count = mask - ((mask >> 1) & 0x05555555555555555ull);                          // Per bit pair

   count = (count & 0x03333333333333333ull) + ((count >> 2) & 0x03333333333333333ull);  // Per nibble
   count = (count + (count >> 4)) & 0x00F0F0F0F0F0F0F0Full;                             // Per byte

   return ui8((count * 0x00101010101010101ull) >> 56); // The multiply sums all eight bytes into the top one
}

typedef       SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *const SLPIEXptrc;
typedef const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *const cSLPIEXptrc;

/// Running state of the topology walk, carried across its three passes
struct TOPOLOGY_SCAN { // 12 bytes
   si32 vCores  = 0;     // Virtual cores accepted into the maps
   si32 dropped = 0;     // Virtual cores refused for want of a thread slot
   ui8  effLow  = 0x0FF; // Lowest EfficiencyClass any processor core reported; 0x0FF if no core was seen
   ui8  effHigh = 0;     // Highest EfficiencyClass any processor core reported
   ///--- 2 bytes unused
}; typedef TOPOLOGY_SCAN *const TOPOLOGY_SCANptrc;

/// Files one physical core into one of the program's two core classes. The narrower class is 0 and the wider
/// 1, and which property makes a core "wider" is the machine's to say: a conventional CPU is split by SMT
/// width, a hybrid one by core design.
///
/// The sibling count alone, which is what this used to be, sorts an Intel P/E-core part correctly and names
/// it wrongly -- the E-cores land in a class labelled "non-SMT" and the P-cores in one labelled "SMT", with
/// two sets of cache sizes to match and nothing anywhere saying so. It also stops sorting it at all the
/// moment SMT is disabled in firmware: every core then carries one virtual core, so an eight-P plus
/// sixteen-E machine becomes one undifferentiated class of 24 (ISSUES.MD G9). EfficiencyClass is the OS's
/// own answer to the same question, is 0 for every core of a machine that is not hybrid, and orders the
/// classes the same way round -- 0 is the least performant -- so a machine reporting one class falls back to
/// the sibling count, which is the property that describes it
/// @param efficiency EfficiencyClass of the core, from its RelationProcessorCore record
/// @param vCores Number of virtual cores the core carries
/// @param topClass Highest EfficiencyClass the machine reported, from pass 0 of the walk
/// @return 0 for the narrower class (non-SMT, or efficiency), 1 for the wider (SMT, or performance)
static inline cui8 CoreClass(cui8 efficiency, csi32 vCores, cui8 topClass) {
   // Three or more efficiency classes -- a performance, an efficiency and a low-power-efficiency tier, as
   // Intel's Meteor Lake reports -- collapse onto the two this program has: the most performant tier is
   // class 1 and everything below it class 0, which keeps "class 1 is the widest core the machine has" true
   // whatever the tier count
   if(cfg.sys.hybrid) return efficiency >= topClass ? 1 : 0;

   return vCores > 1 ? 1 : 0;
}

/// One pass of the processor-topology walk.
///
/// Records are stepped by the Size each one carries rather than by a fixed stride, so the walk stops on
/// anything it cannot step over: a Size of 0 would not advance, and a Size reaching past the buffer describes
/// a record the API never wrote. Relationship and Size are the two fields every record opens with, so a tail
/// too short to hold them cannot name a record at all.
///
/// The three passes are three walks over the one buffer because each needs the one before it complete, and
/// GetLogicalProcessorInformationEx documents no order for the records it returns:
///   0 - the range of EfficiencyClass values the machine reports. No single core can say whether the machine
///       is hybrid, so every core record is read before any core is filed
///   1 - the core maps, the sibling bitmaps, and the per-class core counts and SMT widths
///   2 - the cache sizes, filed under the class of the cores each cache serves, which only the finished maps
///       of pass 1 can answer
/// @param buffer First byte of the GetLogicalProcessorInformationEx(RelationAll) buffer
/// @param bytes Length of the buffer, in bytes
/// @param pass Which pass to perform; 0=Efficiency classes, 1=Cores, 2=Caches
/// @param scan Running state of the walk, updated by passes 0 and 1
static void WalkTopology(cptrc buffer, cui32 bytes, cui8 pass, TOPOLOGY_SCANptrc scan) {
   ui32 os = 0;

   while(os + ui32(sizeof(LOGICAL_PROCESSOR_RELATIONSHIP) + sizeof(DWORD)) <= bytes) {
      cSLPIEXptrc lpi = (cSLPIEXptrc)&((cchptr)buffer)[os];

      if(!lpi->Size || os + lpi->Size > bytes) break;
      os += lpi->Size;

      switch(lpi->Relationship) {
      case RelationProcessorCore: {
         // A processor core never spans processor groups, so GroupCount is documented to be 1 for this
         // relationship and GroupMask[0] is the whole of the core
         cui64 coreMask   = lpi->Processor.GroupMask[0].Mask;
         cui16 group      = lpi->Processor.GroupMask[0].Group;
         cui8  efficiency = lpi->Processor.EfficiencyClass;
         csi32 coreVCores = si32(SetBitCount64(coreMask));

         if(!coreMask) break; // A core of no virtual cores

         if(!pass) { // Pass 0 reads nothing but the class the OS assigns the core
            if(scan->effLow  > efficiency) scan->effLow  = efficiency;
            if(scan->effHigh < efficiency) scan->effHigh = efficiency;
            break;
         }
         if(pass != 1 || group >= MAX_GROUPS) break; // A group the maps cannot address

         cui8 coreClass = CoreClass(efficiency, coreVCores, scan->effHigh);

         if(cfg.sys.coreMap[coreClass][group] & coreMask) break; // The map is what remembers having seen it

         // Every thread-indexed table -- threadData, the four result planes, the completion bitmap -- holds
         // MAX_THREADS entries, and a thread's index is its ordinal among the selected cores. A machine of
         // more virtual cores than that is trimmed here, where the surplus is still countable and can be
         // reported, rather than left to be written past the end of them. Once one core has been refused
         // every later one is too, tested by 'dropped' rather than by the count: taking a narrower core that
         // still fits would leave a hole in the middle of a group's map for no gain
         if(scan->dropped || scan->vCores + coreVCores > MAX_THREADS) { scan->dropped += coreVCores; break; }

         scan->vCores += coreVCores;
         ++cfg.sys.coreCount[coreClass]; // A core already in the map returned above, so this counts each once
         cfg.sys.coreMap[coreClass][group] |= coreMask;
         // Which virtual cores share a physical core is knowable only here, one record at a time: the maps
         // above are unions and cannot answer it afterwards, which is why SetSMTLoading used to rebuild the
         // sibling layout from a core count and a stride and got it wrong three ways (ISSUES.MD G1, G2, G7).
         // A core with no SMT contributes the same bit to both maps, so it survives either policy
         cfg.sys.coreSibling[0][group] |= LowestSetBit64(coreMask);
         cfg.sys.coreSibling[1][group] |= HighestSetBit64(coreMask);
         // The SMT width is per class, and is the core's own sibling count rather than Processor.Flags:
         // LTP_PC_SMT is set for exactly the cores carrying more than one virtual core, so it answers the
         // same question less precisely -- and one machine-wide maximum described a hybrid part's classes as
         // both being as wide as its widest (ISSUES.MD G9)
         if(cfg.sys.SMT[coreClass] < ui8(coreVCores)) cfg.sys.SMT[coreClass] = ui8(coreVCores);
         break;
      }
      case RelationCache: {
         if(pass != 2) break;

         cui64 cacheMask  = lpi->Cache.GroupMask.Mask;
         cui16 cacheGroup = lpi->Cache.GroupMask.Group;

         if(!cacheMask || cacheGroup >= MAX_GROUPS) break;

         cui64 classMap[2] = { cfg.sys.coreMap[0][cacheGroup], cfg.sys.coreMap[1][cacheGroup] };

         // A cache belongs to the class of the cores it serves, which the maps of pass 1 answer directly.
         // The class used to be the population count of the cache's own mask -- "more than one logical
         // processor shares this" -- which is a different question with a coincidentally similar answer on a
         // conventional SMT part and a wrong one elsewhere: the L2 of a four-core E-core cluster counted 4
         // and was filed as a performance-core cache, and on a CPU without SMT every shared cache in the
         // machine was filed under a class holding no cores at all (ISSUES.MD G9)
         if(!(cacheMask & (classMap[0] | classMap[1]))) break; // Serves no core this build accepted

         cui8 coreClass = (cacheMask & classMap[1] ? 1 : 0);

         switch(lpi->Cache.Level) {
         case 1:
            if(lpi->Cache.Type == CacheInstruction) // Set (smallest) L1 code size
               if(!cfg.sys.cache[coreClass].L1Code || cfg.sys.cache[coreClass].L1Code > lpi->Cache.CacheSize)
                  cfg.sys.cache[coreClass].L1Code = lpi->Cache.CacheSize;
            if(lpi->Cache.Type == CacheData) // Set (smallest) L1 data size
               if(!cfg.sys.cache[coreClass].L1Data || cfg.sys.cache[coreClass].L1Data > lpi->Cache.CacheSize)
                  cfg.sys.cache[coreClass].L1Data = lpi->Cache.CacheSize;
            break;
         case 2:
            if(!cfg.sys.cache[coreClass].L2 || cfg.sys.cache[coreClass].L2 > lpi->Cache.CacheSize) // Set (smallest) L2 size
               cfg.sys.cache[coreClass].L2 = lpi->Cache.CacheSize;
            break;
         case 3:
            if(!cfg.sys.cacheL3 || cfg.sys.cacheL3 > lpi->Cache.CacheSize) // Set (smallest) L3 size
               cfg.sys.cacheL3 = lpi->Cache.CacheSize;
         }
         break;
      }
      default: // Numa node, package, group and everything a later Windows adds
         break;
      }
   }
}

/// Enumerates the system's processor topology into cfg.sys: the per-group core map of each thread class, the
/// sibling bitmaps SetSMTLoading masks with, the physical and virtual core counts, the cache sizes and the
/// per-class SMT widths. cfg.coreMap is left holding every virtual core found, which is what a run with no
/// 'U' argument selects.
///
/// GetLogicalProcessorInformation, which this replaces, cannot describe a machine of more than one processor
/// group: its ProcessorMask is a bare 64-bit affinity mask with no group to qualify it, so on a machine of
/// more than 64 virtual cores the walk saw one group's worth of cores and the rest of the program had no way
/// to name the others (ISSUES.MD G3). The Ex form reports one variable-length record per relationship, each
/// carrying a GROUP_AFFINITY -- the mask *and* the group it is a mask of -- which is what every map, the
/// banner and the affinity walk now carry through. It also carries the EfficiencyClass the two-way core
/// split is taken from on a hybrid machine (G9).
/// @return 0 on success; the exit code wmain is to return on failure
static csi32 EnumerateTopology(void) {
   TOPOLOGY_SCAN scan;
   DWORD         bytesProc = 0;
   ui8           g;

   // An Ex record is variable-length, so no record count bounds the buffer in advance the way one bounded the
   // fixed-stride array this replaced. A first call with no buffer is documented to fail with
   // ERROR_INSUFFICIENT_BUFFER, having written the size it wants
   SetLastError(ERROR_SUCCESS);
   GetLogicalProcessorInformationEx(RelationAll, 0, &bytesProc);
   if(GetLastError() != ERROR_INSUFFICIENT_BUFFER || !bytesProc) {
      wprintf(wstrMessage[31], ui32(GetLastError()));
      return -23;
   }

   ptrc sysLPI = zalloc64(bytesProc);

   // A topology that cannot be read is a topology that cannot be tested, whichever step failed to read it
   if(!sysLPI) {
      wprintf(wstrMessage[31], ui32(ERROR_NOT_ENOUGH_MEMORY));
      return -23;
   }

   if(!GetLogicalProcessorInformationEx(RelationAll, (SLPIEXptrc)sysLPI, &bytesProc)) {
      wprintf(wstrMessage[31], ui32(GetLastError()));
      mfree1(sysLPI);
      return -23;
   }

   WalkTopology(sysLPI, ui32(bytesProc), 0, &scan); // Which of the two classification rules this machine needs

   // A machine reporting more than one EfficiencyClass is one whose cores differ by design rather than by
   // SMT width, and that is the whole of the difference between the two rules CoreClass applies
   cfg.sys.hybrid = scan.effHigh > scan.effLow;

   WalkTopology(sysLPI, ui32(bytesProc), 1, &scan); // Core maps, sibling bitmaps, core counts, SMT widths
   WalkTopology(sysLPI, ui32(bytesProc), 2, &scan); // Cache sizes, filed by the class of the cores they serve

   mfree1(sysLPI);

   // A core carrying one virtual core is what "no SMT" means, and a class the machine does not have has no
   // width at all; both read as 1 rather than 0, so that no later expression can inherit a zero it would
   // shift or multiply by -- which is how a CPU reporting no SMT used to reach a shift of ui64(0 - 1)
   // (ISSUES.MD G1)
   if(!cfg.sys.SMT[0]) cfg.sys.SMT[0] = 1;
   if(!cfg.sys.SMT[1]) cfg.sys.SMT[1] = 1;

   // groupCount was an arithmetic prediction that omitted the SMT core count, doubled the non-SMT one and
   // added 1 (ISSUES.MD G4); counting the groups the walk populated states the same quantity and assumes
   // nothing about the topology it is counting. The scan runs to MAX_GROUPS rather than to the width of the
   // thread bitmap, because a group need not carry 64 virtual cores and 512 threads may span more than eight
   for(cfg.sys.groupCount = 0, g = 0; g < MAX_GROUPS; ++g)
      if(cfg.sys.coreMap[0][g] | cfg.sys.coreMap[1][g]) cfg.sys.groupCount = ui8(g + 1);

   // An enumeration that named no processor core leaves nothing to test: the core map is empty, so no thread
   // is created, and wmain would print an empty results table and return 0 -- "successful completion of
   // stability test" for a CPU that was never exercised. In the 'W' path it is a division by zero
   if(!cfg.sys.groupCount) {
      wprintf(wstrMessage[32]);
      return -23;
   }

   for(g = 0; g < cfg.sys.groupCount; ++g) cfg.coreMap[g] = cfg.sys.coreMap[0][g] | cfg.sys.coreMap[1][g];

   // The count is the population of the maps rather than coreCount[1] * SMT + coreCount[0], which is the same
   // number on every uniform and hybrid topology but assumes one SMT width for the whole machine
   cfg.sys.vCoreCount = si16(scan.vCores);

   // Testing part of a machine is a legitimate outcome; doing so without saying which part is not
   if(scan.dropped) wprintf(wstrMessage[33], scan.vCores + scan.dropped, si32(MAX_THREADS), scan.dropped);

   // Every per-class choice this program makes -- 'Mn' and 'Ms', the two cache records, the two thread
   // classes of the spawn loop -- is documented against a machine whose classes are its non-SMT and its SMT
   // cores. On a hybrid machine they are its efficiency and its performance cores instead, which is a
   // different split under the same names, and nothing anywhere used to say so (ISSUES.MD G9). It needs
   // saying only when it is true, so a conventional CPU prints nothing here
   if(cfg.sys.hybrid)
      wprintf(wstrMessage[34], si32(cfg.sys.coreCount[1]), si32(cfg.sys.SMT[1]),
                               si32(cfg.sys.coreCount[0]), si32(cfg.sys.SMT[0]));

   return 0;
}

/// Advances a core cursor to the next selected virtual core of one thread class, crossing processor-group
/// boundaries. A mask the caller has shifted past bit 63 is 0, which is the signal to resume at bit 0 of the
/// next group; the scan therefore covers every group from the cursor's position onwards exactly once.
/// @param mask Bit within the group to resume from; left holding the core's bit when one is found
/// @param group Processor group to resume from; advanced past every group holding no further core
/// @param threadClass Core class to look for; 0=Non-SMT or efficiency, 1=SMT or performance
/// @return true if a core was found, false if the selected maps hold no further core of that class
static cbool NextSelectedCore(ui64 &mask, ui8 &group, cui8 threadClass) {
   while(group < cfg.sys.groupCount) {
      if(mask & cfg.sys.coreMap[threadClass][group] & cfg.coreMap[group]) return true;
      if(!(mask <<= 1)) { mask = 1; ++group; }
   }
   return false;
}

/// Applies the SMT loading policy to the core map: a cfg.SMTLoad of 1 keeps the first virtual core of each
/// physical core, 2 the last, 3 adds every virtual core of each active physical core, and 0 leaves the map
/// as the enumeration and any 'U' argument left it. All three are exact expressions of one physical core at
/// a time, taken from the sibling bitmaps the enumeration records, and none of them reasons about a stride.
///
/// The two one-virtual-core policies were a single 'default' arm that built its mask arithmetically -- one
/// bit per cfg.sys.coreCount[1] entry, spaced cfg.sys.SMT apart, shifted up to the first set bit of the SMT
/// core map -- and ANDed it over the whole map. Three things were wrong with that. The mask described SMT
/// physical cores only, so the AND cleared every non-SMT core: on a hybrid part "one thread per physical
/// core" tested the P-cores and silently ignored every E-core, while the banner still reported the full
/// bitmap (ISSUES.MD G2). On a CPU reporting no SMT at all, cfg.sys.SMT is 0, so the "last sibling" shift
/// count was ui64(0 - 1) and the scan over an all-zero map ran to 64 -- two undefined shifts, from which
/// the core map was then built (G1). And the scan could only ever yield 0 or 64, because any non-zero map
/// is non-zero shifted right by zero, so it never found the offset it was written to find (G7).
/// Masking with the per-core sibling bitmaps the enumeration records needs none of that arithmetic: it is
/// exact for hybrid parts, for non-SMT parts, and for any virtual core numbering the OS reports.
///
/// The "use all virtual cores" arm was `cfg.coreMap[i] |= (cfg.coreMap[i] << j) & cfg.sys.coreMap[1][i]`
/// over j < the SMT width, and read the map it was writing, so each iteration smeared the already-smeared
/// map: at 4-way SMT the accumulated shift reaches past a core's own siblings and selects the *next*
/// physical core, which had been selected against, and the j == 0 iteration was a no-op (ISSUES.MD G8). Only
/// the class-1 map was consulted as well, so on a hybrid part whose two classes are both SMT -- a Zen 4 plus
/// Zen 4c machine, where the class split is by core design rather than SMT width (G9) -- the class-0 cores
/// were never expanded at all. The expansion below adds a physical core's whole span, once, to a separate
/// accumulator, for exactly the cores the map already holds a virtual core of
static void SetSMTLoading(void) {
   ui8 i;

   if(!cfg.SMTLoad) return;

   switch(cfg.SMTLoad) {
   case 3: // Use every virtual core of each active physical core
      for(i = 0; i < cfg.sys.groupCount; ++i) {
         cui64 groupMap = cfg.sys.coreMap[0][i] | cfg.sys.coreMap[1][i];
         ui64  selected = cfg.coreMap[i]; // Accumulated separately: a map read while it is written smears
         ui64  firsts   = cfg.sys.coreSibling[0][i];

         // One iteration per physical core of the group, walking the first-sibling bitmap. Pairing each
         // first with the lowest last-sibling bit at or above it names one core, because a core's siblings
         // are consecutive and no core's span can hold another's -- which is the same assumption the two
         // sibling bitmaps already encode, a core being described by its first virtual core and its last
         while(firsts) {
            cui64 first = LowestSetBit64(firsts);
            cui64 last  = LowestSetBit64(cfg.sys.coreSibling[1][i] & ~(first - 1ull));
            // 'last - first' is every bit between the two, so 'last | (last - first)' is the core. Every core
            // contributes its highest virtual core to the second bitmap, so a first bit the enumeration wrote
            // always has a last at or above it; a pair that disagreed would leave 'last' at 0, where the span
            // is the one bit rather than the 2^64 - first the subtraction would otherwise wrap to. Masking
            // with the group's own cores keeps a bit belonging to no core out of a map the banner prints
            cui64 span  = (last ? last | (last - first) : first) & groupMap;

            if(span & cfg.coreMap[i]) selected |= span;
            firsts ^= first;
         }
         cfg.coreMap[i] = selected;
      }
      break;
   case 1: // Use the first virtual core of each physical core
   case 2: // Use the last virtual core of each physical core
      for(i = 0; i < cfg.sys.groupCount; ++i) cfg.coreMap[i] &= cfg.sys.coreSibling[cfg.SMTLoad - 1][i];
   }
}
//--- Processor topology ---//

//--- Command-line parsing ---//
// Every numeric option was read by a bare wcstol whose stopChar was never examined, so an option carrying no
// value, or a negative one, produced a configuration the run then graded ".Pass." against: a zero-length
// test, a zero pulse on-time, a zero or wrapped allocation. The index advance was truncated to a ui8 as well,
// so a value longer than 255 characters moved the parse index backwards and the option loop could not
// terminate (ISSUES.MD F4). The two readers below are the only route a number takes into cfg, and neither
// hands back a value it has not range-checked

// Bounds every numeric option is validated against. They are deliberately generous: the point is to reject a
// missing, negative or wrapped value rather than to second-guess a legitimate one. OPT_MEM_MB_MAX is what
// keeps the byte count inside an si64 after the '<< 20' and after the per-thread multiply of MAX_THREADS
constexpr csi64 OPT_MEM_MB_MAX   = 16777216;   // Memory request, in MiB (16TiB)
constexpr csi64 OPT_PULSE_MS_MAX = 86400000;   // Pulse on- and off-times, in milliseconds (24 hours)
constexpr cfl64 OPT_DELAY_MAX    = 86400.0;    // Start-up delay, in seconds (24 hours)
constexpr cfl64 OPT_DURATION_MAX = 31536000.0; // Test duration, in seconds (365 days)

/// Reads a whole number from an option argument and range-checks it
/// @param str Argument being parsed
/// @param j Index of the option's letter; left on the last character of the value when one was read
/// @param low Lowest accepted value
/// @param high Highest accepted value
/// @param value Receives the value; untouched unless the read succeeds
/// @return true only if a value was present, complete and within range
static cbool ParseWholeNumber(cwchptrc str, ui32 &j, csi64 low, csi64 high, si64 &value) {
   cwchptrc first = &str[j + 1];
   wchptr   stopChar;

   // wcstoll skips leading whitespace, and would then take its value from beyond the field this option names.
   // A value has to begin where the option says it begins
   if(*first != L'-' && *first != L'+' && (*first < L'0' || *first > L'9')) return false;

   csi64 result = wcstoll(first, &stopChar, 10);

   // An overflowing field returns LLONG_MAX or LLONG_MIN, which every bound here is narrow enough to reject
   if(stopChar == first || result < low || result > high) return false;

   value = result;
   j     = ui32(stopChar - str) - 1; // The enclosing loop's ++j lands on the character that ended the value

   return true;
}

/// Reads a decimal value from an option argument and range-checks it
/// @param str Argument being parsed
/// @param j Index of the option's letter; left on the last character of the value when one was read
/// @param low Lowest accepted value
/// @param high Highest accepted value
/// @param value Receives the value; untouched unless the read succeeds
/// @return true only if a value was present, complete and within range
static cbool ParseDecimal(cwchptrc str, ui32 &j, cfl64 low, cfl64 high, fl64 &value) {
   cwchptrc first = &str[j + 1];
   wchptr   stopChar;

   // As above, and it additionally keeps the "inf" and "nan" spellings wcstod accepts out of a tic count
   if(*first != L'-' && *first != L'+' && (*first < L'0' || *first > L'9')) return false;

   cfl64 result = wcstod(first, &stopChar);

   if(stopChar == first || !(result >= low) || !(result <= high)) return false;

   value = result;
   j     = ui32(stopChar - str) - 1;

   return true;
}

/// Classifies one character of a 'Uc' or 'Ut' core-utilisation map.
///
/// "Any other character enables the core" cannot be reconciled with the documented spellings 'Uc!.!!...!a'
/// and 'Uc!.!!...!e', whose trailing letter is a further 'U' sub-option rather than the ninth core: the map
/// consumed every remaining character of the argument, so neither example could be written at all
/// (ISSUES.MD F2). The enabling characters are named here instead, and anything else ends the map and is
/// handed back to the 'U' loop as the sub-option it is
/// @param ch Character to classify
/// @return 0 to disable the core, 1 to enable it, 2 if the character is not part of a map
static cui8 CoreMapChar(cwchar ch) {
   switch(ch) {
   case L'.': case L',': case L'_': case L'-': case L'0':
      return 0;
   case L'!': case L'*': case L'#': case L'+': case L'1': case L'x': case L'X':
      return 1;
   default:
      return 2;
   }
}

/// Applies a 'Uc' (one character per physical core) or 'Ut' (one per virtual core) map to cfg.coreMap.
///
/// The map is the whole of the selection: cfg.coreMap is cleared before the characters are read, so a core
/// the map does not name is not utilised. Neither option used to clear it, so the characters only modified
/// whatever the topology walk had left there, and a map shorter than the machine kept the remainder.
///
/// Every index comes from the topology rather than from the character's position in the argument. 'Ut'
/// numbers the virtual cores group by group, so character n is bit n & 63 of group n >> 6. It used to set bit
/// j of coreMap[(j - 1) >> 3], which addresses a 64-bit word as though it held eight cores, disagrees with
/// its own bit index from the eighth character on, is off by two at the first -- j is 2 there, so cores 0 and
/// 1 could not be named at all -- and shifts by 64 or more from the 64th (ISSUES.MD F2, C10). 'Uc' numbers
/// the physical cores and sets or clears each one's whole span of virtual cores, read from the sibling
/// bitmaps one core at a time, which is exact for a hybrid part and for any SMT width. Both mask with the
/// cores the machine reported, so a character naming a core that does not exist cannot enter the map the
/// banner prints
/// @param str Argument being parsed
/// @param j Index of the map's first character; left on the last character the map consumed
/// @param physical true for 'Uc', false for 'Ut'
static void ParseCoreMap(cwchptrc str, ui32 &j, cbool physical) {
   ui32 pos = j;
   ui8  g;

   for(g = 0; g < cfg.sys.groupCount; ++g) cfg.coreMap[g] = 0;

   if(physical) {
      ui64 firsts = cfg.sys.coreSibling[0][0]; // EnumerateTopology refuses a machine of no groups, with -23
      ui8  group  = 0;
      ui8  state;

      for(; (state = CoreMapChar(str[pos])) < 2; ++pos) {
         while(!firsts && group + 1u < ui32(cfg.sys.groupCount)) firsts = cfg.sys.coreSibling[0][++group];
         if(!firsts) continue; // A map naming more cores than the machine holds; the surplus changes nothing

         cui64 first = LowestSetBit64(firsts);
         cui64 last  = LowestSetBit64(cfg.sys.coreSibling[1][group] & ~(first - 1ull));
         // 'last | (last - first)' is every bit from the core's first virtual core to its last -- the same
         // expression SetSMTLoading expands a physical core with. A first bit the enumeration could not pair
         // leaves 'last' at 0, where the span is the one virtual core it did report, rather than the
         // 2^64 - first the subtraction would otherwise wrap to
         cui64 span  = (last ? last | (last - first) : first) & (cfg.sys.coreMap[0][group] | cfg.sys.coreMap[1][group]);

         if(state) cfg.coreMap[group] |= span;
         firsts ^= first;
      }
   } else {
      ui32 index = 0;
      ui8  state;

      for(; (state = CoreMapChar(str[pos])) < 2; ++pos, ++index) {
         cui32 group = index >> 6;

         if(group >= ui32(cfg.sys.groupCount)) continue; // As above: a character beyond the machine's last core

         if(state)
            cfg.coreMap[group] |= (1ull << (index & 0x03F)) & (cfg.sys.coreMap[0][group] | cfg.sys.coreMap[1][group]);
      }
   }

   // The enclosing 'U' loop's ++j must land on the character that ended the map, which is where the
   // documented 'Uc!.!!...!a' spelling puts its next sub-option. A map of no characters at all leaves j on
   // the 'c' or 't' that introduced it, and the loop then re-reads the character that ended it
   j = pos - 1;
}
//--- Command-line parsing ---//

/// Prints computational failure data, and clears the calling thread's completion bit. Defined in CPU.cpp:
/// every caller is a job cycle, and those now live in the four CPU_jobs_*.cpp units (ISSUES.MD H4, H9)
/// @param coreNum Index of the thread that found the mismatch
/// @param threadByte Byte of the completion bitmap holding that thread's bit
/// @param unit Unit whose value[3] member the caller has just written; 0=AVX-512, 1=AVX2, 2=SSE, 3=FPU, 4=ALU
extern void Failed(cui64 coreNum, vchptrc threadByte, cui8 unit);

///--- Add vector versions ---///
// Evaluate integrity of results.
// unit==Processing unit (0=AVX512, 1=AVX2, 2=SSE4.1, 3=FPU, 4=ALU, -1=All)
static inline cui8 Evaluate(csi16 thread, csi8 unit) {
   ui8  index = unit == -1 ? 0  : 16 - (1 << (4 - unit));
   cui8 end   = unit == -1 ? 16 : (1 << max(0, 3 - unit)) + index;

   // The increment advances the loop, not the subscript of one operand. It used to sit inside the comparison
   // -- 'value[2][thread].raw[index] != value[3][thread].raw[index++]' -- where it modifies the very index
   // the left operand is subscripting with, and the operands of '!=' are unsequenced: a compilation that
   // evaluates the right operand first compares expected lane i+1 against observed lane i for every lane of
   // the window, and reads one lane past the end of it. This is the single verdict function in the program --
   // it grades every row of the results table, and gates the 65,536-iteration self-check 'W' will not write a
   // file without -- so the evaluation order the right answer depends on may not be the compiler's to choose
   // (ISSUES.MD A1)
   for(; index < end; ++index)
      if(value[2][thread].raw[index] != value[3][thread].raw[index])
         return 1;

   return 0;
}

// Number of times every generated entry is re-derived and compared before "cpu.values" may be written. The
// help text and README have always promised this figure; the loop below ran one short of it, and ran it for
// only the first entry of each thread's range (ISSUES.MD B4, B8). It does not fit a ui16 -- a counter of that
// width can never reach it, and the comparison would never end -- so the counter is a ui32
constexpr cui32 VALUES_SELF_CHECK_ITERATIONS = 65536;

// Generates output values. __stdcall and ui32-returning to match _beginthreadex's start-address signature
static ui32 __stdcall GenerateValues(ptr dataPtr) {
   RESULTS         resultCopy;
   cTHREAD_CFGptrc tcfg = (THREAD_CFGptrc)dataPtr;

   cui32 entries[2] = { MAX_THREADS / (cui32)cfg.sys.vCoreCount, MAX_THREADS % (cui32)cfg.sys.vCoreCount };

   // The final thread absorbs the remainder, so the ranges tile [0, MAX_THREADS) exactly: no gaps, no overlaps
   csi16 threadNum  = si16((cui64(tcfg->threadByte) << 3) + tcfg->threadBit);
   csi16 entryCount = si16(entries[0] + (threadNum == cfg.sys.vCoreCount - 1 ? entries[1] : 0));
   si16  coreNum    = si16(threadNum * entries[0]);
   csi16 range      = si16(coreNum + entryCount);
   cui8  threadMask = ui8(~(1u << tcfg->threadBit)); // Clears this thread's completion bit, preserving the other 7

   for(; coreNum < range; ++coreNum) {
      // Declared here, one per entry: as a variable of the whole function it was never reset, so every entry
      // after the first in this thread's range met a counter already at its limit, skipped the self-check
      // entirely, and was written to "cpu.values" unverified. Across a dozen threads that left about 2% of
      // the table checked, in place of the whole of it (ISSUES.MD B4)
      ui32 i = 0;

      value[2][coreNum] = value[3][coreNum];

      RunGoldenLadder(value[3][coreNum]);

      // Test computatational integrity. The self-check has to walk the same ladder as the generation above,
      // lane for lane, or it grades every entry against a different function; one shared ladder is what
      // makes that structural rather than a pair of blocks that have to be kept identical by hand
      for(resultCopy = value[2][coreNum]; i < VALUES_SELF_CHECK_ITERATIONS; ++i) {
         RunGoldenLadder(value[2][coreNum]);

         if(Evaluate(coreNum, -1) == 1) {
            // Recorded in a flag of its own, interlocked so that it is ordered ahead of the completion bit
            // this thread clears below. The two sentinel values that used to stand in for it were written
            // into value[2][0] and value[3][0] -- storage that belongs to entry 0, and that whichever thread
            // owns entry 0 overwrites wholesale on its way past. A computational error detected before that
            // thread got there was erased by it, and 'W' then wrote the file and reported success on a run
            // that had already failed (ISSUES.MD B7)
            _InterlockedOr8((vchptr)&generateError, 1);
            break;
         }

         value[2][coreNum] = resultCopy;
      }

      // One dot per entry generated, wide like every other write this program makes to stdout: byte I/O on a
      // stream the rest of the program has oriented wide is undefined in both languages, and worked here only
      // because the MSVC CRT implements no stream orientation at all (ISSUES.MD F4)
      wprintf(L".");
   }

   _InterlockedAnd8(&((chptr)threadBits)[tcfg->threadByte], threadMask);

   return 0; // Returning ends the thread: _beginthreadex's thunk calls _endthreadex with this value
}
