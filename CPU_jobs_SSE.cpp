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

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

#ifndef _mm_abs_pd
#define _mm_abs_pd(input) _mm_and_pd((fl64x2&)_mm_set1_epi64x(0x07FFFFFFFFFFFFFFF), (input))
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
