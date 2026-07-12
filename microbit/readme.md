Here is the technical breakdown of the micro:bit code implementation and the complete bitmask reference table.

### 🧠 Code Logic Breakdown

The micro:bit handles the incoming data through two primary mechanisms: **Bitwise Reassembly** and **Isolated Thread Execution**.

#### 1. Bitwise Reassembly (Merging the Bytes)

The ESP32 splits the controller's button states across two separate 8-bit registers (`buttons_lo` at byte index 1, and `buttons_hi` at byte index 2). To evaluate these efficiently, the micro:bit merges them into a single 16-bit unsigned integer using a bitwise left-shift (`<<`) and a bitwise OR (`|`):

```typescript
ControllerState.buttons = pad[1] | (pad[2] << 8)

```

* **`(pad[2] << 8)`**: Shifts the bits of the high byte 8 positions to the left, moving them into positions 8 through 15.
* **`| pad[1]`**: Combines the low byte into positions 0 through 7 without altering the shifted high byte.

#### 2. Thread Separation

* **I2C Intake Loop (`basic.forever`)**: Runs continuously every 15ms. Its sole job is to grab the 10-byte buffer from the I2C bus, unpack it, update the `ControllerState` global variables, and exit.
* **Display Renderer Loop (`control.inBackground`)**: Runs asynchronously every 30ms (~33 FPS). It reads the data directly from the global memory variables. Because it does not touch the physical I2C lines, any delays caused by screen updates will never stall the incoming controller data stream.

---

### 📊 Button Mask Values

The following table details the binary bitwise shifts, hexadecimal values, and standard button mappings used by the `isButtonPressed()` helper function.

| Mask Constant | Bit Shift Expression | Hexadecimal Mask | Decimal Value | Standard Controller Mapping |
| --- | --- | --- | --- | --- |
| **`BTN_A`** | `1 << 0` | `0x0001` | `1` | Cross (PS) / A (Xbox) / B (Switch) |
| **`BTN_B`** | `1 << 1` | `0x0002` | `2` | Circle (PS) / B (Xbox) / A (Switch) |
| **`BTN_X`** | `1 << 2` | `0x0004` | `4` | Square (PS) / X (Xbox) / Y (Switch) |
| **`BTN_Y`** | `1 << 3` | `0x0008` | `8` | Triangle (PS) / Y (Xbox) / X (Switch) |
| **`BTN_L1`** | `1 << 4` | `0x0010` | `16` | L1 Bumper / Left Bumper |
| **`BTN_R1`** | `1 << 5` | `0x0020` | `32` | R1 Bumper / Right Bumper |
| **`BTN_L2`** | `1 << 6` | `0x0040` | `64` | Left Trigger (Digital Threshold) |
| **`BTN_R2`** | `1 << 7` | `0x0080` | `128` | Right Trigger (Digital Threshold) |
| **`BTN_THUMBL`** | `1 << 8` | `0x0100` | `256` | Left Stick Press (L3 Click) |
| **`BTN_THUMBR`** | `1 << 9` | `0x0200` | `512` | Right Stick Press (R3 Click) |
| **`BTN_SELECT`** | `1 << 10` | `0x0400` | `1024` | Create / Share / Select / Minus |
| **`BTN_START`** | `1 << 11` | `0x0800` | `2048` | Options / Menu / Start / Plus |
| **`BTN_SYSTEM`** | `1 << 12` | `0x1000` | `4096` | PS Button / Xbox Guide / Home |

> **Note on D-Pad:** The D-Pad is omitted from this 16-bit integer mask because the incoming struct places it inside its own dedicated byte (`pad[3]`). It uses an isolated 4-bit layout (`0x01` = Up, `0x02` = Down, `0x04` = Left, `0x08` = Right).
