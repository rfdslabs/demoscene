# TinyIntro — Design Spec

**Date:** 2026-08-30
**Status:** Approved for implementation
**Scope:** A macOS demoscene cracktro *creator* — an old-school pixel control panel where
you pick style + technique, read the "secrets" behind each effect, preview live, and bake a
tiny standalone native binary.

---

## 1. Goal & principles

Build **TinyIntro**: a self-drawn retro panel (the "painel") plus the intros it produces.
Both the panel and the generated intros run on **one shared CPU software-rendering engine**,
presented through **pure Cocoa + CoreAudio** with **zero external libraries**.

Guiding principles:

1. **One engine, three consumers.** The panel, the live preview, and the baked binary all
   run the *exact same* effect code. No effect is implemented twice.
2. **Smaller binary = better.** The bake step compiles only the chosen effects; classic
   8-bit indexed rendering keeps everything tiny. Size is a reported, first-class metric.
3. **The code is the product.** The generated project is real, readable, commented source
   the user keeps. Each effect carries an explanation of its "secret."
4. **Authentic old-school.** 8-bit indexed color, palette cycling, sin/lookup tables,
   procedural pixel font, procedural synth. No asset files, ever.
5. **Pure native.** Window via Cocoa/`CALayer`+`CGImage`, audio via CoreAudio. The only
   linked frameworks are Cocoa, QuartzCore, CoreAudio, AudioToolbox.

Fixed decisions (from brainstorming):

- Output model: **both** — data-driven live preview while designing, codegen→compile bake
  for the tiny release binary.
- Visual tech: **CPU software framebuffer** (classic 8-bit indexed palette effects).
- Runtime deps of generated intro: **pure macOS native** (Cocoa + CoreAudio).
- Panel UI: **retro pixel GUI**, self-drawn with the same engine.
- Name: **TinyIntro**. Resolution: **320×200**, integer-scaled (4× = 1280×800, square pixels).
- Toolchain present: Apple clang 21, SDL2 available but **not used** (native only).
- Git: **local only**, no remotes, never pushed.

---

## 2. Architecture overview

```
                        IntroSpec (config: scenes, palette, text, song)
                              │                         │
                    live preview (panel)          bake (codegen + clang)
                              │                         │
                              ▼                         ▼
                     ┌──────────────────────────────────────────┐
                     │            shared engine (C++)            │
                     │  Surface(8bpp+palette) · tables · font ·  │
                     │  timeline · effect registry · synth       │
                     └──────────────────────────────────────────┘
                              │                         │
                              ▼                         ▼
                     platform layer (.mm): Cocoa window + CoreAudio
```

The panel links **all** effects (instant preview of anything). The baked binary links
**only the chosen** effects (minimal size). Same source files in both cases — the only
generated code is `main.mm` + `timeline.cpp`.

---

## 3. Components

### 3.1 Engine core (`engine/`, portable C++)

- **`surface.{h,cpp}`** — `Surface` holds an `uint8_t*` index buffer (320×200) plus a
  256-entry `uint32_t` palette. `present(argb_out)` expands indices→ARGB. Integer scaling
  happens in the platform blit. Primitives: clear, blit, palette set/cycle.
- **`tables.{h,cpp}`** — precomputed `sin`/`cos`, distance and atan tables. The classic
  optimization; each documented as a "secret."
- **`font.{h,cpp}`** — embedded 8×8 pixel font (procedural array, no asset). Draw string,
  measure, used by scrollers and the panel UI.
- **`timeline.{h,cpp}`** — master clock = **audio sample position**. Scenes have time
  ranges; events (flash, palette swap, scene change) hang off the timeline.
- **`effect.h`** — the uniform effect contract:
  ```cpp
  struct Ctx { double time; double beat; int frame; const IntroSpec* spec; };
  struct EffectInfo {
      const char* name;
      const char* secret;          // shown in panel + emitted as source comments
      void (*render)(Surface&, const Ctx&, const Params&);
      ParamSchema params;          // named, ranged params for the editor
  };
  const EffectRegistry& effects(); // registry populated by each effect TU
  ```
- **`synth.{h,cpp}`** — oscillators (sine/square/saw/noise) → ADSR → sequencer over
  `uint8_t` note arrays → simple filter + delay. Fills the CoreAudio buffer; advances the
  master sample counter.

### 3.2 Effects (`effects/`, one self-contained TU each)

Each depends only on `engine/`. First set:

- `plasma.cpp` — sum-of-sines indexed into a cycling palette.
- `copperbars.cpp` — per-scanline color bars (raster/copper effect).
- `scroller.cpp` — horizontal sine scroller sampling glyph pixels with a vertical sin offset.
- `starfield.cpp` — parallax starfield.
- `rotozoom.cpp` — rotating/zooming textured plane via affine table lookup.
- `tunnel.cpp` — precomputed angle/distance tables mapping to a texture.

Each registers an `EffectInfo` (incl. `secret` text and `ParamSchema`).

