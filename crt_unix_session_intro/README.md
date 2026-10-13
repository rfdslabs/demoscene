# crt_unix_session_intro

CRT monitor style demo with animated UNIX terminal session:

- bash-like prompt and typed command history
- xterm-like steady cursor (no blinking)
- DNS-style lookup lines for `EXAMPLE.COM` (visual simulation)
- scanlines, phosphor glow, ghost-text persistence
- mellow calm-winter ambient background music

## Build

```bash
cd crt_unix_session_intro
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/crt_unix_session_intro
```

## Controls

- `Esc`: quit
- `F`: toggle fullscreen
