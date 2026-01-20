/**
 * SerialBridge Implementation
 */

#include "SerialBridge.h"
#include "ble_serial.h"

size_t SerialBridgeClass::write(uint8_t c) {
    /* Always write to USB Serial */
    size_t written = Serial.write(c);

#ifdef ENABLE_BLE
    /* Also write to BLE if connected */
    if (ble_serial_connected()) {
        ble_serial_write(&c, 1);
    }
#endif

    return written;
}

size_t SerialBridgeClass::write(const uint8_t* buffer, size_t size) {
    /* Always write to USB Serial */
    size_t written = Serial.write(buffer, size);

#ifdef ENABLE_BLE
    /* Also write to BLE if connected */
    if (ble_serial_connected()) {
        ble_serial_write(buffer, size);
    }
#endif

    return written;
}

int SerialBridgeClass::available() {
    /* Check USB Serial first */
    int usb = Serial.available();
    if (usb > 0)
        return usb;

#ifdef ENABLE_BLE
    /* Then check BLE Serial */
    return ble_serial_available();
#else
    return 0;
#endif
}

int SerialBridgeClass::read() {
    /* Prioritize USB Serial */
    if (Serial.available()) {
        return Serial.read();
    }

#ifdef ENABLE_BLE
    /* Then BLE Serial */
    if (ble_serial_available()) {
        return ble_serial_read();
    }
#endif

    return -1;
}

int SerialBridgeClass::peek() {
    /* Peek USB Serial */
    if (Serial.available()) {
        return Serial.peek();
    }

#ifdef ENABLE_BLE
    /* BLE doesn't have peek, would need to implement */
    /* For now, return -1 */
#endif

    return -1;
}

/* Global instance */
SerialBridgeClass SerialBridge;
