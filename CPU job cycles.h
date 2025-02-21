/************************************************************
 * File: CPU.h                          Created: 2025/02/17 *
 *                                    Last mod.: 2025/02/20 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

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
   if(value[1][coreNum].p3[offset] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset];
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(value[1][coreNum].p3[offset + 1] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset + 1];
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(value[1][coreNum].p3[offset + 2] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset + 2];
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   if(value[1][coreNum].p3[offset + 3] != value[2][coreNum].fpu) {
      value[3][coreNum].fpu = value[1][coreNum].p3[offset + 3];
      Failed(coreNum, threadByte, 4);
      return 1;
   }
   return 0;
}

static cui8 JobCycleSSE(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].sse = value[0][coreNum].sse;
   JobSSE(value[1][coreNum].sse);
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
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
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[offset], (__m128i&)value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[offset + 1], (__m128i&)value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 1];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[offset + 2], (__m128i&)value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 2];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[offset + 3], (__m128i&)value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 3];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   return 0;
}

static cui8 JobCycleAVX2(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx = value[0][coreNum].avx;
   JobAVX2(value[1][coreNum].avx);
   if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
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
   if(!_mm256_testc_pd(value[1][coreNum].p1[offset], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!_mm256_testc_pd(value[1][coreNum].p1[offset + 1], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 1];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!_mm256_testc_pd(value[1][coreNum].p1[offset + 2], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 2];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!_mm256_testc_pd(value[1][coreNum].p1[offset + 3], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 3];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   return 0;
}

static cui8 JobCycleAVX512(cui64 coreNum, csi64 offset, vchptrc threadByte) {
   value[1][coreNum].avx512 = value[0][coreNum].avx512;
   JobAVX512(value[1][coreNum].avx512);
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
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
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].p0[offset], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].p0[offset + 1], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 1];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].p0[offset + 2], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 2];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].p0[offset + 3], value[2][coreNum].avx512)) {
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
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].sse, (__m128i&)value[2][coreNum].sse)) {
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
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[offset], (__m128i&)value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[offset + 1], (__m128i&)value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 1];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[offset + 2], (__m128i&)value[2][coreNum].sse)) {
      value[3][coreNum].sse = value[1][coreNum].p2[offset + 2];
      Failed(coreNum, threadByte, 2);
      return 1;
   }
   if(!_mm_testc_si128((__m128i&)value[1][coreNum].p2[offset + 3], (__m128i&)value[2][coreNum].sse)) {
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
   if(!_mm256_testc_pd(value[1][coreNum].avx, value[2][coreNum].avx)) {
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
   if(!_mm256_testc_pd(value[1][coreNum].p1[offset], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!_mm256_testc_pd(value[1][coreNum].p1[offset + 1], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 1];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!_mm256_testc_pd(value[1][coreNum].p1[offset + 2], value[2][coreNum].avx)) {
      value[3][coreNum].avx = value[1][coreNum].p1[offset + 2];
      Failed(coreNum, threadByte, 1);
      return 1;
   }
   if(!_mm256_testc_pd(value[1][coreNum].p1[offset + 3], value[2][coreNum].avx)) {
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
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].avx512, value[2][coreNum].avx512)) {
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
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].p0[offset], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].p0[offset + 1], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 1];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].p0[offset + 2], value[2][coreNum].avx512)) {
      value[3][coreNum].avx512 = value[1][coreNum].p0[offset + 2];
      Failed(coreNum, threadByte, 0);
      return 1;
   }
   if(_mm512_mask_cmpneq_pd_mask(0x0FF, value[1][coreNum].p0[offset + 3], value[2][coreNum].avx512)) {
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
al64 static cui8 (*JobCycle[2][24])(cui64 coreNum, csi64 offset, vchptrc threadByte) = {
 { JobCycleALU,       JobCycleALU,           JobCycleFPU,       JobCycleALU_FPU,       JobCycleSSE,       JobCycleALU_SSE,       JobCycleSSE,       JobCycleALU_SSE,
   JobCycleAVX2,      JobCycleALU_AVX2,      JobCycleAVX2,      JobCycleALU_AVX2,      JobCycleAVX2,      JobCycleALU_AVX2,      JobCycleAVX2,      JobCycleALU_AVX2,
   JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512,    JobCycleAVX512,    JobCycleALU_AVX512, },
 { JobCycleMemALU,    JobCycleMemALU,        JobCycleMemFPU,    JobCycleMemALU_FPU,    JobCycleMemSSE,    JobCycleMemALU_SSE,    JobCycleMemSSE,    JobCycleMemALU_SSE,
   JobCycleMemAVX2,   JobCycleMemALU_AVX2,   JobCycleMemAVX2,   JobCycleMemALU_AVX2,   JobCycleMemAVX2,   JobCycleMemALU_AVX2,   JobCycleMemAVX2,   JobCycleMemALU_AVX2,
   JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512, JobCycleMemAVX512, JobCycleMemALU_AVX512 }
};
