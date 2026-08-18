/*
 * File: CPU_jobs_SSE.cpp
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-01-23
 * Last Modified: 2026-08-16
 * Description: SSE2 job kernels, with their job cycles, family and ladder cross-checks, arena seeding, completion poll and comparison.
 * To Do: 1) Add /// API documentation with @param tags to the four kernels and four job cycles defined here (GCS d1)
 * Dependencies: typedefs.h, CPU_build.h, CPU_job_cycles.h
 * ISA: Scalar | SSE2
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */

#include "typedefs.h"
#include "CPU_build.h"

 // This unit must be compiled at the SSE2 baseline, in every configuration.
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
   #error "CPU_jobs_SSE.cpp must compile at the SSE2 baseline."
#endif

#include "CPU_job_cycles.h"

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

#undef  _mm_abs_pd
#define _mm_abs_pd(input) _mm_and_pd(_mm_castsi128_pd(_mm_set1_epi64x(0x07FFFFFFFFFFFFFFF)), (input))

// SIMD SSE operations only
void JobSSE(fl64x2 &x) {
   fl64x2 acc = _mm_set1_pd(1.0);

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x   = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
            _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      x   = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
            _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      x   = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
            _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      acc = _mm_div_pd(_mm_add_pd(acc, x), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(acc, x)), _mm_set1_pd(0.01)));
      x   = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0),
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
/// @brief  Test all 128 bits of a vector against zero
/// @param  value: Vector to test
/// @return true if every one of the 128 bits is zero
static inline cbool AllBitsZero128(cui128 value) {
   return _mm_movemask_epi8(_mm_cmpeq_epi32(value, _mm_setzero_si128())) == 0x0FFFF;
}
//-- Zero test, at the baseline --//

//-- Bit-exact result comparison --//
/// @brief  Compare a 128-bit (SSE) result against its golden value
/// @param  result:   Value produced by the job kernel
/// @param  expected: Reference value loaded from "cpu.values"
/// @return true if every bit of both operands is identical
static inline cbool ResultsMatch(cfl64x2 result, cfl64x2 expected) {
   csi128 delta = _mm_xor_si128(_mm_castpd_si128(result), _mm_castpd_si128(expected));

   return AllBitsZero128(delta);
}

//--- Thread completion bitmap ---//
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
