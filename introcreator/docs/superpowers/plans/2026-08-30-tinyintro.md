# TinyIntro Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build TinyIntro — a macOS cracktro creator whose retro pixel panel and the tiny native intros it bakes both run on one shared CPU software-rendering engine.

**Architecture:** A portable C++ engine (8-bit indexed `Surface`, lookup tables, pixel font, timeline, effect registry, procedural synth) sits under a thin Objective-C++ platform layer (Cocoa window + CoreAudio). Effects are self-contained TUs; the panel links all of them for live preview, the bake step compiles only the chosen ones into a minimal standalone binary.

**Tech Stack:** C++20, Objective-C++ (`.mm`), Cocoa, QuartzCore, CoreAudio, AudioToolbox, Apple clang 21. No SDL, no Metal, no external libs. Local git only.

**Spec:** `docs/superpowers/specs/2026-08-30-tinyintro-design.md`

## Global Constraints

- Language: C++20 (`-std=c++20`); platform glue in Objective-C++ `.mm`.
- Zero external libraries. Only frameworks: Cocoa, QuartzCore, CoreAudio, AudioToolbox.
- Rendering: 8-bit **indexed** `Surface`, 320×200, integer-scaled to the window (nearest).
- Master clock = audio sample position.
- No asset files — font, palettes, and music are procedural/embedded.
- Engine core (`engine/`, `effects/`) must compile host-side with no Cocoa dependency, so it is unit-testable headless.
- Size discipline: release/bake uses `-Os -flto -fno-exceptions -fno-rtti -Wl,-dead_strip` + `strip -x`; report binary bytes.
- Git is local only; never add a remote or push.

---

## File structure

```
engine/    surface.{h,cpp}  tables.{h,cpp}  font.{h,cpp}  timeline.{h,cpp}
           effect.h  registry.{h,cpp}  synth.{h,cpp}  rng.h
effects/   plasma.cpp  copperbars.cpp  scroller.cpp  starfield.cpp  rotozoom.cpp  tunnel.cpp
platform/  platform.h  mac_window.mm  mac_audio.mm
panel/     introspec.{h,cpp}  panel.cpp  bake.cpp
templates/ main.mm.tmpl  timeline.cpp.tmpl
tests/     test_main.cpp  test_tables.cpp  test_surface.cpp  test_font.cpp
           test_timeline.cpp  test_synth.cpp  test_introspec.cpp  test_bake.cpp
build/     build.sh  test.sh
demos/     first.tro
```

Testing uses a tiny header-only assert harness (`tests/test.h`) — no external test framework (zero-deps constraint).

---

## Phase 1 — Engine + platform vertical slice

Deliverable: a standalone native window showing animated plasma + a sine scroller, with procedural sound.

### Task 1: Test harness + build scripts

**Files:**
- Create: `tests/test.h`, `tests/test_main.cpp`, `build/test.sh`, `build/build.sh`

**Interfaces:**
- Produces: `CHECK(cond)`, `CHECK_EQ(a,b)`, `CHECK_NEAR(a,b,eps)`, `RUN_TEST(fn)`, `int tinyintro_tests_failed()`; test files define `void register_*tests()` called from `test_main.cpp`.

- [ ] **Step 1: Write `tests/test.h`** — a header-only harness.

```cpp
#pragma once
#include <cstdio>
#include <cmath>
#include <cstdlib>
inline int& g_fail() { static int f = 0; return f; }
inline int& g_count() { static int c = 0; return c; }
#define CHECK(cond) do { g_count()++; if(!(cond)) { \
  std::printf("  FAIL %s:%d  CHECK(%s)\n", __FILE__, __LINE__, #cond); g_fail()++; } } while(0)
#define CHECK_EQ(a,b) do { g_count()++; auto _a=(a); auto _b=(b); if(!(_a==_b)) { \
  std::printf("  FAIL %s:%d  CHECK_EQ(%s,%s)\n", __FILE__, __LINE__, #a, #b); g_fail()++; } } while(0)
#define CHECK_NEAR(a,b,eps) do { g_count()++; double _d=std::fabs((double)(a)-(double)(b)); \
  if(_d>(eps)) { std::printf("  FAIL %s:%d  CHECK_NEAR(%s,%s) diff=%g\n", __FILE__,__LINE__,#a,#b,_d); g_fail()++; } } while(0)
#define RUN_TEST(fn) do { std::printf("[ RUN ] %s\n", #fn); fn(); } while(0)
inline int tinyintro_tests_failed() { return g_fail(); }
```

