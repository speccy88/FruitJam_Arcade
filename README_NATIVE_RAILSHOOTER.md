# Fruit Jam Native Rail Shooter

Native RP2350B firmware for Adafruit Fruit Jam using wili8jam's DVI, graphics, USB host/XInput, audio, and board setup. It boots directly into **Fruit Jam: Vector Raid**, a 128x128 indexed-color wireframe rail-shooter, and does **not** run Lua, PICO-8 carts, the console/editor, or the REPL.

## Game

- Finite escalating sectors with formations and a major Overseer boss every five waves
- Five regular enemy classes with different speed, durability, movement, and firing patterns
- Shootable enemy projectiles, telegraphed attacks, shield damage, three hulls, and recovery invulnerability
- Projection-matched hitboxes, critical core hits, timed reloads, hit chains, and score multipliers up to x5
- Shootable shield, ammo, and pulse-energy pickups
- A screen-clearing Pulse Wave earned through accurate hits and kills
- Animated tunnel themes, rotating meshes, particles, hit markers, camera shake, damage effects, boss health, and full run statistics
- Procedural four-channel title/gameplay/boss music with distinct weapon, hit, reload, warning, and reward cues

## Run on macOS

The SDL2 host runner uses the same native game, wireframe, raw-pad, and graphics code as the firmware. From the repository root:

```sh
./host/run.sh
```

It opens a resizable pixel-scaled window with mouse, keyboard, and SDL-compatible controller support. See [`host/README.md`](host/README.md) for controls, dependencies, manual build commands, and headless smoke testing.

## Build

```sh
cmake -S . -B build -G "Unix Makefiles"
cmake --build build --target fruitjam_railshooter -j
```

Expected output: `build/fruitjam_railshooter.uf2`. Copy the UF2 to the Fruit Jam BOOTSEL drive.

GitHub releases can package one UF2 from each maintained game branch. The
branch list and release procedure are documented in [`RELEASING.md`](RELEASING.md).

## Controls

- Xbox 360 left stick: move crosshair
- Right trigger or A: fire
- B or left trigger: reload
- Y: activate Pulse Wave when the blue meter is full
- Start: start / pause / resume
- Back/View or service: toggle debug overlay
- D-pad / wili8jam digital input: fallback movement/menu input
- USB keyboard Q: activate Pulse Wave

Debug overlay shows raw `lx`, `ly`, `lt`, `rt`, button mask, FPS, live enemies, and input mode.

## GPIO arcade I/O

Default compile-time macros: `ENABLE_ARCADE_IO=1`, `ENABLE_ADC_AIM=0`, `ENABLE_NEOPIXEL_FEEDBACK=0`.

| Pin | Direction | Use |
| --- | --- | --- |
| GPIO6 | output active-high | recoil pulse on shot |
| GPIO7 | output active-high | muzzle LED/effect pulse on shot |
| GPIO10 | input pull-up | service button to GND |
| GPIO4 | input pull-up | start button / onboard Button 2 fallback |
| GPIO5 | input pull-up | coin button / onboard Button 3 fallback |
| GPIO29 | output inverted | red status LED, LOW = on |
| GPIO40 / ADC0 / A0 | analog input | optional pan aim |
| GPIO41 / ADC1 / A1 | analog input | optional tilt aim |

**Electrical safety:** Fruit Jam GPIO pins must never drive solenoids, motors, coils, or high-current LEDs directly. Use active-high GPIO only into a logic-level MOSFET gate driver, an external load supply, a flyback diode for inductive loads, and a common ground. Verify GPIO6/GPIO7 with an LED or meter before connecting driver hardware.

Enable ADC aiming by configuring with `cmake -S . -B build -DENABLE_ADC_AIM=ON`. A0 maps to pan and A1 maps to tilt with simple two-point constants in `src/native_io.c`; the stick can still nudge/override aim.

## Test checklist

1. Flash `build/fruitjam_railshooter.uf2` to Fruit Jam.
2. Connect HDMI/DVI monitor.
3. Connect Xbox 360 USB controller.
4. Boot with controller plugged in.
5. Confirm title screen appears.
6. Press Start.
7. Confirm left stick moves crosshair.
8. Confirm RT fires.
9. Confirm B or LT reloads.
10. Confirm the reload bar takes time to complete and ammo returns to eight.
11. Confirm enemy projectiles can be shot before they hit the shield.
12. Fill the blue meter and confirm Y launches a Pulse Wave.
13. Confirm score, combo multiplier, sector progress, pickups, and boss health update.
14. Confirm GPIO6/GPIO7 pulse using LED/test meter before connecting real driver hardware.
15. Confirm no crash if no controller is connected.

## Known limitations

- All gameplay pools and procedural art are fixed-size and allocation-free to protect the 60 Hz firmware loop.
- ADC aiming is compiled but disabled by default.
- Controller disconnect state is inferred from the host stack callbacks; raw pad state remains last-known until new reports arrive.
