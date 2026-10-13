#include "registry.h"
#include "surface.h"
#include "rng.h"

// STARFIELD — parallax stars streaming past, the oldest 3D trick in the book.
//
// The secret: give each star a fixed random position and a depth z. Project it
// to the screen by dividing x and y by z (perspective), then march z toward the
// viewer over time; when it passes, recycle the star to the far plane. Nearer
// stars move faster and are brighter — instant depth from one divide per star.
namespace {

  const int NSTARS = 220;

  struct Star { float x, y, z; };
  static Star g_stars[NSTARS];
  static bool g_init = false;

  void init_stars() {
    ti::Rng rng(12345);
    for (int i = 0; i < NSTARS; ++i) {
      g_stars[i].x = (rng.f01() * 2.0f - 1.0f) * 2.0f;
      g_stars[i].y = (rng.f01() * 2.0f - 1.0f) * 2.0f;
      g_stars[i].z = rng.f01() * 2.0f + 0.05f;
    }
    g_init = true;
  }

  void render(ti::Surface& s, const ti::Ctx& c, const ti::Params& p) {
    if (!g_init) init_stars();
    for (int i = 0; i < 16; ++i)
      s.set_pal(i, (unsigned char)(i * 17), (unsigned char)(i * 17), (unsigned char)(i * 17));
    s.clear(0);

    float speed = (p.p[0] != 0 ? p.p[0] : 0.6f);
    // Deterministic march from absolute time (recompute z, no accumulation).
    for (int i = 0; i < NSTARS; ++i) {
      float z = g_stars[i].z - (float)c.time * speed;
      // wrap depth into (0, 2.05]
      float span = 2.0f;
      while (z <= 0.05f) z += span;
      int sx = (int)(g_stars[i].x / z * (ti::W / 2)) + ti::W / 2;
      int sy = (int)(g_stars[i].y / z * (ti::W / 2)) + ti::H / 2;
      if ((unsigned)sx < (unsigned)ti::W && (unsigned)sy < (unsigned)ti::H) {
        int bright = (int)((2.05f - z) / 2.0f * 15.0f);
        if (bright < 1) bright = 1; if (bright > 15) bright = 15;
        s.idx[sy * ti::W + sx] = (unsigned char)bright;
      }
    }
  }

  ti::EffectInfo starfield_effect = {
    "starfield", "Starfield",
    "STARFIELD\n"
    "Each star has a fixed random position and a depth z. Project with x/z, y/z\n"
    "for perspective, and march z toward the camera over time; recycle stars that\n"
    "pass by. Nearer stars (small z) move faster and shine brighter. One divide\n"
    "per star buys a convincing sense of 3D depth.",
    {"speed", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, 0, 0, 0, 0},
    &render,
  };
  TI_REGISTER_EFFECT(starfield_effect);
}
