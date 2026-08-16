/************************************************************
 * File: CPU_job_cycles.h               Created: 2025/02/17 *
 *                                    Last mod.: 2026/08/15 *
 *                                                          *
 * Desc: Job cycle declarations and the dispatch table.     *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

#include "CPU.h"

//-- One job cycle, and where each of them lives --//
// A job cycle seeds the working values from value[0], runs the kernel, compares the result against value[2]
// and, on a mismatch, records value[3] and calls Failed. Each of the eighteen is defined in the translation
// unit of the kernel it wraps -- CPU_jobs_standard.cpp, CPU_jobs_SSE.cpp, CPU_jobs_AVX.cpp,
// CPU_jobs_AVX512.cpp -- because the comparison and the value moves either side of it are instructions of
// that unit's own width.
//
// They used to be defined here, and this header is included by CPU.cpp, which is compiled at the
// StreamingSIMDExtensions2 baseline in every configuration. So the file that holds the option parser and the
// results table also held _mm512_xor_si512, _mm512_test_epi64_mask and a 512-bit move per AVX-512 job cycle:
// MSVC accepts an intrinsic whatever /arch is set to, so nothing failed, but the compiler was allocating
// registers and placing vzeroupper for code it had been told the target could not execute, and the isolation
// CLAUDE.md describes -- "the global StreamingSIMDExtensions2 setting keeps AVX-512 opcodes out of the
// baseline code path" -- was true of the kernels only (ISSUES.MD H4). Reaching value[][] and Failed from
// another translation unit is what H9 had to be fixed for
//
//   JobCycle<UNIT>(coreNum, offset, threadByte)      Register-resident: one record
//   JobCycleMem<UNIT>(coreNum, offset, threadByte)   Memory-backed: records offset ~ offset+3
//
// Failed's third argument must name the unit whose value[3] member the wrapper has just written -- 0=AVX-512,
// 1=AVX2, 2=SSE, 3=FPU, 4=ALU -- because that argument selects both the format the mismatch is printed in and
// the lanes it is read from (ISSUES.MD A12)

//-- Bit-exact result comparison --//
// A job's output must be bit-identical to its golden value, so every exponent and mantissa bit has to be
// examined. _mm_testc_si128 is a subset test -- a bit that should be 0 turning 1 satisfies it -- and
// _mm256_testc_pd inspects only the sign bit of each lane. Neither is an equality test; XOR-then-testz is.
//
// _mm512_mask_cmpneq_pd_mask, which the AVX-512 cycles used, is not one either: it is a *floating-point*
// predicate, so it answers a question about numeric values where every other unit asks about bit patterns.
// +0.0 and -0.0 satisfy it as equal, hiding a sign-bit flip in a zero lane, and two NaNs of identical
// encoding satisfy it as unequal, so a fault that produced one in both planes would be reported where a bit
// compare would find nothing to report. All five units now agree on what "identical" means: the three vector
// ResultsMatch overloads -- one per width, each defined beside the job cycles that call it -- XOR the
// operands and test the difference against zero, the scalar overload in CPU_jobs_standard.cpp byte-compares
// the two fl64, and the ALU cycles compare si64 with !=, which already examines every bit.
// Every one of the five asks its question in the integer domain. The FPU cycles were the last to be brought
// to it: they compared fl64 with !=, the same numeric predicate, in the same two directions (ISSUES.MD A11,
// A2)

extern cui8 JobCycleALU          (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleFPU          (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleALU_FPU      (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemALU       (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemFPU       (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemALU_FPU   (cui64 coreNum, csi64 offset, vchptrc threadByte);

extern cui8 JobCycleSSE          (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleALU_SSE      (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemSSE       (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemALU_SSE   (cui64 coreNum, csi64 offset, vchptrc threadByte);

extern cui8 JobCycleAVX2         (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleALU_AVX2     (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemAVX2      (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemALU_AVX2  (cui64 coreNum, csi64 offset, vchptrc threadByte);

extern cui8 JobCycleAVX512       (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleALU_AVX512   (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemAVX512    (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemALU_AVX512(cui64 coreNum, csi64 offset, vchptrc threadByte);

// Job cycle functions array. [0][]==Without memory, [1][]==With memory
// ComputationPulse indexes this with (procUnits & 0x1F), so every one of the 32 combinations must carry an
// entry: a shorter table is read past its end and calls through whatever follows it. Only the ALU bit and
// the widest selected unit alter the dispatch, which makes 24~31 (AVX2 with AVX-512) AVX-512 entries and
// matches the arena-sizing switch in wmain, whose own cases already span the whole 0~31 domain.
// Defined in CPU.cpp, with every other object this program holds at namespace scope (ISSUES.MD H9)
al64 extern cui8 (*JobCycle[2][32])(cui64 coreNum, csi64 offset, vchptrc threadByte);
