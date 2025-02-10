# Pulsed Integrity Tests for CPUs v1.0a


This software is primarily for testing the idle stability of CPUs.


Command-line options
--------------------
 B  : Run the benchmark. !!! Currently incomplete !!!
 
      Utilises the ALU, largest vector unit, and level 3 cache of all (virtual) cores in the system, and divides 1024MB of memory amongst them.      
 Ix : Set intruction usage options. Specifies which units to utilise. Options can be stacked; eg. I2av !!! Caches not yet implemented !!!
 
      Caches; 1==Level 1, 2==Level 2, 3==Level 3  |  Processing; A==ALU, F==FPU, S==SSE4.1, V==AVX2, X==AVX512      
 Lx : Set interface language.
 
      Recognises ISO 639 language codes; eg. Leng
 Mx : Set amount of memory to utilise during test. !!! Currently not yet implemented !!!
 
      Memory will be evenly split amonsgt all utilised cores. Value is in MebiBytes; eg. M1024      
 Ox : Output results to file.
 
      Requires valid filename; eg. Oresults.txt      
 Sx : Set core synchronisation options. The first three options (P,R,S) can be stacked with the last (T); eg. Spt
 
      P==Parallel, R==Round-robin, S==Staggered, T==Time synchronised      
 Tx : Set timing options. One of the first three options (C,F,T) can be stacked with any of the remaining (D,T,[,]); eg. Tfd1.0t12.5[100]2400
 
      C==Constant, F==Fixed-length pulses, S==Sweeping-length pulses      
      Global options: Dx==Set start-up delay, Tx==Set test duration                             |  Replace 'x' with a decimal value; eg. d10.0      
      Fixed-length pulse options (in milliseconds): [x==Active duration, ]x==Inactive duration  |  Replace 'x' with a whole number; eg. [250      
 Ux : Set core usage options. One of the first two options (C,T) can be stacked with the last (S); eg. Uc!.!!...!s
 
      C==Binary sequence map of physical cores to utilise, T==Binary sequence map of virtual cores to utilise      
         Format for core utilisation map is: ','/'.'/'_'==Core disabled, Any other character==Core enabled         
      S==Symmetric Multi-Threading; generate threads for every virtual core of each utilised physical core.      
 ~  : Write new "cpu.values" file.
 
      The integrity of the results will be checked for 65,536 iterations, only creating the file if all checks pass.      
 -x : Configuration presets.
 
      1==Constant stress; one thread per physical core      
      2==Constant stress on all virtual cores      
      3==Fixed-width round-robin pulsed stress; one thread per physical core      
      4==Synchronised fixed-width pulsed stress; one thread per physical core      
      5==Synchronised fixed-width pulsed stress on all virtual cores      
      6==Synchronised sweeping-width pulsed stress; one thread per physical core      
      7==Synchronised staggered fixed-width pulsed stress; one thread per physical core      
      8==Synchronised staggered fixed-width pulsed stress on all virtual cores


Example: "pitc.exe I3x M256 Spt Tcd8.0t3600 Us"
