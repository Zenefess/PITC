/*
 * File: CPU_jobs_AVX512.cpp
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-01-23
 * Last Modified: 2026-08-18
 * Description: AVX-512 job kernels, with their job cycles, family and ladder cross-checks, arena seeding, completion poll and comparison.
 * To Do: 1) Add /// API documentation with @param tags to the four kernels and four job cycles defined here (GCS d1)
 * Dependencies: typedefs.h, CPU_build.h, CPU_job_cycles.h
 * ISA: Scalar | AVX-512
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */

#include "typedefs.h"
#include "CPU_build.h"

// This unit must be compiled with /arch:AVX512
#if !defined(__AVX512F__)
   #error "CPU_jobs_AVX512.cpp must be compiled with /arch:AVX512. See CPU.vcxproj and ISSUES.MD H3."
#endif

#include "CPU_job_cycles.h"

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

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
/// @brief  Compare a 512-bit (AVX-512) result against its golden value
/// @param  result:   Value produced by the job kernel
/// @param  expected: Reference value loaded from "cpu.values"
/// @return true if every bit of both operands is identical
static inline cbool ResultsMatch(cfl64x8 result, cfl64x8 expected) {
   csi512 delta = _mm512_xor_si512(_mm512_castpd_si512(result), _mm512_castpd_si512(expected));

   return !_mm512_test_epi64_mask(delta, delta);
}

//--- Thread completion bitmap ---//
bool ThreadsRunningAVX512(void) {
   cui64ptrc bits = (cui64ptrc)ThreadBitsView();

   return !AllFalse((si512&)bits[0], max512);
}
//--- Thread completion bitmap ---//

//--- Job kernel cross-check ---//
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
