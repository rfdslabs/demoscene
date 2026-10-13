#pragma once
// IntroSpec: the editable description of a whole intro. This is what the panel
// mutates, what a .tro file stores, and what the bake step turns into source.
// It is plain host C++ (STL is fine here — only the *baked binary* must stay
// dependency-free; codegen emits plain arrays, never std::string).
#include <string>
#include <vector>

namespace ti {
  struct Surface;

  struct SpecScene {
    std::string effect_id;
    float p[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    double start_beat = 0.0;
    double length_beats = 8.0;
  };

  struct IntroSpec {
    std::string title = "Untitled";
    double bpm = 125.0;
    std::string scroller_text = "TINYINTRO PRESENTS ... ";
    std::vector<SpecScene> scenes;

    double total_beats() const;

    std::string to_string() const;                       // .tro text
    static IntroSpec from_string(const std::string& s);
    bool save(const char* path) const;
    static IntroSpec load(const char* path);             // empty spec if missing

    static IntroSpec demo();                             // a built-in example
  };

  // Shared player: render the intro at absolute `time` seconds into `s`.
  // Used identically by the panel preview and by the baked standalone binary.
  void play(const IntroSpec& spec, Surface& s, double time);
}
