/*
 * PS5 (DualSense) -> ESP32 (Bluepad32) -> I2C slave -> BBC micro:bit
 *
 * The ESP32 keeps doing what it did before (reading the PS5 controller
 * over Bluetooth via Bluepad32), but now also acts as an I2C SLAVE.
 * The micro:bit is the I2C MASTER and just reads a fixed-size block of
 * bytes whenever it wants the latest controller state.
 *
 * WIRING (3.3V logic both sides, safe to connect directly):
 *   ESP32 GPIO21 (SDA) -- micro:bit P20 (SDA)
 *   ESP32 GPIO22 (SCL) -- micro:bit P19 (SCL)
 *   ESP32 GND          -- micro:bit GND
 * Add 4.7k pull-up resistors from SDA and SCL to 3.3V if you don't
 * already have them (the micro:bit edge connector doesn't reliably
 * supply them for external I2C devices).
 *
 * Same Arduino IDE board setup as before (esp32_bluepad32 package).
 */

#include <Bluepad32.h>
#include <Wire.h>

#define I2C_SLAVE_ADDR 0x42
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define FAILSAFE_TIMEOUT_MS 500 // no fresh data for this long -> zero out

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// ---- Shared state sent over I2C ----
// Keep this a plain packed struct so both sides agree on byte layout.
struct __attribute__((packed)) PadState {
  uint8_t connected;   // 0/1
  uint8_t buttons_lo;  // ctl->buttons() low byte
  uint8_t buttons_hi;  // ctl->buttons() high byte
  uint8_t dpad;        // ctl->dpad()
  int8_t  leftX;       // -127..127
  int8_t  leftY;
  int8_t  rightX;
  int8_t  rightY;
  uint8_t brake;       // L2, 0..255
  uint8_t throttle;    // R2, 0..255
};

volatile PadState padState = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long lastUpdateMs = 0;

// ---- Command frame received FROM the micro:bit (rumble + LED color) ----
struct __attribute__((packed)) CommandFrame {
  uint8_t cmd;         // bit0 = apply rumble, bit1 = apply color
  uint8_t rumbleLeft;  // 0..255
  uint8_t rumbleRight; // 0..255
  uint8_t r;
  uint8_t g;
  uint8_t b;
};
#define CMD_RUMBLE 0x01
#define CMD_COLOR  0x02

volatile CommandFrame pendingCommand = {0, 0, 0, 0, 0, 0};
volatile bool hasPendingCommand = false;

// Helper: scale Bluepad32's ~ -512..512 stick range down to int8
int8_t scaleAxis(int32_t v) {
  int32_t scaled = v / 4; // -512..512 -> roughly -128..128
  if (scaled > 127) scaled = 127;
  if (scaled < -127) scaled = -127;
  return (int8_t)scaled;
}

// Helper: Bluepad32 throttle/brake are already roughly 0..1023 on most pads
uint8_t scaleTrigger(int32_t v) {
  int32_t scaled = v / 4;
  if (scaled > 255) scaled = 255;
  if (scaled < 0) scaled = 0;
  return (uint8_t)scaled;
}

void dumpGamepad(ControllerPtr ctl) {
  Serial.printf(
      "idx=%d, dpad=0x%02x, buttons=0x%04x, axis L=%4d,%4d R=%4d,%4d, "
      "brake=%4d, throttle=%4d, misc=0x%02x, gyro=%d,%d,%d, "
      "accel=%d,%d,%d\n",
      ctl->index(), ctl->dpad(), ctl->buttons(), ctl->axisX(), ctl->axisY(),
      ctl->axisRX(), ctl->axisRY(), ctl->brake(), ctl->throttle(),
      ctl->miscButtons(), ctl->gyroX(), ctl->gyroY(), ctl->gyroZ(),
      ctl->accelX(), ctl->accelY(), ctl->accelZ());
}

