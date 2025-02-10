/************************************************************
 * File: CPU.cpp                        Created: 2025/01/21 *
 *                                    Last mod.: 2025/02/10 *
 *                                                          *
 * Desc: Pulsed integrity tests for CPUs.                   *
 *                                                          *
 * To do: *) Add pulse sweep functionality                  *
 *        *) Add utilisation of caches                      *
 *        *) Add memory processing                          *
 *        *) Expand core handling to >64 cores              *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#include "CPU.h"

#pragma warning(disable:4996)

// Force 1 thread per SMT core
static void OnePerSMT(void) {
   for(ui8 i = 0; i < cfg.sys.groupCount; ++i) cfg.coreMap[i] &= ~((cfg.coreMap[i] & cfg.sys.coreMap[1][i]) << 1);
}

// Force maximum threads per SMT core
static void MaxPerSMT(void) {
   for(ui8 i = 0; i < cfg.sys.groupCount; ++i) cfg.coreMap[i] |= (cfg.coreMap[i] & cfg.sys.coreMap[1][i]) << 1;
}

// Evaluate integrity of results
static cchptrc Evaluate(csi16 thread, cui8 index, ui8 size) {
   bool success = true;

   do if(value[1][thread].raw[index + --size] != value[2][thread].raw[index + size]) success = false;
   while(size);

   return success ? strPass[0] : strPass[1];
}

// Evaluate integrity of results
static cwchptrc EvaluateW(csi16 thread, cui8 index, ui8 size) {
   bool success = true;

   do if(value[1][thread].raw[index + --size] != value[2][thread].raw[index + size]) success = false;
   while(size);

   return success ? wstrPass[0] : wstrPass[1];
}

// Generates output values
static void GenerateValues(ptr dataPtr) {
   const THREAD_CFG *const tcfg = (THREAD_CFG *)dataPtr;

   cui64   coreNum    = (cui64(tcfg->threadByte) << 3) + tcfg->threadBit;
   cui8    threadBit  = 1u << tcfg->threadBit;
   RESULTS resultCopy = value[2][coreNum];
   ui16    i;

   value[2][coreNum] = value[1][coreNum];

   JobSSE((fl64x2 &)value[1][coreNum]._fl64[0]);
   JobSSE((fl64x2 &)value[1][coreNum]._fl64[2]);
   JobSSE((fl64x2 &)value[1][coreNum]._fl64[4]);
   JobSSE((fl64x2 &)value[1][coreNum]._fl64[6]);
   JobSSE((fl64x2 &)value[1][coreNum]._fl64[8]);
   JobSSE((fl64x2 &)value[1][coreNum]._fl64[10]);
   JobSSE(value[1][coreNum].sse);
   JobFPU(value[1][coreNum].fpu);
   JobALU(value[1][coreNum].alu);

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

      if(EvaluateW((si16)coreNum, 0, 16) == wstrPass[1]) {
         if(!coreNum) value[1][0].raw[0] = value[2][0].raw[0] = 0x0AAAAAAAAAAAAAAAA;
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
   al64 declare1d64z(SYSTEM_LOGICAL_PROCESSOR_INFORMATION, sysLPI, MAX_THREADS * 2);
        ptr   outFile;
        ui64  mask;
        DWORD bytesProc = -1;
        int   c = 1;
        si16  i;
        ui8   j;
        ui8   procGroup   = 0;
        si16  threadCount = 0;
        ui8   lang        = 0;

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
         case 'b': // Run benchmark
         case 'B':
            cfg.allocMem  = 256;
            cfg.procSync  = 0x012;
            cfg.procUnits = (cfg.sys.cpuAVX512 ? 0x091 : cfg.sys.cpuAVX2 ? 0x089 : 0x085);
            break;
         case 'i': // Set instruction usage options
         case 'I':
            cfg.procUnits = 0;
            for(j = 1; argv[i][j] && argv[i][j] != 32; ++j) {
               switch(argv[i][j]) {
               case '1': // Process dataset for L1 cache
                  cfg.procUnits |= 0x020;
                  break;
               case '2': // Process dataset for L2 cache
                  cfg.procUnits |= 0x040;
                  break;
               case '3': // Process dataset for L3 cache
                  cfg.procUnits |= 0x080;
                  break;
               case 'a': // Execute ALU codepath
               case 'A':
                  cfg.procUnits |= 0x01;
                  break;
               case 'f': // Execute FPU codepath
               case 'F':
                  cfg.procUnits |= 0x02;
                  break;
               case 's': // Execute SSE codepath
               case 'S':
                  cfg.procUnits |= 0x04;
                  break;
               case 'v': // Execute AVX2 codepath
               case 'V':
                  cfg.procUnits |= 0x08;
                  break;
               case 'x': // Execute AVX512 codepath
               case 'X':
                  cfg.procUnits |= 0x010;
                  break;
               }
            }
            break;
         case 'l': // Set language
         case 'L':
            while(argv[i][c] && argv[i][c] != ' ' && c < 1024) ++c;
            lstrcpynW(wstrLang, &argv[i][1], c);
            if(lstrcmpiW(wstrLang, L"0"))       lang = 0;
            if(lstrcmpiW(wstrLang, L"AUS"))     lang = 0;
            if(lstrcmpiW(wstrLang, L"ENG"))     lang = 0;
            if(lstrcmpiW(wstrLang, L"English")) lang = 0;
            break;
         case 'm': // Set amount of memory (in MB) to utilise during test
         case 'M':
            cfg.allocMem = wcstol(&argv[i][1], 0, 10);
            break;
         case 'o': // Output results to file   ---   !!! TO DO !!!
         case 'O':
            while(argv[i][c] && argv[i][c] != ' ' && c < 1024) ++c;
            if(!lstrcpynW(wstrOut, &argv[i][1], c)) {
               wprintf(L"\n\nUnable to create file \"%s\".\n\n", wstrOut);
               return -32;
            };
            break;
         case 's': // Set core synchronisation options
         case 'S':
            cfg.procSync = 0;
            for(j = 1; argv[i][j] && argv[i][j] != 32; ++j) {
               switch(argv[i][j]) {
               case 'p': // Parallel thread execution
               case 'P':
                  cfg.procSync |= 0x02;
                  break;
               case 'r': // Round-robin thread execution
               case 'R':
                  cfg.procSync |= 0x01;
                  break;
               case 's': // Staggered thread execution
               case 'S':
                  cfg.procSync |= 0x04;
                  break;
               case 't': // Time-synchronised execution
               case 'T':
                  cfg.procSync |= 0x08;
                  break;
               }
            }
            break;
         case 't': // Set timing options
         case 'T':
            for(j = 1; argv[i][j] && argv[i][j] != 32; ++j) {
               wchptr stopChar;
               switch(argv[i][j]) {
               case '[': // Fixed pulse on-time
                  cfg.onTime = ui32(wcstol(&argv[i][j + 1], &stopChar, 10));
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               case ']': // Fixed pulse off-time
                  cfg.offTime = ui32(wcstol(&argv[i][j + 1], &stopChar, 10));
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               case 'c': // Constant thread execution
               case 'C':
                  cfg.procSync |= 0x010;
                  break;
               case 'd': // Set start-up delay
               case 'D':
                  cfg.delayTime = ui32(wcstod(&argv[i][j + 1], &stopChar) * 1000.0);
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               case 'f': // Fixed pulse-width thread execution
               case 'F':
                  cfg.procSync |= 0x020;
                  break;
               case 's': // Sweeping pulse-width thread execution
               case 'S':
                  cfg.procSync |= 0x040;
                  break;
               case 't': // Test duration
               case 'T':
                  cfg.tics = si64((fl64)timer.siFrequency * wcstod(&argv[i][j + 1], &stopChar));
                  j += ui8(stopChar - &argv[i][j] - 1);
                  break;
               }
            }
            break;
         case 'u': // Set core usage options
         case 'U':
            for(j = 1; argv[i][j] && argv[i][j] != 32; ++j) {
               switch(argv[i][j]) {
               case 'c': // Binary sequence map of physical cores to utilise (eg. x..x.xxx)
               case 'C':
                  for(++j; argv[i][j] && argv[i][j] != 32; ++j) {
                     switch(argv[i][j]) {
                     case '.': // Physical core not to be utilised
                     case ',':
                     case '_':
                        cfg.coreMap[(j - 1) >> 3] &= ~(0x03ull << (j << 1)); ///--- Modify to account for non-SMT CPUs !!!
                        break;
                     default:  // Physical core to be utilised
                        cfg.coreMap[(j - 1) >> 3] |= 0x03ull << (j << 1); ///--- Modify to account for non-SMT CPUs !!!
                        break;
                     }
                  }
                  break;
               case 's': // Generate threads for every virtual core (ie. SMT)
               case 'S':
                  cfg.SMT = true;
                  break;
               case 't': // Binary sequence map of virtual cores to utilise (eg. xx..x.x...xx.xxx)
               case 'T':
                  for(++j; argv[i][j] && argv[i][j] != 32; ++j) {
                     switch(argv[i][j]) {
                     case '.': // Virtual core not to be utilised
                     case ',':
                     case '_':
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
            if(!cfg.SMT) OnePerSMT(); else MaxPerSMT();
            break;
         case '~': // Write new "cpu.values" file
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
            if(value[1][0].raw[0] == 0x0AAAAAAAAAAAAAAAA && value[2][0].raw[0] == 0x0AAAAAAAAAAAAAAAA) {
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
            if(!WriteFile(outFile, value[1], RESULTS_BUF_SIZE, &bytesProc, 0)) {
               printf("\n\nFailed to write all output entries to \"cpu.values\" file.\n\n");
               return -19;
            }

            printf("\n\nNew \"cpu.values\" file generated.\n\n");

            CloseHandle(outFile);

            return 1;
         case '-': // Configuration presets
            cfg.allocMem  = 1024;
            cfg.procUnits = (cfg.sys.cpuAVX512 ? 0x091 : cfg.sys.cpuAVX2 ? 0x089 : 0x085);
            switch(argv[i][1]) {
            case '1': // Constant stress; one thread per physical core
               cfg.procSync = 0x012;
               OnePerSMT();
               break;
            case '2': // Constant stress on all virtual cores
               cfg.procSync = 0x012;
               MaxPerSMT();
               break;
            case '3': // Fixed-width round-robin pulsed stress; one thread per physical core
               cfg.procSync = 0x021;
               OnePerSMT();
               break;
            case '4': // Synchronised fixed-width pulsed stress; one thread per physical core
               cfg.procSync = 0x02A;
               OnePerSMT();
               break;
            case '5': // Synchronised fixed-width pulsed stress on all virtual cores
               cfg.procSync = 0x02A;
               MaxPerSMT();
               break;
            case '6': // Synchronised sweeping-width pulsed stress; one thread per physical core
               cfg.procSync = 0x04A;
               OnePerSMT();
               break;
            case '7': // Synchronised staggered fixed-width pulsed stress; one thread per physical core
               cfg.procSync = 0x02C;
               OnePerSMT();
               break;
            case '8': // Synchronised staggered fixed-width pulsed stress on all virtual cores
               cfg.procSync = 0x02C;
               MaxPerSMT();
               break;
            }
            break;
         }
      }
   } else { // Display instructions
    wprintf(wstrInstructions[lang]);

    return 16;
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
   CloseHandle(outFile);

   for(j = 0; j < cfg.sys.groupCount; ++j) threadCount += (si16)PopulationCount64(cfg.coreMap[j]);

   cui64 memBlockSize = (cfg.allocMem << 10) / (cui64)threadCount;

   timer.Update();

   // Display configuration properties
   wprintf(L"Start-up delay: %dms\t\tMaximum duration: %5.1fs\tPulse on-time: %4dms\t\tUnits:", cfg.delayTime, fl64(cfg.tics) / fl64(timer.siFrequency), cfg.onTime);
   for(j = 0; j < 8; ++j)
      if(cfg.procUnits & (0x01ull << j)) wprintf(L" %s", wstrUnitsCPU[j]);
   wprintf(L"\nThread count: %d\t\tMemory block size: %3lldMB\tPulse off-time: %3dms\t\tSync: ", threadCount, memBlockSize >> 10, cfg.offTime);
   for(j = 0; j < 8; ++j)
      if(cfg.procSync & (0x01ull << j)) wprintf(L" %s", wstrSyncCPU[j]);
   wprintf(L"\n\nThread bitmap: ");
   for(j = 0; j < cfg.sys.groupCount; ++j) {
      for(mask = 1, i = 0; mask && i < threadCount; mask <<= 1, ++i)
         printf("%c", (mask & cfg.coreMap[j] ? '!' : '.'));
      printf("\n               ");
   }
   printf("\n");

   j = 0;
   switch(cfg.procSync >> 4) {
   case 1: // Constant computation
      j = 0; break;
   case 2: // Fixed pulses
      j = 2; break;
   case 4: // Sweeping pulses
      j = 4; break;
   case 8: // RESERVED
      j = 6; break;
   }
   if(cfg.procSync & 0x08) ++j; // Synchronised

   // Spawn child processes
   for(i = 0, mask = 1; i < threadCount; ++i) {
      threadData[i].maxDuration   = cfg.tics;
      threadData[i].startTime     = cfg.delayTime * timer.siFrequency / 1000 + timer.siCurrentTics;
      threadData[i].endTime       = threadData[i].startTime + threadData[i].maxDuration;
      threadData[i].activeTime    = cfg.onTime;
      threadData[i].inactiveTime  = cfg.offTime;
      threadData[i].packetSizeRAM = memBlockSize;
      threadData[i].procUnits     = cfg.procUnits;
      threadData[i].procSync      = cfg.procSync;
      threadData[i].threadByte    = i >> 3;
      threadData[i].threadBit     = i & 0x07;

      threadBits[i >> 6] |= 1uLL << (i & 0x03F);

      ptrc handle = (ptr)_beginthread(MethodCPU[j], 0, &threadData[i]);

      while(mask & ~cfg.coreMap[procGroup]) mask <<= 1; ///--- Modify to account for >64 virtual cores !!!
      SetThreadAffinityMask(handle, mask);
      mask <<= 1;
   }
   while(ThreadsRunning) Sleep(100);

   printf("\n");

   // Write outputs to console (and/or file?)
   declare1d16(char, strTemp, 32768);
   for(c = 0, j = 1; j < 255 && j < argc - 1; ++j) { // Format command-line options
      if(argv[j][0] == 'o' || argv[j][0] == 'O') continue;
      c += int(wcstombs(&strTemp[c + 8192], argv[j], 8192));
      c += sprintf(&strTemp[c + 8192], "  ");
   }
   c += sprintf(&strTemp[c + 8190], "\n");

   for(c = (wstrOut[0] ? sprintf(strTemp, &strTemp[8192]) : 0), j = 0; j < 5; ++j) {
      mask = 0x01ull << j;
      if(~cfg.procUnits & mask) continue;

      wprintf(L"\n Thread | ProcUnit | Correct values   ");
      switch(cfg.procUnits & mask) { default: i = 1; break; case 4: i = 5; break; case 8: i = 13; break; case 16: i = 29; } while(--i) printf("    ");
      wprintf(L"| Result\n--------+----------+--");
      switch(cfg.procUnits & mask) { default: i = 5; break; case 4: i = 9; break; case 8: i = 17; break; case 16: i = 33; } while(--i) printf("----");
      printf("+--");
      switch(cfg.procUnits & mask) { default: i = 5; break; case 4: i = 9; break; case 8: i = 17; break; case 16: i = 33; } while(--i) printf("----");
      printf(".");
      for(i = 0; i < threadCount; ++i) {
         switch(threadData[i].procUnits & mask) {
         default:
            wprintf(L"\n  #%3.1d  |  %s 64  | %16.16llX | %16.16llX | %s", i, wstrUnitsCPU[j], value[2][i].raw[15], value[1][i].raw[15], EvaluateW(i, 15 - j, 1));
            break;
         case 4:
            if(threadData[i].procUnits & 0x04) wprintf(L"\n  #%3.1d  | SSE  128 | %16.16llX%16.16llX | %16.16llX%16.16llX | %s",
               i, value[2][i].raw[12], value[2][i].raw[13], value[1][i].raw[12], value[1][i].raw[13], EvaluateW(i, 12, 2));
            break;
         case 8:
            if(threadData[i].procUnits & 0x08) wprintf(L"\n  #%3.1d  | AVX  256 | %16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX | %s",
               i, value[2][i].raw[8], value[2][i].raw[9], value[2][i].raw[10], value[2][i].raw[11], value[1][i].raw[8], value[1][i].raw[9], value[1][i].raw[10], value[1][i].raw[11], EvaluateW(i, 8, 4));
            break;
         case 10:
            if(threadData[i].procUnits & 0x10) wprintf(L"\n  #%3.1d  | AVX  512 | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %s",
               i, value[2][i].raw[0], value[2][i].raw[1], value[2][i].raw[2], value[2][i].raw[3], value[2][i].raw[4], value[2][i].raw[5], value[2][i].raw[6], value[2][i].raw[7],
               value[1][i].raw[0], value[1][i].raw[1], value[1][i].raw[2], value[1][i].raw[3], value[1][i].raw[4], value[1][i].raw[5], value[1][i].raw[6], value[1][i].raw[7], EvaluateW(i, 0, 8));
         }
      }
      printf("\n");

      if(wstrOut[0]) { // Format results for file output
         c += sprintf(&strTemp[c], "\n Thread | ProcUnit | Correct values   ");
         switch(cfg.procUnits & mask) { default: i = 1; break; case 4: i = 5; break; case 8: i = 13; break; case 16: i = 29; } while(--i) c += sprintf(&strTemp[c], "    ");
         c += sprintf(&strTemp[c], "| Result\n--------+----------+--");
         switch(cfg.procUnits & mask) { default: i = 5; break; case 4: i = 9; break; case 8: i = 17; break; case 16: i = 33; } while(--i) c += sprintf(&strTemp[c], "----");
         c += sprintf(&strTemp[c], "+--");
         switch(cfg.procUnits & mask) { default: i = 5; break; case 4: i = 9; break; case 8: i = 17; break; case 16: i = 33; } while(--i) c += sprintf(&strTemp[c], "----");
         c += sprintf(&strTemp[c], ".");
         for(i = 0; i < threadCount; ++i) {
            switch(threadData[i].procUnits & mask) {
            case 1:
               c += sprintf(&strTemp[c], "\n  #%3.1d  |  ALU 64  | %16.16llX | %16.16llX | %s", i, value[2][i].raw[15], value[1][i].raw[15], Evaluate(i, 15 - j, 1));
               break;
            case 4:
               if(threadData[i].procUnits & 0x04) c += sprintf(&strTemp[c], "\n  #%3.1d  | SSE  128 | %16.16llX%16.16llX | %16.16llX%16.16llX | %s",
                  i, value[2][i].raw[12], value[2][i].raw[13], value[1][i].raw[12], value[1][i].raw[13], Evaluate(i, 12, 2));
               break;
            case 8:
               if(threadData[i].procUnits & 0x08) c += sprintf(&strTemp[c], "\n  #%3.1d  | AVX  256 | %16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX | %s",
                  i, value[2][i].raw[8], value[2][i].raw[9], value[2][i].raw[10], value[2][i].raw[11], value[1][i].raw[8], value[1][i].raw[9], value[1][i].raw[10], value[1][i].raw[11], Evaluate(i, 8, 4));
               break;
            case 10:
               if(threadData[i].procUnits & 0x10) c += sprintf(&strTemp[c], "\n  #%3.1d  | AVX  512 | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %s",
                  i, value[2][i].raw[0], value[2][i].raw[1], value[2][i].raw[2], value[2][i].raw[3], value[2][i].raw[4], value[2][i].raw[5], value[2][i].raw[6], value[2][i].raw[7],
                  value[1][i].raw[0], value[1][i].raw[1], value[1][i].raw[2], value[1][i].raw[3], value[1][i].raw[4], value[1][i].raw[5], value[1][i].raw[6], value[1][i].raw[7], Evaluate(i, 0, 8));
            }
         }
         c += sprintf(&strTemp[c], "\n");
      }
   }
   printf("\n");

   if(wstrOut[0]) {
      outFile = CreateFileW(wstrOut, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
      if(outFile == INVALID_HANDLE_VALUE) {
         wprintf(L"\n\nCannot create \"%s\" file.\n\n", wstrOut);
         return -64;
      }
      if(!WriteFile(outFile, strTemp, c, &bytesProc, 0)) {
         wprintf(L"\n\nFailed to write results to \"%s\" file.\n\n", wstrOut);
         return -65;
      } else
         wprintf(L"\n\nSuccessfully wrote results to \"%s\" file.\n\n", wstrOut);
      CloseHandle(outFile);
   }
   mfree1(strTemp);

   return 0;
}
