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
   cui32   cycleTime  = cui32(tcfg->cycleTics * 1000 / timer.siFrequency);
   cui32   coreStag   = 1u << (coreNum & 0x07u);
   cui8    jobProc    = tcfg->procUnits & 0x01F;
   cbool   sweepSync  = tcfg->procSync & 0x040 ? true : false;
   si64    startTics  = tcfg->startTics;
   si64    cycleTics  = tcfg->cycleTics;
   si64    nextTic    = sweepSync ? 0 : tcfg->activeTics;
   si64    i, j       = 0;
   si64    oldTics    = 0;
   si64    curTics    = 0;
   si64    jitterSpan = 0; // Width of this thread's jitter window, in tics; 0 when no jitter is to be applied
   ui64    jitterRNG  = 0; // Jitter generator state, seeded per thread and per run by JitterSeed
   ui32    sleepDelay = tcfg->inactiveTime;

   if(tcfg->procSync & 0x010) // Constant computation
      nextTic = tcfg->endTics;
   else { // Pulsed computation
      switch(tcfg->procSync & 0x07) {
      case 1: // Round-robin
         startTics  += cycleTics * si64(coreNum);
         sleepDelay += cycleTime * (tcfg->threadCount - 1);
         cycleTics  *= tcfg->threadCount;
         break;
      case 4: { // Staggered
         csi64 cycleStag = cycleTics * (coreStag - 1);
         startTics  += cycleStag;
         sleepDelay += cycleTime * (coreStag);
         cycleTics  *= si64(coreStag) << 1;
         nextTic    += cycleStag;
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
      nextTic += startTics;
   }

   // Wait for start time. A Sleep(1) poll overshoots by up to one scheduler tick per thread independently,
   // which is exactly the coincidence the parallel and time-synchronised shapes exist to produce
   PulseWaitUntil(pulseTimer, startTics);

   curTics = CurrentTics();

   // Force minimum of one cycle for the sake of sweeping-pulse width
//   if(JobCycle[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte)) goto fail;
   if(!JobCycle[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte))
      // Main loop
      for(i = j = 0; curTics < tcfg->endTics; i = (i >= recCount - 4 ? 0 : i + 4), ++j) {
         curTics = CurrentTics();
         // The loop condition tested the timestamp of the previous iteration, so the body is regularly
         // entered after the run has ended. Re-checking here keeps the sweep ramp inside its window
         if(curTics >= tcfg->endTics) break;
         if(!coreNum && curTics - oldTics > timer.siFrequency) { printf("."); oldTics = curTics; }
         if(curTics < nextTic) {
            if(JobCycle[recCount ? 1 : 0][jobProc](coreNum, i, threadByte)) break;
         } else {
            ui32 pulseDelay = sleepDelay;

            if(sweepSync) { // Ramp the idle time down across the run, raising the duty cycle as it progresses
               csi64 ramp = si64(cycleTime) - ((curTics - tcfg->startTics) * si64(cycleTime) / (tcfg->endTics - tcfg->startTics));

               // Clamped because a ratio outside [0, 1] wraps the unsigned delay to roughly 49 days
               pulseDelay = ui32(max(si64(0), min(si64(cycleTime), ramp)));
            }
            // The off-phase is the duration that was asked for: offset[0] was added to it every cycle, which
            // is a permanent change of duty cycle rather than jitter, and it was taken out of the *following*
            // on-phase because nextTic fixes the period arithmetically -- up to 63ms of a 250ms pulse
            PulseSleep(pulseTimer, pulseDelay);
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
