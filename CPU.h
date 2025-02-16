/************************************************************
 * File: CPU.h                          Created: 2025/01/25 *
 *                                    Last mod.: 2025/02/16 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

#define _CRT_RAND_S

#include <iostream>
#include <process.h>
#include <memory management.h>
#include <class_timers.h>
#include "instructions.h"

constexpr auto MAX_THREADS       = 512;
constexpr auto MAX_THREADS_BYTES = (MAX_THREADS + 7) >> 3;

al16 struct THREAD_CFG {
   si64 packetSizeRAM; // Amount of RAM (in bytes) to utilise per pulse
   si64 startTime;
   si64 endTime;
   si64 maxDuration;
   ui32 activeTime;    // Computation duration (in ms)
   ui32 inactiveTime;  // Sleep duration (in ms)
   ui32 recordCount;   // Total number of memory-bound records to process
   union {
      ui32 flags;
      struct {
         ui8 procUnits;  // 0==ALU, 1==FPU, 2==SSE4.1, 3==AVX, 4==AVX512, 5==L1 cache, 6==L2 cache, 7==L3 cache
         ui8 procSync;   // 0==Round-robin, 1==Parallel, 2==Staggered, 3==Synchronised, 4==Constant, 5==Fixed pulse, 6==Sweeping pulse, 7==???
         ui8 threadByte;
         ui8 threadBit;
      };
   };
}; typedef THREAD_CFG *const THREAD_CFGptrc; typedef const THREAD_CFG *const cTHREAD_CFGptrc;

struct GLOBAL_CFG {
   struct _C_D_ { ui32 L1Code, L1Data, L2; };
   struct _SYS_ {
      struct {
         declare2d64z(ui64, coreMap, 2, MAX_THREADS); // Bitmaps of available virtual cores; 0=Non-SMT cores, 1=SMT cores
         _C_D_ cache[2];     // Sizes of L1 code & data and L2 caches; 0=Non-SMT cores, 1=SMT cores
         ui32  cacheL3;      // Size of L3 cache per core complex
         si16  coreCount[2]; // Number of physical cores: 0==Non-SMT, 1==SMT
      };
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
   ui8  procSync    = 0x02A;    // 8==Round-robin, 9==Parallel 10==Staggered, 11==Synchronised, 12==Constant, 13==Fixed pulse, 14==Sweeping pulse, 15==???
   ui8  SMTLoad     = 0;        // Only utilise the specified virtual core(s) of each active physical core; 0=Unchanged, 1=First, 2=Last, 3=All
   ui8  memConfig   = 0;        // 0=Total equally split, 1=Per core, 2=non-SMT/SMT split

   ~GLOBAL_CFG(void) { mfree(coreMap, sys.coreMap); }
}; typedef GLOBAL_CFG *const GLOBAL_CFGptrc;

al64 union RESULTS {
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

struct RESULTS_ARRAYS {
   fl64x8ptr avx512;
   fl64x4ptr avx;
   fl64x2ptr sse;
   fl64ptr   fpu;
   si64ptr   alu[2];                  // Non-SMT, SMT
   ptr       p;                       // Master pointer
   ui64      blockSize[2] = { 0, 1 }; // Memory per thread; Non-SMT, SMT
   ui32      records[2]   = { 0, 0 }; // Memory records per thread; Non-SMT, SMT
};

constexpr csi64 RESULTS_BUF_SIZE = sizeof(RESULTS) * MAX_THREADS;

// Global variables
al64 CLASS_TIMER timer;
     GLOBAL_CFG  cfg;

     declare1d64z(THREAD_CFG, threadData, MAX_THREADS);
     declare2d64z(RESULTS, value, 4, MAX_THREADS); // Result values: 0==Input, 1=Processed, 2=Output, 3=Error
     declare1d64z(vui64, threadBits, MAX_THREADS_BYTES);
     declare1d64z(wchar, wstrOut, 1024);
     RESULTS_ARRAYS memArray;

     cwchptrc wstrInstructions[8] = { wstrInstructions_EN_UK, 0, 0, 0, 0, 0, 0, 0 };
     cwchar   wstrUnitsCPU[8][4]  = { L"ALU", L"FPU", L"SSE", L"AVX", L"512", L"CL1", L"CL2", L"CL3" };
     cwchar   wstrSyncCPU[8][4]   = { L"R-R", L"Par", L"Sta", L"T-S", L"Con", L"F-P", L"S-P", L"..." };
     cwchar   wstrPass[2][8]      = { L".Pass.", L"!Fail!" };
     cchar    strPass[2][8]       = { ".Pass.", "!Fail!" };
     cwchar   outUTF16header      = 0x0FEFF;
     cchar    outUTF8header[3]    = { char(0x0EF), char(0x0BB), char(0x0BF) };
     wchar    wstrLang[4]         = L"ENG";

#ifdef __AVX__
#define ThreadsRunning !(AllFalse((si256&)threadBits[0], Max256) & AllFalse((si256&)threadBits[2]), max256)
#else
#define ThreadsRunning !(AllFalse((ui128&)threadBits[0], max128) & AllFalse((ui128&)threadBits[1], max128) & AllFalse((ui128&)threadBits[2], max128) & AllFalse((ui128&)threadBits[3], max128))
#endif

extern void JobALU(si64&);            extern void JobFPU(fl64&);                          extern void JobALU_FPU(fl64&, si64&);
extern void JobSSE(fl64x2&);          extern void JobALU_SSE(fl64x2&, si64&);
extern void JobAVX2(fl64x4&);         extern void JobALU_AVX2(fl64x4&, si64&);
extern void JobAVX512(fl64x8&);       extern void JobALU_AVX512(fl64x8&, si64&);
extern void JobMemALU(si64ptrc);      extern void JobMemFPU(fl64ptrc);                    extern void JobMemALU_FPU(fl64ptrc, si64ptrc);
extern void JobMemSSE(fl64x2ptrc);    extern void JobMemALU_SSE(fl64x2ptrc, si64ptrc);
extern void JobMemAVX2(fl64x4ptrc);   extern void JobMemALU_AVX2(fl64x4ptrc, si64ptrc);
extern void JobMemAVX512(fl64x8ptrc); extern void JobMemALU_AVX512(fl64x8ptrc, si64ptrc);

// Print computational failure data
static void Failed(cui64 coreNum, vchptrc threadByte, cui8 unit) {
   printf("ERROR! Core: %2.1lld  Expected: ", coreNum);
   switch(unit) {
   case 0:
      printf("%1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n",
         value[2][coreNum].avx512.m512d_f64[0], value[2][coreNum].avx512.m512d_f64[1], value[2][coreNum].avx512.m512d_f64[2], value[2][coreNum].avx512.m512d_f64[3],
         value[2][coreNum].avx512.m512d_f64[4], value[2][coreNum].avx512.m512d_f64[5], value[2][coreNum].avx512.m512d_f64[6], value[2][coreNum].avx512.m512d_f64[7],
         value[1][coreNum].avx512.m512d_f64[0], value[1][coreNum].avx512.m512d_f64[1], value[1][coreNum].avx512.m512d_f64[2], value[1][coreNum].avx512.m512d_f64[3],
         value[1][coreNum].avx512.m512d_f64[4], value[1][coreNum].avx512.m512d_f64[5], value[1][coreNum].avx512.m512d_f64[6], value[1][coreNum].avx512.m512d_f64[7]);
      break;
   case 1:
      printf("%1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f\n",
         value[2][coreNum].avx.m256d_f64[0], value[2][coreNum].avx.m256d_f64[1], value[2][coreNum].avx.m256d_f64[2], value[2][coreNum].avx.m256d_f64[3],
         value[1][coreNum].avx.m256d_f64[0], value[1][coreNum].avx.m256d_f64[1], value[1][coreNum].avx.m256d_f64[2], value[1][coreNum].avx.m256d_f64[3]);
      break;
   case 2:
      printf("%1.9f, %1.9f  Output: %1.9f, %1.9f\n",
         value[2][coreNum].sse.m128d_f64[0], value[2][coreNum].sse.m128d_f64[1], value[1][coreNum].sse.m128d_f64[0], value[1][coreNum].sse.m128d_f64[1]);
      break;
   case 3:
      printf("%1.9f  Output: %1.9f\n", value[2][coreNum].fpu, value[1][coreNum].fpu);
      break;
   case 4:
      printf("%lld  Output: %lld\n", value[2][coreNum].alu, value[3][coreNum].alu);
   }
   _InterlockedAnd8(threadByte, 0x0);
   return;
}

static cui8 JobCycleALU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].alu = value[0][coreNum].alu;
   JobALU(value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemALU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   ui8 i;
   for(i = 0; i < 4; ++i)
      value[1][coreNum].p4[i + offset] = value[0][coreNum].alu;
   JobMemALU(&value[1][coreNum].p4[offset]);
   for(i = 0; i < 4; ++i)
      if(value[1][coreNum].p4[i + offset] != value[2][coreNum].alu) {
         value[3][coreNum].alu = value[1][coreNum].p4[i + offset];
         Failed(coreNum, threadByte, 4);
         return 1;
      }
   return 0;
}

static cui8 JobCycleFPU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].fpu = value[0][coreNum].fpu;
   JobFPU(value[1][coreNum].fpu);
   if(value[1][coreNum].fpu != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].fpu;
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemFPU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   ui8 i;
   for(i = 0; i < 4; ++i)
      value[1][coreNum].p3[i + offset] = value[0][coreNum].fpu;
   JobMemFPU(&value[1][coreNum].p3[offset]);
   for(i = 0; i < 4; ++i)
      if(value[1][coreNum].p3[i + offset] != value[2][coreNum].fpu) {
         value[3][coreNum].fpu = value[1][coreNum].p3[i + offset];
         Failed(coreNum, threadByte, 3);
         return 1;
      }
   return 0;
}

static cui8 JobCycleSSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].sse = value[0][coreNum].sse;
   JobSSE(value[1][coreNum].sse);
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].sse;
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemSSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   ui8 i;
   for(i = 0; i < 4; ++i)
      value[1][coreNum].p2[i + offset] = value[0][coreNum].sse;
   JobMemSSE(&value[1][coreNum].p2[offset]);
   for(i = 0; i < 4; ++i)
      if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[i + offset], (__m128i&)value[2][coreNum].sse)) {
         value[3][coreNum].sse = value[1][coreNum].p2[i + offset];
         Failed(coreNum, threadByte, 2);
         return 1;
      }
   return 0;
}

static cui8 JobCycleAVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx = value[0][coreNum].avx;
   JobAVX2(value[1][coreNum].avx);
   if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].avx;
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemAVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   ui8 i;
   for(i = 0; i < 4; ++i)
      value[1][coreNum].p1[i + offset] = value[0][coreNum].avx;
   JobMemAVX2(&value[1][coreNum].p1[offset]);
   for(i = 0; i < 4; ++i)
      if(!_mm256_testc_pd(value[1][coreNum].p1[i + offset], value[2][coreNum].avx)) {
         value[3][coreNum].avx = value[1][coreNum].p1[i + offset];
         Failed(coreNum, threadByte, 1);
         return 1;
      }
   return 0;
}

static cui8 JobCycleAVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx512 = value[0][coreNum].avx512;
   JobAVX512(value[1][coreNum].avx512);
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].avx512;
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemAVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   ui8 i;
   for(i = 0; i < 4; ++i)
      value[1][coreNum].p0[i + offset] = value[0][coreNum].avx512;
   JobMemAVX512(&value[1][coreNum].p0[offset]);
   for(i = 0; i < 4; ++i)
      if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
         value[3][coreNum].avx512 = value[1][coreNum].p0[i + offset];
         Failed(coreNum, threadByte, 0);
         return 1;
      }
   return 0;
}

static cui8 JobCycleALU_FPU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].fpu = value[0][coreNum].fpu;
   value[1][coreNum].alu = value[0][coreNum].alu;
   JobALU_FPU(value[1][coreNum].fpu, value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(value[1][coreNum].fpu != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].fpu;
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemALU_FPU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   ui8 i;
   for(i = 0; i < 4; ++i) {
      value[1][coreNum].p3[i + offset] = value[0][coreNum].fpu;
      value[1][coreNum].p4[i + offset] = value[0][coreNum].alu;
   }
   JobMemALU_FPU(&value[1][coreNum].p3[offset], &value[1][coreNum].p4[offset]);
   for(i = 0; i < 4; ++i) {
      if(value[1][coreNum].p3[i + offset] != value[2][coreNum].fpu) {
         value[3][coreNum].fpu = value[1][coreNum].p3[i + offset];
         Failed(coreNum, threadByte, 3);
         return 1;
      }
      if(value[1][coreNum].p4[i + offset] != value[2][coreNum].alu) {
         value[3][coreNum].alu = value[1][coreNum].p4[i + offset];
         Failed(coreNum, threadByte, 4);
         return 1;
      }
   }
   return 0;
}

static cui8 JobCycleALU_SSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].sse = value[0][coreNum].sse;
   value[1][coreNum].alu = value[0][coreNum].alu;
   JobALU_SSE(value[1][coreNum].sse, value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].sse;
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemALU_SSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   ui8 i;
   for(i = 0; i < 4; ++i) {
      value[1][coreNum].p2[i + offset] = value[0][coreNum].sse;
      value[1][coreNum].p4[i + offset] = value[0][coreNum].alu;
   }
   JobMemALU_SSE(&value[1][coreNum].p2[offset], &value[1][coreNum].p4[offset]);
   for(i = 0; i < 4; ++i) {
      if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[i + offset], (__m128i&)value[2][coreNum].sse)) {
         value[3][coreNum].sse = value[1][coreNum].p2[i + offset];
         Failed(coreNum, threadByte, 2);
         return 1;
      }
      if(value[1][coreNum].p4[i + offset] != value[2][coreNum].alu) {
         value[3][coreNum].alu = value[1][coreNum].p4[i + offset];
         Failed(coreNum, threadByte, 4);
         return 1;
      }
   }
   return 0;
}

static cui8 JobCycleALU_AVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx = value[0][coreNum].avx;
   value[1][coreNum].alu = value[0][coreNum].alu;
   JobALU_AVX2(value[1][coreNum].avx, value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].avx;
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemALU_AVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   ui8 i;
   for(i = 0; i < 4; ++i) {
      value[1][coreNum].p1[i + offset] = value[0][coreNum].avx;
      value[1][coreNum].p4[i + offset] = value[0][coreNum].alu;
   }
   JobMemALU_AVX2(&value[1][coreNum].p1[offset], &value[1][coreNum].p4[offset]);
   for(i = 0; i < 4; ++i) {
      if(!_mm256_testc_pd(value[1][coreNum].p1[i + offset], value[2][coreNum].avx)) {
         value[3][coreNum].avx = value[1][coreNum].p1[i + offset];
         Failed(coreNum, threadByte, 1);
         return 1;
      }
      if(value[1][coreNum].p4[i + offset] != value[2][coreNum].alu) {
         value[3][coreNum].alu = value[1][coreNum].p4[i + offset];
         Failed(coreNum, threadByte, 4);
         return 1;
      }
   }
   return 0;
}

static cui8 JobCycleALU_AVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx512 = value[0][coreNum].avx512;
   value[1][coreNum].alu    = value[0][coreNum].alu;
   JobALU_AVX512(value[1][coreNum].avx512, value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].avx512;
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemALU_AVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   ui8 i;
   for(i = 0; i < 4; ++i) {
      value[1][coreNum].p0[i + offset] = value[0][coreNum].avx512;
      value[1][coreNum].p4[i + offset] = value[0][coreNum].alu;
   }
   JobMemALU_AVX512(&value[1][coreNum].p0[offset], &value[1][coreNum].p4[offset]);
   for(i = 0; i < 4; ++i) {
      if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].p0[i + offset], value[2][coreNum].avx512)) {
         value[3][coreNum].avx512 = value[1][coreNum].p0[i + offset];
         Failed(coreNum, threadByte, 0);
         return 1;
      }
      if(value[1][coreNum].p4[i + offset] != value[2][coreNum].alu) {
         value[3][coreNum].alu = value[1][coreNum].p4[i + offset];
         Failed(coreNum, threadByte, 4);
         return 1;
      }
   }
   return 0;
}

// Job cycle functions array. [0][]==Without memory, [1][]==With memory
al64 static cui8(*JobCycle[2][24])(cui64 coreNum, csi64 offset, vchptrc threadByte) = {
 { JobCycleALU,       JobCycleALU,           JobCycleFPU,       JobCycleALU_FPU,       JobCycleSSE,       JobCycleALU_SSE,       JobCycleSSE,       JobCycleALU_SSE,
   JobCycleAVX2,      JobCycleALU_AVX2,      JobCycleAVX2,      JobCycleALU_AVX2,      JobCycleAVX2,      JobCycleALU_AVX2,      JobCycleAVX2,      JobCycleALU_AVX2,
   JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512, },
 { JobCycleMemALU,    JobCycleMemALU,        JobCycleMemFPU,    JobCycleMemALU_FPU,    JobCycleMemSSE,    JobCycleMemALU_SSE,    JobCycleMemSSE,    JobCycleMemALU_SSE,
   JobCycleMemAVX2,   JobCycleMemALU_AVX2,   JobCycleMemAVX2,   JobCycleMemALU_AVX2,   JobCycleMemAVX2,   JobCycleMemALU_AVX2,   JobCycleMemAVX2,   JobCycleMemALU_AVX2,
   JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512 }
};

// Constant computation
void Constant(ptrc dataPtr) {
   cTHREAD_CFGptrc tcfg = (THREAD_CFG*)dataPtr;

   vchptrc threadByte = &((chptr)threadBits)[tcfg->threadByte];
   cui64   coreNum    = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   si64    i;
   ui32    j;

   // Wait for start time
   do {
      Sleep(1);
      timer.Update();
   } while(tcfg->startTime > timer.siCurrentTics);

   // Main loop
   for(j = 0, i = 0; timer.siCurrentTics < tcfg->endTime; ++j, i = i >= tcfg->recordCount - 4 ? 0 : i + 4) {
      if(JobCycle[tcfg->recordCount ? 1 : 0][tcfg->procUnits](coreNum, i, threadByte)) break;

      if(j >= 131072) { printf("."); j = 0; }

      timer.Update();
   }

   _InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));

   _endthread();
}

// Alternates between fixed-length compute & sleep cycles without synchronised threads
void FixedNoSync(ptrc dataPtr) {
   cTHREAD_CFGptrc tcfg = (THREAD_CFG *)dataPtr;

   vchptrc threadByte = &((chptr)threadBits)[tcfg->threadByte];
   cui64   coreNum    = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   csi64   cycleTics  = timer.siFrequency * si64(tcfg->activeTime + tcfg->inactiveTime) / 1000;
   cui32   sleepDelay = tcfg->inactiveTime;
   si64    nextTic    = tcfg->startTime + (timer.siFrequency * si64(tcfg->activeTime) / 1000);

   // Wait for start time
   do {
      Sleep(1);
      timer.Update();
   } while(tcfg->startTime > timer.siCurrentTics);

   // Main loop
   while(timer.siCurrentTics < tcfg->endTime) {
      if(timer.siCurrentTics < nextTic) {
         if(JobCycle[tcfg->recordCount ? 1 : 0][tcfg->procUnits](coreNum, 0, threadByte)) break;

         timer.Update();
      } else {
         printf(".");
         Sleep(sleepDelay + (rand() & 0x03F));
         timer.Update();
         nextTic += cycleTics + (rand() & 0x0FFFF);
      }
   }

   _InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));

   _endthread();
}

// Alternates between fixed-length compute & sleep cycles with synchronised threads
void FixedSync(ptrc dataPtr) {
   cTHREAD_CFGptrc tcfg = (THREAD_CFG *)dataPtr;

   vchptrc threadByte = &((chptr)threadBits)[tcfg->threadByte];
   cui64   coreNum    = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   csi64   cycleTics  = timer.siFrequency * si64(tcfg->activeTime + tcfg->inactiveTime) / 1000;
   cui32   sleepDelay = tcfg->inactiveTime;
   si64    nextTic    = tcfg->startTime + (timer.siFrequency * si64(tcfg->activeTime) / 1000);
   cDWORD  offset[2]  = { tcfg->procSync & 0x08 ? 0 : cDWORD(rand() & 0x03F), tcfg->procSync & 0x08 ? 0 : cDWORD(rand() & 0x0FFFF) };

   // Configure thread

   // Wait for start time
   do {
      Sleep(1);
      timer.Update();
   } while(tcfg->startTime > timer.siCurrentTics);

   // Main loop
   while(timer.siCurrentTics < tcfg->endTime) {
      if(timer.siCurrentTics < nextTic) {
         if(JobCycle[tcfg->recordCount ? 1 : 0][tcfg->procUnits](coreNum, 0, threadByte)) break;

         timer.Update();
      } else {
         printf(".");
         Sleep(sleepDelay + offset[0]);
         timer.Update();
         nextTic += cycleTics + offset[1];
      }
   }

   _InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));

   _endthread();
}

// !!!
void SweepNoSync(ptrc dataPtr) {
   cTHREAD_CFGptrc tcfg = (THREAD_CFG *)dataPtr;

   vchptrc threadByte = &((chptr)threadBits)[tcfg->threadByte];
   cui64   coreNum    = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   csi64   cycleMs    = si64(tcfg->activeTime); // tcfg->activeTime serves as total cycle period in ms
   csi64   cycleTics  = cycleMs * timer.siFrequency / 1000;
   si64    nextTic    = tcfg->startTime + (timer.siFrequency * si64(tcfg->activeTime) / 1000);

   // Wait for start time
   do {
      Sleep(1);
      timer.Update();
   } while(tcfg->startTime > timer.siCurrentTics);

   // Main loop
   do {
      if(timer.siCurrentTics < nextTic) {
         if(JobCycle[tcfg->recordCount ? 1 : 0][tcfg->procUnits](coreNum, 0, threadByte)) break;

         timer.Update();
      } else {
         printf(".");
         Sleep(DWORD(cycleMs - ((timer.siCurrentTics - tcfg->startTime) * cycleMs / (tcfg->endTime - tcfg->startTime)) + (rand() & 0x03F)));
         timer.Update();
         nextTic += cycleTics + (rand() & 0x0FFFF);
      }
   } while(timer.siCurrentTics < tcfg->endTime);

   _InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));

   _endthread();
}

// Alternates between variable-length compute & sleep cycles with synchronised threads
void SweepSync(ptrc dataPtr) {
   cTHREAD_CFGptrc tcfg = (THREAD_CFG*)dataPtr;

   vchptrc threadByte = &((chptr)threadBits)[tcfg->threadByte];
   cui64   coreNum    = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   csi64   cycleMs    = si64(tcfg->activeTime); // tcfg->activeTime serves as total cycle period in ms
   csi64   cycleTics  = cycleMs * timer.siFrequency / 1000;
   si64    nextTic    = tcfg->startTime;
   cDWORD  offset[2]  = { tcfg->procSync & 0x08 ? 0 : cDWORD(rand() & 0x03F), tcfg->procSync & 0x08 ? 0 : cDWORD(rand() & 0x0FFFF) };

   // Wait for start time
   do {
      Sleep(1);
      timer.Update();
   } while(tcfg->startTime > timer.siCurrentTics);

   // Main loop
   do {
      if(timer.siCurrentTics < nextTic) {
         if(JobCycle[tcfg->recordCount ? 1 : 0][tcfg->procUnits](coreNum, 0, threadByte)) break;

         timer.Update();
      } else {
         printf(".");
         Sleep(DWORD(cycleMs - ((timer.siCurrentTics - tcfg->startTime) * cycleMs / (tcfg->endTime - tcfg->startTime))) + offset[0]);
         timer.Update();
         nextTic += cycleTics + offset[1];
      }
   } while(timer.siCurrentTics < tcfg->endTime);

   _InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));

   _endthread();
}

// CPU methods library
// 0=Constant, 1=Fixed-width pulse, 3=Sweeping-width pulse
const threadfunc MethodCPU[3] = { Constant, FixedSync, SweepSync };
