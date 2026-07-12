# DualSense → ESP32 → micro:bit Bridge

This code is the ESP32 code of a project to allow the use of a PS5/DualSense controller with a BBC Microbit, using an ESP32 Wroom as an i2c slave and bluetooth receiver for the controller

World's First: BBC microbit Control with DualSense! This repository presents a groundbreaking solution for controlling a BBC microbit with a PlayStation 5 DualSense controller. Here's what makes it unique:

Pioneering Functionality: This project establishes itself as the first of its kind, enabling the microbit to leverage the advanced features of the DualSense controller. ESP32 Bridge: The ESP32 Wroom microcontroller equipped with BluePad32 firmware acts as a bridge, seamlessly translating DualSense inputs. Efficient Communication: I2C protocol facilitates efficient data exchange between the ESP32 and the BBC microbit, ensuring smooth and responsive control. This project unlocks exciting possibilities for microbit applications in various fields, from robotics and game development to interactive installations.

Connects a PS5 DualSense controller to an ESP32 over Bluetooth, then exposes
the controller state to a BBC micro:bit over I2C. The micro:bit can also
send commands back to rumble the controller and change its light bar color.

## How it works

```
PS5 DualSense  --(Bluetooth, Bluepad32)-->  ESP32 (I2C slave)  <--(I2C)-->  micro:bit (I2C master)
```

- The ESP32 uses the **Bluepad32** library to pair with and read the
  DualSense over Bluetooth.
- The ESP32 also runs as an **I2C slave** on address `0x42`. It packs the
  latest controller state into a fixed 10-byte struct and sends it whenever
  the micro:bit reads from it.
- The micro:bit is the **I2C master**. It polls the ESP32 for pad state, and
  can write a 6-byte command frame back to trigger rumble or change the
  controller's LED color.

## Files

| File | Runs on | Purpose |
|---|---|---|
| `arduino-ide.ino` | ESP32 (Arduino IDE) | Reads the DualSense via Bluepad32, serves state over I2C, applies rumble/color commands |
| `microbit_ps5_i2c_read.ts` | micro:bit (MakeCode) | Reads pad state over I2C, sends rumble/color commands |

## Hardware

- ESP32 DOIT DevKit V1 (or similar ESP32 dev board)
- BBC micro:bit (v2 recommended)
- PS5 DualSense controller
- 2x 4.7kΩ resistors (I2C pull-ups, if your boards don't already have them)

## Wiring

Both boards run I2C at 3.3V logic, so they can be wired directly together.

| ESP32 | micro:bit | Notes |
|---|---|---|
| GPIO21 (SDA) | P20 | Add 4.7kΩ pull-up to 3.3V if not already present |
| GPIO22 (SCL) | P19 | Add 4.7kΩ pull-up to 3.3V if not already present |
| GND | GND | Common ground, required |

**Do not** power the micro:bit from the ESP32's 3.3V pin (or vice versa)
unless you've checked the current budget — power each board separately
over USB and just share GND/SDA/SCL.

## ESP32 setup (Arduino IDE)

Bluepad32 ships as its own ESP32 board package, not a regular library.

1. **File → Preferences → Additional Board Manager URLs**, add:
   ```
   https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
   ```
2. **Tools → Board → Boards Manager**, search `esp32_bluepad32`, install it.
3. **Tools → Board**, select a board from the **Bluepad32** section (e.g.
   "ESP32 Dev Module" — this covers the DOIT DevKit V1).
4. Open `arduino-ide.ino` and upload.

### Pairing the DualSense

Hold **Create + PS** on the controller until the light bar flashes rapidly.
Bluepad32 remembers the pairing across ESP32 reboots.

## micro:bit setup (MakeCode)

1. Open [makecode.microbit.org](https://makecode.microbit.org), create a new
   project.
2. Switch to the JavaScript editor (the `{ }` icon).
3. Paste in `microbit_ps5_i2c_read.ts`.
4. Download/flash to the micro:bit as usual.

## I2C protocol

### ESP32 → micro:bit (pad state, 10 bytes, sent on every read)

| Byte | Field | Notes |
|---|---|---|
| 0 | `connected` | 0 or 1 |
| 1 | `buttons_lo` | Low byte of button bitmask |
| 2 | `buttons_hi` | High byte of button bitmask |
| 3 | `dpad` | D-pad state |
| 4 | `leftX` | int8, -127..127 |
| 5 | `leftY` | int8, -127..127 |
| 6 | `rightX` | int8, -127..127 |
| 7 | `rightY` | int8, -127..127 |
| 8 | `brake` | L2 analog, 0..255 |
| 9 | `throttle` | R2 analog, 0..255 |

Button bitmask (unverified against your exact Bluepad32 version — confirm
by watching serial output while pressing one button at a time):

```
bit0 = Cross      bit1 = Circle     bit2 = Square     bit3 = Triangle
bit4 = L1         bit5 = R1         bit6 = L2 (digital) bit7 = R2 (digital)
bit8 = ThumbL     bit9 = ThumbR     bit10 = Share       bit11 = Options
```

### micro:bit → ESP32 (command frame, 6 bytes, written by micro:bit)

| Byte | Field | Notes |
|---|---|---|
| 0 | `cmd` | Bitmask: bit0 = apply rumble, bit1 = apply color |
| 1 | `rumbleLeft` | 0..255 |
| 2 | `rumbleRight` | 0..255 |
| 3 | `r` | Light bar red, 0..255 |
| 4 | `g` | Light bar green, 0..255 |
| 5 | `b` | Light bar blue, 0..255 |

The ESP32 latches this in the I2C receive callback and applies it in the
main loop, to avoid blocking the Bluetooth stack from inside the interrupt.

## Failsafe behaviour

If the DualSense disconnects cleanly, or if no fresh Bluepad32 data has
arrived for 500ms (silent Bluetooth dropout), the ESP32 zeroes out the
entire pad state before the micro:bit can act on stale button/stick values.

## Known issues / things to verify

- **Button bitmask** is based on typical Bluepad32 convention, not verified
  against your specific firmware version — confirm with serial output
  before relying on it.
- **ESP32 running hot**: under investigation. Likely candidates are the
  continuous Bluetooth Classic radio use (some warmth is normal for this),
  or heat from the onboard 3.3V regulator rather than the chip itself if
  you're powering over USB. Not yet confirmed root cause.
- `dumpGamepad()` serial debug prints on every Bluepad32 update — fine for
  bring-up, but worth removing or throttling with a `millis()` check once
  you've confirmed the button mapping, since it runs 100+ times/second.
