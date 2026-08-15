/************************************************************
 * File: CPU.h                          Created: 2025/01/25 *
 *                                    Last mod.: 2025/02/22 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

#define _CRT_RAND_S

#include <iostream>
#include <string.h> // memcmp, for the byte-exact comparisons in ValidateKernelFamilies
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

al32 struct GLOBAL_CFG { // 96 bytes
   struct _C_D_ { ui32 L1Code, L1Data, L2; };
   struct {
      struct {
         declare2d64z(ui64, coreMap, 2, MAX_THREADS); // Bitmaps of available virtual cores; 0=Non-SMT cores, 1=SMT cores
         _C_D_ cache[2];     // Sizes of L1 code & data and L2 caches; 0=Non-SMT cores, 1=SMT cores
         ui32  cacheL3;      // Size of L3 cache per core complex
         si16  coreCount[2]; // Number of physical cores: 0==Non-SMT, 1==SMT
      };
      si16 vCoreCount = 0;     // Total number of virtual processors
      ui8  groupCount = 0;     // Number of virtual processor groups
      ui8  SMT        = 0;     // Number of virtual cores per physical core
      bool cpuSSE4_1  = false; // CPU supports the SSE 1.0~4.1 instruction sets
      bool cpuAVX2    = false; // CPU supports the AVX 1.0~2.0 instruction sets
      bool cpuAVX512  = false; // CPU supports the AVX512F instruction set
      ///--- 3 bytes unused
   } sys;
   declare1d64z(ui64, coreMap, MAX_THREADS); // Bitmap of available virtual cores
   si64 tics        = 0;        // Global time limit
   si64 allocMem[2] = { 0, 1 }; // Amount(s) of memory allocated for threads
   ui32 onTime      = 100;      // Computation duration (in ms)
   ui32 offTime     = 900;      // Sleep duration (in ms)
   ui32 delayTime   = 2000;     // Start-up delay duration (in ms)
   ui8  procUnits   = 0x04;     // 0==ALU, 1==FPU, 2==SSE4.1, 3==AVX, 4==AVX512, 5==L1 cache, 6==L2 cache, 7==L3 cache
   ui8  procSync    = 0x02A;    // 0==Round-robin, 1==Parallel 2==Staggered, 3==Synchronised, 4==Constant, 5==Fixed pulse, 6==Sweeping pulse, 7==Benchmark
   ui8  SMTLoad     = 0;        // Only utilise the specified virtual core(s) of each active physical core; 0=Unchanged, 1=First, 2=Last, 3=All
   ui8  memConfig   = 0;        // 0=Total equally split, 1=Per core, 2=non-SMT/SMT split

   ~GLOBAL_CFG(void) { mfree(coreMap, sys.coreMap); }
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
}; typedef RESULTS *const RESULTSptrc;

constexpr csi64 RESULTS_BUF_SIZE = sizeof(RESULTS) * MAX_THREADS;

al32 struct RESULTS_ARRAYS { // 96 bytes
   fl64x8ptr avx512;
   fl64x4ptr avx;
   fl64x2ptr sse;
   fl64ptr   fpu;
   si64ptr   alu;
   ptr       p;    // Master pointer
   si64ptr   iter; // Total iterations performed per thread
   ui64 blockSize[2] = { 0, 1 }; // Memory per thread; Non-SMT, SMT
   ui64 records[2]   = { 0, 0 }; // Memory records per thread; Non-SMT, SMT
};

// Global variables
al64 CLASS_TIMER timer;
     GLOBAL_CFG  cfg;

     declare1d64z(THREAD_CFG, threadData, MAX_THREADS);
     declare2d64z(RESULTS, value, 4, MAX_THREADS); // Result values: 0==Input, 1=Processed, 2=Output, 3=Error
     declare1d64z(vui64, threadBits, MAX_THREADS_WORDS);
     declare1d64z(wchar, wstrOut, 1024);
     RESULTS_ARRAYS resArray;

#include "translations.h"

     cwchar   wstrUnitsCPU[8][4]  = { L"ALU", L"FPU", L"SSE", L"AVX", L"512", L"CL1", L"CL2", L"CL3" };
     cwchar   wstrSyncCPU[8][4]   = { L"R-R", L"Par", L"Sta", L"T-S", L"Con", L"F-P", L"S-P", L"Ben" };
     cwchar   wstrPass[2][8]      = { L".Pass.", L"!Fail!" }; ///--- Modify for translation ---///
     wchar    wstrLang[6]         = L"en-GB";
     cwchar   outUTF16header      = 0x0FEFF;
     cchar    outUTF8header[3]    = { char(0x0EF), char(0x0BB), char(0x0BF) };

extern void JobALU(si64&);            extern void JobFPU(fl64&);                          extern void JobALU_FPU(fl64&, si64&);
extern void JobSSE(fl64x2&);          extern void JobALU_SSE(fl64x2&, si64&);
extern void JobAVX2(fl64x4&);         extern void JobALU_AVX2(fl64x4&, si64&);
extern void JobAVX512(fl64x8&);       extern void JobALU_AVX512(fl64x8&, si64&);
extern void JobMemALU(si64ptrc);      extern void JobMemFPU(fl64ptrc);                    extern void JobMemALU_FPU(fl64ptrc, si64ptrc);
extern void JobMemSSE(fl64x2ptrc);    extern void JobMemALU_SSE(fl64x2ptrc, si64ptrc);
extern void JobMemAVX2(fl64x4ptrc);   extern void JobMemALU_AVX2(fl64x4ptrc, si64ptrc);
extern void JobMemAVX512(fl64x8ptrc); extern void JobMemALU_AVX512(fl64x8ptrc, si64ptrc);

//--- Job kernel cross-check ---//
// The eighteen job kernels are written out by hand across four translation units, with no shared
// implementation, and until now nothing required them to agree. "cpu.values" records only what the five
// register-resident kernels produce, so a JobMem* or JobALU_* kernel that had drifted from its counterpart
// was never compared against anything: it was graded against a reference for arithmetic it does not
// perform, and every memory-backed run -- which is to say every preset, every 'B' and every 'M' -- reported
// the difference as silicon at fault (ISSUES.MD B5). 'W' now proves the whole family agrees before it
// writes a file, which is where the invariant is established and the only place it can still be corrected

// Names of the kernels the check walks, in the order it walks them; index 0 is "nothing disagreed".
// These are identifiers rather than prose, so the table is not part of the translated strings
cwchar wstrKernelName[14][20] = {
   L"",
   L"JobALU_FPU",    L"JobMemALU",    L"JobMemFPU",    L"JobMemALU_FPU",
   L"JobALU_SSE",    L"JobMemSSE",    L"JobMemALU_SSE",
   L"JobALU_AVX2",   L"JobMemAVX2",   L"JobMemALU_AVX2",
   L"JobALU_AVX512", L"JobMemAVX512", L"JobMemALU_AVX512"
};

/// Runs one seed through every job kernel the CPU can execute, and requires each memory-array and combined
/// variant to reproduce its register-resident counterpart exactly. Each JobMem* kernel is handed four
/// records carrying the same seed, so all four must come back equal to the single register result -- which
/// checks the record indexing as well as the arithmetic. The comparison is a byte compare: a golden value
/// is a bit pattern, and every floating-point spelling of "equal" this codebase has reached for has at some
/// point compared something other than every bit (ISSUES.MD A1~A3, A11)
/// @return 0 if every kernel agreed; otherwise the wstrKernelName index of the first that did not
static cui8 ValidateKernelFamilies(void) {
   RESULTS seed = {}; // Zeroed first, so every lane stays defined if the seeding below is ever narrowed
   fl64x8  refAVX512, memAVX512[4];
   fl64x4  refAVX2,   memAVX2[4];
   fl64x2  refSSE,    memSSE[4];
   fl64    refFPU,    memFPU[4];
   si64    refALU,    memALU[4];
   fl64x8  regAVX512;
   fl64x4  regAVX2;
   fl64x2  regSSE;
   fl64    regFPU;
   si64    regALU;
   ui8     k;

   // The seed KernelFingerprint probes with: a magnitude the FP chains settle from within two steps, and a
   // plain integer for the ALU lane
   for(k = 0; k < 15; ++k) seed._fl64[k] = fl64(0x0123456789ABCull >> (k & 0x03)) + fl64(k);
   seed.raw[15] = 0x0123456789ABCDEF;

   //--- ALU and FPU: the scalar paths every x64 CPU carries ---//
   refALU = seed.alu;   JobALU(refALU);
   refFPU = seed.fpu;   JobFPU(refFPU);

   regFPU = seed.fpu;   regALU = seed.alu;   JobALU_FPU(regFPU, regALU);
   if(memcmp(&regFPU, &refFPU, sizeof(fl64)) || regALU != refALU) return 1;

   for(k = 0; k < 4; ++k) memALU[k] = seed.alu;
   JobMemALU(memALU);
   for(k = 0; k < 4; ++k) if(memALU[k] != refALU) return 2;

   for(k = 0; k < 4; ++k) memFPU[k] = seed.fpu;
   JobMemFPU(memFPU);
   for(k = 0; k < 4; ++k) if(memcmp(&memFPU[k], &refFPU, sizeof(fl64))) return 3;

   for(k = 0; k < 4; ++k) { memFPU[k] = seed.fpu;   memALU[k] = seed.alu; }
   JobMemALU_FPU(memFPU, memALU);
   for(k = 0; k < 4; ++k) if(memcmp(&memFPU[k], &refFPU, sizeof(fl64)) || memALU[k] != refALU) return 4;

   //--- SSE: reached on every CPU, being the golden ladder's fallback ---//
   refSSE = seed.sse;   JobSSE(refSSE);

   regSSE = seed.sse;   regALU = seed.alu;   JobALU_SSE(regSSE, regALU);
   if(memcmp(&regSSE, &refSSE, sizeof(fl64x2)) || regALU != refALU) return 5;

   for(k = 0; k < 4; ++k) memSSE[k] = seed.sse;
   JobMemSSE(memSSE);
   for(k = 0; k < 4; ++k) if(memcmp(&memSSE[k], &refSSE, sizeof(fl64x2))) return 6;

   for(k = 0; k < 4; ++k) { memSSE[k] = seed.sse;   memALU[k] = seed.alu; }
   JobMemALU_SSE(memSSE, memALU);
   for(k = 0; k < 4; ++k) if(memcmp(&memSSE[k], &refSSE, sizeof(fl64x2)) || memALU[k] != refALU) return 7;

   //--- AVX2 and AVX-512: gated exactly as RunGoldenLadder gates them ---//
   if(cfg.sys.cpuAVX2) {
      refAVX2 = seed.avx;   JobAVX2(refAVX2);

      regAVX2 = seed.avx;   regALU = seed.alu;   JobALU_AVX2(regAVX2, regALU);
      if(memcmp(&regAVX2, &refAVX2, sizeof(fl64x4)) || regALU != refALU) return 8;

      for(k = 0; k < 4; ++k) memAVX2[k] = seed.avx;
      JobMemAVX2(memAVX2);
      for(k = 0; k < 4; ++k) if(memcmp(&memAVX2[k], &refAVX2, sizeof(fl64x4))) return 9;

      for(k = 0; k < 4; ++k) { memAVX2[k] = seed.avx;   memALU[k] = seed.alu; }
      JobMemALU_AVX2(memAVX2, memALU);
      for(k = 0; k < 4; ++k) if(memcmp(&memAVX2[k], &refAVX2, sizeof(fl64x4)) || memALU[k] != refALU) return 10;
   }

   if(cfg.sys.cpuAVX512) {
      refAVX512 = seed.avx512;   JobAVX512(refAVX512);

      regAVX512 = seed.avx512;   regALU = seed.alu;   JobALU_AVX512(regAVX512, regALU);
      if(memcmp(&regAVX512, &refAVX512, sizeof(fl64x8)) || regALU != refALU) return 11;

      for(k = 0; k < 4; ++k) memAVX512[k] = seed.avx512;
      JobMemAVX512(memAVX512);
      for(k = 0; k < 4; ++k) if(memcmp(&memAVX512[k], &refAVX512, sizeof(fl64x8))) return 12;

      for(k = 0; k < 4; ++k) { memAVX512[k] = seed.avx512;   memALU[k] = seed.alu; }
      JobMemALU_AVX512(memAVX512, memALU);
      for(k = 0; k < 4; ++k) if(memcmp(&memAVX512[k], &refAVX512, sizeof(fl64x8)) || memALU[k] != refALU) return 13;
   }

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
/// on another of a different vector width. The generating half of GenerateValues, its self-checking half
/// and the fingerprint stored in the file's header must all walk the same ladder, so there is only one
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

///--- Expand beyond 512 cores ---///
// threadBits is an array of ui64, so a 512-bit view of it spans 8 elements, a 256-bit view 4 and a 128-bit
// view 2. Advancing by one element per vector step re-read bits already examined and left the tail of the
// map unexamined altogether -- bytes 48~63 for the AVX2 poll, 40~63 for the SSE one -- and bound vector
// references to addresses their alignment does not permit. The loads are unaligned forms because the poll
// must stay well-defined for any future change to the map's size or alignment; the addresses below are all
// naturally aligned today, so no instruction is added on any current CPU
inline bool ThreadsRunningAVX512(void) {
   return !AllFalse((si512&)threadBits[0], max512);
}
inline bool ThreadsRunningAVX(void) {
   return !(AllFalse(_mm256_loadu_si256((cui256ptr)&threadBits[0]), max256) && AllFalse(_mm256_loadu_si256((cui256ptr)&threadBits[4]), max256));
}
inline bool ThreadsRunningSSE(void) {
   return !(AllFalse(_mm_loadu_si128((cui128ptr)&threadBits[0]), max128) && AllFalse(_mm_loadu_si128((cui128ptr)&threadBits[2]), max128) &&
            AllFalse(_mm_loadu_si128((cui128ptr)&threadBits[4]), max128) && AllFalse(_mm_loadu_si128((cui128ptr)&threadBits[6]), max128));
}
///--- Expand beyond 512 cores ---///
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

// Force 1 thread per SMT core
static void SetSMTLoading(void) {
   ui8 i = 0, j, k;

   if(!cfg.SMTLoad) return;

   switch(cfg.SMTLoad) {
   case 3: // Use all virtual cores
      for(ui8 i = 0; i < cfg.sys.groupCount; ++i)
         for(j = 0; j < cfg.sys.SMT; ++j)
            cfg.coreMap[i] |= (cfg.coreMap[i] << j) & cfg.sys.coreMap[1][i];
      break;
   default: // Use one virtual core
      ui64  mask;
      cui64 shift = cfg.SMTLoad == 2 ? ui64(cfg.sys.SMT - 1) : 0;
      cui64 mask0 = mask = 0x01ull << shift;
      while(i < cfg.sys.groupCount) {
         for(j = 1; j < cfg.sys.coreCount[1]; ++j) mask = (mask << cfg.sys.SMT) + mask0;
         for(k = 0; k < 64 && cfg.sys.coreMap[1][i] >> k == 0; ++k);
         cfg.coreMap[i++] &= (mask <<= k);
      }
   }
}

// Print computational failure data
static void Failed(cui64 coreNum, vchptrc threadByte, cui8 unit) {
   // threadByte addresses the byte holding eight threads' completion bits, so zeroing it told wmain that all
   // eight had finished: its wait loop could then return while up to seven of them were still writing
   // value[3] and resArray.iter, and read the results table out from under them. coreNum is
   // (threadByte << 3) + threadBit, so the failing thread's bit within that byte is coreNum & 0x07
   cui8 threadMask = ui8(~(1u << (coreNum & 0x07)));

   wprintf(wstrInterface[11], coreNum);
   switch(unit) {
   case 0:
      wprintf(L"%1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  %s %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n",
         value[2][coreNum].avx512.m512d_f64[0], value[2][coreNum].avx512.m512d_f64[1], value[2][coreNum].avx512.m512d_f64[2], value[2][coreNum].avx512.m512d_f64[3],
         value[2][coreNum].avx512.m512d_f64[4], value[2][coreNum].avx512.m512d_f64[5], value[2][coreNum].avx512.m512d_f64[6], value[2][coreNum].avx512.m512d_f64[7], wstrInterface[12],
         value[1][coreNum].avx512.m512d_f64[0], value[1][coreNum].avx512.m512d_f64[1], value[1][coreNum].avx512.m512d_f64[2], value[1][coreNum].avx512.m512d_f64[3],
         value[1][coreNum].avx512.m512d_f64[4], value[1][coreNum].avx512.m512d_f64[5], value[1][coreNum].avx512.m512d_f64[6], value[1][coreNum].avx512.m512d_f64[7]);
      break;
   case 1:
      wprintf(L"%1.9f, %1.9f, %1.9f, %1.9f  %s %1.9f, %1.9f, %1.9f, %1.9f\n",
         value[2][coreNum].avx.m256d_f64[0], value[2][coreNum].avx.m256d_f64[1], value[2][coreNum].avx.m256d_f64[2], value[2][coreNum].avx.m256d_f64[3], wstrInterface[12],
         value[1][coreNum].avx.m256d_f64[0], value[1][coreNum].avx.m256d_f64[1], value[1][coreNum].avx.m256d_f64[2], value[1][coreNum].avx.m256d_f64[3]);
      break;
   case 2:
      wprintf(L"%1.9f, %1.9f  %s %1.9f, %1.9f\n",
         value[2][coreNum].sse.m128d_f64[0], value[2][coreNum].sse.m128d_f64[1], wstrInterface[12], value[1][coreNum].sse.m128d_f64[0], value[1][coreNum].sse.m128d_f64[1]);
      break;
   case 3:
      wprintf(L"%1.9f  %s %1.9f\n", value[2][coreNum].fpu, wstrInterface[12], value[1][coreNum].fpu);
      break;
   case 4:
      wprintf(L"%lld  %s %lld\n", value[2][coreNum].alu, wstrInterface[12], value[3][coreNum].alu);
   }
   _InterlockedAnd8(threadByte, threadMask);
   return;
}

///--- Add vector versions ---///
// Evaluate integrity of results.
// unit==Processing unit (0=AVX512, 1=AVX2, 2=SSE4.1, 3=FPU, 4=ALU, -1=All)
static inline cui8 Evaluate(csi16 thread, csi8 unit) {
   ui8  index = unit == -1 ? 0  : 16 - (1 << (4 - unit));
   cui8 end   = unit == -1 ? 16 : (1 << max(0, 3 - unit)) + index;

   while(index < end)
      if(value[2][thread].raw[index] != value[3][thread].raw[index++])
         return 1;

   //return index >= end ? 0 : 1;
   return 0;
}

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
   ui16  i          = 0;
   cui8  threadMask = ui8(~(1u << tcfg->threadBit)); // Clears this thread's completion bit, preserving the other 7

   for(resultCopy = value[2][coreNum]; coreNum < range; ++coreNum) {
      value[2][coreNum] = value[3][coreNum];

      RunGoldenLadder(value[3][coreNum]);

      // Test computatational integrity. The self-check has to walk the same ladder as the generation above,
      // lane for lane, or it grades every entry against a different function; one shared ladder is what
      // makes that structural rather than a pair of blocks that have to be kept identical by hand
      for(resultCopy = value[2][coreNum]; i < 65535; ++i) {
         RunGoldenLadder(value[2][coreNum]);

         if(Evaluate(coreNum, -1) == 1) {
            //if(!coreNum) {
            value[3][0].raw[0] = 0x05555555555555555;
            value[2][0].raw[0] = 0x0AAAAAAAAAAAAAAAA;
            //}
            break;
         }

         value[2][coreNum] = resultCopy;
      }

      printf(".");
   }

   _InterlockedAnd8(&((chptr)threadBits)[tcfg->threadByte], threadMask);

   return 0; // Returning ends the thread: _beginthreadex's thunk calls _endthreadex with this value
}
