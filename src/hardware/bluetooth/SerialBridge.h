/**
 * SerialBridge - Unified Serial Output Wrapper
 *
 * Drop-in replacement for Arduino Serial that sends output to:
 * - USB Serial (always)
 * - BLE Serial (when connected, if BLE enabled)
 *
 * Usage: Replace SerialOutput.print() with SerialBridge.print()
 */

#ifndef MESHGRID_SERIAL_BRIDGE_CLASS_H
#define MESHGRID_SERIAL_BRIDGE_CLASS_H

#include <Arduino.h>
#include <Print.h>

class SerialBridgeClass : public Print {
public:
    /* Print interface - required */
    virtual size_t write(uint8_t c);
    virtual size_t write(const uint8_t* buffer, size_t size);

    /* Convenience methods (inherited from Print) */
    using Print::print;
    using Print::println;
    using Print::write; // pull in write(str) and write(buf, size)

    /* Input methods - read from USB or BLE */
    int available();
    int read();
    int peek();
};

/* Global instance - use like Serial */
extern SerialBridgeClass SerialBridge;

#endif // MESHGRID_SERIAL_BRIDGE_CLASS_H
