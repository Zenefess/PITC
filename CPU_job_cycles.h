/************************************************************
 * File: CPU_job_cycles.h               Created: 2025/02/17 *
 *                                    Last mod.: 2025/02/20 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

//-- Bit-exact result comparison --//
// A job's output must be bit-identical to its golden value, so every exponent and mantissa bit has to be
// examined. _mm_testc_si128 is a subset test -- a bit that should be 0 turning 1 satisfies it -- and
// _mm256_testc_pd inspects only the sign bit of each lane. Neither is an equality test; XOR-then-testz is.
//
// _mm512_mask_cmpneq_pd_mask, which the AVX-512 cycles used, is not one either: it is a *floating-point*
// predicate, so it answers a question about numeric values where every other unit asks about bit patterns.
// +0.0 and -0.0 satisfy it as equal, hiding a sign-bit flip in a zero lane, and two NaNs of identical
// encoding satisfy it as unequal, so a fault that produced one in both planes would be reported where a bit
// compare would find nothing to report. All five units now agree on what "identical" means: the three
// overloads below XOR the operands and test the difference against zero, over the integer domain, and the
// ALU and FPU cycles compare raw scalars with != (ISSUES.MD A11)

/// @brief  Compare a 128-bit (SSE) result against its golden value
/// @param  result:   Value produced by the job kernel
/// @param  expected: Reference value loaded from "cpu.values"
/// @return true if every bit of both operands is identical
static inline cbool ResultsMatch(cfl64x2 result, cfl64x2 expected) {
   csi128 delta = _mm_xor_si128(_mm_castpd_si128(result), _mm_castpd_si128(expected));

   return _mm_testz_si128(delta, delta);
}

/// @brief  Compare a 256-bit (AVX2) result against its golden value
/// @param  result:   Value produced by the job kernel
/// @param  expected: Reference value loaded from "cpu.values"
/// @return true if every bit of both operands is identical
static inline cbool ResultsMatch(cfl64x4 result, cfl64x4 expected) {
   csi256 delta = _mm256_xor_si256(_mm256_castpd_si256(result), _mm256_castpd_si256(expected));

   return _mm256_testz_si256(delta, delta);
}

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

static cui8 JobCycleALU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].alu = value[0][coreNum].alu;
   JobALU(value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemALU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p4[offset]     = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 1] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 2] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 3] = value[0][coreNum].alu;
   JobMemALU(&value[1][coreNum].p4[offset]);
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

static cui8 JobCycleFPU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].fpu = value[0][coreNum].fpu;
   JobFPU(value[1][coreNum].fpu);
   if(value[1][coreNum].fpu != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].fpu;
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemFPU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p3[offset]     = value[0][coreNum].fpu;
   value[1][coreNum].p3[offset + 1] = value[0][coreNum].fpu;
   value[1][coreNum].p3[offset + 2] = value[0][coreNum].fpu;
   value[1][coreNum].p3[offset + 3] = value[0][coreNum].fpu;
   JobMemFPU(&value[1][coreNum].p3[offset]);
   // Failed's unit argument selects the format the mismatch is printed in, and must name the unit whose
   // value[3] member was just written -- 3 (FPU) here, as in JobCycleFPU and JobCycleMemALU_FPU. These four
   // passed 4 (ALU), so Failed's case 4 printed value[2].alu and value[3].alu with "%lld": two integers from
   // lanes this function never touches, in place of the two doubles that disagreed (ISSUES.MD A12)
   if(value[1][coreNum].p3[offset] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset];
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   if(value[1][coreNum].p3[offset + 1] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset + 1];
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   if(value[1][coreNum].p3[offset + 2] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset + 2];
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   if(value[1][coreNum].p3[offset + 3] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset + 3];
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   return 0;
}

static cui8 JobCycleSSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].sse = value[0][coreNum].sse;
   JobSSE(value[1][coreNum].sse);
   if(!ResultsMatch(value[1][coreNum].sse, value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].sse;
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemSSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
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

static cui8 JobCycleAVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx = value[0][coreNum].avx;
   JobAVX2(value[1][coreNum].avx);
   if(!ResultsMatch(value[1][coreNum].avx, value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].avx;
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemAVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
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

static cui8 JobCycleAVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx512 = value[0][coreNum].avx512;
   JobAVX512(value[1][coreNum].avx512);
   if(!ResultsMatch(value[1][coreNum].avx512, value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].avx512;
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemAVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
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

static cui8 JobCycleALU_FPU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].fpu = value[0][coreNum].fpu;
   value[1][coreNum].alu = value[0][coreNum].alu;
   JobALU_FPU(value[1][coreNum].fpu, value[1][coreNum].alu);
   if(value[1][coreNum].alu != value[2][coreNum].alu) {
      value[3][coreNum].alu = value[1][coreNum].alu;
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(value[1][coreNum].fpu != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].fpu;
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   return 0;
}

static cui8 JobCycleMemALU_FPU(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].p3[offset]     = value[0][coreNum].fpu;
   value[1][coreNum].p3[offset + 1] = value[0][coreNum].fpu;
   value[1][coreNum].p3[offset + 2] = value[0][coreNum].fpu;
   value[1][coreNum].p3[offset + 3] = value[0][coreNum].fpu;
   value[1][coreNum].p4[offset]     = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 1] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 2] = value[0][coreNum].alu;
   value[1][coreNum].p4[offset + 3] = value[0][coreNum].alu;
   JobMemALU_FPU(&value[1][coreNum].p3[offset], &value[1][coreNum].p4[offset]);
   if(value[1][coreNum].p3[offset] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset];
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   if(value[1][coreNum].p3[offset + 1] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset + 1];
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   if(value[1][coreNum].p3[offset + 2] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset + 2];
      Failed(coreNum, threadByte, 3);
      return 1;
   }
   if(value[1][coreNum].p3[offset + 3] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset + 3];
      Failed(coreNum, threadByte, 3);
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

static cui8 JobCycleALU_SSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
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

static cui8 JobCycleMemALU_SSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
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

static cui8 JobCycleALU_AVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
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

static cui8 JobCycleMemALU_AVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
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

static cui8 JobCycleALU_AVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
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

static cui8 JobCycleMemALU_AVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
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

// Job cycle functions array. [0][]==Without memory, [1][]==With memory
// ComputationPulse indexes this with (procUnits & 0x1F), so every one of the 32 combinations must carry an
// entry: a shorter table is read past its end and calls through whatever follows it. Only the ALU bit and
// the widest selected unit alter the dispatch, which makes 24~31 (AVX2 with AVX-512) AVX-512 entries and
// matches the arena-sizing switch in wmain, whose own cases already span the whole 0~31 domain
al64 static cui8 (*JobCycle[2][32])(cui64 coreNum, csi64 offset, vchptrc threadByte) = {
 { JobCycleALU,       JobCycleALU,           JobCycleFPU,       JobCycleALU_FPU,       JobCycleSSE,       JobCycleALU_SSE,       JobCycleSSE,       JobCycleALU_SSE,
   JobCycleAVX2,      JobCycleALU_AVX2,      JobCycleAVX2,      JobCycleALU_AVX2,      JobCycleAVX2,      JobCycleALU_AVX2,      JobCycleAVX2,      JobCycleALU_AVX2,
   JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,
   JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512, },
 { JobCycleMemALU,    JobCycleMemALU,        JobCycleMemFPU,    JobCycleMemALU_FPU,    JobCycleMemSSE,    JobCycleMemALU_SSE,    JobCycleMemSSE,    JobCycleMemALU_SSE,
   JobCycleMemAVX2,   JobCycleMemALU_AVX2,   JobCycleMemAVX2,   JobCycleMemALU_AVX2,   JobCycleMemAVX2,   JobCycleMemALU_AVX2,   JobCycleMemAVX2,   JobCycleMemALU_AVX2,
   JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512,
   JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512 }
};
