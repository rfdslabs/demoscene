#pragma once
// Tiny deterministic PRNG (xorshift32). Determinism matters: procedural content
// (starfields, noise) must look identical every run and in the baked binary.
namespace ti {
  struct Rng {
    unsigned s;
    explicit Rng(unsigned seed = 2463534242u) : s(seed ? seed : 1u) {}
    unsigned next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f01() { return (next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    int range(int n) { return n > 0 ? (int)(next() % (unsigned)n) : 0; }
  };
}
