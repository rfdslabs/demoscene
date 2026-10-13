#include "test.h"
#include "registry.h"
#include "surface.h"
#include "tables.h"
#include <cstring>

static bool non_uniform(const ti::Surface& s) {
  unsigned char first = s.idx[0];
  for (int i = 1; i < ti::W * ti::H; ++i)
    if (s.idx[i] != first) return true;
  return false;
}

static void test_plasma_registered_and_draws() {
  tt::init_tables();
  const ti::EffectInfo* e = ti::effects().find("plasma");
  CHECK(e != nullptr);
  if (!e) return;
  static ti::Surface s;
  s.clear(0);
  ti::Ctx c; c.time = 1.5; c.frame = 90;
  ti::Params p;
  e->render(s, c, p);
  CHECK(non_uniform(s));
}

static void test_scroller_registered_and_draws() {
  tt::init_tables();
  const ti::EffectInfo* e = ti::effects().find("scroller");
  CHECK(e != nullptr);
  if (!e) return;
  static ti::Surface s;
  s.clear(0);
  ti::Ctx c; c.time = 0.0; c.frame = 0;
  ti::Params p; p.text = "HELLO WORLD"; p.p[0] = 100; // y position
  e->render(s, c, p);
  // With text at time 0 and offset starting on-screen, some pixels should be set.
  int set = 0;
  for (int i = 0; i < ti::W * ti::H; ++i) if (s.idx[i] != 0) set++;
  CHECK(set > 0);
}

static void test_all_effects_draw() {
  tt::init_tables();
  const char* ids[] = {"copperbars", "starfield", "rotozoom", "tunnel", "vector3d", "logo"};
  for (const char* id : ids) {
    const ti::EffectInfo* e = ti::effects().find(id);
    CHECK(e != nullptr);
    if (!e) continue;
    static ti::Surface s;
    s.clear(0);
    ti::Ctx c; c.time = 2.0; c.frame = 120;
    ti::Params p;
    e->render(s, c, p);
    CHECK(non_uniform(s));                 // produced a picture
    CHECK(e->secret && e->secret[0]);      // ships an explanation
  }
}

void register_effects_tests() {
  RUN_TEST(test_plasma_registered_and_draws);
  RUN_TEST(test_scroller_registered_and_draws);
  RUN_TEST(test_all_effects_draw);
}
