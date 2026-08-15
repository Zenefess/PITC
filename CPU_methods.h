/************************************************************
 * File: CPU_methods.h                  Created: 2025/02/17 *
 *                                    Last mod.: 2025/04/17 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

#include "CPU.h"
#include "CPU_job_cycles.h"

// Staging function for computation threads. __stdcall and ui32-returning to match _beginthreadex's
// start-address signature; every wait and every timestamp below is thread-local, because the global timer
// is shared by every thread and CLASS_TIMER::Update is not atomic (see CurrentTics in CPU.h)
ui32 __stdcall ComputationPulse(ptrc dataPtr) {
   cTHREAD_CFGptrc tcfg = (THREAD_CFG *)dataPtr;

   cHANDLE pulseTimer = CreatePulseTimer(); // Owned by this thread alone; see CreatePulseTimer
   vchptrc threadByte = &((chptr)threadBits)[tcfg->threadByte];
   csi64   recCount   = tcfg->rc_tc & 0x0FFFFFFFFFFFF;
   cui32   coreNum    = (cui32(tcfg->threadByte) << 3) + tcfg->threadBit;
   cui32   coreStag   = 1u << (coreNum & 0x07u);
   // Records one call of the dispatched job cycle transforms: every JobCycleMem* processes the four records
   // offset~offset+3, and every register-resident cycle exactly one. It is the unit the benchmark counts in,
   // so that a memory-backed score and a register-resident one are counts of the same thing (ISSUES.MD E12)
   csi64   recStep    = recCount ? 4 : 1;
   cui8    jobProc    = tcfg->procUnits & 0x01F;
   cbool   sweepSync  = tcfg->procSync & 0x040 ? true : false;
   si64    startTics  = tcfg->startTics;
   si64    cycleTics  = tcfg->cycleTics;
   si64    nextTic    = sweepSync ? 0 : tcfg->activeTics;
   si64    sleepTics  = 0; // Idle half of this thread's cycle, in tics; taken from the cycle once it is stretched
   si64    i, j       = 0;
   si64    oldTics    = 0;
   si64    curTics    = 0;
   si64    jitterSpan = 0; // Width of this thread's jitter window, in tics; 0 when no jitter is to be applied
   ui64    jitterRNG  = 0; // Jitter generator state, seeded per thread and per run by JitterSeed

   // Timing mode: Constant (bit 4), Fixed-width pulse (bit 5) or Sweeping pulse (bit 6), of which wmain
   // guarantees exactly one -- each of 'T's C/F/S sub-options replaces the other two, every preset sets one,
   // and a configuration naming none is normalised before the threads are configured. Each is selected here
   // by its own bit. Inferring "fixed pulse" from the absence of the other two, which is what this did, left
   // bit 5 with no reader at all: the banner's 'F-P' asserted a mode no line of the executor implemented,
   // and a mode added later would have been run as a fixed pulse of whatever width the defaults hold
   // (ISSUES.MD E10)
   switch(tcfg->procSync & 0x070) {
   case 0x020: // Fixed-width pulse
   case 0x040: // Sweeping pulse
      switch(tcfg->procSync & 0x07) {
      case 1: // Round-robin
         startTics += cycleTics * si64(coreNum);
         cycleTics *= tcfg->threadCount;
         break;
      case 4: { // Staggered
         // The ramp doubles across each group of 8 cores, so the last core of a group opens its first window
         // 127 cycles into the run and repeats every 128th. A run holding fewer cycles than that never
         // reaches those threads, and their rows of the results table then grade silicon that was never
         // exercised, so the ramp is folded down to the widest doubling the run can hold: cores share a
         // stagger rather than sit out the test (ISSUES.MD E3)
         si64 stagger = coreStag;

         while(stagger > 1 && cycleTics * (stagger - 1) + tcfg->activeTics > tcfg->endTics - tcfg->startTics)
            stagger >>= 1;

         // startTics carries the whole of the offset and nextTic takes it from startTics below. Adding it to
         // both did not delay the first window -- nextTic is where an active window *ends* -- it held that
         // window open one whole stagger period longer than the on-time asked for: 7.9s of unbroken compute
         // at preset -8's 900ms setting, at the head of a run whose point is that the load be pulsed. The
         // cycle is stretched by the stagger itself rather than by twice it, or core 0 and every eighth core
         // after it run at half the cycle frequency that was requested (E3)
         startTics += cycleTics * (stagger - 1);
         cycleTics *= stagger;
         break;
      }
      // 2==Parallel. Every other combination is rejected during parsing; treating them as parallel here
      // guarantees that nextTic can never be left holding a duration instead of an absolute tic count
      default:
         // Unless it is time-synchronised, a parallel thread's train is offset by a random fraction of a
         // cycle and its period jittered around cycleTics, so that the threads do not all step at one
         // instant -- which is the whole of what 'Spt' promises over 'Sp'. Only this shape is jittered: the
         // other two arrange the threads against each other, and a perturbation of any size erodes the
         // property they exist to deliver, round-robin's one-thread-at-a-time above all. The two offsets
         // this replaces eroded neither, but only because every thread of a run drew the same two values
         // from an unseeded rand() -- which is also why they desynchronised nothing (ISSUES.MD D6)
         if(!(tcfg->procSync & 0x08)) {
            // One span in tics governs the start offset and the per-cycle jitter alike, so the two can no
            // longer disagree about their units the way a millisecond offset[0] and a tic-counted offset[1]
            // did (ISSUES.MD D7). A quarter of the shorter phase is as wide as the window can be without a
            // pulse being consumed whole, at which point the shape stops being the one that was asked for
            jitterSpan = min(tcfg->activeTics, tcfg->cycleTics - tcfg->activeTics) >> 2;
            jitterRNG  = JitterSeed(coreNum);
            // Drawn once, because it is a phase offset: it shifts the whole train, distorting neither the
            // period nor the duty cycle, and every later edge inherits it through nextTic
            startTics += NextJitter(jitterRNG, jitterSpan);
         }
         break;
      }
      // The idle half of the cycle, after the shape above has stretched it. Deriving it from the cycle is
      // what keeps the two in step: it was grown in milliseconds beside a cycle grown in tics, and the pair
      // could disagree -- the staggered arm did, sleeping one stagger period per cycle where its cycle had
      // been stretched by two, which is an active window of the wrong width for the rest of the run
      sleepTics = cycleTics - tcfg->activeTics;
      nextTic  += startTics;
      break;
   case 0x010: // Constant computation: the compute branch never yields, so the active window is the whole run
   default:    // ...which is also the arm a mode this build does not implement lands on, that being the one
      nextTic = tcfg->endTics; // shape which cannot leave a thread idle for a test it was told to compute
   }

   // Wait for start time. A Sleep(1) poll overshoots by up to one scheduler tick per thread independently,
   // which is exactly the coincidence the parallel and time-synchronised shapes exist to produce
   PulseWaitUntil(pulseTimer, startTics);

   // Force minimum of one cycle for the sake of sweeping-pulse width
//   if(JobCycle[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte)) goto fail;
   if(!JobCycle[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte))
      // Main loop. The condition tested the timestamp the *previous* iteration had read, so the body was
      // regularly entered after the run had ended and a second, fresh test inside it was what kept the sweep
      // ramp and the sleep below inside the window they are defined on. Reading the counter in the condition
      // is that test, done once, and it is a thread-local reading rather than the shared timer another
      // thread may be part-way through updating (ISSUES.MD E5, D2). j opens at the cycle forced above
      for(i = 0, j = recStep; (curTics = CurrentTics()) < tcfg->endTics; i = (i >= recCount - 4 ? 0 : i + 4)) {
         if(!coreNum && curTics - oldTics > timer.siFrequency) { printf("."); oldTics = curTics; }
         if(curTics < nextTic) {
            if(JobCycle[recCount ? 1 : 0][jobProc](coreNum, i, threadByte)) break;
            // Counted here rather than in the loop's increment, and in records rather than in iterations:
            // the increment counts the idle iterations as well, and one iteration is four records in
            // memory-backed mode against one in register mode, so the benchmark's two figures were counts
            // of different things and neither was a count of the work done (ISSUES.MD E12)
            j += recStep;
         } else {
            si64 pulseTics = sleepTics;

            if(sweepSync) { // Ramp the idle time down across the run, raising the duty cycle as it progresses
               // The ratio is taken in fl64 because the si64 product of a cycle and a run overflows at a
               // day-long run of minute-long cycles, which 'Ts' and a round-robin thread count reach
               // between them. The clamp is what a ratio outside [0, 1] would otherwise carry into an
               // unsigned delay of roughly 49 days, and is kept whether or not one can still be produced
               cfl64 elapsed = fl64(curTics - tcfg->startTics) / fl64(tcfg->endTics - tcfg->startTics);

               pulseTics = max(si64(0), min(cycleTics, si64(fl64(cycleTics) * (1.0 - elapsed))));
            }
            // No idle phase may outlast the run. A round-robin thread's is one cycle per thread and a
            // staggered thread's up to 127 of them, so 64 threads at a 2s cycle issued a single 126s sleep
            // and the run overshot its duration by all of it -- time wmain spends waiting on a thread that
            // has nothing left to do (ISSUES.MD E6). The off-phase is otherwise the duration that was asked
            // for: offset[0] was added to it every cycle, which is a permanent change of duty cycle rather
            // than jitter, and it was taken out of the *following* on-phase because nextTic fixes the period
            // arithmetically -- up to 63ms of a 250ms pulse
            PulseWaitUntil(pulseTimer, min(curTics + pulseTics, tcfg->endTics));
            // Drawn afresh each cycle and centred on zero, so the thread's mean period remains cycleTics.
            // offset[1] added one fixed value to every cycle instead, which is not jitter but a period this
            // thread then kept for the whole of the run (ISSUES.MD D7)
            nextTic += cycleTics + NextJitter(jitterRNG, jitterSpan) - (jitterSpan >> 1);
         }
      }
//fail:
   if(tcfg->procSync & 0x080) resArray.iter[coreNum] = j;

   if(pulseTimer) CloseHandle(pulseTimer); // Closed before the completion bit, which releases wmain

   _InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));

   return 0; // Returning ends the thread: _beginthreadex's thunk calls _endthreadex with this value
}
