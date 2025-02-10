/************************************************************
 * File: CPU jobs; AVX512.cpp           Created: 2025/01/23 *
 *                                    Last mod.: 2025/02/10 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/

#include <typedefs.h>

// SIMD AVX512 operations only
void JobAVX512(fl64x8 &x) {
   for(ui8 i = 0; i < 16; ++i) {
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      x = _mm512_mul_pd(x, _mm512_add_pd(_mm512_mul_pd(x, _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
   }
}

// SIMD AVX512 operations only
void JobALU_AVX512(fl64x8 &x, si64& y) {
   for(ui8 i = 0; i < 16; ++i) {
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      y *= 791ull / 14 + 502; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 9 - 294941;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      y *= 787ull / 11 + 500; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 5 - 294937;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      y *= 791ull / 14 + 502; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 9 - 294941;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      y *= 787ull / 11 + 500; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 5 - 294937;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      y *= 791ull / 14 + 502; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 9 - 294941;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      y *= 787ull / 11 + 500; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 5 - 294937;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.12)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.0001)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.91)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.011)))))), _mm512_set1_pd(0.001)));
      y *= 791ull / 14 + 502; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 9 - 294941;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(1.15)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.01)))))), _mm512_set1_pd(0.01)));
      y *= 789ull / 13 + 501; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 7 - 294939;
      x = _mm512_div_pd(_mm512_sqrt_pd(_mm512_set1_pd(0.85)), _mm512_add_pd(_mm512_abs_pd(_mm512_sub_pd(_mm512_set1_pd(1.0),
             _mm512_sqrt_pd(_mm512_sqrt_pd(_mm512_div_pd(x, _mm512_set1_pd(2.009)))))), _mm512_set1_pd(0.1)));
      y *= 787ull / 11 + 500; y = ((i < 32 ? y << 1 : y >> 1) ^ -1) / 5 - 294937;
      x = _mm512_mul_pd(x, _mm512_add_pd(_mm512_mul_pd(x, _mm512_set1_pd(1.01010101010101)), _mm512_set1_pd(0.00021)));
   }
}
