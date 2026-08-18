/*
 * File: CPU.cpp
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-01-21
 * Last Modified: 2026-08-18
 * Description: PITC entry point: option parsing, arena allocation, thread spawn and reporting; defines every namespace-scope object.
 * To Do: 1) Add a VERSIONINFO resource, so the build states its version rather than the prose around it (ISSUES.MD K3)
 * Dependencies: CPU_methods.h
 * ISA: Scalar
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
#include "CPU_methods.h"

//--- Global variables ---//
// Every object this program holds at namespace scope is defined here, and declared in CPU.h -- or, for the
// six language tables, in translations.h. A definition in a header hands the second translation unit to
// include it either a duplicate symbol at link time or, where the object is const and so internally linked, a
// private copy of it; and the storage below is precisely what the worker threads and wmain compare and signal
// through, so a private copy of it is a program whose halves cannot see each other (ISSUES.MD H9). The
// initialisation order is the order of the definitions, which is all one translation unit has to guarantee:
// none of them reads another as it is constructed
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
// translation unit of the kernel it wraps (ISSUES.MD H4); CPU_job_cycles.h declares them and states the rules
// this table has to keep, of which the first is that all 32 indices of the (procUnits & 0x1F) domain are
// covered, because ComputationPulse does not range-check the index it dispatches through.
// UPPER_SNAKE because it is a table at namespace scope, which GCS r12 spells that way; the entries keep the
// PascalCase r11 gives a function, so the one name here that is not a function is the one that reads
// differently from the eighteen that are (ISSUES.MD K8)
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
// why it is a definition here rather than a static function in the header (ISSUES.MD H4, H9)
void Failed(cui64 coreNum, vchptrc threadByte, cui8 unit) {
   // threadByte addresses the byte holding eight threads' completion bits, so zeroing it told wmain that all
   // eight had finished: its wait loop could then return while up to seven of them were still writing
   // value[3] and resArray.iter, and read the results table out from under them. coreNum is
   // (threadByte << 3) + threadBit, so the failing thread's bit within that byte is coreNum & 0x07
   cui8 threadMask = ui8(~(1u << (coreNum & 0x07)));

   // The observed value is read from value[3], never value[1]. Every caller copies the value that failed into
   // value[3] immediately before calling, and value[1] is the *working* plane, which in memory-backed mode
   // does not hold results at all: its first 40 bytes are the arena pointers p0~p4, which the RESULTS union
   // overlays on the avx512 member. Cases 0~3 read value[1], so an AVX-512 failure printed eight pointers
   // reinterpreted as doubles, and the AVX, SSE and FPU cases printed whatever a register-resident run had
   // last left in those lanes -- zero, under any 'M', 'B' or preset run. Only the ALU case, which already
   // read value[3], reported the value the CPU actually produced (ISSUES.MD A8)
   wprintf(wstrInterface[11], coreNum);
   // Each vector case binds the two planes' lanes to a pair of local views before formatting them. Spelt
   // out, one 'value[2][coreNum].avx512.m512d_f64[k]' per lane, the AVX-512 case ran to 184 columns against
   // the 180 GCS e2 makes a hard cap, and the AVX and SSE cases to 172 and 173 against the 150 it asks for
   // (ISSUES.MD K8). The views are per case rather than one pair before the switch, so that each names the
   // member of RESULTS its own unit writes: nothing here has to know the lane offsets that Evaluate and the
   // results table derive from the union's widest-unit-first ordering.
   // The value line of each case is wstrInterface[13 + unit] rather than a literal here: this function is the
   // one output a failing CPU produces, and half of it -- the lane separators, and where [12] falls between
   // the expected lanes and the observed ones -- was fixed in English however the 'L' option was set
   // (ISSUES.MD D1). The five entries are in the order of this switch, which is the order of the 'unit'
   // argument, so a case added here is an entry added at the end of that run and nowhere else
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
// destructor, and the code page is conhost state the launching shell keeps after this process is gone
// (ISSUES.MD D2). The handler restores it and declines the event, so default termination still proceeds
static BOOL __stdcall RestoreConsoleCP(DWORD) {
   if(consoleCP.codePage) SetConsoleOutputCP(consoleCP.codePage);
   return FALSE;
}

csi32 wmain(csi32 argc, cwchptrc argv[]) {
   VALUES_HEADER header;
   // Handles of the worker threads, in thread order, so that this function can wait for them to end rather
   // than for the completion bit each of them clears from inside its own body (ISSUES.MD D8). The 'W' path
   // returns before the test path spawns anything, so the one array serves both
   HANDLE threadHandle[MAX_THREADS] = {};
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
   wchar wstrLangArg[6]; // The candidate code an 'L' argument names; wstrLang records only the active language

   // The CRT is asked for the UTF-8 LC_CTYPE locale first, and the console's output code page is moved to
   // UTF-8 only where that grant succeeds: wprintf converts wide text through the one and the console decodes
   // the bytes through the other, so setting either alone is the mojibake ISSUES.MD D2 names rather than the
   // fix. Where the UCRT is too old to grant it (before Windows 10 1803) the machine's regional locale is
   // installed exactly as before and the console is left alone -- ASCII English survives any code page, which
   // is why this program never noticed. SetConsoleOutputCP's own failure is tolerated rather than reverted:
   // with stdout redirected there may be no console to set, and the UTF-8 bytes the CRT now writes are
   // exactly what the redirected file should hold. The console is machine state that outlives the process, so
   // the change is put back on both of the ways out: the global consoleCP's destructor covers every return
   // below, and RestoreConsoleCP, registered here as a console control handler, covers the Ctrl-C that
   // terminates through ExitProcess and unwinds nothing.
   // LC_CTYPE is the one category any locale is installed for. LC_ALL handed LC_NUMERIC to the machine as
   // well, and every %f conversion this program performs takes its decimal separator from that category: on a
   // comma-decimal system the banner's "Maximum duration" (wstrInterface[5]), the range a malformed decimal
   // option is reported against (wstrMessage[36]) and every value Failed() prints were written "1,234567" --
   // into the file 'O[name]' saves as much as to the console, where this program's own documentation and any
   // consumer of that file expect "1.234567" (ISSUES.MD F3). The last call states the invariant rather than
   // leaving it to the C startup default, so that the one place this program has a locale policy says what
   // that policy is, and so that a category added to LC_ALL by a later CRT cannot quietly acquire a separator
   // with it. ParseDecimal pins its own read through a locale object of its own regardless (F2): together the
   // two make a decimal spelling mean the same thing on every machine, in and out
   if(setlocale(LC_CTYPE, ".UTF8")) { SetConsoleOutputCP(CP_UTF8); SetConsoleCtrlHandler(RestoreConsoleCP, TRUE); }
   else setlocale(LC_CTYPE, "");
   setlocale(LC_NUMERIC, "C");

   // The topology is read before anything else: nothing below can be sized, selected or pinned without it,
   // and a machine that cannot be described is one this build must not claim to have tested. The walk itself
   // moved into EnumerateTopology when it moved onto GetLogicalProcessorInformationEx, which is the only
   // form of the call that reports which processor group a mask belongs to (ISSUES.MD G3)
   csi32 topology = EnumerateTopology();

   if(topology) return topology;

   // Each flag names the set the unit gated on it actually executes. They used to name a higher one apiece:
   // the SSE kernels are SSE2 throughout and the AVX kernels AVX1 throughout, and only the comparison in each
   // reached above that -- PTEST in one, VPXOR ymm in the other -- so the requirement being tested for here
   // was the verdict's rather than the arithmetic's, and a Penryn or a Sandy Bridge was refused a unit it
   // could have run in full (ISSUES.MD C1). PF_XMMI64_INSTRUCTIONS_AVAILABLE is SSE2, which every x64 CPU
   // carries and CPU_build.h refuses to build for anything else -- it is asked so that the answer is the
   // machine's rather than an assumption, and so that ThreadsRunningScalar keeps a selector
   cfg.sys.cpuSSE2   = IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE);
   cfg.sys.cpuAVX    = IsProcessorFeaturePresent(PF_AVX_INSTRUCTIONS_AVAILABLE);
   cfg.sys.cpuAVX512 = IsProcessorFeaturePresent(PF_AVX512F_INSTRUCTIONS_AVAILABLE);

   // Set vector-dependent functions to use largest instruction width available. The SSE poll was once not the
   // baseline it was being used as: AllFalse of two ui128 is _mm_testz_si128, an SSE4.1 instruction, so
   // selecting it on nothing but the absence of AVX2 faulted with an illegal instruction on a CPU carrying
   // SSE2 and no more -- before any test began, and before the pre-flight check at the end of the parse
   // could name the missing instruction set. An ALU-only run does not ask for a vector unit and never reaches
   // that check at all, so nothing else in the program stood between such a CPU and the fault (ISSUES.MD D4).
   // The poll folds its zero test at SSE2 now, so the condition below is the flag it always should have been
   // rather than a stand-in for one (C1)
   static bool (&ThreadsRunning)(void) = cfg.sys.cpuAVX512 ? ThreadsRunningAVX512 : cfg.sys.cpuAVX ? ThreadsRunningAVX
                                       : cfg.sys.cpuSSE2   ? ThreadsRunningSSE    : ThreadsRunningScalar;

   /// Defaults ///
   cfg.tics        = timer.siFrequency * 900; // 15 minute duration
   cfg.procSync    = 0x012;
   cfg.procUnits   = 0x03;
   // Both class sizes, because 'M' no longer clears either of them: only 'B' and the presets reset the
   // memory configuration, so this is the one place a run with no 'M' option at all is given its zero, and
   // naming one entry here would leave the other holding whatever the object's initialiser last set
   // (ISSUES.MD F1)
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
            // 'B' may be given twice on one command line, and each occurrence allocated a fresh block over
            // the pointer to the last one, leaking it; mdealloc ignores the null of the first (ISSUES.MD C13)
            mfree1(resArray.iter);
            resArray.iter = zalloc1d64(si64, cfg.sys.vCoreCount);
            // The counters are one of the program's three run-length allocations, and were the only one whose
            // result was never examined: the arena and the report buffer below both refuse the run with -17
            // on a null, while this one went on to spawn the threads, every one of which writes its record
            // count through the pointer as it ends (CPU_methods.h) -- a null-pointer write from every thread
            // of the run, in place of the diagnosis the other two give. A few KiB is a remote failure, which
            // is what made it easy to leave unchecked and is no reason to treat it differently (ISSUES.MD C2)
            if(!resArray.iter) {
               wprintf(wstrMessage[22], (si64(cfg.sys.vCoreCount) * si64(sizeof(si64))) >> 20);
               return -17;
            }
            cfg.tics        = timer.siFrequency * 60;
            cfg.SMTLoad     = 3;
            cfg.memConfig   = 1;
            // 'B' is one of the two things documented as resetting the memory configuration, so it clears
            // the second class's size as well as setting the first's: memConfig 1 leaves allocMem[1] unread,
            // but an 'Ms' given afterwards selects memConfig 2 and would otherwise be composed with a size
            // an earlier 'Ms' had left behind (ISSUES.MD F1)
            cfg.allocMem[0] = 8388608;
            cfg.allocMem[1] = 0;
            // 8MB per thread is the benchmark's own figure rather than one this command line asked for, so an
            // 'I1', 'I2' or 'I3' given after 'B' derives its sizes from that cache level instead of merely
            // being checked against it -- 'B' resets the memory configuration, and that is what the reset
            // means here. An 'M' after 'B' sets the flag again and keeps its size (ISSUES.MD A1)
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
            // an argument longer than the buffer would otherwise be copied over whatever follows it
            // (ISSUES.MD C6). The candidate lands in a local rather than in wstrLang, because wstrLang names
            // the language the interface is in: a code this build does not carry must leave it untouched,
            // where parsing into it directly left it naming a language that was never selected
            lstrcpynW(wstrLangArg, &argv[i][1], min(c, si32(_countof(wstrLangArg))));
            // The LANGUAGES registry in translations.h is the whole of the selection: this case names no
            // language of its own, so adding one is a header and a registry row rather than an edit here
            // (ISSUES.MD D2). lstrcmpiW returns 0 when the two codes match; testing its result directly
            // selected a language exactly when the argument did *not* name it (ISSUES.MD F7)
            for(d = 0; d < si32(_countof(LANGUAGES)); ++d)
               if(!lstrcmpiW(wstrLangArg, LANGUAGES[d].wstrCode)) {
                  // Every table of the language is repointed together, from the one row. A language that set
                  // some of them and left the rest at the previous selection would print a report in two
                  // languages, and the three label tables are the half most easily forgotten: they carry no
                  // prose long enough to look like text
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
            // A language this build does not carry is worth saying; it is not worth stopping for
            if(d == si32(_countof(LANGUAGES))) wprintf(wstrMessage[39], wstrLangArg);
            break;
         case L'm': // Set amount of memory (in MB) to utilise during test
         case L'M':
            // No reset here. Every 'M' argument used to open by clearing memConfig and allocMem[0] while
            // leaving allocMem[1] untouched, so the two per-class sizes composed in one order only: 'Mn8 Ms8'
            // ended as { 0, 8MB } and the per-class record check below refused the run with -18 and "Only 0
            // bytes of memory per thread", while 'Ms8 Mn8' and the single-argument 'Mn8s8' both ran. Giving
            // both is precisely how the help text spells the two-class case, and clearing allocMem[1] here
            // as well would only have made the two orders agree by refusing both of them. Only 'B' and the
            // presets are documented as resetting the memory configuration, and each sub-option below
            // assigns memConfig together with the size it names, so the last option to set a property still
            // wins (ISSUES.MD F1)
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
                  // Every sub-option records that a size was asked for by name, so that a cache-targeted run
                  // checks this size against the level's residency window rather than replacing it with one
                  // of its own. The flag is cleared only by the three things documented as resetting the
                  // memory configuration -- the defaults, 'B' and a preset -- so 'Mc8 -1' derives and
                  // '-1 Mc8' does not, exactly as the last-option-wins contract already reads (ISSUES.MD A1)
                  cfg.memConfig   = 1;
                  cfg.memExplicit = 1;
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
               // The off-time a sweep does not have is cleared once the whole command line has been read,
               // rather than here: clearing it as this option is parsed leaves the order of the two mattering
               // -- 'Ts]500' and 'T]500s' were different runs -- and does nothing for the two presets, which
               // set the sweep bit without passing through here at all (ISSUES.MD E7)
               case L's': // Sweeping pulse-width thread execution
               case L'S':
                  cfg.procSync &= 0x08F;
                  cfg.procSync |= 0x040;
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
            // Prove the whole family agrees before generating anything (ISSUES.MD B5).
            // The same call now also holds each vector kernel to JobFPU lane for lane, because the file's
            // readability on a CPU of another vector width rests on nothing else, and a drift there was
            // reported to the next machine as a stale file rather than as the kernel that caused it (B1).
            // The two disagreements are different diagnoses, so the table's ladder boundary selects between
            // the two messages
            cui8 badKernel = ValidateKernelFamilies();
            if(badKernel) {
               wprintf(wstrMessage[badKernel < KERNEL_NAME_LADDER ? 30 : 41], wstrKernelName[badKernel]);
               return -22;
            }

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
               // hold. A thread that never starts never clears its bit, hanging the wait loop below forever.
               // The handle is kept until every thread has been waited on rather than closed here, because
               // the completion bit each thread clears is cleared from inside the thread (ISSUES.MD D8)
               threadHandle[t] = (HANDLE)_beginthreadex(0, 0, GenerateValues, &threadData[t], 0, 0);

               if(!threadHandle[t]) {
                  ClearThreadRunning(t); ReleaseThreads(threadHandle, t); wprintf(wstrMessage[23], t); return -19;
               }
            }
            while(ThreadsRunning()) Sleep(100);

            // The bitmap says only that every thread has reached its last statement; the threads themselves
            // are still executing it, and the results they wrote are read immediately below (ISSUES.MD D8)
            JoinThreads(threadHandle, cfg.sys.vCoreCount);

            // Test for computational error. Read from a flag of its own rather than from the two sentinel
            // values that used to be written into entry 0's results, which the thread that owns entry 0
            // overwrote with its own output -- discarding the error and writing the file regardless
            // (ISSUES.MD B7). Every thread has cleared its completion bit by here, and each of them set the
            // flag with an interlocked write before doing so, so a set flag cannot still be in flight
            if(generateError) {
               wprintf(wstrMessage[5]);
               return -4;
            }

            // The file is built under a temporary name and moved over the old one once every byte of it has
            // reached the disk, rather than written into "cpu.values" itself. Written in place, a crash, a
            // full disk or a Ctrl-C anywhere between the CreateFileW and the last WriteFile left a truncated
            // file where the previous good one had been: the header hashes make the next run reject the
            // remnant with -21 rather than grade a CPU against it, but the file it replaced is gone either
            // way, and it costs minutes of ladder runs to produce another. Nothing here can now destroy it
            // except a move that succeeds.
            // The share mode is 0 rather than FILE_SHARE_WRITE, which admitted a second writer to the very
            // bytes this one is writing -- two concurrent 'W' runs interleaving their blocks. They now meet
            // at this call instead, and the second is refused with -5 before it has written anything
            // (ISSUES.MD B2)
            cwchptrc valuesName = L"cpu.values";
            cwchptrc valuesTemp = L"cpu.values.tmp";

            outFile = CreateFileW(valuesTemp, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            if(outFile == INVALID_HANDLE_VALUE) {
               // The message names the file that could not be created, which is the temporary rather than
               // "cpu.values" -- and naming the wrong one would misdirect the very case the share mode above
               // exists to produce, where a second 'W' is refused this file while "cpu.values" is untouched
               wprintf(wstrMessage[6], valuesTemp);
               return -5;
            }

            // The header records the build and the kernel arithmetic these values were produced by, and a
            // hash of each block, so a file left over from an earlier revision is reported as a stale file
            // rather than reaching the comparison and accusing the CPU. Each write is checked for length --
            // WriteFile reports a partial write as success -- and each failure path closes the file and
            // removes the temporary, leaving both the directory and any previous "cpu.values" as it found them
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
            // The memory and unit preamble below used to be applied before the digit was examined, so a
            // preset that names nothing -- 'PITC.exe -a' -- still rewrote procUnits, memConfig and allocMem
            // on its way to being ignored, and a run that looked like a default one was not one
            // (ISSUES.MD F9). A trailing character is refused for the same reason: '-5x' is not preset 5
            if(argv[i][1] < L'0' || argv[i][1] > L'9' || (argv[i][2] && argv[i][2] != L' ')) {
               wprintf(wstrMessage[37], argv[i]);
               return -25;
            }
            cfg.memConfig   = 1;
            // Both class sizes, for the reason 'B' clears both: a preset is the other thing documented as
            // resetting the memory configuration, and an 'Ms' after it must not inherit an 'Ms' before it
            // (ISSUES.MD F1)
            cfg.allocMem[0] = 8388608;
            cfg.allocMem[1] = 0;
            // A preset's 8MB is a default rather than a request, exactly as 'B's is, so '-1 I3a' derives its
            // block sizes from the level-3 window and '-1 Mc8' keeps the 8MB it was given (ISSUES.MD A1)
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
   // and the widest other unit, so a selection naming two of FPU/SSE2/AVX/AVX-512 would seed sub-arrays
   // that overlap inside the arena, and a selection naming none would size the arena in bytes instead of
   // records, dispatch an ALU test that was never requested, and print an empty results table
   cui8 procUnitBits = ui8(cfg.procUnits & 0x01F); // ALU, FPU, SSE2, AVX and AVX-512
   cui8 vectorBits   = ui8(cfg.procUnits & 0x01E); // FPU, SSE2, AVX and AVX-512 are mutually exclusive
   if(!procUnitBits)                 { wprintf(wstrMessage[18]); return -15; }
   if(vectorBits & (vectorBits - 1)) { wprintf(wstrMessage[19]); return -16; }

   // Requested vector unit checks. The first can no longer fire on a machine this build runs on at all: SSE2
   // is part of the x64 architecture, and CPU_build.h refuses to compile for anything else. It stays because
   // cfg.sys.cpuSSE2 is what IsProcessorFeaturePresent answered rather than what the architecture promises,
   // and because the three vector units are gated alike -- 'Is' asking for SSE2 is the same kind of statement
   // as 'Iv' asking for AVX, and the day a check like it is needed it should not have to be reinvented (C1)
   if(cfg.procUnits & 0x04  && !cfg.sys.cpuSSE2)   { wprintf(wstrMessage[12]); return -11; }
   if(cfg.procUnits & 0x08  && !cfg.sys.cpuAVX)    { wprintf(wstrMessage[13]); return -11; }
   if(cfg.procUnits & 0x010 && !cfg.sys.cpuAVX512) { wprintf(wstrMessage[14]); return -11; }

   // Requested synchronisation & timing checks. Each rejects a configuration that would idle the threads,
   // or end the test before they compute, and then report ".Pass." for silicon that was never exercised
   cui8 syncShape = ui8(cfg.procSync & 0x07); // Round-robin, Parallel and Staggered are mutually exclusive
   if(syncShape & (syncShape - 1))                 { wprintf(wstrMessage[15]); return -12; }
   if(cfg.tics <= 0)                               { wprintf(wstrMessage[16]); return -13; }

   if(!syncShape) cfg.procSync |= 0x02; // An unspecified pulse shape is parallel; it must never stay 0

   // ComputationPulse selects each of Constant, Fixed pulse and Sweeping pulse from its own bit rather than
   // inferring one from the absence of the others, so a procSync naming none of the three would reach the
   // single arm left to catch it while the banner named no mode at all. Each of 'T's C/F/S sub-options
   // replaces the other two and every preset sets one, so this is the guarantee that rule rests on rather
   // than a state a command line can produce; Constant is the mode the defaults carry (ISSUES.MD E10)
   if(!(cfg.procSync & 0x070)) cfg.procSync |= 0x010;

   // In a sweeping-pulse run the on-time is the whole cycle -- the ramp divides it into the active and idle
   // halves as the run progresses -- so an off-time is not part of the request. 'Ts' used to clear it as it
   // was parsed, which presets -6 and -7 never pass through: both set the sweep bit directly and left
   // offTime holding its 900ms default, so -6 ran a 2900ms cycle where its 2000ms on-time says 2000ms and
   // -7 ran 3400ms rather than 2500ms. Clearing it here covers every route into the mode, in any order the
   // options are given (ISSUES.MD E7)
   if(cfg.procSync & 0x040) cfg.offTime = 0;

   if(!(cfg.procSync & 0x010) && !cfg.onTime)      { wprintf(wstrMessage[17]); return -14; }

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
      threadCount[0] += (si16)SetBitCount64(cfg.sys.coreMap[0][j] & cfg.coreMap[j]);
      threadCount[1] += (si16)SetBitCount64(cfg.sys.coreMap[1][j] & cfg.coreMap[j]);
   }
   threadCount[2] = threadCount[0] + threadCount[1];

   // A 'U' map naming no core at all leaves nothing to divide the memory request between, and threadCount[2]
   // is the divisor two of the three memory configurations use unchecked (ISSUES.MD C8). The enumeration
   // already refuses a machine reporting no cores (-23), so this is the one remaining route to that divisor
   // being zero -- and the run it would otherwise reach prints an empty results table and returns 0, which
   // reads as a clean pass of a CPU that was never touched. Reachable in practice only since the 'U' maps
   // began to work (F2): the map could not previously be cleared
   if(!threadCount[2]) { wprintf(wstrMessage[40]); return -26; }

   // Check that the results file can be written, without writing it. The file itself is created after the
   // run, immediately before the report goes into it: it used to be created here with CREATE_ALWAYS -- before
   // the arena was sized, before the memory pre-flight, before a thread was spawned -- so every error return
   // between this point and the write ('-18', three '-17's and two '-19's) left a zero-length file where the
   // user's previous results had been, said nothing about having touched it, and leaked the handle on the way
   // out. 'PITC.exe -1 Mc999999 O[results.txt]' destroyed an existing results.txt and then failed the memory
   // check. A Ctrl-C during the run did the same, there being no console handler (ISSUES.MD C1).
   //
   // The path is still validated here rather than at the end, because 'O' takes it from the command line and
   // a run of hours should not discover a typo in it after the test has finished. OPEN_ALWAYS neither
   // truncates an existing file nor destroys one, and a file this check had to create is removed again, so a
   // run that never reaches its results leaves the filesystem as it found it
   if(wstrOut[0]) {
      outFile = CreateFileW(wstrOut, GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

      // OPEN_ALWAYS reports ERROR_ALREADY_EXISTS on a file it opened rather than created, and does so through
      // GetLastError on *success*, so the code has to be taken before any other call can overwrite it
      cbool fileExisted = (GetLastError() == ERROR_ALREADY_EXISTS);

      if(outFile == INVALID_HANDLE_VALUE) {
         wprintf(wstrMessage[10], wstrOut);
         return -9;
      }
      CloseHandle(outFile);
      if(!fileExisted) DeleteFileW(wstrOut);
   }

   // Cache-targeted block sizing. 'I1', 'I2' and 'I3' set procUnits bits 5~7, and until now nothing read them:
   // the banner named a cache level the run had never been sized to, in the saved results file as much as on
   // the console (ISSUES.MD A1). Deriving a block size is the whole of the feature -- a non-zero record count
   // already puts every thread onto the JobCycleMem* path -- so no kernel, dispatch entry or seeding pass
   // changes with it.
   //
   // The pass belongs exactly here. It needs the selection that will actually run, so it must follow
   // SetSMTLoading and the 'U' maps, and the -26 check that guarantees there is a thread to size for; it reads
   // the unit selection through RecordGeometry, so it must follow the -15/-16 checks that make that selection
   // valid; and the arena below is sized from what it writes, so it must precede the memConfig switch. It
   // performs no file handling, which is what keeps the probe above the last thing to touch the results file
   // before the run (ISSUES.MD C1)
   cui8 cacheLevel   = HighestCacheLevel(cfg.procUnits);
   ui64 recSize      = 0, vecUnits = 0; // Record geometry, hoisted out of the arena block below
   ui64 cacheSize[2] = { 0, 0 };        // Derived per-thread block size, per core class
   ui64 cacheLow[2]  = { 0, 0 };        // Smallest level-resident block size, per core class
   ui64 cacheHigh[2] = { 0, 0 };        // Largest level-resident block size, per core class
   ui32 feasibleK    = 0;               // Threads one instance of the level could hold resident, where none can

   RecordGeometry(ui8(cfg.procUnits & 0x01F), recSize, vecUnits);

   if(cacheLevel) {
      csi8 sized = CalcCacheBlockSizes(cacheLevel, recSize, threadCount, cacheSize, cacheLow, cacheHigh, feasibleK);

      // A level the system does not report cannot label a test, whether the sizes would have been derived from
      // it or merely checked against it, so the refusal stands ahead of the explicit-'M' branch rather than
      // inside it. The alternative is a fallback size, which puts a cache level's name on a run that nothing
      // sized to it -- the defect this whole pass exists to end
      if(sized < 0) { wprintf(wstrMessage[43], ui32(cacheLevel)); return -27; }

      // An 'M' given since the last reset states the size itself; its window is checked below the switch
      if(!cfg.memExplicit) {
         if(sized > 0) wprintf(wstrMessage[44], ui32(cacheLevel), feasibleK, ui32(cacheLevel));

         resArray.blockSize[0] = cacheSize[0];
         resArray.blockSize[1] = cacheSize[1];
         // The true total, so that the -17 pre-flight, malloc64 and the banner's MB figure each describe the
         // allocation this run is about to make rather than a request nobody typed
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

   // An explicit 'M' keeps its size, but a size outside the level's residency window makes the CL1/CL2/CL3 the
   // banner prints a claim that is not true of the run, so it is reported rather than silently accepted. The
   // check sits below the switch because that is what turns an 'Mt' total into the per-thread size it compares
   if(cacheLevel && cfg.memExplicit)
      for(ui8 cc = 0; cc < 2; ++cc)
         if(threadCount[cc] && (resArray.blockSize[cc] < cacheLow[cc] || resArray.blockSize[cc] > cacheHigh[cc]))
            wprintf(wstrMessage[45], ui32(cacheLevel), cacheLow[cc] >> 10, cacheHigh[cc] >> 10, ui32(cc));

   // Either class having been given a size is a memory-backed run. Testing the first alone meant 'Ms8', which
   // names the second class, allocated nothing at all: the banner reported the memory it had asked for while
   // every thread ran the register-resident kernels (ISSUES.MD C9). A class that has threads but no memory is
   // caught by the per-class record check below, which is where a request of nothing belongs
   if(resArray.blockSize[0] || resArray.blockSize[1]) {
      MEMORYSTATUSEX memStatus = { ui32(sizeof(MEMORYSTATUSEX)) };
      cbool memStatusValid     = GlobalMemoryStatusEx(&memStatus) ? true : false;
      ui64  bos;
      // l walks a thread class, whose population is an si16: as a ui8 it wrapped at 256 and the loop below
      // could not terminate, which the 64-virtual-core ceiling was all that kept out of reach (ISSUES.MD C11)
      si16  l;
      ui8   m;

      // recSize and vecUnits are the record geometry RecordGeometry (CPU.h) computed above, once, for this
      // block and for the cache sizing pass alike: a derived block size is a record count scaled by the record
      // size, so a second copy of the switch here is a second thing to keep in step with the unit set

      // Every JobCycleMem* call processes 4 records and the cursor in ComputationPulse advances in steps of
      // 4, so a count that is not a multiple of 4 is walked up to 3 records past the end of the slice.
      // The count is rounded to a multiple of 8 rather than 4 so that every slice begins on a cache line
      // (ISSUES.MD C12): a thread's base in each sub-array is its record offset scaled by that sub-array's
      // element size, and the ALU sub-array's is the smallest at 8 bytes, so 8 records is what makes both
      // offsets a whole number of 64-byte lines for every unit combination the switch above lists. It also
      // lands the ALU sub-array itself on a line, its base being the vector records of every thread scaled
      // by 8 bytes or more. Without it adjacent threads share the line at each boundary, and the false
      // sharing is measured as the cache behaviour this mode exists to exercise
      resArray.records[0] = (resArray.blockSize[0] / recSize) & ~0x07ull;
      resArray.records[1] = (resArray.blockSize[1] / recSize) & ~0x07ull;

      // A slice too small for a single call cannot be processed at all: a count of 0 also drops the thread
      // onto the register code path, with value[1][k].p0~p4 already overwritten with arena pointers. It is
      // also where a class given no memory at all arrives -- 'Mn8' names the first class and leaves the
      // second holding nothing (ISSUES.MD C9)
      for(m = 0; m < 2; ++m)
         if(threadCount[m] && !resArray.records[m]) {
            wprintf(wstrMessage[20], si64(resArray.blockSize[m]), si64(recSize << 3));
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
      // selection naming more than one of FPU/SSE2/AVX/AVX-512, so at most one of p0~p3 is written below.
      // bos is the running record offset into the arena: it counts across both thread classes, because
      // restarting it at the first class-1 thread would hand each of them a slice a class-0 thread already has
      for(k = 0, m = 0, bos = 0; m < 2; ++m)
         for(l = 0; l < threadCount[m]; ++k, ++l, bos += resArray.records[m]) {
            value[1][k].p0 = &resArray.avx512[bos];
            value[1][k].p1 = &resArray.avx[bos];
            value[1][k].p2 = &resArray.sse[bos];
            value[1][k].p3 = &resArray.fpu[bos];
            value[1][k].p4 = &resArray.alu[bos];
            // Filling a slice is a store of the unit's own width repeated -- 'p0[os] = value[0][k].avx512'
            // is a 512-bit move -- so each unit's pass is a call into the translation unit built for that
            // unit, and this file emits none of them (ISSUES.MD H4). At most two of the five run: wmain has
            // rejected any selection naming more than one of FPU/SSE2/AVX/AVX-512 by here, and the ALU
            // sub-array p4 addresses is the only one of the five that is not a view of the same records
            if(cfg.procUnits & 0x010) SeedRecordsAVX512(value[1][k].p0, resArray.records[m], value[0][k].avx512);
            if(cfg.procUnits & 0x08)  SeedRecordsAVX   (value[1][k].p1, resArray.records[m], value[0][k].avx);
            if(cfg.procUnits & 0x04)  SeedRecordsSSE   (value[1][k].p2, resArray.records[m], value[0][k].sse);
            if(cfg.procUnits & 0x02)  SeedRecordsFPU   (value[1][k].p3, resArray.records[m], value[0][k].fpu);
            if(cfg.procUnits & 0x01)  SeedRecordsALU   (value[1][k].p4, resArray.records[m], value[0][k].alu);
         }
   }

   // The thread bitmap prints one row per processor group, and every row after the first hangs under the
   // label wstrInterface[7] ends with, so the indent is that label's own width: the characters after its last
   // newline. It was a literal run of 15 spaces below, with a matching 'c -= 15' after the loop to trim the
   // last of them -- two copies of a constant that is a property of a *translated* string, so a language
   // whose label is not exactly as wide as "Thread bitmap: " misaligned every row after the first, and
   // nothing in the build or the run could see it (ISSUES.MD D1). Measured here, both follow the label
   si32 bitmapIndent = 0;

   for(j = 0; wstrInterface[7][j]; ++j) bitmapIndent = (wstrInterface[7][j] == L'\n' ? 0 : bitmapIndent + 1);

   // The results table is one row per thread per selected unit, and the widest row -- AVX-512, two 512-bit
   // values printed in hexadecimal -- is a little under 300 characters. wstrOutput was a fixed 32 768 wide
   // characters however many threads were running, which the table passes at about 150 (ISSUES.MD C7); the
   // 64-virtual-core ceiling is what kept that unreachable, so lifting it is what makes the sizing necessary.
   // At most two unit rows can be selected at once -- the ALU bit plus one of FPU/SSE/AVX/AVX-512 -- so a
   // kibicharacter per thread is a little over half as much again as the widest pair of rows can need. The
   // bitmap is one row per processor group rather than per thread, and 96 characters covers a row of 64 cores
   // and its newline; the indent is added rather than assumed, being a translated label's width now
   csi32 outChars = 4096 + si32(cfg.sys.groupCount) * (96 + bitmapIndent) + si32(threadCount[2]) * 1024;

   al64 declare1d64z(wchar, wstrOutput, outChars);

   // The buffer had no matching free on any path out of this function -- not on the eight error returns below
   // and not on the successful one, so a run of any length ended by handing back up to a mebibyte for the process
   // teardown to reclaim, against GCS rule p2 and against the rule the arena and the benchmark counters were
   // moved into ~RESULTS_ARRAYS to keep (ISSUES.MD C3, C13). The owner below frees it as its scope ends, by
   // whichever return that happens to be, and freeing null is a no-op so it stands above the check as safely
   // as below it. Nothing reads it: wstrOutput is still the pointer everything from here down is written
   // through, and no error return between here and the end of the function needs a free of its own
   cREPORT_BUFFER wstrOutputOwner = { wstrOutput };

   if(!wstrOutput) {
      wprintf(wstrMessage[22], (si64(outChars) * si64(sizeof(wchar))) >> 20);
      return -17;
   }

   // Wide, as every write this program makes to stdout is. C and C++ give a stream an orientation at its
   // first use and forbid byte I/O on a wide-oriented one, and every other write here -- the banner below,
   // the results table, each message -- is a wprintf; the byte spelling of these three newlines and of the
   // two progress dots worked only because the MSVC CRT deliberately implements no orientation at all, and
   // would have been the first thing to break under any other runtime (ISSUES.MD F4)
   wprintf(L"\n");
   // RULE-DEV:C4996 The report below is built with the two-argument swprintf, which MSVC deprecates in favour
   // of the count-taking ISO form; every call in this region is one, and there is nothing else here that
   // C4996 has anything to say about. The suppression used to be a bare '#pragma warning(disable:4996)' at
   // the top of the file, which silenced every deprecation diagnostic in the whole of CPU.cpp -- the parser,
   // the arena, the thread spawn and the file I/O included -- so a deprecated call added anywhere in it would
   // have compiled without a word. It is now pushed and popped around the two regions that need it, which is
   // this one and the results table below (ISSUES.MD H10). The calls themselves are unbounded, which is a
   // separate defect and a separate entry (C7); wstrOutput is sized for the widest report a run can produce
#pragma warning(push)
#pragma warning(disable:4996)
   // Output configuration properties
   c = swprintf(wstrOutput, wstrInterface[0]);
   for(i = 0, j = 0; i < 8; ++i) if(cfg.procUnits & (0x01ull << i)) { c += swprintf(&wstrOutput[c], L" %s", wstrUnitsCPU[i]); ++j; }
   for(; j < 3; j++) c += swprintf(&wstrOutput[c], L"    ");
   // cfg.allocMem[0] is the whole of the allocation in every memory configuration: the switch above leaves it
   // holding the total, and allocMem[1] is a per-thread size that has already been counted into it. Adding
   // the two reported 'Mn8 Ms4' as the true total plus 4MB, and stayed invisible in the other two
   // configurations only because allocMem[1] then defaulted to a single byte, which vanished under the
   // shift (ISSUES.MD F8; that default is 0 since C9)
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
      // '%*s' over an empty string is the indent measured above, spelt once. The last group's is written and
      // then trimmed by the same width, which leaves its newline and ends the bitmap where the banner resumes
      c += swprintf(&wstrOutput[c], L"\n%*s", bitmapIndent, L"");
   }
#pragma warning(pop)
   c -= bitmapIndent;
   // wstrOutput is built at run time, so passing it as the format string makes every '%' it ever comes to
   // hold a conversion specifier reading arguments that were never passed. Nothing can put one there today,
   // but wstrOut -- a filename straight off the command line -- is already interpolated into messages
   // elsewhere, and one edit is all it would take (ISSUES.MD F11)
   wprintf(L"%s", wstrOutput);
   wprintf(L"\n"); // Wide, as every write to stdout is; see the note above the banner (ISSUES.MD F4)

   // The one clock reading every thread's deadlines are derived from, and the reason it is taken here: the
   // arena is allocated and seeded between the option loop and this point, which writes every byte of the
   // request -- hundreds of milliseconds for 'Mc8' across many threads, and far longer for a multi-gigabyte
   // one. Taken before that, the reading was stale by the whole of the seeding pass: the start-up delay was
   // silently spent on it, and once seeding outlasted the delay the computed startTics was already in the
   // past, so the threads began unsynchronised and endTics had moved earlier by the same amount -- a test
   // shorter than the duration that was asked for (ISSUES.MD E11). It is also the single reading that gives
   // every thread an identical startTics, which is what the time-synchronised shapes are built on
   timer.Update();

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
         // rather than after it has already begun running on whichever core the scheduler chose. The handle
         // is kept until the thread has been waited on rather than closed at the end of this iteration: the
         // completion bit that releases the wait loop below is cleared from inside the thread, several
         // statements and one CRT thread shutdown before the thread ends (ISSUES.MD D8)
         threadHandle[d] = (HANDLE)_beginthreadex(0, 0, ComputationPulse, &threadData[d], CREATE_SUSPENDED, 0);

         GROUP_AFFINITY affinity = {}; // Its Reserved[3] must be zero; the initialiser is what guarantees it

         // A thread that never starts never clears its completion bit, hanging the wait loop below forever.
         // The threads already spawned are running the test they were given and are not waited on here; the
         // handles are released because this function is leaving with an error rather than a result
         if(!threadHandle[d]) {
            ClearThreadRunning(d); ReleaseThreads(threadHandle, d); wprintf(wstrMessage[23], d); return -19;
         }

         if(NextSelectedCore(mask, coreGroup, ui8(j))) {
            affinity.Mask  = mask;
            affinity.Group = ui16(coreGroup);
         }

         // SetThreadAffinityMask takes a bare 64-bit mask, which the OS reads as a mask of the thread's own
         // processor group, so it cannot pin a thread to a core of any other group and fails outright once
         // the walk has shifted past the last selected bit of group 0 (ISSUES.MD G3). SetThreadGroupAffinity
         // names the group. A cursor that found no further core leaves an empty mask, which it rejects --
         // the same warning as any other affinity the OS will not accept
         if(!SetThreadGroupAffinity(threadHandle[d], &affinity, 0)) wprintf(wstrMessage[24], d);

         if(ResumeThread(threadHandle[d]) == DWORD(-1)) {
            ClearThreadRunning(d); ReleaseThreads(threadHandle, d + 1); wprintf(wstrMessage[23], d); return -19;
         }
      }
   }
   while(ThreadsRunning()) Sleep(100);

   // Every thread has cleared its completion bit, which it does from inside ComputationPulse: the results
   // table and the KUPS score below are read, and ~RESULTS_ARRAYS frees the arena those threads work over,
   // while the threads themselves are still executing. Waiting on the handles is what makes them finished
   // rather than nearly so (ISSUES.MD D8)
   JoinThreads(threadHandle, threadCount[2]);

   // Output results
   wprintf(L"\n"); // Wide, as every write to stdout is; see the note above the banner (ISSUES.MD F4)
   // RULE-DEV:C4996 The second of the two regions built with the two-argument swprintf; see the note above
   // the configuration banner
#pragma warning(push)
#pragma warning(disable:4996)
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
         // The expected and observed lanes of this row, bound once. Spelt out per lane, the AVX-512 row ran
         // to 224 columns and the AVX row to 209, against the 180 GCS e2 makes a hard cap (ISSUES.MD K8).
         // Both are the union's ui64 view: this table prints bit patterns, which is the same question the
         // job cycles ask of them, and RESULTS orders that view widest unit first
         cui64ptrc expect = value[2][i].raw;
         cui64ptrc actual = value[3][i].raw;

         // One row format per value width, wstrInterface[18]~[21]. They are the language's rather than this
         // file's because their cells have to line up under the column headers of [8] and [9], which a
         // language already owned: the row was half of a layout whose other half could be translated, and the
         // three vector rows named their unit -- "SSE  128", "AVX  256", "AVX  512" -- in English besides
         // (ISSUES.MD D1). The lane indices and the Evaluate unit numbers stay here, being properties of the
         // RESULTS union's widest-unit-first ordering rather than of any language
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
      // The 64-bit values one job cycle updates in each record: the widest vector unit, which is what the
      // dispatch table selects, plus the ALU's own lane where the ALU bit is set. The weight was
      // '(procUnits & 0x1F) >> 1', an arithmetic accident of the bit-field that yields a vector width only
      // for the three combinations 'B' itself installs -- 'Iasv' weighted by 6, a width no unit has, while
      // the run executed the AVX kernel JOB_CYCLE had selected. It described what was requested, where the
      // score has to describe what was dispatched (ISSUES.MD E12)
      csi64 unitLanes = si64(cfg.procUnits & 0x010 ? 8 : cfg.procUnits & 0x08 ? 4 : cfg.procUnits & 0x04 ? 2 : cfg.procUnits & 0x02 ? 1 : 0) +
                        si64(cfg.procUnits & 0x01 ? 1 : 0);
      si64  accum     = 0;

      for(i = 0; i < threadCount[2]; ++i) accum += resArray.iter[i];

      // resArray.iter counts records rather than loop iterations, so accum * unitLanes is a count of values
      // updated whichever code path ran: one iteration is four records in memory-backed mode against one in
      // register mode, and it counted the idle iterations of a pulsed run as well. The rate is taken from
      // the tic count, not from a count of whole seconds -- 'B Tt0.5' divided by an integer zero (E4) -- and
      // in fl64, which carries a product si64 would overflow at a few hundred thread-hours of AVX-512
      c += swprintf(&wstrOutput[c], wstrInterface[10],
                    si64(fl64(accum) * fl64(unitLanes) * fl64(timer.siFrequency) / fl64(cfg.tics)) >> 10);
   }
   c += swprintf(&wstrOutput[c], L"\n");
#pragma warning(pop)

   // Write outputs to console and/or file
   wprintf(L"%s", &wstrOutput[d]); // A run-time buffer is never a format string (ISSUES.MD F11)
   if(wstrOut[0]) {
      // Created here, with results in hand, rather than before the run: CREATE_ALWAYS truncates whatever it
      // opens, and until there is a report to write there is nothing to destroy the user's previous file for
      // (ISSUES.MD C1). The path was probed for writability before the test began, so the failure this
      // reports is one that arose during the run -- a removed drive, a file made read-only under us -- and
      // the report has already gone to the console above either way
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
         // It converted through wcstombs, which uses the LC_CTYPE locale wmain installs -- on most Windows
         // installations the system ANSI code page. The '8' option therefore wrote ANSI bytes behind
         // a UTF-8 byte-order mark, and every non-ASCII character in the file was mojibake to anything that
         // believed the mark (ISSUES.MD F6). WideCharToMultiByte names the encoding the option asked for and
         // reports the exact byte count that encoding needs, rather than the (size_t)-1 that used to be
         // handed to WriteFile as a length -- and the flags make what an encoding cannot carry loud.
         // WC_ERR_INVALID_CHARS fails the UTF-8 path on malformed UTF-16, and WC_NO_BEST_FIT_CHARS makes the
         // ANSI path turn any character outside its code page into the default character and say so through
         // usedDef, where its best-fit mappings substituted lookalikes in silence. That path is warned about
         // (wstrMessage[46]) rather than failed: the report of a run of hours has already been paid for, and
         // 'O8'/'O16' are the documented route to keeping every character of it (ISSUES.MD D2). Both calls
         // pass the same flags, so the byte count the first reports is the count the second produces.
         // The ANSI arm asks GetACP() first, and takes the UTF-8 flags whenever the answer is UTF-8: the API
         // validates its flags against the code page CP_ACP *resolves to*, and for code page 65001 -- which
         // the "Use Unicode UTF-8 for worldwide language support" setting makes the machine's ANSI code
         // page -- it refuses WC_NO_BEST_FIT_CHARS and a non-null usedDef argument outright, so keying the
         // flags off the option letter alone failed every default-encoding write on such a machine. Nothing
         // is lost by the substitution: UTF-8 has no best-fit mappings and can represent every character, so
         // the wstrMessage[46] warning correctly cannot fire there.
         // The character count c is passed rather than -1, so no terminating null reaches the file
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
