// MakeCode / pxt-microbit TypeScript
const ESP32_ADDR = 0x42
const FRAME_LEN = 10

const CMD_RUMBLE = 0x01
const CMD_COLOR = 0x02

let isWriting = false
let isReading = false

// Full Button Bitmasks
const BTN_A = 1 << 0
const BTN_B = 1 << 1 
const BTN_X = 1 << 2
const BTN_Y = 1 << 3
const BTN_L1 = 1 << 4
const BTN_R1 = 1 << 5
const BTN_L2 = 1 << 6
const BTN_R2 = 1 << 7
const BTN_THUMBL = 1 << 8
const BTN_THUMBR = 1 << 9
const BTN_SELECT = 1 << 10
const BTN_START = 1 << 11
const BTN_SYSTEM = 1 << 12

// Global Controller State Storage
namespace ControllerState {
    export let connected = 0
    export let buttons = 0  // Combined 16-bit state
    export let dpad = 0
    export let leftX = 0
    export let leftY = 0
    export let rightX = 0
    export let rightY = 0
    export let brake = 0
    export let throttle = 0
}

function sendCommand(cmd: number, rumbleLeft: number, rumbleRight: number, r: number, g: number, b: number): void {
    let timeoutCounter = 0
    while (isReading) {
        basic.pause(1)
        timeoutCounter++
        if (timeoutCounter > 50) {
            isReading = false
            break
        }
    }

    isWriting = true
    let buf = pins.createBuffer(6)
    buf.setNumber(NumberFormat.UInt8LE, 0, cmd)
    buf.setNumber(NumberFormat.UInt8LE, 1, rumbleLeft)
    buf.setNumber(NumberFormat.UInt8LE, 2, rumbleRight)
    buf.setNumber(NumberFormat.UInt8LE, 3, r)
    buf.setNumber(NumberFormat.UInt8LE, 4, g)
    buf.setNumber(NumberFormat.UInt8LE, 5, b)
    pins.i2cWriteBuffer(ESP32_ADDR, buf, false)
    isWriting = false
}

function readPad(): number[] {
    if (isWriting) return []
    isReading = true

    let buf = pins.i2cReadBuffer(ESP32_ADDR, FRAME_LEN, false)
    let result: number[] = []

    if (buf && buf.length == FRAME_LEN) {
        result.push(buf.getNumber(NumberFormat.UInt8LE, 0))
        result.push(buf.getNumber(NumberFormat.UInt8LE, 1))
        result.push(buf.getNumber(NumberFormat.UInt8LE, 2))
        result.push(buf.getNumber(NumberFormat.UInt8LE, 3))
        result.push(buf.getNumber(NumberFormat.Int8LE, 4))
        result.push(buf.getNumber(NumberFormat.Int8LE, 5))
        result.push(buf.getNumber(NumberFormat.Int8LE, 6))
        result.push(buf.getNumber(NumberFormat.Int8LE, 7))
        result.push(buf.getNumber(NumberFormat.UInt8LE, 8))
        result.push(buf.getNumber(NumberFormat.UInt8LE, 9))
    }
    isReading = false
    return result
}

function isButtonPressed(buttonMask: number): boolean {
    return (ControllerState.buttons & buttonMask) !== 0
}

// ==========================================
// THREAD 1: Dedicated I2C Data Intake Loop
// ==========================================
basic.forever(function () {
    let pad = readPad()
    if (pad.length < FRAME_LEN) {
        basic.pause(5)
        return
    }

    // Atomically stream fresh states into globals
    ControllerState.connected = pad[0]
    ControllerState.buttons = pad[1] | (pad[2] << 8)
    ControllerState.dpad = pad[3]
    ControllerState.leftX = pad[4]
    ControllerState.leftY = pad[5]
    ControllerState.rightX = pad[6]
    ControllerState.rightY = pad[7]
    ControllerState.brake = pad[8]
    ControllerState.throttle = pad[9]

    basic.pause(15) // Clean 15ms sampling interval 
})

// ==========================================
// THREAD 2: Independent Display Renderer Loop
// ==========================================
control.inBackground(function () {
    while (true) {
        if (ControllerState.connected == 1) {
            led.plot(0, 0)

            // Check A Button
            if (isButtonPressed(BTN_A)) {
                led.plot(2, 2)
            } else {
                led.unplot(2, 2)
            }

            // Check D-Pad Up
            if ((ControllerState.dpad & 0x01) !== 0) {
                led.plot(2, 0)
            } else {
                led.unplot(2, 0)
            }

            // Check Left Analog Stick Y (North)
            if (ControllerState.leftY < -20) {
                led.plot(0, 2)
            } else {
                led.unplot(0, 2)
            }

        } else {
            basic.clearScreen()
            led.plot(4, 4)
        }

        // Display update pacing rate (runs at ~33 FPS)
        basic.pause(30)
    }
})

// Async Outbound Triggers
input.onButtonPressed(Button.A, function () {
    sendCommand(CMD_RUMBLE, 200, 200, 0, 0, 0)
    basic.pause(50)
    sendCommand(CMD_COLOR, 0, 0, randint(0, 255), randint(0, 255), randint(0, 255))
})