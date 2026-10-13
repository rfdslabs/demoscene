#include "registry.h"
#include "surface.h"
#include "tables.h"

// COPPER BARS — the horizontal colour bars sliding vertically, named after the
// Amiga's "copper" co-processor that could change colour registers per scanline.
//
// The secret: pick a few bar centres that bob up and down on a sine, then for
// every scanline compute its distance to each centre and shade it brightest at
// the middle fading to the edges. It is a pure per-row effect: cheap, and the
// glossy metallic look comes entirely from the vertical brightness ramp.
namespace {

  const int NBARS = 4;

  void render(ti::Surface& s, const ti::Ctx& c, const ti::Params& p) {
    // Palette: index 0 = black background; 1..64 = a shaded ramp per bar hue.
    s.set_pal(0, 0, 0, 0);
    int t = (int)(c.time * 220.0);
    int height = 26 + (int)p.p[0];    // bar half-thickness in px

    // Bar base hues (R,G,B) — classic red/green/blue/yellow set.
    static const unsigned char hue[NBARS][3] = {
      {255, 40, 40}, {40, 255, 60}, {60, 120, 255}, {255, 210, 40}
    };
    // Build a 4x16 shaded ramp: pal[1 + bar*16 + level].
    for (int b = 0; b < NBARS; ++b)
      for (int level = 0; level < 16; ++level) {
        int m = level * level * 255 / (15 * 15);  // quadratic gloss
        s.set_pal(1 + b * 16 + level,
                  (unsigned char)(hue[b][0] * m / 255),
                  (unsigned char)(hue[b][1] * m / 255),
                  (unsigned char)(hue[b][2] * m / 255));
      }

    for (int y = 0; y < ti::H; ++y) {
      unsigned char best = 0;
      for (int b = 0; b < NBARS; ++b) {
        int centre = ti::H / 2 + tt::isin(t + b * (tt::SIN_SIZE / NBARS)) * (ti::H / 2 - height) / tt::SIN_ONE;
        int d = y - centre; if (d < 0) d = -d;
        if (d < height) {
          int level = (height - d) * 15 / height;   // 0..15 bright in centre
          unsigned char col = (unsigned char)(1 + b * 16 + level);
          if (col > best) best = col;                // front bar wins
        }
      }
      unsigned char* row = &s.idx[y * ti::W];
      for (int x = 0; x < ti::W; ++x) row[x] = best;
    }
    (void)p;
  }

  ti::EffectInfo copperbars_effect = {
    "copperbars", "Copper Bars",
    "COPPER BARS\n"
    "Named after the Amiga copper that rewrote colour registers per scanline.\n"
    "Each bar centre bobs on a sine; for every row we take the distance to the\n"
    "nearest bar and shade brightest in the middle. A quadratic brightness ramp\n"
    "gives the glossy metallic sheen. Entirely a per-row effect.",
    {"thickness", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, 0, 0, 0, 0},
    &render,
  };
  TI_REGISTER_EFFECT(copperbars_effect);
}
