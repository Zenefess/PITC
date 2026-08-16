/************************************************************
 * File: CPU_jobs_AVX.cpp               Created: 2025/01/23 *
 *                                    Last mod.: 2026/08/15 *
 *                                                          *
 * Desc: AVX2 job kernels.                                  *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/

#include "typedefs.h"
#include "CPU_build.h"

// This unit must be compiled with /arch:AVX2, in every configuration. MSVC accepts an AVX2 intrinsic whatever
// /arch is set to, so a unit compiled at the SSE2 baseline still produces the right answer -- but it will not
// use VEX encoding for the code around the intrinsics, which costs an AVX-to-SSE transition penalty on every
// boundary, and it leaves the compiler allocating registers for an instruction set it has been told the
// target does not have. Both per-file overrides in CPU.vcxproj used to carry Condition="…=='Release|x64'",
// which is exactly the Debug build this rejects (ISSUES.MD H3). The complementary guard -- that the scalar
// unit is never raised -- is in CPU_jobs_standard.cpp (H1)
#if !defined(__AVX2__)
   #error "CPU_jobs_AVX.cpp must be compiled with /arch:AVX2. See CPU.vcxproj and ISSUES.MD H3."
#endif

// ...and with /arch:AVX2 rather than with anything above it, which is the half of the bound that was missing.
// /arch:AVX512 defines __AVX2__ as well, so raising this one file's per-file setting satisfies the guard above
// while the compiler-generated code around these intrinsics becomes EVEX-encoded -- and wmain gates this
// unit's kernels on cfg.sys.cpuAVX2, so it dispatches them to every CPU reporting plain AVX2, where an EVEX
// opcode is an illegal instruction. A lower bound alone cannot catch that: the arithmetic would still be
// right and every intrinsic below would still compile. The scalar unit's guard is two-sided for exactly this
// reason, and this is its counterpart at the other end of the range (ISSUES.MD H3, H1)
#if defined(__AVX512F__)
   #error "CPU_jobs_AVX.cpp must be compiled with /arch:AVX2, not above it. See CPU.vcxproj and ISSUES.MD H3."
#endif

// The AVX2 job cycles, family cross-check, arena seeding and completion-bitmap poll live here rather than in
// CPU.cpp, beside the kernels of their own width, so that no 256-bit instruction is emitted from a file
// compiled at the StreamingSIMDExtensions2 baseline (ISSUES.MD H4)
#include "CPU_job_cycles.h"

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

// SIMD management.h, reached through the include above, defines _mm256_abs_pd as well -- but only for a unit
// compiled below AVX2, which the #error above has already refused. The #undef is here for the same reason as
// the one in CPU_jobs_SSE.cpp: the definition this unit's kernels are written against is this one, and it is
// a bare #define over there, so an #ifndef alone would inherit whichever spelling arrived first
#undef  _mm256_abs_pd
#define _mm256_abs_pd(input) _mm256_and_pd((fl64x4&)_mm256_set1_epi64x(0x07FFFFFFFFFFFFFFF), (input))

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

// SIMD AVX2 operations only
void JobAVX2(fl64x4 &x) {
   fl64x4 acc = _mm256_set1_pd(1.0);

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x, _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc = _mm256_div_pd(_mm256_add_pd(acc, x), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc, x)), _mm256_set1_pd(0.01)));
      x = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x, _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc = _mm256_div_pd(_mm256_add_pd(acc, x), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc, x)), _mm256_set1_pd(0.01)));
      x = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x, _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc = _mm256_div_pd(_mm256_add_pd(acc, x), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc, x)), _mm256_set1_pd(0.01)));
      x = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x, _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc = _mm256_div_pd(_mm256_add_pd(acc, x), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc, x)), _mm256_set1_pd(0.01)));
      )
      x = _mm256_mul_pd(x, _mm256_add_pd(_mm256_mul_pd(x, _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
   }
   x = _mm256_mul_pd(x, acc);
}

// ALU + SIMD AVX2 operations only
void JobALU_AVX2(fl64x4 &x, si64 &y) {
   ui64  &v   = (ui64&)y;
   fl64x4 acc = _mm256_set1_pd(1.0);

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x, _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc = _mm256_div_pd(_mm256_add_pd(acc, x), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc, x)), _mm256_set1_pd(0.01)));
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      x = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x, _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc = _mm256_div_pd(_mm256_add_pd(acc, x), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc, x)), _mm256_set1_pd(0.01)));
      v *= 791ull / 14 + 502; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 9 - 294941;
      x = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x, _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc = _mm256_div_pd(_mm256_add_pd(acc, x), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc, x)), _mm256_set1_pd(0.01)));
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      x = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x, _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc = _mm256_div_pd(_mm256_add_pd(acc, x), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc, x)), _mm256_set1_pd(0.01)));
      v *= 787ull / 11 + 500; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 5 - 294937;
      )
      x = _mm256_mul_pd(x, _mm256_add_pd(_mm256_mul_pd(x, _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
   }
   x = _mm256_mul_pd(x, acc);
}

// Memory-loaded SIMD AVX2 operations only
void JobMemAVX2(fl64x4ptrc x) {
   fl64x4 acc[4] = { _mm256_set1_pd(1.0), _mm256_set1_pd(1.0), _mm256_set1_pd(1.0), _mm256_set1_pd(1.0) };

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[0], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc[0] = _mm256_div_pd(_mm256_add_pd(acc[0], x[0]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[0], x[0])), _mm256_set1_pd(0.01)));
      x[2] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[2], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc[2] = _mm256_div_pd(_mm256_add_pd(acc[2], x[2]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[2], x[2])), _mm256_set1_pd(0.01)));
      x[1] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[1], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc[1] = _mm256_div_pd(_mm256_add_pd(acc[1], x[1]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[1], x[1])), _mm256_set1_pd(0.01)));
      x[3] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[3], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc[3] = _mm256_div_pd(_mm256_add_pd(acc[3], x[3]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[3], x[3])), _mm256_set1_pd(0.01)));
      x[0] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[0], _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc[0] = _mm256_div_pd(_mm256_add_pd(acc[0], x[0]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[0], x[0])), _mm256_set1_pd(0.01)));
      x[2] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[2], _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc[2] = _mm256_div_pd(_mm256_add_pd(acc[2], x[2]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[2], x[2])), _mm256_set1_pd(0.01)));
      x[1] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[1], _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc[1] = _mm256_div_pd(_mm256_add_pd(acc[1], x[1]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[1], x[1])), _mm256_set1_pd(0.01)));
      x[3] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[3], _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc[3] = _mm256_div_pd(_mm256_add_pd(acc[3], x[3]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[3], x[3])), _mm256_set1_pd(0.01)));
      x[0] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[0], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc[0] = _mm256_div_pd(_mm256_add_pd(acc[0], x[0]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[0], x[0])), _mm256_set1_pd(0.01)));
      x[2] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[2], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc[2] = _mm256_div_pd(_mm256_add_pd(acc[2], x[2]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[2], x[2])), _mm256_set1_pd(0.01)));
      x[1] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[1], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc[1] = _mm256_div_pd(_mm256_add_pd(acc[1], x[1]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[1], x[1])), _mm256_set1_pd(0.01)));
      x[3] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[3], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc[3] = _mm256_div_pd(_mm256_add_pd(acc[3], x[3]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[3], x[3])), _mm256_set1_pd(0.01)));
      x[0] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[0], _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc[0] = _mm256_div_pd(_mm256_add_pd(acc[0], x[0]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[0], x[0])), _mm256_set1_pd(0.01)));
      x[2] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[2], _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc[2] = _mm256_div_pd(_mm256_add_pd(acc[2], x[2]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[2], x[2])), _mm256_set1_pd(0.01)));
      x[1] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[1], _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc[1] = _mm256_div_pd(_mm256_add_pd(acc[1], x[1]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[1], x[1])), _mm256_set1_pd(0.01)));
      x[3] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[3], _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc[3] = _mm256_div_pd(_mm256_add_pd(acc[3], x[3]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[3], x[3])), _mm256_set1_pd(0.01)));
      )
      x[0] = _mm256_mul_pd(x[0], _mm256_add_pd(_mm256_mul_pd(x[0], _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
      x[2] = _mm256_mul_pd(x[2], _mm256_add_pd(_mm256_mul_pd(x[2], _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
      x[1] = _mm256_mul_pd(x[1], _mm256_add_pd(_mm256_mul_pd(x[1], _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
      x[3] = _mm256_mul_pd(x[3], _mm256_add_pd(_mm256_mul_pd(x[3], _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
   }
   x[0] = _mm256_mul_pd(x[0], acc[0]);
   x[2] = _mm256_mul_pd(x[2], acc[2]);
   x[1] = _mm256_mul_pd(x[1], acc[1]);
   x[3] = _mm256_mul_pd(x[3], acc[3]);
}

// Memory-loaded ALU + SIMD AVX2 operations only
void JobMemALU_AVX2(fl64x4ptrc x, si64ptrc y) {
   ui64ptrc v      = (ui64ptr)y;
   fl64x4   acc[4] = { _mm256_set1_pd(1.0), _mm256_set1_pd(1.0), _mm256_set1_pd(1.0), _mm256_set1_pd(1.0) };

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[0], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc[0] = _mm256_div_pd(_mm256_add_pd(acc[0], x[0]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[0], x[0])), _mm256_set1_pd(0.01)));
      v[0] *= 789ull / 13 + 501; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;
      x[2] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[2], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc[2] = _mm256_div_pd(_mm256_add_pd(acc[2], x[2]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[2], x[2])), _mm256_set1_pd(0.01)));
      v[2] *= 789ull / 13 + 501; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      x[1] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[1], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc[1] = _mm256_div_pd(_mm256_add_pd(acc[1], x[1]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[1], x[1])), _mm256_set1_pd(0.01)));
      v[1] *= 789ull / 13 + 501; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;
      x[3] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.12)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[3], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.0001)));
      acc[3] = _mm256_div_pd(_mm256_add_pd(acc[3], x[3]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[3], x[3])), _mm256_set1_pd(0.01)));
      v[3] *= 789ull / 13 + 501; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      x[0] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[0], _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc[0] = _mm256_div_pd(_mm256_add_pd(acc[0], x[0]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[0], x[0])), _mm256_set1_pd(0.01)));
      v[0] *= 791ull / 14 + 502; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 9 - 294941;
      x[2] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[2], _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc[2] = _mm256_div_pd(_mm256_add_pd(acc[2], x[2]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[2], x[2])), _mm256_set1_pd(0.01)));
      v[2] *= 791ull / 14 + 502; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 9 - 294941;
      x[1] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[1], _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc[1] = _mm256_div_pd(_mm256_add_pd(acc[1], x[1]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[1], x[1])), _mm256_set1_pd(0.01)));
      v[1] *= 791ull / 14 + 502; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 9 - 294941;
      x[3] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.91)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[3], _mm256_set1_pd(2.011)))))), _mm256_set1_pd(0.001)));
      acc[3] = _mm256_div_pd(_mm256_add_pd(acc[3], x[3]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[3], x[3])), _mm256_set1_pd(0.01)));
      v[3] *= 791ull / 14 + 502; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 9 - 294941;
      x[0] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[0], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc[0] = _mm256_div_pd(_mm256_add_pd(acc[0], x[0]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[0], x[0])), _mm256_set1_pd(0.01)));
      v[0] *= 789ull / 13 + 501; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;
      x[2] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[2], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc[2] = _mm256_div_pd(_mm256_add_pd(acc[2], x[2]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[2], x[2])), _mm256_set1_pd(0.01)));
      v[2] *= 789ull / 13 + 501; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      x[1] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[1], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc[1] = _mm256_div_pd(_mm256_add_pd(acc[1], x[1]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[1], x[1])), _mm256_set1_pd(0.01)));
      v[1] *= 789ull / 13 + 501; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;
      x[3] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(1.15)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[3], _mm256_set1_pd(2.01)))))), _mm256_set1_pd(0.01)));
      acc[3] = _mm256_div_pd(_mm256_add_pd(acc[3], x[3]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[3], x[3])), _mm256_set1_pd(0.01)));
      v[3] *= 789ull / 13 + 501; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      x[0] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[0], _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc[0] = _mm256_div_pd(_mm256_add_pd(acc[0], x[0]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[0], x[0])), _mm256_set1_pd(0.01)));
      v[0] *= 787ull / 11 + 500; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 5 - 294937;
      x[2] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[2], _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc[2] = _mm256_div_pd(_mm256_add_pd(acc[2], x[2]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[2], x[2])), _mm256_set1_pd(0.01)));
      v[2] *= 787ull / 11 + 500; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 5 - 294937;
      x[1] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[1], _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc[1] = _mm256_div_pd(_mm256_add_pd(acc[1], x[1]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[1], x[1])), _mm256_set1_pd(0.01)));
      v[1] *= 787ull / 11 + 500; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 5 - 294937;
      x[3] = _mm256_div_pd(_mm256_sqrt_pd(_mm256_set1_pd(0.85)), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(_mm256_set1_pd(1.0),
             _mm256_sqrt_pd(_mm256_sqrt_pd(_mm256_div_pd(x[3], _mm256_set1_pd(2.009)))))), _mm256_set1_pd(0.1)));
      acc[3] = _mm256_div_pd(_mm256_add_pd(acc[3], x[3]), _mm256_add_pd(_mm256_abs_pd(_mm256_sub_pd(acc[3], x[3])), _mm256_set1_pd(0.01)));
      v[3] *= 787ull / 11 + 500; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 5 - 294937;
      )
      x[0] = _mm256_mul_pd(x[0], _mm256_add_pd(_mm256_mul_pd(x[0], _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
      x[2] = _mm256_mul_pd(x[2], _mm256_add_pd(_mm256_mul_pd(x[2], _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
      x[1] = _mm256_mul_pd(x[1], _mm256_add_pd(_mm256_mul_pd(x[1], _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
      x[3] = _mm256_mul_pd(x[3], _mm256_add_pd(_mm256_mul_pd(x[3], _mm256_set1_pd(1.01010101010101)), _mm256_set1_pd(0.00021)));
   }
   x[0] = _mm256_mul_pd(x[0], acc[0]);
   x[2] = _mm256_mul_pd(x[2], acc[2]);
   x[1] = _mm256_mul_pd(x[1], acc[1]);
   x[3] = _mm256_mul_pd(x[3], acc[3]);
}

//-- Bit-exact result comparison --//
// XOR the two operands and test the difference against zero, over the integer domain: a golden value is a bit
// pattern, and every other spelling of "equal" this codebase has reached for compared something less than
// every bit -- _mm256_testc_pd, which this replaced, examines each lane's sign bit and nothing else, and
// every job output is positive. CPU_job_cycles.h carries the whole of that reasoning (ISSUES.MD A11)

/// @brief  Compare a 256-bit (AVX2) result against its golden value
/// @param  result:   Value produced by the job kernel
/// @param  expected: Reference value loaded from "cpu.values"
/// @return true if every bit of both operands is identical
static inline cbool ResultsMatch(cfl64x4 result, cfl64x4 expected) {
   csi256 delta = _mm256_xor_si256(_mm256_castpd_si256(result), _mm256_castpd_si256(expected));

   return _mm256_testz_si256(delta, delta);
}

//--- Thread completion bitmap ---//
// The 256-bit view of the map: four ui64 per step, so the two steps below cover all 64 bytes of it. Stepping
// one element per vector re-read bits already examined and left bytes 48~63 unchecked (ISSUES.MD D3)
bool ThreadsRunningAVX(void) {
   cui64ptrc bits = (cui64ptrc)ThreadBitsView();

   return !(AllFalse(_mm256_loadu_si256((cui256ptr)&bits[0]), max256) && AllFalse(_mm256_loadu_si256((cui256ptr)&bits[4]), max256));
}
//--- Thread completion bitmap ---//

//--- Job kernel cross-check ---//

// The AVX2 family. ValidateKernelFamilies (CPU.h) reaches this only on a CPU reporting AVX2, exactly as
// RunGoldenLadder gates the ladder itself
cui8 ValidateFamilyAVX2(cRESULTS &seed) {
   fl64x4 refAVX2, memAVX2[4], regAVX2;
   si64   refALU,  memALU[4],  regALU;
   ui8    k;

   refALU  = seed.alu;   JobALU(refALU);
   refAVX2 = seed.avx;   JobAVX2(refAVX2);

   regAVX2 = seed.avx;   regALU = seed.alu;   JobALU_AVX2(regAVX2, regALU);
   if(memcmp(&regAVX2, &refAVX2, sizeof(fl64x4)) || regALU != refALU) return 8;

   for(k = 0; k < 4; ++k) memAVX2[k] = seed.avx;
   JobMemAVX2(memAVX2);
   for(k = 0; k < 4; ++k) if(memcmp(&memAVX2[k], &refAVX2, sizeof(fl64x4))) return 9;

   for(k = 0; k < 4; ++k) { memAVX2[k] = seed.avx;   memALU[k] = seed.alu; }
   JobMemALU_AVX2(memAVX2, memALU);
   for(k = 0; k < 4; ++k) if(memcmp(&memAVX2[k], &refAVX2, sizeof(fl64x4)) || memALU[k] != refALU) return 10;

   return 0;
}

// The AVX2 half of the golden ladder's cross-width equivalence: JobAVX2 against JobFPU, lane for lane. The
// loads are unaligned because the probe is a scalar array of the caller's, and the comparison is one memcmp
// over the whole set because a fl64x4 array is its lanes in memory order (ISSUES.MD B1)
cui8 ValidateLadderAVX2(cfl64ptrc probe, cfl64ptrc reference) {
   fl64x4 lane[LADDER_PROBE_LANES / 4];
   ui8    k;

   for(k = 0; k < LADDER_PROBE_LANES / 4; ++k) lane[k] = _mm256_loadu_pd(&probe[k * 4]);
   for(k = 0; k < LADDER_PROBE_LANES / 4; ++k) JobAVX2(lane[k]);

   return memcmp(lane, reference, sizeof(fl64) * LADDER_PROBE_LANES) ? ui8(KERNEL_NAME_LADDER + 1) : 0; // JobAVX2
}
//--- Job kernel cross-check ---//

//--- Arena seeding ---//

void SeedRecordsAVX2(fl64x4ptrc records, cui64 count, cfl64x4 &seed) {
   for(ui64 i = 0; i < count; ++i) records[i] = seed;
}
//--- Arena seeding ---//

//--- Job cycles ---//

cui8 JobCycleAVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx = value[0][coreNum].avx;
   JobAVX2(value[1][coreNum].avx);
   if(!ResultsMatch(value[1][coreNum].avx, value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].avx;
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   return 0;
}

cui8 JobCycleMemAVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p1[offset]     = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 1] = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 2] = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 3] = value[0][coreNum].avx;
   JobMemAVX2(&value[1][coreNum].p1[offset]);
   if(!ResultsMatch(value[1][coreNum].p1[offset], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p1[offset + 1], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 1];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p1[offset + 2], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 2];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p1[offset + 3], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 3];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   return 0;
}

cui8 JobCycleALU_AVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx = value[0][coreNum].avx;
   value[1][coreNum].alu = value[0][coreNum].alu;
   JobALU_AVX2(value[1][coreNum].avx, value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].avx, value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].avx;
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   return 0;
}

cui8 JobCycleMemALU_AVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p1[offset]     = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 1] = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 2] = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 3] = value[0][coreNum].avx;
   value[1][coreNum].p4[offset]     = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 1] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 2] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 3] = value[0][coreNum].alu;
   JobMemALU_AVX2(&value[1][coreNum].p1[offset], &value[1][coreNum].p4[offset]);
   if(!ResultsMatch(value[1][coreNum].p1[offset], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p1[offset + 1], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 1];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p1[offset + 2], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 2];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!ResultsMatch(value[1][coreNum].p1[offset + 3], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 3];
      Failed(coreNum, threadByte, 1);
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
