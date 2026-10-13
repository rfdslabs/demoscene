# pcx_island_zoom_intro

90s-PCX style visual example:

- low-res indexed palette look (320x200 layout, rendered at 640x400)
- tropical island map with coconut trees
- infinite zoom into the focal coconut tree
- when zoom reaches deep level, it swaps to a different island map and repeats

## Build

```bash
cd pcx_island_zoom_intro
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/pcx_island_zoom_intro
```

## Controls

- `Esc`: quit
- `F`: fullscreen
