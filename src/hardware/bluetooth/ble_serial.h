/**
 * BLE UART Serial Bridge
 *
 * Implements Nordic UART Service (NUS) to bridge Bluetooth to serial commands.
 * Uses existing serial protocol - no changes needed to command handling!
 *
 * Compatible with:
 * - BLE terminal apps (nRF Connect, Serial Bluetooth Terminal, etc.)
 * - meshgrid-cli with --bluetooth flag
 * - Custom Flutter apps
 */

#ifndef MESHGRID_BLE_SERIAL_H
#define MESHGRID_BLE_SERIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize BLE UART service
 *
 * Creates Nordic UART Service with standard UUIDs:
 * - Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * - RX Char: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (app writes)
 * - TX Char: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (app reads)
 *
 * @param device_name BLE device name (e.g., "meshgrid-12")
 * @return 0 on success, -1 on error
 */
int ble_serial_init(const char* device_name);

/**
 * Process BLE events
 * Call this periodically from main loop
 */
void ble_serial_process(void);

/**
 * Check if BLE client is connected
 * @return true if connected, false otherwise
 */
bool ble_serial_connected(void);

/**
 * Send data to BLE client (like Serial.print)
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent
 */
int ble_serial_write(const uint8_t* data, size_t len);

/**
 * Get number of bytes available to read
 * @return Number of bytes in RX buffer
 */
int ble_serial_available(void);

/**
 * Read one byte from BLE RX buffer
 * @return Byte read, or -1 if no data
 */
int ble_serial_read(void);

#ifdef __cplusplus
}
#endif

#endif // MESHGRID_BLE_SERIAL_H
