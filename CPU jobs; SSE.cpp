/************************************************************
 * File: CPU jobs; SSE.cpp              Created: 2025/01/23 *
 *                                    Last mod.: 2025/02/17 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/

#include <typedefs.h>

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

#ifndef _mm_abs_pd
#define _mm_abs_pd(input) _mm_and_pd((fl64x2&)_mm_set1_epi64x(0x07FFFFFFFFFFFFFFF), (input))
#endif

// SIMD SSE operations only
void JobSSE(fl64x2 &x) {
   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))),  _mm_set1_pd(0.0001)));
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))),  _mm_set1_pd(0.01)));
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      )
      x = _mm_mul_pd(x, _mm_add_pd(_mm_mul_pd(x, _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
   }
}

// ALU + SIMD SSE operations only
void JobALU_SSE(fl64x2& x, si64& y) {
   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      y *= 791ull / 14 + 502; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 9 - 294941;
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x, _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      y *= 787ull / 11 + 500; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 5 - 294937;
      )
      x = _mm_mul_pd(x, _mm_add_pd(_mm_mul_pd(x, _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
   }
}

void JobMemSSE(fl64x2ptrc x) {
   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      )
      x[0] = _mm_mul_pd(x[0], _mm_add_pd(_mm_mul_pd(x[0], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[2] = _mm_mul_pd(x[2], _mm_add_pd(_mm_mul_pd(x[2], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[1] = _mm_mul_pd(x[1], _mm_add_pd(_mm_mul_pd(x[1], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[3] = _mm_mul_pd(x[3], _mm_add_pd(_mm_mul_pd(x[3], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
   }
}

void JobMemALU_SSE(fl64x2ptrc x, si64ptrc y) {
   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      y[0] *= 789ull / 13 + 501; y[0] = ((i < 32 ? y[0] << 1 : y[0] >> 1) ^ -1) / 7 - 294939;
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      y[2] *= 789ull / 13 + 501; y[2] = ((i < 32 ? y[2] << 1 : y[2] >> 1) ^ -1) / 7 - 294939;
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      y[1] *= 789ull / 13 + 501; y[1] = ((i < 32 ? y[1] << 1 : y[1] >> 1) ^ -1) / 7 - 294939;
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.12)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.0001)));
      y[3] *= 789ull / 13 + 501; y[3] = ((i < 32 ? y[3] << 1 : y[3] >> 1) ^ -1) / 7 - 294939;
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      y[0] *= 791ull / 14 + 502; y[0] = ((i < 32 ? y[0] << 1 : y[0] >> 1) ^ -1) / 9 - 294941;
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      y[2] *= 791ull / 14 + 502; y[2] = ((i < 32 ? y[2] << 1 : y[2] >> 1) ^ -1) / 9 - 294941;
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      y[1] *= 791ull / 14 + 502; y[1] = ((i < 32 ? y[1] << 1 : y[1] >> 1) ^ -1) / 9 - 294941;
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.91)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.011)))))), _mm_set1_pd(0.001)));
      y[3] *= 791ull / 14 + 502; y[3] = ((i < 32 ? y[3] << 1 : y[3] >> 1) ^ -1) / 9 - 294941;
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      y[0] *= 789ull / 13 + 501; y[0] = ((i < 32 ? y[0] << 1 : y[0] >> 1) ^ -1) / 7 - 294939;
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      y[2] *= 789ull / 13 + 501; y[2] = ((i < 32 ? y[2] << 1 : y[2] >> 1) ^ -1) / 7 - 294939;
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      y[1] *= 789ull / 13 + 501; y[1] = ((i < 32 ? y[1] << 1 : y[1] >> 1) ^ -1) / 7 - 294939;
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(1.15)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.01)))))), _mm_set1_pd(0.01)));
      y[3] *= 789ull / 13 + 501; y[3] = ((i < 32 ? y[3] << 1 : y[3] >> 1) ^ -1) / 7 - 294939;
      x[0] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[0], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      y[0] *= 787ull / 11 + 500; y[0] = ((i < 32 ? y[0] << 1 : y[0] >> 1) ^ -1) / 5 - 294937;
      x[2] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[2], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      y[2] *= 787ull / 11 + 500; y[2] = ((i < 32 ? y[2] << 1 : y[2] >> 1) ^ -1) / 5 - 294937;
      x[1] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[1], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      y[1] *= 787ull / 11 + 500; y[1] = ((i < 32 ? y[1] << 1 : y[1] >> 1) ^ -1) / 5 - 294937;
      x[3] = _mm_div_pd(_mm_sqrt_pd(_mm_set1_pd(0.85)), _mm_add_pd(_mm_abs_pd(_mm_sub_pd(_mm_set1_pd(1.0), _mm_sqrt_pd(_mm_sqrt_pd(_mm_div_pd(x[3], _mm_set1_pd(2.009)))))), _mm_set1_pd(0.1)));
      y[3] *= 787ull / 11 + 500; y[3] = ((i < 32 ? y[3] << 1 : y[3] >> 1) ^ -1) / 5 - 294937;
      )
      x[0] = _mm_mul_pd(x[0], _mm_add_pd(_mm_mul_pd(x[0], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[2] = _mm_mul_pd(x[2], _mm_add_pd(_mm_mul_pd(x[2], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[1] = _mm_mul_pd(x[1], _mm_add_pd(_mm_mul_pd(x[1], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
      x[3] = _mm_mul_pd(x[3], _mm_add_pd(_mm_mul_pd(x[3], _mm_set1_pd(1.01010101010101)), _mm_set1_pd(0.00021)));
   }
}
