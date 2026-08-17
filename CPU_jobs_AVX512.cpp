/*
 * File: CPU_jobs_AVX512.cpp
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-01-23
 * Last Modified: 2026-08-16
 * Description: AVX-512 job kernels, with their job cycles, family and ladder cross-checks, arena seeding, completion poll and comparison.
 * To Do: 1) Make ThreadsRunningAVX512 genuinely 512-bit: AllFalse(cui512&, cui512&) issues two 256-bit vptests (ISSUES.MD I10)
 *        2) Add /// API documentation with @param tags to the four kernels and four job cycles defined here (GCS d1)
 * Dependencies: typedefs.h, CPU_build.h, CPU_job_cycles.h
 * ISA: Scalar | AVX2 | AVX-512
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */

#include "typedefs.h"
#include "CPU_build.h"

// This unit must be compiled with /arch:AVX512, in every configuration. MSVC accepts an AVX-512 intrinsic
// whatever /arch is set to, so a unit compiled at the SSE2 baseline still produces the right answer -- but it
// emits ZMM-using code without having been told the target supports it, which decides register allocation and
// vzeroupper placement, and leaves the surrounding code unable to use EVEX encoding. Both per-file overrides
// in CPU.vcxproj used to carry Condition="…=='Release|x64'", which is exactly the Debug build this rejects
// (ISSUES.MD H3). The complementary guard -- that the scalar unit is never raised -- is in
// CPU_jobs_standard.cpp (H1)
#if !defined(__AVX512F__)
   #error "CPU_jobs_AVX512.cpp must be compiled with /arch:AVX512. See CPU.vcxproj and ISSUES.MD H3."
#endif

// The AVX-512 job cycles, family cross-check, arena seeding and completion-bitmap poll live here rather than
// in CPU.cpp, beside the kernels of their own width, so that no 512-bit instruction is emitted from a file
// compiled at the StreamingSIMDExtensions2 baseline (ISSUES.MD H4)
#include "CPU_job_cycles.h"

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

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

