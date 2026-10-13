#include "test.h"

// Each suite is defined in its own TU. Suites not yet implemented are provided
// as weak stubs in stubs.cpp and get overridden when the real TU is linked.
void register_tables_tests();
void register_surface_tests();
void register_font_tests();
void register_effects_tests();
void register_timeline_tests();
void register_synth_tests();
void register_introspec_tests();
void register_bake_tests();

int main() {
  register_tables_tests();
  register_surface_tests();
  register_font_tests();
  register_effects_tests();
  register_timeline_tests();
  register_synth_tests();
  register_introspec_tests();
  register_bake_tests();
  std::printf("\n%d checks, %d failed\n", g_count(), g_fail());
  return tinyintro_tests_failed() ? 1 : 0;
}
