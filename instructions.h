/************************************************************
 * File: instructions.h                 Created: 2025/02/10 *
 *                                    Last mod.: 2025/02/16 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

al64 cwchar wstrInstructions_EN_UK[] =
L"\nPulsed Integrity Tests for CPUs v1.0   ---   Copyright (c) David William Bull\n"
 "\nReturn values"
 "\n-------------"
 "\n-65 : Failed to write results file                                 \t\t0  : Successul completion of stability test"
 "\n-64 : Unable to create results file"
 "\n-32 : Invalid filename for results file                            \t\t1  : Correct values successfully saved to file"
 "\n-19 : Failed to write all correct output values to file"
 "\n-18 : Failed to write all correct input values to file             \t\t2  : Instructions displayed to console"
 "\n-17 : Unable to create file for correct values"
 "\n-16 : Computational errors detected while generating correct values"
 "\n-3  : Insufficient output entries found in file"
 "\n-2  : Insufficient input entries found in file"
 "\n-1  : File containing correct values not found\n"
 "\nCommand-line options   ---   Example: pitc.exe I3x M256 Spt Tcd8.0t3600 Us"
 "\n--------------------"
 "\n B  : Run the benchmark. !!! CACHES NOT IMPLEMENTED !!!"
 "\n      Utilises the ALU and largest vector unit of all (virtual) cores in the system, level 3 cache, and 8MB memory per thread."
 "\n Ix : Set intruction usage options. Specifies which units to utilise. Options can be stacked; eg. I2av !!! CACHES NOT IMPLEMENTED !!!"
 "\n      Caches; 1==Level 1, 2==Level 2, 3==Level 3  |  Processing; A==ALU, F==FPU, S==SSE4.1, V==AVX2, X==AVX512"
 "\n Lx : Set interface language."
 "\n      Recognises ISO 639 language codes; eg. Leng"
 "\n Mx : Set amount of memory to utilise during test. Memory will be evenly split amongst all utilised cores. Values are in MebiBytes; eg. Mt128"
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
 "\n      S==Symmetric Multi-Threading; forces utilisation of every virtual core of each active physical core."
 "\n      E==Only utilise the first virtual core of each active physical core, O==Only utilise the last virtual core of each active physical core"
 "\n W  : Write new \"cpu.values\" file."
 "\n      File will only be created if the integrity of the results pass 65,536 iterations."
 "\n -x : Configuration presets."
 "\n      1==Constant stress; one thread per physical core"
 "\n      2==Constant stress on all virtual cores"
 "\n      3==Fixed-width round-robin pulsed stress; one thread per physical core"
 "\n      4==Synchronised fixed-width pulsed stress; one thread per physical core"
 "\n      5==Synchronised fixed-width pulsed stress on all virtual cores"
 "\n      6==Synchronised sweeping-width pulsed stress; one thread per physical core"
 "\n      7==Synchronised staggered fixed-width pulsed stress; one thread per physical core"
 "\n      8==Synchronised staggered fixed-width pulsed stress on all virtual cores\n\n";
