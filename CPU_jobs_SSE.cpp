/*
 * File: CPU_jobs_SSE.cpp
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-01-23
 * Last Modified: 2026-08-16
 * Description: SSE2 job kernels, with their job cycles, family and ladder cross-checks, arena seeding, completion poll and comparison.
 * To Do: 1) Raise with the standard's owners that r17's ISA vocabulary has no SSE2 token; nothing here is above that set (C1)
 *        2) Add /// API documentation with @param tags to the four kernels and four job cycles defined here (GCS d1)
 * Dependencies: typedefs.h, CPU_build.h, CPU_job_cycles.h
 * ISA: Scalar | SSE4.2
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */

#include "typedefs.h"
#include "CPU_build.h"

// This unit carries the SSE job cycles, the SSE family cross-check, the SSE arena seeding and the SSE
// completion-bitmap poll as well as the kernels, so that no 128-bit comparison or move is emitted from
// CPU.cpp, which compiles at the StreamingSIMDExtensions2 baseline (ISSUES.MD H4).

// Nothing in this unit is above SSE2, and that is now a property of the whole file rather than of its
// arithmetic alone. Its kernels were always SSE2 -- set1_pd, div_pd, sqrt_pd, add_pd, sub_pd, mul_pd, and_pd,
// set1_epi64x and nothing else -- while ResultsMatch and ThreadsRunningSSE reached for PTEST, so an SSE4.1-
// less CPU was refused a unit it could have run in full: the requirement was the comparison's, never the
// computation's (ISSUES.MD C1). Both now test a vector against zero with PCMPEQD and PMOVMSKB, so the run-time
// gate that stood in front of this unit is the x64 baseline itself and no per-file /arch setting is needed --
// the global StreamingSIMDExtensions2 already names this unit's set exactly, and CPU_build.h's x64 guard is
// what makes SSE2 something the target cannot lack. That is why no lower bound is stated below.

// What CPU.vcxproj can still do is raise this unit *above* SSE2, and that is what the guard below refuses.
// A per-file /arch:AVX, /arch:AVX2 or /arch:AVX512 leaves every intrinsic here compiling and every answer
// right, while the compiler-generated code around them takes VEX or EVEX encoding -- and wmain dispatches
// these kernels to every CPU carrying SSE2, on which a VEX opcode is an illegal instruction. Every x64 part
// older than Sandy Bridge and Bulldozer is hardware this unit exists to serve. The _mm_abs_pd definition
// below rests on the same bound from the other side: SIMD management.h states its own only for a unit
// compiling below AVX2, and this one says here that it always does. It is the AVX unit's upper bound and the
// scalar unit's, spelt for the set between them (ISSUES.MD H3, H1)
// RULE-DEV:a2,a3,a6,a12 GCS section 12 makes AVX2+FMA3+BMI2 the [MUST] CPU baseline and calls SSE-only
// variants unsupported. This unit is SSE2 and is dispatched to every x64 CPU. See the same marker in
// CPU_jobs_AVX.cpp: a CPU integrity tester cannot have a hardware baseline above the hardware it is asked
// to test, and the deviation is program-wide rather than this unit's (ISSUES.MD C1)
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
   #error "CPU_jobs_SSE.cpp must compile at the SSE2 baseline. See CPU.vcxproj and ISSUES.MD H3."
#endif

#include "CPU_job_cycles.h"

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

// The include above reaches SIMD management.h, through common functions.h, and that header defines _mm_abs_pd
// itself whenever the unit compiles below AVX2 -- which this one always does, the guard above having refused
// every setting that would raise it. Its spelling was _mm_and_epi64, an AVX-512VL instruction (ISSUES.MD I1),
// so taking it here would have put an EVEX opcode in the middle of kernels dispatched to CPUs with SSE2 only.
// The definition below is the one this unit has always used, and the #undef is what keeps it that way:
// _mm_abs_pd is a bare #define there, so an #ifndef alone would silently inherit whichever form arrived first
#undef  _mm_abs_pd
#define _mm_abs_pd(input) _mm_and_pd(_mm_castsi128_pd(_mm_set1_epi64x(0x07FFFFFFFFFFFFFFF)), (input))

