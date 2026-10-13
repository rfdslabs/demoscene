#include "test.h"
#include "timeline.h"
#include <string>

static void test_beat_mapping() {
  ti::Timeline tl;
  tl.bpm = 120.0;                 // 2 beats per second
  CHECK_NEAR(tl.beat_at(0.0), 0.0, 1e-9);
  CHECK_NEAR(tl.beat_at(1.0), 2.0, 1e-9);
  CHECK_NEAR(tl.beat_at(2.5), 5.0, 1e-9);
}

static void test_scene_lookup() {
  ti::Timeline tl;
  tl.bpm = 120.0;
  tl.add("logo", 0.0, 4.0);       // beats [0,4)
  tl.add("plasma", 4.0, 8.0);     // beats [4,12)
  tl.add("tunnel", 12.0, 4.0);    // beats [12,16)
  CHECK_EQ(tl.count, 3);
  // At t=0 -> beat 0 -> logo
  const ti::Scene* s0 = tl.scene_at(0.0);
  CHECK(s0 && std::string(s0->effect_id) == "logo");
  // t=2.5s -> beat 5 -> plasma
  const ti::Scene* s1 = tl.scene_at(2.5);
  CHECK(s1 && std::string(s1->effect_id) == "plasma");
  // t=7s -> beat 14 -> tunnel
  const ti::Scene* s2 = tl.scene_at(7.0);
  CHECK(s2 && std::string(s2->effect_id) == "tunnel");
  // Past the end -> nullptr
  CHECK(tl.scene_at(100.0) == nullptr);
  // total length
  CHECK_NEAR(tl.total_beats(), 16.0, 1e-9);
}

static void test_local_time() {
  ti::Timeline tl;
  tl.bpm = 120.0;
  tl.add("plasma", 4.0, 8.0);
  // At beat 6 (t=3s), local beat within scene = 2
  CHECK_NEAR(tl.local_beat_at(3.0), 2.0, 1e-9);
}

void register_timeline_tests() {
  RUN_TEST(test_beat_mapping);
  RUN_TEST(test_scene_lookup);
  RUN_TEST(test_local_time);
}
