/************************************************************
 * File: SIMD management.h              Created: 2025/09/18 *
 *                                Last modified: 2025/09/27 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

#include <immintrin.h>

///== Fused multiply-add, single-rounded where the ISA carries it
// These were five #ifndef-guarded macros over the intrinsics' own names. _mm_fmadd_ps and its family are
// functions declared by <immintrin.h> and are never macros, so no #ifndef over one can ever be false: every
// guard was true and the two-rounding split form was substituted for the intrinsic throughout any unit
// compiled below AVX2/FMA. A caller writing _mm_fmadd_ps and expecting the one rounding an FMA performs was
// given two, silently, under the name of the instruction it had asked for -- and a guard spelt as a test
// implied a detection that cannot be written. They are functions with names of their own instead: a unit
// that cannot execute VFMADD gets the split form because it called simd::fmadd_ps, not because a macro
// rewrote its call, and a unit that can gets the instruction (ISSUES.MD I2)
namespace simd {
   inline fl32x4 __vectorcall fmadd_ps(fl32x4 a, fl32x4 b, fl32x4 c) noexcept {
#if defined(__FMA__) || defined(__AVX2__)
      return _mm_fmadd_ps(a, b, c);
#else
      return _mm_add_ps(_mm_mul_ps(a, b), c);
#endif
   }

   inline fl32x4 __vectorcall fmsub_ps(fl32x4 a, fl32x4 b, fl32x4 c) noexcept {
#if defined(__FMA__) || defined(__AVX2__)
      return _mm_fmsub_ps(a, b, c);
#else
      return _mm_sub_ps(_mm_mul_ps(a, b), c);
#endif
   }

   inline fl32x4 __vectorcall fnmadd_ps(fl32x4 a, fl32x4 b, fl32x4 c) noexcept {
#if defined(__FMA__) || defined(__AVX2__)
      return _mm_fnmadd_ps(a, b, c);
#else
      return _mm_sub_ps(c, _mm_mul_ps(a, b));
#endif
   }

// The 256-bit overloads exist only where 256-bit code does. Their fallbacks are VEX-encoded -- vmulps and
// vaddps -- so defining them for a unit compiled at the SSE2 baseline offers an SSE2 caller a function it
// cannot execute, which is the I1 defect below in another form
#if defined(__AVX__)
   inline fl32x8 __vectorcall fmadd_ps(fl32x8 a, fl32x8 b, fl32x8 c) noexcept {
#if defined(__FMA__) || defined(__AVX2__)
      return _mm256_fmadd_ps(a, b, c);
#else
      return _mm256_add_ps(_mm256_mul_ps(a, b), c);
#endif
   }

   inline fl32x8 __vectorcall fmsub_ps(fl32x8 a, fl32x8 b, fl32x8 c) noexcept {
#if defined(__FMA__) || defined(__AVX2__)
      return _mm256_fmsub_ps(a, b, c);
#else
      return _mm256_sub_ps(_mm256_mul_ps(a, b), c);
#endif
   }

   inline fl32x8 __vectorcall fnmadd_ps(fl32x8 a, fl32x8 b, fl32x8 c) noexcept {
#if defined(__FMA__) || defined(__AVX2__)
      return _mm256_fnmadd_ps(a, b, c);
#else
      return _mm256_sub_ps(c, _mm256_mul_ps(a, b));
#endif
   }
#endif
}
///== Fused multiply-add, single-rounded where the ISA carries it

///== Intrinsics macros
// Everything defined in this block must be executable at the baseline the block itself selects. It is active
// precisely where the wide instruction sets are absent, so an opcode reached from here that the enclosing
// condition has just established the unit does not have is one no CPU on this path can execute
#if !defined(__FMA__) && !defined(__AVX2__)

// SSE2 throughout. This was '(fl64x2&)_mm_and_epi64(...)', and _mm_and_epi64 is AVX-512VL+F: a definition
// offered only to units compiled below AVX2 handed every one of them an EVEX vpandq, which is why the two
// PITC kernel units that use the macro must #undef it before defining their own -- without that, the SSE
// kernels this program dispatches to CPUs carrying SSE and nothing more would each carry an AVX-512 opcode.
// The cast intrinsic also replaces a reference cast that bound a non-const fl64x2& to an rvalue (ISSUES.MD I1)
#define _mm_abs_pd(input)  _mm_and_pd(_mm_castsi128_pd(_mm_set1_epi64x(0x07FFFFFFFFFFFFFFF)), (input))

// The 256-bit form is VEX-encoded, so it is defined only where 256-bit code is legal: /arch:AVX defines
// __AVX__ without __AVX2__ and reaches this block legitimately, whereas an SSE2 unit reaching it was handed
// vandpd and vpbroadcastq -- neither of which such a CPU can execute (ISSUES.MD I1)
#if defined(__AVX__)
#define _mm256_abs_pd(input)  _mm256_and_pd(_mm256_castsi256_pd(_mm256_set1_epi64x(0x07FFFFFFFFFFFFFFF)), (input))
#endif

#endif
///== Intrinsics macros
