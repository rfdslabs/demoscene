#include "test.h"
#include "bake.h"
#include "introspec.h"
#include <string>

static bool contains(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

static void test_codegen_only_chosen() {
  ti::IntroSpec s;
  s.title = "Bake Test";
  s.bpm = 120;
  s.scroller_text = "GENERATED CRACKTRO";
  ti::SpecScene a; a.effect_id = "plasma"; a.start_beat = 0;  a.length_beats = 8;
  ti::SpecScene b; b.effect_id = "tunnel"; b.start_beat = 8;  b.length_beats = 8;
  s.scenes = {a, b};

  std::string src = ti::generate_main_source(s);
  CHECK(contains(src, "\"plasma\""));
  CHECK(contains(src, "\"tunnel\""));
  CHECK(contains(src, "GENERATED CRACKTRO"));       // embedded text
  CHECK(contains(src, "int main"));
  CHECK(contains(src, "plat::run"));
  // Not-chosen effects must not appear in the generated scene table.
  CHECK(!contains(src, "\"copperbars\""));
  CHECK(!contains(src, "\"starfield\""));
  // The technique explanation is emitted as comments.
  CHECK(contains(src, "PLASMA"));
  CHECK(contains(src, "TUNNEL"));
}

void register_bake_tests() {
  RUN_TEST(test_codegen_only_chosen);
}
