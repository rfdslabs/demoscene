# TinyIntro

An old-school **demoscene cracktro creator** for macOS, in C++/Objective-C++.

A self-drawn retro pixel **panel** lets you pick the style and technique for each
scene, watch a **live preview**, read the *secret* behind every effect, and
**bake** a tiny, self-contained native binary that runs your intro.

The panel and the baked intros run on **one shared CPU software-rendering engine**
(8-bit indexed colour, sine tables, a pixel font) presented through **pure Cocoa +
CoreAudio** — **zero external libraries**. Smaller is better: the bake step
compiles only the effects you chose.

> "Cracktro" here means the demoscene art form (scrolltexts, plasma, copper bars,
> chiptune) — not software cracking. Nothing here touches DRM.

## Build & run

```sh
bash build/build.sh          # -> build/out/tinyintro (panel) and tinyintro-demo
./build/out/tinyintro        # the creator panel
./build/out/tinyintro-demo   # a ready-made multi-scene intro
bash build/test.sh           # headless engine tests (no window)
```

Requires Apple clang (macOS). No Homebrew/SDL/Metal needed.

## Using the panel

Mouse **or** keyboard. The panel plays the chiptune while you work and blips on
input. Click a scene to select it, the `< >` to change its effect, the `- +` to
nudge a parameter, the preview to play/pause, or the bottom button bar
(`PLAY ADD DEL RND SAVE LOAD BAKE`).

Keyboard (BBS style):

| Key            | Action                                  |
|----------------|-----------------------------------------|
| `UP` / `DOWN`  | select a scene                          |
| `LEFT`/`RIGHT` | change the selected scene's effect      |
| `[` / `]`      | select a parameter                      |
| `-` / `=`      | decrease / increase the parameter       |
| `Z`            | randomize the selected scene's params   |
| `SPACE`        | play / pause the live preview           |
| `R`            | restart the preview                     |
| `A` / `D`      | add / delete a scene                    |
| `S` / `L`      | save / load `demos/out.tro`             |
| `B`            | **bake** to `generated/intro` (+ size)  |
| `ESC`          | quit                                    |

## Baking a tiny binary

Press `B` in the panel, or headlessly:

```sh
./build/out/bake_cli demos/first.tro   # (build it: see tools/)
./generated/intro                      # your standalone cracktro
```

The `generated/` folder is a **real, readable, commented project** you keep —
that's the "secrets of the code" artifact. The demo spec bakes to a **~68 KB**
binary that links only macOS system frameworks.

## The effects (and their secrets)

- **Plasma** — sum sine waves of x/y/time into a palette index; animate by
  cycling the palette (movement for free).
- **Copper bars** — per-scanline colour with a quadratic gloss ramp; bars bob on
  a sine.
- **Sine scroller** — walk screen columns into a looping text ribbon; offset each
  column's y by a sin-table lookup.
- **Starfield** — project stars with `x/z`, `y/z`; march `z` toward the camera.
- **Rotozoom** — step through texture space along two rotated basis vectors that
  wrap, tiling forever.
- **Tunnel** — precompute per-pixel angle + `1/distance`; per frame just add
  scroll offsets and look up a texture.
- **Vector 3D** — a wireframe object as 3D points + edges; rotate with table
  sin/cos, project by depth, draw edges with an integer Bresenham line.
- **Logo** — blow every 8×8 font pixel up into an `S×S` block for a giant logo
  with no assets; bounce on a sine, glow by cycling one palette entry.

## Sound

A procedural chiptune generated sample-by-sample: square bass, sine arp lead, a
pad, and a drum kit (kick / snare / hi-hat from a deterministic noise hash). The
audio sample position is the master clock, so visuals stay locked to the beat.
The panel also fires short UI blips on interaction.

## Layout

```
engine/    shared software renderer (surface, tables, font, timeline, synth, registry)
effects/   one self-contained .cpp per effect (its render + its explanation)
platform/  pure-native Cocoa window + CoreAudio (Objective-C++)
panel/     the creator: IntroSpec model + .tro I/O, the pixel GUI, the bake pipeline
tools/     headless helpers (frame dumper, bake CLI)
docs/      design spec + implementation plan
```

## Status

Phases 1–4 complete: engine + platform slice, full effect set + sequenced
chiptune, the creator panel, and the bake pipeline. See
`docs/superpowers/plans/2026-08-30-tinyintro.md`.
