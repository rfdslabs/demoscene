#include "registry.h"
#include "surface.h"
#include "tables.h"
#include <cmath>

// TUNNEL — flying down an endless textured pipe.
//
// The secret: for every screen pixel precompute two things that never change —
// the angle around the centre and a value proportional to 1/distance (depth).
// Each frame just add scrolling offsets to those precomputed angle/depth tables
// and look up a texture. All the trigonometry and the divide happen ONCE at
// startup; per frame it is two adds and a lookup.
namespace {

  static bool g_ready = false;
  static unsigned char g_angle[ti::W * ti::H];
  static unsigned char g_depth[ti::W * ti::H];

  void build_tables() {
    for (int y = 0; y < ti::H; ++y)
      for (int x = 0; x < ti::W; ++x) {
        float dx = x - ti::W / 2.0f;
        float dy = y - ti::H / 2.0f;
        float dist = std::sqrt(dx * dx + dy * dy) + 0.0001f;
        float ang = std::atan2(dy, dx);
        g_angle[y * ti::W + x] = (unsigned char)((ang / 6.2831853f + 0.5f) * 255.0f);
        g_depth[y * ti::W + x] = (unsigned char)((int)(3000.0f / dist) & 0xFF);
      }
    g_ready = true;
  }

  unsigned char tex(int u, int v) {   // xor pattern, 256x256 wrap
    return (unsigned char)(((u & 0x1F) ^ (v & 0x1F)) * 8);
  }

  void render(ti::Surface& s, const ti::Ctx& c, const ti::Params& p) {
    if (!g_ready) build_tables();
    for (int i = 0; i < 256; ++i)
      s.set_pal(i, (unsigned char)i, (unsigned char)(i / 2), (unsigned char)(255 - i));

    int move = (int)(c.time * 160.0);      // travel speed
    int spin = (int)(c.time * 40.0 * (p.p[0] != 0 ? p.p[0] : 1.0f)); // rotation
    for (int i = 0; i < ti::W * ti::H; ++i)
      s.idx[i] = tex(g_angle[i] + spin, g_depth[i] + move);
  }

  ti::EffectInfo tunnel_effect = {
    "tunnel", "Tunnel",
    "TUNNEL\n"
    "For each pixel precompute its angle around the centre and a value ~1/distance.\n"
    "All the atan2 and the divide happen once at startup. Per frame you just add a\n"
    "scroll offset to the depth (moving forward) and to the angle (spinning) and\n"
    "index a texture. The illusion of an infinite pipe from two table lookups.",
    {"spin", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, 0, 0, 0, 0},
    &render,
  };
  TI_REGISTER_EFFECT(tunnel_effect);
}
