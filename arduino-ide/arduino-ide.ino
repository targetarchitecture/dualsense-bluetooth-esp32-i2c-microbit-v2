/*
 * PS5 (DualSense) controller connection for DOIT ESP32 DEVKIT V1
 * Using the Bluepad32 library (Bluetooth HID gamepad support).
 *
 * REQUIRED SETUP (Arduino IDE):
 * 1. File > Preferences > Additional Board Manager URLs, add:
 *    https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
 *    (you can have the standard espressif URL there too, doesn't matter)
 * 2. Tools > Board > Boards Manager > search "esp32_bluepad32" > install
 *    (this is a SEPARATE board package from the normal "esp32" one -
 *    it bundles Bluepad32 into the core, so you don't install Bluepad32
 *    as a library)
 * 3. Tools > Board > select an ESP32 board from the "Bluepad32" boards
 *    list (e.g. "ESP32 Dev Module" under the Bluepad32 section)
 * 4. Upload this sketch as normal
 *
 * Pairing:
 * - Put the DualSense in pairing mode: hold the Create button + PS
 *   button until the light bar flashes rapidly.
 * - On first boot the ESP32 will show as discoverable and the
 *   controller should connect. Bluepad32 remembers paired controllers
 *   across reboots (stored in flash).
 */

#include <Bluepad32.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

void onConnectedController(ControllerPtr ctl) {
  bool foundEmptySlot = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      Serial.printf("Controller connected, slot %d\n", i);
      ControllerProperties properties = ctl->getProperties();
      Serial.printf("Model: %s, VID=0x%04x, PID=0x%04x\n",
                    ctl->getModelName().c_str(), properties.vendor_id,
                    properties.product_id);
      myControllers[i] = ctl;
      foundEmptySlot = true;

      // Rumble briefly and flash the light bar to confirm connection
      ctl->setRumble(0x80 /* left */, 0x40 /* right */);
      ctl->setColorLED(0, 255, 0); // green light bar on DualSense
      break;
    }
  }
  if (!foundEmptySlot) {
    Serial.println("Controller connected, but no empty slot found");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      Serial.printf("Controller disconnected from slot %d\n", i);
      myControllers[i] = nullptr;
      break;
    }
  }
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

void processGamepad(ControllerPtr ctl) {
  // Example: react to button presses
  if (ctl->a()) {
    Serial.println("Cross (X) pressed");
  }
  if (ctl->b()) {
    Serial.println("Circle pressed");
  }
  if (ctl->x()) {
    Serial.println("Square pressed");
  }
  if (ctl->y()) {
    Serial.println("Triangle pressed");
  }

  // Left/right analog sticks: -512 to 512 roughly
  int32_t leftX = ctl->axisX();
  int32_t leftY = ctl->axisY();
  int32_t rightX = ctl->axisRX();
  int32_t rightY = ctl->axisRY();

  // Only print periodically to avoid flooding serial - here every call for
  // simplicity, throttle in loop() with millis() if it's too noisy
  dumpGamepad(ctl);

  (void)leftX; (void)leftY; (void)rightX; (void)rightY;
  // TODO: map these to your motors / servos / whatever you're driving
}

void processControllers() {
  for (auto ctl : myControllers) {
    if (ctl && ctl->isConnected() && ctl->hasData()) {
      if (ctl->isGamepad()) {
        processGamepad(ctl);
      } else {
        Serial.println("Unsupported controller type");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Bluepad32 firmware: %s\n", BP32.firmwareVersion());

  BP32.setup(&onConnectedController, &onDisconnectedController);

  // If you want to forget previously paired devices, uncomment:
  // BP32.forgetBluetoothKeys();

  // Enables mouse/gamepad/keyboard reports over BLE too, PS5 uses classic BT
  BP32.enableVirtualDevice(false);
}

void loop() {
  bool dataUpdated = BP32.update();
  if (dataUpdated) {
    processControllers();
  }

  delay(10); // small delay is fine, Bluepad32 runs its own BT task
}
