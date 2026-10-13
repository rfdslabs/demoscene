#include "timeline.h"

namespace ti {

  void Timeline::add(const char* effect_id, double start_beat, double length_beats) {
    if (count >= 32) return;
    scenes[count++] = Scene{effect_id, start_beat, length_beats};
  }

  double Timeline::total_beats() const {
    double end = 0.0;
    for (int i = 0; i < count; ++i) {
      double e = scenes[i].start_beat + scenes[i].length_beats;
      if (e > end) end = e;
    }
    return end;
  }

  const Scene* Timeline::scene_at(double seconds) const {
    double b = beat_at(seconds);
    for (int i = 0; i < count; ++i) {
      double s = scenes[i].start_beat;
      double e = s + scenes[i].length_beats;
      if (b >= s && b < e) return &scenes[i];
    }
    return nullptr;
  }

  double Timeline::local_beat_at(double seconds) const {
    const Scene* s = scene_at(seconds);
    if (!s) return 0.0;
    return beat_at(seconds) - s->start_beat;
  }
}