- [ ] **Step 2: Write `tests/test_main.cpp`** calling each suite.

```cpp
#include "test.h"
void register_tables_tests();
void register_surface_tests();
void register_font_tests();
void register_timeline_tests();
void register_synth_tests();
void register_introspec_tests();
void register_bake_tests();
int main() {
  register_tables_tests();
  register_surface_tests();
  register_font_tests();
  register_timeline_tests();
  register_synth_tests();
  register_introspec_tests();
  register_bake_tests();
  std::printf("\n%d checks, %d failed\n", g_count(), g_fail());
  return tinyintro_tests_failed() ? 1 : 0;
}
```

Each `register_*` is defined in its own test TU; until a TU exists, provide an empty stub in that TU (added per task). To keep `test_main.cpp` linking before all suites exist, define weak empty stubs in `tests/stubs.cpp`.

- [ ] **Step 3: Write `build/test.sh`** — compiles engine + effects + tests headless.

```bash
#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
SRC="engine/*.cpp effects/*.cpp panel/introspec.cpp panel/bake.cpp"
mkdir -p build/out
clang++ -std=c++20 -O1 -g -Iengine -Ipanel \
  tests/*.cpp $SRC -o build/out/tests 2>&1
./build/out/tests
```

(Effects/panel sources are added as their tasks land; start with the ones that exist and expand `SRC` — but globbing handles it automatically.)

- [ ] **Step 4: Write `build/build.sh`** — builds the Phase-1 standalone app (dev flags).

```bash
#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/out
clang++ -std=c++20 -O2 -Iengine -Ipanel -fobjc-arc \
  engine/*.cpp effects/*.cpp platform/*.mm panel/introspec.cpp \
  -o build/out/tinyintro-demo \
  -framework Cocoa -framework QuartzCore -framework CoreAudio -framework AudioToolbox
echo "built build/out/tinyintro-demo"
```

- [ ] **Step 5: Run `bash build/test.sh`** — Expected: compiles, `0 checks, 0 failed` (no suites yet), exit 0. Fix pathing until green.

- [ ] **Step 6: Commit** — `git add -A && git commit -m "test: add zero-dep test harness and build scripts"`

### Task 2: Lookup tables (`engine/tables`)

**Files:** Create `engine/tables.h`, `engine/tables.cpp`, `tests/test_tables.cpp`

**Interfaces:**
- Produces: `namespace tt { const int SIN_BITS=12, SIN_SIZE=4096, SIN_MASK=4095; int isin(int a); int icos(int a);` returning fixed-point sine in range [-4096,4096] (i.e. `<<12`); `float fsin(float radians)` table-backed; `void init_tables()` (idempotent). Angle unit for `isin` is table index (0..4095 = full turn).}

- [ ] **Step 1: Write failing test `tests/test_tables.cpp`.**

```cpp
#include "test.h"
#include "tables.h"
#include <cmath>
static void test_isin_matches_libm() {
  tt::init_tables();
  for (int i = 0; i < tt::SIN_SIZE; i += 37) {
    double rad = (double)i / tt::SIN_SIZE * 2.0 * M_PI;
    double want = std::sin(rad) * 4096.0;
    CHECK_NEAR(tt::isin(i), want, 8.0);
  }
  CHECK_EQ(tt::isin(0), 0);
  CHECK_EQ(tt::icos(0), 4096);
}
void register_tables_tests() { RUN_TEST(test_isin_matches_libm); }
```

- [ ] **Step 2: Run `bash build/test.sh`** — Expected: FAIL to compile (no `tables.h`).

- [ ] **Step 3: Implement `engine/tables.h` + `engine/tables.cpp`.**

