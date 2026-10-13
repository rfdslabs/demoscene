# realism_island_intro

Semi-realistic island demo (Mac, C++/SDL2/OpenGL):

- raymarched terrain island with sand/grass/rock shading
- animated water with fresnel reflection and highlights
- sky/sun lighting + fog for cinematic depth
- slow camera orbit and subtle zoom
- calm cinematic procedural ambient audio

## Build

```bash
cd realism_island_intro
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/realism_island_intro
```

## Controls

- `Esc`: quit
- `F`: fullscreen
