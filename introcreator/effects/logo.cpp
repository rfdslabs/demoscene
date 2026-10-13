#include "registry.h"
#include "surface.h"
#include "tables.h"
#include "font.h"
#include <cstring>

// LOGO — a big chunky pixel logo bouncing on a sine, with a drop shadow and a
// colour that cycles. The staple centrepiece of any cracktro.
//
// The secret: reuse the 8x8 pixel font but blow every glyph pixel up into an
// SxS block, so one tiny font renders a giant logo with zero extra assets.
// Bounce is a single sin-table lookup added to the y origin; the glow is just
// the palette entry animating over time.
namespace {

  void render(ti::Surface& s, const ti::Ctx& c, const ti::Params& p) {
    s.set_pal(0, 8, 4, 20);
    s.clear(0);

    const char* txt = (p.text && p.text[0]) ? p.text : "TINYINTRO";
    int S = 3 + (int)p.p[0];                 // pixel block size
    if (S < 2) S = 2; if (S > 6) S = 6;
    int len = (int)std::strlen(txt);
    int tw = len * 8 * S;
    int x0 = (ti::W - tw) / 2;

    int bounce = tt::isin((int)(c.time * 180.0)) * 26 / tt::SIN_ONE;
    int y0 = 70 + bounce;

    // Cycling logo colour + a fixed dark shadow.
    int hue = (int)(c.time * 60.0);
    int r = (tt::isin(hue) + tt::SIN_ONE) * 255 / (2 * tt::SIN_ONE);
    int g = (tt::isin(hue + tt::SIN_SIZE / 3) + tt::SIN_ONE) * 255 / (2 * tt::SIN_ONE);
    int b = (tt::isin(hue + 2 * tt::SIN_SIZE / 3) + tt::SIN_ONE) * 255 / (2 * tt::SIN_ONE);
    s.set_pal(200, (unsigned char)r, (unsigned char)g, (unsigned char)b);
    s.set_pal(199, 20, 10, 40);

    auto blit = [&](int ox, int oy, unsigned char col) {
      for (int ci = 0; txt[ci]; ++ci)
        for (int gy = 0; gy < 8; ++gy)
          for (int gx = 0; gx < 8; ++gx)
            if (ti::font::glyph_pixel(txt[ci], gx, gy)) {
              int bx = ox + (ci * 8 + gx) * S, by = oy + gy * S;
              for (int yy = 0; yy < S; ++yy)
                for (int xx = 0; xx < S; ++xx) s.put(bx + xx, by + yy, col);
            }
    };
    blit(x0 + S, y0 + S, 199);   // shadow
    blit(x0, y0, 200);           // logo
  }

  ti::EffectInfo logo_effect = {
    "logo", "Logo",
    "LOGO\n"
    "Reuse the 8x8 pixel font but expand every glyph pixel into an SxS block, so a\n"
    "tiny font draws a giant logo with no extra assets. The bounce is one sin-table\n"
    "lookup added to the y origin; the glow is the palette entry cycling over time.\n"
    "A drop shadow is the same text drawn once, offset, in a dark colour.",
    {"size", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, 0, 0, 0, 0},
    &render,
  };
  TI_REGISTER_EFFECT(logo_effect);
}