// SIMD AVX512 operations only
void JobAVX512(fl64x8 &x) {
   fl64x8 acc = _mm512_set1_pd(1.0);

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc = _mm512_div_pd(_mm512_add_pd(acc, x), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc, x)), _mm512_set1_pd(0.01)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc = _mm512_div_pd(_mm512_add_pd(acc, x), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc, x)), _mm512_set1_pd(0.01)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc = _mm512_div_pd(_mm512_add_pd(acc, x), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc, x)), _mm512_set1_pd(0.01)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc = _mm512_div_pd(_mm512_add_pd(acc, x), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc, x)), _mm512_set1_pd(0.01)));
      )
      x = _mm512_mul_pd(x, _mm512_add_pd(_mm512_mul_pd(x, _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
   }
   x = _mm512_mul_pd(x, acc);
}

// ALU + SIMD AVX512 operations only
void JobALU_AVX512(fl64x8 &x, si64 &y) {
   ui64  &v   = (ui64&)y;
   fl64x8 acc = _mm512_set1_pd(1.0);

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc = _mm512_div_pd(_mm512_add_pd(acc, x), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc, x)), _mm512_set1_pd(0.01)));
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc = _mm512_div_pd(_mm512_add_pd(acc, x), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc, x)), _mm512_set1_pd(0.01)));
      v *= 791ull / 14 + 502; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 9 - 294941;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc = _mm512_div_pd(_mm512_add_pd(acc, x), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc, x)), _mm512_set1_pd(0.01)));
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc = _mm512_div_pd(_mm512_add_pd(acc, x), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc, x)), _mm512_set1_pd(0.01)));
      v *= 787ull / 11 + 500; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 5 - 294937;
      )
      x = _mm512_mul_pd(x, _mm512_add_pd(_mm512_mul_pd(x, _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
   }
   x = _mm512_mul_pd(x, acc);
}

// Memory-loaded SIMD AVX512 operations only
void JobMemAVX512(fl64x8ptrc x) {
   fl64x8 acc[4] = { _mm512_set1_pd(1.0), _mm512_set1_pd(1.0), _mm512_set1_pd(1.0), _mm512_set1_pd(1.0) };

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[0], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc[0] = _mm512_div_pd(_mm512_add_pd(acc[0], x[0]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[0], x[0])), _mm512_set1_pd(0.01)));
      x[2] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[2], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc[2] = _mm512_div_pd(_mm512_add_pd(acc[2], x[2]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[2], x[2])), _mm512_set1_pd(0.01)));
      x[1] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[1], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc[1] = _mm512_div_pd(_mm512_add_pd(acc[1], x[1]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[1], x[1])), _mm512_set1_pd(0.01)));
      x[3] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[3], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc[3] = _mm512_div_pd(_mm512_add_pd(acc[3], x[3]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[3], x[3])), _mm512_set1_pd(0.01)));
      x[0] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[0], _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc[0] = _mm512_div_pd(_mm512_add_pd(acc[0], x[0]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[0], x[0])), _mm512_set1_pd(0.01)));
      x[2] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[2], _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc[2] = _mm512_div_pd(_mm512_add_pd(acc[2], x[2]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[2], x[2])), _mm512_set1_pd(0.01)));
      x[1] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[1], _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc[1] = _mm512_div_pd(_mm512_add_pd(acc[1], x[1]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[1], x[1])), _mm512_set1_pd(0.01)));
      x[3] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[3], _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc[3] = _mm512_div_pd(_mm512_add_pd(acc[3], x[3]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[3], x[3])), _mm512_set1_pd(0.01)));
      x[0] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[0], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc[0] = _mm512_div_pd(_mm512_add_pd(acc[0], x[0]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[0], x[0])), _mm512_set1_pd(0.01)));
      x[2] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[2], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc[2] = _mm512_div_pd(_mm512_add_pd(acc[2], x[2]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[2], x[2])), _mm512_set1_pd(0.01)));
      x[1] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[1], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc[1] = _mm512_div_pd(_mm512_add_pd(acc[1], x[1]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[1], x[1])), _mm512_set1_pd(0.01)));
      x[3] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[3], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc[3] = _mm512_div_pd(_mm512_add_pd(acc[3], x[3]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[3], x[3])), _mm512_set1_pd(0.01)));
      x[0] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[0], _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc[0] = _mm512_div_pd(_mm512_add_pd(acc[0], x[0]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[0], x[0])), _mm512_set1_pd(0.01)));
      x[2] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[2], _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc[2] = _mm512_div_pd(_mm512_add_pd(acc[2], x[2]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[2], x[2])), _mm512_set1_pd(0.01)));
      x[1] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[1], _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc[1] = _mm512_div_pd(_mm512_add_pd(acc[1], x[1]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[1], x[1])), _mm512_set1_pd(0.01)));
      x[3] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[3], _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc[3] = _mm512_div_pd(_mm512_add_pd(acc[3], x[3]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[3], x[3])), _mm512_set1_pd(0.01)));
      )
      x[0] = _mm512_mul_pd(x[0], _mm512_add_pd(_mm512_mul_pd(x[0], _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
      x[2] = _mm512_mul_pd(x[2], _mm512_add_pd(_mm512_mul_pd(x[2], _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
      x[1] = _mm512_mul_pd(x[1], _mm512_add_pd(_mm512_mul_pd(x[1], _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
      x[3] = _mm512_mul_pd(x[3], _mm512_add_pd(_mm512_mul_pd(x[3], _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
   }
   x[0] = _mm512_mul_pd(x[0], acc[0]);
   x[2] = _mm512_mul_pd(x[2], acc[2]);
   x[1] = _mm512_mul_pd(x[1], acc[1]);
   x[3] = _mm512_mul_pd(x[3], acc[3]);
}

// Memory-loaded ALU + SIMD AVX512 operations only
void JobMemALU_AVX512(fl64x8ptrc x, si64ptrc y) {
   ui64ptrc v      = (ui64ptr)y;
   fl64x8   acc[4] = { _mm512_set1_pd(1.0), _mm512_set1_pd(1.0), _mm512_set1_pd(1.0), _mm512_set1_pd(1.0) };

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[0], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc[0] = _mm512_div_pd(_mm512_add_pd(acc[0], x[0]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[0], x[0])), _mm512_set1_pd(0.01)));
      v[0] *= 789ull / 13 + 501; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;
      x[2] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[2], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc[2] = _mm512_div_pd(_mm512_add_pd(acc[2], x[2]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[2], x[2])), _mm512_set1_pd(0.01)));
      v[2] *= 789ull / 13 + 501; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      x[1] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[1], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc[1] = _mm512_div_pd(_mm512_add_pd(acc[1], x[1]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[1], x[1])), _mm512_set1_pd(0.01)));
      v[1] *= 789ull / 13 + 501; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;
      x[3] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[3], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      acc[3] = _mm512_div_pd(_mm512_add_pd(acc[3], x[3]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[3], x[3])), _mm512_set1_pd(0.01)));
      v[3] *= 789ull / 13 + 501; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      x[0] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[0], _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc[0] = _mm512_div_pd(_mm512_add_pd(acc[0], x[0]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[0], x[0])), _mm512_set1_pd(0.01)));
      v[0] *= 791ull / 14 + 502; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 9 - 294941;
      x[2] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[2], _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc[2] = _mm512_div_pd(_mm512_add_pd(acc[2], x[2]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[2], x[2])), _mm512_set1_pd(0.01)));
      v[2] *= 791ull / 14 + 502; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 9 - 294941;
      x[1] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[1], _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc[1] = _mm512_div_pd(_mm512_add_pd(acc[1], x[1]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[1], x[1])), _mm512_set1_pd(0.01)));
      v[1] *= 791ull / 14 + 502; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 9 - 294941;
      x[3] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[3], _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      acc[3] = _mm512_div_pd(_mm512_add_pd(acc[3], x[3]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[3], x[3])), _mm512_set1_pd(0.01)));
      v[3] *= 791ull / 14 + 502; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 9 - 294941;
      x[0] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[0], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc[0] = _mm512_div_pd(_mm512_add_pd(acc[0], x[0]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[0], x[0])), _mm512_set1_pd(0.01)));
      v[0] *= 789ull / 13 + 501; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;
      x[2] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[2], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc[2] = _mm512_div_pd(_mm512_add_pd(acc[2], x[2]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[2], x[2])), _mm512_set1_pd(0.01)));
      v[2] *= 789ull / 13 + 501; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      x[1] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[1], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc[1] = _mm512_div_pd(_mm512_add_pd(acc[1], x[1]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[1], x[1])), _mm512_set1_pd(0.01)));
      v[1] *= 789ull / 13 + 501; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;
      x[3] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[3], _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      acc[3] = _mm512_div_pd(_mm512_add_pd(acc[3], x[3]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[3], x[3])), _mm512_set1_pd(0.01)));
      v[3] *= 789ull / 13 + 501; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      x[0] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[0], _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc[0] = _mm512_div_pd(_mm512_add_pd(acc[0], x[0]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[0], x[0])), _mm512_set1_pd(0.01)));
      v[0] *= 787ull / 11 + 500; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 5 - 294937;
      x[2] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[2], _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc[2] = _mm512_div_pd(_mm512_add_pd(acc[2], x[2]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[2], x[2])), _mm512_set1_pd(0.01)));
      v[2] *= 787ull / 11 + 500; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 5 - 294937;
      x[1] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[1], _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc[1] = _mm512_div_pd(_mm512_add_pd(acc[1], x[1]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[1], x[1])), _mm512_set1_pd(0.01)));
      v[1] *= 787ull / 11 + 500; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 5 - 294937;
      x[3] = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
         _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x[3], _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      acc[3] = _mm512_div_pd(_mm512_add_pd(acc[3], x[3]), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(acc[3], x[3])), _mm512_set1_pd(0.01)));
      v[3] *= 787ull / 11 + 500; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 5 - 294937;
      )
      x[0] = _mm512_mul_pd(x[0], _mm512_add_pd(_mm512_mul_pd(x[0], _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
      x[2] = _mm512_mul_pd(x[2], _mm512_add_pd(_mm512_mul_pd(x[2], _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
      x[1] = _mm512_mul_pd(x[1], _mm512_add_pd(_mm512_mul_pd(x[1], _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
      x[3] = _mm512_mul_pd(x[3], _mm512_add_pd(_mm512_mul_pd(x[3], _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
   }
   x[0] = _mm512_mul_pd(x[0], acc[0]);
   x[2] = _mm512_mul_pd(x[2], acc[2]);
   x[1] = _mm512_mul_pd(x[1], acc[1]);
   x[3] = _mm512_mul_pd(x[3], acc[3]);
}

//-- Bit-exact result comparison --//
// XOR the two operands and test the difference against zero, over the integer domain: a golden value is a bit
// pattern, and _mm512_mask_cmpneq_pd_mask -- which this replaced -- is a floating-point predicate, so it
// answers a question about numeric values instead and errs in both directions. CPU_job_cycles.h carries the
// whole of that reasoning (ISSUES.MD A11)

/// @brief  Compare a 512-bit (AVX-512) result against its golden value
/// @param  result:   Value produced by the job kernel
/// @param  expected: Reference value loaded from "cpu.values"
/// @return true if every bit of both operands is identical
static inline cbool ResultsMatch(cfl64x8 result, cfl64x8 expected) {
   // AVX-512 has no testz: VPTESTMQ sets one mask bit per 64-bit lane whose AND is non-zero, so testing the
   // difference against itself yields a bit for every lane that differs, and an empty mask is the match
   csi512 delta = _mm512_xor_si512(_mm512_castpd_si512(result), _mm512_castpd_si512(expected));

   return !_mm512_test_epi64_mask(delta, delta);
}

//--- Thread completion bitmap ---//
// The 512-bit view of the map: eight ui64 in one step, which is the whole of it
bool ThreadsRunningAVX512(void) {
   cui64ptrc bits = (cui64ptrc)ThreadBitsView();

   return !AllFalse((si512&)bits[0], max512);
}
//--- Thread completion bitmap ---//

//--- Job kernel cross-check ---//

// The AVX-512 family. ValidateKernelFamilies (CPU.h) reaches this only on a CPU reporting AVX-512, exactly as
// RunGoldenLadder gates the ladder itself
cui8 ValidateFamilyAVX512(cRESULTS &seed) {
   fl64x8 refAVX512, memAVX512[4], regAVX512;
   si64   refALU,    memALU[4],    regALU;
   ui8    k;

   refALU    = seed.alu;      JobALU(refALU);
   refAVX512 = seed.avx512;   JobAVX512(refAVX512);

   regAVX512 = seed.avx512;   regALU = seed.alu;   JobALU_AVX512(regAVX512, regALU);
   if(memcmp(&regAVX512, &refAVX512, sizeof(fl64x8)) || regALU != refALU) return 11;

   for(k = 0; k < 4; ++k) memAVX512[k] = seed.avx512;
   JobMemAVX512(memAVX512);
   for(k = 0; k < 4; ++k) if(memcmp(&memAVX512[k], &refAVX512, sizeof(fl64x8))) return 12;

   for(k = 0; k < 4; ++k) { memAVX512[k] = seed.avx512;   memALU[k] = seed.alu; }
   JobMemALU_AVX512(memAVX512, memALU);
   for(k = 0; k < 4; ++k) if(memcmp(&memAVX512[k], &refAVX512, sizeof(fl64x8)) || memALU[k] != refALU) return 13;

   return 0;
}

// The AVX-512 half of the golden ladder's cross-width equivalence: JobAVX512 against JobFPU, lane for lane.
// The loads are unaligned because the probe is a scalar array of the caller's, and the comparison is one
// memcmp over the whole set because a fl64x8 array is its lanes in memory order (ISSUES.MD B1)
cui8 ValidateLadderAVX512(cfl64ptrc probe, cfl64ptrc reference) {
   fl64x8 lane[LADDER_PROBE_LANES / 8];
   ui8    k;

   for(k = 0; k < LADDER_PROBE_LANES / 8; ++k) lane[k] = _mm512_loadu_pd(&probe[k * 8]);
   for(k = 0; k < LADDER_PROBE_LANES / 8; ++k) JobAVX512(lane[k]);

   return memcmp(lane, reference, sizeof(fl64) * LADDER_PROBE_LANES) ? ui8(KERNEL_NAME_LADDER + 2) : 0; // JobAVX512
}
//--- Job kernel cross-check ---//

//--- Arena seeding ---//

void SeedRecordsAVX512(fl64x8ptrc records, cui64 count, cfl64x8 &seed) {
   for(ui64 i = 0; i < count; ++i) records[i] = seed;
}
//--- Arena seeding ---//

//--- Job cycles ---//

cui8 JobCycleAVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx512 = value[0][coreNum].avx512;
   JobAVX512(value[1][coreNum].avx512);
   if(!ResultsMatch(value[1][coreNum].avx512, value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].avx512;
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   return 0;
}

cui8 JobCycleMemAVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p0[offset]     = value[0][coreNum].avx512;
   value[1][coreNum].p0[offset + 1] = value[0][coreNum].avx512;
   value[1][coreNum].p0[offset + 2] = value[0][coreNum].avx512;
   value[1][coreNum].p0[offset + 3] = value[0][coreNum].avx512;
   JobMemAVX512(&value[1][coreNum].p0[offset]);
   if(!ResultsMatch(value[1][coreNum].p0[offset], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p0[offset + 1], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 1];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p0[offset + 2], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 2];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p0[offset + 3], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 3];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   return 0;
}

cui8 JobCycleALU_AVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx512 = value[0][coreNum].avx512;
   value[1][coreNum].alu    = value[0][coreNum].alu;
   JobALU_AVX512(value[1][coreNum].avx512, value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].avx512, value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].avx512;
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   return 0;
}

cui8 JobCycleMemALU_AVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p0[offset]     = value[0][coreNum].avx512;
   value[1][coreNum].p0[offset + 1] = value[0][coreNum].avx512;
   value[1][coreNum].p0[offset + 2] = value[0][coreNum].avx512;
   value[1][coreNum].p0[offset + 3] = value[0][coreNum].avx512;
   value[1][coreNum].p4[offset]     = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 1] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 2] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 3] = value[0][coreNum].alu;
   JobMemALU_AVX512(&value[1][coreNum].p0[offset], &value[1][coreNum].p4[offset]);
   if(!ResultsMatch(value[1][coreNum].p0[offset], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p0[offset + 1], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 1];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p0[offset + 2], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 2];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p0[offset + 3], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 3];
      Failed(coreNum, threadByte, 0);
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
