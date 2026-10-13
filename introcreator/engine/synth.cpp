#include "synth.h"
#include <cmath>

namespace ti {

  float Synth::osc_sine(double phase) {
    return std::sin(phase * 6.28318530717958647692);
  }

  namespace {
    double note_hz(int n) { return 440.0 * std::pow(2.0, (n - 69) / 12.0); }
    float square(double phase) { return std::fmod(phase, 1.0) < 0.5 ? 1.0f : -1.0f; }

    // Deterministic white noise from an absolute sample index (so the drum kit
    // renders identically every run and in the baked binary).
    float hash_noise(unsigned long long n) {
      unsigned x = (unsigned)(n * 2654435761u) + 12345u;
      x ^= x >> 13; x *= 1274126177u; x ^= x >> 16;
      return (x / 2147483648.0f) - 1.0f;
    }

    // 16-step patterns (0 = rest). Two-bar feel via the |15 index wrap.
    const int BASS[16] = { 36,0,36,0, 43,0,36,0, 41,0,41,0, 38,0,45,0 };
    const int LEAD[16] = { 60,64,67,72, 71,67,64,60, 65,69,72,77, 76,72,69,65 };
    const int KICK[16] = { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,1,0 };
    const int SNAR[16] = { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 };
    const int HAT [16] = { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,1,1,1 };
  }

  void Synth::render(float* out, int frames) {
    const double bpm = 125.0;
    const double step_len = 60.0 / bpm / 4.0;    // 16th note in seconds

    for (int i = 0; i < frames; ++i) {
      unsigned long long n = sample_pos + i;
      double t = (double)n / sr;
      int    step = (int)(t / step_len);
      double in_step = t - step * step_len;        // seconds into current step
      double env = std::exp(-in_step * 8.0);       // plucky decay for tonal voices

      int bn = BASS[step & 15];
      int ln = LEAD[step & 15];

      float bass = bn ? (float)(square(t * note_hz(bn)) * 0.16 * (0.4 + 0.6 * env)) : 0.0f;
      float lead = ln ? (float)(osc_sine(t * note_hz(ln)) * 0.14 * env) : 0.0f;
      float pad  = (float)(osc_sine(t * note_hz(bn ? bn + 12 : 48)) * 0.04);

      // --- drum kit ---
      float drums = 0.0f;
      if (KICK[step & 15]) {
        double e = std::exp(-in_step * 26.0);
        double f = 40.0 + 90.0 * std::exp(-in_step * 60.0);   // pitch drop
        drums += (float)(osc_sine(t * f) * 0.5 * e);
      }
      if (SNAR[step & 15]) {
        double e = std::exp(-in_step * 22.0);
        drums += (float)((hash_noise(n) * 0.35 + osc_sine(t * 180.0) * 0.15) * e);
      }
      if (HAT[step & 15]) {
        double e = std::exp(-in_step * 90.0);                 // very short
        drums += (float)(hash_noise(n * 7u + 3u) * 0.18 * e);
      }

      // --- one-shot UI blip (never triggered during tests) ---
      float sfx = 0.0f;
      if (sfx_kind && n >= sfx_start) {
        double st = (double)(n - sfx_start) / sr;
        if (st < 0.09) {
          double e = std::exp(-st * 40.0);
          double f = sfx_kind == 2 ? (st < 0.045 ? 900.0 : 1400.0) : 1300.0;
          sfx = (float)(square(t * f) * 0.10 * e);
        }
      }

      float s = bass + lead + pad + drums + sfx;
      if (s > 1.0f) s = 1.0f;
      if (s < -1.0f) s = -1.0f;
      out[i * 2 + 0] = s;
      out[i * 2 + 1] = s;
    }
    sample_pos += (unsigned long long)frames;
  }
}
