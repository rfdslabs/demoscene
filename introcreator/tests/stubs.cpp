// Weak stubs for test suites not yet implemented. A real suite TU overrides
// its stub at link time (Mach-O strong-over-weak). Remove entries as suites land.
#define STUB(name) __attribute__((weak)) void name() {}
STUB(register_tables_tests)
STUB(register_surface_tests)
STUB(register_font_tests)
STUB(register_effects_tests)
STUB(register_timeline_tests)
STUB(register_synth_tests)
STUB(register_introspec_tests)
STUB(register_bake_tests)
