# mac_demo

Mac-first demoscene-style mini intro in C++ with:

- SDL2 window/input
- OpenGL fragment shader visuals
- procedural realtime audio (no external sound files)

## Requirements

- macOS
- Xcode Command Line Tools (`xcode-select --install`)
- Homebrew packages:
  - `cmake`
  - `sdl2`

## Build and Run

```bash
cd mac_demo
cmake -S . -B build
cmake --build build -j
./build/mac_demo
```

## Controls

- `Esc`: quit
- `F`: toggle fullscreen
- `1`: colorful tunnel scene
- `2`: monochrome Revision-style scene
