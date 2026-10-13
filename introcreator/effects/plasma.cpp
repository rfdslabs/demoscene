#include "registry.h"
#include "surface.h"
#include "tables.h"

// PLASMA — the definitive old-school effect.
//
// The secret: never compute colour per pixel. Instead, for each pixel we sum a
// few sine waves of the pixel's x and y (and time) using a fixed-point sin
// TABLE, fold the sum into a 0..255 palette index, and let a smooth, animated
// palette do the colouring. Because colour lives in the palette, you can also
// animate the whole thing for free by cycling the palette — no pixels touched.
namespace {

  void build_palette(ti::Surface& s, double time) {
    // A smooth cyclic palette: 3 phase-shifted cosines -> rainbow that drifts.
    int shift = (int)(time * 40.0);
    for (int i = 0; i < 256; ++i) {
      int a = (i + shift) * (tt::SIN_SIZE / 256);
      int r = (tt::isin(a) + tt::SIN_ONE) * 255 / (2 * tt::SIN_ONE);
      int g = (tt::isin(a + tt::SIN_SIZE / 3) + tt::SIN_ONE) * 255 / (2 * tt::SIN_ONE);
      int b = (tt::isin(a + 2 * tt::SIN_SIZE / 3) + tt::SIN_ONE) * 255 / (2 * tt::SIN_ONE);
      s.set_pal(i, (unsigned char)r, (unsigned char)g, (unsigned char)b);
    }
  }

  void render(ti::Surface& s, const ti::Ctx& c, const ti::Params& p) {
    build_palette(s, c.time);
    int t = (int)(c.time * 200.0);
    // p[0]=speed-x scale, p[1]=speed-y scale (defaults give a classic look).
    int sx = 20 + (int)(p.p[0]);
    int sy = 24 + (int)(p.p[1]);
    for (int y = 0; y < ti::H; ++y) {
      int ry = y * (tt::SIN_SIZE / 64);
      int v1 = tt::isin(ry + t * 2);
      for (int x = 0; x < ti::W; ++x) {
        int rx = x * (tt::SIN_SIZE / 64);
        int v = tt::isin(rx * sx / 20 + t)
              + v1
              + tt::isin((rx + ry) / 2 + t * 3)
              + tt::isin(((x - 160) * (x - 160) + (y - 100) * (y - 100)) / 8 - t * 2);
        // v is in ~[-4*ONE, 4*ONE]; fold into 0..255.
        unsigned char idx = (unsigned char)(((v >> 4) & 0xFF));
        s.idx[y * ti::W + x] = idx;
      }
    }
    (void)sy;
  }

  ti::EffectInfo plasma_effect = {
    "plasma", "Plasma",
    "PLASMA\n"
    "Sum several sine waves of x, y and time using a fixed-point sin table,\n"
    "fold the result into a palette index, and colour it with a smooth cyclic\n"
    "palette. Movement is nearly free: cycle the palette and the whole field\n"
    "shimmers without touching a single pixel again.",
    {"speed_x", "speed_y", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, 0, 0, 0, 0},
    &render,
  };
  TI_REGISTER_EFFECT(plasma_effect);
}
