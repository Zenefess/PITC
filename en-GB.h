/************************************************************
 * File: en-GB.h                        Created: 2025/02/10 *
 *                                    Last mod.: 2025/02/21 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

al64 cwchptrc wstrInstructions_English =
L"\nPulsed Integrity Tests for CPUs v1.0   ---   Copyright (c) David William Bull\n"
 "\nReturn values"
 "\n-------------"
 "\n-1  : File containing correct values not found                                  0 : Successul completion of stability test"
 "\n-2  : Insufficient input entries found in file"
 "\n-3  : Insufficient output entries found in file                                 1 : Correct values successfully saved to file"
 "\n-4  : Computational errors detected while generating correct values"
 "\n-5  : Unable to create file for correct values                                  2 : Instructions displayed to console"
 "\n-6  : Failed to write all correct input values to file"
 "\n-7  : Failed to write all correct output values to file"
 "\n-8  : Invalid filename for results file"
 "\n-8  : Unable to create results file"
 "\n-10 : Failed to write results file\n"
 "\nCommand-line options   ---   Example: pitc.exe I3x Mc8 Spt Tcd8.0t3600 Ua"
 "\n--------------------"
 "\n B  : Run the benchmark. Options after 'B' override defaults; eg. pitc.exe B Iaf mt1024 !!! CACHE USE NOT YET IMPLEMENTED !!!"
 "\n      Utilises the ALU and largest vector unit of all (virtual) cores in the system, level 3 cache, and 8MB memory per thread for 60 seconds."
 "\n Ix : Set intruction usage options. Specifies which units to utilise. Options can be stacked; eg. I2av !!! CACHE USE NOT YET IMPLEMENTED !!!"
 "\n      Caches: 1==Level 1, 2==Level 2, 3==Level 3  |  Processing: A==ALU, F==FPU, S==SSE4.1, V==AVX2, X==AVX512"
 "\n Lx : Set interface language."
 "\n      Recognises ISO 639-1 language codes; eg. Len-GB"
 "\n Mx : Set amount of memory to utilise during test. Values are in MebiBytes; eg. Mt128"
 "\n      C==Per virtual core, N==Per non-SMT core, S==Per SMT virtual core, T=Total split amongst all virtual cores"
 "\n Ox : Results file output options. A filename can be stacked with any of the remaining options; eg. O[results.txt]16"
 "\n      []=Filename, A=Non-UTF ASCII, 8=UTF-8, 16=UTF-16"
 "\n Sx : Set core synchronisation options. The first three options (P,R,S) can be stacked with the last (T); eg. Spt"
 "\n      P==Parallel, R==Round-robin, S==Staggered, T==Time synchronised"
 "\n Tx : Set timing options. One of the first three options (C,F,T) can be stacked with any of the remaining (D,T,[,]); eg. Tfd1.0t12.5[100]2400"
 "\n      C==Constant, F==Fixed-length pulses, S==Sweeping-length pulses"
 "\n      Global options: Dx==Set start-up delay, Tx==Set test duration                             |  Replace 'x' with a decimal value; eg. d10.0"
 "\n      Fixed-length pulse options (in milliseconds): [x==Active duration, ]x==Inactive duration  |  Replace 'x' with a whole number; eg. [250"
 "\n      Sweeping-length pulse option (in milliseconds): [x==Cycle duration"
 "\n Ux : Set core usage options. One of the first two options (C,T) can be stacked with the one of the remaining (S, O); eg. Uc!.!!...!s"
 "\n      C==Binary sequence map of physical cores to utilise, T==Binary sequence map of virtual cores to utilise"
 "\n         Format for core utilisation map is: ','/'.'/'_'==Core disabled, Any other character==Core enabled"
 "\n      A==Forces utilisation of every virtual core of each active physical core"
 "\n      E==Only utilise the first virtual core of each active physical core, O==Only utilise the last virtual core of each active physical core"
 "\n W  : Write new \"cpu.values\" file."
 "\n      File will only be created if the integrity of the results pass 65,536 iterations."
 "\n -x : Configuration presets."
 "\n      1==Constant stress; one thread per physical core. 10 minute duration"
 "\n      2==Constant stress on all virtual cores. 30 minute duration"
 "\n      3==Fixed-width round-robin pulsed stress; one thread per physical core. 10 minute duration"
 "\n      4==Synchronised fixed-width pulsed stress; one thread per physical core. 10 minute duration"
 "\n      5==Synchronised fixed-width pulsed stress on all virtual cores. 30 minute duration"
 "\n      6==Synchronised sweeping-width pulsed stress; one thread per physical core. 30 minute duration"
 "\n      7==Synchronised sweeping-width pulsed stresson all virtual cores; 30 minute duration. 30 minute duration"
 "\n      8==Synchronised staggered fixed-width pulsed stress; one thread per physical core. 1 hour duration"
 "\n      9==Synchronised staggered fixed-width pulsed stress on all virtual cores. 4 hour duration\n\n";

cwchptrc wstrMessage_English[15] = {
   L"\nSuccessfully wrote results to \"%s\" file.\n\n",
   L"\n\nNew \"cpu.values\" file generated.\n\n",
   L"\n\n\"cpu.values\" file not found. Generate via 'W' command-line option.\n\n",
   L"\n\nInsufficient input entries in \"cpu.values\" file.\n\n",
   L"\n\nInsufficient output entries in \"cpu.values\" file.\n\n",
   L"\n\nComputational error(s) detected. Results not written.\n\n",
   L"\n\nCannot create \"cpu.values\" file.\n\n",
   L"\n\nFailed to write all input entries to \"cpu.values\" file.\n\n",
   L"\n\nFailed to write all output entries to \"cpu.values\" file.\n\n",
   L"\n\nUnable to create file \"%s\".\n\n",
   L"\n\nCannot create \"%s\" file.\n\n",
   L"\n\nFailed to write results to \"%s\" file.\n\n",
   L"\nSystem processor cores do not support the SSE4.1 instruction set.\n",
   L"\nSystem processor cores do not support the AVX2 instruction set.\n",
   L"\nSystem processor cores do not support the AVX512F instruction set.\n"
};

cwchptrc wstrInterface_English[13] = {
   L"Units:",
   L"\t Memory allocated: %3lldMB\tStart-up delay: %7dms",
   L"\t Pulse on-time: %dms",
   L"\tCycle time: %dms",
   L"\nSync: ",
   L"\t     Thread count: %-3d  \tMaximum duration: %5.1fs",
   L"\tPulse off-time: %dms",
   L"\n\nThread bitmap: ",
   L"\n Thread | ProcUnit | Correct values   ",
   L"| Result\n--------+----------+--",
   L"\nPITC benchmark score: %lld",
   L"\nERROR! Core: %2.1lld  Expected: ",
   L"Output:"
};
