# Pulsed Integrity Tests for CPUs v1.1


This software is primarily for testing the idle stability of CPUs, but also provides a range of options for more traditional stress testing. Run the executable without parameters to display instructions.


Building
--------
 Windows x64 only, built with MSVC — Visual Studio 2022, platform toolset v143, Windows 10 SDK. No solution file is checked in, so build the project file directly:

```
msbuild CPU.vcxproj /p:Configuration=Release /p:Platform=x64    ->  x64\PITC.exe
msbuild CPU.vcxproj /p:Configuration=Debug   /p:Platform=x64    ->  x64\Debug\PITC.exe
```


Generating "cpu.values"
-----------------------
 Every test grades what the CPU computes against `cpu.values`, a file of golden results the program generates for itself. It is not shipped, so generate it once, in the directory the executable is run from, before the first test of a build:

```
PITC.exe W
```

 Without that file a test stops immediately and returns `-1`. The file records the build and the job kernels that produced it, so one written by a different build of PITC is refused with `-21` rather than graded against — regenerate it after any rebuild that changes a job kernel. It is not machine-specific: a file written on one CPU verifies on another, whatever vector width either of them carries.

 `W` verifies every entry it generates, and cross-checks the job kernels against each other before it generates anything, so expect it to run for minutes rather than seconds.


