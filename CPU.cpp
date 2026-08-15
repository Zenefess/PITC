/************************************************************
 * File: CPU.cpp                        Created: 2025/01/21 *
 *                                    Last mod.: 2025/04/16 *
 *                                                          *
 * Desc: Pulsed integrity tests for CPUs.                   *
 *                                                          *
 * To do: *) Add utilisation of (code) caches               *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma warning(disable:4996)

#include "CPU_methods.h"

csi32 wmain(csi32 argc, cwchptrc argv[]) {
   VALUES_HEADER header;
   ptr   outFile;
   ui64  mask;
   int   c = 1, d;
   si16  threadCount[3] = { 0, 0, 0 }; // 0=First core class, 1=Second core class, 2=Total
   si16  i;
   si16  k;     // Indexes value[] over every selected core, so it counts to MAX_THREADS, not to 255
   // Indexes the characters of one argument. As a ui8 it wrapped on any argument longer than 255 characters,
   // and every numeric option advanced it by a ui8-truncated delta -- a value field longer than 255 moved it
   // backwards, and the option loop could then never terminate (ISSUES.MD F4)
   ui32  j;
   ui8   outUTF = 0;

   setlocale(LC_ALL, "");

   // The topology is read before anything else: nothing below can be sized, selected or pinned without it,
   // and a machine that cannot be described is one this build must not claim to have tested. The walk itself
   // moved into EnumerateTopology when it moved onto GetLogicalProcessorInformationEx, which is the only
   // form of the call that reports which processor group a mask belongs to (ISSUES.MD G3)
   csi32 topology = EnumerateTopology();

   if(topology) return topology;

   cfg.sys.cpuSSE4_1  = IsProcessorFeaturePresent(PF_SSE4_1_INSTRUCTIONS_AVAILABLE);
   cfg.sys.cpuAVX2    = IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE);
   cfg.sys.cpuAVX512  = IsProcessorFeaturePresent(PF_AVX512F_INSTRUCTIONS_AVAILABLE);

   // Set vector-dependant functions to use largest instruction width available
   static bool (&ThreadsRunning)(void) = cfg.sys.cpuAVX512 ? ThreadsRunningAVX512 : cfg.sys.cpuAVX2 ? ThreadsRunningAVX : ThreadsRunningSSE;

   /// Defaults ///
   cfg.tics        = timer.siFrequency * 900; // 15 minute duration
   cfg.procSync    = 0x012;
   cfg.procUnits   = 0x03;
   cfg.allocMem[0] = 0;
   /// Defaults ///

   if(argc > 1) {
      for(i = 1; i < argc; ++i) {
         switch(argv[i][0]) {
         case L'b': // Run benchmark: All virtual cores, constant computation, ALU + largest vector unit, L3 cache, 8MB memory per virtual core, for 60 seconds
         case L'B':
            if(argv[i][1] && argv[i][1] != L' ') { wprintf(wstrMessage[37], argv[i]); return -25; }
            resArray.iter = zalloc1d64(si64, cfg.sys.vCoreCount);
            cfg.tics        = timer.siFrequency * 60;
            cfg.SMTLoad     = 3;
            cfg.memConfig   = 1;
            cfg.allocMem[0] = 8388608;
            cfg.procSync    = 0x092;
            cfg.procUnits   = (cfg.sys.cpuAVX512 ? 0x091 : cfg.sys.cpuAVX2 ? 0x089 : 0x085);
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
               // A letter naming no unit used to leave the selection at whatever the letters around it had
               // set, so 'Iq' ran the defaults and 'Iaq' silently dropped nothing at all (ISSUES.MD F9)
               default:
                  wprintf(wstrMessage[38], argv[i][j], argv[i]);
                  return -25;
               }
            }
            break;
         case L'l': // Set language
         case L'L':
            for(c = 1; argv[i][c] && argv[i][c] != L' ' && c < 1024; ++c);
            // lstrcpynW's third argument is the capacity of the destination, not the length of the source:
            // an argument longer than wstrLang would otherwise be copied over the globals that follow it
            lstrcpynW(wstrLang, &argv[i][1], min(c, si32(_countof(wstrLang))));
            // lstrcmpiW returns 0 when the two codes match, so testing its result directly selected a
            // language exactly when the argument did *not* name it -- and since both arms select English,
            // every input landed there regardless (ISSUES.MD F7). A second language makes that fatal rather
            // than invisible, so the comparison is corrected before one is added
            if(!lstrcmpiW(wstrLang, L"en-GB") || !lstrcmpiW(wstrLang, L"en-US")) {
               wstrInstructions = wstrInstructions_English;
               wstrMessage      = wstrMessage_English;
               wstrInterface    = wstrInterface_English;
            } else { // A language this build does not carry is worth saying; it is not worth stopping for
               wprintf(wstrMessage[39], wstrLang);
               lstrcpynW(wstrLang, L"en-GB", si32(_countof(wstrLang)));
            }
            break;
         case L'm': // Set amount of memory (in MB) to utilise during test
         case L'M':
            cfg.memConfig   = 0;
            cfg.allocMem[0] = 0;
            for(j = 1; argv[i][j] && argv[i][j] != L' '; ++j) {
               si64 megabytes;

               // wcstol returns a 32-bit long on Windows, so 'Mt3000000' overflowed before the '<< 20'; the
               // reader below is 64-bit and refuses a value that will not survive the shift (ISSUES.MD F4)
               switch(argv[i][j]) {
               case L'c': // For each virtual core
               case L'C':
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_MEM_MB_MAX, megabytes)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_MEM_MB_MAX); return -24;
                  }
                  cfg.memConfig   = 1;
                  cfg.allocMem[0] = megabytes << 20;
                  break;
               // The two classes are the CPU's non-SMT and SMT cores, or its efficiency and performance
               // cores where it is hybrid; CoreClass (CPU.h) decides which, and the letters cannot say
               case L'n': // For each core of the first class
               case L'N':
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_MEM_MB_MAX, megabytes)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_MEM_MB_MAX); return -24;
                  }
                  cfg.memConfig   = 2;
                  cfg.allocMem[0] = megabytes << 20;
                  break;
               case L's': // For each virtual core of the second class
               case L'S':
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_MEM_MB_MAX, megabytes)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_MEM_MB_MAX); return -24;
                  }
                  cfg.memConfig   = 2;
                  cfg.allocMem[1] = megabytes << 20;
                  break;
               case L't': // Equally split amongst all utilised virtual cores
               case L'T':
                  if(!ParseWholeNumber(argv[i], j, 0, OPT_MEM_MB_MAX, megabytes)) {
                     wprintf(wstrMessage[35], argv[i][j], argv[i], si64(0), OPT_MEM_MB_MAX); return -24;
                  }
                  cfg.memConfig   = 0;
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
               // The '6' is examined at c + 1 without advancing onto it, and c only moves once the character
               // is known to be there. For the argument 'O1' the old '++c' landed on the terminating null,
               // after which the enclosing ++c stepped past it and the loop condition read out of bounds
               // (ISSUES.MD F10)
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
                  // The count was 'c - 1', which is the length of the name only when '[' immediately follows
                  // the 'O'; 'O16[results.txt]' therefore produced the name "results.txt]". lstrcpynW's third
                  // argument is a destination capacity, so the count of a name spanning [d, c) is c - d + 1,
                  // and the scan stops one short of wstrOut's capacity rather than at a fixed 1024 offset
                  // into the argument. Advancing c past the ']' inside the call, as 'c++' did, skipped the
                  // character after it as well (ISSUES.MD F5)
                  for(d = ++c; argv[i][c] && argv[i][c] != L']' && c - d < 1023; ++c);
                  // An unterminated, empty or over-long name is the invalid-filename condition -8 documents.
                  // lstrcpynW answered a different question -- it fails only on a bad pointer -- so 'O[]'
                  // used to copy nothing, return a valid pointer, and disable file output silently, while
                  // the branch that was supposed to catch it printed "Unable to create file" and no name at
                  // all (ISSUES.MD F14)
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
                  cfg.offTime   = 0;
                  break;
               case L't': // Test duration
               case L'T':
                  // A duration of zero still reaches the -13 check below, which names the condition exactly;
                  // what this rejects is the missing, negative and non-numeric value that used to arrive
                  // there as a silent zero, or as a duration of some other length entirely (ISSUES.MD F4)
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
               // Both maps are read by ParseCoreMap (CPU.h), which indexes the map by the topology rather
               // than by the character's position in the argument, clears the map before applying it, and
               // stops at the first character that is not part of a map -- so a further 'U' sub-option can
               // follow one, as the documented 'Uc!.!!...!a' spelling requires (ISSUES.MD F2, C10)
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

            // The values written below describe the five register-resident kernels only, so nothing in the
            // file would ever contradict a JobMem* or JobALU_* kernel that had drifted away from its
            // counterpart -- every memory-backed run would simply report the difference as a CPU fault.
            // Prove the whole family agrees before generating anything (ISSUES.MD B5)
            cui8 badKernel = ValidateKernelFamilies();
            if(badKernel) { wprintf(wstrMessage[30], wstrKernelName[badKernel]); return -22; }

            // Both loops below indexed the argument list with i and the seed lanes with j, the two indices
            // the enclosing option loop is walking argv with. The case returns before either is read again,
            // so nothing came of it, but the reuse is what made 'W' unable to appear beside another option
            // at all; local counters cost nothing and remove the coupling (ISSUES.MD F12)
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

               // _beginthreadex reports failure with 0, and hands back a handle this thread owns and must
               // close; _beginthread's is closed by the CRT when the thread exits, so it is never valid to
               // hold. A thread that never starts never clears its bit, hanging the wait loop below forever
               cHANDLE thread = (HANDLE)_beginthreadex(0, 0, GenerateValues, &threadData[t], 0, 0);

               if(!thread) { ClearThreadRunning(t); wprintf(wstrMessage[23], t); return -19; }

               CloseHandle(thread);
            }
            while(ThreadsRunning()) Sleep(100);

            // Test for computational error
            if(value[3][0].raw[0] == 0x05555555555555555 && value[2][0].raw[0] == 0x0AAAAAAAAAAAAAAAA) {
               wprintf(wstrMessage[5]);
               return -4;
            }

            outFile = CreateFileW(L"cpu.values", GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            if(outFile == INVALID_HANDLE_VALUE) {
               wprintf(wstrMessage[6]);
               return -5;
            }

            // The header records the build and the kernel arithmetic these values were produced by, and a
            // hash of each block, so a file left over from an earlier revision is reported as a stale file
            // rather than reaching the comparison and accusing the CPU. Each write is checked for length --
            // WriteFile reports a partial write as success -- and each failure path closes the file
            FillValuesHeader(header, value[0], value[3]);

            if(!WriteBlock(outFile, &header, ui32(sizeof(VALUES_HEADER)))) {
               wprintf(wstrMessage[25]);
               CloseHandle(outFile);
               return -20;
            }
            if(!WriteBlock(outFile, value[0], ui32(RESULTS_BUF_SIZE))) {
               wprintf(wstrMessage[7]);
               CloseHandle(outFile);
               return -6;
            }
            if(!WriteBlock(outFile, value[3], ui32(RESULTS_BUF_SIZE))) {
               wprintf(wstrMessage[8]);
               CloseHandle(outFile);
               return -7;
            }

            wprintf(wstrMessage[1]);

            CloseHandle(outFile);

            return 1;
         }
         case L'-': // Configuration presets
            // The memory and unit preamble below used to be applied before the digit was examined, so a
            // preset that names nothing -- 'PITC.exe -a' -- still rewrote procUnits, memConfig and allocMem
            // on its way to being ignored, and a run that looked like a default one was not one
            // (ISSUES.MD F9). A trailing character is refused for the same reason: '-5x' is not preset 5
            if(argv[i][1] < L'0' || argv[i][1] > L'9' || (argv[i][2] && argv[i][2] != L' ')) {
               wprintf(wstrMessage[37], argv[i]);
               return -25;
            }
            cfg.memConfig   = 1;
            cfg.allocMem[0] = 8388608;
            cfg.procUnits   = (cfg.sys.cpuAVX512 ? 0x011 : cfg.sys.cpuAVX2 ? 0x09 : 0x05);
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
            case L'7': // Synchronised sweeping-width pulsed stress on all virtual cores. 30 minute duration. 10 minute duration
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
         // An argument naming no option at all used to fall out of the switch and run the defaults without a
         // word -- 'PITC.exe Zzz' tested the CPU it was not asked to test, and every mistyped option was a
         // silently different configuration from the one on the command line (ISSUES.MD F9)
         default:
            wprintf(wstrMessage[37], argv[i]);
            return -25;
         }
      }
   } else { // Display instructions
      // system("pause") blocked the documented way of reading the option reference on a keypress, so a CI
      // job, a scheduled task or 'PITC.exe | more' hung until it was killed -- and it reached cmd.exe through
      // the PATH to do it. A console program that has printed its usage is finished (ISSUES.MD F15)
      wprintf(L"%s", wstrInstructions);
      return 2;
   }

   // Requested processing unit checks. Both dispatch and arena sizing reduce a unit selection to the ALU bit
   // and the widest other unit, so a selection naming two of FPU/SSE4.1/AVX2/AVX-512 would seed sub-arrays
   // that overlap inside the arena, and a selection naming none would size the arena in bytes instead of
   // records, dispatch an ALU test that was never requested, and print an empty results table
   cui8 procUnitBits = ui8(cfg.procUnits & 0x01F); // ALU, FPU, SSE4.1, AVX2 and AVX-512
   cui8 vectorBits   = ui8(cfg.procUnits & 0x01E); // FPU, SSE4.1, AVX2 and AVX-512 are mutually exclusive
   if(!procUnitBits)                 { wprintf(wstrMessage[18]); return -15; }
   if(vectorBits & (vectorBits - 1)) { wprintf(wstrMessage[19]); return -16; }

   // Requested vector unit checks
   if(cfg.procUnits & 0x04  && !cfg.sys.cpuSSE4_1) { wprintf(wstrMessage[12]); return -11; }
   if(cfg.procUnits & 0x08  && !cfg.sys.cpuAVX2)   { wprintf(wstrMessage[13]); return -11; }
   if(cfg.procUnits & 0x010 && !cfg.sys.cpuAVX512) { wprintf(wstrMessage[14]); return -11; }

   // Requested synchronisation & timing checks. Each rejects a configuration that would idle the threads,
   // or end the test before they compute, and then report ".Pass." for silicon that was never exercised
   cui8 syncShape = ui8(cfg.procSync & 0x07); // Round-robin, Parallel and Staggered are mutually exclusive
   if(syncShape & (syncShape - 1))                 { wprintf(wstrMessage[15]); return -12; }
   if(cfg.tics <= 0)                               { wprintf(wstrMessage[16]); return -13; }
   if(!(cfg.procSync & 0x010) && !cfg.onTime)      { wprintf(wstrMessage[17]); return -14; }

   if(!syncShape) cfg.procSync |= 0x02; // An unspecified pulse shape is parallel; it must never stay 0

   outFile = CreateFileW(L"cpu.values", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, 0);
   if(outFile == INVALID_HANDLE_VALUE) {
      wprintf(wstrMessage[2]);
      return -1;
   }

   // Every read below is checked for length, and the header is checked before the blocks it describes.
   // ReadFile reports reaching the end of the file as success, so a truncated, empty or unrelated file used
   // to pass both reads with value[0] and value[2] left holding stale zeroes, which every thread then
   // reported as "!Fail!" -- the most common user error of all presenting as a fleet-wide hardware fault
   if(!ReadBlock(outFile, &header, ui32(sizeof(VALUES_HEADER))) || header.magic != VALUES_FILE_MAGIC) {
      wprintf(wstrMessage[26]);
      CloseHandle(outFile);
      return -21;
   }
   // The size is tested alongside the version, not with the magic value above: a header of another length is
   // a header of another layout, and naming the version the file was written in is the more useful diagnosis
   if(header.version != VALUES_FILE_VERSION || header.headerSize != ui32(sizeof(VALUES_HEADER))) {
      wprintf(wstrMessage[27], header.version, VALUES_FILE_VERSION);
      CloseHandle(outFile);
      return -21;
   }
   // Seeds transformed by different arithmetic, or by a build that rounds differently, would grade this CPU
   // against results it was never going to produce. KernelFingerprint runs the same ladder that generated
   // the file, so an edit to any kernel it walks invalidates every file written before that edit
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
      threadCount[0] += (si16)PopulationCount64(cfg.sys.coreMap[0][j] & cfg.coreMap[j]);
      threadCount[1] += (si16)PopulationCount64(cfg.sys.coreMap[1][j] & cfg.coreMap[j]);
   }
   threadCount[2] = threadCount[0] + threadCount[1];

   // A 'U' map naming no core at all leaves nothing to divide the memory request between, and threadCount[2]
   // is the divisor two of the three memory configurations use unchecked (ISSUES.MD C8). The enumeration
   // already refuses a machine reporting no cores (-23), so this is the one remaining route to that divisor
   // being zero -- and the run it would otherwise reach prints an empty results table and returns 0, which
   // reads as a clean pass of a CPU that was never touched. Reachable in practice only since the 'U' maps
   // began to work (F2): the map could not previously be cleared
   if(!threadCount[2]) { wprintf(wstrMessage[40]); return -26; }

   timer.Update();

   // Prepare results output file
   if(wstrOut[0]) {
      outFile = CreateFileW(wstrOut, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
      if(outFile == INVALID_HANDLE_VALUE) {
         wprintf(wstrMessage[10], wstrOut);
         return -9;
      }
      // Both byte-order marks go through WriteBlock for the reason every other write in the program does:
      // WriteFile reports a partial write as success, and a half-written mark makes the file's encoding
      // unreadable to anything that opens it
      switch(outUTF) {
      case 1: // UTF-8
         if(!WriteBlock(outFile, outUTF8header, 3)) { wprintf(wstrMessage[11], wstrOut); CloseHandle(outFile); return -10; }
         break;
      case 2: // UTF-16
         if(!WriteBlock(outFile, &outUTF16header, 2)) { wprintf(wstrMessage[11], wstrOut); CloseHandle(outFile); return -10; }
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
   }

   if(resArray.blockSize[0]) {
      MEMORYSTATUSEX memStatus = { ui32(sizeof(MEMORYSTATUSEX)) };
      cbool memStatusValid     = GlobalMemoryStatusEx(&memStatus) ? true : false;
      ui64  os, bos, recSize, vecUnits;
      // l walks a thread class, whose population is an si16: as a ui8 it wrapped at 256 and the loop below
      // could not terminate, which the 64-virtual-core ceiling was all that kept out of reach (ISSUES.MD C11)
      si16  l;
      ui8   m;

      // Bytes per record for the selected unit set, and the size of a record's vector portion in the 8-byte
      // units resArray.alu is indexed by; the two always satisfy recSize == (vecUnits + 1) * 8 whenever the
      // ALU bit is set, which is what makes the two sub-arrays tile the arena exactly. 'default' catches a
      // unit selection with no processing bit set: wmain rejects that before reaching here, so the arm is
      // unreachable, but without it recSize would be read uninitialised. Change the unit set or a record's
      // layout and this switch must change with it
      switch(cfg.procUnits & 0x01F) {
      default: case 1: case 2:                                                 recSize =  8; vecUnits = 0; break;
      case 3:                                                                  recSize = 16; vecUnits = 1; break;
      case 4: case 6:                                                          recSize = 16; vecUnits = 0; break;
      case 5: case 7:                                                          recSize = 24; vecUnits = 2; break;
      case 8: case 10: case 12: case 14:                                       recSize = 32; vecUnits = 0; break;
      case 9: case 11: case 13: case 15:                                       recSize = 40; vecUnits = 4; break;
      case 16: case 18: case 20: case 22: case 24: case 26: case 28: case 30:  recSize = 64; vecUnits = 0; break;
      case 17: case 19: case 21: case 23: case 25: case 27: case 29: case 31:  recSize = 72; vecUnits = 8;
      }

      // Every JobCycleMem* call processes 4 records and the cursor in ComputationPulse advances in steps of
      // 4, so a count that is not a multiple of 4 is walked up to 3 records past the end of the slice
      resArray.records[0] = (resArray.blockSize[0] / recSize) & ~0x03ull;
      resArray.records[1] = (resArray.blockSize[1] / recSize) & ~0x03ull;

      // A slice too small for a single call cannot be processed at all: a count of 0 also drops the thread
      // onto the register code path, with value[1][k].p0~p4 already overwritten with arena pointers
      for(m = 0; m < 2; ++m)
         if(threadCount[m] && !resArray.records[m]) {
            wprintf(wstrMessage[20], si64(resArray.blockSize[m]), si64(recSize << 2));
            return -18;
         }

      // Every pointer handed to a thread below is derived from this one allocation, so a request the machine
      // cannot supply, or a failure to satisfy one it can, would be a null-pointer write in each of them
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

      // resArray.avx512, .avx, .sse and .fpu are four views of one address with different element strides, so
      // seeding two of them would leave each overwriting the other's records. wmain has already rejected any
      // selection naming more than one of FPU/SSE4.1/AVX2/AVX-512, so at most one of p0~p3 is written below.
      // bos is the running record offset into the arena: it counts across both thread classes, because
      // restarting it at the first class-1 thread would hand each of them a slice a class-0 thread already has
      for(k = 0, m = 0, bos = 0; m < 2; ++m)
         for(l = 0; l < threadCount[m]; ++k, ++l, bos += resArray.records[m]) {
            value[1][k].p0 = &resArray.avx512[bos];
            value[1][k].p1 = &resArray.avx[bos];
            value[1][k].p2 = &resArray.sse[bos];
            value[1][k].p3 = &resArray.fpu[bos];
            value[1][k].p4 = &resArray.alu[bos];
            for(os = 0; os < resArray.records[m]; ++os) {
               if(cfg.procUnits & 0x010) value[1][k].p0[os] = value[0][k].avx512;
               if(cfg.procUnits & 0x08)  value[1][k].p1[os] = value[0][k].avx;
               if(cfg.procUnits & 0x04)  value[1][k].p2[os] = value[0][k].sse;
               if(cfg.procUnits & 0x02)  value[1][k].p3[os] = value[0][k].fpu;
               if(cfg.procUnits & 0x01)  value[1][k].p4[os] = value[0][k].alu;
            }
         }
   }

   // The results table is one row per thread per selected unit, and the widest row -- AVX-512, two 512-bit
   // values printed in hexadecimal -- is a little under 300 characters. wstrOutput was a fixed 32 768 wide
   // characters however many threads were running, which the table passes at about 150 (ISSUES.MD C7); the
   // 64-virtual-core ceiling is what kept that unreachable, so lifting it is what makes the sizing necessary.
   // At most two unit rows can be selected at once -- the ALU bit plus one of FPU/SSE/AVX2/AVX-512 -- so a
   // kibicharacter per thread is a little over half as much again as the widest pair of rows can need. The
   // thread bitmap above it is one row per processor group rather than per thread, and 96 characters covers
   // a row of 64 cores and its indent, so the two terms between them bound everything written below
   csi32 outChars = 4096 + si32(cfg.sys.groupCount) * 96 + si32(threadCount[2]) * 1024;

   al64 declare1d64z(wchar, wstrOutput, outChars);

   if(!wstrOutput) {
      wprintf(wstrMessage[22], (si64(outChars) * si64(sizeof(wchar))) >> 20);
      return -17;
   }

   printf("\n");
   // Output configuration properties
   c = swprintf(wstrOutput, wstrInterface[0]);
   for(i = 0, j = 0; i < 8; ++i) if(cfg.procUnits & (0x01ull << i)) { c += swprintf(&wstrOutput[c], L" %s", wstrUnitsCPU[i]); ++j; }
   for(; j < 3; j++) c += swprintf(&wstrOutput[c], L"    ");
   // cfg.allocMem[0] is the whole of the allocation in every memory configuration: the switch above leaves it
   // holding the total, and allocMem[1] is a per-thread size that has already been counted into it. Adding
   // the two reported 'Mn8 Ms4' as the true total plus 4MB, and stayed invisible in the other two
   // configurations only because allocMem[1]'s default of 1 byte vanishes under the shift (ISSUES.MD F8)
   c += swprintf(&wstrOutput[c], wstrInterface[1], cfg.allocMem[0] >> 20, cfg.delayTime);
   if(cfg.procSync & 0x060) c += swprintf(&wstrOutput[c], wstrInterface[cfg.procSync & 0x020 ? 2 : 3], cfg.onTime);
   c += swprintf(&wstrOutput[c], wstrInterface[4]);
   for(i = 0, j = 0; i < 8; ++i) if(cfg.procSync & (0x01ull << i)) { c += swprintf(&wstrOutput[c], L" %s", wstrSyncCPU[i]); ++j; }
   for(; j < 3; j++) c += swprintf(&wstrOutput[c], L"    ");
   c += swprintf(&wstrOutput[c], wstrInterface[5], threadCount[2], (fl64(cfg.tics) / fl64(timer.siFrequency)));
   if(cfg.procSync & 0x020) c += swprintf(&wstrOutput[c], wstrInterface[6], cfg.offTime);
   c += swprintf(&wstrOutput[c], wstrInterface[7]);
   // One row per processor group, one character per virtual core *of that group*, the row ending at the
   // group's highest. The width was the virtual core count of the whole machine for every row, which
   // describes a machine of one group and no other (ISSUES.MD G3): a second group would have been printed to
   // the first group's width, its cores beyond that width missing and its row padded with dots for cores
   // that are not in it. Ending at the highest core the group holds assumes nothing about the numbering
   for(i = 0; i < cfg.sys.groupCount; ++i) {
      cui64 groupMap = cfg.sys.coreMap[0][i] | cfg.sys.coreMap[1][i];

      for(mask = 1; mask && mask <= groupMap; mask <<= 1)
         c += swprintf(&wstrOutput[c], L"%c", (mask & cfg.coreMap[i] ? '!' : '.'));
      c += swprintf(&wstrOutput[c], L"\n               ");
   }
   c -= 15;
   // wstrOutput is built at run time, so passing it as the format string makes every '%' it ever comes to
   // hold a conversion specifier reading arguments that were never passed. Nothing can put one there today,
   // but wstrOut -- a filename straight off the command line -- is already interpolated into messages
   // elsewhere, and one edit is all it would take (ISSUES.MD F11)
   wprintf(L"%s", wstrOutput);
   printf("\n");

   // Spawn child processes
   for(d = 0, j = 0; j < 2; ++j) {
      // The affinity cursor walks the selected cores of the thread's own class, group by group. It consulted
      // the combined map from bit 0 for both classes, so on any topology carrying two populated core classes
      // the second class was pinned over cores the first already held while the top of the map idled -- and
      // that is every hybrid P/E-core part at every setting, its efficiency cores being the first class and
      // its performance cores the second (ISSUES.MD G5, G9).
      // Walking the class map also keeps packetSizeRAM and resArray.records[j], both selected by class,
      // describing the core the thread is actually pinned to. The two class maps are disjoint, so restarting
      // the cursor at group 0 bit 0 for each class costs nothing beyond skipping the other class's bits, and
      // threadCount[j] is the population count of these very maps, so there is exactly one core in them for
      // each thread of the class
      ui8 coreGroup = 0;

      for(i = 0, mask = 1; i < threadCount[j]; ++d, ++i, mask <<= 1) {
         threadData[d].packetSizeRAM = resArray.blockSize[j];
         threadData[d].startTics     = timer.siFrequency * cfg.delayTime / 1000 + timer.siCurrentTics;
         threadData[d].endTics       = threadData[d].startTics + cfg.tics;
         threadData[d].maxTics       = cfg.tics;
         threadData[d].activeTics    = timer.siFrequency * si64(cfg.onTime) / 1000;
         threadData[d].cycleTics     = timer.siFrequency * si64(cfg.onTime + cfg.offTime) / 1000;
         threadData[d].inactiveTime  = cfg.offTime;
         threadData[d].rc_tc         = (resArray.records[j] & 0x0FFFFFFFFFFFF) | (ui64(threadCount[2]) << 48);
         threadData[d].procUnits     = cfg.procUnits;
         threadData[d].procSync      = cfg.procSync;
         threadData[d].threadByte    = d >> 3;
         threadData[d].threadBit     = d & 0x07;

         SetThreadRunning(d); // Interlocked: a thread spawned earlier may be clearing this same byte

         // _beginthreadex reports failure with 0, and hands back a handle this thread owns and must close;
         // _beginthread's is closed by the CRT when the thread exits, so it is never valid to pass to an
         // affinity call. Creating suspended applies the affinity before the thread executes anything,
         // rather than after it has already begun running on whichever core the scheduler chose
         cHANDLE thread = (HANDLE)_beginthreadex(0, 0, ComputationPulse, &threadData[d], CREATE_SUSPENDED, 0);

         GROUP_AFFINITY affinity = {}; // Its Reserved[3] must be zero; the initialiser is what guarantees it

         // A thread that never starts never clears its completion bit, hanging the wait loop below forever
         if(!thread) { ClearThreadRunning(d); wprintf(wstrMessage[23], d); return -19; }

         if(NextSelectedCore(mask, coreGroup, ui8(j))) {
            affinity.Mask  = mask;
            affinity.Group = ui16(coreGroup);
         }

         // SetThreadAffinityMask takes a bare 64-bit mask, which the OS reads as a mask of the thread's own
         // processor group, so it cannot pin a thread to a core of any other group and fails outright once
         // the walk has shifted past the last selected bit of group 0 (ISSUES.MD G3). SetThreadGroupAffinity
         // names the group. A cursor that found no further core leaves an empty mask, which it rejects --
         // the same warning as any other affinity the OS will not accept
         if(!SetThreadGroupAffinity(thread, &affinity, 0)) wprintf(wstrMessage[24], d);

         if(ResumeThread(thread) == DWORD(-1)) { ClearThreadRunning(d); CloseHandle(thread); wprintf(wstrMessage[23], d); return -19; }

         CloseHandle(thread);
      }
   }
   while(ThreadsRunning()) Sleep(100);

   // Output results
   printf("\n");
   for(d = c, j = 0; j < 5; ++j) { // Cycle through each processing unit
      mask = 0x01ull << j;
      if(~cfg.procUnits & mask) continue;

      c += swprintf(&wstrOutput[c], wstrInterface[8]);
      switch(cfg.procUnits & mask) { default: i = 1; break; case 4: i = 5; break; case 8: i = 13; break; case 16: i = 29; } while(--i) c += swprintf(&wstrOutput[c], L"    ");
      c += swprintf(&wstrOutput[c], wstrInterface[9]);
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
         case 16:
            c += swprintf(&wstrOutput[c], L"\n  #%3.1d  | AVX  512 | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %s",
               i, value[2][i].raw[0], value[2][i].raw[1], value[2][i].raw[2], value[2][i].raw[3], value[2][i].raw[4], value[2][i].raw[5], value[2][i].raw[6], value[2][i].raw[7],
               value[3][i].raw[0], value[3][i].raw[1], value[3][i].raw[2], value[3][i].raw[3], value[3][i].raw[4], value[3][i].raw[5], value[3][i].raw[6], value[3][i].raw[7], wstrPass[Evaluate(i, 0)]);
         }
      }
      c+= swprintf(&wstrOutput[c], L"\n");
   }
   if(cfg.procSync & 0x080) { // Print benchmark results
      si64 accum = 0;
      for(i = 0; i < threadCount[2]; ++i) accum += resArray.iter[i];
      c += swprintf(&wstrOutput[c], wstrInterface[10], accum * max(si64((cfg.procUnits & 0x01F) >> 1), 1) / (cfg.tics / timer.siFrequency) >> 10);
   }
   c += swprintf(&wstrOutput[c], L"\n");

   // Write outputs to console and/or file
   wprintf(L"%s", &wstrOutput[d]); // A run-time buffer is never a format string (ISSUES.MD F11)
   if(wstrOut[0]) {
      if(outUTF == 2) { // UTF-16 encoding
         // WriteFile's third argument is a byte count and c is a count of wide characters, so a UTF-16
         // results file held exactly the first half of the report and ended mid-character. The 8-bit path
         // below was correct only because there one character really is one byte (ISSUES.MD F3)
         if(!WriteBlock(outFile, wstrOutput, ui32(c) * ui32(sizeof(wchar)))) {
            wprintf(wstrMessage[11], wstrOut);
            CloseHandle(outFile);
            return -10;
         }
      } else { // 8-bit encodings
         // The conversion used to write the narrow string into wstrOutput itself, starting at wide-character
         // index c, which asks a 2c-byte buffer to hold 3c bytes -- and asks it while it is still reading the
         // wide string it is overwriting (ISSUES.MD C7). A separate allocation is the only spelling of this
         // that cannot overlap its own source.
         // It converted through wcstombs, which uses the locale setlocale(LC_ALL, "") installed -- on most
         // Windows installations the system ANSI code page. The '8' option therefore wrote ANSI bytes behind
         // a UTF-8 byte-order mark, and every non-ASCII character in the file was mojibake to anything that
         // believed the mark (ISSUES.MD F6). WideCharToMultiByte names the encoding the option asked for,
         // reports the exact byte count that encoding needs, and reports a character it cannot represent as a
         // failure rather than as the (size_t)-1 that used to be handed to WriteFile as a length.
         // The character count c is passed rather than -1, so no terminating null reaches the file
         cui32 codePage  = (outUTF == 1 ? CP_UTF8 : CP_ACP);
         csi32 narrowLen = WideCharToMultiByte(codePage, 0, wstrOutput, c, 0, 0, 0, 0);
         chptr strNarrow = (narrowLen > 0 ? (chptr)zalloc64(csize_t(narrowLen)) : 0);

         if(!strNarrow || WideCharToMultiByte(codePage, 0, wstrOutput, c, strNarrow, narrowLen, 0, 0) != narrowLen ||
            !WriteBlock(outFile, strNarrow, ui32(narrowLen))) {
            wprintf(wstrMessage[11], wstrOut);
            mfree1(strNarrow);
            CloseHandle(outFile);
            return -10;
         }
         mfree1(strNarrow);
      }
      wprintf(wstrMessage[0], wstrOut);
      CloseHandle(outFile);
   }

   return 0;
}
