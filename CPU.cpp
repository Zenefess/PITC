/************************************************************
 * File: CPU.cpp                        Created: 2025/01/21 *
 *                                    Last mod.: 2025/02/16 *
 *                                                          *
 * Desc: Pulsed integrity tests for CPUs.                   *
 *                                                          *
 * To do: *) Add R-R and STA scheduling                     *
 *        *) Add utilisation of caches                      *
 *        *) Expand core handling to >64 cores              *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#include "CPU.h"

#pragma warning(disable:4996)

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

// Evaluate integrity of results.
// unit==Processing unit (0=AVX512, 1=AVX2, 2=SSE4.1, 3=FPU, 4=ALU, -1=All)
static cui8 Evaluate(csi16 thread, csi8 unit) {
   ui8  index = unit == -1 ? 0  : 16 - (1 << (4 - unit));
   cui8 end   = unit == -1 ? 16 : (1 << max(0, 3 - unit)) + index;

   while(index < end)
      if(value[3][thread].raw[index] != value[2][thread].raw[index++])
         break;

   return index >= end ? 0 : 1;
}

// Generates output values
static void GenerateValues(ptr dataPtr) {
   const THREAD_CFGptrc tcfg = (THREAD_CFGptrc)dataPtr;

   cui64   coreNum    = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   cui8    threadBit  = 1u << tcfg->threadBit;
   RESULTS resultCopy = value[2][coreNum];
   ui16    i;

   value[2][coreNum] = value[3][coreNum];

   JobSSE((fl64x2 &)value[3][coreNum]._fl64[0]);
   JobSSE((fl64x2 &)value[3][coreNum]._fl64[2]);
   JobSSE((fl64x2 &)value[3][coreNum]._fl64[4]);
   JobSSE((fl64x2 &)value[3][coreNum]._fl64[6]);
   JobSSE((fl64x2 &)value[3][coreNum]._fl64[8]);
   JobSSE((fl64x2 &)value[3][coreNum]._fl64[10]);
   JobSSE(value[3][coreNum].sse);
   JobFPU(value[3][coreNum].fpu);
   JobALU(value[3][coreNum].alu);

   // Test computatational integrity
   for(i = 0, resultCopy = value[2][coreNum]; i < 65535; ++i) {
      JobSSE((fl64x2&)value[2][coreNum]._fl64[0]);
      JobSSE((fl64x2&)value[2][coreNum]._fl64[2]);
      JobSSE((fl64x2&)value[2][coreNum]._fl64[4]);
      JobSSE((fl64x2&)value[2][coreNum]._fl64[6]);
      JobSSE((fl64x2&)value[2][coreNum]._fl64[8]);
      JobSSE((fl64x2&)value[2][coreNum]._fl64[10]);
      JobSSE(value[2][coreNum].sse);
      JobFPU(value[2][coreNum].fpu);
      JobALU(value[2][coreNum].alu);

      if(Evaluate((si16)coreNum, -1) == 1) {
         if(!coreNum) value[3][0].raw[0] = value[2][0].raw[0] = 0x0AAAAAAAAAAAAAAAA;
         break;
      }

      if(!(i & 0x03FFF)) printf(".");

      value[2][coreNum] = resultCopy;
   }

   _InterlockedAnd8(&((chptr)threadBits)[tcfg->threadByte], threadBit);

   _endthread();
}

// Main body of program
csi32 wmain(csi32 argc, cwchptrc argv[]) {
   al64 declare1d64z(wchar, wstrOutput, 32768);
        declare1d64z(SYSTEM_LOGICAL_PROCESSOR_INFORMATION, sysLPI, MAX_THREADS * 2);
        ptr   outFile;
        ui64  mask;
        DWORD bytesProc = -1;
        int   c = 1, d;
        si16  threadCount[3] = { 0, 0, 0 }; // 0=Non-SMT, 1=SMT, 2=Total
        si16  i;
        ui8   j, k;
        ui8   procGroup = 0;
        ui8   lang      = 0;
        ui8   outUTF    = 0;

   GetLogicalProcessorInformation(sysLPI, &(bytesProc = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) * MAX_THREADS * 2));
   for(i = 0; (si32&)bytesProc > 0; ++i, bytesProc -= sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)) { ///--- Modify to account for >64 virtual cores !!!
      cui8 coreType = (PopulationCount64(sysLPI[i].ProcessorMask) > 1 ? 1 : 0);

      switch(sysLPI[i].Relationship) {
      case 0: // Processor core
         if(!(cfg.sys.coreMap[coreType][procGroup] & sysLPI[i].ProcessorMask)) ++cfg.sys.coreCount[coreType];
         cfg.sys.coreMap[coreType][procGroup] |= sysLPI[i].ProcessorMask;
         if(sysLPI[i].ProcessorCore.Flags) {
            cui8 SMT = (ui8)PopulationCount64(sysLPI[i].ProcessorMask);
            if(!cfg.sys.SMT || cfg.sys.SMT < SMT) // Set (maximum) SMT count per physical core
               cfg.sys.SMT = SMT;
         }
         break;
      case 1: // Numa node
         break;
      case 2: // Cache
         switch(sysLPI[i].Cache.Level) {
         case 1:
            if(sysLPI[i].Cache.Type == CacheInstruction) // Set (smallest) L1 code size
               if(!cfg.sys.cache[coreType].L1Code || cfg.sys.cache[coreType].L1Code > sysLPI[i].Cache.Size)
                  cfg.sys.cache[coreType].L1Code = sysLPI[i].Cache.Size;
            if(sysLPI[i].Cache.Type == CacheData) // Set (smallest) L1 code size
               if(!cfg.sys.cache[coreType].L1Data || cfg.sys.cache[coreType].L1Data > sysLPI[i].Cache.Size)
                  cfg.sys.cache[coreType].L1Data = sysLPI[i].Cache.Size;
            break;
         case 2:
            if(!cfg.sys.cache[coreType].L2 || cfg.sys.cache[coreType].L2 > sysLPI[i].Cache.Size) // Set (smallest) L2 size
               cfg.sys.cache[coreType].L2 = sysLPI[i].Cache.Size;
            break;
         case 3:
            if(!cfg.sys.cacheL3 || cfg.sys.cacheL3 > sysLPI[i].Cache.Size) // Set (smallest) L3 size
               cfg.sys.cacheL3 = sysLPI[i].Cache.Size;
            break;
         }
         break;
      case 3: // Processor package
         break;
      }
   }
   mfree1(sysLPI);
   cfg.sys.groupCount = ((cfg.sys.coreCount[0] + cfg.sys.coreCount[0] + 63) >> 6) + 1; ///--- Modify to account for >64 virtual cores !!!
   for(j = 0; j < cfg.sys.groupCount; ++j) cfg.coreMap[j] = cfg.sys.coreMap[0][j] | cfg.sys.coreMap[1][j];
   cfg.sys.cpuSSE4_1 = IsProcessorFeaturePresent(PF_SSE4_1_INSTRUCTIONS_AVAILABLE);
   cfg.sys.cpuAVX2   = IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE);
   cfg.sys.cpuAVX512 = IsProcessorFeaturePresent(PF_AVX512F_INSTRUCTIONS_AVAILABLE);

   cfg.tics = timer.siFrequency * 300; // Default to 5 minute duration

   if(argc > 1) {
      for(i = 1; i < argc; ++i) {
         switch(argv[i][0]) {
         case L'b': // Run benchmark: Constant computation, ALU + largest vector unit, all virtual cores utilised, L3 cache, 8MB memory per virtual core
         case L'B':
            cfg.memConfig   = 1;
            cfg.allocMem[0] = si64(cfg.sys.coreCount[1] * si16(cfg.sys.SMT) + cfg.sys.coreCount[0]) * 8192;
            cfg.procSync    = 0x012;
            cfg.procUnits = (cfg.sys.cpuAVX512 ? 0x091 : cfg.sys.cpuAVX2 ? 0x089 : 0x085);
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
               case L'v': // Execute AVX2 codepath
               case L'V':
                  cfg.procUnits |= 0x08;
                  break;
               case L'x': // Execute AVX512 codepath
               case L'X':
                  cfg.procUnits |= 0x010;
                  break;
               }
            }
            break;
         case L'l': // Set language
         case L'L':
            for(c = 1; argv[i][c] && argv[i][c] != L' ' && c < 1024; ++c);
            lstrcpynW(wstrLang, &argv[i][1], c);
            if(lstrcmpiW(wstrLang, L"0"))       lang = 0;
            if(lstrcmpiW(wstrLang, L"AUS"))     lang = 0;
            if(lstrcmpiW(wstrLang, L"ENG"))     lang = 0;
            if(lstrcmpiW(wstrLang, L"English")) lang = 0;
            break;
         case L'm': // Set amount of memory (in MB) to utilise during test
         case L'M':
            cfg.memConfig = 0;
            for(j = 1; argv[i][j] && argv[i][j] != L' '; ++j) {
               wchptr stopChar;
               switch(argv[i][j]) {
               case L'c': // For each virtual core
               case L'C':
                  cfg.memConfig   = 1;
                  cfg.allocMem[0] = si64(wcstol(&argv[i][j + 1], &stopChar, 10)) << 20;
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               case L'n': // For each non-SMT core
               case L'N':
                  cfg.memConfig   = 2;
                  cfg.allocMem[0] = si64(wcstol(&argv[i][j + 1], &stopChar, 10)) << 20;
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               case L's': // For each SMT virtual core
               case L'S':
                  cfg.memConfig   = 2;
                  cfg.allocMem[1] = si64(wcstol(&argv[i][j + 1], &stopChar, 10)) << 20;
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               case L't': // Equally split amongst all utilised virtual cores
               case L'T':
                  cfg.memConfig   = 0;
                  cfg.allocMem[0] = si64(wcstol(&argv[i][j + 1], &stopChar, 10)) << 20;
                  j += ui8(stopChar - &argv[i][j] - 1);
               }
            }
            break;
         case L'o': // Output results to file
         case L'O':
            for(c = 1; argv[i][c] && argv[i][c] != ' '; ++c) {
               switch(argv[i][c]) {
               case L'1':
                  if(argv[i][++c] == L'6') outUTF = 2;
                  break;
               case L'8':
                  outUTF = 1;
                  break;
               case L'a':
               case L'A':
                  outUTF = 0;
                  break;
               case L'[':
                  for(d = ++c; argv[i][c] && argv[i][c] != L']' && c < 1024; ++c);
                  if(!lstrcpynW(wstrOut, &argv[i][d], c++ - 1)) {
                     wprintf(L"\n\nUnable to create file \"%s\".\n\n", wstrOut);
                     return -32;
                  }
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
               }
            }
            break;
         case L't': // Set timing options
         case L'T':
            cfg.procSync &= 0x0F;
            for(j = 1; argv[i][j] && argv[i][j] != L' '; ++j) {
               wchptr stopChar;
               switch(argv[i][j]) {
               case L'[': // Fixed pulse on-time
                  cfg.onTime = ui32(wcstol(&argv[i][j + 1], &stopChar, 10));
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               case L']': // Fixed pulse off-time
                  cfg.offTime = ui32(wcstol(&argv[i][j + 1], &stopChar, 10));
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               case L'c': // Constant thread execution
               case L'C':
                  cfg.procSync |= 0x010;
                  break;
               case L'd': // Set start-up delay
               case L'D':
                  cfg.delayTime = ui32(wcstod(&argv[i][j + 1], &stopChar) * 1000.0);
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               case L'f': // Fixed pulse-width thread execution
               case L'F':
                  cfg.procSync |= 0x020;
                  break;
               case L's': // Sweeping pulse-width thread execution
               case L'S':
                  cfg.procSync |= 0x040;
                  break;
               case L't': // Test duration
               case L'T':
                  cfg.tics = si64((fl64)timer.siFrequency * wcstod(&argv[i][j + 1], &stopChar));
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               }
            }
            break;
         case L'u': // Set core usage options
         case L'U':
            for(j = 1; argv[i][j] && argv[i][j] != L' '; ++j) {
               switch(argv[i][j]) {
               case L'c': // Binary sequence map of physical cores to utilise (eg. x..x.xxx)
               case L'C':
                  for(++j; argv[i][j] && argv[i][j] != L' '; ++j) {
                     switch(argv[i][j]) {
                     case L'.': // Physical core not to be utilised
                     case L',':
                     case L'_':
                        cfg.coreMap[(j - 1) >> 3] &= ~(0x03ull << (j << 1)); ///--- Modify to account for non-SMT CPUs !!!
                        break;
                     default:  // Physical core to be utilised
                        cfg.coreMap[(j - 1) >> 3] |= 0x03ull << (j << 1); ///--- Modify to account for non-SMT CPUs !!!
                        break;
                     }
                  }
                  break;
               case L'e': // Only utilise the first virtual core of each active physical core
               case L'E':
                  cfg.SMTLoad = 1;
                  break;
               case L'o': // Only utilise the last virtual core of each active physical core
               case L'O':
                  cfg.SMTLoad = 2;
                  break;
               case L's': // Generate threads for every virtual core
               case L'S':
                  cfg.SMTLoad = 3;
                  break;
               case L't': // Binary sequence map of virtual cores to utilise (eg. xx..x.x...xx.xxx)
               case L'T':
                  for(++j; argv[i][j] && argv[i][j] != L' '; ++j) {
                     switch(argv[i][j]) {
                     case L'.': // Virtual core not to be utilised
                     case L',':
                     case L'_':
                        cfg.coreMap[(j - 1) >> 3] &= ~(0x01ull << j);
                        break;
                     default:  // Virtual core to be utilised
                        cfg.coreMap[(j - 1) >> 3] |= 0x01ull << j;
                        break;
                     }
                  }
                  break;
               }
            }
            break;
         case L'W': // Write new "cpu.values" file
            union { ui64 _64; ui32 _32[2]; } randNum;

            for(i = 0; i < MAX_THREADS; ++i) {
               for(j = 0; j < 15; ++j) {
                  rand_s(randNum._32); rand_s(&randNum._32[1]);
                  value[0][i]._fl64[j] = fl64(randNum._64) / 2048.0;
               }
               rand_s(&value[0][i].raw32[30]); rand_s(&value[0][i].raw32[31]);
            }
            memcpy_s(value[1], RESULTS_BUF_SIZE, value[0], RESULTS_BUF_SIZE);

            for(i = 0; i < MAX_THREADS; ++i) {
               threadBits[i >> 6] |= 1uLL << (i & 0x03F);

               threadData[i].threadByte = i >> 3;
               threadData[i].threadBit  = i & 0x07;
               ptrc handle = (ptr)_beginthread(GenerateValues, 0, &threadData[i]);
            }
            while(ThreadsRunning) Sleep(100);

            // Test for computational error
            if(value[3][0].raw[0] == 0x0AAAAAAAAAAAAAAAA && value[2][0].raw[0] == 0x0AAAAAAAAAAAAAAAA) {
               printf("\n\nComputational error(s) detected. Results not written.\n\n");
               return -16;
            }

            outFile = CreateFileW(L"cpu.values", GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            if(outFile == INVALID_HANDLE_VALUE) {
               printf("\n\nCannot create \"cpu.values\" file.\n\n");
               return -17;
            }
            if(!WriteFile(outFile, value[0], RESULTS_BUF_SIZE, &bytesProc, 0)) {
               printf("\n\nFailed to write all input entries to \"cpu.values\" file.\n\n");
               return -18;
            }
            if(!WriteFile(outFile, value[3], RESULTS_BUF_SIZE, &bytesProc, 0)) {
               printf("\n\nFailed to write all output entries to \"cpu.values\" file.\n\n");
               return -19;
            }

            printf("\n\nNew \"cpu.values\" file generated.\n\n");

            CloseHandle(outFile);

            return 1;
         case L'-': // Configuration presets
            cfg.memConfig   = 1;
            cfg.allocMem[0] = 8388608;
            cfg.procUnits   = (cfg.sys.cpuAVX512 ? 0x091 : cfg.sys.cpuAVX2 ? 0x089 : 0x085);
            switch(argv[i][1]) {
            case L'1': // Constant stress; one thread per physical core
               cfg.procSync = 0x012;
               cfg.SMTLoad  = 2;
               break;
            case L'2': // Constant stress on all virtual cores
               cfg.procSync = 0x012;
               cfg.SMTLoad  = 3;
               break;
            case L'3': // Fixed-width round-robin pulsed stress; one thread per physical core
               cfg.procSync = 0x021;
               cfg.SMTLoad  = 2;
               break;
            case L'4': // Synchronised fixed-width pulsed stress; one thread per physical core
               cfg.procSync = 0x02A;
               cfg.SMTLoad  = 2;
               break;
            case L'5': // Synchronised fixed-width pulsed stress on all virtual cores
               cfg.procSync = 0x02A;
               cfg.SMTLoad  = 3;
               break;
            case L'6': // Synchronised sweeping-width pulsed stress; one thread per physical core
               cfg.procSync = 0x04A;
               cfg.SMTLoad  = 2;
               break;
            case L'7': // Synchronised staggered fixed-width pulsed stress; one thread per physical core
               cfg.procSync = 0x02C;
               cfg.SMTLoad  = 2;
               break;
            case L'8': // Synchronised staggered fixed-width pulsed stress on all virtual cores
               cfg.procSync = 0x02C;
               cfg.SMTLoad  = 3;
               break;
            }
            break;
         }
      }
   } else { // Display instructions
    wprintf(wstrInstructions[lang]);

    return 2;
   }

   outFile = CreateFileW(L"cpu.values", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, 0);
   if(outFile == INVALID_HANDLE_VALUE) {
      printf("\n\n\"cpu.values\" file not found.\n\n");
      return -1;
   }
   if(!ReadFile(outFile, value[0], RESULTS_BUF_SIZE, &bytesProc, 0)) {
      printf("\n\nInsufficient input entries in \"cpu.values\" file.\n\n");
      return -2;
   }
   memcpy_s(value[1], RESULTS_BUF_SIZE, value[0], RESULTS_BUF_SIZE);
   if(!ReadFile(outFile, value[2], RESULTS_BUF_SIZE, &bytesProc, 0)) {
      printf("\n\nInsufficient output entries in \"cpu.values\" file.\n\n");
      return -3;
   }
   memcpy_s(value[3], RESULTS_BUF_SIZE, value[2], RESULTS_BUF_SIZE);
   CloseHandle(outFile);

   // Count virtual cores to be used
   SetSMTLoading();
   for(j = 0; j < cfg.sys.groupCount; ++j) {
      threadCount[0] += (si16)PopulationCount64(cfg.sys.coreMap[0][j] & cfg.coreMap[j]);
      threadCount[1] += (si16)PopulationCount64(cfg.sys.coreMap[1][j] & cfg.coreMap[j]);
   }
   threadCount[2] = threadCount[0] + threadCount[1];

   timer.Update();

   // Prepare results output file
   if(wstrOut[0]) {
      outFile = CreateFileW(wstrOut, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
      if(outFile == INVALID_HANDLE_VALUE) {
         wprintf(L"\n\nCannot create \"%s\" file.\n\n", wstrOut);
         return -64;
      }
      switch(outUTF) {
      case 1: // UTF-8
         WriteFile(outFile, outUTF8header, 3, &bytesProc, 0);
         break;
      case 2: // UTF-16
         WriteFile(outFile, &outUTF16header, 2, &bytesProc, 0);
      }
   }

   ///--- REWRITE ---///
   // Memory allocation and pointer configuration
   switch(cfg.memConfig) {
   case 0: // Total memory
      memArray.blockSize[0] = memArray.blockSize[1] = cfg.allocMem[0] / (cui64)threadCount[2];
      break;
   case 1: // Memory per core
      memArray.blockSize[0] = memArray.blockSize[1] = cfg.allocMem[0];
      cfg.allocMem[0] *= (cui64)threadCount[2];
      break;
   case 2: // Separate non-SMT/SMT
      memArray.blockSize[0] = cfg.allocMem[0];
      memArray.blockSize[1] = cfg.allocMem[1];
      cfg.allocMem[0] = memArray.blockSize[0] * cfg.sys.coreCount[0] + (memArray.blockSize[1] * cfg.sys.coreCount[1] * cfg.sys.SMT);
   }

   if(memArray.blockSize[0]) {
      ui64 os, bos;
      ui8  l, m;

      memArray.p = malloc64(cfg.allocMem[0]);
      switch(cfg.procUnits & 0x01F) {
      case 1:
         memArray.records[0] = ui32(memArray.blockSize[0] / 8);
         memArray.records[1] = ui32(memArray.blockSize[1] / 8);
         memArray.alu[0]     = (si64ptrc)memArray.p;
         break;
      case 2:
         memArray.records[0] = ui32(memArray.blockSize[0] / 8);
         memArray.records[1] = ui32(memArray.blockSize[1] / 8);
         memArray.fpu        = (fl64ptrc)memArray.p;
         break;
      case 3:
         memArray.records[0] = ui32(memArray.blockSize[0] / 16);
         memArray.records[1] = ui32(memArray.blockSize[1] / 16);
         memArray.fpu        = (fl64ptrc)memArray.p;
         memArray.alu[0]     = (si64ptrc)memArray.p + (memArray.records[0] * threadCount[0]);
         memArray.alu[1]     = memArray.alu[0] + (memArray.records[1] * threadCount[1]);
         break;
      case 4: case 6:
         memArray.records[0] = ui32(memArray.blockSize[0] / 16);
         memArray.records[1] = ui32(memArray.blockSize[1] / 16);
         memArray.sse        = (fl64x2ptrc)memArray.p;
         break;
      case 5: case 7:
         memArray.records[0] = ui32(memArray.blockSize[0] / 24);
         memArray.records[1] = ui32(memArray.blockSize[1] / 24);
         memArray.sse        = (fl64x2ptrc)memArray.p;
         memArray.alu[0]     = (si64ptrc)memArray.p + (memArray.records[0] * 2 * threadCount[0]);
         memArray.alu[1]     = memArray.alu[0] + (memArray.records[1] * 2 * threadCount[1]);
         break;
      case 8: case 10: case 12: case 14:
         memArray.records[0] = ui32(memArray.blockSize[0] / 32);
         memArray.records[1] = ui32(memArray.blockSize[1] / 32);
         memArray.avx        = (fl64x4ptrc)memArray.p;
         break;
      case 9: case 11: case 13: case 15:
         memArray.records[0] = ui32(memArray.blockSize[0] / 40);
         memArray.records[1] = ui32(memArray.blockSize[1] / 40);
         memArray.avx        = (fl64x4ptrc)memArray.p;
         memArray.alu[0]     = (si64ptrc)memArray.p + (memArray.records[0] * 4 * threadCount[0]);
         memArray.alu[1]     = memArray.alu[0] + (memArray.records[1] * 4 * threadCount[1]);
         break;
      case 16: case 18: case 20: case 22: case 24: case 26: case 28: case 30:
         memArray.records[0] = ui32(memArray.blockSize[0] / 64);
         memArray.records[1] = ui32(memArray.blockSize[1] / 64);
         memArray.avx512     = (fl64x8ptrc)memArray.p;
         break;
      case 17: case 19: case 21: case 23: case 25: case 27: case 29: case 31:
         memArray.records[0] = ui32(memArray.blockSize[0] / 72);
         memArray.records[1] = ui32(memArray.blockSize[1] / 72);
         memArray.avx512     = (fl64x8ptrc)memArray.p;
         memArray.alu[0]     = (si64ptrc)memArray.p + (memArray.records[0] * 8 * threadCount[0]);
         memArray.alu[1]     = memArray.alu[0] + (memArray.records[1] * 8 * threadCount[1]);
      }

      for(k = 0, m = 0; m < 2; ++m)
         for(l = 0, bos = 0; l < threadCount[m]; ++k, ++l, bos += memArray.records[m]) {
            value[1][k].p0 = &memArray.avx512[bos];
            value[1][k].p1 = &memArray.avx[bos];
            value[1][k].p2 = &memArray.sse[bos];
            value[1][k].p3 = &memArray.fpu[bos];
            value[1][k].p4 = &memArray.alu[m][bos];
            for(os = 0; os < memArray.records[m]; ++os) {
               if(cfg.procUnits & 0x010) value[1][k].p0[os] = value[0][k].avx512;
               if(cfg.procUnits & 0x08)  value[1][k].p1[os] = value[0][k].avx;
               if(cfg.procUnits & 0x04)  value[1][k].p2[os] = value[0][k].sse;
               if(cfg.procUnits & 0x02)  value[1][k].p3[os] = value[0][k].fpu;
               if(cfg.procUnits & 0x01)  value[1][k].p4[os] = value[0][k].alu;
            }
         }
   }
   ///--- REWRITE ---///

   // Output configuration properties
   c = swprintf(wstrOutput, L"Units:");
   for(i = 0; i < 8; ++i)
      if(cfg.procUnits & (0x01ull << i)) c += swprintf(&wstrOutput[c], L" %s", wstrUnitsCPU[i]);
   c += swprintf(&wstrOutput[c], L"\t\tStart-up delay: %7dms\t    Thread count: %d", cfg.delayTime, threadCount[2]);
   switch(cfg.procSync >> 4) {
   default:
      c += swprintf(&wstrOutput[c], L"\t\t");
      break;
   case 2:
      c += swprintf(&wstrOutput[c], L"Pulse on-time: %4dms", cfg.onTime);
      break;
   case 4:
      c += swprintf(&wstrOutput[c], L"Cycle time: %dms", cfg.onTime);
   }
   c += swprintf(&wstrOutput[c], L"\nSync: ");
   for(i = 0; i < 8; ++i)
      if(cfg.procSync & (0x01ull << i)) c += swprintf(&wstrOutput[c], L" %s", wstrSyncCPU[i]);
   c += swprintf(&wstrOutput[c], L"\tMaximum duration: %5.1fs\tMemory allocated: %lldMB", (fl64(cfg.tics) / fl64(timer.siFrequency)), (cfg.allocMem[0] + cfg.allocMem[1]) >> 20);
   if(cfg.procSync >> 4 == 2) c += swprintf(&wstrOutput[c], L"Pulse off-time: %3dms", cfg.offTime);
   c += swprintf(&wstrOutput[c], L"\n\nThread bitmap: ");
   for(i = 0; i < cfg.sys.groupCount; ++i) {
      for(mask = 1, d = threadCount[2]; mask && d; mask <<= 1, --d)
         c += swprintf(&wstrOutput[c], L"%c", (mask & cfg.coreMap[i] ? '!' : '.'));
      c += swprintf(&wstrOutput[c], L"\n               ");
   }
   d = c -= 15;
   wprintf(wstrOutput);
   wprintf(L"\n");

   BitScanForward(&bytesProc, cfg.procSync >> 4);
   k = (ui8)bytesProc;

   // Spawn child processes
   for(c = 0, j = 0; j < 2; ++j)
      for(i = 0, mask = 1; i < threadCount[j]; ++c, ++i, mask <<= 1) {
         threadData[c].maxDuration   = cfg.tics;
         threadData[c].startTime     = cfg.delayTime * timer.siFrequency / 1000 + timer.siCurrentTics;
         threadData[c].endTime       = threadData[c].startTime + threadData[c].maxDuration;
         threadData[c].activeTime    = cfg.onTime;
         threadData[c].inactiveTime  = cfg.offTime;
         threadData[c].packetSizeRAM = memArray.blockSize[j];
         threadData[c].recordCount   = memArray.records[j];
         threadData[c].procUnits     = cfg.procUnits;
         threadData[c].procSync      = cfg.procSync;
         threadData[c].threadByte    = c >> 3;
         threadData[c].threadBit     = c & 0x07;

         threadBits[c >> 6] |= 1uLL << (c & 0x03F);

         ptrc handle = (ptr)_beginthread(MethodCPU[k], 0, &threadData[c]);

         while(mask & ~cfg.coreMap[procGroup]) mask <<= 1; ///--- Modify to account for >64 virtual cores !!!
         SetThreadAffinityMask(handle, mask);
      }
   while(ThreadsRunning) Sleep(100);

   // Output results
   wprintf(L"\n");
   for(j = 0; j < 5; ++j) { // Cycle through each processing unit
      mask = 0x01ull << j;
      if(~cfg.procUnits & mask) continue;

      c += swprintf(&wstrOutput[c], L"\n Thread | ProcUnit | Correct values   ");
      switch(cfg.procUnits & mask) { default: i = 1; break; case 4: i = 5; break; case 8: i = 13; break; case 16: i = 29; } while(--i) c += swprintf(&wstrOutput[c], L"    ");
      c += swprintf(&wstrOutput[c], L"| Result\n--------+----------+--");
      switch(cfg.procUnits & mask) { default: i = 5; break; case 4: i = 9; break; case 8: i = 17; break; case 16: i = 33; } while(--i) c += swprintf(&wstrOutput[c], L"----");
      c += swprintf(&wstrOutput[c], L"+--");
      switch(cfg.procUnits & mask) { default: i = 5; break; case 4: i = 9; break; case 8: i = 17; break; case 16: i = 33; } while(--i) c += swprintf(&wstrOutput[c], L"----");
      c += swprintf(&wstrOutput[c], L".");
      for(i = 0; i < threadCount[2]; ++i) {
         switch(cui8 bit = threadData[i].procUnits & mask) {
         default:
            c += swprintf(&wstrOutput[c], L"\n  #%3.1d  |  %s 64  | %16.16llX | %16.16llX | %s", i, wstrUnitsCPU[j], value[2][i].raw[16 - bit], value[3][i].raw[16 - bit], wstrPass[Evaluate(i, 5 - bit)]);
            break;
         case 4:
            c += swprintf(&wstrOutput[c], L"\n  #%3.1d  | SSE  128 | %16.16llX%16.16llX | %16.16llX%16.16llX | %s",
               i, value[2][i].raw[12], value[2][i].raw[13], value[3][i].raw[12], value[3][i].raw[13], wstrPass[Evaluate(i, 2)]);
            break;
         case 8:
            c += swprintf(&wstrOutput[c], L"\n  #%3.1d  | AVX  256 | %16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX | %s",
               i, value[2][i].raw[8], value[2][i].raw[9], value[2][i].raw[10], value[2][i].raw[11], value[3][i].raw[8], value[3][i].raw[9], value[3][i].raw[10], value[3][i].raw[11], wstrPass[Evaluate(i, 1)]);
            break;
         case 10:
            c += swprintf(&wstrOutput[c], L"\n  #%3.1d  | AVX  512 | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %s",
               i, value[2][i].raw[0], value[2][i].raw[1], value[2][i].raw[2], value[2][i].raw[3], value[2][i].raw[4], value[2][i].raw[5], value[2][i].raw[6], value[2][i].raw[7],
               value[3][i].raw[0], value[3][i].raw[1], value[3][i].raw[2], value[3][i].raw[3], value[3][i].raw[4], value[3][i].raw[5], value[3][i].raw[6], value[3][i].raw[7], wstrPass[Evaluate(i, 0)]);
         }
      }
      c+= swprintf(&wstrOutput[c], L"\n");
   }
   c += swprintf(&wstrOutput[c], L"\n");

   // Write outputs to console and/or file
   wprintf(&wstrOutput[d]);
   if(wstrOut[0]) {
      if(outUTF == 2) { // UTF-16 encoding
         if(!WriteFile(outFile, wstrOutput, c, &bytesProc, 0)) {
            wprintf(L"\n\nFailed to write results to \"%s\" file.\n\n", wstrOut);
            CloseHandle(outFile);
            return -65;
         }
      } else { // 8-bit encodings
         setlocale(LC_ALL, "");
         wcstombs((chptr)&wstrOutput[c], wstrOutput, c);
         if(!WriteFile(outFile, (chptr)&wstrOutput[c], c, &bytesProc, 0)) {
            wprintf(L"\n\nFailed to write results to \"%s\" file.\n\n", wstrOut);
            CloseHandle(outFile);
            return -65;
         }
      }
      wprintf(L"\n\nSuccessfully wrote results to \"%s\" file.\n\n", wstrOut);
      CloseHandle(outFile);
   }

   return 0;
}
