#include <Arduino.h>
#include <ps5.h>
#include <Wire.h>

#define I2C_DEV_ADDR 0x42

struct GamepadState {
    uint32_t buttons;
    uint8_t lStickX;
    uint8_t lStickY;
    uint8_t rStickX;
    uint8_t rStickY;
    uint8_t l2Value;
    uint8_t r2Value;
    uint8_t battery;
} __attribute__((packed));

GamepadState currentState = {0, 128, 128, 128, 128, 0, 0, 0};

// Bitmasks for micro:bit matching
#define MASK_CROSS         (1 << 0)
#define MASK_CIRCLE        (1 << 1)
#define MASK_SQUARE        (1 << 2)
#define MASK_TRIANGLE      (1 << 3)
#define MASK_DPAD_UP       (1 << 4)
#define MASK_DPAD_DOWN     (1 << 5)
#define MASK_DPAD_LEFT     (1 << 6)
#define MASK_DPAD_RIGHT    (1 << 7)
#define MASK_L1            (1 << 8)
#define MASK_R1            (1 << 9)
#define MASK_TOUCHPAD      (1 << 10)

void onI2CRequest() {
    Wire.write((uint8_t*)&currentState, sizeof(GamepadState));
}

void setup() {
    // Default Wire.begin() on original ESP32 automatically uses GPIO 21 & 22
    Wire.begin(I2C_DEV_ADDR);
    Wire.onRequest(onI2CRequest);
    
    // Replace with your DualSense MAC address
    ps5.begin("1a:2b:3c:01:01:01"); 
}

void loop() {
    if (ps5.isConnected()) {
        uint32_t tempButtons = 0;

        if (ps5.Cross())    tempButtons |= MASK_CROSS;
        if (ps5.Circle())   tempButtons |= MASK_CIRCLE;
        if (ps5.Square())   tempButtons |= MASK_SQUARE;
        if (ps5.Triangle()) tempButtons |= MASK_TRIANGLE;
        if (ps5.Up())       tempButtons |= MASK_DPAD_UP;
        if (ps5.Down())     tempButtons |= MASK_DPAD_DOWN;
        if (ps5.Left())     tempButtons |= MASK_DPAD_LEFT;
        if (ps5.Right())    tempButtons |= MASK_DPAD_RIGHT;
        if (ps5.L1())       tempButtons |= MASK_L1;
        if (ps5.R1())       tempButtons |= MASK_R1;
        if (ps5.Touchpad()) tempButtons |= MASK_TOUCHPAD;

        currentState.buttons = tempButtons;

        // Convert native [-128, 127] sticks to standard unsigned [0, 255]
        currentState.lStickX = (uint8_t)(ps5.LStickX() + 128);
        currentState.lStickY = (uint8_t)(ps5.LStickY() + 128);
        currentState.rStickX = (uint8_t)(ps5.RStickX() + 128);
        currentState.rStickY = (uint8_t)(ps5.RStickY() + 128);

        currentState.l2Value = ps5.L2Value();
        currentState.r2Value = ps5.R2Value();
        currentState.battery = ps5.Battery();
    } else {
        currentState.buttons = 0;
        currentState.lStickX = 128; currentState.lStickY = 128;
        currentState.rStickX = 128; currentState.rStickY = 128;
        currentState.l2Value = 0;   currentState.r2Value = 0;
        currentState.battery = 0;
    }
    delay(10); 
}