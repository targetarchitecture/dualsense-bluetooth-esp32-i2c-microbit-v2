// MakeCode / pxt-microbit TypeScript
// Reads the 10-byte PadState struct sent by the ESP32 I2C slave.
//
// Struct layout (must match the ESP32 sketch exactly):
// [0] connected
// [1] buttons_lo
// [2] buttons_hi
// [3] dpad
// [4] leftX   (int8)
// [5] leftY   (int8)
// [6] rightX  (int8)
// [7] rightY  (int8)
// [8] brake
// [9] throttle

const ESP32_ADDR = 0x42
const FRAME_LEN = 10

// Bluepad32 button bit meanings (buttons_lo/buttons_hi combined):
// bit0=A(Cross) bit1=B(Circle) bit2=X(Square) bit3=Y(Triangle)
// bit4=L1 bit5=R1 bit6=L2(digital) bit7=R2(digital)
// bit8=ThumbL bit9=ThumbR bit10=Select/Share bit11=Start/Options
// (Confirm on your setup: print buttons_lo/hi over serial from the ESP32
// and press one button at a time - mapping can vary slightly by version.)

function readPad(): number[] {
    let buf = pins.i2cReadBuffer(ESP32_ADDR, FRAME_LEN, false)
    let result: number[] = []
    result.push(buf.getNumber(NumberFormat.UInt8LE, 0))  // connected
    result.push(buf.getNumber(NumberFormat.UInt8LE, 1))  // buttons_lo
    result.push(buf.getNumber(NumberFormat.UInt8LE, 2))  // buttons_hi
    result.push(buf.getNumber(NumberFormat.UInt8LE, 3))  // dpad
    result.push(buf.getNumber(NumberFormat.Int8LE, 4))   // leftX
    result.push(buf.getNumber(NumberFormat.Int8LE, 5))   // leftY
    result.push(buf.getNumber(NumberFormat.Int8LE, 6))   // rightX
    result.push(buf.getNumber(NumberFormat.Int8LE, 7))   // rightY
    result.push(buf.getNumber(NumberFormat.UInt8LE, 8))  // brake
    result.push(buf.getNumber(NumberFormat.UInt8LE, 9))  // throttle
    return result
}

basic.forever(function () {
    let pad = readPad()
    let connected = pad[0]
    let buttonsLo = pad[1]
    let buttonsHi = pad[2]
    let dpad = pad[3]
    let leftX = pad[4]
    let leftY = pad[5]
    let rightX = pad[6]
    let rightY = pad[7]
    let brake = pad[8]
    let throttle = pad[9]

    if (connected == 1) {
        let crossPressed = (buttonsLo & 0x01) != 0
        if (crossPressed) {
            basic.showIcon(IconNames.Happy)
        } else if (leftY < -20) {
            basic.showArrow(ArrowNames.North)
        } else {
            basic.showIcon(IconNames.No)
        }
    } else {
        basic.showIcon(IconNames.Sad)
    }

    basic.pause(50)
})
