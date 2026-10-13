#include "registry.h"
#include "surface.h"
#include "tables.h"

// VECTOR 3D — a rotating wireframe object, the classic "vector demo" look.
//
// The secret: keep the model as a handful of 3D points and a list of edges.
// Each frame, rotate every point with sin/cos from the lookup table (two axes),
// then project to 2D by dividing by depth (perspective). Draw the edges with a
// Bresenham line — integer only, no floating point in the inner loop. A whole
// spinning solid is a dozen lines a frame.
namespace {

  struct V3 { float x, y, z; };
  const V3 CUBE[8] = {
    {-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
    {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1},
  };
  const int EDGES[12][2] = {
    {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
  };

  void line(ti::Surface& s, int x0, int y0, int x1, int y1, unsigned char c) {
    int dx = x1 - x0, dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int err = adx - ady;
    for (;;) {
      s.put(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 > -ady) { err -= ady; x0 += sx; }
      if (e2 <  adx) { err += adx; y0 += sy; }
    }
  }

  void render(ti::Surface& s, const ti::Ctx& c, const ti::Params& p) {
    s.set_pal(0, 6, 6, 18);
    s.set_pal(1, 90, 255, 200);       // edge colour
    s.set_pal(2, 40, 120, 110);       // dim edge
    s.clear(0);

    float scale = 1.0f + 0.2f * p.p[0];
    int speed = (int)(60.0 + p.p[1] * 10.0);
    int ax = (int)(c.time * speed);
    int ay = (int)(c.time * speed * 0.7);
    float ca = tt::icos(ax) / 4096.0f, sa = tt::isin(ax) / 4096.0f;
    float cb = tt::icos(ay) / 4096.0f, sb = tt::isin(ay) / 4096.0f;

    int px[8], py[8];
    for (int i = 0; i < 8; ++i) {
      float x = CUBE[i].x, y = CUBE[i].y, z = CUBE[i].z;
      // rotate around X
      float y1 = y * ca - z * sa, z1 = y * sa + z * ca;
      // rotate around Y
      float x2 = x * cb + z1 * sb, z2 = -x * sb + z1 * cb;
      float d = 3.2f, f = 130.0f * scale;
      px[i] = 160 + (int)(x2 * f / (z2 + d));
      py[i] = 100 + (int)(y1 * f / (z2 + d));
    }
    for (int e = 0; e < 12; ++e)
      line(s, px[EDGES[e][0]], py[EDGES[e][0]], px[EDGES[e][1]], py[EDGES[e][1]], 1);
  }

  ti::EffectInfo vector3d_effect = {
    "vector3d", "Vector 3D",
    "VECTOR 3D\n"
    "Store the object as 3D points plus an edge list. Each frame rotate every\n"
    "point with sin/cos from the table on two axes, project by dividing by depth\n"
    "(perspective), and connect the edges with an integer Bresenham line. A whole\n"
    "spinning wireframe costs only a dozen lines per frame.",
    {"scale", "speed", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, 0, 0, 0, 0},
    &render,
  };
  TI_REGISTER_EFFECT(vector3d_effect);
}
