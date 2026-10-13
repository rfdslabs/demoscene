#pragma once
// Procedural synth. No audio files — the soundtrack is generated sample by
// sample. The running sample counter is the demo's master clock (visuals sync
// to the music). Voices: bass (square), arp lead (sine), pad, plus a drum kit
// (kick/hihat/snare from a deterministic noise hash). A separate one-shot SFX
// channel lets the panel blip on interaction without disturbing the music or
// the deterministic render used by tests.
namespace ti {
  struct Synth {
    double sr = 44100.0;
    unsigned long long sample_pos = 0;

    // One-shot UI sound effect (set from any thread; benign racy for a blip).
    unsigned long long sfx_start = 0;   // sample_pos when the blip began
    int sfx_kind = 0;                   // 0 = none, 1 = move, 2 = confirm

    // Fill `frames` interleaved stereo samples (2*frames floats) in [-1,1] and
    // advance sample_pos. Deterministic when no SFX is triggered.
    void render(float* out, int frames);

    double seconds() const { return (double)sample_pos / sr; }

    void blip(int kind) { sfx_kind = kind; sfx_start = sample_pos; }

    static float osc_sine(double phase);   // phase in cycles (0..1 = one period)
  };
}
