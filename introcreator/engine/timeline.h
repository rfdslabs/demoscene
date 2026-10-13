#pragma once
// Timeline: maps the master clock (seconds) to musical beats and to scenes.
// Scene positions are expressed in beats so everything stays locked to the
// music regardless of tempo. This is the backbone the panel edits and the bake
// step emits.
namespace ti {
  struct Scene {
    const char* effect_id;
    double start_beat;
    double length_beats;
  };

  struct Timeline {
    double bpm = 125.0;
    Scene scenes[32];
    int count = 0;

    void add(const char* effect_id, double start_beat, double length_beats);
    double beat_at(double seconds) const { return seconds * bpm / 60.0; }
    double total_beats() const;

    const Scene* scene_at(double seconds) const;   // nullptr past the end
    double local_beat_at(double seconds) const;    // beats since current scene start
  };
}