### 3.3 Platform layer (`platform/`, Objective-C++ `.mm`)

- **`platform.h`** — C++-facing interface: `run(App&)`, audio callback registration,
  key events, present(argb, w, h).
- **`mac_window.mm`** — `NSApplication`/`NSWindow`; a `CALayer` whose `contents` is a
  `CGImage` built from the ARGB buffer each frame; `CVDisplayLink`/`CADisplayLink` drives
  60fps; `NSEvent` keyboard. Integer upscaling via layer `magnificationFilter = nearest`.
- **`mac_audio.mm`** — CoreAudio default-output `AudioUnit`; render callback pulls from the
  synth. The running sample count is exposed as the master clock.

### 3.4 The panel (`panel/`)

- **`introspec.{h,cpp}`** — `IntroSpec` config model: ordered scenes (effect id + params +
  duration), global palette, scroller text, song data. Text `.tro` load/save (small,
  human-readable).
- **`panel.cpp`** — retro pixel GUI drawn with the engine: effect/scene list, timeline
  strip, parameter editor (from `ParamSchema`), live preview pane, explanation box
  (the selected effect's `secret`). Actions: Preview (windowed/fullscreen), Save/Load, Bake.
- **`bake.cpp`** — codegen + build (see §3.5).

### 3.5 Bake pipeline (`templates/` + `panel/bake.cpp`)

1. From `IntroSpec`, emit `generated/main.mm` and `generated/timeline.cpp`, embedding
   palette, scroller text and song as `constexpr` data. Emit each chosen effect's `secret`
   as a leading comment.
2. Copy/reference only the chosen `effects/*.cpp` + `engine/*` + `platform/*`.
3. Invoke:
   `clang++ -std=c++20 -Os -flto -ffunction-sections -fno-exceptions -fno-rtti
    -Wl,-dead_strip -framework Cocoa -framework QuartzCore -framework CoreAudio
    -framework AudioToolbox`, then `strip -x`.
4. Report the resulting **binary size in bytes**. Optional "ultra-tiny" flag set.

The `generated/` folder is a complete, buildable, commented project the user keeps — this
is the "secrets of the code" artifact.

---

## 4. Data model (`IntroSpec`)

```
IntroSpec {
  Palette palette[256];
  string  scrollerText;
  Song    song;               // uint8_t note arrays per track + tempo
  Scene   scenes[];           // { effectId, Params, startBeat, durationBeats }
}
```

`.tro` file: a compact key/value + table text format. Deterministic round-trip.

---

## 5. Directory layout

```
introcreator/
  engine/     surface, tables, font, timeline, effect, synth
  effects/    plasma, copperbars, scroller, starfield, rotozoom, tunnel
  platform/   platform.h, mac_window.mm, mac_audio.mm
  panel/      introspec, panel, bake
  templates/  main.mm.tmpl, timeline.cpp.tmpl
  build/      build.sh / Makefile
  demos/      example .tro files
  generated/  (bake output; gitignored)
  docs/superpowers/specs/
```

---

## 6. Phasing (drives the implementation plan)

- **Phase 1 — Engine + platform vertical slice.** Surface+palette present via Cocoa, frame
  loop, keyboard, CoreAudio callback, sin tables, pixel font → a standalone native window
  showing animated plasma + a sine scroller **with sound**. De-risks every hard part
  (native CPU-buffer present, CoreAudio callback synth, tiny build).
- **Phase 2 — Effects + timeline + song.** Implement the full effect set and a real
  sequenced song; a hand-written `IntroSpec` plays a complete timed cracktro. Deliverable
  on its own.
- **Phase 3 — The panel.** Retro pixel GUI, picker, param editor, live preview,
  explanations, `.tro` save/load.
- **Phase 4 — Bake.** Codegen + compile to tiny binary + size report. Realizes the full
  dev/release duality.

---

## 7. Testing strategy

Native macOS GUI/audio is hard to unit-test end-to-end, so isolate the pure logic:

- **Engine unit tests** (host-compiled, no Cocoa): table correctness (sin table vs libm
  within tolerance), palette expansion, font glyph metrics, timeline beat/scene mapping,
  synth sample determinism, `IntroSpec` `.tro` round-trip, codegen output shape (generated
  source contains only chosen effects; compiles).
- **Platform layer**: smoke-tested by running the app; kept thin so little logic lives here.
- **Bake**: golden test that a known `IntroSpec` produces source that compiles and links,
  and that binary size is reported.

TDD applies to engine + panel logic. Platform `.mm` glue is validated by running.

---

## 8. Non-goals (YAGNI for v1)

- No Metal/GPU path (CPU framebuffer only).
- No SDL (native only).
- No ARM64 hand-asm in v1 (kept as a later "low-level challenge" option, per `info.txt`).
- No cross-platform (macOS only).
- No external audio/image asset loading (all procedural).
- No networking, no packaging/notarization/signing beyond a runnable binary.
