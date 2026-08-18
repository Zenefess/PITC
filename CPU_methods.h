/*
 * File: CPU_methods.h
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-02-17
 * Last Modified: 2026-08-16
 * Description: The computation thread body: pulse-shape set-up, the dispatch loop, and the record count the benchmark score is taken from.
 * To Do: 1) Range-check the dispatch index once per thread, so a hand-built 'I' string cannot reach a wild indirect call
 *        2) Delete the superseded 'goto fail' spelling left commented at the forced first job cycle and at the loop's exit
 * Dependencies: CPU.h, CPU_job_cycles.h
 * ISA: Scalar
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
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
   // by its own bit.
   switch(tcfg->procSync & 0x070) {
   case 0x020: // Fixed-width pulse
   case 0x040: // Sweeping pulse
      switch(tcfg->procSync & 0x07) {
      case 1: { // Round-robin
         // Thread n's window opens n cycles into the run and the cycle is stretched by the thread count, so
         // exactly one thread computes at a time.
         si64 slots = tcfg->threadCount;

         while(slots > 1 && cycleTics * (slots - 1) + tcfg->activeTics > tcfg->endTics - tcfg->startTics)
            --slots;

         startTics += cycleTics * si64(coreNum % ui32(slots));
         cycleTics *= slots;
         break;
      }
      case 4: { // Staggered
         si64 stagger = coreStag;

         while(stagger > 1 && cycleTics * (stagger - 1) + tcfg->activeTics > tcfg->endTics - tcfg->startTics)
            stagger >>= 1;

         startTics += cycleTics * (stagger - 1);
         cycleTics *= stagger;
         break;
      }
      // 2==Parallel. Every other combination is rejected during parsing; treating them as parallel here
      // guarantees that nextTic can never be left holding a duration instead of an absolute tic count
      default:
         // Unless it is time-synchronised, a parallel thread's train is offset by a random fraction of a
         // cycle and its period jittered around cycleTics, so that the threads do not all step at one
         // instant -- which is the whole of what 'Spt' promises over 'Sp'.
         if(!(tcfg->procSync & 0x08)) {
            jitterSpan = min(tcfg->activeTics, tcfg->cycleTics - tcfg->activeTics) >> 2;
            jitterRNG  = JitterSeed(coreNum);
            // Drawn once, because it is a phase offset: it shifts the whole train, distorting neither the
            // period nor the duty cycle, and every later edge inherits it through nextTic
            startTics += NextJitter(jitterRNG, jitterSpan);
         }
         break;
      }
      // The idle half of the cycle, after the shape above has stretched it. Deriving it from the cycle is
      // what keeps the two in step
      sleepTics = cycleTics - tcfg->activeTics;
      nextTic  += startTics;
      break;
   case 0x010:                 // Constant computation: the compute branch never yields, so the active window is the whole run
   default:                    // which is also the arm a mode this build does not implement lands on, that being the one
      nextTic = tcfg->endTics; // shape which cannot leave a thread idle for a test it was told to compute
   }

   // Wait for start time.
   PulseWaitUntil(pulseTimer, min(startTics, tcfg->endTics));

   // Force minimum of one cycle for the sake of sweeping-pulse width
//   if(JOB_CYCLE[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte)) goto fail;
   if(!JOB_CYCLE[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte))
      // Main loop.
      for(i = 0, j = recStep; (curTics = CurrentTics()) < tcfg->endTics; i = (i >= recCount - 4 ? 0 : i + 4)) {
         // One dot per second from the first thread only, wide like every other write this program makes to
         // stdout
         if(!coreNum && curTics - oldTics > timer.siFrequency) { wprintf(L"."); oldTics = curTics; }
         if(curTics < nextTic) {
            if(JOB_CYCLE[recCount ? 1 : 0][jobProc](coreNum, i, threadByte)) break;
            j += recStep;
         } else {
            si64 pulseTics = sleepTics;

            if(sweepSync) { // Ramp the idle time down across the run, raising the duty cycle as it progresses
               cfl64 elapsed = fl64(curTics - tcfg->startTics) / fl64(tcfg->endTics - tcfg->startTics);

               pulseTics = max(si64(0), min(cycleTics, si64(fl64(cycleTics) * (1.0 - elapsed))));
            }
            // No idle phase may outlast the run.
            PulseWaitUntil(pulseTimer, min(curTics + pulseTics, tcfg->endTics));
            // Drawn afresh each cycle and centred on zero, so the thread's mean period remains cycleTics.
            nextTic += cycleTics + NextJitter(jitterRNG, jitterSpan) - (jitterSpan >> 1);
         }
      }
//fail:
   if(tcfg->procSync & 0x080) resArray.iter[coreNum] = j;

   if(pulseTimer) CloseHandle(pulseTimer); // Closed before the completion bit, which releases wmain

   _InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));

   return 0; // Returning ends the thread: _beginthreadex's thunk calls _endthreadex with this value
}
