#include "registry.h"
#include "surface.h"
#include "tables.h"

// ROTOZOOM — an infinite textured plane that spins and zooms under the screen.
//
// The secret: walk screen pixels, but step through *texture* space along two
// rotated basis vectors (du, dv) whose length is the zoom and whose angle is the
// rotation. Because the texture coordinates wrap (& 63), the pattern tiles
// forever. Precomputed sin/cos make the per-pixel cost two adds.
namespace {

  // A procedural 64x64 checker/xor texture -> palette indices.
  unsigned char tex(int u, int v) {
    u &= 63; v &= 63;
    return (unsigned char)(((u ^ v) * 4) & 0xFF);
  }

  void render(ti::Surface& s, const ti::Ctx& c, const ti::Params& p) {
    // Warm palette ramp.
    for (int i = 0; i < 256; ++i) {
      int r = i, g = (i * 3 / 4), b = 255 - i;
      s.set_pal(i, (unsigned char)r, (unsigned char)g, (unsigned char)b);
    }
    int ang = (int)(c.time * 120.0);
    float zoom = 0.5f + 0.4f * (tt::isin((int)(c.time * 90.0)) / (float)tt::SIN_ONE + 1.0f);
    if (p.p[0] != 0) zoom *= p.p[0];

    // Rotated basis vectors in texture space (fixed point via sin table).
    float cosv = tt::isin(ang + tt::SIN_SIZE / 4) / (float)tt::SIN_ONE * zoom;
    float sinv = tt::isin(ang) / (float)tt::SIN_ONE * zoom;

    for (int y = 0; y < ti::H; ++y) {
      float sxr = -(ti::W / 2) * cosv - (y - ti::H / 2) * sinv;
      float syr =  (ti::W / 2) * sinv - (y - ti::H / 2) * cosv;
      // start texture coords for x=0
      float u = sxr;
      float v = syr;
      unsigned char* row = &s.idx[y * ti::W];
      for (int x = 0; x < ti::W; ++x) {
        row[x] = tex((int)u, (int)v);
        u += cosv;
        v -= sinv;
      }
    }
  }

  ti::EffectInfo rotozoom_effect = {
    "rotozoom", "Rotozoom",
    "ROTOZOOM\n"
    "March through texture space along two rotated basis vectors: their angle is\n"
    "the rotation, their length the zoom. Wrapping the texture coords (& 63) tiles\n"
    "the plane infinitely. With sin/cos from a table the inner loop is two adds\n"
    "per pixel — a spinning, breathing floor for almost free.",
    {"zoom", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, 0, 0, 0, 0},
    &render,
  };
  TI_REGISTER_EFFECT(rotozoom_effect);
}
