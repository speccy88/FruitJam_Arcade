# macOS host simulator

This standalone SDL2 target runs the same native game, wireframe renderer, raw-pad state, and 128x128 graphics implementation as the Fruit Jam firmware. It does not build or link the Pico SDK, TinyUSB, DVI, Lua, or PICO-8 runtime. GPIO effects are harmless host-side shims, and mouse coordinates feed the game's existing optional analog-aim interface.

## Requirements

- macOS with Apple clang and CMake
- SDL2 discoverable by either CMake or `pkg-config`

With Homebrew, install missing tools with:

```sh
brew install cmake sdl2 pkg-config
```

## Build and run

From the repository root:

```sh
./host/run.sh
```

Or build manually:

```sh
cmake -S host -B host/build
cmake --build host/build --parallel
./host/build/fruitjam_railshooter_host
```

The window is resizable and preserves the original 128x128 image with nearest-neighbor scaling.

## Controls

- Mouse: aim
- Left mouse button, Space, or Z: fire
- Right mouse button, R, X, or B: reload
- Q: activate Pulse Wave when the blue meter is full
- Return or P: start / pause
- Tab or Backspace: toggle debug overlay
- C: coin / credit
- Arrow keys or WASD: digital aim fallback
- Escape: quit
- SDL controller left stick: aim
- Controller right trigger or A: fire
- Controller left trigger or B: reload
- Controller Y: activate Pulse Wave
- Controller Start: start / pause
- Controller Back/View: toggle debug overlay

Moving the mouse selects mouse aiming. Moving the controller stick or pressing an aim key selects velocity-style aiming until the mouse moves again.

SDL can use a controller only when macOS exposes it as a game controller. Keyboard and mouse controls remain available if a particular wired Xbox 360 adapter or driver is not recognized by macOS.

## Automated testing and screenshots

`--frames N` exits successfully after exactly N rendered frames. SDL's dummy video and audio drivers are supported for terminal or CI smoke tests:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./host/run.sh --frames 3
```

Pass `--mute` to skip opening an audio device.

`--demo` supplies a synthetic controller, `--fast` advances the game at a fixed 60 Hz without waiting in real time, and `--screenshot` saves the final framebuffer. Together they exercise actual combat and wave progression instead of remaining on the title screen:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./host/run.sh --demo --fast --frames 7200 --mute \
  --screenshot /tmp/vector-raid.bmp
```

The simulator prints its final state, wave, score, live-enemy count, and rendered-frame count after a demo run.
