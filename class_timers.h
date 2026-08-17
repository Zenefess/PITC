/************************************************************
 * File: class_timers.h                 Created: 2022/10/20 *
 *                                Last modified: 2026/08/17 *
 *                                                          *
 * Desc: Single and multi timer classes.                    *
 *                                                          *
 * To do: Add "AdvanceBy" functions                         *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

#include "typedefs.h"

// Real allocator with bounds-checked, lock-serialised tracking. Replaces the former standalone salloc fallback,
// which duplicated an inline definition (ODR hazard), referenced sysData with no include, and appended to the
// tracking table with no maxAllocations check.
#include "memory management.h"

al64 struct TIMER_VARIABLES {
   fl64 dGrandTotal;
   si64 siFrequency;
   fl64 dTotal, dElapsed, dScale, dScaleConst, dScaleAccel, _dScaleAccelTemp;
   si64 siOriginTics, siStartTics, siPrevTics, siCurrentTics, siTotalTics, siElapsedTics, siPauseTics;
   si32 siTotalUpdates, siUpdatesSinceLastReset;
};

al64 struct CLASS_TIMER {
   fl64 dGrandTotal;
   si64 siFrequency;
   fl64 dTotal, dElapsed, dScale, dScaleConst, dScaleAccel, _dScaleAccelTemp;
   si64 siOriginTics, siStartTics, siPrevTics, siCurrentTics, siTotalTics, siElapsedTics, siPauseTics;
   si32 siTotalUpdates, siUpdatesSinceLastReset;

   CLASS_TIMER(void) { Reinitialise(); }
   CLASS_TIMER(cptrptr globalPointer) { Reinitialise(); if(globalPointer) *globalPointer = this; }

   void Reinitialise(void) {
      QueryPerformanceCounter((LARGE_INTEGER *)&siOriginTics);
      QueryPerformanceFrequency((LARGE_INTEGER *)&siFrequency);
      // dGrandTotal, dScaleAccel, _dScaleAccelTemp, siPrevTics, siElapsedTics and siPauseTics were left
      // unassigned, so any CLASS_TIMER that is not a zero-initialised global -- an automatic or a heap one --
      // read indeterminate storage in its first Update, 'dScale += dScaleAccel' over garbage above all, and
      // carried the result into every reading it went on to report (ISSUES.MD I5)
      dGrandTotal    = dTotal = dElapsed = 0.0;
      dScale         = dScaleConst = 1.0;
      dScaleAccel    = _dScaleAccelTemp = 0.0;
      siStartTics    = siCurrentTics = siPrevTics = siOriginTics;
      // Zero, not the counter reading. siTotalTics is a duration, and its floating-point twin dTotal is
      // seeded 0.0 two lines above; seeded with the raw origin instead it stayed permanently equal to
      // siCurrentTics, so GetTotalTimeUnscaled reported time since the QPC epoch and disagreed with
      // GetTotalTimeScaled by the whole of the machine's uptime (ISSUES.MD I6)
      siTotalTics    = siElapsedTics = siPauseTics = 0;
      siTotalUpdates = siUpdatesSinceLastReset = 0;
   };

   inline void Update(void) {
      siPrevTics = siCurrentTics;
      QueryPerformanceCounter((LARGE_INTEGER *)&siCurrentTics);
      siTotalTics += (siElapsedTics = siCurrentTics - siPrevTics);
      // The acceleration is applied and then clamped. Spelt as '(dScale += dScaleAccel) = clamp(dScale...)'
      // it was neither: C++17 sequences an assignment's right operand before its left, so the clamp was
      // computed from the *unaccelerated* dScale and then stored over the result of the compound assignment.
      // The acceleration was added and unconditionally discarded on every update, which is the whole of what
      // Pause, UnPause and Reset's time scaling do (ISSUES.MD I5)
      dScale += dScaleAccel;
      dScale  = (dScale < 0.0 ? max(dScale, dScaleConst) : min(dScale, dScaleConst));
      // Each total accumulates the increment. 'dGrandTotal += (dTotal += dElapsed)' added the *running total*
      // to the grand total instead, so it grew quadratically with the update count -- 1+2+...+n seconds after
      // n one-second updates -- where its purpose is to measure the time the timer has run across the resets
      // of dTotal that Reset performs (ISSUES.MD I3)
      dElapsed     = (fl64(siElapsedTics) * dScale) / fl64(siFrequency);
      dTotal      += dElapsed;
      dGrandTotal += dElapsed;
      siUpdatesSinceLastReset++;
      siTotalUpdates++;
   };

   inline void Pause(cfl64 deceleration) {
      siPauseTics = siElapsedTics;
      dScaleAccel = deceleration;
   };

   inline void UnPause(cfl64 acceleration) {
      siPauseTics = 0;
      dScaleAccel = acceleration;
   };

   inline void Freeze(void) {
      _dScaleAccelTemp = dScaleAccel;
      siPauseTics = siCurrentTics;
      dScale      = 0.0;
      dScaleAccel = 0.0;
   };

   inline void Unfreeze(void) {
      dScaleAccel = _dScaleAccelTemp;
      siPauseTics = 0;
      dScale = dScaleConst;
   };

   inline void Reset(cfl64 timeScale) {
      dScale        = dScaleConst = dScaleAccel = timeScale;
      siStartTics   = siCurrentTics;
      dTotal        = dElapsed = 0.0;
      siElapsedTics = siUpdatesSinceLastReset = 0;
   };

   inline void SetScale(cfl64 timeScale) { dScaleConst = timeScale; };

   inline cfl64 GetElapsedTimeScaled(void) const { return dElapsed; };

   inline cfl64 GetTotalTimeScaled(void) const { return dTotal; };

   inline cfl64 GetGrandTotalTimeScaled(void) const { return dGrandTotal; };

   inline cfl64 GetElapsedTimeUnscaled(void) const { return fl64(siElapsedTics) / fl64(siFrequency); };

   inline cfl64 GetTotalTimeUnscaled(void) const { return fl64(siTotalTics) / fl64(siFrequency); };

   inline csi32 GetUpdates(void) const { return siUpdatesSinceLastReset; }

   inline csi32 GetTotalUpdates(void) const { return siTotalUpdates; }

   inline cfl64 UpdatesPerSecondScaled(void) const { return fl64(siUpdatesSinceLastReset) / dElapsed; };

   inline cfl64 AverageUpdatesPerSecondScaled(void) const { return fl64(siTotalUpdates) / dTotal; };
};

// Declared ahead of the definition so the deleted copy operations below can name the const alias rather than
// spell 'const CLASS_TIMERS &' at the parameter; GCS r2/t2 put the qualifier in the typedef, and t3 forbids
// mixing the two styles in one translation unit. CLASS_TIMER above needs no such alias -- it owns nothing,
// and so needs neither a destructor nor copy control
struct CLASS_TIMERS;
typedef const CLASS_TIMERS cCLASS_TIMERS;

al16 struct CLASS_TIMERS {
#ifndef MAX_TIMERS
   #define MAX_TIMERS 16
#endif

   TIMER_VARIABLES *const timer = (TIMER_VARIABLES *)salloc(RoundUpToNearest64(sizeof(TIMER_VARIABLES[MAX_TIMERS])), 64, (cui64)0);

   ui32 uiTimerCount = 0;

   CLASS_TIMERS(void) { ReinitialiseAll(); }
   CLASS_TIMERS(cptrptr globalPointer) { ReinitialiseAll(); if(globalPointer) *globalPointer = this; }

   // The in-class initialiser above is an allocation, and it had no matching free: the 2KB table (16 slots of
   // 128 bytes) was leaked however the object's scope ended, contrary to GCS p2 and to the destructor-owned-
   // pointer pattern GLOBAL_CFG, RESULTS_ARRAYS and REPORT_BUFFER follow in CPU.h (ISSUES.MD B2). mdealloc
   // ignores a null pointer, so an object whose salloc failed destructs exactly as safely as one whose salloc
   // succeeded -- the property those three rest on as well
   ~CLASS_TIMERS(void) { mfree1(timer); }

   // The free's companions, not a change of interface. A destructor that releases an owned pointer makes the
   // *implicit* copy constructor a double free -- it copies the address, and both objects then free it -- so
   // adding the one without suppressing the other would trade a leak for corruption, which for a header this
   // repository vendors rather than instantiates would be the sharper defect of the two. Copy assignment is
   // already implicitly deleted, 'timer' being 'TIMER_VARIABLES *const', and is stated anyway so that
   // relaxing that constness cannot reintroduce it silently; a user-declared destructor suppresses the move
   // operations, so all four are accounted for. Nothing is lost by refusing a copy, because no copy was ever
   // usable: before the destructor existed it duplicated a pointer to one table and leaked both objects' claim
   // on it, and PITC itself never instantiates CLASS_TIMERS at all
   CLASS_TIMERS(cCLASS_TIMERS &)            = delete;
   CLASS_TIMERS &operator=(cCLASS_TIMERS &) = delete;

   // The same six members CLASS_TIMER::Reinitialise left unassigned (I5), and the same seeding of siTotalTics
   // with the raw counter reading that made GetTotalTimeUnscaled report the machine's uptime (I6)
   void Reinitialise(cui8 index) {
      TIMER_VARIABLES &slot = timer[index];

      QueryPerformanceCounter((LARGE_INTEGER *)&slot.siOriginTics);

      if(!slot.siFrequency) QueryPerformanceFrequency((LARGE_INTEGER *)&slot.siFrequency);

      slot.dGrandTotal    = slot.dTotal = slot.dElapsed = 0.0;
      slot.dScale         = slot.dScaleConst = 1.0;
      slot.dScaleAccel    = slot._dScaleAccelTemp = 0.0;
      slot.siStartTics    = slot.siCurrentTics = slot.siPrevTics = slot.siOriginTics;
      slot.siTotalTics    = slot.siElapsedTics = slot.siPauseTics = 0;
      slot.siTotalUpdates = slot.siUpdatesSinceLastReset = 0;
   };

   // This held a second copy of the body above, which is how the one function came to seed a slot differently
   // from the other; it defers to the single definition instead
   void ReinitialiseAll(void) {
      for(ui32 index = 0; index < uiTimerCount; ++index) Reinitialise(cui8(index));
   };

   cui32 Create(void) {
      if(uiTimerCount >= MAX_TIMERS) return 0x0c0000001;
      Reinitialise(uiTimerCount);
      return uiTimerCount++;
   }

   inline void Update(cui8 index) {
      TIMER_VARIABLES &slot = timer[index];

      slot.siPrevTics = slot.siCurrentTics;
      QueryPerformanceCounter((LARGE_INTEGER *)&slot.siCurrentTics);
      slot.siTotalTics += (slot.siElapsedTics = slot.siCurrentTics - slot.siPrevTics);
      // Applied, then clamped -- the C++17 sequencing of the single-expression form discarded the
      // acceleration it had just added (I5, and the same spelling as CLASS_TIMER::Update above)
      slot.dScale += slot.dScaleAccel;
      slot.dScale  = (slot.dScale < 0.0 ? max(slot.dScale, slot.dScaleConst) : min(slot.dScale, slot.dScaleConst));
      // The elapsed time is computed once. The identical computation stood here twice, back to back, the
      // first result folded into dTotal and then the second folded into it again, so every scaled total this
      // class reported advanced at twice real time (ISSUES.MD I4). Each total takes the increment rather than
      // the running sum, for the reason CLASS_TIMER::Update gives (I3)
      slot.dElapsed     = (fl64(slot.siElapsedTics) * slot.dScale) / fl64(slot.siFrequency);
      slot.dTotal      += slot.dElapsed;
      slot.dGrandTotal += slot.dElapsed;
      slot.siUpdatesSinceLastReset++;
      slot.siTotalUpdates++;
   };

   inline void UpdateAll(void) {
      si64 curTics;

      // Into a local. The reading was taken into timer[0].siCurrentTics, which is a slot's own state: by the
      // time slot 0 came to record its previous reading, the previous reading had already been overwritten
      QueryPerformanceCounter((LARGE_INTEGER *)&curTics);

      for(ui32 i = 0; i < uiTimerCount; i++) {
         TIMER_VARIABLES &slot = timer[i];

         // siPrevTics was never written anywhere in this function, so siElapsedTics measured from whatever
         // the member last held -- zero for a slot this class had initialised, which is the QPC epoch and so
         // roughly the machine's uptime -- on every call rather than since the previous update. The reading
         // was also assigned twice, from two spellings of the one value (ISSUES.MD I4)
         slot.siPrevTics    = slot.siCurrentTics;
         slot.siCurrentTics = curTics;
         slot.siTotalTics  += (slot.siElapsedTics = slot.siCurrentTics - slot.siPrevTics);
         slot.dScale       += slot.dScaleAccel;
         slot.dScale        = (slot.dScale < 0.0 ? max(slot.dScale, slot.dScaleConst) : min(slot.dScale, slot.dScaleConst));
         slot.dElapsed      = (fl64(slot.siElapsedTics) * slot.dScale) / fl64(slot.siFrequency);
         slot.dTotal       += slot.dElapsed;
         slot.dGrandTotal  += slot.dElapsed;
         slot.siUpdatesSinceLastReset++;
         slot.siTotalUpdates++;
      }
   };

   inline void Pause(cfl64 deceleration, cui8 index) {
      TIMER_VARIABLES &slot = timer[index];

      slot.siPauseTics = slot.siElapsedTics;
      slot.dScaleAccel = deceleration;
   };

   inline void UnPause(cfl64 acceleration, cui8 index) {
      TIMER_VARIABLES &slot = timer[index];

      slot.siPauseTics = 0;
      slot.dScaleAccel = acceleration;
   };

   inline void Freeze(cui8 index) {
      TIMER_VARIABLES &slot = timer[index];

      slot._dScaleAccelTemp = slot.dScaleAccel;
      slot.siPauseTics      = slot.siCurrentTics;
      slot.dScale           = 0.0;
      slot.dScaleAccel      = 0.0;
   };

   inline void Unfreeze(cui8 index) {
      TIMER_VARIABLES &slot = timer[index];

      slot.dScaleAccel = slot._dScaleAccelTemp;
      slot.siPauseTics = 0;
      slot.dScale = slot.dScaleConst;
   };

   inline void Reset(cfl64 timeScale, cui8 index) {
      TIMER_VARIABLES &slot = timer[index];

      slot.dScale        = slot.dScaleConst = slot.dScaleAccel = timeScale;
      slot.siStartTics   = slot.siCurrentTics;
      slot.dElapsed      = 0.0;
      slot.siElapsedTics = slot.siUpdatesSinceLastReset = 0;
   };

   inline void SetScale(cfl64 timeScale, cui8 index) { timer[index].dScaleConst = timeScale; };

   inline cfl64 GetElapsedTimeScaled(cui8 index) const { return timer[index].dElapsed; };

   inline cfl64 GetTotalTimeScaled(cui8 index) const { return timer[index].dTotal; };

   inline cfl64 GetGrandTotalTimeScaled(cui8 index) const { return timer[index].dGrandTotal; };

   inline cfl64 GetElapsedTimeUnscaled(cui8 index) const { return fl64(timer[index].siElapsedTics) / fl64(timer[index].siFrequency); };

   inline cfl64 GetTotalTimeUnscaled(cui8 index) const { return fl64(timer[index].siTotalTics) / fl64(timer[index].siFrequency); };

   inline csi32 GetUpdates(cui8 index) const { return timer[index].siUpdatesSinceLastReset; }

   inline csi32 GetTotalUpdates(cui8 index) const { return timer[index].siTotalUpdates; }

   inline cfl64 UpdatesPerSecondScaled(cui8 index) const { return fl64(timer[index].siUpdatesSinceLastReset) / timer[index].dElapsed; };

   inline cfl64 AverageUpdatesPerSecondScaled(cui8 index) const { return fl64(timer[index].siTotalUpdates) / timer[index].dTotal; };
};
