// Phase-2 standalone demo: play a full multi-scene IntroSpec (the built-in demo)
// through the shared player — starfield, plasma, copper bars, rotozoom, tunnel,
// with the sine scroller riding on top, synced to the procedural music clock.
// This is the exact code path the baked binary uses.
#include "platform.h"
#include "introspec.h"
#include "surface.h"
#include "tables.h"
#include "synth.h"
#include <cmath>

namespace {
  ti::Surface g_surf;
  ti::IntroSpec g_spec;

  struct DemoApp : plat::App {
    DemoApp() {
      tt::init_tables();
      g_spec = ti::IntroSpec::demo();
    }
    void frame(double time, unsigned int* argb) override {
      // Loop the intro so the window keeps showing something.
      double total_seconds = g_spec.total_beats() * 60.0 / g_spec.bpm;
      double t = total_seconds > 0 ? std::fmod(time, total_seconds) : time;
      ti::play(g_spec, g_surf, t);
      g_surf.expand(argb);
    }
  };
}

int main() {
  static ti::Synth synth;
  plat::audio_start(&synth);
  DemoApp app;
  return plat::run(app, "TinyIntro — Phase 2 demo", 3);
}