void failsafeZero() {
  padState.connected  = 0;
  padState.buttons_lo = padState.buttons_hi = padState.dpad = 0;
  padState.leftX = padState.leftY = padState.rightX = padState.rightY = 0;
  padState.brake = padState.throttle = 0;
}

// ---- I2C slave callbacks ----
void onI2CRequest() {
  // Send the whole struct in one go; micro:bit just does a plain read.
  Wire.write((const uint8_t*)&padState, sizeof(PadState));
}

void onI2CReceive(int numBytes) {
  // Expecting a 6-byte CommandFrame from the micro:bit. Just latch the
  // bytes here - do NOT call ctl->setRumble()/setColorLED() from inside
  // this callback, since those can block on the Bluetooth stack.
  uint8_t buf[sizeof(CommandFrame)];
  int i = 0;
  while (Wire.available()) {
    uint8_t b = Wire.read();
    if (i < (int)sizeof(CommandFrame)) {
      buf[i] = b;
    }
    i++;
  }

  if (i >= (int)sizeof(CommandFrame)) {
    memcpy((void*)&pendingCommand, buf, sizeof(CommandFrame));
    hasPendingCommand = true;
  }
}

void applyPendingCommand() {
  if (!hasPendingCommand) return;

  CommandFrame cmdCopy;
  memcpy(&cmdCopy, (const void*)&pendingCommand, sizeof(CommandFrame));
  hasPendingCommand = false;

  for (auto ctl : myControllers) {
    if (ctl && ctl->isConnected()) {
      if (cmdCopy.cmd & CMD_RUMBLE) {
        ctl->setRumble(cmdCopy.rumbleLeft, cmdCopy.rumbleRight);
      }
      if (cmdCopy.cmd & CMD_COLOR) {
        ctl->setColorLED(cmdCopy.r, cmdCopy.g, cmdCopy.b);
      }
    }
  }
}

// ---- Bluepad32 callbacks ----
void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      Serial.printf("Controller connected, slot %d\n", i);
      myControllers[i] = ctl;
      ctl->setColorLED(0, 255, 0);
      padState.connected = 1;
      lastUpdateMs = millis();
      break;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      Serial.printf("Controller disconnected from slot %d\n", i);
      myControllers[i] = nullptr;
      failsafeZero();
      break;
    }
  }
}

void updatePadState(ControllerPtr ctl) {
  padState.connected  = 1;
  uint16_t buttons    = ctl->buttons();
  padState.buttons_lo = buttons & 0xFF;
  padState.buttons_hi = (buttons >> 8) & 0xFF;
  padState.dpad       = ctl->dpad();
  padState.leftX      = scaleAxis(ctl->axisX());
  padState.leftY      = scaleAxis(ctl->axisY());
  padState.rightX     = scaleAxis(ctl->axisRX());
  padState.rightY     = scaleAxis(ctl->axisRY());
  padState.brake      = scaleTrigger(ctl->brake());
  padState.throttle   = scaleTrigger(ctl->throttle());

  lastUpdateMs = millis();

  dumpGamepad(ctl); // debug print - comment out or throttle if too noisy
}

void processControllers() {
  for (auto ctl : myControllers) {
    if (ctl && ctl->isConnected() && ctl->hasData() && ctl->isGamepad()) {
      updatePadState(ctl);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Bluepad32 firmware: %s\n", BP32.firmwareVersion());

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);

  Wire.begin((uint8_t)I2C_SLAVE_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  Wire.onRequest(onI2CRequest);
  Wire.onReceive(onI2CReceive);

  Serial.printf("I2C slave ready on address 0x%02X (SDA=%d, SCL=%d)\n",
                I2C_SLAVE_ADDR, I2C_SDA_PIN, I2C_SCL_PIN);
}

void loop() {
  bool dataUpdated = BP32.update();
  if (dataUpdated) {
    processControllers();
  }

  if (padState.connected && (millis() - lastUpdateMs > FAILSAFE_TIMEOUT_MS)) {
    failsafeZero();
  }

  applyPendingCommand();

  delay(10);
}