// ISSUES.MD J1/J2: the shift alternates direction across the loop -- the predicate was 'i < 32' against a
// counter that never exceeds 15, so the right-shift arm was unreachable and an entire instruction class
// went unexercised. The chain is also carried in ui64 rather than si64: it relies on wraparound at every
// step, and only unsigned arithmetic is defined to wrap in every language mode. si64 and ui64 may alias
// one another's storage, so the alias below re-types the value in place without copying it out of memory

// ISSUES.MD J7: 'acc' is the running accumulator each intermediate is folded into. Every step of the value
// chain begins by taking a fourth root, which divides a relative perturbation by four and rounds it away
// before the cancellation further down can amplify it, so a single-ULP fault was absorbed four times in
// five. The accumulator reaches the result without passing through a root, and its own (acc + x) over
// |acc - x| shape is expansive rather than contracting, so a perturbation grows instead of decaying. It
// is folded into the value once, at the end. Measured over 5,000,000 seeds it stays inside 2^11 and never
// reaches zero or infinity, and it adds no instruction the kernels did not already issue

// SIMD SSE operations only
void JobSSE(fl64x2 &x) {
   fl64x2 acc = _mm_set1_pd(1.0);

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      )
      x = _mm_mul_pd(x, _mm_add_pd(_mm_mul_pd(x, _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
   }
   x = _mm_mul_pd(x, acc);
}

// ALU + SIMD SSE operations only
void JobALU_SSE(fl64x2 &x, si64 &y) {
   ui64  &v   = (ui64&)y;
   fl64x2 acc = _mm_set1_pd(1.0);

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      v *= 791ull / 14 + 502; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 9 - 294941;
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      v *= 787ull / 11 + 500; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 5 - 294937;
      )
      x = _mm_mul_pd(x, _mm_add_pd(_mm_mul_pd(x, _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
   }
   x = _mm_mul_pd(x, acc);
}

// Memory-loaded SIMD SSE operations only
void JobMemSSE(fl64x2ptrc x) {
   fl64x2 acc[4] = { _mm_set1_pd(1.0), _mm_set1_pd(1.0), _mm_set1_pd(1.0), _mm_set1_pd(1.0) };

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc[0] = _mm_div_pd(_mm_add_pd(acc[0], x[0]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[0], x[0])), _mm_set1_pd(0.01)));
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc[2] = _mm_div_pd(_mm_add_pd(acc[2], x[2]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[2], x[2])), _mm_set1_pd(0.01)));
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc[1] = _mm_div_pd(_mm_add_pd(acc[1], x[1]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[1], x[1])), _mm_set1_pd(0.01)));
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc[3] = _mm_div_pd(_mm_add_pd(acc[3], x[3]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[3], x[3])), _mm_set1_pd(0.01)));
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc[0] = _mm_div_pd(_mm_add_pd(acc[0], x[0]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[0], x[0])), _mm_set1_pd(0.01)));
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc[2] = _mm_div_pd(_mm_add_pd(acc[2], x[2]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[2], x[2])), _mm_set1_pd(0.01)));
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc[1] = _mm_div_pd(_mm_add_pd(acc[1], x[1]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[1], x[1])), _mm_set1_pd(0.01)));
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc[3] = _mm_div_pd(_mm_add_pd(acc[3], x[3]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[3], x[3])), _mm_set1_pd(0.01)));
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc[0] = _mm_div_pd(_mm_add_pd(acc[0], x[0]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[0], x[0])), _mm_set1_pd(0.01)));
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc[2] = _mm_div_pd(_mm_add_pd(acc[2], x[2]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[2], x[2])), _mm_set1_pd(0.01)));
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc[1] = _mm_div_pd(_mm_add_pd(acc[1], x[1]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[1], x[1])), _mm_set1_pd(0.01)));
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc[3] = _mm_div_pd(_mm_add_pd(acc[3], x[3]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[3], x[3])), _mm_set1_pd(0.01)));
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc[0] = _mm_div_pd(_mm_add_pd(acc[0], x[0]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[0], x[0])), _mm_set1_pd(0.01)));
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc[2] = _mm_div_pd(_mm_add_pd(acc[2], x[2]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[2], x[2])), _mm_set1_pd(0.01)));
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc[1] = _mm_div_pd(_mm_add_pd(acc[1], x[1]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[1], x[1])), _mm_set1_pd(0.01)));
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc[3] = _mm_div_pd(_mm_add_pd(acc[3], x[3]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[3], x[3])), _mm_set1_pd(0.01)));
      )
      x[0] = _mm_mul_pd(x[0], _mm_add_pd(_mm_mul_pd(x[0], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[2] = _mm_mul_pd(x[2], _mm_add_pd(_mm_mul_pd(x[2], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[1] = _mm_mul_pd(x[1], _mm_add_pd(_mm_mul_pd(x[1], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[3] = _mm_mul_pd(x[3], _mm_add_pd(_mm_mul_pd(x[3], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
   }
   x[0] = _mm_mul_pd(x[0], acc[0]);
   x[2] = _mm_mul_pd(x[2], acc[2]);
   x[1] = _mm_mul_pd(x[1], acc[1]);
   x[3] = _mm_mul_pd(x[3], acc[3]);
}

// Memory-loaded ALU + SIMD SSE operations only
void JobMemALU_SSE(fl64x2ptrc x, si64ptrc y) {
   ui64ptrc v      = (ui64ptr)y;
   fl64x2   acc[4] = { _mm_set1_pd(1.0), _mm_set1_pd(1.0), _mm_set1_pd(1.0), _mm_set1_pd(1.0) };

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc[0] = _mm_div_pd(_mm_add_pd(acc[0], x[0]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[0], x[0])), _mm_set1_pd(0.01)));
      v[0] *= 789ull / 13 + 501; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc[2] = _mm_div_pd(_mm_add_pd(acc[2], x[2]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[2], x[2])), _mm_set1_pd(0.01)));
      v[2] *= 789ull / 13 + 501; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc[1] = _mm_div_pd(_mm_add_pd(acc[1], x[1]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[1], x[1])), _mm_set1_pd(0.01)));
      v[1] *= 789ull / 13 + 501; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc[3] = _mm_div_pd(_mm_add_pd(acc[3], x[3]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[3], x[3])), _mm_set1_pd(0.01)));
      v[3] *= 789ull / 13 + 501; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc[0] = _mm_div_pd(_mm_add_pd(acc[0], x[0]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[0], x[0])), _mm_set1_pd(0.01)));
      v[0] *= 791ull / 14 + 502; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 9 - 294941;
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc[2] = _mm_div_pd(_mm_add_pd(acc[2], x[2]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[2], x[2])), _mm_set1_pd(0.01)));
      v[2] *= 791ull / 14 + 502; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 9 - 294941;
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc[1] = _mm_div_pd(_mm_add_pd(acc[1], x[1]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[1], x[1])), _mm_set1_pd(0.01)));
      v[1] *= 791ull / 14 + 502; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 9 - 294941;
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc[3] = _mm_div_pd(_mm_add_pd(acc[3], x[3]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[3], x[3])), _mm_set1_pd(0.01)));
      v[3] *= 791ull / 14 + 502; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 9 - 294941;
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc[0] = _mm_div_pd(_mm_add_pd(acc[0], x[0]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[0], x[0])), _mm_set1_pd(0.01)));
      v[0] *= 789ull / 13 + 501; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc[2] = _mm_div_pd(_mm_add_pd(acc[2], x[2]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[2], x[2])), _mm_set1_pd(0.01)));
      v[2] *= 789ull / 13 + 501; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc[1] = _mm_div_pd(_mm_add_pd(acc[1], x[1]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[1], x[1])), _mm_set1_pd(0.01)));
      v[1] *= 789ull / 13 + 501; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc[3] = _mm_div_pd(_mm_add_pd(acc[3], x[3]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[3], x[3])), _mm_set1_pd(0.01)));
      v[3] *= 789ull / 13 + 501; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc[0] = _mm_div_pd(_mm_add_pd(acc[0], x[0]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[0], x[0])), _mm_set1_pd(0.01)));
      v[0] *= 787ull / 11 + 500; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 5 - 294937;
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc[2] = _mm_div_pd(_mm_add_pd(acc[2], x[2]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[2], x[2])), _mm_set1_pd(0.01)));
      v[2] *= 787ull / 11 + 500; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 5 - 294937;
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc[1] = _mm_div_pd(_mm_add_pd(acc[1], x[1]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[1], x[1])), _mm_set1_pd(0.01)));
      v[1] *= 787ull / 11 + 500; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 5 - 294937;
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
         _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      acc[3] = _mm_div_pd(_mm_add_pd(acc[3], x[3]), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc[3], x[3])), _mm_set1_pd(0.01)));
      v[3] *= 787ull / 11 + 500; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 5 - 294937;
      )
      x[0] = _mm_mul_pd(x[0], _mm_add_pd(_mm_mul_pd(x[0], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[2] = _mm_mul_pd(x[2], _mm_add_pd(_mm_mul_pd(x[2], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[1] = _mm_mul_pd(x[1], _mm_add_pd(_mm_mul_pd(x[1], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[3] = _mm_mul_pd(x[3], _mm_add_pd(_mm_mul_pd(x[3], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
   }
   x[0] = _mm_mul_pd(x[0], acc[0]);
   x[2] = _mm_mul_pd(x[2], acc[2]);
   x[1] = _mm_mul_pd(x[1], acc[1]);
   x[3] = _mm_mul_pd(x[3], acc[3]);
}

//-- Zero test, at the baseline --//
// The one question both readers below ask of a 128-bit vector, and the only thing either of them ever wanted
// SSE4.1 for. PTEST answers it in one instruction; PCMPEQD against a zeroed vector followed by PMOVMSKB
// answers it in three, and both are SSE2. The mask carries one bit per byte of the comparison result, so all
// sixteen are set exactly when all four 32-bit lanes matched zero -- which is exactly when all 128 bits are
// zero. Nothing is examined that PTEST examined, and nothing is left unexamined that it did not: the two are
// the same predicate at two instruction sets (ISSUES.MD C1).
//
// _mm_movemask_epi8 returns an int, and 0x0FFFF is the whole of its range here; the comparison is written
// against that literal rather than assigned to a named integer, so no bare int is declared (GCS r1)

/// @brief  Test all 128 bits of a vector against zero
/// @param  value: Vector to test
/// @return true if every one of the 128 bits is zero
static inline cbool AllBitsZero128(cui128 value) {
   return _mm_movemask_epi8(_mm_cmpeq_epi32(value, _mm_setzero_si128())) == 0x0FFFF;
}
//-- Zero test, at the baseline --//

//-- Bit-exact result comparison --//
// XOR the two operands and test the difference against zero, over the integer domain: a golden value is a bit
// pattern, and every other spelling of "equal" this codebase has reached for compared something less than
// every bit. CPU_job_cycles.h carries the whole of that reasoning (ISSUES.MD A11). Only the test itself has
// changed -- _mm_testz_si128 for the SSE2 fold above -- and the question it asks has not (C1)

/// @brief  Compare a 128-bit (SSE) result against its golden value
/// @param  result:   Value produced by the job kernel
/// @param  expected: Reference value loaded from "cpu.values"
/// @return true if every bit of both operands is identical
static inline cbool ResultsMatch(cfl64x2 result, cfl64x2 expected) {
   csi128 delta = _mm_xor_si128(_mm_castpd_si128(result), _mm_castpd_si128(expected));

   return AllBitsZero128(delta);
}

//--- Thread completion bitmap ---//
// The 128-bit view of the map: two ui64 per step, so the four steps below cover all 64 bytes of it. See the
// note in CPU.h for why the address comes from ThreadBitsView.
//
// AllFalse(cui128, cui128) in "common functions.h" is _mm_testz_si128, and calling it here was the second of
// this unit's two PTEST sites -- the one that faulted before any test began, and before the pre-flight check
// that names a missing instruction set could be reached (ISSUES.MD D4). The fold above answers the same
// question at SSE2, so this poll is now executable wherever the kernels beside it are (C1)
bool ThreadsRunningSSE(void) {
   cui64ptrc bits = (cui64ptrc)ThreadBitsView();

   return !(AllBitsZero128(_mm_loadu_si128((cui128ptr)&bits[0])) && AllBitsZero128(_mm_loadu_si128((cui128ptr)&bits[2])) &&
            AllBitsZero128(_mm_loadu_si128((cui128ptr)&bits[4])) && AllBitsZero128(_mm_loadu_si128((cui128ptr)&bits[6])));
}
//--- Thread completion bitmap ---//

//--- Job kernel cross-check ---//

// The SSE family. Reached on every CPU, SSE being the golden ladder's fallback
cui8 ValidateFamilySSE(cRESULTS &seed) {
   fl64x2 refSSE, memSSE[4], regSSE;
   si64   refALU, memALU[4], regALU;
   ui8    k;

   refALU = seed.alu;   JobALU(refALU);
   refSSE = seed.sse;   JobSSE(refSSE);

   regSSE = seed.sse;   regALU = seed.alu;   JobALU_SSE(regSSE, regALU);
   if(memcmp(&regSSE, &refSSE, sizeof(fl64x2)) || regALU != refALU) return 5;

   for(k = 0; k < 4; ++k) memSSE[k] = seed.sse;
   JobMemSSE(memSSE);
   for(k = 0; k < 4; ++k) if(memcmp(&memSSE[k], &refSSE, sizeof(fl64x2))) return 6;

   for(k = 0; k < 4; ++k) { memSSE[k] = seed.sse;   memALU[k] = seed.alu; }
   JobMemALU_SSE(memSSE, memALU);
   for(k = 0; k < 4; ++k) if(memcmp(&memSSE[k], &refSSE, sizeof(fl64x2)) || memALU[k] != refALU) return 7;

   return 0;
}

// The SSE half of the golden ladder's cross-width equivalence: JobSSE against JobFPU, lane for lane. The
// loads are unaligned because the probe is a scalar array of the caller's, and the comparison is one memcmp
// over the whole set because a fl64x2 array is its lanes in memory order (ISSUES.MD B1)
cui8 ValidateLadderSSE(cfl64ptrc probe, cfl64ptrc reference) {
   fl64x2 lane[LADDER_PROBE_LANES / 2];
   ui8    k;

   for(k = 0; k < LADDER_PROBE_LANES / 2; ++k) lane[k] = _mm_loadu_pd(&probe[k * 2]);
   for(k = 0; k < LADDER_PROBE_LANES / 2; ++k) JobSSE(lane[k]);

   return memcmp(lane, reference, sizeof(fl64) * LADDER_PROBE_LANES) ? KERNEL_NAME_LADDER : 0; // JobSSE
}
//--- Job kernel cross-check ---//

//--- Arena seeding ---//

void SeedRecordsSSE(fl64x2ptrc records, cui64 count, cfl64x2 &seed) {
   for(ui64 i = 0; i < count; ++i) records[i] = seed;
}
//--- Arena seeding ---//

//--- Job cycles ---//

cui8 JobCycleSSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].sse = value[0][coreNum].sse;
   JobSSE(value[1][coreNum].sse);
   if(!ResultsMatch(value[1][coreNum].sse, value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].sse;
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   return 0;
}

cui8 JobCycleMemSSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p2[offset]     = value[0][coreNum].sse;
   value[1][coreNum].p2[offset + 1] = value[0][coreNum].sse;
   value[1][coreNum].p2[offset + 2] = value[0][coreNum].sse;
   value[1][coreNum].p2[offset + 3] = value[0][coreNum].sse;
   JobMemSSE(&value[1][coreNum].p2[offset]);
   if(!ResultsMatch(value[1][coreNum].p2[offset], value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p2[offset + 1], value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 1];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p2[offset + 2], value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 2];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p2[offset + 3], value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 3];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   return 0;
}

cui8 JobCycleALU_SSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].sse = value[0][coreNum].sse;
   value[1][coreNum].alu = value[0][coreNum].alu;
   JobALU_SSE(value[1][coreNum].sse, value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].sse, value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].sse;
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   return 0;
}

cui8 JobCycleMemALU_SSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p2[offset]     = value[0][coreNum].sse;
   value[1][coreNum].p2[offset + 1] = value[0][coreNum].sse;
   value[1][coreNum].p2[offset + 2] = value[0][coreNum].sse;
   value[1][coreNum].p2[offset + 3] = value[0][coreNum].sse;
   value[1][coreNum].p4[offset]     = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 1] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 2] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 3] = value[0][coreNum].alu;
   JobMemALU_SSE(&value[1][coreNum].p2[offset], &value[1][coreNum].p4[offset]);
   if(!ResultsMatch(value[1][coreNum].p2[offset], value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p2[offset + 1], value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 1];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p2[offset + 2], value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 2];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p2[offset + 3], value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 3];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(value[1][coreNum].p4[offset] != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].p4[offset];
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(value[1][coreNum].p4[offset + 1] != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].p4[offset + 1];
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(value[1][coreNum].p4[offset + 2] != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].p4[offset + 2];
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(value[1][coreNum].p4[offset + 3] != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].p4[offset + 3];
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   return 0;
}
//--- Job cycles ---//
