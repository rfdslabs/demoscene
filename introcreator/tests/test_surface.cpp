#include "test.h"
#include "surface.h"

static void test_surface_basics() {
  static ti::Surface s;
  s.clear(0);
  s.set_pal(5, 255, 128, 0);
  s.put(10, 10, 5);
  CHECK_EQ((int)s.get(10, 10), 5);
  CHECK_EQ((int)s.get(-1, 0), 0);       // OOB read -> 0
  s.put(9999, 9999, 7);                  // OOB write ignored (no crash)
  static unsigned int out[ti::W * ti::H];
  s.expand(out);
  CHECK_EQ(out[10 * ti::W + 10], 0xFFFF8000u);
  CHECK_EQ(out[0], s.pal[0]);
}

static void test_palette_cycle() {
  static ti::Surface s;
  for (int i = 0; i < 4; i++) s.set_pal(i, (unsigned char)i, 0, 0);
  // pal: [0,1,2,3] in red channel. Rotate forward by 1 => old[3] lands in [0].
  s.cycle_pal(0, 4, 1);
  CHECK_EQ(s.pal[0] & 0x00FF0000u, 0x00030000u);
  CHECK_EQ(s.pal[1] & 0x00FF0000u, 0x00000000u);
}

void register_surface_tests() {
  RUN_TEST(test_surface_basics);
  RUN_TEST(test_palette_cycle);
}
