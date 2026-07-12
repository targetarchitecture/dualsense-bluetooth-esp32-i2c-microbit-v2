An updated version of the main `README.md` has been rewritten below. This version corrects inaccuracies regarding file naming conventions, data structures, and the verified button mapping directly extracted from the ESP32 firmware and micro:bit TypeScript implementation.

---

# DualSense → ESP32 → micro:bit Bridge

This repository provides an firmware framework to bridge a PlayStation 5 DualSense controller with a BBC micro:bit. An ESP32 Wroom module acts as the Bluetooth host receiver and an I2C slave, passing real-time controller states to a BBC micro:bit running as the I2C master.

The system supports full bidirectional communication: reading the complete gamepad state (digital buttons, D-pad, dual analog sticks, and analog triggers) and sending outbound payload updates to drive dual-motor rumble and the RGB light bar.

## Architecture Overview

```
PS5 DualSense  --(Bluetooth Classic via Bluepad32)-->  ESP32 (I2C Slave: 0x42)  <--(I2C at 3.3V Logic)-->  micro:bit (I2C Master)

```

* **ESP32 Core:** Runs the **Bluepad32** platform to pair with the controller. It translates incoming Bluetooth inputs into a locally latched struct.
* **I2C Interface:** The ESP32 exposes this data on I2C address `0x42`. State transfers utilize a tightly packed 10-byte transaction window.
* **Asynchronous Command Trapping:** Actuator outputs (rumble intensity and RGB parameters) sent by the micro:bit are captured via I2C receive interrupts and processed safely within the main execution loop to prevent stalling the Bluetooth stack.

## Repository Structure

| File Path | Target Hardware | Execution Context / Role |
| --- | --- | --- |
| `esp32-arduino-ide/esp32-arduino-ide.ino` | ESP32 (e.g., DevKit V1) | Manages Bluetooth pairing, I2C slave event callbacks, scaling functions, and failsafe zeroing. |
| `microbit/pxt-microbit-ps5-dualsense-bridge.ts` | BBC micro:bit (v2) | Handles background I2C polling, bitwise reassembly of the state registers, and async control threads. |

## Hardware Configuration

* **Microcontrollers:** ESP32 DOIT DevKit V1 (or equivalent ESP32-WROOM-32 module) and a BBC micro:bit v2.
* **I2C Pull-Up Resistors:** Two 4.7kΩ resistors tied to the 3.3V rail. *(Note: Internal ESP32 pull-ups are programmatically enabled in code, but hardware pull-ups are highly recommended for stable signal line propagation over the micro:bit edge connector).*

### Pin Mapping

| ESP32 Pin | micro:bit Pin | Description |
| --- | --- | --- |
| **GPIO21 (SDA)** | **P20 (SDA)** | Shared I2C Data Line (3.3V Logic) |
| **GPIO22 (SCL)** | **P19 (SCL)** | Shared I2C Clock Line (3.3V Logic) |
| **GND** | **GND** | Common Ground Reference |

*Warning: Power both units independently via their respective USB interfaces. Do not cross-connect the 3.3V power rails unless you have calculated the current draw overhead of the ESP32's radio operations.*

## Data Layout Protocols

### 1. Gamepad State Frame (ESP32 → micro:bit)

**Size:** 10 bytes (Fixed-width packed struct). Polled sequentially by the micro:bit.

| Byte | Struct Field | Data Type | Operational Ranges / Interpretation |
| --- | --- | --- | --- |
| `0` | `connected` | `uint8_t` | `0` = Disconnected, `0x01` = Active/Paired |
| `1` | `buttons_lo` | `uint8_t` | Low byte of the unified 16-bit button bitmask |
| `2` | `buttons_hi` | `uint8_t` | High byte of the unified 16-bit button bitmask |
| `3` | `dpad` | `uint8_t` | Discrete 4-bit nibble layout (`0x01`=U, `0x02`=D, `0x04`=L, `0x08`=R) |
| `4` | `leftX` | `int8_t` | Left stick X-axis, scaled linearly from `-127` to `127` |
| `5` | `leftY` | `int8_t` | Left stick Y-axis, scaled linearly from `-127` to `127` |
| `6` | `rightX` | `int8_t` | Right stick X-axis, scaled linearly from `-127` to `127` |
| `7` | `rightY` | `int8_t` | Right stick Y-axis, scaled linearly from `-127` to `127` |
| `8` | `brake` | `uint8_t` | L2 analog pressure value, scaled from `0` to `255` |
| `9` | `throttle` | `uint8_t` | R2 analog pressure value, scaled from `0` to `255` |

#### Verified Button Bitmask Reference Table

When combined into a 16-bit word (`pad[1] | (pad[2] << 8)`) on the master node, specific indices are decoded via the following masks:

| Mask Constant | Shift Bit | Hex Mask | Physical Button Equivalent |
| --- | --- | --- | --- |
| `BTN_A` | `1 << 0` | `0x0001` | Cross ($\times$) |
| `BTN_B` | `1 << 1` | `0x0002` | Circle ($\bigcirc$) |
| `BTN_X` | `1 << 2` | `0x0004` | Square ($\square$) |
| `BTN_Y` | `1 << 3` | `0x0008` | Triangle ($\triangle$) |
| `BTN_L1` | `1 << 4` | `0x0010` | L1 Bumper |
| `BTN_R1` | `1 << 5` | `0x0020` | R1 Bumper |
| `BTN_L2` | `1 << 6` | `0x0040` | L2 Digital Threshold |
| `BTN_R2` | `1 << 7` | `0x0080` | R2 Digital Threshold |
| `BTN_THUMBL` | `1 << 8` | `0x0100` | L3 Stick Click |
| `BTN_THUMBR` | `1 << 9` | `0x0200` | R3 Stick Click |
| `BTN_SELECT` | `1 << 10` | `0x0400` | Create / Share Button |
| `BTN_START` | `1 << 11` | `0x0800` | Options Button |
| `BTN_SYSTEM` | `1 << 12` | `0x1000` | PlayStation (PS) Center Button |

### 2. Outbound Actuator Command Frame (micro:bit → ESP32)

**Size:** 6 bytes (Written directly to the peripheral address).

| Byte | Struct Field | Data Type | Purpose / Description |
| --- | --- | --- | --- |
| `0` | `cmd` | `uint8_t` | Execution bitmask flags: `0x01` = Apply Rumble, `0x02` = Apply Color |
| `1` | `rumbleLeft` | `uint8_t` | Left heavy rumble motor intensity (`0` to `255`) |
| `2` | `rumbleRight` | `uint8_t` | Right light rumble motor intensity (`0` to `255`) |
| `3` | `r` | `uint8_t` | Red LED light bar channel target (`0` to `255`) |
| `4` | `g` | `uint8_t` | Green LED light bar channel target (`0` to `255`) |
| `5` | `b` | `uint8_t` | Blue LED light bar channel target (`0` to `255`) |

## Firmware Operational Safeties

* **Active Drop Failsafe:** The ESP32 evaluates data fresh metrics continuously. If no configuration updates cross the Bluetooth threshold for `500ms`, the internal memory block zeroes out automatically. This drops stick alignments and prevents the micro:bit from continuing to act on stale inputs.
* **I2C Non-Blocking Protection:** Commands received from the micro:bit are captured via memory copies inside the I2C peripheral callback `onI2CReceive()`. Actuator state assignments are systematically routed downstream to `applyPendingCommand()` in the main loop to avoid hanging the time-critical Bluetooth stack inside the hardware interrupt handler.
