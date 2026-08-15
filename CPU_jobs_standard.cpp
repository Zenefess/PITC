/************************************************************
 * File: CPU_jobs_standard.cpp          Created: 2025/01/21 *
 *                                    Last mod.: 2026/08/15 *
 *                                                          *
 * Desc: Scalar ALU and FPU job kernels.                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/

#include <cmath>
#include "typedefs.h"
#include "CPU_build.h"

// This unit holds the scalar ALU and FPU kernels: the path that must run on any x64 CPU, and the only path
// available when no vector unit is selected. A per-file /arch override here lets the compiler emit
// EVEX-encoded scalar instructions, which fault with an illegal instruction on every CPU without AVX-512 --
// and, below /fp:strict, lets it contract JobFPU's trailing x * (x * k + c) into an FMA that rounds once
// where the source rounds twice, so a "cpu.values" written by one build fails under the other
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
   #error "CPU_jobs_standard.cpp must compile at the SSE2 baseline. See ISSUES.MD H1."
#endif

#ifndef UNLOOPx4
#define UNLOOPx4(code) code code code code
#endif

// ISSUES.MD J1/J2: the shift alternates direction across the loop -- the predicate was 'i < 32' against a
// counter that never exceeds 15, so the right-shift arm was unreachable and an entire instruction class
// went unexercised. The chain is also carried in ui64 rather than si64: it relies on wraparound at every
// step, and only unsigned arithmetic is defined to wrap in every language mode. si64 and ui64 may alias
// one another's storage, so the alias below re-types the value in place without copying it out of memory

// ISSUES.MD J7: 'acc' is the running accumulator each intermediate is folded into. Every step of the value
// chain begins by taking a fourth root, which divides a relative perturbation by four and rounds it away
// before the cancellation further down can amplify it, so a single-ULP fault was absorbed four times in
// five. The accumulator reaches the result without passing through a root, and its own (acc + x) over
// |acc - x| shape is expansive rather than contracting, so a perturbation grows instead of decaying. It
// is folded into the value once, at the end. Measured over 5,000,000 seeds it stays inside 2^11 and never
// reaches zero or infinity, and it adds no instruction the kernels did not already issue

// Non-simultaneous ALU operations only
void JobALU(si64 &x) {
   ui64 &v = (ui64&)x;

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      v *= 791ull / 14 + 502; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 9 - 294941;
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      v *= 787ull / 11 + 500; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 5 - 294937;
      )
   }
}

// Non-simultaneous FPU operations only
void JobFPU(fl64 &x) {
   fl64 acc = 1.0;

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x / 2.01)))  + 0.0001);   acc = (acc + x) / (abs(acc - x) + 0.01);
      x = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x / 2.011))) + 0.001);    acc = (acc + x) / (abs(acc - x) + 0.01);
      x = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x / 2.01)))  + 0.01);     acc = (acc + x) / (abs(acc - x) + 0.01);
      x = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x / 2.009))) + 0.1);      acc = (acc + x) / (abs(acc - x) + 0.01);
      )
      x *= x * 1.01010101010101 + 0.00021;
   }
   x *= acc;
}

// ALU + FPU operations only
void JobALU_FPU(fl64 &x, si64 &y) {
   ui64 &v   = (ui64&)y;
   fl64  acc = 1.0;

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x / 2.01)))  + 0.0001);   acc = (acc + x) / (abs(acc - x) + 0.01);
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      x = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x / 2.011))) + 0.001);    acc = (acc + x) / (abs(acc - x) + 0.01);
      v *= 791ull / 14 + 502; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 9 - 294941;
      x = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x / 2.01)))  + 0.01);     acc = (acc + x) / (abs(acc - x) + 0.01);
      v *= 789ull / 13 + 501; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 7 - 294939;
      x = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x / 2.009))) + 0.1);      acc = (acc + x) / (abs(acc - x) + 0.01);
      v *= 787ull / 11 + 500; v = ((i < 8 ? v << 1 : v >> 1) ^ ~0ull) / 5 - 294937;
      )
      x *= x * 1.01010101010101 + 0.00021;
   }
   x *= acc;
}

// Memory-loaded non-simultaneous ALU operations only
void JobMemALU(si64ptrc x) {
   ui64ptrc v = (ui64ptr)x;

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      v[0] *= 789ull / 13 + 501;   v[2] *= 789ull / 13 + 501;   v[1] *= 789ull / 13 + 501;   v[3] *= 789ull / 13 + 501;
      v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;   v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;   v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      v[0] *= 791ull / 14 + 502;   v[2] *= 791ull / 14 + 502;   v[1] *= 791ull / 14 + 502;   v[3] *= 791ull / 14 + 502;
      v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 9 - 294941;   v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 9 - 294941;
      v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 9 - 294941;   v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 9 - 294941;
      v[0] *= 789ull / 13 + 501;   v[2] *= 789ull / 13 + 501;   v[1] *= 789ull / 13 + 501;   v[3] *= 789ull / 13 + 501;
      v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;   v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;   v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      v[0] *= 787ull / 11 + 500;   v[2] *= 787ull / 11 + 500;   v[1] *= 787ull / 11 + 500;   v[3] *= 787ull / 11 + 500;
      v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 5 - 294937;   v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 5 - 294937;
      v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 5 - 294937;   v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 5 - 294937;
      )
   }
}

// Memory-loaded non-simultaneous FPU operations only
void JobMemFPU(fl64ptrc x) {
   fl64 acc[4] = { 1.0, 1.0, 1.0, 1.0 };

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x[0] / 2.01)))  + 0.0001);   acc[0] = (acc[0] + x[0]) / (abs(acc[0] - x[0]) + 0.01);
      x[2] = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x[2] / 2.01)))  + 0.0001);   acc[2] = (acc[2] + x[2]) / (abs(acc[2] - x[2]) + 0.01);
      x[1] = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x[1] / 2.01)))  + 0.0001);   acc[1] = (acc[1] + x[1]) / (abs(acc[1] - x[1]) + 0.01);
      x[3] = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x[3] / 2.01)))  + 0.0001);   acc[3] = (acc[3] + x[3]) / (abs(acc[3] - x[3]) + 0.01);
      x[0] = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x[0] / 2.011))) + 0.001);    acc[0] = (acc[0] + x[0]) / (abs(acc[0] - x[0]) + 0.01);
      x[2] = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x[2] / 2.011))) + 0.001);    acc[2] = (acc[2] + x[2]) / (abs(acc[2] - x[2]) + 0.01);
      x[1] = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x[1] / 2.011))) + 0.001);    acc[1] = (acc[1] + x[1]) / (abs(acc[1] - x[1]) + 0.01);
      x[3] = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x[3] / 2.011))) + 0.001);    acc[3] = (acc[3] + x[3]) / (abs(acc[3] - x[3]) + 0.01);
      x[0] = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x[0] / 2.01)))  + 0.01);     acc[0] = (acc[0] + x[0]) / (abs(acc[0] - x[0]) + 0.01);
      x[2] = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x[2] / 2.01)))  + 0.01);     acc[2] = (acc[2] + x[2]) / (abs(acc[2] - x[2]) + 0.01);
      x[1] = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x[1] / 2.01)))  + 0.01);     acc[1] = (acc[1] + x[1]) / (abs(acc[1] - x[1]) + 0.01);
      x[3] = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x[3] / 2.01)))  + 0.01);     acc[3] = (acc[3] + x[3]) / (abs(acc[3] - x[3]) + 0.01);
      x[0] = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x[0] / 2.009))) + 0.1);      acc[0] = (acc[0] + x[0]) / (abs(acc[0] - x[0]) + 0.01);
      x[2] = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x[2] / 2.009))) + 0.1);      acc[2] = (acc[2] + x[2]) / (abs(acc[2] - x[2]) + 0.01);
      x[1] = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x[1] / 2.009))) + 0.1);      acc[1] = (acc[1] + x[1]) / (abs(acc[1] - x[1]) + 0.01);
      x[3] = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x[3] / 2.009))) + 0.1);      acc[3] = (acc[3] + x[3]) / (abs(acc[3] - x[3]) + 0.01);
      )
      x[0] *= x[0] * 1.01010101010101 + 0.00021;   x[2] *= x[2] * 1.01010101010101 + 0.00021;
      x[1] *= x[1] * 1.01010101010101 + 0.00021;   x[3] *= x[3] * 1.01010101010101 + 0.00021;
   }
   x[0] *= acc[0];   x[2] *= acc[2];   x[1] *= acc[1];   x[3] *= acc[3];
}

// Memory-loaded ALU + FPU operations only
void JobMemALU_FPU(fl64ptrc x, si64ptrc y) {
   ui64ptrc v      = (ui64ptr)y;
   fl64     acc[4] = { 1.0, 1.0, 1.0, 1.0 };

   for(ui8 i = 0; i < 16; ++i) {
      UNLOOPx4(
      x[0] = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x[0] / 2.01)))  + 0.0001);   acc[0] = (acc[0] + x[0]) / (abs(acc[0] - x[0]) + 0.01);
      v[0] *= 789ull / 13 + 501; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;
      x[2] = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x[2] / 2.01)))  + 0.0001);   acc[2] = (acc[2] + x[2]) / (abs(acc[2] - x[2]) + 0.01);
      v[2] *= 789ull / 13 + 501; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      x[1] = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x[1] / 2.01)))  + 0.0001);   acc[1] = (acc[1] + x[1]) / (abs(acc[1] - x[1]) + 0.01);
      v[1] *= 789ull / 13 + 501; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;
      x[3] = sqrt(1.12) / (abs(1.0 - sqrt(sqrt(x[3] / 2.01)))  + 0.0001);   acc[3] = (acc[3] + x[3]) / (abs(acc[3] - x[3]) + 0.01);
      v[3] *= 789ull / 13 + 501; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      x[0] = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x[0] / 2.011))) + 0.001);    acc[0] = (acc[0] + x[0]) / (abs(acc[0] - x[0]) + 0.01);
      v[0] *= 791ull / 14 + 502; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 9 - 294941;
      x[2] = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x[2] / 2.011))) + 0.001);    acc[2] = (acc[2] + x[2]) / (abs(acc[2] - x[2]) + 0.01);
      v[2] *= 791ull / 14 + 502; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 9 - 294941;
      x[1] = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x[1] / 2.011))) + 0.001);    acc[1] = (acc[1] + x[1]) / (abs(acc[1] - x[1]) + 0.01);
      v[1] *= 791ull / 14 + 502; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 9 - 294941;
      x[3] = sqrt(0.91) / (abs(1.0 - sqrt(sqrt(x[3] / 2.011))) + 0.001);    acc[3] = (acc[3] + x[3]) / (abs(acc[3] - x[3]) + 0.01);
      v[3] *= 791ull / 14 + 502; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 9 - 294941;
      x[0] = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x[0] / 2.01)))  + 0.01);     acc[0] = (acc[0] + x[0]) / (abs(acc[0] - x[0]) + 0.01);
      v[0] *= 789ull / 13 + 501; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 7 - 294939;
      x[2] = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x[2] / 2.01)))  + 0.01);     acc[2] = (acc[2] + x[2]) / (abs(acc[2] - x[2]) + 0.01);
      v[2] *= 789ull / 13 + 501; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 7 - 294939;
      x[1] = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x[1] / 2.01)))  + 0.01);     acc[1] = (acc[1] + x[1]) / (abs(acc[1] - x[1]) + 0.01);
      v[1] *= 789ull / 13 + 501; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 7 - 294939;
      x[3] = sqrt(1.15) / (abs(1.0 - sqrt(sqrt(x[3] / 2.01)))  + 0.01);     acc[3] = (acc[3] + x[3]) / (abs(acc[3] - x[3]) + 0.01);
      v[3] *= 789ull / 13 + 501; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 7 - 294939;
      x[0] = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x[0] / 2.009))) + 0.1);      acc[0] = (acc[0] + x[0]) / (abs(acc[0] - x[0]) + 0.01);
      v[0] *= 787ull / 11 + 500; v[0] = ((i < 8 ? v[0] << 1 : v[0] >> 1) ^ ~0ull) / 5 - 294937;
      x[2] = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x[2] / 2.009))) + 0.1);      acc[2] = (acc[2] + x[2]) / (abs(acc[2] - x[2]) + 0.01);
      v[2] *= 787ull / 11 + 500; v[2] = ((i < 8 ? v[2] << 1 : v[2] >> 1) ^ ~0ull) / 5 - 294937;
      x[1] = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x[1] / 2.009))) + 0.1);      acc[1] = (acc[1] + x[1]) / (abs(acc[1] - x[1]) + 0.01);
      v[1] *= 787ull / 11 + 500; v[1] = ((i < 8 ? v[1] << 1 : v[1] >> 1) ^ ~0ull) / 5 - 294937;
      x[3] = sqrt(0.85) / (abs(1.0 - sqrt(sqrt(x[3] / 2.009))) + 0.1);      acc[3] = (acc[3] + x[3]) / (abs(acc[3] - x[3]) + 0.01);
      v[3] *= 787ull / 11 + 500; v[3] = ((i < 8 ? v[3] << 1 : v[3] >> 1) ^ ~0ull) / 5 - 294937;
      )
      x[0] *= x[0] * 1.01010101010101 + 0.00021;   x[2] *= x[2] * 1.01010101010101 + 0.00021;
      x[1] *= x[1] * 1.01010101010101 + 0.00021;   x[3] *= x[3] * 1.01010101010101 + 0.00021;
   }
   x[0] *= acc[0];   x[2] *= acc[2];   x[1] *= acc[1];   x[3] *= acc[3];
}
