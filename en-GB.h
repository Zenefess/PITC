/*
 * File: en-GB.h
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-02-10
 * Last Modified: 2026-08-18
 * Description: English (en-GB) text: the option reference and return codes, the message table, the interface fragments and the label tables.
 * To Do: 1) Take the banner's version from one authoritative site rather than restating it here as prose (ISSUES.MD K3)
 * Dependencies: None
 * ISA: Scalar
 * Thread-safety: N/A
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
#pragma once

// The three prose tables are 'inline' for the reason the three label tables at the end always were: the
// LANGUAGES registry in translations.h is an inline table that takes their addresses, and an inline
// definition must mean one entity in every translation unit, which a const table at namespace scope -- being
// internally linked -- is not (ISSUES.MD D2, H9)
inline al64 cwchptrc wstrInstructions_English =
L"\nPulsed Integrity Tests for CPUs v1.0.2   ---   Copyright (c) David William Bull\n"
 "\nReturn values"
 "\n-------------"
 "\n-1  : File containing correct values not found                                  0 : Successful completion of stability test"
 "\n-2  : Insufficient input entries found in file"
 "\n-3  : Insufficient output entries found in file                                 1 : Correct values successfully saved to file"
 "\n-4  : Computational errors detected while generating correct values"
 "\n-5  : Unable to create or replace the file for correct values                   2 : Instructions displayed to console"
 "\n-6  : Failed to write all correct input values to file"
 "\n-7  : Failed to write all correct output values to file"
 "\n-8  : Invalid filename for results file"
 "\n-9  : Unable to create results file"
 "\n-10 : Failed to write results file"
 "\n-11 : Requested processing unit not supported by the CPU"
 "\n-12 : More than one thread synchronisation option requested"
 "\n-13 : Test duration of zero or less requested"
 "\n-14 : Pulse on-time of zero requested"
 "\n-15 : No processing unit requested"
 "\n-16 : More than one non-ALU processing unit requested"
 "\n-17 : Unable to allocate the requested amount of memory"
 "\n-18 : Insufficient memory per thread for the requested processing unit(s)"
 "\n-19 : Unable to create a computation thread"
 "\n-20 : Unable to write the \"cpu.values\" header"
 "\n-21 : Contents of \"cpu.values\" are not valid for this build"
 "\n-22 : A job kernel disagrees with the kernel it is required to reproduce"
 "\n-23 : Unable to enumerate the processor topology of the system"
 "\n-24 : Missing, malformed or out-of-range value for a command-line option"
 "\n-25 : Unrecognised command-line option"
 "\n-26 : The selected core map contains no cores to test"
 "\n-27 : Requested cache level not reported by the system\n"
 "\nCommand-line options   ---   Example: pitc.exe I3x Spt Tcd8.0t3600 Ua"
 "\n--------------------"
 "\n      Options are applied in the order given: where two of them set the same property, the last one wins."
 "\n      'B' and the presets reset the processing units and the memory configuration, so give either of them"
 "\n      before any 'I' or 'M' option. A sweeping-pulse run has no off-time, so ']' is ignored wherever it is given."
 "\n      Every test is graded against the \"cpu.values\" file, so generate it with 'W' before the first test of a build."
 "\n      Defaults, where no option and no preset sets otherwise: the ALU & FPU of every virtual core, parallel constant"
 "\n      execution, no memory (the register-resident kernels), a 2000ms start-up delay and a 15 minute duration. The"
 "\n      100ms on-time and 900ms off-time are what a pulsed mode selected without '[' or ']' inherits."
 "\n B  : Run the benchmark. Options after 'B' override defaults; eg. pitc.exe B Iaf mt1024"
 "\n      Utilises the ALU and largest vector unit of all (virtual) cores in the system, and 8MB memory per thread for 60 seconds."
 "\n Ix : Set instruction usage options. Specifies which units to utilise. Options can be stacked; eg. I2av"
 "\n      Caches: 1==Level 1, 2==Level 2, 3==Level 3                                                |  The highest cache level given is used"
 "\n      Processing: A==ALU, F==FPU, S==SSE2, V==AVX, X==AVX512                                    |  F, S, V and X are mutually exclusive"
 "\n         At least one processing unit is required; a cache level names no unit of its own and is optional"
 "\n         A cache level sizes the memory per thread to it: the blocks of every selected thread sharing one instance of that level"
 "\n         fill it, and together overflow the level below. An 'M' option overrides the derived sizes, and a level this system does"
 "\n         not report is refused with -27 rather than tested at some other size"
 "\n Lx : Set interface language."
 "\n      The code is matched case-insensitively against those this build carries: en-GB & en-US; eg. Len-GB"
 "\n         An unrecognised code is warned about and leaves the language unchanged"
 "\n Mx : Set amount of memory to utilise during test. Values are in MebiBytes; eg. Mt128"
 "\n      C==Per virtual core, N==Per first-class core, S==Per second-class virtual core, T==Total split amongst all virtual cores"
 "\n         The two core classes are the CPU's non-SMT and SMT cores; on a hybrid CPU they are its efficiency and performance cores"
 "\n         'N' and 'S' each cover one class only: where the CPU has cores of both, give both, or the class left without memory is refused"
 "\n         An 'M' overrides the sizes an 'I1', 'I2' or 'I3' would derive; a size outside that level's residency window is warned about"
 "\n Ox : Results file output options. A filename can be stacked with any of the remaining options; eg. O[results.txt]16"
 "\n      []=Filename, A=Non-UTF ASCII, 8=UTF-8, 16=UTF-16"
 "\n Sx : Set core synchronisation options. One of the first three options (P,R,S) can be stacked with the last (T); eg. Spt"
 "\n      P==Parallel, R==Round-robin, S==Staggered, T==Time synchronised                           |  P, R and S are mutually exclusive"
 "\n         'T' aligns every thread's pulse edges. Without it a parallel run offsets each thread by a random fraction of a cycle"
 "\n Tx : Set timing options. One of the first three options (C,F,S) can be stacked with any of the remaining (D,T,[,]); eg. Tfd1.0t12.5[100]2400"
 "\n      C==Constant, F==Fixed-length pulses, S==Sweeping-length pulses                            |  The last of C, F and S given is used"
 "\n      Global options: Dx==Set start-up delay, Tx==Set test duration                             |  Replace 'x' with a decimal value; eg. d10.0"
 "\n      Fixed-length pulse options (in milliseconds): [x==Active duration, ]x==Inactive duration  |  Replace 'x' with a whole number; eg. [250"
 "\n      Sweeping-length pulse option (in milliseconds): [x==Cycle duration                        |  A sweep has no off-time"
 "\n         Each cycle begins idle and the duty cycle rises in a straight line to 100% at the end of the test duration"
 "\n Ux : Set core usage options. One of the first two options (C,T) can be stacked with one of the remaining (A,E,O); eg. Uc!.!!...!a"
 "\n      C==Binary sequence map of physical cores to utilise, T==Binary sequence map of virtual cores to utilise"
 "\n         Core disabled: '.' ',' '_' '-' '0'  |  Core enabled: '!' '*' '#' '+' '1' 'x' 'X'  |  Any other character ends the map"
 "\n         The map is the whole selection: a core it does not name is not utilised, and an empty selection is refused"
 "\n         'C' numbers the physical cores in sequence, group after group. 'T' gives every processor group 64 characters"
 "\n         however many virtual cores it holds, so the characters past a group's last core are padding that must still be"
 "\n         written to reach the next group; the thread bitmap prints each group to its own width, not to 64"
 "\n      A==Symmetric Multi-Threading; forces utilisation of every virtual core of each active physical core"
 "\n      E==Only utilise the first virtual core of each active physical core, O==Only utilise the last virtual core of each active physical core"
 "\n         Both keep one virtual core per active physical core, whatever its SMT width; a core carrying only one is kept by either"
 "\n W  : Write new \"cpu.values\" file."
 "\n      The file is built as \"cpu.values.tmp\" and moved into place once it is complete, so an interrupted"
 "\n      run leaves any previous \"cpu.values\" exactly as it was."
 "\n      File will only be created if the integrity of the results pass 65,536 iterations."
 "\n      All 512 entries are verified, not one per thread, so expect the check to run for minutes rather than seconds."
 "\n      The job kernels are cross-checked first: each memory and combined kernel against the register-resident kernel of its"
 "\n      own unit, and each vector kernel against the FPU kernel lane for lane, which is what makes the file readable on a CPU"
 "\n      of a different vector width."
 "\n -x : Configuration presets. By default will use the ALU & the largest vector unit, and 8MB memory per core."
 "\n      1==Constant stress; one thread per physical core. 10 minute duration"
 "\n      2==Constant stress on all virtual cores. 30 minute duration"
 "\n      3==Fixed-width round-robin pulsed stress; one thread per physical core. 10 minute duration"
 "\n      4==Synchronised fixed-width pulsed stress; one thread per physical core. 10 minute duration"
 "\n      5==Synchronised fixed-width pulsed stress on all virtual cores. 30 minute duration"
 "\n      6==Sweeping-width pulsed stress; one thread per physical core. 30 minute duration"
 "\n      7==Synchronised sweeping-width pulsed stress on all virtual cores. 30 minute duration"
 "\n      8==Staggered fixed-width pulsed stress; one thread per physical core. 1 hour duration"
 "\n      9==Synchronised staggered fixed-width pulsed stress on all virtual cores. 4 hour duration"
 "\n      0==Synchronised fixed-width pulsed stress on all virtual cores, using ALU & SSE code-paths with 2MB memory per core. 1 hour duration\n\n";

inline cwchptrc wstrMessage_English[47] = {
   L"\nSuccessfully wrote results to \"%s\" file.\n\n",
   L"\n\nNew \"cpu.values\" file generated.\n\n",
   L"\n\n\"cpu.values\" file not found. Generate via 'W' command-line option.\n\n",
   L"\n\nInsufficient input entries in \"cpu.values\" file.\n\n",
   L"\n\nInsufficient output entries in \"cpu.values\" file.\n\n",
   L"\n\nComputational error(s) detected. Results not written.\n\n",
   L"\n\nCannot create the \"%s\" file.\n\n",
   L"\n\nFailed to write all input entries to \"cpu.values\" file.\n\n",
   L"\n\nFailed to write all output entries to \"cpu.values\" file.\n\n",
   L"\nNo valid filename for the results file in the argument \"%s\"; expected 'O[name]'.\n\n",
   L"\n\nCannot create \"%s\" file.\n\n",
   L"\n\nFailed to write results to \"%s\" file.\n\n",
   L"\nSystem processor cores do not support the SSE2 instruction set.\n",
   L"\nSystem processor cores do not support the AVX instruction set.\n",
   L"\nSystem processor cores do not support the AVX512F instruction set.\n",
   L"\nOnly one of the 'S' options P, R and S can be active; they are mutually exclusive.\n",
   L"\nTest duration must be greater than zero.\n",
   L"\nPulse on-time must be greater than zero.\n",
   L"\nAt least one processing unit must be selected via the 'I' option; eg. Ia\n",
   L"\nOnly one of the 'I' options F, S, V and X can be active; they are mutually exclusive.\n",
   L"\nOnly %lld bytes of memory per thread; the requested processing unit(s) require at least %lld.\n",
   L"\nRequested %lldMB of memory, but only %lldMB is available.\n",
   L"\nUnable to allocate %lldMB of memory.\n",
   L"\nUnable to create computation thread #%d.\n\n",
   L"\nWARNING: Unable to pin thread #%d to a core; it will run wherever the scheduler places it.\n",
   L"\n\nFailed to write the header of the \"cpu.values\" file.\n\n",
   L"\n\n\"cpu.values\" is not a PITC values file. Generate via 'W' command-line option.\n\n",
   L"\n\n\"cpu.values\" is in format version %u; this build reads version %u. Regenerate via 'W'.\n\n",
   L"\n\n\"cpu.values\" was generated by a different build, or by different job kernels. Regenerate via 'W'.\n\n",
   L"\n\n\"cpu.values\" is corrupt; its contents do not match the hashes in its header. Regenerate via 'W'.\n\n",
   L"\n\nThe %s kernel does not agree with the register-resident kernel for its unit. \"cpu.values\" not written.\n\n",
   L"\nUnable to enumerate the processor topology of the system; error code %u.\n\n",
   L"\nThe system reported no processor cores; there is nothing to test.\n\n",
   L"\nWARNING: The system has %d virtual cores; this build tests at most %d, so %d will not be tested.\n",
   L"\nHybrid CPU: %d performance core(s) at %d-way SMT, and %d efficiency core(s) at %d-way SMT.\n"
    "  The two core classes are those rather than non-SMT and SMT cores, so 'Mn' and the first cache record\n"
    "  describe the efficiency cores, and 'Ms' and the second describe the performance cores.\n",
   L"\nThe '%c' option of the argument \"%s\" requires a whole number from %lld to %lld.\n\n",
   L"\nThe '%c' option of the argument \"%s\" requires a decimal value from %.1f to %.1f.\n\n",
   L"\nUnrecognised command-line argument \"%s\". Run with no arguments for the option reference.\n\n",
   L"\nUnrecognised '%c' option in the argument \"%s\". Run with no arguments for the option reference.\n\n",
   L"\nWARNING: The language \"%s\" is not available in this build; the interface language is unchanged.\n",
   L"\nThe 'U' core map selected no cores; there is nothing to test.\n\n",
   L"\n\nThe %s kernel does not compute JobFPU element-wise, so a \"cpu.values\" written here would not be\n"
    "  readable on a CPU of a different vector width. \"cpu.values\" not written.\n\n",
   L"\n\nUnable to replace the \"%s\" file; any previous one has been left exactly as it was.\n\n",
   L"\nThe system does not report a level %u cache, so a test cannot be sized to one here.\n\n",
   L"\nWARNING: Level %u working sets cannot be made resident at this thread count; running at the smallest\n"
    "  size that defeats the level below. At most %u thread(s) per level %u cache instance could have been\n"
    "  held resident, so select fewer cores to test the level itself.\n",
   L"\nWARNING: The requested memory per thread is outside the level %u residency window of %llu ~ %llu KiB\n"
    "  for the class-%u cores, so this run is not confined to that cache level.\n",
   L"\nWARNING: The ANSI encoding of the \"%s\" file cannot represent every character of this language, so\n"
    "  those characters were written as substitutes. Request 'O8' or 'O16' to keep them.\n"
};

inline cwchptrc wstrInterface_English[22] = {
   L"Units:",
   L"\t Memory allocated: %3lldMB\tStart-up delay: %7dms",
   L"\t Pulse on-time: %dms",
   L"\tCycle time: %dms",
   L"\nSync: ",
   L"\t     Thread count: %-3d  \tMaximum duration: %5.1fs",
   L"\tPulse off-time: %dms",
   // The characters of [7] after its last newline are measured at run time: they are the hanging indent every
   // thread-bitmap row after the first is given, and the amount trimmed after the last, so the label may be
   // translated to any width -- but it must not end in a tab, which the measure counts as one column and a
   // console renders as several
   L"\n\nThread bitmap: ",
   L"\n Thread | ProcUnit | Correct values   ",
   L"| Result\n--------+----------+--",
   L"\nPITC benchmark score: %lld KUPS (Kibi-units per second)",
   L"\nERROR! Core: %2.1lld  Expected: ",
   L"Output:",
   // [13]~[17]: the value line Failed() prints after [11], one per processing unit, in the order of the 'unit'
   // argument that selects them: 0==AVX-512, 1==AVX, 2==SSE, 3==FPU, 4==ALU. Each carries the expected lanes,
   // then [12], then the observed lanes, and the specifier count of each is fixed by the lanes of its unit --
   // sixteen, eight, four, two and two conversions respectively, in that order and no other. This is the one
   // output a failing CPU produces, so it was the worst of the report to have left untranslatable
   L"%1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  %s %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n",
   L"%1.9f, %1.9f, %1.9f, %1.9f  %s %1.9f, %1.9f, %1.9f, %1.9f\n",
   L"%1.9f, %1.9f  %s %1.9f, %1.9f\n",
   L"%1.9f  %s %1.9f\n",
   L"%lld  %s %lld\n",
   // [18]~[21]: one results-table row per value width -- 64-bit, 128-bit, 256-bit and 512-bit. They sit here
   // rather than in CPU.cpp because their cells have to line up under the column headers of [8] and [9], which
   // a language owns: the thread number, the ProcUnit cell and the two value cells are one layout with those
   // headers, and half of it could not previously be translated. Every row renders the ProcUnit cell 10
   // characters wide, and the rule below [9] is drawn to that width, so a row that renders it otherwise puts
   // the table's own separator out of true.
   // [18] serves both units of the 64-bit width and takes their name from wstrUnitsCPU; the three vector rows
   // name their unit in the literal, so a language that respells wstrUnitsCPU[2]~[4] respells these to match
   L"\n  #%3.1d  |  %s 64  | %16.16llX | %16.16llX | %s",
   L"\n  #%3.1d  | SSE  128 | %16.16llX%16.16llX | %16.16llX%16.16llX | %s",
   L"\n  #%3.1d  | AVX  256 | %16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX | %s",
   // Sixteen conversions and their separators are 224 columns on one line, against the 180 GCS e2 makes a hard
   // cap; a wide literal is the one token here that cannot be broken any other way, so the row is joined by
   // concatenation exactly as the multi-line messages above are
   L"\n  #%3.1d  | AVX  512 | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX"
    " | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %s"
};

// The bit-indexed label tables. Both are indexed by the bit position of the property they name, so their order
// is fixed by GLOBAL_CFG's bit-fields and not by this file, and every entry must be present in every language.
// Their array type is what holds them to three characters: the banner prints each selected label in a slot of
// four columns and pads the unused slots with four spaces each, and the results table gives wstrUnitsCPU a
// fixed cell, so a wider label would silently shift every column to its right (see translations.h)
inline cwchar wstrUnitsCPU_English[8][4] = { L"ALU", L"FPU", L"SSE", L"AVX", L"512", L"CL1", L"CL2", L"CL3" };
inline cwchar wstrSyncCPU_English[8][4]  = { L"R-R", L"Par", L"Sta", L"T-S", L"Con", L"F-P", L"S-P", L"Ben" };
// The verdict of one results row, indexed by Evaluate: 0==every lane matched, 1==at least one did not
inline cwchar wstrPass_English[2][8]     = { L".Pass.", L"!Fail!" };
