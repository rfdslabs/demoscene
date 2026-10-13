#include "test.h"
#include "font.h"
#include "surface.h"

static void test_font_metrics() {
  CHECK_EQ(ti::font::text_width("ABC"), 24);
  CHECK_EQ(ti::font::text_width(""), 0);
  CHECK_EQ(ti::font::GW, 8);
}

static void test_glyph_pixels() {
  // 'A' must have some set pixels.
  int set = 0;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
      if (ti::font::glyph_pixel('A', x, y)) set++;
  CHECK(set > 4);
  // space is blank
  int sp = 0;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
      if (ti::font::glyph_pixel(' ', x, y)) sp++;
  CHECK_EQ(sp, 0);
  // lowercase maps to uppercase
  CHECK_EQ(ti::font::glyph_pixel('a', 3, 3), ti::font::glyph_pixel('A', 3, 3));
}

static void test_draw() {
  static ti::Surface s;
  s.clear(0);
  ti::font::draw(s, 0, 0, "A", 7);
  int set = 0;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
      if (s.get(x, y) == 7) set++;
  CHECK(set > 4);
}

void register_font_tests() {
  RUN_TEST(test_font_metrics);
  RUN_TEST(test_glyph_pixels);
  RUN_TEST(test_draw);
}
