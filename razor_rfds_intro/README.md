# razor_rfds_intro

Mac demo example inspired by oldschool cracktro aesthetics:

- grayscale/cyan oldschool vector look
- `R -> F -> D -> S` logo build loop, synced by beat
- blinking message: `RFDS WAS HERE`
- generated realtime techno loop with per-letter hit sound

## Build

```bash
cd razor_rfds_intro
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/razor_rfds_intro
```

## Controls

- `Esc`: quit
- `F`: toggle fullscreen
