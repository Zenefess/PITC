/************************************************************
 * File: CPU_jobs_SSE.cpp               Created: 2025/01/23 *
 *                                    Last mod.: 2026/08/15 *
 *                                                          *
 * Desc: SSE job kernels.                                   *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/

#include "typedefs.h"
#include "CPU_build.h"

// This unit carries the SSE job cycles, the SSE family cross-check, the SSE arena seeding and the SSE
// completion-bitmap poll as well as the kernels, so that no 128-bit comparison or move is emitted from
// CPU.cpp, which compiles at the StreamingSIMDExtensions2 baseline (ISSUES.MD H4). It is the one unit whose
// own instruction set CPU.vcxproj cannot name: MSVC offers /arch:AVX2 and /arch:AVX512 but nothing for
// SSE4.1, which is why _mm_testz_si128 below carries a run-time gate (cfg.sys.cpuSSE4_1) rather than a
// build-time one, and why this unit states no lower bound -- there is no macro a correct build would set.

// What CPU.vcxproj can do is raise this unit *above* its own set, and that is what the guard below refuses.
// A per-file /arch:AVX, /arch:AVX2 or /arch:AVX512 leaves every intrinsic here compiling and every answer
// right, while the compiler-generated code around them takes VEX or EVEX encoding -- and wmain gates these
// kernels on cfg.sys.cpuSSE4_1, so it dispatches them to every CPU reporting SSE4.1, on which a VEX opcode
// is an illegal instruction. The Penryn, Nehalem and Westmere parts that carry SSE4.1 and no AVX are exactly
// the hardware this unit exists to serve. The _mm_abs_pd definition below rests on the same bound from the
// other side: SIMD management.h states its own only for a unit compiling below AVX2, and this one says here
// that it always does. It is the AVX2 unit's upper bound and the scalar unit's, spelt for the set between
// them (ISSUES.MD H3, H1)
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
   #error "CPU_jobs_SSE.cpp must compile at the SSE2 baseline. See CPU.vcxproj and ISSUES.MD H3."
#endif

#include "CPU_job_cycles.h"

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

// The include above reaches SIMD management.h, through common functions.h, and that header defines _mm_abs_pd
// itself whenever the unit compiles below AVX2 -- which this one always does, MSVC offering no /arch for
// SSE4.1. Its spelling is _mm_and_epi64, an AVX-512VL instruction (ISSUES.MD I1), so taking it here would put
// an EVEX opcode in the middle of the kernels this program dispatches to CPUs that have SSE and nothing more.
// The definition below is the one this unit has always used, and the #undef is what keeps it that way:
// _mm_abs_pd is a bare #define there, so an #ifndef alone would silently inherit the AVX-512 form
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

//-- Bit-exact result comparison --//
// XOR the two operands and test the difference against zero, over the integer domain: a golden value is a bit
// pattern, and every other spelling of "equal" this codebase has reached for compared something less than
// every bit. CPU_job_cycles.h carries the whole of that reasoning (ISSUES.MD A11)

/// @brief  Compare a 128-bit (SSE) result against its golden value
/// @param  result:   Value produced by the job kernel
/// @param  expected: Reference value loaded from "cpu.values"
/// @return true if every bit of both operands is identical
static inline cbool ResultsMatch(cfl64x2 result, cfl64x2 expected) {
   csi128 delta = _mm_xor_si128(_mm_castpd_si128(result), _mm_castpd_si128(expected));

   return _mm_testz_si128(delta, delta);
}

//--- Thread completion bitmap ---//
// The 128-bit view of the map: two ui64 per step, so the four steps below cover all 64 bytes of it. See the
// note in CPU.h for why the address comes from ThreadBitsView and why this is not the baseline poll
bool ThreadsRunningSSE(void) {
   cui64ptrc bits = (cui64ptrc)ThreadBitsView();

   return !(AllFalse(_mm_loadu_si128((cui128ptr)&bits[0]), max128) && AllFalse(_mm_loadu_si128((cui128ptr)&bits[2]), max128) &&
            AllFalse(_mm_loadu_si128((cui128ptr)&bits[4]), max128) && AllFalse(_mm_loadu_si128((cui128ptr)&bits[6]), max128));
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