```cpp
// tables.h
#pragma once
namespace tt {
  constexpr int SIN_BITS = 12, SIN_SIZE = 4096, SIN_MASK = 4095, SIN_ONE = 4096;
  void init_tables();
  int isin(int idx);   // idx & SIN_MASK, returns sin*4096
  int icos(int idx);
  float fsin(float radians);
}
```
```cpp
// tables.cpp
#include "tables.h"
#include <cmath>
namespace tt {
  static int s_sin[SIN_SIZE];
  static bool s_ready = false;
  void init_tables() {
    if (s_ready) return;
    for (int i = 0; i < SIN_SIZE; ++i)
      s_sin[i] = (int)std::lround(std::sin((double)i / SIN_SIZE * 2.0 * M_PI) * SIN_ONE);
    s_ready = true;
  }
  int isin(int idx) { return s_sin[idx & SIN_MASK]; }
  int icos(int idx) { return s_sin[(idx + SIN_SIZE/4) & SIN_MASK]; }
  float fsin(float r) {
    int idx = (int)(r / (2.0f * 3.14159265358979f) * SIN_SIZE);
    return isin(idx) / (float)SIN_ONE;
  }
}
```

- [ ] **Step 4: Register suite** — ensure `register_tables_tests()` is declared in `test_main.cpp` (already is) and remove its weak stub from `tests/stubs.cpp`.

- [ ] **Step 5: Run `bash build/test.sh`** — Expected: PASS.

- [ ] **Step 6: Commit** — `git commit -am "feat(engine): fixed-point sin/cos lookup tables"`

### Task 3: Indexed Surface + palette (`engine/surface`)

**Files:** Create `engine/surface.h`, `engine/surface.cpp`, `tests/test_surface.cpp`

**Interfaces:**
- Produces:
```cpp
namespace ti {
  constexpr int W = 320, H = 200;
  struct Surface {
    unsigned char idx[W*H];
    unsigned int  pal[256];        // 0xAARRGGBB
    void clear(unsigned char c);
    void put(int x, int y, unsigned char c);      // bounds-checked
    unsigned char get(int x, int y) const;        // 0 if OOB
    void expand(unsigned int* argb_out) const;    // W*H ARGB
    void set_pal(int i, unsigned char r, unsigned char g, unsigned char b);
    void cycle_pal(int first, int count, int by);  // rotate a palette range
  };
}
```

- [ ] **Step 1: Write failing test `tests/test_surface.cpp`.**

```cpp
#include "test.h"
#include "surface.h"
static void test_surface_basics() {
  static ti::Surface s;
  s.clear(0);
  s.set_pal(5, 255, 128, 0);
  s.put(10, 10, 5);
  CHECK_EQ((int)s.get(10,10), 5);
  CHECK_EQ((int)s.get(-1,0), 0);       // OOB read -> 0
  s.put(9999, 9999, 7);                 // OOB write ignored (no crash)
  static unsigned int out[ti::W*ti::H];
  s.expand(out);
  CHECK_EQ(out[10*ti::W+10], 0xFFFF8000u);
}
static void test_palette_cycle() {
  static ti::Surface s;
  for (int i=0;i<4;i++) s.set_pal(i, (unsigned char)i, 0, 0);
  s.cycle_pal(0, 4, 1);
  CHECK_EQ(s.pal[0] & 0xFF0000u, 0x030000u); // old pal[3] rotated into pal[0]
}
void register_surface_tests() { RUN_TEST(test_surface_basics); RUN_TEST(test_palette_cycle); }
```

- [ ] **Step 2: Run test** — Expected: FAIL (no `surface.h`).
- [ ] **Step 3: Implement `surface.h` / `surface.cpp`** with `pal` storing `0xFF<<24 | r<<16 | g<<8 | b`; `expand` maps each index; `cycle_pal` rotates in place.
- [ ] **Step 4: Remove `register_surface_tests` stub.**
- [ ] **Step 5: Run test** — Expected: PASS.
- [ ] **Step 6: Commit** — `git commit -am "feat(engine): indexed Surface with palette + cycling"`

### Task 4: Pixel font (`engine/font`)

**Files:** Create `engine/font.h`, `engine/font.cpp`, `tests/test_font.cpp`

**Interfaces:**
- Produces:
```cpp
namespace ti {
  struct Surface;
  namespace font {
    constexpr int GW = 8, GH = 8;
    const unsigned char* glyph(char c);     // 8 bytes, MSB=leftmost pixel; unknown -> space
    int text_width(const char* s);          // GW * len
    void draw(Surface& s, int x, int y, const char* str, unsigned char color);
    bool glyph_pixel(char c, int gx, int gy); // gx 0..7 left->right, gy 0..7 top->bottom
  }
}
```
Font covers ASCII 32..90 (space, digits, punctuation, A–Z) uppercase — classic cracktro set. Store a compact `unsigned char[59][8]` for a subset; lowercase maps to uppercase.

