/**
 * BLE UART Serial Bridge Implementation
 *
 * Nordic UART Service (NUS) bridge to existing serial command handler
 */

#include "ble_serial.h"
#include "utils/debug.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(ENABLE_BLE)

#    include <Arduino.h>
#    include <BLEDevice.h>
#    include <BLEServer.h>
#    include <BLEUtils.h>
#    include <BLE2902.h>

/* Nordic UART Service UUIDs (standard - compatible with most BLE apps) */
#    define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#    define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#    define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

/* BLE state */
static BLEServer* ble_server = nullptr;
static BLECharacteristic* ble_tx_char = nullptr;
static BLECharacteristic* ble_rx_char = nullptr;
static bool ble_client_connected = false;
static bool ble_initialized = false;

/* RX buffer for incoming BLE data */
#    define BLE_RX_BUFFER_SIZE 512
static uint8_t ble_rx_buffer[BLE_RX_BUFFER_SIZE];
static volatile uint16_t ble_rx_head = 0;
static volatile uint16_t ble_rx_tail = 0;

/* Server callbacks */
class MyBLEServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        ble_client_connected = true;
        DEBUG_INFO("[BLE] Client connected");
    }

    void onDisconnect(BLEServer* pServer) {
        ble_client_connected = false;
        DEBUG_INFO("[BLE] Client disconnected");

        /* Restart advertising so clients can reconnect */
        delay(500);
        pServer->startAdvertising();
        DEBUG_INFO("[BLE] Advertising restarted");
    }
};

/* RX characteristic callback - receives data from app */
class BLERxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();

        if (value.length() > 0) {
            /* Add received data to RX buffer */
            for (size_t i = 0; i < value.length(); i++) {
                uint16_t next = (ble_rx_head + 1) % BLE_RX_BUFFER_SIZE;

                /* Check for buffer overflow */
                if (next != ble_rx_tail) {
                    ble_rx_buffer[ble_rx_head] = value[i];
                    ble_rx_head = next;
                } else {
                    DEBUG_INFO("[BLE] RX buffer overflow!");
                    break;
                }
            }
        }
    }
};

int ble_serial_init(const char* device_name) {
    if (ble_initialized) {
        DEBUG_INFO("[BLE] Already initialized");
        return 0;
    }

    DEBUG_INFO("[BLE] Initializing as: ");
    DEBUG_INFO(device_name);

    /* Initialize BLE */
    BLEDevice::init(device_name);

    /* Create BLE Server */
    ble_server = BLEDevice::createServer();
    ble_server->setCallbacks(new MyBLEServerCallbacks());

    /* Create BLE Service */
    BLEService* service = ble_server->createService(SERVICE_UUID);

    /* Create TX characteristic (notify - app reads from this) */
    ble_tx_char = service->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    ble_tx_char->addDescriptor(new BLE2902());

    /* Create RX characteristic (write - app writes to this) */
    ble_rx_char = service->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    ble_rx_char->setCallbacks(new BLERxCallbacks());

    /* Start service */
    service->start();

    /* Start advertising */
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06); /* Help with iPhone connections */
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    DEBUG_INFO("[BLE] UART service started");
    DEBUG_INFO("[BLE] Waiting for client connection...");

    ble_initialized = true;
    return 0;
}

void ble_serial_process(void) {
    /* Nothing needed - callbacks handle everything */
    /* Could add reconnection logic here if needed */
}

bool ble_serial_connected(void) {
    return ble_client_connected;
}

int ble_serial_write(const uint8_t* data, size_t len) {
    if (!ble_initialized || !ble_client_connected || !ble_tx_char) {
        return 0;
    }

    /* BLE has MTU limit (~20 bytes default, can negotiate higher) */
    /* For now, send in chunks if needed */
    const size_t MAX_CHUNK = 20;
    size_t sent = 0;

    while (sent < len) {
        size_t chunk_size = (len - sent) > MAX_CHUNK ? MAX_CHUNK : (len - sent);

        ble_tx_char->setValue((uint8_t*)(data + sent), chunk_size);
        ble_tx_char->notify();

        sent += chunk_size;

        /* Small delay between chunks to avoid overwhelming client */
        if (sent < len) {
            delay(10);
        }
    }

    return sent;
}

int ble_serial_available(void) {
    if (ble_rx_head >= ble_rx_tail) {
        return ble_rx_head - ble_rx_tail;
    } else {
        return BLE_RX_BUFFER_SIZE - ble_rx_tail + ble_rx_head;
    }
}

int ble_serial_read(void) {
    if (ble_rx_tail == ble_rx_head) {
        return -1; /* No data */
    }

    uint8_t data = ble_rx_buffer[ble_rx_tail];
    ble_rx_tail = (ble_rx_tail + 1) % BLE_RX_BUFFER_SIZE;
    return data;
}

#else /* !ENABLE_BLE || !ARDUINO_ARCH_ESP32 */

/* Stub implementations for non-BLE builds or non-ESP32 platforms */
int ble_serial_init(const char* device_name) {
    return 0; /* Silently succeed - BLE not enabled */
}

void ble_serial_process(void) {}
bool ble_serial_connected(void) {
    return false;
}
int ble_serial_write(const uint8_t* data, size_t len) {
    return 0;
}
int ble_serial_available(void) {
    return 0;
}
int ble_serial_read(void) {
    return -1;
}

#endif /* ENABLE_BLE && ARDUINO_ARCH_ESP32 */
