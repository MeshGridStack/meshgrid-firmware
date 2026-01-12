/**
 * Serial Output Abstraction
 *
 * Routes output to both USB Serial and BLE Serial (when BLE is enabled)
 * Use SerialOutput instead of Serial for all print/println calls
 */

#ifndef MESHGRID_SERIAL_OUTPUT_H
#define MESHGRID_SERIAL_OUTPUT_H

#ifdef ENABLE_BLE
/* BLE enabled: Output goes to both USB and BLE */
#include "hardware/bluetooth/SerialBridge.h"
#define SerialOutput SerialBridge
#else
/* No BLE: Output goes to USB Serial only */
#include <Arduino.h>
#define SerialOutput Serial
#endif

#endif // MESHGRID_SERIAL_OUTPUT_H
