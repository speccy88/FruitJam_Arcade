# Fruit Jam Native Rail Shooter

Native RP2350B firmware for Adafruit Fruit Jam using wili8jam's DVI, graphics, USB host/XInput, audio, and board setup. It boots directly into a 128x128 indexed-color wireframe rail-shooter and does **not** run Lua, PICO-8 carts, the console/editor, or the REPL.

## Build

```sh
git submodule update --init --recursive
cmake -S . -B build -G "Unix Makefiles"
cmake --build build --target fruitjam_railshooter -j
```

Expected output: `build/fruitjam_railshooter.uf2`. Copy the UF2 to the Fruit Jam BOOTSEL drive.

## Controls

- Xbox 360 left stick: move crosshair
- Right trigger or A: fire
- B or left trigger: reload
- Start: start / pause / resume
- Back/View or service: toggle debug overlay
- D-pad / wili8jam digital input: fallback movement/menu input

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

Enable ADC aiming by configuring the native target with `-DENABLE_ADC_AIM=1` or adding that definition to the target. A0 maps to pan and A1 maps to tilt with simple two-point constants in `src/native_io.c`; the stick can still nudge/override aim.

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
10. Confirm score increases when shooting enemies.
11. Confirm GPIO6/GPIO7 pulse using LED/test meter before connecting real driver hardware.
12. Confirm no crash if no controller is connected.

## Known limitations

- Wireframe art and audio are intentionally small/static to avoid allocation in the frame loop.
- ADC aiming is compiled but disabled by default.
- Controller disconnect state is inferred from the host stack callbacks; raw pad state remains last-known until new reports arrive.