- [ ] **Step 1: Write failing test `tests/test_font.cpp`** checking `text_width("ABC")==24`, `glyph('A')` non-empty, `glyph_pixel` sane for a known glyph (e.g. `'-'` middle row set, top row clear), and `draw` sets expected pixels for `"A"`.
- [ ] **Step 2: Run** — FAIL.
- [ ] **Step 3: Implement** the font table (hand-designed 8×8 for space, `!`, `.`, `,`, `:`, `-`, `<`, `>`, digits 0–9, A–Z, plus `♪`? no — ASCII only; map unknown to space). Provide a small but complete set; document each row as a bit pattern comment.
- [ ] **Step 4: Remove stub.**
- [ ] **Step 5: Run** — PASS.
- [ ] **Step 6: Commit** — `git commit -am "feat(engine): 8x8 procedural pixel font"`

### Task 5: Effect contract + registry (`engine/effect.h`, `engine/registry`)

**Files:** Create `engine/effect.h`, `engine/registry.h`, `engine/registry.cpp`, `engine/rng.h`

**Interfaces:**
- Produces:
```cpp
namespace ti {
  struct Surface;
  struct Params {                 // fixed-size named float params
    float p[8] = {0,0,0,0,0,0,0,0};
    const char* text = nullptr;   // optional (scroller uses it)
  };
  struct Ctx { double time; double beat; int frame; };
  struct EffectInfo {
    const char* id;
    const char* name;
    const char* secret;                  // multi-line explanation
    const char* param_names[8];          // nullptr-terminated-ish
    float param_defaults[8];
    void (*render)(Surface&, const Ctx&, const Params&);
  };
  struct EffectRegistry {
    const EffectInfo** items; int count;
    const EffectInfo* find(const char* id) const;
  };
  const EffectRegistry& effects();
  int register_effect(const EffectInfo* e);   // called by REGISTER_EFFECT
}
#define TI_REGISTER_EFFECT(sym) \
  static int _reg_##sym = ti::register_effect(&sym);
```
`rng.h`: a tiny deterministic xorshift `struct Rng { unsigned s; unsigned next(); float f01(); }`.

- [ ] **Step 1:** No direct test file yet (registry is exercised via effects). Add `register_effect`/`effects()` with a static array (cap 32).
- [ ] **Step 2: Implement** `registry.cpp` (static array + `find` by strcmp) and `rng.h`.
- [ ] **Step 3: Compile** via `build/test.sh` (still green; nothing calls it yet).
- [ ] **Step 4: Commit** — `git commit -am "feat(engine): effect contract + registry + rng"`

### Task 6: Plasma effect (`effects/plasma.cpp`)

**Files:** Create `effects/plasma.cpp`. (Tested indirectly; add `tests/test_effects.cpp` smoke.)

**Interfaces:**
- Consumes: `Surface`, `tt::isin`, `Ctx`, `Params`, registry.
- Produces: `EffectInfo plasma_effect` (id `"plasma"`); writes indices 0..255 across the surface; sets a smooth cycling palette on first call.

- [ ] **Step 1: Write smoke test `tests/test_effects.cpp`** — `effects().find("plasma")` non-null; after `render`, surface is non-uniform (at least two distinct indices).
- [ ] **Step 2: Run** — FAIL (no plasma).
- [ ] **Step 3: Implement** classic sum-of-sines plasma using `tt::isin`; comment the "secret" inline; register via `TI_REGISTER_EFFECT`.
- [ ] **Step 4: Add `register_effects_tests()`** to `test_main.cpp` + declaration; remove stub.
- [ ] **Step 5: Run** — PASS.
- [ ] **Step 6: Commit** — `git commit -am "feat(effects): plasma"`

### Task 7: Sine scroller effect (`effects/scroller.cpp`)

**Files:** Create `effects/scroller.cpp`; extend `tests/test_effects.cpp`.

**Interfaces:**
- Consumes: `font`, `tt::isin`, `Params.text`, `Ctx.time`.
- Produces: `EffectInfo scroller_effect` (id `"scroller"`): renders `Params.text` scrolling right→left with a per-column vertical sine offset; color index from a param.

