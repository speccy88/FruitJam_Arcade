# SDL desktop host simulator

This standalone SDL2 target runs the same native game, hybrid filled/wireframe
renderer, raw-pad state, and 128x128 design-coordinate graphics implementation
as the Fruit Jam firmware.
The desktop target replaces the board-specific DVI, USB, audio, and GPIO layers
with host adapters; GPIO effects are harmless shims, and mouse coordinates feed
the same aiming boundary intended for future physical arcade controls.

## Requirements

- macOS with Apple clang, or Linux with GCC/Clang and standard build tools
- SDL2 discoverable by either CMake or `pkg-config`

On macOS with Homebrew:

```sh
brew install cmake sdl2 pkg-config
```

On Ubuntu or Debian Linux:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake libsdl2-dev pkg-config
```

SDL 2.0.18 or newer is required.

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

The game is authored in a 128x128 logical coordinate space, but the host
framebuffer is 256x256 by default. World lines, circles, and filled low-poly
surfaces use the denser raster while controls and hit testing keep the same
logical coordinates. The window is resizable and uses nearest-neighbor integer
scaling.

Render density is configured once for the whole target (`GFX_SCALE=1` through
`GFX_SCALE=8`):

```sh
cmake -S host -B host/build-3x -DGFX_SCALE=3
cmake --build host/build-3x --parallel
./host/build-3x/fruitjam_railshooter_host
```

This produces the same 384x384 render density as the current Fruit Jam firmware
without changing game layouts or input code. `GFX_SCALE=1` remains useful for
low-resolution regression comparisons.

## Controls

- Title Left/Right: select Arcade or God Test mode
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

SDL can use a controller only when the operating system exposes it through the
SDL game-controller API. Keyboard and mouse controls remain available if a
particular controller or USB adapter is not recognized.

## Automated testing and screenshots

`--frames N` exits successfully after exactly N rendered frames. SDL's dummy video and audio drivers are supported for terminal or CI smoke tests. Screenshots use the configured physical framebuffer resolution (256x256 for the default host build):

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./host/run.sh --frames 3
```

Pass `--mute` to skip opening an audio device.

`--demo` supplies a synthetic controller, `--fast` advances the game at a fixed
60 Hz without waiting in real time, and `--screenshot` saves the final
framebuffer. Together they exercise actual combat and wave progression instead
of remaining on the title screen:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./host/run.sh --demo --fast --frames 7200 --mute \
  --screenshot /tmp/vector-raid.bmp
```

`--god` implies `--demo`, selects God Test at the title screen, and is useful
for long invulnerability/ammo smoke tests. `--no-fire` disables the synthetic
trigger during a demo so enemies and attacks can approach for visual QA.

The simulator prints its final state, wave, score, live-enemy count, rendered
frame count, active mode, ammo, lives, and shield after a demo run.