Command-line options
--------------------
 Options are applied in the order given: where two of them set the same property, the last one wins. 'B' and the presets reset the processing units and the memory configuration, so give either of them before any 'I' or 'M' option. Pulsed mode (selected without '[' or ']') defaults to 100ms on-time and 900ms off-time. A sweeping-pulse run has no off-time, so ']' is ignored wherever it is given.

 B  : Run the benchmark. Options after 'B' override defaults; e.g. pitc.exe B Iaf mt1024
 
      Defaults utilise the ALU and largest vector unit of every virtual core, 8MB of memory per thread,
      parallel constant computation, a 2000ms start-up delay and a 60 second duration.
 Ix : Set instruction usage options. Specifies which units to utilise. Options can be stacked; e.g. I2av
 
      Caches: 1==Level 1, 2==Level 2, 3==Level 3
         The highest cache level given is used
      Processing: A==ALU, F==FPU, S==SSE2, V==AVX, X==AVX512
         'F', 'S', 'V' and 'X' are mutually exclusive
         At least one processing unit is required; a cache level names no unit of its own and is optional
         A cache level sizes the memory per thread to it: the blocks of every selected thread sharing one
         instance of that level fill it, and together overflow the level below. An 'M' option overrides the
         derived sizes, and a level this system does not report is refused with -27 rather than tested at
         some other size
 Lx : Set interface language.
 
      The code is matched case-insensitively against those this build carries: en-GB, en-US & fr-FR; e.g. Len-GB
         An unrecognised code is warned about and leaves the language unchanged
 Mx : Set amount of memory to utilise during test. Values are in MebiBytes; e.g. Mt128
 
      C==Per virtual core, N==Per first-class core,
      S==Per second-class virtual core, T==Total split amongst all virtual cores
         The two core classes are the CPU's non-SMT and SMT cores; on a hybrid CPU they are its efficiency
         and performance cores 'N' and 'S' each cover one class only: where the CPU has cores of both, give
         both, or the class left without memory is refused
         An 'M' overrides the sizes an 'I1', 'I2' or 'I3' would derive; a size outside that level's
         residency window is warned about
 Ox : Results file output options. A filename can be stacked with any of the remaining options; e.g. O[results.txt]16
 
      []=Filename, A=Non-UTF ASCII, 8=UTF-8, 16=UTF-16
 Sx : Set core synchronisation options. One of the first three options ('P','R','S') can be stacked with the last ('T'); e.g. Spt
 
      P==Parallel, R==Round-robin, S==Staggered, T==Time synchronised
         'P', 'R' and 'S' are mutually exclusive. 'T' aligns every thread's pulse edges. Without it a
         parallel run offsets each thread by a random fraction of a cycle
 Tx : Set timing options. One of the first three options ('C','F','S') can be stacked with any of the remaining ('D','T','[',']'); e.g. Tfd1.0t12.5[100]2400
 
      C==Constant, F==Fixed-length pulses, S==Sweeping-length pulses
         The last of C, F and S given is used
      Global options: Dx==Set start-up delay, Tx==Set test duration
         Replace 'x' with a decimal value; e.g. d10.0
      Fixed-length pulse options (in milliseconds): [x==Active duration, ]x==Inactive duration
         Replace 'x' with a whole number; e.g. [250
      Sweeping-length pulse option (in milliseconds): [x==Cycle duration
         A sweep has no off-time. Each cycle begins idle and the duty cycle rises in a straight line to 100%
         at the end of the test duration
 Ux : Set core usage options. One of the first two options (C,T) can be stacked with one of the remaining ('A','E','O'); e.g. Uc!.!!...!a
 
      C==Binary sequence map of physical cores to utilise,
      T==Binary sequence map of virtual cores to utilise
         Core disabled: '.' ',' '_' '-' '0'  |  Core enabled: '!' '*' '#' '+' '1' 'x' 'X'
         Any other character ends the map
         The map is the whole selection: a core it does not name is not utilised, and an empty selection is
         refused
         'C' numbers the physical cores in sequence, group after group. 'T' gives every processor group 64
         characters however many virtual cores it holds, so the characters past a group's last core are
         padding that must still be written to reach the next group; the thread bitmap prints each group to
         its own width, not to 64
      A==Symmetric Multi-Threading; forces utilisation of every virtual core of each active physical core
      E==Only utilise the first virtual core of each active physical core,
      O==Only utilise the last virtual core of each active physical core
         Both keep one virtual core per active physical core, whatever its SMT width; a core carrying only
         one is kept by either
 W  : Write new "cpu.values" file.
 
      The file is built as "cpu.values.tmp" and moved into place once it is complete, so an interrupted run
      leaves any previous "cpu.values" exactly as it was.
      File will only be created if the integrity of the results pass 65,536 iterations.
      All entries are verified, not one per thread, so expect the check to run for minutes rather than
      seconds. The job kernels are cross-checked first: each memory and combined kernel against the
      register-resident kernel of its own unit, and each vector kernel against the FPU kernel lane for lane,
      which is what makes the file readable on a CPU of a different vector width.
 -x : Configuration presets. By default will use the ALU & the largest vector unit, and 8MB memory per core.
 
      1==Constant stress; one thread per physical core. 10 minute duration
      2==Constant stress on all virtual cores. 30 minute duration
      3==Fixed-width round-robin pulsed stress; one thread per physical core. 10 minute duration
      4==Synchronised fixed-width pulsed stress; one thread per physical core. 10 minute duration
      5==Synchronised fixed-width pulsed stress on all virtual cores. 30 minute duration
      6==Sweeping-width pulsed stress; one thread per physical core. 30 minute duration
      7==Synchronised sweeping-width pulsed stress on all virtual cores. 30 minute duration
      8==Staggered fixed-width pulsed stress; one thread per physical core. 1 hour duration
      9==Synchronised staggered fixed-width pulsed stress on all virtual cores. 4 hour duration
      0==Synchronised fixed-width pulsed stress on all virtual cores, using ALU & SSE code-paths with 2MB
      memory per core. 1 hour duration


Example: "pitc.exe I3x Spt Tcd8.0t3600 Ua"


Return values
-------------
 The process exit code says what the run did. Zero and above are outcomes, below zero are refusals.

| Code | Meaning |
|-----:|---------|
| `2` | Instructions displayed to console |
| `1` | Correct values successfully saved to file |
| `0` | Successful completion of stability test |
| `-1` | File containing correct values not found |
| `-2` | Insufficient input entries found in file |
| `-3` | Insufficient output entries found in file |
| `-4` | Computational errors detected while generating correct values |
| `-5` | Unable to create or replace the file for correct values |
| `-6` | Failed to write all correct input values to file |
| `-7` | Failed to write all correct output values to file |
| `-8` | Invalid filename for results file |
| `-9` | Unable to create results file |
| `-10` | Failed to write results file |
| `-11` | Requested processing unit not supported by the CPU |
| `-12` | More than one thread synchronisation option requested |
| `-13` | Test duration of zero or less requested |
| `-14` | Pulse on-time of zero requested |
| `-15` | No processing unit requested |
| `-16` | More than one non-ALU processing unit requested |
| `-17` | Unable to allocate the requested amount of memory |
| `-18` | Insufficient memory per thread for the requested processing unit(s) |
| `-19` | Unable to create a computation thread |
| `-20` | Unable to write the "cpu.values" header |
| `-21` | Contents of "cpu.values" are not valid for this build |
| `-22` | A job kernel disagrees with the kernel it is required to reproduce |
| `-23` | Unable to enumerate the processor topology of the system |
| `-24` | Missing, malformed or out-of-range value for a command-line option |
| `-25` | Unrecognised command-line option |
| `-26` | The selected core map contains no cores to test |
| `-27` | Requested cache level not reported by the system |


Screenshot of the benchmark result of an AMD Ryzen 9 5950X:
![PITC benchmark v1 0 2](https://github.com/user-attachments/assets/543ed696-ed0f-4ac8-8328-887109a0c2dc)
