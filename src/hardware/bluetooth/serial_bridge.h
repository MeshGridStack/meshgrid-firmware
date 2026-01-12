/**
 * Serial Bridge - Unified serial I/O for USB and BLE
 *
 * Provides a unified interface to read from both USB Serial and BLE Serial,
 * and write to both when connected.
 */

#ifndef MESHGRID_SERIAL_BRIDGE_H
#define MESHGRID_SERIAL_BRIDGE_H

#include <Arduino.h>

/**
 * Check if data is available from any serial source (USB or BLE)
 * @return Number of bytes available
 */
int serial_bridge_available(void);

/**
 * Read one byte from serial sources (USB first, then BLE)
 * @return Byte read, or -1 if no data
 */
int serial_bridge_read(void);

/**
 * Write data to all connected serial outputs (USB + BLE)
 * @param data Data to write
 * @param len Length of data
 */
void serial_bridge_write(const uint8_t *data, size_t len);

/**
 * Print string to all connected serial outputs
 * @param str String to print
 */
void serial_bridge_print(const char *str);

/**
 * Print string with newline to all connected serial outputs
 * @param str String to print
 */
void serial_bridge_println(const char *str);

#endif // MESHGRID_SERIAL_BRIDGE_H