- [ ] **Step 1: Extend smoke test** — with `text="HELLO"`, after render at a time where 'H' is on screen, surface has set pixels in the scroller band.
- [ ] **Step 2: Run** — FAIL.
- [ ] **Step 3: Implement** scroller: for each screen column, compute source text pixel via `font::glyph_pixel`, apply `y += isin(col*freq + phase)`; comment the secret.
- [ ] **Step 4: Run** — PASS.
- [ ] **Step 5: Commit** — `git commit -am "feat(effects): sine scroller"`

### Task 8: Platform layer — Cocoa window (`platform/mac_window.mm`, `platform/platform.h`)

**Files:** Create `platform/platform.h`, `platform/mac_window.mm`.

**Interfaces:**
- Produces:
```cpp
// platform.h (C++-facing)
namespace ti { struct Surface; }
namespace plat {
  struct App {
    virtual void frame(double time, unsigned int* argb) = 0; // fill W*H ARGB
    virtual void key(int keycode, bool down) {}
    virtual ~App() {}
  };
  int run(App& app, const char* title);   // returns exit code
  double audio_time();                     // master clock (seconds); 0 until audio task
}
```
`mac_window.mm`: `NSApplication`, borderless-or-titled `NSWindow` sized `W*scale × H*scale` (scale=4), an `NSView` backed by a `CALayer` whose `contents` = `CGImage` from the ARGB buffer each frame; `magnificationFilter = kCAFilterNearest`; `CVDisplayLink` calls `app.frame`; `NSEvent` local monitor forwards keys; ESC quits.

- [ ] **Step 1:** (No headless unit test — GUI.) Implement `platform.h` and `mac_window.mm`. Use ARC (`-fobjc-arc`). Build a `CGImage` per frame from a `CGBitmapContext`/`CGDataProvider` over the ARGB buffer (kCGImageAlphaNoneSkipFirst, little-endian).
- [ ] **Step 2:** Provide a temporary `audio_time()` returning wall-clock seconds (replaced in Task 10).
- [ ] **Step 3: Build** with `bash build/build.sh` — Expected: links (a `DemoApp` main is added in Task 9; until then build may reference it — so implement Task 9 before first full build, or stub `main` here). To keep this task independently buildable, add a minimal `platform/_selftest_main.mm` under `#ifdef PLAT_SELFTEST` that opens a black window; not compiled by default.
- [ ] **Step 4: Commit** — `git commit -am "feat(platform): Cocoa window presenting an ARGB framebuffer"`

### Task 9: Demo app wiring (`panel/introspec.cpp` minimal + a demo main)

**Files:** Create `platform/demo_main.mm` (Phase-1 entry), minimal `panel/introspec.h`/`.cpp` (enough for Ctx/time). Update `build/build.sh` to compile `platform/demo_main.mm`.

**Interfaces:**
- Consumes: `effects()`, `Surface`, `plat::run`, `plat::audio_time`.
- Produces: a `DemoApp : plat::App` that on each `frame` renders plasma into the Surface, overlays the scroller (`text="TINYINTRO ... GREETZ TO EVERYONE STILL WRITING C++ IN 2026 ..."`), expands to ARGB.

- [ ] **Step 1:** Implement `demo_main.mm`: build `Surface`, init tables, on `frame(time,argb)` set `Ctx{time,...}`, call plasma then scroller, `expand(argb)`.
- [ ] **Step 2: Build + run** `bash build/build.sh && ./build/out/tinyintro-demo` — Expected: a window with animated plasma + scroller (visually verify; the process runs without crashing; ESC quits). Since this session can't see the window, verify: builds cleanly, launches, and exits 0 on ESC/quit within a short run.
- [ ] **Step 3: Commit** — `git commit -am "feat: Phase 1 demo app — plasma + scroller window"`

### Task 10: CoreAudio + minimal synth (`platform/mac_audio.mm`, `engine/synth`)

**Files:** Create `engine/synth.h`, `engine/synth.cpp`, `platform/mac_audio.mm`, `tests/test_synth.cpp`. Wire audio start into `plat::run`; make `audio_time()` return the sample-based clock.

