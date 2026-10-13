#include "test.h"
#include "introspec.h"
#include "surface.h"
#include "tables.h"
#include <string>

static void test_roundtrip() {
  ti::IntroSpec a;
  a.title = "Cracktro One";
  a.bpm = 130.0;
  a.scroller_text = "HELLO WORLD GREETZ TO ALL";
  ti::SpecScene s1; s1.effect_id = "plasma";   s1.start_beat = 0;  s1.length_beats = 16;
  s1.p[0] = 3; s1.p[1] = 5;
  ti::SpecScene s2; s2.effect_id = "scroller"; s2.start_beat = 16; s2.length_beats = 16;
  s2.p[0] = 170; s2.p[3] = 100;
  a.scenes = {s1, s2};

  std::string txt = a.to_string();
  ti::IntroSpec b = ti::IntroSpec::from_string(txt);

  CHECK(b.title == a.title);
  CHECK_NEAR(b.bpm, a.bpm, 1e-9);
  CHECK(b.scroller_text == a.scroller_text);
  CHECK_EQ((int)b.scenes.size(), 2);
  CHECK(b.scenes[0].effect_id == "plasma");
  CHECK_NEAR(b.scenes[0].p[0], 3.0, 1e-6);
  CHECK_NEAR(b.scenes[0].length_beats, 16.0, 1e-9);
  CHECK(b.scenes[1].effect_id == "scroller");
  CHECK_NEAR(b.scenes[1].p[3], 100.0, 1e-6);
}

static void test_total_beats() {
  ti::IntroSpec a = ti::IntroSpec::demo();
  CHECK(a.total_beats() > 0.0);
  CHECK(a.scenes.size() >= 3);
}

static void test_player_renders() {
  tt::init_tables();
  ti::IntroSpec a = ti::IntroSpec::demo();
  static ti::Surface s;
  s.clear(0);
  // Render at a time well inside the timeline; expect a non-empty picture.
  ti::play(a, s, 1.0);
  int set = 0;
  for (int i = 0; i < ti::W * ti::H; ++i) if (s.idx[i] != 0) set++;
  CHECK(set > 0);
}

void register_introspec_tests() {
  RUN_TEST(test_roundtrip);
  RUN_TEST(test_total_beats);
  RUN_TEST(test_player_renders);
}
