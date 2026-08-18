/*
 * File: CPU.h
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-01-25
 * Last Modified: 2026-08-18
 * Description: Core types, global declarations and scalar helpers: values-file format, completion bitmap, pulse timing, topology, parsing.
 * To Do: 1) Drop GLOBAL_CFG's in-class procUnits and procSync defaults; wmain overwrites both before an argument is read (ISSUES.MD K9)
 *        2) Remove THREAD_CFG's packetSizeRAM, maxTics, inactiveTime and records32, written by the spawn loop and read by nothing (K9)
 *        3) Raise MAX_THREADS past 512, widening every thread-indexed table with it
 *        4) Add vector forms of Evaluate
 * Dependencies: iostream, atomic, stdlib.h, string.h, locale.h, windows.h, process.h, memory management.h, class_timers.h,
 *               CPU_build.h, translations.h
 * ISA: Scalar
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
#pragma once

#define _CRT_RAND_S

#include <iostream>
#include <atomic>   // atomic_signal_fence, the compiler barrier the completion-bitmap polls read through
#include <stdlib.h> // rand_s, which is declared only for a translation unit that defined _CRT_RAND_S
#include <string.h> // memcmp, for the byte-exact comparisons in each unit's ValidateFamily*
#include <locale.h> // _create_locale and _free_locale, the invariant LC_NUMERIC ParseDecimal reads through
#include <windows.h>
#include <process.h>
#include "memory management.h"
#include "class_timers.h"
#include "CPU_build.h"

constexpr auto MAX_THREADS       = 512;
constexpr auto MAX_THREADS_BYTES = (MAX_THREADS + 7) >> 3;
constexpr auto MAX_THREADS_WORDS = (MAX_THREADS_BYTES + 7) >> 3;
constexpr auto MAX_GROUPS = 64;

//--- Core classes ---//
al32 struct GLOBAL_CFG { // 113 bytes
   struct _C_D_ { ui32 L1Code, L1Data, L2; };
   struct {
      struct {
         declare2d64z(ui64, coreMap, 2, MAX_THREADS); // Bitmaps of available virtual cores, per core class
         declare2d64z(ui64, coreSibling, 2, MAX_THREADS); // Bitmaps of one virtual core per physical core; 0=First, 1=Last
         _C_D_ cache[2];     // Sizes of L1 code & data and L2 caches, per core class
         ui32  cacheL3;      // Size of L3 cache per core complex
         si16  coreCount[2]; // Number of physical cores, per core class
      };
      si16 vCoreCount = 0;     // Total number of virtual processors
      ui8  groupCount = 0;     // Number of virtual processor groups
      ui8  SMT[2]     = { 0, 0 };
      bool hybrid     = false; // Classes are efficiency/performance cores rather than non-SMT/SMT cores
      bool cpuSSE2    = false; // CPU supports the SSE 1.0~2.0 instruction sets
      bool cpuAVX     = false; // CPU supports the AVX 1.0 instruction set
      bool cpuAVX512  = false; // CPU supports the AVX512F instruction set
      ///--- 7 bytes unused
   } sys;
   declare1d64z(ui64, coreMap, MAX_THREADS); // Bitmap of available virtual cores
   si64 tics        = 0;        // Global time limit
   si64 allocMem[2] = { 0, 0 }; // Amount(s) of memory allocated for threads, per core class
   ui32 onTime      = 100;      // Computation duration (in ms)
   ui32 offTime     = 900;      // Sleep duration (in ms)
   ui32 delayTime   = 2000;     // Start-up delay duration (in ms)
   ui8  procUnits   = 0x03;     // 0==ALU, 1==FPU, 2==SSE2, 3==AVX, 4==AVX512, 5==L1 cache, 6==L2 cache, 7==L3 cache
   ui8  procSync    = 0x012;    // 0==Round-robin, 1==Parallel 2==Staggered, 3==Synchronised, 4==Constant, 5==Fixed pulse, 6==Sweeping pulse, 7==Benchmark
   ui8  SMTLoad     = 0;        // Only utilise the specified virtual core(s) of each active physical core; 0=Unchanged, 1=First, 2=Last, 3=All
   ui8  memConfig   = 0;        // 0=Total equally split, 1=Per core, 2=Split per core class, 3=Derived from the requested cache level
   ui8  memExplicit = 0;        // An 'M' argument set the memory sizes; cache-derived sizing must not override them

   ~GLOBAL_CFG(void) { mfree(coreMap, sys.coreMap, sys.coreSibling); }
}; typedef GLOBAL_CFG *const GLOBAL_CFGptrc;

al64 struct THREAD_CFG { // 44 bytes
   si64 startTics;     // Thread start time (in tics)
   si64 endTics;       // Thread shutdown time (in tics)
   si64 activeTics;    // Computation duration (in tics)
   si64 cycleTics;     // Cycle duration (in tics)
   union {
      struct {
         ui16 records48[3]; // Total number of memory-bound records to process (48-bit value)
         ui16 threadCount;  // Total number of computation threads
      };
      ui64 rc_tc;
   };
   union {
      ui32 flags;
      struct {
         ui8 procUnits;  // 0==ALU, 1==FPU, 2==SSE2, 3==AVX, 4==AVX512, 5==L1 cache, 6==L2 cache, 7==L3 cache
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
   ptr       p            = 0;        // Master pointer
   si64ptr   iter         = 0;        // Total records processed per thread
   ui64      blockSize[2] = { 0, 0 }; // Memory per thread, per core class
   ui64      records[2]   = { 0, 0 }; // Memory records per thread, per core class

   ~RESULTS_ARRAYS(void) { mfree(p, iter); }
};

struct REPORT_BUFFER {
   wchptrc text; // The buffer, as the declare1d64z that allocates it produced; freed however its scope ends

   ~REPORT_BUFFER(void) { mfree1(text); }
}; typedef const REPORT_BUFFER cREPORT_BUFFER;

struct CONSOLE_CODE_PAGE {
   cui32 codePage = GetConsoleOutputCP(); // 0 where no console is attached; the destructor then restores nothing

   ~CONSOLE_CODE_PAGE(void) { if(codePage) SetConsoleOutputCP(codePage); }
}; typedef const CONSOLE_CODE_PAGE cCONSOLE_CODE_PAGE;

//--- Global variables ---//
extern al64 CLASS_TIMER timer;
extern      GLOBAL_CFG  cfg;

extern THREAD_CFG  *const threadData;          // declare1d64z(THREAD_CFG, threadData, MAX_THREADS)
extern RESULTS    (*const value)[MAX_THREADS]; // declare2d64z(RESULTS, value, 4, MAX_THREADS) -- Result values: 0==Input, 1=Processed, 2=Output, 3=Error

extern vui64 *const threadBits; // declare1d64z(vui64, threadBits, MAX_THREADS_WORDS)
extern wchar *const wstrOut;    // declare1d64z(wchar, wstrOut, 1024)

extern RESULTS_ARRAYS    resArray;
extern vsi8              generateError;
extern CONSOLE_CODE_PAGE consoleCP;

#include "translations.h"

inline cwchar outUTF16header   = 0x0FEFF;
inline cchar  outUTF8header[3] = { char(0x0EF), char(0x0BB), char(0x0BF) };
extern wchar  wstrLang[6];
//--- Global variables ---//

extern void JobALU(si64&);            extern void JobFPU(fl64&);                          extern void JobALU_FPU(fl64&, si64&);
extern void JobSSE(fl64x2&);          extern void JobALU_SSE(fl64x2&, si64&);
extern void JobAVX(fl64x4&);          extern void JobALU_AVX(fl64x4&, si64&);
extern void JobAVX512(fl64x8&);       extern void JobALU_AVX512(fl64x8&, si64&);
extern void JobMemALU(si64ptrc);      extern void JobMemFPU(fl64ptrc);                    extern void JobMemALU_FPU(fl64ptrc, si64ptrc);
extern void JobMemSSE(fl64x2ptrc);    extern void JobMemALU_SSE(fl64x2ptrc, si64ptrc);
extern void JobMemAVX(fl64x4ptrc);    extern void JobMemALU_AVX(fl64x4ptrc, si64ptrc);
extern void JobMemAVX512(fl64x8ptrc); extern void JobMemALU_AVX512(fl64x8ptrc, si64ptrc);

//--- Arena seeding ---//
/// @param records First record of the thread's slice of the arena
/// @param count Number of records in the slice
/// @param seed Value every record is to be given
extern void SeedRecordsALU   (si64ptrc   records, cui64 count, csi64    seed);
extern void SeedRecordsFPU   (fl64ptrc   records, cui64 count, cfl64    seed);
extern void SeedRecordsSSE   (fl64x2ptrc records, cui64 count, cfl64x2 &seed);
extern void SeedRecordsAVX   (fl64x4ptrc records, cui64 count, cfl64x4 &seed);
extern void SeedRecordsAVX512(fl64x8ptrc records, cui64 count, cfl64x8 &seed);
//--- Arena seeding ---//

//--- Job kernel cross-check ---//
inline cwchar wstrKernelName[17][20] = {
   L"",
   L"JobALU_FPU",    L"JobMemALU",    L"JobMemFPU",    L"JobMemALU_FPU",
   L"JobALU_SSE",    L"JobMemSSE",    L"JobMemALU_SSE",
   L"JobALU_AVX",    L"JobMemAVX",    L"JobMemALU_AVX",
   L"JobALU_AVX512", L"JobMemAVX512", L"JobMemALU_AVX512",
   L"JobSSE",        L"JobAVX",       L"JobAVX512"
};

// Where the table divides. Entries 1~13 name a memory-array or combined kernel, and the reference each of
// them failed to reproduce is the register-resident kernel of its own unit; entries from here on name a
// register-resident *vector* kernel, whose reference is JobFPU one lane at a time. The two failures mean
// different things to whatever reads the message, so wmain selects the message from this boundary
constexpr cui8 KERNEL_NAME_LADDER = 14;

// Lanes the cross-width ladder check runs through every vector kernel the CPU carries: one AVX-512 vector,
// two AVX vectors, four SSE vectors, and eight separate JobFPU calls, all over the same eight seeds
constexpr cui8 LADDER_PROBE_LANES = 8;

/// @param seed The one seed every kernel of the family is run over
/// @return 0 if every kernel of the family agreed; otherwise the wstrKernelName index of the first that did not
extern cui8 ValidateFamilyScalar(cRESULTS &seed); // JobALU_FPU, JobMemALU, JobMemFPU, JobMemALU_FPU
extern cui8 ValidateFamilySSE   (cRESULTS &seed); // JobALU_SSE, JobMemSSE, JobMemALU_SSE
extern cui8 ValidateFamilyAVX   (cRESULTS &seed); // JobALU_AVX, JobMemAVX, JobMemALU_AVX
extern cui8 ValidateFamilyAVX512(cRESULTS &seed); // JobALU_AVX512, JobMemAVX512, JobMemALU_AVX512

/// @param probe The lanes to transform, one seed per lane
/// @param reference The same lanes after JobFPU, one call per lane
/// @return 0 if every lane agreed; otherwise the wstrKernelName index of the vector kernel that did not
extern cui8 ValidateLadderSSE   (cfl64ptrc probe, cfl64ptrc reference); // JobSSE
extern cui8 ValidateLadderAVX   (cfl64ptrc probe, cfl64ptrc reference); // JobAVX
extern cui8 ValidateLadderAVX512(cfl64ptrc probe, cfl64ptrc reference); // JobAVX512

/// @return 0 if every kernel agreed; otherwise the wstrKernelName index of the first that did not
static cui8 ValidateKernelFamilies(void) {
   RESULTS seed = {}; // Zeroed first, so every lane stays defined if the seeding below is ever narrowed
   fl64    probe[LADDER_PROBE_LANES], reference[LADDER_PROBE_LANES];
   ui8     badKernel;

   // The seed KernelFingerprint probes with: a magnitude the FP chains settle from within two steps, and a
   // plain integer for the ALU lane
   for(ui8 k = 0; k < 15; ++k) seed._fl64[k] = fl64(0x0123456789ABCull >> (k & 0x03)) + fl64(k);
   seed.raw[15] = 0x0123456789ABCDEF;

   // ALU, FPU and SSE: the paths every x64 CPU carries, SSE being the golden ladder's fallback
   if((badKernel = ValidateFamilyScalar(seed)) != 0) return badKernel;
   if((badKernel = ValidateFamilySSE(seed))    != 0) return badKernel;

   // AVX and AVX-512: gated exactly as RunGoldenLadder gates them
   if(cfg.sys.cpuAVX    && (badKernel = ValidateFamilyAVX(seed))    != 0) return badKernel;
   if(cfg.sys.cpuAVX512 && (badKernel = ValidateFamilyAVX512(seed)) != 0) return badKernel;

   // The ladder itself: every vector width against the scalar kernel it stands in for
   for(ui8 k = 0; k < LADDER_PROBE_LANES; ++k) {
      probe[k] = reference[k] = seed._fl64[k];
      JobFPU(reference[k]);
   }

   if((badKernel = ValidateLadderSSE(probe, reference)) != 0) return badKernel;

   if(cfg.sys.cpuAVX    && (badKernel = ValidateLadderAVX(probe, reference))    != 0) return badKernel;
   if(cfg.sys.cpuAVX512 && (badKernel = ValidateLadderAVX512(probe, reference)) != 0) return badKernel;

   return 0;
}
//--- Job kernel cross-check ---//

//--- "cpu.values" file format ---//
// A 64-byte header followed by the two RESULTS[MAX_THREADS] blocks: the input seeds, then the golden
// outputs those seeds produce.
constexpr cui64 VALUES_FILE_MAGIC   = 0x0534C415643544950; // "PITCVALS", little-endian
constexpr cui32 VALUES_FILE_VERSION = 1;                   // Raised whenever VALUES_HEADER changes
constexpr cui64 VALUES_HASH_BASIS   = 0x0CBF29CE484222325; // FNV-1a 64-bit offset basis
constexpr cui64 VALUES_HASH_PRIME   = 0x0100000001B3;      // FNV-1a 64-bit prime

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

/// @param result The 16 lanes to transform, in place
static void RunGoldenLadder(RESULTS &result) {
   if(cfg.sys.cpuAVX512) {
      JobAVX512(result.avx512);
      JobAVX(result.avx);
   } else if(cfg.sys.cpuAVX) {
      JobAVX((fl64x4&)result._fl64[0]);
      JobAVX((fl64x4&)result._fl64[4]);
      JobAVX(result.avx);
   } else { // Lanes 0~11 are the AVX-512 and AVX windows; lanes 12~15 are covered by the 3 calls below
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

/// @return Fingerprint of the job kernels, folded together with the settings they were built under
static cui64 KernelFingerprint(void) {
   RESULTS probe = {}; // Zeroed first: every byte of it is hashed, so none of them may be indeterminate

   for(ui8 i = 0; i < 15; ++i) probe._fl64[i] = fl64(0x0123456789ABCull >> (i & 0x03)) + fl64(i);
   probe.raw[15] = 0x0123456789ABCDEF; // Lane 15 is the ALU lane, and is a plain integer

   RunGoldenLadder(probe);

   return HashBytes(&probe, csi64(sizeof(RESULTS)), VALUES_BUILD_ID);
}

/// @param file Handle opened for reading
/// @param data First byte of the destination
/// @param bytes Length of the block, in bytes
/// @return true only if every byte was read
static cbool ReadBlock(cHANDLE file, ptrc data, cui32 bytes) {
   DWORD bytesRead = 0;

   return ReadFile(file, data, bytes, &bytesRead, 0) && bytesRead == bytes;
}

/// @param file Handle opened for writing
/// @param data First byte of the source
/// @param bytes Length of the block, in bytes
/// @return true only if every byte was written
static cbool WriteBlock(cHANDLE file, cptrc data, cui32 bytes) {
   DWORD bytesWritten = 0;

   return WriteFile(file, data, bytes, &bytesWritten, 0) && bytesWritten == bytes;
}

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

/// @param thread Index of the thread whose completion bit is to be set
static inline void SetThreadRunning(csi32 thread) {
   _InterlockedOr8(&((vchptr)threadBits)[thread >> 3], ui8(1u << (thread & 0x07)));
}

/// @param thread Index of the thread whose completion bit is to be cleared
static inline void ClearThreadRunning(csi32 thread) {
   _InterlockedAnd8(&((vchptr)threadBits)[thread >> 3], ui8(~(1u << (thread & 0x07))));
}

/// @return Address of the completion bitmap, without the volatile qualifier
static inline ptr ThreadBitsView(void) {
   std::atomic_signal_fence(std::memory_order_acq_rel);

   return (ptr)threadBits;
}

///--- Expand beyond 512 cores ---///
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

/// @param handle Thread handles from _beginthreadex, in thread order; a null entry is skipped
/// @param count  Number of handles to release
static void ReleaseThreads(HANDLEptrc handle, csi32 count) {
   for(si32 i = 0; i < count; ++i)
      if(handle[i]) { CloseHandle(handle[i]); handle[i] = 0; }
}
//--- Thread completion bitmap ---//

//--- High-resolution pulse timing ---//
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION // Declared by the Windows 10 1803 SDK and later
   #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x02
#endif

constexpr csi64 UNITS_PER_SECOND = 10000000; // Waitable timer due-time units (100ns) per second
constexpr csi64 UNITS_PER_MS     = 10000;    // Waitable timer due-time units (100ns) per millisecond

/// @return Timer handle, to be closed by the calling thread, or 0 if no waitable timer could be created
static cHANDLE CreatePulseTimer(void) {
   cHANDLE hTimer = CreateWaitableTimerExW(0, 0, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);

   // Kernels older than Windows 10 1803 reject the flag; the tick-quantised timer is then the best available
   return hTimer ? hTimer : CreateWaitableTimerExW(0, 0, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
}

/// @return The current performance-counter reading, in tics
static inline csi64 CurrentTics(void) {
   si64 tics;

   QueryPerformanceCounter((LARGE_INTEGER *)&tics);

   return tics;
}

/// @param tics Duration in performance-counter tics
/// @return The same duration in 100ns units; the split keeps multi-hour durations from overflowing
static inline csi64 TicsTo100ns(csi64 tics) {
   return (tics / timer.siFrequency) * UNITS_PER_SECOND + ((tics % timer.siFrequency) * UNITS_PER_SECOND) / timer.siFrequency;
}

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

/// @param hTimer Timer from CreatePulseTimer, or 0 to fall back to the tick-quantised Sleep
/// @param milliseconds Delay in milliseconds
static inline void PulseSleep(cHANDLE hTimer, cui32 milliseconds) { PulseWait(hTimer, csi64(milliseconds) * UNITS_PER_MS); }

/// @param hTimer Timer from CreatePulseTimer, or 0 to fall back to the tick-quantised Sleep
/// @param targetTics Absolute tic count to wait for; a count already reached returns immediately
static void PulseWaitUntil(cHANDLE hTimer, csi64 targetTics) {
   for(si64 curTics = CurrentTics(); targetTics > curTics; curTics = CurrentTics())
      PulseWait(hTimer, TicsTo100ns(targetTics - curTics));
}
//--- High-resolution pulse timing ---//

//--- Pulse jitter ---//
constexpr cui64 JITTER_GAMMA = 0x09E3779B97F4A7C15; // SplitMix64's increment, and the seed's per-thread stride
constexpr cui64 JITTER_MIX_1 = 0x0BF58476D1CE4E5B9; // First multiplier of SplitMix64's finaliser
constexpr cui64 JITTER_MIX_2 = 0x094D049BB133111EB; // Second multiplier of SplitMix64's finaliser

/// @param coreNum Index of the calling thread
/// @return A seed for NextJitter
static inline cui64 JitterSeed(cui32 coreNum) {
   ui32 entropy = 0;

   if(rand_s(&entropy)) entropy = 0; // Non-zero is a failure; the two terms below still differ per thread

   return (cui64(entropy) << 32) ^ cui64(CurrentTics()) ^ ((cui64(coreNum) + 1) * JITTER_GAMMA);
}

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
/// @param mask Bitmap to scan
/// @return The lowest set bit of mask; 0 if mask is empty
static inline cui64 LowestSetBit64(cui64 mask) { return mask & (~mask + 1ull); }

/// @param mask Bitmap to scan
/// @return The highest set bit of mask; 0 if mask is empty
static inline cui64 HighestSetBit64(cui64 mask) {
   ui64 smear = mask;

   smear |= smear >> 1;   smear |= smear >> 2;    smear |= smear >> 4;
   smear |= smear >> 8;   smear |= smear >> 16;   smear |= smear >> 32;

   return smear - (smear >> 1);
}

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
   // 2 bytes unused
}; typedef TOPOLOGY_SCAN *const TOPOLOGY_SCANptrc;

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

         cfg.sys.coreSibling[0][group] |= LowestSetBit64(coreMask);
         cfg.sys.coreSibling[1][group] |= HighestSetBit64(coreMask);

         if(cfg.sys.SMT[coreClass] < ui8(coreVCores)) cfg.sys.SMT[coreClass] = ui8(coreVCores);
         break;
      }
      case RelationCache: {
         if(pass != 2) break;

         cui64 cacheMask  = lpi->Cache.GroupMask.Mask;
         cui16 cacheGroup = lpi->Cache.GroupMask.Group;

         if(!cacheMask || cacheGroup >= MAX_GROUPS) break;

         cui64 classMap[2] = { cfg.sys.coreMap[0][cacheGroup], cfg.sys.coreMap[1][cacheGroup] };

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

/// @return 0 on success; the exit code wmain is to return on failure
static csi32 EnumerateTopology(void) {
   TOPOLOGY_SCAN scan;
   DWORD         bytesProc = 0;
   ui8           g;

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
   // shift or multiply by
   if(!cfg.sys.SMT[0]) cfg.sys.SMT[0] = 1;
   if(!cfg.sys.SMT[1]) cfg.sys.SMT[1] = 1;

   for(cfg.sys.groupCount = 0, g = 0; g < MAX_GROUPS; ++g)
      if(cfg.sys.coreMap[0][g] | cfg.sys.coreMap[1][g]) cfg.sys.groupCount = ui8(g + 1);

   if(!cfg.sys.groupCount) {
      wprintf(wstrMessage[32]);
      return -23;
   }

   for(g = 0; g < cfg.sys.groupCount; ++g) cfg.coreMap[g] = cfg.sys.coreMap[0][g] | cfg.sys.coreMap[1][g];

   cfg.sys.vCoreCount = si16(scan.vCores);

   // Testing part of a machine is a legitimate outcome; doing so without saying which part is not
   if(scan.dropped) wprintf(wstrMessage[33], scan.vCores + scan.dropped, si32(MAX_THREADS), scan.dropped);

   // Every per-class choice this program makes -- 'Mn' and 'Ms', the two cache records, the two thread
   // classes of the spawn loop -- is documented against a machine whose classes are its non-SMT and its SMT
   // cores. On a hybrid machine they are its efficiency and its performance cores instead, which is a
   // different split under the same names. It needs saying only when it is true, so a conventional CPU
   // prints nothing here
   if(cfg.sys.hybrid)
      wprintf(wstrMessage[34], si32(cfg.sys.coreCount[1]), si32(cfg.sys.SMT[1]),
                               si32(cfg.sys.coreCount[0]), si32(cfg.sys.SMT[0]));

   return 0;
}

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

static void SetSMTLoading(void) {
   ui8 i;

   if(!cfg.SMTLoad) return;

   switch(cfg.SMTLoad) {
   case 3: // Use every virtual core of each active physical core
      for(i = 0; i < cfg.sys.groupCount; ++i) {
         cui64 groupMap = cfg.sys.coreMap[0][i] | cfg.sys.coreMap[1][i];
         ui64  selected = cfg.coreMap[i]; // Accumulated separately: a map read while it is written smears
         ui64  firsts   = cfg.sys.coreSibling[0][i];

         while(firsts) {
            cui64 first = LowestSetBit64(firsts);
            cui64 last  = LowestSetBit64(cfg.sys.coreSibling[1][i] & ~(first - 1ull));
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

//--- Cache-test block sizing ---//
constexpr cui64 CACHE_OCCUPANCY_NUM = 1, CACHE_OCCUPANCY_DEN = 2; // Target: the blocks fill half the level
constexpr cui64 CACHE_CEILING_NUM   = 3, CACHE_CEILING_DEN   = 4; // Hard cap: never above three quarters
constexpr cui64 CACHE_DEFEAT_MUL    = 2;                          // The blocks span twice the level below

/// @param unitBits Processing-unit selection, already masked to bits 0~4
/// @param recSize Receives the bytes one record of the selection occupies
/// @param vecUnits Receives the record's vector portion, in 8-byte units; 0 where no ALU offset is needed
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

/// @param units Processing-unit selection, all 8 bits
/// @return Requested cache level, 1~3; 0 when none of bits 5~7 is set
static cui8 HighestCacheLevel(cui8 units) { return units & 0x080 ? 3 : units & 0x040 ? 2 : units & 0x020 ? 1 : 0; }

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

// One instance per selected physical core is the worst case a level can present, and no more virtual cores
// than MAX_THREADS are ever selected, so the table cannot be filled by any topology this build accepts
constexpr cui32 MAX_CACHE_DOMAINS = MAX_THREADS;

al8 struct CACHE_DOMAIN { // 8 bytes
   ui32 size;      // CacheSize of the instance, in bytes
   ui16 n[2];      // Selected virtual cores of each core class the instance serves
   ui8  coreClass; // Class the instance files under, by the same rule WalkTopology's cache pass applies
   // 1 byte unused
};

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

   // Every path from here frees the buffer, the two failures included: GCS p2 requires a matching free for
   // every aligned allocation, and this one's lifetime ends inside this function whichever way it leaves
   if(!buffer) return 0;
   if(!GetLogicalProcessorInformationEx(RelationCache, (SLPIEXptrc)buffer, &bytes)) { mfree1(buffer); return 0; }

   for(ui32 os = 0; os + ui32(sizeof(LOGICAL_PROCESSOR_RELATIONSHIP) + sizeof(DWORD)) <= bytes; ) {
      cSLPIEXptrc lpi = (cSLPIEXptrc)&((cchptr)buffer)[os];

      if(!lpi->Size || os + lpi->Size > bytes) break;
      os += lpi->Size;

      if(lpi->Relationship != RelationCache || lpi->Cache.Level != level) continue;
      if(lpi->Cache.Type == CacheInstruction || lpi->Cache.Type == CacheTrace || !lpi->Cache.CacheSize) continue;

      cui64 mask  = lpi->Cache.GroupMask.Mask;
      cui16 group = lpi->Cache.GroupMask.Group;

      if(!mask || group >= MAX_GROUPS) continue; // A group the maps cannot address

      cui64 sel0 = mask & cfg.coreMap[group] & cfg.sys.coreMap[0][group];
      cui64 sel1 = mask & cfg.coreMap[group] & cfg.sys.coreMap[1][group];

      if(!(sel0 | sel1)) continue; // Serves no selected core, so no block of this run is resident in it

      if(count >= MAX_CACHE_DOMAINS) { count = 0; break; } // A table overrun reads as a failed query

      dom[count].size      = lpi->Cache.CacheSize;
      dom[count].n[0]      = SetBitCount64(sel0);
      dom[count].n[1]      = SetBitCount64(sel1);
      // Filed by the class of the cores it serves, which is the rule WalkTopology's cache pass applies to the
      // per-class cache records: a cache reaching any core of the wider class belongs to that class
      dom[count].coreClass = (mask & cfg.sys.coreMap[1][group]) ? 1 : 0;
      ++count;
   }
   mfree1(buffer);

   return count;
}

/// @param level Requested cache level, 1~3
/// @param recSize Bytes per record of the selected unit set, from RecordGeometry
/// @param threadCount Selected virtual cores per class, and their total, as wmain counts them
/// @param blockSize Receives each class's per-thread block size in bytes; 0 for a class with no threads
/// @param lower Receives each class's smallest level-resident size, for the explicit-'M' window warning
/// @param ceiling Receives each class's largest level-resident size, for the explicit-'M' window warning
/// @param feasibleK Receives, where the window was empty, the most threads one instance could hold resident
/// @return 0 on success; 1 if the window was empty and the defeat bound was used; -1 if a needed level is unreported
static csi8 CalcCacheBlockSizes(cui8 level,       cui64  recSize,      csi16 (&threadCount)[3], ui64 (&blockSize)[2],
                                ui64 (&lower)[2], ui64  (&ceiling)[2], ui32   &feasibleK) {
   CACHE_DOMAIN dom[MAX_CACHE_DOMAINS];
   cui64 floor8    = recSize << 3; // The 8-record granule of C12; no block may be smaller than one
   ui64  target[2] = { 0, 0 };
   ui32  domains   = 0, d = 0;
   si8   clamped   = 0;
   ui8   c;

   feasibleK    = 0;
   blockSize[0] = blockSize[1] = 0;
   lower[0]     = lower[1]     = floor8;
   ceiling[0]   = ceiling[1]   = 0;

   switch(level) {
   case 1:
      // Private to a physical core, so the only sharers are that core's own selected siblings, and the
      // level below it is the register file: there is no defeat bound beyond the structural floor
      for(c = 0; c < 2; ++c) {
         ui32 sibMin, sibMax;

         if(!threadCount[c]) continue;
         if(!cfg.sys.cache[c].L1Data) return -1;

         MinMaxSelectedSiblings(c, sibMin, sibMax);

         target[c]  = (cui64(cfg.sys.cache[c].L1Data) * CACHE_OCCUPANCY_NUM) / (CACHE_OCCUPANCY_DEN * sibMax);
         ceiling[c] = (cui64(cfg.sys.cache[c].L1Data) * CACHE_CEILING_NUM)   / (CACHE_CEILING_DEN   * sibMax);
      }
      break;
   case 2:
      if(!(domains = QueryCacheDomains(2, dom))) return -1;

      for(c = 0; c < 2; ++c) {
         ui32 sibMin, sibMax;
         ui64 ceilShare = 0;

         if(!threadCount[c]) continue;
         if(!cfg.sys.cache[c].L1Data) return -1; // The L1 the blocks must overflow to be tested at level 2

         MinMaxSelectedSiblings(c, sibMin, sibMax);

         // The smallest per-thread share any instance of the class grants: the ceiling has to hold on every
         // instance, so it is the worst-served thread that fixes it
         for(d = 0; d < domains; ++d) {
            if(dom[d].coreClass != c) continue;

            cui64 share = cui64(dom[d].size) / cui64(dom[d].n[0] + dom[d].n[1]);

            if(!ceilShare || ceilShare > share) ceilShare = share;
         }
         if(!ceilShare) return -1; // The class has threads and no level-2 instance serving them

         cui64 defeat = (cui64(cfg.sys.cache[c].L1Data) * CACHE_DEFEAT_MUL) / sibMin;

         if(lower[c] < defeat) lower[c] = defeat;

         target[c]  = (ceilShare * CACHE_OCCUPANCY_NUM) / CACHE_OCCUPANCY_DEN;
         ceiling[c] = (ceilShare * CACHE_CEILING_NUM)   / CACHE_CEILING_DEN;
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

         if(lower[c] < defeatShare * CACHE_DEFEAT_MUL) lower[c] = defeatShare * CACHE_DEFEAT_MUL;
      }

      // Residency budget: one level-3 instance serves cores of both classes, so the two sizes are chosen
      // jointly rather than one class at a time. The uniform share below is what the classes start from
      if(!(domains = QueryCacheDomains(3, dom))) return -1;

      for(d = 0; d < domains; ++d) {
         cui32 n = ui32(dom[d].n[0]) + ui32(dom[d].n[1]);

         if(!l3Min || l3Min > dom[d].size) l3Min = dom[d].size;
         if(n3Max < n)                     n3Max = n;
      }

      ceiling[0] = ceiling[1] = (l3Min * CACHE_CEILING_NUM) / (CACHE_CEILING_DEN * n3Max);

      for(c = 0; c < 2; ++c)
         if(threadCount[c]) target[c] = max((l3Min * CACHE_OCCUPANCY_NUM) / (CACHE_OCCUPANCY_DEN * n3Max), lower[c]);

      // The uniform share cannot breach any instance -- every instance sees at most n3Max threads of it, which
      // is half of the smallest capacity -- so only the raise to the defeat bound above can, and the fallback
      // is therefore exactly "run at the defeat bound and say the residency claim has been withdrawn"
      for(d = 0; d < domains; ++d) {
         cui64 budget = (cui64(dom[d].size) * CACHE_CEILING_NUM) / CACHE_CEILING_DEN;
         cui64 load   = cui64(dom[d].n[0]) * target[0] + cui64(dom[d].n[1]) * target[1];

         if(load <= budget) continue;

         // The largest defeat bound this instance has to carry, and so the most threads of it that could have
         // been held resident at once: the figure the warning reports for the user to select cores against
         cui64 maxLower = max(dom[d].n[0] ? lower[0] : 0ull, dom[d].n[1] ? lower[1] : 0ull);
         cui32 k        = ui32(budget / maxLower);

         if(!clamped || feasibleK > k) feasibleK = k;
         clamped = 1;
      }
      if(clamped) for(c = 0; c < 2; ++c) target[c] = threadCount[c] ? lower[c] : 0;
   }
   }

   for(c = 0; c < 2; ++c) {
      ui64 s = target[c];

      if(!threadCount[c]) continue;

      // Level 3 has verified its joint budget above, where a breach is a property of an instance serving both
      // classes rather than of either class's own window; the scalar clamp is levels 1 and 2
      if(level != 3) {
         if(lower[c] > ceiling[c]) { s = lower[c]; feasibleK = 1; clamped = 1; }
         else                        s = min(max(s, lower[c]), ceiling[c]);
      }

      // Rounded down to the 8-record granule last of all, so that the block is one the arena block will accept
      // unchanged: it undershoots the lower bound by at most 8 records, which is noise against the x2 and
      // three-quarter margins the bounds carry
      ui64 records = (s / recSize) & ~0x07ull;

      if(records < 8) records = 8;

      blockSize[c] = records * recSize;
   }

   return clamped;
}
//--- Cache-test block sizing ---//

//--- Command-line parsing ---//
// Bounds every numeric option is validated against. They are deliberately generous: the point is to reject a
// missing, negative or wrapped value rather than to second-guess a legitimate one. OPT_MEM_MB_MAX is what
// keeps the byte count inside an si64 after the '<< 20' and after the per-thread multiply of MAX_THREADS
constexpr csi64 OPT_MEM_MB_MAX   = 16777216;   // Memory request, in MiB (16TiB)
constexpr csi64 OPT_PULSE_MS_MAX = 86400000;   // Pulse on- and off-times, in milliseconds (24 hours)
constexpr cfl64 OPT_DELAY_MAX    = 86400.0;    // Start-up delay, in seconds (24 hours)
constexpr cfl64 OPT_DURATION_MAX = 31536000.0; // Test duration, in seconds (365 days)

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

// _locale_t is itself a pointer typedef, so the constant form of it is spelt here rather than as a raw
// 'const _locale_t' at the declaration below, the way cSLPIEXptrc spells the topology walk's record (GCS t2)
typedef _locale_t const localeptrc;

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

   localeptrc numeric = _create_locale(LC_NUMERIC, "C");
   cfl64      result  = (numeric ? _wcstod_l(first, &stopChar, numeric) : wcstod(first, &stopChar));

   if(numeric) _free_locale(numeric);

   if(stopChar == first || !(result >= low) || !(result <= high)) return false;

   value = result;
   j     = ui32(stopChar - str) - 1;

   return true;
}

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

/// @param coreNum Index of the thread that found the mismatch
/// @param threadByte Byte of the completion bitmap holding that thread's bit
/// @param unit Unit whose value[3] member the caller has just written; 0=AVX-512, 1=AVX, 2=SSE, 3=FPU, 4=ALU
extern void Failed(cui64 coreNum, vchptrc threadByte, cui8 unit);

///--- Add vector versions ---///
// Evaluate integrity of results.
// unit==Processing unit (0=AVX512, 1=AVX, 2=SSE, 3=FPU, 4=ALU, -1=All)
static inline cui8 Evaluate(csi16 thread, csi8 unit) {
   ui8  index = unit == -1 ? 0  : 16 - (1 << (4 - unit));
   cui8 end   = unit == -1 ? 16 : (1 << max(0, 3 - unit)) + index;

   for(; index < end; ++index)
      if(value[2][thread].raw[index] != value[3][thread].raw[index])
         return 1;

   return 0;
}

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
      ui32 i = 0;

      value[2][coreNum] = value[3][coreNum];

      RunGoldenLadder(value[3][coreNum]);

      // Test computatational integrity. The self-check has to walk the same ladder as the generation above,
      // lane for lane, or it grades every entry against a different function; one shared ladder is what
      // makes that structural rather than a pair of blocks that have to be kept identical by hand
      for(resultCopy = value[2][coreNum]; i < VALUES_SELF_CHECK_ITERATIONS; ++i) {
         RunGoldenLadder(value[2][coreNum]);

         if(Evaluate(coreNum, -1) == 1) {
            _InterlockedOr8((vchptr)&generateError, 1);
            break;
         }

         value[2][coreNum] = resultCopy;
      }

      wprintf(L".");
   }

   _InterlockedAnd8(&((chptr)threadBits)[tcfg->threadByte], threadMask);

   return 0; // Returning ends the thread: _beginthreadex's thunk calls _endthreadex with this value
}
