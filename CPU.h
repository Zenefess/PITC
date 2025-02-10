/************************************************************
 * File: CPU.h                          Created: 2025/01/25 *
 *                                    Last mod.: 2025/02/10 *
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
   union {
      ui32 flags;
      struct {
         ui8 procUnits;  // 0==ALU, 1==FPU, 2==SSE4.1, 3==AVX, 4==AVX512, 5==L1 cache, 6==L2 cache, 7==L3 cache
         ui8 procSync;   // 0==Round-robin, 1==Parallel, 2==Staggered, 3==Synchronised, 4==Constant, 5==Fixed pulse, 6==Sweeping pulse, 7==???
         ui8 threadByte;
         ui8 threadBit;
      };
   };
}; typedef THREAD_CFG *const THREAD_CFGptrc;

struct GLOBAL_CFG {
   struct _C_D_ { ui32 L1Code, L1Data, L2; };
   struct _SYS_ {
      struct {
         declare2d64z(ui64, coreMap, 2, MAX_THREADS); // Bitmaps of available virtual cores; 0==Non-SMT cores, 1==SMT cores
         _C_D_ cache[2];     // Sizes of L1 code & data and L2 caches; 0==Non-SMT cores, 1==SMT cores
         ui32  cacheL3;      // Size of L3 cache per core complex
         si16  coreCount[2]; // Number of cores: 0==Non-SMT, 1==SMT
      };
      ui8  groupCount = 0;     // Number of virtual processor groups
      ui8  SMT        = 0;     // Number of virtual cores per physical core
      bool cpuSSE4_1  = false; // CPU supports the SSE 1.0~4.1 instruction sets
      bool cpuAVX2    = false; // CPU supports the AVX 1.0~2.0 instruction sets
      bool cpuAVX512  = false; // CPU supports the AVX512F instruction set
      ///--- 3 bytes unused
   } sys;
   declare1d64z(ui64, coreMap, MAX_THREADS); // Bitmap of available virtual cores
   si64 tics      = 0;     // Global time limit
   si64 allocMem  = 0;     // Global amount of memory allocated for threads
   ui32 onTime    = 250;   // Computation duration (in ms)
   ui32 offTime   = 750;   // Sleep duration (in ms)
   ui32 delayTime = 2000;  // Start-up delay duration (in ms)
   ui8  procUnits = 0x04;  // 0==ALU, 1==FPU, 2==SSE4.1, 3==AVX, 4==AVX512, 5==L1 cache, 6==L2 cache, 7==L3 cache
   ui8  procSync  = 0x02A; // 8==Round-robin, 9==Parallel 10==Staggered, 11==Synchronised, 12==Constant, 13==Fixed pulse, 14==Sweeping pulse, 15==???
   bool SMT       = false; // Generate multiple threads per physical core

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
}; typedef RESULTS *const RESULTSptrc;

constexpr csi64 RESULTS_BUF_SIZE = sizeof(RESULTS) * MAX_THREADS;

#ifdef __AVX__
#define ThreadsRunning !AllFalse(_mm256_or_si256((si256&)threadBits[0], (si256&)threadBits[4]), max256)
#else
#define ThreadsRunning !AllFalse(_mm_or_si128(_mm_or_si128((ui128&)threadBits[0], (ui128&)threadBits[2]), _mm_or_si128((ui128&)threadBits[4], (ui128&)threadBits[6])), max128)
#endif

al64 CLASS_TIMER timer;
     GLOBAL_CFG  cfg;

     declare1d64z(THREAD_CFG, threadData, MAX_THREADS);
     declare2d64z(RESULTS, value, 3, MAX_THREADS);
     declare1d64z(vui64, threadBits, MAX_THREADS_BYTES);
     declare1d64z(wchar, wstrOut, 1024);

     cwchptrc wstrInstructions[8] = { wstrInstructions_ENG, 0, 0, 0, 0, 0, 0, 0 };
     cwchar   wstrUnitsCPU[8][4]  = { L"ALU", L"FPU", L"SSE", L"AVX", L"512", L"CL1", L"CL2", L"CL3" };
     cwchar   wstrSyncCPU[8][4]   = { L"R-R", L"Par", L"Sta", L"T-S", L"Con", L"F-P", L"S-P", L"..." };
     cwchar   wstrPass[2][8]      = { L".Pass.", L"!Fail!" };
     cchar    strPass[2][8]       = { ".Pass.", "!Fail!" };
     wchar    wstrLang[4]         = L"ENG";

extern void JobALU(si64&);
extern void JobFPU(fl64&);
extern void JobALU_FPU(fl64&, si64&);
extern void JobSSE(fl64x2&);
extern void JobALU_SSE(fl64x2&, si64&);
extern void JobAVX2(fl64x4&);
extern void JobALU_AVX2(fl64x4&, si64&);
extern void JobAVX512(fl64x8&);
extern void JobALU_AVX512(fl64x8&, si64&);

// Constant computation
void Constant(ptrc dataPtr) {
   const THREAD_CFG *const tcfg = (THREAD_CFG *)dataPtr;
   cui64 coreNum   = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   cui8  threadBit = 1u << tcfg->threadBit;
   cui8  procUnits = ui8(PopulationCount64(ui64(tcfg->procUnits)));

   vchar *const threadByte = &((chptr)threadBits)[tcfg->threadByte];

   // Configure thread

   // Wait for start time
   do {
      _mm_pause();
      timer.Update();
   } while(tcfg->startTime > timer.siCurrentTics);

   // Main loop
   while(timer.siCurrentTics < tcfg->endTime) {
      if(procUnits > 1) {
         switch(tcfg->procUnits) {
         case 3:
            value[1][coreNum].fpu = value[0][coreNum].fpu;
            value[1][coreNum].alu = value[0][coreNum].alu;
            JobALU_FPU(value[1][coreNum].fpu, value[1][coreNum].alu);
            if(value[1][coreNum].alu != value[2][coreNum].alu) {
               printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
               _InterlockedAnd8(threadByte, 0x0);
            }
            if(value[1][coreNum].fpu != value[2][coreNum].fpu) {
               printf("ERROR! Core: %2.1lld  Expected: %1.9f  Output: %1.9f\n", coreNum, value[2][coreNum].fpu, value[1][coreNum].fpu);
               _InterlockedAnd8(threadByte, 0x0);
            }
            break;
         case 5:
            value[1][coreNum].sse = value[0][coreNum].sse;
            value[1][coreNum].alu = value[0][coreNum].alu;
            JobALU_SSE(value[1][coreNum].sse, value[1][coreNum].alu);
            if(value[1][coreNum].alu != value[2][coreNum].alu) {
               printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
               _InterlockedAnd8(threadByte, 0x0);
            }
            if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
               printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f  Output: %1.9f, %1.9f\n", coreNum,
                  value[2][coreNum].sse.m128d_f64[0], value[2][coreNum].sse.m128d_f64[1], value[1][coreNum].sse.m128d_f64[0], value[1][coreNum].sse.m128d_f64[1]);
               _InterlockedAnd8(threadByte, 0x0);
            }
            break;
         case 9:
            value[1][coreNum].avx = value[0][coreNum].avx;
            value[1][coreNum].alu = value[0][coreNum].alu;
            JobALU_AVX2(value[1][coreNum].avx, value[1][coreNum].alu);
            if(value[1][coreNum].alu != value[2][coreNum].alu) {
               printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
               _InterlockedAnd8(threadByte, 0x0);
            }
            if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
               printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                  value[2][coreNum].avx.m256d_f64[0], value[2][coreNum].avx.m256d_f64[1], value[2][coreNum].avx.m256d_f64[2], value[2][coreNum].avx.m256d_f64[3],
                  value[1][coreNum].avx.m256d_f64[0], value[1][coreNum].avx.m256d_f64[1], value[1][coreNum].avx.m256d_f64[2], value[1][coreNum].avx.m256d_f64[3]);
               _InterlockedAnd8(threadByte, 0x0);
            }
            break;
         case 17:
            value[1][coreNum].avx512 = value[0][coreNum].avx512;
            value[1][coreNum].alu = value[0][coreNum].alu;
            JobALU_AVX512(value[1][coreNum].avx512, value[1][coreNum].alu);
            if(value[1][coreNum].alu != value[2][coreNum].alu) {
               printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
               _InterlockedAnd8(threadByte, 0x0);
            }
            if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
               printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                  value[2][coreNum].avx512.m512d_f64[0], value[2][coreNum].avx512.m512d_f64[1], value[2][coreNum].avx512.m512d_f64[2], value[2][coreNum].avx512.m512d_f64[3],
                  value[2][coreNum].avx512.m512d_f64[4], value[2][coreNum].avx512.m512d_f64[5], value[2][coreNum].avx512.m512d_f64[6], value[2][coreNum].avx512.m512d_f64[7],
                  value[1][coreNum].avx512.m512d_f64[0], value[1][coreNum].avx512.m512d_f64[1], value[1][coreNum].avx512.m512d_f64[2], value[1][coreNum].avx512.m512d_f64[3],
                  value[1][coreNum].avx512.m512d_f64[4], value[1][coreNum].avx512.m512d_f64[5], value[1][coreNum].avx512.m512d_f64[6], value[1][coreNum].avx512.m512d_f64[7]);
               _InterlockedAnd8(threadByte, 0x0);
            }
         }
      } else {
         if(tcfg->procUnits & 0x01) {
            value[1][coreNum].alu = value[0][coreNum].alu;
            JobALU(value[1][coreNum].alu);
            if(value[1][coreNum].alu != value[2][coreNum].alu) {
               printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
               _InterlockedAnd8(threadByte, 0x0);
               break;
            }
         }
         if(tcfg->procUnits & 0x02) {
            value[1][coreNum].fpu = value[0][coreNum].fpu;
            JobFPU(value[1][coreNum].fpu);
            if(value[1][coreNum].fpu != value[2][coreNum].fpu) {
               printf("ERROR! Core: %2.1lld  Expected: %1.9f  Output: %1.9f\n", coreNum, value[2][coreNum].fpu, value[1][coreNum].fpu);
               _InterlockedAnd8(threadByte, 0x0);
               break;
            }
         }
         if(tcfg->procUnits & 0x04) {
            value[1][coreNum].sse = value[0][coreNum].sse;
            JobSSE(value[1][coreNum].sse);
            if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
               printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f  Output: %1.9f, %1.9f\n", coreNum,
                  value[2][coreNum].sse.m128d_f64[0], value[2][coreNum].sse.m128d_f64[1], value[1][coreNum].sse.m128d_f64[0], value[1][coreNum].sse.m128d_f64[1]);
               _InterlockedAnd8(threadByte, 0x0);
               break;
            }
         }
         if(tcfg->procUnits & 0x08) {
            value[1][coreNum].avx = value[0][coreNum].avx;
            JobAVX2(value[1][coreNum].avx);
            if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
               printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                  value[2][coreNum].avx.m256d_f64[0], value[2][coreNum].avx.m256d_f64[1], value[2][coreNum].avx.m256d_f64[2], value[2][coreNum].avx.m256d_f64[3],
                  value[1][coreNum].avx.m256d_f64[0], value[1][coreNum].avx.m256d_f64[1], value[1][coreNum].avx.m256d_f64[2], value[1][coreNum].avx.m256d_f64[3]);
               _InterlockedAnd8(threadByte, 0x0);
               break;
            }
         }
         if(tcfg->procUnits & 0x10) {
            value[1][coreNum].avx512 = value[0][coreNum].avx512;
            JobAVX512(value[1][coreNum].avx512);
            if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
               printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                  value[2][coreNum].avx512.m512d_f64[0], value[2][coreNum].avx512.m512d_f64[1], value[2][coreNum].avx512.m512d_f64[2], value[2][coreNum].avx512.m512d_f64[3],
                  value[2][coreNum].avx512.m512d_f64[4], value[2][coreNum].avx512.m512d_f64[5], value[2][coreNum].avx512.m512d_f64[6], value[2][coreNum].avx512.m512d_f64[7],
                  value[1][coreNum].avx512.m512d_f64[0], value[1][coreNum].avx512.m512d_f64[1], value[1][coreNum].avx512.m512d_f64[2], value[1][coreNum].avx512.m512d_f64[3],
                  value[1][coreNum].avx512.m512d_f64[4], value[1][coreNum].avx512.m512d_f64[5], value[1][coreNum].avx512.m512d_f64[6], value[1][coreNum].avx512.m512d_f64[7]);
               _InterlockedAnd8(threadByte, 0x0);
               break;
            }
         }
      }

      timer.Update();
   }

   _InterlockedXor8(threadByte, threadBit);

   _endthread();
}

// Alternates between compute & sleep cycles without synchronised threads
void PulseNoSync(ptrc dataPtr) {
   const THREAD_CFG *const tcfg = (THREAD_CFG *)dataPtr;
   cui64 coreNum    = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   csi64 totalTics  = timer.siFrequency * si64(tcfg->activeTime + tcfg->inactiveTime) / 1000;
   cui32 sleepDelay = tcfg->inactiveTime;
   cui8  threadBit  = 1u << tcfg->threadBit;
   cui8  procUnits  = ui8(PopulationCount64(ui64(tcfg->procUnits)));
   si64  nextTic    = tcfg->startTime + (timer.siFrequency * si64(tcfg->activeTime) / 1000);

   vchar *const threadByte = &((chptr)threadBits)[tcfg->threadByte];

   // Configure thread

   // Wait for start time
   do {
      _mm_pause();
      timer.Update();
   } while(tcfg->startTime > timer.siCurrentTics);

   // Main loop
   while(timer.siCurrentTics < tcfg->endTime) {
      if(timer.siCurrentTics < nextTic) {
         if(procUnits > 1) {
            if(tcfg->procUnits == 0x05) {
               value[1][coreNum].sse = value[0][coreNum].sse;
               value[1][coreNum].alu = value[0][coreNum].alu;
               JobALU_SSE(value[1][coreNum].sse, value[1][coreNum].alu);
               if(value[1][coreNum].alu != value[2][coreNum].alu) {
                  printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
               if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f  Output: %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].sse.m128d_f64[0], value[2][coreNum].sse.m128d_f64[1], value[1][coreNum].sse.m128d_f64[0], value[1][coreNum].sse.m128d_f64[1]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits == 0x09) {
               value[1][coreNum].avx = value[0][coreNum].avx;
               value[1][coreNum].alu = value[0][coreNum].alu;
               JobALU_AVX2(value[1][coreNum].avx, value[1][coreNum].alu);
               if(value[1][coreNum].alu != value[2][coreNum].alu) {
                  printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
               if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].avx.m256d_f64[0], value[2][coreNum].avx.m256d_f64[1], value[2][coreNum].avx.m256d_f64[2], value[2][coreNum].avx.m256d_f64[3],
                     value[1][coreNum].avx.m256d_f64[0], value[1][coreNum].avx.m256d_f64[1], value[1][coreNum].avx.m256d_f64[2], value[1][coreNum].avx.m256d_f64[3]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits == 0x11) {
               value[1][coreNum].avx512 = value[0][coreNum].avx512;
               value[1][coreNum].alu = value[0][coreNum].alu;
               JobALU_AVX512(value[1][coreNum].avx512, value[1][coreNum].alu);
               if(value[1][coreNum].alu != value[2][coreNum].alu) {
                  printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
               if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].avx512.m512d_f64[0], value[2][coreNum].avx512.m512d_f64[1], value[2][coreNum].avx512.m512d_f64[2], value[2][coreNum].avx512.m512d_f64[3],
                     value[2][coreNum].avx512.m512d_f64[4], value[2][coreNum].avx512.m512d_f64[5], value[2][coreNum].avx512.m512d_f64[6], value[2][coreNum].avx512.m512d_f64[7],
                     value[1][coreNum].avx512.m512d_f64[0], value[1][coreNum].avx512.m512d_f64[1], value[1][coreNum].avx512.m512d_f64[2], value[1][coreNum].avx512.m512d_f64[3],
                     value[1][coreNum].avx512.m512d_f64[4], value[1][coreNum].avx512.m512d_f64[5], value[1][coreNum].avx512.m512d_f64[6], value[1][coreNum].avx512.m512d_f64[7]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
         } else {
            if(tcfg->procUnits & 0x01) {
               value[1][coreNum].alu = value[0][coreNum].alu;
               JobALU(value[1][coreNum].alu);
               if(value[1][coreNum].alu != value[2][coreNum].alu) {
                  printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits & 0x02) {
               value[1][coreNum].fpu = value[0][coreNum].fpu;
               JobFPU(value[1][coreNum].fpu);
               if(value[1][coreNum].fpu != value[2][coreNum].fpu) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f  Output: %1.9f\n", coreNum, value[2][coreNum].fpu, value[1][coreNum].fpu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits & 0x04) {
               value[1][coreNum].sse = value[0][coreNum].sse;
               JobSSE(value[1][coreNum].sse);
               if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f  Output: %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].sse.m128d_f64[0], value[2][coreNum].sse.m128d_f64[1], value[1][coreNum].sse.m128d_f64[0], value[1][coreNum].sse.m128d_f64[1]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits & 0x08) {
               value[1][coreNum].avx = value[0][coreNum].avx;
               JobAVX2(value[1][coreNum].avx);
               if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].avx.m256d_f64[0], value[2][coreNum].avx.m256d_f64[1], value[2][coreNum].avx.m256d_f64[2], value[2][coreNum].avx.m256d_f64[3],
                     value[1][coreNum].avx.m256d_f64[0], value[1][coreNum].avx.m256d_f64[1], value[1][coreNum].avx.m256d_f64[2], value[1][coreNum].avx.m256d_f64[3]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits & 0x10) {
               value[1][coreNum].avx512 = value[0][coreNum].avx512;
               JobAVX512(value[1][coreNum].avx512);
               if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].avx512.m512d_f64[0], value[2][coreNum].avx512.m512d_f64[1], value[2][coreNum].avx512.m512d_f64[2], value[2][coreNum].avx512.m512d_f64[3],
                     value[2][coreNum].avx512.m512d_f64[4], value[2][coreNum].avx512.m512d_f64[5], value[2][coreNum].avx512.m512d_f64[6], value[2][coreNum].avx512.m512d_f64[7],
                     value[1][coreNum].avx512.m512d_f64[0], value[1][coreNum].avx512.m512d_f64[1], value[1][coreNum].avx512.m512d_f64[2], value[1][coreNum].avx512.m512d_f64[3],
                     value[1][coreNum].avx512.m512d_f64[4], value[1][coreNum].avx512.m512d_f64[5], value[1][coreNum].avx512.m512d_f64[6], value[1][coreNum].avx512.m512d_f64[7]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
         }

         timer.Update();
      } else {
         printf(".");
         Sleep(sleepDelay);
         timer.Update();
         nextTic += totalTics;
      }
   }

   _InterlockedXor8(threadByte, threadBit);

   _endthread();
}

// Alternates between compute & sleep cycles with synchronised threads
void PulseSync(ptrc dataPtr) {
   const THREAD_CFG *const tcfg = (THREAD_CFG *)dataPtr;
   cui64 coreNum    = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   csi64 activeTics = timer.siFrequency * si64(tcfg->activeTime) / 1000;
   cui32 sleepDelay = tcfg->inactiveTime;
   cui8  threadBit  = 1u << tcfg->threadBit;
   cui8  procUnits  = ui8(PopulationCount64(ui64(tcfg->procUnits)));
   si64  nextTic    = tcfg->startTime + activeTics;

   vchar *const threadByte = &((chptr)threadBits)[tcfg->threadByte];

   // Configure thread

   // Wait for start time
   do {
      _mm_pause();
      timer.Update();
   } while(tcfg->startTime > timer.siCurrentTics);

   // Main loop
   while(timer.siCurrentTics < tcfg->endTime) {
      if(timer.siCurrentTics < nextTic) {
         if(procUnits > 1) {
            if(tcfg->procUnits == 0x05) {
               value[1][coreNum].sse = value[0][coreNum].sse;
               value[1][coreNum].alu = value[0][coreNum].alu;
               JobALU_SSE(value[1][coreNum].sse, value[1][coreNum].alu);
               if(value[1][coreNum].alu != value[2][coreNum].alu) {
                  printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
               if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f  Output: %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].sse.m128d_f64[0], value[2][coreNum].sse.m128d_f64[1], value[1][coreNum].sse.m128d_f64[0], value[1][coreNum].sse.m128d_f64[1]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits == 0x09) {
               value[1][coreNum].avx = value[0][coreNum].avx;
               value[1][coreNum].alu = value[0][coreNum].alu;
               JobALU_AVX2(value[1][coreNum].avx, value[1][coreNum].alu);
               if(value[1][coreNum].alu != value[2][coreNum].alu) {
                  printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
               if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].avx.m256d_f64[0], value[2][coreNum].avx.m256d_f64[1], value[2][coreNum].avx.m256d_f64[2], value[2][coreNum].avx.m256d_f64[3],
                     value[1][coreNum].avx.m256d_f64[0], value[1][coreNum].avx.m256d_f64[1], value[1][coreNum].avx.m256d_f64[2], value[1][coreNum].avx.m256d_f64[3]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits == 0x11) {
               value[1][coreNum].avx512 = value[0][coreNum].avx512;
               value[1][coreNum].alu = value[0][coreNum].alu;
               JobALU_AVX512(value[1][coreNum].avx512, value[1][coreNum].alu);
               if(value[1][coreNum].alu != value[2][coreNum].alu) {
                  printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
               if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].avx512.m512d_f64[0], value[2][coreNum].avx512.m512d_f64[1], value[2][coreNum].avx512.m512d_f64[2], value[2][coreNum].avx512.m512d_f64[3],
                     value[2][coreNum].avx512.m512d_f64[4], value[2][coreNum].avx512.m512d_f64[5], value[2][coreNum].avx512.m512d_f64[6], value[2][coreNum].avx512.m512d_f64[7],
                     value[1][coreNum].avx512.m512d_f64[0], value[1][coreNum].avx512.m512d_f64[1], value[1][coreNum].avx512.m512d_f64[2], value[1][coreNum].avx512.m512d_f64[3],
                     value[1][coreNum].avx512.m512d_f64[4], value[1][coreNum].avx512.m512d_f64[5], value[1][coreNum].avx512.m512d_f64[6], value[1][coreNum].avx512.m512d_f64[7]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
         } else {
            if(tcfg->procUnits & 0x01) {
               value[1][coreNum].alu = value[0][coreNum].alu;
               JobALU(value[1][coreNum].alu);
               if(value[1][coreNum].alu != value[2][coreNum].alu) {
                  printf("ERROR! Core: %2.1lld  Expected: %lld  Output: %lld\n", coreNum, value[2][coreNum].alu, value[1][coreNum].alu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits & 0x02) {
               value[1][coreNum].fpu = value[0][coreNum].fpu;
               JobFPU(value[1][coreNum].fpu);
               if(value[1][coreNum].fpu != value[2][coreNum].fpu) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f  Output: %1.9f\n", coreNum, value[2][coreNum].fpu, value[1][coreNum].fpu);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits & 0x04) {
               value[1][coreNum].sse = value[0][coreNum].sse;
               JobSSE(value[1][coreNum].sse);
               if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f  Output: %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].sse.m128d_f64[0], value[2][coreNum].sse.m128d_f64[1], value[1][coreNum].sse.m128d_f64[0], value[1][coreNum].sse.m128d_f64[1]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits & 0x08) {
               value[1][coreNum].avx = value[0][coreNum].avx;
               JobAVX2(value[1][coreNum].avx);
               if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].avx.m256d_f64[0], value[2][coreNum].avx.m256d_f64[1], value[2][coreNum].avx.m256d_f64[2], value[2][coreNum].avx.m256d_f64[3],
                     value[1][coreNum].avx.m256d_f64[0], value[1][coreNum].avx.m256d_f64[1], value[1][coreNum].avx.m256d_f64[2], value[1][coreNum].avx.m256d_f64[3]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
            if(tcfg->procUnits & 0x10) {
               value[1][coreNum].avx512 = value[0][coreNum].avx512;
               JobAVX512(value[1][coreNum].avx512);
               if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
                  printf("ERROR! Core: %2.1lld  Expected: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  Output: %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n", coreNum,
                     value[2][coreNum].avx512.m512d_f64[0], value[2][coreNum].avx512.m512d_f64[1], value[2][coreNum].avx512.m512d_f64[2], value[2][coreNum].avx512.m512d_f64[3],
                     value[2][coreNum].avx512.m512d_f64[4], value[2][coreNum].avx512.m512d_f64[5], value[2][coreNum].avx512.m512d_f64[6], value[2][coreNum].avx512.m512d_f64[7],
                     value[1][coreNum].avx512.m512d_f64[0], value[1][coreNum].avx512.m512d_f64[1], value[1][coreNum].avx512.m512d_f64[2], value[1][coreNum].avx512.m512d_f64[3],
                     value[1][coreNum].avx512.m512d_f64[4], value[1][coreNum].avx512.m512d_f64[5], value[1][coreNum].avx512.m512d_f64[6], value[1][coreNum].avx512.m512d_f64[7]);
                  _InterlockedAnd8(threadByte, 0x0);
                  break;
               }
            }
         }

         timer.Update();
      } else {
         printf(".");
         Sleep(sleepDelay);
         timer.Update();
         nextTic = timer.siCurrentTics + activeTics;
      }
   }

   _InterlockedXor8(threadByte, threadBit);

   _endthread();
}
// CPU methods library
// 0=Constant
// 2=PulseNoSync, 3=PulseSync
// 4=SweepNoSync, 5=SweepSync
// 6=???
const threadfunc MethodCPU[8] = { Constant, Constant, PulseNoSync, PulseSync, 0, 0, 0, 0 };
