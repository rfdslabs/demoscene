#include "surface.h"
#include <cstring>

namespace ti {

  void Surface::clear(unsigned char c) {
    std::memset(idx, c, sizeof(idx));
  }

  void Surface::put(int x, int y, unsigned char c) {
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return;
    idx[y * W + x] = c;
  }

  unsigned char Surface::get(int x, int y) const {
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return 0;
    return idx[y * W + x];
  }

  void Surface::expand(unsigned int* out) const {
    for (int i = 0; i < W * H; ++i) out[i] = pal[idx[i]];
  }

  void Surface::set_pal(int i, unsigned char r, unsigned char g, unsigned char b) {
    pal[i & 255] = 0xFF000000u | ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b;
  }

  // Rotate `count` palette entries starting at `first` by `by` steps. A positive
  // `by` shifts contents forward so entry i shows what was at i-by (classic cycle).
  void Surface::cycle_pal(int first, int count, int by) {
    if (count <= 1) return;
    unsigned int tmp[256];
    for (int i = 0; i < count; ++i) tmp[i] = pal[first + i];
    for (int i = 0; i < count; ++i) {
      int src = ((i - by) % count + count) % count;
      pal[first + i] = tmp[src];
    }
  }
}