**Interfaces:**
- Produces:
```cpp
namespace ti {
  struct Synth {
    double sr = 44100.0;
    unsigned long long sample_pos = 0;
    void render(float* out, int frames);     // fills interleaved stereo
    static float osc_sine(double phase);
  };
}
namespace plat { void audio_start(ti::Synth* s); }  // called by run()
```
`audio_time()` returns `synth.sample_pos / sr`.

- [ ] **Step 1: Write `tests/test_synth.cpp`** — determinism: two `Synth` instances render identical buffers; `sample_pos` advances by `frames`; output within [-1,1].
- [ ] **Step 2: Run** — FAIL.
- [ ] **Step 3: Implement** `synth.cpp`: a simple bass+lead sine as in `info.txt` (evolves in Phase 2), advancing `sample_pos`. Implement `mac_audio.mm`: default-output `AudioUnit`, render callback → `Synth::render`.
- [ ] **Step 4: Wire** `plat::run` to start audio; `audio_time()` uses sample clock.
- [ ] **Step 5: Run tests** — PASS; **build+run** app — plasma+scroller now driven by sample clock, sound audible.
- [ ] **Step 6: Commit** — `git commit -am "feat(audio): CoreAudio output + sample-clock synth"`

**Phase 1 exit check:** `bash build/test.sh` green; `bash build/build.sh` produces `tinyintro-demo` that runs with visuals + sound, ESC quits.

---

## Phase 2 — Effects library, timeline, sequenced song (task-level; expand on arrival)

- **Task 11: Timeline** (`engine/timeline`) — beat/scene mapping from the sample clock; unit-tested (`test_timeline.cpp`): given BPM + scene table, `scene_at(time)` and `beat_at(time)` correct at boundaries.
- **Task 12: copperbars** effect + smoke test.
- **Task 13: starfield** effect (uses `Rng`) + smoke test.
- **Task 14: rotozoom** effect (affine table lookup) + smoke test.
- **Task 15: tunnel** effect (precomputed angle/distance tables) + smoke test.
- **Task 16: Sequencer** in `synth` — `Song` = per-track `uint8_t` note arrays + tempo; ADSR; square/saw/noise osc; simple one-pole filter + delay; deterministic render test.
- **Task 17: Hand-authored timeline demo** — `demos/first.tro`-equivalent in code: black→logo→starfield→scroller→plasma→rotozoom→tunnel+credits→finale, synced to song. Build+run.

## Phase 3 — The panel (task-level; expand on arrival)

- **Task 18: `IntroSpec` full model + `.tro` text load/save** (`panel/introspec`) — round-trip unit test (`test_introspec.cpp`).
- **Task 19: Panel interpreter** — render an `IntroSpec` timeline live (shared with bake output semantics).
- **Task 20: Retro pixel GUI shell** — effect/scene list, timeline strip, selection, drawn with `font`+`Surface`.
- **Task 21: Parameter editor** — edit `Params` from `EffectInfo.param_names`/defaults; live update.
- **Task 22: Explanation box** — show selected effect `secret`.
- **Task 23: Save/Load + demo `.tro`** in `demos/`.

## Phase 4 — Bake pipeline (task-level; expand on arrival)

- **Task 24: Codegen** (`panel/bake.cpp` + `templates/`) — from `IntroSpec` emit `generated/main.mm` + `generated/timeline.cpp` embedding palette/text/song as `constexpr`; include each chosen effect's `secret` as comments. Unit test (`test_bake.cpp`): generated source references only chosen effect ids.
- **Task 25: Compile + size report** — invoke clang with size flags, `strip -x`, report bytes; golden test that a known spec compiles and links; assert non-zero size reported.
- **Task 26: "Ultra-tiny" flag set** + a size-comparison note in the panel.

---

## Self-review notes

- **Spec coverage:** engine (Tasks 2–7,11,16), platform native (8,10), panel (18–23), bake (24–26), effects (6,7,12–15), synth/master-clock (10,16), testing strategy (headless engine tests throughout). All spec §3–§7 map to tasks.
- **Type consistency:** `Surface`, `Ctx`, `Params`, `EffectInfo`, `effects()`, `Synth`, `plat::App/run/audio_time` names are used identically across tasks.
- **Placeholders:** Phase 1 tasks carry real code/tests; Phases 2–4 are intentionally task-level and each will be expanded to bite-sized steps when reached (executor: expand before implementing).
