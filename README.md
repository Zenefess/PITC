# Pulsed Integrity Tests for CPUs v1.0.2


This software is primarily for testing the idle stability of CPUs, but also provides a range of options for more traditional stress testing. Run the executable without parameters to display instructions.


Command-line options
--------------------
 B  : Run the benchmark. Options after 'B' override defaults; eg. pitc.exe B Iaf mt1024 !!! Cache use not yet implemented !!!
 
      Utilises the ALU and largest vector unit of all (virtual) cores in the system, level 3 cache, and 8MB memory per thread for 60 seconds.
 Ix : Set intruction usage options. Specifies which units to utilise. Options can be stacked; eg. I2av !!! Cache use not yet implemented !!!
 
      Caches: 1==Level 1, 2==Level 2, 3==Level 3  |  Processing: A==ALU, F==FPU, S==SSE4.1, V==AVX2, X==AVX512
 Lx : Set interface language.
 
      Recognises ISO 639-1 language codes; eg. Len-GB
 Mx : Set amount of memory to utilise during test. Values are in MebiBytes; eg. Mt128
 
      C==Per virtual core, N==Per non-SMT core, S==Per SMT virtual core, T=Total split amongst all virtual cores
 Ox : Results file output options. A filename can be stacked with any of the remaining options; eg. O[results.txt]16
 
      []=Filename, A=Non-UTF ASCII, 8=UTF-8, 16=UTF-16
 Sx : Set core synchronisation options. The first three options (P,R,S) can be stacked with the last (T); eg. Spt
 
      P==Parallel, R==Round-robin, S==Staggered, T==Time synchronised
 Tx : Set timing options. One of the first three options (C,F,T) can be stacked with any of the remaining (D,T,[,]); eg. Tfd1.0t12.5[100]2400
 
      C==Constant, F==Fixed-length pulses, S==Sweeping-length pulses
      Global options: Dx==Set start-up delay, Tx==Set test duration                             |  Replace 'x' with a decimal value; eg. d10.0
      Fixed-length pulse options (in milliseconds): [x==Active duration, ]x==Inactive duration  |  Replace 'x' with a whole number; eg. [250
      Sweeping-length pulse option (in milliseconds): [x==Cycle duration
 Ux : Set core usage options. One of the first two options (C,T) can be stacked with the one of the remaining (S, O); eg. Uc!.!!...!s
 
      C==Binary sequence map of physical cores to utilise, T==Binary sequence map of virtual cores to utilise
         Format for core utilisation map is: ','/'.'/'_'==Core disabled, Any other character==Core enabled
      A==Symmetric Multi-Threading; forces utilisation of every virtual core of each active physical core
      E==Only utilise the first virtual core of each active physical core, O==Only utilise the last virtual core of each active physical core
 W  : Write new "cpu.values" file.
 
      File will only be created if the integrity of the results pass 65,536 iterations.
 -x : Configuration presets.
 
      1==Constant stress; one thread per physical core. 10 minute duration
      2==Constant stress on all virtual cores. 30 minute duration
      3==Fixed-width round-robin pulsed stress; one thread per physical core. 10 minute duration
      4==Synchronised fixed-width pulsed stress; one thread per physical core. 10 minute duration
      5==Synchronised fixed-width pulsed stress on all virtual cores. 30 minute duration
      6==Sweeping-width pulsed stress; one thread per physical core. 30 minute duration
      7==Synchronised sweeping-width pulsed stress on all virtual cores. 30 minute duration
      8==Synchronised staggered fixed-width pulsed stress; one thread per physical core. 1 hour duration
      9==Staggered fixed-width pulsed stress on all virtual cores. 4 hour duration
      0==Synchronised fixed-width pulsed stress on all virtual cores, using ALU & SSE code-paths with 2MB memory per core. 1 hour duration


Example: "pitc.exe I3x Mc8 Spt Tcd8.0t3600 Ua"
![PITC benchmark v1 0 2](https://github.com/user-attachments/assets/543ed696-ed0f-4ac8-8328-887109a0c2dc)
