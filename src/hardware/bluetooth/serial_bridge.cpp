/**
 * Serial Bridge Implementation
 */

#include "serial_bridge.h"
#include "ble_serial.h"

int serial_bridge_available(void) {
    int usb = Serial.available();
    int ble = ble_serial_available();
    return usb + ble;
}

int serial_bridge_read(void) {
    /* Prioritize USB Serial (for debugging/flashing compatibility) */
    if (Serial.available()) {
        return Serial.read();
    }

    /* Then check BLE Serial */
    if (ble_serial_available()) {
        return ble_serial_read();
    }

    return -1;
}

void serial_bridge_write(const uint8_t *data, size_t len) {
    /* Always write to USB Serial */
    Serial.write(data, len);

    /* Also write to BLE if connected */
    if (ble_serial_connected()) {
        ble_serial_write(data, len);
    }
}

void serial_bridge_print(const char *str) {
    serial_bridge_write((const uint8_t*)str, strlen(str));
}

void serial_bridge_println(const char *str) {
    serial_bridge_print(str);
    serial_bridge_write((const uint8_t*)"\n", 1);
}
