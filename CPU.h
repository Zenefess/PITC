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
#include <windows.h>
#include <timeapi.h>
#include <process.h>
#include <memory management.h>
#include <class_timers.h>

constexpr auto MAX_THREADS       = 512;
constexpr auto MAX_THREADS_BYTES = (MAX_THREADS + 7) >> 3;

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
   ui8  procSync    = 0x02A;    // 8==Round-robin, 9==Parallel 10==Staggered, 11==Synchronised, 12==Constant, 13==Fixed pulse, 14==Sweeping pulse, 15==Benchmark
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
     declare1d64z(vui64, threadBits, MAX_THREADS_BYTES);
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

///--- Expand beyond 512 cores ---///
inline bool ThreadsRunningAVX512(void) {
   return !AllFalse((si512&)threadBits[0], max512);
}
inline bool ThreadsRunningAVX(void) {
   return !(AllFalse((si256&)threadBits[0], max256) && AllFalse((si256&)threadBits[2], max256));
}
inline bool ThreadsRunningSSE(void) {
   return !(AllFalse((ui128&)threadBits[0], max128) && AllFalse((ui128&)threadBits[1], max128) && AllFalse((ui128&)threadBits[2], max128) && AllFalse((ui128&)threadBits[3], max128));
}
///--- Expand beyond 512 cores ---///

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
   _InterlockedAnd8(threadByte, 0x0);
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

// Generates output values
static void GenerateValues(ptr dataPtr) {
   RESULTS         resultCopy;
   cTHREAD_CFGptrc tcfg = (THREAD_CFGptrc)dataPtr;

   cui32 entries[2] = { MAX_THREADS / (cui32)cfg.sys.vCoreCount, MAX_THREADS % (cui32)cfg.sys.vCoreCount };

   si16  coreNum   = ((cui64(tcfg->threadByte) << 3) + tcfg->threadBit);
         coreNum   = coreNum * entries[0] + (coreNum ? 0 : entries[1]);
   csi16 range     = coreNum + entries[0];
   ui16  i         = 0;
   cui8  threadBit = 1u << tcfg->threadBit;

   for(resultCopy = value[2][coreNum]; coreNum < range; ++coreNum) {
      value[2][coreNum] = value[3][coreNum];

      if(cfg.sys.cpuAVX512) {
         JobAVX512(value[3][coreNum].avx512);
         JobAVX2(value[3][coreNum].avx);
      } else if(cfg.sys.cpuAVX2) {
         JobAVX2((fl64x4&)value[3][coreNum]._fl64[0]);
         JobAVX2((fl64x4&)value[3][coreNum]._fl64[4]);
         JobAVX2(value[3][coreNum].avx);
      } else {
         JobSSE((fl64x2&)value[3][coreNum]._fl64[0]);
         JobSSE((fl64x2&)value[3][coreNum]._fl64[2]);
         JobSSE((fl64x2&)value[3][coreNum]._fl64[4]);
         JobSSE((fl64x2&)value[3][coreNum]._fl64[8]);
         JobSSE((fl64x2&)value[3][coreNum]._fl64[10]);
         JobSSE((fl64x2&)value[3][coreNum]._fl64[12]);
      }
      JobSSE(value[3][coreNum].sse);
      JobFPU(value[3][coreNum].fpu);
      JobALU(value[3][coreNum].alu);

      // Test computatational integrity
      for(resultCopy = value[2][coreNum]; i < 65535; ++i) {
         if(cfg.sys.cpuAVX512) {
            JobAVX512(value[2][coreNum].avx512);
            JobAVX2(value[2][coreNum].avx);
         } else if(cfg.sys.cpuAVX2) {
            JobAVX2((fl64x4&)value[2][coreNum]._fl64[0]);
            JobAVX2((fl64x4&)value[2][coreNum]._fl64[4]);
            JobAVX2(value[2][coreNum].avx);
         } else {
            JobSSE((fl64x2&)value[2][coreNum]._fl64[0]);
            JobSSE((fl64x2&)value[2][coreNum]._fl64[2]);
            JobSSE((fl64x2&)value[2][coreNum]._fl64[4]);
            JobSSE((fl64x2&)value[2][coreNum]._fl64[8]);
            JobSSE((fl64x2&)value[2][coreNum]._fl64[8]);
            JobSSE((fl64x2&)value[2][coreNum]._fl64[10]);
         }
         JobSSE(value[2][coreNum].sse);
         JobFPU(value[2][coreNum].fpu);
         JobALU(value[2][coreNum].alu);

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

   _InterlockedAnd8(&((chptr)threadBits)[tcfg->threadByte], threadBit);

   _endthread();
}
