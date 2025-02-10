/************************************************************
 * File: instructions.h                 Created: 2025/02/10 *
 *                                    Last mod.: 2025/02/10 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

al64 cwchar wstrInstructions_ENG[] =
L"\nPulsed Integrity Tests for CPUs v1.0a   ---   Copyright (c) David William Bull\n"
 "\nCommand-line options   ---   Example: pitc.exe I3x M256 Spt Tcd8.0t3600 Us"
 "\n--------------------"
 "\n B  : Run the benchmark. !!! INCOMPLETE !!!"
 "\n      Utilises the ALU, largest vector unit, and level 3 cache of all (virtual) cores in the system, and divides 1024MB of memory amongst them."
 "\n Ix : Set intruction usage options. Specifies which units to utilise. Options can be stacked; eg. I2av !!! CACHES NOT IMPLEMENTED !!!"
 "\n      Caches; 1==Level 1, 2==Level 2, 3==Level 3  |  Processing; A==ALU, F==FPU, S==SSE4.1, V==AVX2, X==AVX512"
 "\n Lx : Set interface language."
 "\n      Recognises ISO 639 language codes; eg. Leng"
 "\n Mx : Set amount of memory to utilise during test. !!! NOT YET IMPLEMENTED !!!"
 "\n      Memory will be evenly split amonsgt all utilised cores. Value is in MebiBytes; eg. M1024"
 "\n Ox : Output results to file."
 "\n      Requires valid filename; eg. Oresults.txt"
 "\n Sx : Set core synchronisation options. The first three options (P,R,S) can be stacked with the last (T); eg. Spt"
 "\n      P==Parallel, R==Round-robin, S==Staggered, T==Time synchronised"
 "\n Tx : Set timing options. One of the first three options (C,F,T) can be stacked with any of the remaining (D,T,[,]); eg. Tfd1.0t12.5[100]2400"
 "\n      C==Constant, F==Fixed-length pulses, S==Sweeping-length pulses"
 "\n      Global options: Dx==Set start-up delay, Tx==Set test duration                             |  Replace 'x' with a decimal value; eg. d10.0"
 "\n      Fixed-length pulse options (in milliseconds): [x==Active duration, ]x==Inactive duration  |  Replace 'x' with a whole number; eg. [250"
 "\n Ux : Set core usage options. One of the first two options (C,T) can be stacked with the last (S); eg. Uc!.!!...!s"
 "\n      C==Binary sequence map of physical cores to utilise, T==Binary sequence map of virtual cores to utilise"
 "\n         Format for core utilisation map is: ','/'.'/'_'==Core disabled, Any other character==Core enabled"
 "\n      S==Symmetric Multi-Threading; generate threads for every virtual core of each utilised physical core."
 "\n ~  : Write new \"cpu.values\" file."
 "\n      The integrity of the results will be checked for 65,536 iterations, only creating the file if all checks pass."
 "\n -x : Configuration presets."
 "\n      1==Constant stress; one thread per physical core"
 "\n      2==Constant stress on all virtual cores"
 "\n      3==Fixed-width round-robin pulsed stress; one thread per physical core"
 "\n      4==Synchronised fixed-width pulsed stress; one thread per physical core"
 "\n      5==Synchronised fixed-width pulsed stress on all virtual cores"
 "\n      6==Synchronised sweeping-width pulsed stress; one thread per physical core"
 "\n      7==Synchronised staggered fixed-width pulsed stress; one thread per physical core"
 "\n      8==Synchronised staggered fixed-width pulsed stress on all virtual cores\n\n";
