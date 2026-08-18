/*
 * File: CPU_job_cycles.h
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-02-17
 * Last Modified: 2026-08-16
 * Description: Declarations of the eighteen job cycles and of the JOB_CYCLE dispatch table, with the rules that table and its callers keep.
 * To Do: 1) Check at startup that no JOB_CYCLE entry is null: the declared extent rules out a short table, a missing initialiser does not
 *        2) Assert that JOB_CYCLE's second extent equals the (procUnits & 0x1F) index domain, so widening that field fails the build
 * Dependencies: CPU.h
 * ISA: Scalar
 * Thread-safety: MT-safe
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
#pragma once

#include "CPU.h"

//-- One job cycle, and where each of them lives --//
// A job cycle seeds the working values from value[0], runs the kernel, compares the result against value[2]
// and, on a mismatch, records value[3] and calls Failed. Each of the eighteen is defined in the translation
// unit of the kernel it wraps -- CPU_jobs_standard.cpp, CPU_jobs_SSE.cpp, CPU_jobs_AVX.cpp,
// CPU_jobs_AVX512.cpp -- because the comparison and the value moves either side of it are instructions of
// that unit's own width.

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

extern cui8 JobCycleAVX          (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleALU_AVX      (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemAVX       (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemALU_AVX   (cui64 coreNum, csi64 offset, vchptrc threadByte);

extern cui8 JobCycleAVX512       (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleALU_AVX512   (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemAVX512    (cui64 coreNum, csi64 offset, vchptrc threadByte);
extern cui8 JobCycleMemALU_AVX512(cui64 coreNum, csi64 offset, vchptrc threadByte);

// Job cycle functions array. [0][]==Without memory, [1][]==With memory
al64 extern cui8 (*JOB_CYCLE[2][32])(cui64 coreNum, csi64 offset, vchptrc threadByte);
