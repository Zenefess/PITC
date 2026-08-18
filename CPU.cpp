/*
 * File: CPU.cpp
 * Version: v1.1
 * Owner: David William Bull
 * Created: 2025-01-21
 * Last Modified: 2026-08-18
 * Description: PITC entry point: option parsing, arena allocation, thread spawn and reporting; defines every namespace-scope object.
 * Dependencies: CPU_methods.h
 * ISA: Scalar
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */

#include "CPU.h"
#include "CPU_methods.h"

//--- Global variables ---//
// Every object this program holds at namespace scope is defined here, and declared in CPU.h -- for the
// six language tables, in translations.h. The initialisation order is the order of the definitions,
// which is all one translation unit has to guarantee: none of them reads another as it is constructed
al64 CLASS_TIMER timer;
     GLOBAL_CFG  cfg;

     declare1d64z(THREAD_CFG, threadData, MAX_THREADS);
     declare2d64z(RESULTS, value, 4, MAX_THREADS); // Result values: 0==Input, 1=Processed, 2=Output, 3=Error
     declare1d64z(vui64, threadBits, MAX_THREADS_WORDS);
     declare1d64z(wchar, wstrOut, 1024);
     RESULTS_ARRAYS resArray;
     vsi8  generateError = 0;
     wchar wstrLang[6]   = L"en-GB";
     CONSOLE_CODE_PAGE consoleCP; // Captured here, before wmain runs; restored at exit and by RestoreConsoleCP

// Default: English. All six move together, and the 'L' case below is the only other place that writes them
cwchptr     wstrInstructions = wstrInstructions_English;
cwchptrcptr wstrMessage      = wstrMessage_English;
cwchptrcptr wstrInterface    = wstrInterface_English;
cwchar4ptr  wstrUnitsCPU     = wstrUnitsCPU_English;
cwchar4ptr  wstrSyncCPU      = wstrSyncCPU_English;
cwchar8ptr  wstrPass         = wstrPass_English;

// Job cycle functions array. [0][]==Without memory, [1][]==With memory. Each entry is defined in the
// translation unit of the kernel it wraps; CPU_job_cycles.h declares them and states the rules this
// table has to keep, of which the first is that all 32 indices of the (procUnits & 0x1F) domain are
// covered, because ComputationPulse does not range-check the index it dispatches through.
al64 cui8 (*JOB_CYCLE[2][32])(cui64 coreNum, csi64 offset, vchptrc threadByte) = {
 { JobCycleALU,       JobCycleALU,           JobCycleFPU,       JobCycleALU_FPU,       JobCycleSSE,       JobCycleALU_SSE,       JobCycleSSE,       JobCycleALU_SSE,
   JobCycleAVX,       JobCycleALU_AVX,       JobCycleAVX,       JobCycleALU_AVX,       JobCycleAVX,       JobCycleALU_AVX,       JobCycleAVX,       JobCycleALU_AVX,
   JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,
   JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512, },
 { JobCycleMemALU,    JobCycleMemALU,        JobCycleMemFPU,    JobCycleMemALU_FPU,    JobCycleMemSSE,    JobCycleMemALU_SSE,    JobCycleMemSSE,    JobCycleMemALU_SSE,
   JobCycleMemAVX,    JobCycleMemALU_AVX,    JobCycleMemAVX,    JobCycleMemALU_AVX,    JobCycleMemAVX,    JobCycleMemALU_AVX,    JobCycleMemAVX,    JobCycleMemALU_AVX,
   JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512,
   JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512 }
};
//--- Global variables ---//

// Print computational failure data. Declared in CPU.h and called from all four CPU_jobs_*.cpp units, which is
// why it is a definition here rather than a static function in the header
void Failed(cui64 coreNum, vchptrc threadByte, cui8 unit) {
   cui8 threadMask = ui8(~(1u << (coreNum & 0x07)));

   wprintf(wstrInterface[11], coreNum);

   switch(unit) {
   case 0: {
      cfl64ptrc expect = value[2][coreNum].avx512.m512d_f64;
      cfl64ptrc actual = value[3][coreNum].avx512.m512d_f64;

      wprintf(wstrInterface[13],
         expect[0], expect[1], expect[2], expect[3], expect[4], expect[5], expect[6], expect[7], wstrInterface[12],
         actual[0], actual[1], actual[2], actual[3], actual[4], actual[5], actual[6], actual[7]);
      break;
   }
   case 1: {
      cfl64ptrc expect = value[2][coreNum].avx.m256d_f64;
      cfl64ptrc actual = value[3][coreNum].avx.m256d_f64;

      wprintf(wstrInterface[14],
         expect[0], expect[1], expect[2], expect[3], wstrInterface[12],
         actual[0], actual[1], actual[2], actual[3]);
      break;
   }
   case 2: {
      cfl64ptrc expect = value[2][coreNum].sse.m128d_f64;
      cfl64ptrc actual = value[3][coreNum].sse.m128d_f64;

      wprintf(wstrInterface[15], expect[0], expect[1], wstrInterface[12], actual[0], actual[1]);
      break;
   }
   case 3:
      wprintf(wstrInterface[16], value[2][coreNum].fpu, wstrInterface[12], value[3][coreNum].fpu);
      break;
   case 4:
      wprintf(wstrInterface[17], value[2][coreNum].alu, wstrInterface[12], value[3][coreNum].alu);
   }
   _InterlockedAnd8(threadByte, threadMask);
   return;
}

// Console control handler, registered by wmain beside the one write that changes the console's output code
// page. Ctrl-C -- the way a run of hours is abandoned -- terminates through ExitProcess, which unwinds no
// destructor, and the code page is conhost state the launching shell keeps after this process is gone.
// The handler restores it and declines the event, so default termination still proceeds
static BOOL __stdcall RestoreConsoleCP(DWORD) {
   if(consoleCP.codePage) SetConsoleOutputCP(consoleCP.codePage);
   return FALSE;
}

