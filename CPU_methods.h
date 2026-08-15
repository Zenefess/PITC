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

// Staging function for computation threads
void ComputationPulse(ptrc dataPtr) {
   cTHREAD_CFGptrc tcfg = (THREAD_CFG *)dataPtr;

   cHANDLE pulseTimer = CreatePulseTimer(); // Owned by this thread alone; see CreatePulseTimer
   vchptrc threadByte = &((chptr)threadBits)[tcfg->threadByte];
   csi64   recCount   = tcfg->rc_tc & 0x0FFFFFFFFFFFF;
   cui32   coreNum    = (cui32(tcfg->threadByte) << 3) + tcfg->threadBit;
   cDWORD  offset[2]  = { tcfg->procSync & 0x08 ? 0 : cDWORD(rand() & 0x03F), tcfg->procSync & 0x08 ? 0 : cDWORD(rand() & 0x0FFFF) };
   cui32   cycleTime  = cui32(tcfg->cycleTics * 1000 / timer.siFrequency);
   cui32   coreStag   = 1u << (coreNum & 0x07u);
   cui8    jobProc    = tcfg->procUnits & 0x01F;
   cbool   sweepSync  = tcfg->procSync & 0x040 ? true : false;
   si64    startTics  = tcfg->startTics;
   si64    cycleTics  = tcfg->cycleTics;
   si64    nextTic    = sweepSync ? 0 : tcfg->activeTics;
   si64    i, j       = 0;
   si64    oldTics    = 0;
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
         break;
      }
      nextTic += startTics;
   }

   // Wait for start time. A Sleep(1) poll overshoots by up to one scheduler tick per thread independently,
   // which is exactly the coincidence the parallel and time-synchronised shapes exist to produce
   PulseWaitUntil(pulseTimer, startTics);

   // Force minimum of one cycle for the sake of sweeping-pulse width
//   if(JobCycle[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte)) goto fail;
   if(!JobCycle[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte))
      // Main loop
      for(i = j = 0; timer.siCurrentTics < tcfg->endTics; i = (i >= recCount - 4 ? 0 : i + 4), ++j) {
         timer.Update();
         // The loop condition tested the timestamp of the previous iteration, so the body is regularly
         // entered after the run has ended. Re-checking here keeps the sweep ramp inside its window
         if(timer.siCurrentTics >= tcfg->endTics) break;
         if(!coreNum && timer.siCurrentTics - oldTics > timer.siFrequency) { printf("."); oldTics = timer.siCurrentTics; }
         if(timer.siCurrentTics < nextTic) {
            if(JobCycle[recCount ? 1 : 0][jobProc](coreNum, i, threadByte)) break;
         } else {
            ui32 pulseDelay = sleepDelay;

            if(sweepSync) { // Ramp the idle time down across the run, raising the duty cycle as it progresses
               csi64 ramp = si64(cycleTime) - ((timer.siCurrentTics - tcfg->startTics) * si64(cycleTime) / (tcfg->endTics - tcfg->startTics));

               // Clamped because a ratio outside [0, 1] wraps the unsigned delay to roughly 49 days
               pulseDelay = ui32(max(si64(0), min(si64(cycleTime), ramp)));
            }
            PulseSleep(pulseTimer, pulseDelay + offset[0]);
            nextTic += cycleTics + offset[1];
         }
      }
//fail:
   if(tcfg->procSync & 0x080) resArray.iter[coreNum] = j;

   if(pulseTimer) CloseHandle(pulseTimer); // Closed before the completion bit, which releases wmain

   _InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));

   _endthread();
}
