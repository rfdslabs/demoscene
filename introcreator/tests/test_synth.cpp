#include "test.h"
#include "synth.h"

static void test_determinism_and_advance() {
  ti::Synth a, b;
  float bufA[512 * 2] = {0};
  float bufB[512 * 2] = {0};
  a.render(bufA, 512);
  b.render(bufB, 512);
  bool identical = true;
  for (int i = 0; i < 512 * 2; ++i) if (bufA[i] != bufB[i]) identical = false;
  CHECK(identical);
  CHECK_EQ(a.sample_pos, (unsigned long long)512);
}

static void test_range() {
  ti::Synth s;
  float buf[1024 * 2] = {0};
  s.render(buf, 1024);
  bool in_range = true;
  for (int i = 0; i < 1024 * 2; ++i) if (buf[i] < -1.001f || buf[i] > 1.001f) in_range = false;
  CHECK(in_range);
}

static void test_clock_progresses() {
  ti::Synth s;
  float buf[256 * 2] = {0};
  s.render(buf, 256);
  s.render(buf, 256);
  CHECK_EQ(s.sample_pos, (unsigned long long)512);
  CHECK_NEAR(s.seconds(), 512.0 / s.sr, 1e-9);
}

static void test_not_silent() {
  ti::Synth s;
  float buf[4410 * 2] = {0};   // 0.1s
  s.render(buf, 4410);
  float peak = 0;
  for (int i = 0; i < 4410 * 2; ++i) { float a = buf[i] < 0 ? -buf[i] : buf[i]; if (a > peak) peak = a; }
  CHECK(peak > 0.1f);          // the kit/voices actually make sound
}

void register_synth_tests() {
  RUN_TEST(test_determinism_and_advance);
  RUN_TEST(test_range);
  RUN_TEST(test_clock_progresses);
  RUN_TEST(test_not_silent);
}