csi32 wmain(csi32 argc, cwchptrc argv[]) {
   VALUES_HEADER header;
   // Handles of the worker threads, in thread order. The 'W' path returns before the test path spawns
   // anything, so the one array serves both
   HANDLE threadHandle[MAX_THREADS] = {};
   ptr   outFile;
   ui64  mask;
   int   c = 1, d;
   si16  threadCount[3] = { 0, 0, 0 }; // 0=First core class, 1=Second core class, 2=Total
   si16  i;
   ui32  j;
   si16  k;              // Indexes value[] over every selected core, so it counts to MAX_THREADS, not to 255
   ui8   outUTF = 0;
   wchar wstrLangArg[6]; // The candidate code an 'L' argument names; wstrLang records only the active language

   if(setlocale(LC_CTYPE, ".UTF8")) { SetConsoleOutputCP(CP_UTF8); SetConsoleCtrlHandler(RestoreConsoleCP, TRUE); }
   else setlocale(LC_CTYPE, "");
   setlocale(LC_NUMERIC, "C");

   csi32 topology = EnumerateTopology();

   if(topology) return topology;

   cfg.sys.cpuSSE2   = IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE);
   cfg.sys.cpuAVX    = IsProcessorFeaturePresent(PF_AVX_INSTRUCTIONS_AVAILABLE);
   cfg.sys.cpuAVX512 = IsProcessorFeaturePresent(PF_AVX512F_INSTRUCTIONS_AVAILABLE);

   // Set vector-dependent functions to use largest instruction width available.
   static bool (&ThreadsRunning)(void) = cfg.sys.cpuAVX512 ? ThreadsRunningAVX512 : cfg.sys.cpuAVX ? ThreadsRunningAVX
                                       : cfg.sys.cpuSSE2   ? ThreadsRunningSSE    : ThreadsRunningScalar;

   /// Defaults ///
   cfg.tics        = timer.siFrequency * 900; // 15 minute duration
   cfg.procSync    = 0x012;
   cfg.procUnits   = 0x03;
   cfg.allocMem[0] = 0;
   cfg.allocMem[1] = 0;
   // No 'M' has been given yet, so a cache-targeted run derives its own sizes rather than checking one the
   // command line stated. Every 'M' sub-option sets this, and 'B' and the preset preamble clear it again
   cfg.memExplicit = 0;
   /// Defaults ///

   if(argc > 1) {
      for(i = 1; i < argc; ++i) {
         switch(argv[i][0]) {
         case L'b': // Run benchmark: All virtual cores, constant computation, ALU + largest vector unit, 8MB memory per virtual core, for 60 seconds
         case L'B':
            if(argv[i][1] && argv[i][1] != L' ') { wprintf(wstrMessage[37], argv[i]); return -25; }
            mfree1(resArray.iter);
            resArray.iter = zalloc1d64(si64, cfg.sys.vCoreCount);
            if(!resArray.iter) {
               wprintf(wstrMessage[22], (si64(cfg.sys.vCoreCount) * si64(sizeof(si64))) >> 20);
               return -17;
            }
            cfg.tics        = timer.siFrequency * 60;
            cfg.SMTLoad     = 3;
            cfg.memConfig   = 1;
            cfg.allocMem[0] = 8388608;
            cfg.allocMem[1] = 0;
            cfg.memExplicit = 0;
            cfg.procSync    = 0x092;
            cfg.procUnits   = (cfg.sys.cpuAVX512 ? 0x011 : cfg.sys.cpuAVX ? 0x09 : 0x05);
            break;
         case L'i': // Set instruction usage options
         case L'I':
            cfg.procUnits = 0;
            for(j = 1; argv[i][j] && argv[i][j] != L' '; ++j) {
               switch(argv[i][j]) {
               case L'1': // Process dataset for L1 cache
                  cfg.procUnits |= 0x020;
                  break;
               case L'2': // Process dataset for L2 cache
                  cfg.procUnits |= 0x040;
                  break;
               case L'3': // Process dataset for L3 cache
                  cfg.procUnits |= 0x080;
                  break;
               case L'a': // Execute ALU codepath
               case L'A':
                  cfg.procUnits |= 0x01;
                  break;
               case L'f': // Execute FPU codepath
               case L'F':
                  cfg.procUnits |= 0x02;
                  break;
               case L's': // Execute SSE codepath
               case L'S':
                  cfg.procUnits |= 0x04;
                  break;
               case L'v': // Execute AVX codepath
               case L'V':
                  cfg.procUnits |= 0x08;
                  break;
               case L'x': // Execute AVX512 codepath
               case L'X':
                  cfg.procUnits |= 0x010;
                  break;
               default:
                  wprintf(wstrMessage[38], argv[i][j], argv[i]);
                  return -25;
               }
            }
            break;
         case L'l': // Set language
         case L'L':
            for(c = 1; argv[i][c] && argv[i][c] != L' ' && c < 1024; ++c);
            lstrcpynW(wstrLangArg, &argv[i][1], min(c, si32(_countof(wstrLangArg))));
            for(d = 0; d < si32(_countof(LANGUAGES)); ++d)
               if(!lstrcmpiW(wstrLangArg, LANGUAGES[d].wstrCode)) {
                  wstrInstructions = LANGUAGES[d].wstrInstructions;
                  wstrMessage      = LANGUAGES[d].wstrMessage;
                  wstrInterface    = LANGUAGES[d].wstrInterface;
                  wstrUnitsCPU     = LANGUAGES[d].wstrUnitsCPU;
                  wstrSyncCPU      = LANGUAGES[d].wstrSyncCPU;
                  wstrPass         = LANGUAGES[d].wstrPass;
                  // The recorded code is the registry's spelling, not the argument's: 'len-us' selects en-US
                  lstrcpynW(wstrLang, LANGUAGES[d].wstrCode, si32(_countof(wstrLang)));
                  break;
               }
            // A language this build does not carry will be reported on, but execution will continue
            if(d == si32(_countof(LANGUAGES))) wprintf(wstrMessage[39], wstrLangArg);
            break;
         case L'm': // Set amount of memory (in MB) to utilise during test
         case L'M':
            for(j = 1; argv[i][j] && argv[i][j] != L' '; ++j) {
               si64 megabytes;

               switch(argv[i][j]) {
               case L'c': // For each virtual core
               case L'C':
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_MEM_MB_MAX, megabytes)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_MEM_MB_MAX); return -24;
                  }
                  cfg.memConfig   = 1;
                  cfg.memExplicit = 1;
                  cfg.allocMem[0] = megabytes << 20;
                  break;
               case L'n': // For each core of the first class
               case L'N':
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_MEM_MB_MAX, megabytes)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_MEM_MB_MAX); return -24;
                  }
                  cfg.memConfig   = 2;
                  cfg.memExplicit = 1;
                  cfg.allocMem[0] = megabytes << 20;
                  break;
               case L's': // For each virtual core of the second class
               case L'S':
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_MEM_MB_MAX, megabytes)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_MEM_MB_MAX); return -24;
                  }
                  cfg.memConfig   = 2;
                  cfg.memExplicit = 1;
                  cfg.allocMem[1] = megabytes << 20;
                  break;
               case L't': // Equally split amongst all utilised virtual cores
               case L'T':
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_MEM_MB_MAX, megabytes)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_MEM_MB_MAX); return -24;
                  }
                  cfg.memConfig   = 0;
                  cfg.memExplicit = 1;
                  cfg.allocMem[0] = megabytes << 20;
                  break;
               default:
                  wprintf(wstrMessage[38], argv[i][j], argv[i]);
                  return -25;
               }
            }
            break;
         case L'o': // Output results to file
         case L'O':
            for(c = 1; argv[i][c] && argv[i][c] != L' '; ++c) {
               switch(argv[i][c]) {
               case L'1':
                  if(argv[i][c + 1] != L'6') {
                     wprintf(wstrMessage[38], argv[i][c], argv[i]);
                     return -25;
                  }
                  outUTF = 2;
                  ++c;
                  break;
               case L'8':
                  outUTF = 1;
                  break;
               case L'a':
               case L'A':
                  outUTF = 0;
                  break;
               case L'[':
                  for(d = ++c; argv[i][c] && argv[i][c] != L']' && c - d < 1023; ++c);
                  if(argv[i][c] != L']' || c == d) {
                     wprintf(wstrMessage[9], argv[i]);
                     return -8;
                  }
                  lstrcpynW(wstrOut, &argv[i][d], c - d + 1);
                  break;
               default:
                  wprintf(wstrMessage[38], argv[i][c], argv[i]);
                  return -25;
               }
            }
            break;
         case L's': // Set core synchronisation options
         case L'S':
            cfg.procSync &= 0x0F0;
            for(j = 1; argv[i][j] && argv[i][j] != L' '; ++j) {
               switch(argv[i][j]) {
               case L'p': // Parallel thread execution
               case L'P':
                  cfg.procSync |= 0x02;
                  break;
               case L'r': // Round-robin thread execution
               case L'R':
                  cfg.procSync |= 0x01;
                  break;
               case L's': // Staggered thread execution
               case L'S':
                  cfg.procSync |= 0x04;
                  break;
               case L't': // Time-synchronised execution
               case L'T':
                  cfg.procSync |= 0x08;
                  break;
               default:
                  wprintf(wstrMessage[38], argv[i][j], argv[i]);
                  return -25;
               }
            }
            break;
         case L't': // Set timing options
         case L'T':
            for(j = 1; argv[i][j] && argv[i][j] != L' '; ++j) {
               si64 milliseconds;
               fl64 seconds;

               switch(argv[i][j]) {
               case L'[': // Fixed pulse: on time / Sweeping pulse: cycle time
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_PULSE_MS_MAX, milliseconds)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_PULSE_MS_MAX); return -24;
                  }
                  cfg.onTime = ui32(milliseconds);
                  break;
               case L']': // Fixed pulse off-time
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_PULSE_MS_MAX, milliseconds)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_PULSE_MS_MAX); return -24;
                  }
                  cfg.offTime = ui32(milliseconds);
                  break;
               case L'c': // Constant thread execution
               case L'C':
                  cfg.procSync &= 0x08F; // Bits 4-6 are mutually exclusive, and are only replaced by an
                  cfg.procSync |= 0x010; // argument that names a mode: 'C', 'F' or 'S'
                  break;
               case L'd': // Set start-up delay
               case L'D':
                  if(!ParseDecimal(argv[i], j, 0.0, OPT_DELAY_MAX, seconds)) {
                     wprintf(wstrMessage[36], argv[i][j], argv[i], 0.0, OPT_DELAY_MAX); return -24;
                  }
                  cfg.delayTime = ui32(seconds * 1000.0);
                  break;
               case L'f': // Fixed pulse-width thread execution
               case L'F':
                  cfg.procSync &= 0x08F;
                  cfg.procSync |= 0x020;
                  break;
               case L's': // Sweeping pulse-width thread execution
               case L'S':
                  cfg.procSync &= 0x08F;
                  cfg.procSync |= 0x040;
                  break;
               case L't': // Test duration
               case L'T':
                  if(!ParseDecimal(argv[i], j, 0.0, OPT_DURATION_MAX, seconds)) {
                     wprintf(wstrMessage[36], argv[i][j], argv[i], 0.0, OPT_DURATION_MAX); return -24;
                  }
                  cfg.tics = si64(fl64(timer.siFrequency) * seconds);
                  break;
               default:
                  wprintf(wstrMessage[38], argv[i][j], argv[i]);
                  return -25;
               }
            }
            break;
         case L'u': // Set core usage options
         case L'U':
            for(j = 1; argv[i][j] && argv[i][j] != L' '; ++j) {
               switch(argv[i][j]) {
               case L'a': // Generate threads for every virtual core
               case L'A':
                  cfg.SMTLoad = 3;
                  break;
               case L'c': // Binary sequence map of physical cores to utilise (eg. x..x.xxx)
               case L'C':
                  ParseCoreMap(argv[i], ++j, true);
                  break;
               case L'e': // Only utilise the first virtual core of each active physical core
               case L'E':
                  cfg.SMTLoad = 1;
                  break;
               case L'o': // Only utilise the last virtual core of each active physical core
               case L'O':
                  cfg.SMTLoad = 2;
                  break;
               case L't': // Binary sequence map of virtual cores to utilise (eg. xx..x.x...xx.xxx)
               case L'T':
                  ParseCoreMap(argv[i], ++j, false);
                  break;
               default:
                  wprintf(wstrMessage[38], argv[i][j], argv[i]);
                  return -25;
               }
            }
            break;
         case L'w':
         case L'W': { // Write new "cpu.values" file
            union { ui64 _64; ui32 _32[2]; } randNum;

            if(argv[i][1] && argv[i][1] != L' ') { wprintf(wstrMessage[37], argv[i]); return -25; }

            cui8 badKernel = ValidateKernelFamilies();
            if(badKernel) {
               wprintf(wstrMessage[badKernel < KERNEL_NAME_LADDER ? 30 : 41], wstrKernelName[badKernel]);
               return -22;
            }

            for(si16 t = 0; t < MAX_THREADS; ++t) {
               for(ui8 lane = 0; lane < 15; ++lane) {
                  rand_s(randNum._32); rand_s(&randNum._32[1]);
                  value[0][t]._fl64[lane] = fl64(randNum._64) / 2048.0;
               }
               rand_s(&value[0][t].raw32[30]); rand_s(&value[0][t].raw32[31]);
            }
            memcpy_s(value[3], RESULTS_BUF_SIZE, value[0], RESULTS_BUF_SIZE);

            for(si16 t = 0; t < cfg.sys.vCoreCount; ++t) {
               threadData[t].threadByte = t >> 3;
               threadData[t].threadBit  = t & 0x07;

               SetThreadRunning(t); // Interlocked: a thread spawned earlier may be clearing this same byte

               threadHandle[t] = (HANDLE)_beginthreadex(0, 0, GenerateValues, &threadData[t], 0, 0);

               if(!threadHandle[t]) {
                  ClearThreadRunning(t); ReleaseThreads(threadHandle, t); wprintf(wstrMessage[23], t); return -19;
               }
            }
            while(ThreadsRunning()) Sleep(100);

            JoinThreads(threadHandle, cfg.sys.vCoreCount);

            if(generateError) {
               wprintf(wstrMessage[5]);
               return -4;
            }

            cwchptrc valuesName = L"cpu.values";
            cwchptrc valuesTemp = L"cpu.values.tmp";

            outFile = CreateFileW(valuesTemp, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            if(outFile == INVALID_HANDLE_VALUE) {
               wprintf(wstrMessage[6], valuesTemp);
               return -5;
            }

            // The header records the build and the kernel arithmetic these values were produced by, and a
            // hash of each block, so a file left over from an earlier revision is reported as a stale file
            // rather than reaching the comparison and accusing the CPU. Each write is checked for length,
            // and each failure path closes the file and removes the temporary, leaving both the directory
            // and any previous "cpu.values" as it found them
            FillValuesHeader(header, value[0], value[3]);

            if(!WriteBlock(outFile, &header, ui32(sizeof(VALUES_HEADER)))) {
               wprintf(wstrMessage[25]);
               CloseHandle(outFile);   DeleteFileW(valuesTemp);
               return -20;
            }
            if(!WriteBlock(outFile, value[0], ui32(RESULTS_BUF_SIZE))) {
               wprintf(wstrMessage[7]);
               CloseHandle(outFile);   DeleteFileW(valuesTemp);
               return -6;
            }
            if(!WriteBlock(outFile, value[3], ui32(RESULTS_BUF_SIZE))) {
               wprintf(wstrMessage[8]);
               CloseHandle(outFile);   DeleteFileW(valuesTemp);
               return -7;
            }

            // Flushed before the handle is closed, so that the contents cannot still be in a cache when the
            // move below publishes the name: a power loss between the two would otherwise leave a
            // "cpu.values" of the right name and the wrong contents, which is the one outcome this whole
            // arrangement exists to prevent. A failed flush is a failed write, and is treated as one
            cbool flushed = FlushFileBuffers(outFile) ? true : false;

            CloseHandle(outFile);

            // MOVEFILE_REPLACE_EXISTING is the replacement itself -- a rename within one directory, so the
            // old file is either wholly there or wholly replaced -- and MOVEFILE_WRITE_THROUGH holds the call
            // until the move itself is on the disk. A failure leaves the previous file exactly as it was, so
            // there is nothing to repair beyond removing the temporary this run built
            if(!flushed || !MoveFileExW(valuesTemp, valuesName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
               wprintf(wstrMessage[42], valuesName);
               DeleteFileW(valuesTemp);
               return -5;
            }

            wprintf(wstrMessage[1]);

            return 1;
         }
         case L'-': // Configuration presets
            if(argv[i][1] < L'0' || argv[i][1] > L'9' || (argv[i][2] && argv[i][2] != L' ')) {
               wprintf(wstrMessage[37], argv[i]);
               return -25;
            }
            cfg.memConfig   = 1;
            cfg.allocMem[0] = 8388608;
            cfg.allocMem[1] = 0;
            cfg.memExplicit = 0;
            cfg.procUnits   = (cfg.sys.cpuAVX512 ? 0x011 : cfg.sys.cpuAVX ? 0x09 : 0x05);
            switch(argv[i][1]) {
            case L'1': // Constant stress; one thread per physical core. 10 minute duration
               cfg.procSync = 0x012;
               cfg.SMTLoad  = 2;
               cfg.tics     = timer.siFrequency * 600;
               break;
            case L'2': // Constant stress on all virtual cores. 30 minute duration
               cfg.procSync = 0x012;
               cfg.SMTLoad  = 3;
               cfg.tics     = timer.siFrequency * 1800;
               break;
            case L'3': // Fixed-width round-robin pulsed stress; one thread per physical core. 10 minute duration
               cfg.procSync = 0x021;
               cfg.SMTLoad  = 2;
               cfg.tics     = timer.siFrequency * 600;
               cfg.onTime   = 200;
               cfg.offTime  = 0;
               break;
            case L'4': // Synchronised fixed-width pulsed stress; one thread per physical core. 10 minute duration
               cfg.procSync = 0x02A;
               cfg.SMTLoad  = 2;
               cfg.tics     = timer.siFrequency * 600;
               cfg.onTime   = 250;
               cfg.offTime  = 1750;
               break;
            case L'5': // Synchronised fixed-width pulsed stress on all virtual cores. 30 minute duration
               cfg.procSync = 0x02A;
               cfg.SMTLoad  = 3;
               cfg.tics     = timer.siFrequency * 1800;
               cfg.onTime   = 250;
               cfg.offTime  = 1750;
               break;
            case L'6': // Sweeping-width pulsed stress; one thread per physical core. 30 minute duration
               cfg.procSync = 0x042;
               cfg.SMTLoad  = 2;
               cfg.tics     = timer.siFrequency * 1800;
               cfg.onTime   = 2000;
               break;
            case L'7': // Synchronised sweeping-width pulsed stress on all virtual cores. 30 minute duration
               cfg.procSync = 0x04A;
               cfg.SMTLoad  = 3;
               cfg.tics     = timer.siFrequency * 1800;
               cfg.onTime   = 2500;
               break;
            case L'8': // Staggered fixed-width pulsed stress; one thread per physical core. 1 hour duration
               cfg.procSync = 0x024;
               cfg.SMTLoad  = 2;
               cfg.tics     = timer.siFrequency * 3600;
               cfg.onTime   = 900;
               cfg.offTime  = 100;
               break;
            case L'9': // Synchronised staggered fixed-width pulsed stress on all virtual cores. 4 hour duration
               cfg.procSync = 0x02C;
               cfg.SMTLoad  = 3;
               cfg.tics     = timer.siFrequency * 14400;
               cfg.onTime   = 900;
               cfg.offTime  = 100;
               break;
            case L'0': // Synchronised fixed-width pulsed stress on all virtual cores, using ALU & SSE code-paths with 2MB memory per core. 1 hour duration
               cfg.allocMem[0] = 2097152;
               cfg.procUnits   = 0x05;
               cfg.procSync    = 0x02A;
               cfg.SMTLoad     = 3;
               cfg.tics        = timer.siFrequency * 3600;
               cfg.onTime      = 4000;
               cfg.offTime     = 4000;
            }
            break;
         default:
            wprintf(wstrMessage[37], argv[i]);
            return -25;
         }
      }
   } else { // Display instructions
      wprintf(L"%s", wstrInstructions);
      return 2;
   }

   cui8 procUnitBits = ui8(cfg.procUnits & 0x01F); // ALU, FPU, SSE2, AVX and AVX-512
   cui8 vectorBits   = ui8(cfg.procUnits & 0x01E); // FPU, SSE2, AVX and AVX-512 are mutually exclusive
   if(!procUnitBits)                 { wprintf(wstrMessage[18]); return -15; }
   if(vectorBits & (vectorBits - 1)) { wprintf(wstrMessage[19]); return -16; }

   if(cfg.procUnits & 0x04  && !cfg.sys.cpuSSE2)   { wprintf(wstrMessage[12]); return -11; }
   if(cfg.procUnits & 0x08  && !cfg.sys.cpuAVX)    { wprintf(wstrMessage[13]); return -11; }
   if(cfg.procUnits & 0x010 && !cfg.sys.cpuAVX512) { wprintf(wstrMessage[14]); return -11; }

   cui8 syncShape = ui8(cfg.procSync & 0x07); // Round-robin, Parallel and Staggered are mutually exclusive
   if(syncShape & (syncShape - 1))                 { wprintf(wstrMessage[15]); return -12; }
   if(cfg.tics <= 0)                               { wprintf(wstrMessage[16]); return -13; }

   if(!syncShape) cfg.procSync |= 0x02; // An unspecified pulse shape is parallel; it must never stay 0

   if(!(cfg.procSync & 0x070)) cfg.procSync |= 0x010;

   if(cfg.procSync & 0x040) cfg.offTime = 0;

   if(!(cfg.procSync & 0x010) && !cfg.onTime)      { wprintf(wstrMessage[17]); return -14; }

   outFile = CreateFileW(L"cpu.values", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, 0);
   if(outFile == INVALID_HANDLE_VALUE) {
      wprintf(wstrMessage[2]);
      return -1;
   }

   if(!ReadBlock(outFile, &header, ui32(sizeof(VALUES_HEADER))) || header.magic != VALUES_FILE_MAGIC) {
      wprintf(wstrMessage[26]);
      CloseHandle(outFile);
      return -21;
   }
   if(header.version != VALUES_FILE_VERSION || header.headerSize != ui32(sizeof(VALUES_HEADER))) {
      wprintf(wstrMessage[27], header.version, VALUES_FILE_VERSION);
      CloseHandle(outFile);
      return -21;
   }
   if(header.blockSize != cui64(RESULTS_BUF_SIZE) || header.buildID != VALUES_BUILD_ID ||
      header.kernelID != KernelFingerprint()) {
      wprintf(wstrMessage[28]);
      CloseHandle(outFile);
      return -21;
   }
   if(!ReadBlock(outFile, value[0], ui32(RESULTS_BUF_SIZE))) {
      wprintf(wstrMessage[3]);
      CloseHandle(outFile);
      return -2;
   }
   memcpy_s(value[1], RESULTS_BUF_SIZE, value[0], RESULTS_BUF_SIZE);
   if(!ReadBlock(outFile, value[2], ui32(RESULTS_BUF_SIZE))) {
      wprintf(wstrMessage[4]);
      CloseHandle(outFile);
      return -3;
   }
   if(HashBytes(value[0], RESULTS_BUF_SIZE, VALUES_HASH_BASIS) != header.seedHash ||
      HashBytes(value[2], RESULTS_BUF_SIZE, VALUES_HASH_BASIS) != header.valueHash) {
      wprintf(wstrMessage[29]);
      CloseHandle(outFile);
      return -21;
   }
   memcpy_s(value[3], RESULTS_BUF_SIZE, value[2], RESULTS_BUF_SIZE);
   CloseHandle(outFile);

   // Count virtual cores to be used
   SetSMTLoading();
   for(j = 0; j < ui32(cfg.sys.groupCount); ++j) {
      threadCount[0] += (si16)SetBitCount64(cfg.sys.coreMap[0][j] & cfg.coreMap[j]);
      threadCount[1] += (si16)SetBitCount64(cfg.sys.coreMap[1][j] & cfg.coreMap[j]);
   }
   threadCount[2] = threadCount[0] + threadCount[1];

   if(!threadCount[2]) { wprintf(wstrMessage[40]); return -26; }

   if(wstrOut[0]) {
      outFile = CreateFileW(wstrOut, GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

      cbool fileExisted = (GetLastError() == ERROR_ALREADY_EXISTS);

      if(outFile == INVALID_HANDLE_VALUE) {
         wprintf(wstrMessage[10], wstrOut);
         return -9;
      }
      CloseHandle(outFile);
      if(!fileExisted) DeleteFileW(wstrOut);
   }

   cui8 cacheLevel   = HighestCacheLevel(cfg.procUnits);
   ui64 recSize      = 0, vecUnits = 0; // Record geometry, hoisted out of the arena block below
   ui64 cacheSize[2] = { 0, 0 };        // Derived per-thread block size, per core class
   ui64 cacheLow[2]  = { 0, 0 };        // Smallest level-resident block size, per core class
   ui64 cacheHigh[2] = { 0, 0 };        // Largest level-resident block size, per core class
   ui32 feasibleK    = 0;               // Threads one instance of the level could hold resident, where none can

   RecordGeometry(ui8(cfg.procUnits & 0x01F), recSize, vecUnits);

   if(cacheLevel) {
      csi8 sized = CalcCacheBlockSizes(cacheLevel, recSize, threadCount, cacheSize, cacheLow, cacheHigh, feasibleK);

      if(sized < 0) { wprintf(wstrMessage[43], ui32(cacheLevel)); return -27; }

      // An 'M' given since the last reset states the size itself; its window is checked below the switch
      if(!cfg.memExplicit) {
         if(sized > 0) wprintf(wstrMessage[44], ui32(cacheLevel), feasibleK, ui32(cacheLevel));

         resArray.blockSize[0] = cacheSize[0];
         resArray.blockSize[1] = cacheSize[1];

         cfg.allocMem[0] = si64(cacheSize[0]) * si64(threadCount[0]) + si64(cacheSize[1]) * si64(threadCount[1]);
         cfg.memConfig   = 3;
      }
   }

   // Memory allocation and pointer configuration
   switch(cfg.memConfig) {
   case 0: // Total memory
      resArray.blockSize[0] = resArray.blockSize[1] = cfg.allocMem[0] / (cui64)threadCount[2];
      break;
   case 1: // Memory per core
      resArray.blockSize[0] = resArray.blockSize[1] = cfg.allocMem[0];
      cfg.allocMem[0] *= (cui64)threadCount[2];
      break;
   case 2: // Separate per core class
      resArray.blockSize[0] = cfg.allocMem[0];
      resArray.blockSize[1] = cfg.allocMem[1];
      cfg.allocMem[0] = resArray.blockSize[0] * threadCount[0] + (resArray.blockSize[1] * threadCount[1]);
      break;
   case 3: // Derived from the requested cache level: the sizing block above wrote both sizes and the total
      break;
   }

   if(cacheLevel && cfg.memExplicit)
      for(ui8 cc = 0; cc < 2; ++cc)
         if(threadCount[cc] && (resArray.blockSize[cc] < cacheLow[cc] || resArray.blockSize[cc] > cacheHigh[cc]))
            wprintf(wstrMessage[45], ui32(cacheLevel), cacheLow[cc] >> 10, cacheHigh[cc] >> 10, ui32(cc));

   if(resArray.blockSize[0] || resArray.blockSize[1]) {
      MEMORYSTATUSEX memStatus = { ui32(sizeof(MEMORYSTATUSEX)) };
      cbool memStatusValid     = GlobalMemoryStatusEx(&memStatus) ? true : false;
      ui64  bos;
      si16  l;
      ui8   m;

      resArray.records[0] = (resArray.blockSize[0] / recSize) & ~0x07ull;
      resArray.records[1] = (resArray.blockSize[1] / recSize) & ~0x07ull;

      for(m = 0; m < 2; ++m)
         if(threadCount[m] && !resArray.records[m]) {
            wprintf(wstrMessage[20], si64(resArray.blockSize[m]), si64(recSize << 3));
            return -18;
         }

      if(cfg.allocMem[0] <= 0 || (memStatusValid && cui64(cfg.allocMem[0]) > memStatus.ullAvailPhys)) {
         wprintf(wstrMessage[21], cfg.allocMem[0] >> 20, si64(memStatus.ullAvailPhys >> 20));
         return -17;
      }

      resArray.avx = (fl64x4ptrc)(resArray.avx512 = (fl64x8ptrc)(resArray.p = malloc64(cfg.allocMem[0])));
      resArray.alu = (si64ptrc)(resArray.fpu = (fl64ptrc)(resArray.sse = (fl64x2ptrc)resArray.avx));
      if(!resArray.p) {
         wprintf(wstrMessage[22], cfg.allocMem[0] >> 20);
         return -17;
      }

      // The ALU sub-array is placed after the vector sub-array, so its base advances past the vector records
      // of every thread of both classes; the element-size multiplier applies to that whole count
      cui64 vecRecords = resArray.records[0] * ui64(threadCount[0]) + resArray.records[1] * ui64(threadCount[1]);

      if(vecUnits) resArray.alu = (si64ptrc)resArray.p + vecRecords * vecUnits;

      for(k = 0, m = 0, bos = 0; m < 2; ++m)
         for(l = 0; l < threadCount[m]; ++k, ++l, bos += resArray.records[m]) {
            value[1][k].p0 = &resArray.avx512[bos];
            value[1][k].p1 = &resArray.avx[bos];
            value[1][k].p2 = &resArray.sse[bos];
            value[1][k].p3 = &resArray.fpu[bos];
            value[1][k].p4 = &resArray.alu[bos];

            if(cfg.procUnits & 0x010) SeedRecordsAVX512(value[1][k].p0, resArray.records[m], value[0][k].avx512);
            if(cfg.procUnits & 0x08)  SeedRecordsAVX   (value[1][k].p1, resArray.records[m], value[0][k].avx);
            if(cfg.procUnits & 0x04)  SeedRecordsSSE   (value[1][k].p2, resArray.records[m], value[0][k].sse);
            if(cfg.procUnits & 0x02)  SeedRecordsFPU   (value[1][k].p3, resArray.records[m], value[0][k].fpu);
            if(cfg.procUnits & 0x01)  SeedRecordsALU   (value[1][k].p4, resArray.records[m], value[0][k].alu);
         }
   }

   si32 bitmapIndent = 0;

   for(j = 0; wstrInterface[7][j]; ++j) bitmapIndent = (wstrInterface[7][j] == L'\n' ? 0 : bitmapIndent + 1);

   csi32 outChars = 4096 + si32(cfg.sys.groupCount) * (96 + bitmapIndent) + si32(threadCount[2]) * 1024;

   al64 declare1d64z(wchar, wstrOutput, outChars);

   cREPORT_BUFFER wstrOutputOwner = { wstrOutput };

   if(!wstrOutput) {
      wprintf(wstrMessage[22], (si64(outChars) * si64(sizeof(wchar))) >> 20);
      return -17;
   }

   wprintf(L"\n");

   // Output configuration properties
#pragma warning(push)
#pragma warning(disable:4996)
   c = swprintf(wstrOutput, wstrInterface[0]);
   for(i = 0, j = 0; i < 8; ++i) if(cfg.procUnits & (0x01ull << i)) { c += swprintf(&wstrOutput[c], L" %s", wstrUnitsCPU[i]); ++j; }
   for(; j < 3; j++) c += swprintf(&wstrOutput[c], L"    ");
   c += swprintf(&wstrOutput[c], wstrInterface[1], cfg.allocMem[0] >> 20, cfg.delayTime);
   if(cfg.procSync & 0x060) c += swprintf(&wstrOutput[c], wstrInterface[cfg.procSync & 0x020 ? 2 : 3], cfg.onTime);
   c += swprintf(&wstrOutput[c], wstrInterface[4]);
   for(i = 0, j = 0; i < 8; ++i) if(cfg.procSync & (0x01ull << i)) { c += swprintf(&wstrOutput[c], L" %s", wstrSyncCPU[i]); ++j; }
   for(; j < 3; j++) c += swprintf(&wstrOutput[c], L"    ");
   c += swprintf(&wstrOutput[c], wstrInterface[5], threadCount[2], (fl64(cfg.tics) / fl64(timer.siFrequency)));
   if(cfg.procSync & 0x020) c += swprintf(&wstrOutput[c], wstrInterface[6], cfg.offTime);
   c += swprintf(&wstrOutput[c], wstrInterface[7]);
   for(i = 0; i < cfg.sys.groupCount; ++i) {
      cui64 groupMap = cfg.sys.coreMap[0][i] | cfg.sys.coreMap[1][i];

      for(mask = 1; mask && mask <= groupMap; mask <<= 1)
         c += swprintf(&wstrOutput[c], L"%c", (mask & cfg.coreMap[i] ? '!' : '.'));

      c += swprintf(&wstrOutput[c], L"\n%*s", bitmapIndent, L"");
   }
#pragma warning(pop)

   c -= bitmapIndent;
   wprintf(L"%s", wstrOutput);
   wprintf(L"\n");

   timer.Update();

   // Spawn child processes
   for(d = 0, j = 0; j < 2; ++j) {
      ui8 coreGroup = 0;

      for(i = 0, mask = 1; i < threadCount[j]; ++d, ++i, mask <<= 1) {
         //threadData[d].packetSizeRAM = resArray.blockSize[j];
         threadData[d].startTics     = timer.siFrequency * cfg.delayTime / 1000 + timer.siCurrentTics;
         threadData[d].endTics       = threadData[d].startTics + cfg.tics;
         //threadData[d].maxTics       = cfg.tics;
         threadData[d].activeTics    = timer.siFrequency * si64(cfg.onTime) / 1000;
         threadData[d].cycleTics     = timer.siFrequency * si64(cfg.onTime + cfg.offTime) / 1000;
         //threadData[d].inactiveTime  = cfg.offTime;
         threadData[d].rc_tc         = (resArray.records[j] & 0x0FFFFFFFFFFFF) | (ui64(threadCount[2]) << 48);
         threadData[d].procUnits     = cfg.procUnits;
         threadData[d].procSync      = cfg.procSync;
         threadData[d].threadByte    = d >> 3;
         threadData[d].threadBit     = d & 0x07;

         SetThreadRunning(d); // Interlocked: a thread spawned earlier may be clearing this same byte

         threadHandle[d] = (HANDLE)_beginthreadex(0, 0, ComputationPulse, &threadData[d], CREATE_SUSPENDED, 0);

         GROUP_AFFINITY affinity = {}; // Its Reserved[3] must be zero; the initialiser is what guarantees it

         if(!threadHandle[d]) {
            ClearThreadRunning(d); ReleaseThreads(threadHandle, d); wprintf(wstrMessage[23], d); return -19;
         }

         if(NextSelectedCore(mask, coreGroup, ui8(j))) {
            affinity.Mask  = mask;
            affinity.Group = ui16(coreGroup);
         }

         if(!SetThreadGroupAffinity(threadHandle[d], &affinity, 0)) wprintf(wstrMessage[24], d);

         if(ResumeThread(threadHandle[d]) == DWORD(-1)) {
            ClearThreadRunning(d); ReleaseThreads(threadHandle, d + 1); wprintf(wstrMessage[23], d); return -19;
         }
      }
   }
   while(ThreadsRunning()) Sleep(100);

   JoinThreads(threadHandle, threadCount[2]);

   // Output results
   wprintf(L"\n");

#pragma warning(push)
#pragma warning(disable:4996)
   for(d = c, j = 0; j < 5; ++j) { // Cycle through each processing unit
      mask = 0x01ull << j;
      if(~cfg.procUnits & mask) continue;

      c += swprintf(&wstrOutput[c], wstrInterface[8]);
      switch(cfg.procUnits & mask) { default: i = 1; break; case 4: i = 5; break; case 8: i = 13; break; case 16: i = 29; }
      while(--i) c += swprintf(&wstrOutput[c], L"    ");
      c += swprintf(&wstrOutput[c], wstrInterface[9]);
      switch(cfg.procUnits & mask) { default: i = 5; break; case 4: i = 9; break; case 8: i = 17; break; case 16: i = 33; }
      while(--i) c += swprintf(&wstrOutput[c], L"----");
      c += swprintf(&wstrOutput[c], L"+--");
      switch(cfg.procUnits & mask) { default: i = 5; break; case 4: i = 9; break; case 8: i = 17; break; case 16: i = 33; }
      while(--i) c += swprintf(&wstrOutput[c], L"----");
      c += swprintf(&wstrOutput[c], L".");
      for(i = 0; i < threadCount[2]; ++i) {
         cui64ptrc expect = value[2][i].raw;
         cui64ptrc actual = value[3][i].raw;

         switch(cui8 bit = threadData[i].procUnits & mask) {
         default:
            c += swprintf(&wstrOutput[c], wstrInterface[18],
               i, wstrUnitsCPU[j], expect[16 - bit], actual[16 - bit], wstrPass[Evaluate(i, 5 - bit)]);
            break;
         case 4:
            c += swprintf(&wstrOutput[c], wstrInterface[19],
               i, expect[12], expect[13], actual[12], actual[13], wstrPass[Evaluate(i, 2)]);
            break;
         case 8:
            c += swprintf(&wstrOutput[c], wstrInterface[20],
               i, expect[8], expect[9], expect[10], expect[11], actual[8], actual[9], actual[10], actual[11], wstrPass[Evaluate(i, 1)]);
            break;
         case 16:
            c += swprintf(&wstrOutput[c], wstrInterface[21],
               i, expect[0], expect[1], expect[2], expect[3], expect[4], expect[5], expect[6], expect[7],
               actual[0], actual[1], actual[2], actual[3], actual[4], actual[5], actual[6], actual[7], wstrPass[Evaluate(i, 0)]);
         }
      }
      c+= swprintf(&wstrOutput[c], L"\n");
   }
   if(cfg.procSync & 0x080) { // Print benchmark results
      csi64 unitLanes = si64(cfg.procUnits & 0x010 ? 8 : cfg.procUnits & 0x08 ? 4 : cfg.procUnits & 0x04 ? 2 : cfg.procUnits & 0x02 ? 1 : 0) +
                        si64(cfg.procUnits & 0x01 ? 1 : 0);
      si64  accum     = 0;

      for(i = 0; i < threadCount[2]; ++i) accum += resArray.iter[i];

      c += swprintf(&wstrOutput[c], wstrInterface[10],
                    si64(fl64(accum) * fl64(unitLanes) * fl64(timer.siFrequency) / fl64(cfg.tics)) >> 10);
   }
   c += swprintf(&wstrOutput[c], L"\n");
#pragma warning(pop)

   // Write outputs to console and/or file
   wprintf(L"%s", &wstrOutput[d]);
   if(wstrOut[0]) {
      outFile = CreateFileW(wstrOut, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
      if(outFile == INVALID_HANDLE_VALUE) {
         wprintf(wstrMessage[10], wstrOut);
         return -9;
      }
      switch(outUTF) {
      case 1: // UTF-8
         if(!WriteBlock(outFile, outUTF8header, 3)) { wprintf(wstrMessage[11], wstrOut); CloseHandle(outFile); return -10; }
         break;
      case 2: // UTF-16
         if(!WriteBlock(outFile, &outUTF16header, 2)) { wprintf(wstrMessage[11], wstrOut); CloseHandle(outFile); return -10; }
      }

      if(outUTF == 2) { // UTF-16 encoding
         if(!WriteBlock(outFile, wstrOutput, ui32(c) * ui32(sizeof(wchar)))) {
            wprintf(wstrMessage[11], wstrOut);
            CloseHandle(outFile);
            return -10;
         }
      } else { // 8-bit encodings
         cui32 codePage  = (outUTF == 1 ? CP_UTF8 : CP_ACP);
         cbool ansiUTF8  = (outUTF != 1 && GetACP() == CP_UTF8);
         cui32 wcFlags   = (outUTF == 1 || ansiUTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS);
         BOOL  usedDef   = FALSE; // BOOL, as the API's own out-parameter type; null wherever UTF-8 resolves
         csi32 narrowLen = WideCharToMultiByte(codePage, wcFlags, wstrOutput, c, 0, 0, 0, 0);
         chptr strNarrow = (narrowLen > 0 ? (chptr)zalloc64(csize_t(narrowLen)) : 0);

         if(!strNarrow || WideCharToMultiByte(codePage, wcFlags, wstrOutput, c, strNarrow, narrowLen, 0,
                                              (outUTF == 1 || ansiUTF8 ? 0 : &usedDef)) != narrowLen ||
            !WriteBlock(outFile, strNarrow, ui32(narrowLen))) {
            wprintf(wstrMessage[11], wstrOut);
            mfree1(strNarrow);
            CloseHandle(outFile);
            return -10;
         }
         mfree1(strNarrow);
         if(usedDef) wprintf(wstrMessage[46], wstrOut);
      }
      wprintf(wstrMessage[0], wstrOut);
      CloseHandle(outFile);
   }

   return 0;
}
