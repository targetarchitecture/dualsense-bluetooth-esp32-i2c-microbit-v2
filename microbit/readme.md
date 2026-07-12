# micro:bit DualSense Driver Extension

This directory contains the custom MakeCode TypeScript implementation for the BBC micro:bit v2. It configures the micro:bit to operate as an I2C master device, interfacing with the ESP32 bridge (running at I2C address `0x42`) to poll live controller states and transmit actuator telemetry.

## File Inventory

* `pxt-microbit-ps5-dualsense-bridge.ts`: The primary TypeScript driver file containing the data structures, bitmask definitions, I2C transactional loop, and public namespace blocks.

## Architecture & Integration

The extension manages background scheduling via `control.inBackground()` to poll the ESP32 periodically. The raw 10-byte buffer returned from the ESP32 is processed into local memory states:

```
+-------------------------------------------------------------+
|              micro:bit Background Polling Loop              |
+-------------------------------------------------------------+
                              |
                              v  (Polls I2C Addr 0x42 every 20ms)
+-------------------------------------------------------------+
|               Read 10-Byte Buffer via I2C                   |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|  Byte 0: Connection State                                   |
|  Bytes 1-2: 16-bit Button Bitmask Alignment                 |
|  Byte 3: 4-bit D-Pad Nibble Decoder                         |
|  Bytes 4-7: Scaled Joysticks (int8: -127 to 127)            |
|  Bytes 8-9: Analog Triggers (uint8: 0 to 255)               |
+-------------------------------------------------------------+

```

## API Reference

The extension exposes the `DualSense` namespace to MakeCode scripts.

### Core Initialization & State Loops

#### `DualSense.start()`

Initializes the driver and spawns the background polling thread. It polls the I2C bus every 20ms to update internal button masks and axis thresholds.

#### `DualSense.isControllerConnected(): boolean`

Returns `true` if the ESP32 has confirmed an active Bluetooth classic pairing session with a DualSense pad.

---

### Input Read Actions

#### `DualSense.buttonPressed(button: PS5Button): boolean`

Queries the state of digital switches using the unified 16-bit word filter. Supported parameters via `PS5Button` enum:

| Enum Value | Hardware Equivalent | Hex Mask |
| --- | --- | --- |
| `PS5Button.Cross` | Cross ($\times$) | `0x0001` |
| `PS5Button.Circle` | Circle ($\bigcirc$) | `0x0002` |
| `PS5Button.Square` | Square ($\square$) | `0x0004` |
| `PS5Button.Triangle` | Triangle ($\triangle$) | `0x0008` |
| `PS5Button.L1` | L1 Bumper | `0x0010` |
| `PS5Button.R1` | R1 Bumper | `0x0020` |
| `PS5Button.L2` | L2 Digital Threshold | `0x0040` |
| `PS5Button.R2` | R2 Digital Threshold | `0x0080` |
| `PS5Button.L3` | Left Stick Click | `0x0100` |
| `PS5Button.R3` | Right Stick Click | `0x0200` |
| `PS5Button.Create` | Create / Share | `0x0400` |
| `PS5Button.Options` | Options / Start | `0x0800` |
| `PS5Button.PS` | Center PlayStation Button | `0x1000` |

#### `DualSense.dpadPressed(direction: DPadDirection): boolean`

Decodes discrete directional pad actions utilizing the 4-bit nibble space:

* `DPadDirection.Up = 0x01`
* `DPadDirection.Down = 0x02`
* `DPadDirection.Left = 0x04`
* `DPadDirection.Right = 0x08`

#### `DualSense.getJoystick(axis: JoystickAxis): number`

Returns the stick offset configuration. The values are signed numbers scaled between `-127` and `127`.

* Axes: `JoystickAxis.LeftX`, `JoystickAxis.LeftY`, `JoystickAxis.RightX`, `JoystickAxis.RightY`

#### `DualSense.getTrigger(trigger: TriggerAxis): number`

Returns the absolute analog depth value for the trigger pots. Value ranges from `0` (rest) to `255` (fully compressed).

* Triggers: `TriggerAxis.L2`, `TriggerAxis.R2`

---

### Output Actuator Control Threads

#### `DualSense.setRumble(left: number, right: number)`

Dispatches a 6-byte control frame overriding the dual eccentric rotating mass (ERM) motors inside the pad handle grips.

* `left` (Heavy low-frequency weight): `0` to `255`
* `right` (Light high-frequency weight): `0` to `255`

#### `DualSense.setLEDColor(r: number, g: number, b: number)`

Alters the target color space mix emitted by the RGB light bars framing the touchpad periphery. Each channel argument must fall between `0` and `255`.

## Scripting Example

Below is a standard micro:bit initialization block utilizing the extension API to create a simple tank-drive output loop while changing LED color states on connection changes:

```typescript
// Spawn background thread monitoring and initialize hardware I2C lines
DualSense.start()

basic.forever(function () {
    if (DualSense.isControllerConnected()) {
        // Fetch raw stick positions (Signed -127 to 127)
        let driveSpeed = DualSense.getJoystick(JoystickAxis.LeftY)
        let steering = DualSense.getJoystick(JoystickAxis.RightX)

        // Drive Actuators based on button thresholds
        if (DualSense.buttonPressed(PS5Button.Cross)) {
            // Trigger maximum rumble feedback loop
            DualSense.setRumble(255, 255)
            // Flip lightbar to Red
            DualSense.setLEDColor(255, 0, 0)
        } else {
            DualSense.setRumble(0, 0)
            // Keep lightbar Cyan during typical operations
            DualSense.setLEDColor(0, 128, 255)
        }
    } else {
        // Visual indicator that bridge link is down
        basic.showIcon(IconNames.Sad)
    }
    
    // Throttle iteration execution times safely 
    basic.pause(20)
})

```
