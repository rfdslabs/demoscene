#include "tables.h"
#include <cmath>

namespace tt {
  static int  s_sin[SIN_SIZE];
  static bool s_ready = false;

  void init_tables() {
    if (s_ready) return;
    for (int i = 0; i < SIN_SIZE; ++i)
      s_sin[i] = (int)std::lround(std::sin((double)i / SIN_SIZE * 2.0 * M_PI) * SIN_ONE);
    s_ready = true;
  }

  int isin(int idx) { return s_sin[idx & SIN_MASK]; }
  int icos(int idx) { return s_sin[(idx + SIN_SIZE / 4) & SIN_MASK]; }

  float fsin(float r) {
    const float TAU = 6.28318530717958647692f;
    int idx = (int)(r / TAU * SIN_SIZE);
    return isin(idx) / (float)SIN_ONE;
  }
}
