#pragma once
// Fixed-point trig lookup tables — the classic demo optimization: never call
// libm sin() in a per-pixel loop; index a precomputed table instead.
namespace tt {
  constexpr int SIN_BITS = 12;
  constexpr int SIN_SIZE = 4096;      // one full turn = 4096 steps
  constexpr int SIN_MASK = SIN_SIZE - 1;
  constexpr int SIN_ONE  = 4096;      // sin==1.0 maps to 4096 (12-bit fixed point)

  void init_tables();                 // idempotent; call once at startup
  int isin(int idx);                  // (idx & SIN_MASK) -> sin*SIN_ONE, range [-4096,4096]
  int icos(int idx);
  float fsin(float radians);          // table-backed float sine
}
