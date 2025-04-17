/************************************************************
 * File: CPU.h                          Created: 2025/02/17 *
 *                                    Last mod.: 2025/04/17 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

#include "CPU.h"
#include "CPU job cycles.h"

// Staging function for computation threads
void ComputationPulse(ptrc dataPtr) {
   cTHREAD_CFGptrc tcfg = (THREAD_CFG *)dataPtr;

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
      case 2: // Parallel
         nextTic += startTics;
         break;
      case 4: // Staggered
         csi64 cycleStag = cycleTics * (coreStag - 1);
         startTics  += cycleStag;
         sleepDelay += cycleTime * (coreStag);
         cycleTics  *= si64(coreStag) << 1;
         nextTic    += startTics + cycleStag;
      }
   }

   // Wait for start time
   do {
      Sleep(1);
      timer.Update();
   } while(startTics > timer.siCurrentTics);

   // Force minimum of one cycle for the sake of sweeping-pulse width
//   if(JobCycle[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte)) goto fail;
   if(!JobCycle[recCount ? 1 : 0][jobProc](coreNum, 0, threadByte))
      // Main loop
      for(i = j = 0; timer.siCurrentTics < tcfg->endTics; i = (i >= recCount - 4 ? 0 : i + 4), ++j) {
         timer.Update();
         if(!coreNum && timer.siCurrentTics - oldTics > timer.siFrequency) { printf("."); oldTics = timer.siCurrentTics; }
         if(timer.siCurrentTics < nextTic) {
            if(JobCycle[recCount ? 1 : 0][jobProc](coreNum, i, threadByte)) break;
         } else {
            Sleep(sweepSync ? DWORD(cycleTime - ((timer.siCurrentTics - tcfg->startTics) * cycleTime / (tcfg->endTics - tcfg->startTics))) + offset[0] : sleepDelay + offset[0]);
            nextTic += cycleTics + offset[1];
         }
      }
//fail:
   if(tcfg->procSync & 0x080) resArray.iter[coreNum] = j;

   _InterlockedAnd8(threadByte, ~cui8(1u << tcfg->threadBit));

   _endthread();
}
