#pragma once
// Thin C++-facing platform interface. Everything Mac-specific (Cocoa, CoreAudio)
// lives behind this so the engine and effects stay portable, testable C++.
namespace ti { struct Surface; struct Synth; }

namespace plat {

  // Your app implements frame() to fill a W*H ARGB (0xAARRGGBB) buffer each vsync.
  struct App {
    virtual void frame(double time, unsigned int* argb) = 0;
    virtual void key(int keycode, bool down) { (void)keycode; (void)down; }
    // Mouse in LOGICAL pixel coords (0..W-1, 0..H-1), origin top-left.
    virtual void mouse(int x, int y, bool down) { (void)x; (void)y; (void)down; }
    virtual ~App() {}
  };

  // Open a window, run the render+audio loop until quit. Returns process exit code.
  int run(App& app, const char* title, int scale);

  // Master clock in seconds. Backed by the audio sample position once audio is
  // started; falls back to wall-clock before then.
  double audio_time();

  // Start CoreAudio output pulling from `synth`.
  void audio_start(ti::Synth* synth);

  // Master-clock state (defined in mac_window.mm, updated by the audio callback).
  extern unsigned long long g_audio_samples;
  extern double             g_audio_sr;

  // Common macOS key codes we care about.
  enum { KEY_ESC = 53, KEY_SPACE = 49, KEY_LEFT = 123, KEY_RIGHT = 124,
         KEY_UP = 126, KEY_DOWN = 125, KEY_RETURN = 36 };
}
