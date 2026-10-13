#pragma once
// The uniform effect contract. Every visual effect is a self-contained unit that
// exposes exactly this. The panel links all effects for live preview; the bake
// step compiles only the chosen ones into a minimal binary. Same code in both.
namespace ti {
  struct Surface;

  // Fixed-size, named float parameters plus an optional text payload (scroller).
  struct Params {
    float p[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const char* text = nullptr;
  };

  // Per-frame context. `time` is seconds from the master (audio) clock.
  struct Ctx {
    double time = 0.0;
    double beat = 0.0;
    int    frame = 0;
  };

  struct EffectInfo {
    const char* id;                 // stable identifier, e.g. "plasma"
    const char* name;               // human label for the panel
    const char* secret;             // multi-line explanation of the technique
    const char* param_names[8];     // nullptr where unused
    float       param_defaults[8];
    void (*render)(Surface&, const Ctx&, const Params&);
  };
}
