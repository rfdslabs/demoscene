#include "registry.h"
#include "surface.h"
#include "tables.h"
#include "font.h"

// SINE SCROLLER — the message crawling across the bottom with a wavy bounce.
//
// The secret: we don't move glyph bitmaps around. We walk every screen column,
// map it to an absolute pixel position in the (virtual, infinitely long) text
// ribbon that slides left over time, look up which glyph column that is, and
// draw that 8-pixel-tall slice at a y that is offset by a sine of the column.
// One sin-table lookup per column gives the classic rubber wave.
namespace {

  void render(ti::Surface& s, const ti::Ctx& c, const ti::Params& p) {
    const char* text = p.text ? p.text : "TINYINTRO ";
    int len = 0; while (text[len]) len++;
    if (len == 0) return;

    int base_y   = (int)p.p[0]; if (base_y == 0) base_y = ti::H - 40;
    int amp      = (int)p.p[1]; if (amp == 0) amp = 16;
    unsigned char color = (unsigned char)(p.p[2] ? (int)p.p[2] : 255);
    int speed    = (int)(p.p[3] ? p.p[3] : 90.0f);   // pixels/second

    int ribbon_w = len * ti::font::GW;
    // How far the ribbon has slid to the left, wrapped to its own length so it
    // loops seamlessly.
    int scroll = (int)(c.time * speed);
    int wave_t = (int)(c.time * 260.0);

    for (int x = 0; x < ti::W; ++x) {
      // Absolute column in the looping ribbon for this screen column.
      int abs_col = ((x + scroll) % ribbon_w + ribbon_w) % ribbon_w;
      int char_i  = abs_col / ti::font::GW;
      int gx      = abs_col % ti::font::GW;
      char ch     = text[char_i];

      int wave = tt::isin(x * (tt::SIN_SIZE / 80) + wave_t) * amp / tt::SIN_ONE;
      int y0   = base_y + wave;

      for (int gy = 0; gy < ti::font::GH; ++gy)
        if (ti::font::glyph_pixel(ch, gx, gy))
          s.put(x, y0 + gy, color);
    }
  }

  ti::EffectInfo scroller_effect = {
    "scroller", "Sine Scroller",
    "SINE SCROLLER\n"
    "Instead of moving glyph bitmaps, walk every screen column, map it to a\n"
    "position in a looping text ribbon that slides left over time, and draw the\n"
    "matching 8px-tall glyph slice. Offset each column's y by a sin-table lookup\n"
    "for the classic rubber-band wave. Text is data; motion is one add.",
    {"base_y", "amp", "color", "speed", nullptr, nullptr, nullptr, nullptr},
    {0, 16, 255, 90, 0, 0, 0, 0},
    &render,
  };
  TI_REGISTER_EFFECT(scroller_effect);
}
