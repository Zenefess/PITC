/*
 * File: CPU_jobs_AVX.cpp
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-01-23
 * Last Modified: 2026-08-18
 * Description: AVX job kernels, with their job cycles, family and ladder cross-checks, arena seeding, completion poll and comparison.
 * To Do: 1) Add /// API documentation with @param tags to the four kernels and four job cycles defined here (GCS d1)
 * Dependencies: typedefs.h, CPU_build.h, CPU_job_cycles.h
 * ISA: Scalar | AVX
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */

#include "typedefs.h"
#include "CPU_build.h"

// This unit must be compiled with /arch:AVX, in every configuration.
#if !defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
   #error "CPU_jobs_AVX.cpp must be compiled with /arch:AVX."
#endif

#include "CPU_job_cycles.h"

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

#undef  _mm256_abs_pd
#define _mm256_abs_pd(input) _mm256_and_pd((fl64x4&)_mm256_set1_epi64x(0x07FFFFFFFFFFFFFFF), (input))

// SIMD AVX operations only
void JobAVX(fl64x4 &x) {
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

// ALU + SIMD AVX operations only
void JobALU_AVX(fl64x4 &x, si64 &y) {
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

// Memory-loaded SIMD AVX operations only
void JobMemAVX(fl64x4ptrc x) {
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

// Memory-loaded ALU + SIMD AVX operations only
void JobMemALU_AVX(fl64x4ptrc x, si64ptrc y) {
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
/// @brief  Compare a 256-bit (AVX) result against its golden value
/// @param  result:   Value produced by the job kernel
/// @param  expected: Reference value loaded from "cpu.values"
/// @return true if every bit of both operands is identical
static inline cbool ResultsMatch(cfl64x4 result, cfl64x4 expected) {
   csi256 delta = _mm256_castpd_si256(_mm256_xor_pd(result, expected));

   return _mm256_testz_si256(delta, delta);
}

//--- Thread completion bitmap ---//
bool ThreadsRunningAVX(void) {
   cui64ptrc bits = (cui64ptrc)ThreadBitsView();

   return !(AllFalse(_mm256_loadu_si256((cui256ptr)&bits[0]), max256) && AllFalse(_mm256_loadu_si256((cui256ptr)&bits[4]), max256));
}
//--- Thread completion bitmap ---//

//--- Job kernel cross-check ---//
cui8 ValidateFamilyAVX(cRESULTS &seed) {
   fl64x4 refAVX, memAVX[4], regAVX;
   si64   refALU, memALU[4], regALU;
   ui8    k;

   refALU = seed.alu;   JobALU(refALU);
   refAVX = seed.avx;   JobAVX(refAVX);

   regAVX = seed.avx;   regALU = seed.alu;   JobALU_AVX(regAVX, regALU);
   if(memcmp(&regAVX, &refAVX, sizeof(fl64x4)) || regALU != refALU) return 8;

   for(k = 0; k < 4; ++k) memAVX[k] = seed.avx;
   JobMemAVX(memAVX);
   for(k = 0; k < 4; ++k) if(memcmp(&memAVX[k], &refAVX, sizeof(fl64x4))) return 9;

   for(k = 0; k < 4; ++k) { memAVX[k] = seed.avx;   memALU[k] = seed.alu; }
   JobMemALU_AVX(memAVX, memALU);
   for(k = 0; k < 4; ++k) if(memcmp(&memAVX[k], &refAVX, sizeof(fl64x4)) || memALU[k] != refALU) return 10;

   return 0;
}

cui8 ValidateLadderAVX(cfl64ptrc probe, cfl64ptrc reference) {
   fl64x4 lane[LADDER_PROBE_LANES / 4];
   ui8    k;

   for(k = 0; k < LADDER_PROBE_LANES / 4; ++k) lane[k] = _mm256_loadu_pd(&probe[k * 4]);
   for(k = 0; k < LADDER_PROBE_LANES / 4; ++k) JobAVX(lane[k]);

   return memcmp(lane, reference, sizeof(fl64) * LADDER_PROBE_LANES) ? ui8(KERNEL_NAME_LADDER + 1) : 0; // JobAVX
}
//--- Job kernel cross-check ---//

//--- Arena seeding ---//
void SeedRecordsAVX(fl64x4ptrc records, cui64 count, cfl64x4 &seed) {
   for(ui64 i = 0; i < count; ++i) records[i] = seed;
}
//--- Arena seeding ---//

//--- Job cycles ---//
cui8 JobCycleAVX(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx = value[0][coreNum].avx;
   JobAVX(value[1][coreNum].avx);
   if(!ResultsMatch(value[1][coreNum].avx, value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].avx;
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   return 0;
}

cui8 JobCycleMemAVX(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p1[offset]     = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 1] = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 2] = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 3] = value[0][coreNum].avx;
   JobMemAVX(&value[1][coreNum].p1[offset]);
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

cui8 JobCycleALU_AVX(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx = value[0][coreNum].avx;
   value[1][coreNum].alu = value[0][coreNum].alu;
   JobALU_AVX(value[1][coreNum].avx, value[1][coreNum].alu);
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

cui8 JobCycleMemALU_AVX(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p1[offset]     = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 1] = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 2] = value[0][coreNum].avx;
   value[1][coreNum].p1[offset + 3] = value[0][coreNum].avx;
   value[1][coreNum].p4[offset]     = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 1] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 2] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 3] = value[0][coreNum].alu;
   JobMemALU_AVX(&value[1][coreNum].p1[offset], &value[1][coreNum].p4[offset]);
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
