/**
 * meshgrid MQTT Bridge - Optional MQTT support
 *
 * Bridges mesh messages to/from MQTT broker when WiFi is available.
 * Enable with -DMESHGRID_MQTT_SUPPORT build flag (like BLE).
 *
 * Can be used with both CLIENT and REPEATER device modes.
 */

#ifndef MESHGRID_MQTT_BRIDGE_H
#define MESHGRID_MQTT_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MQTT configuration
 */
struct mqtt_config {
    const char* broker_host;  /* MQTT broker hostname/IP */
    uint16_t broker_port;     /* MQTT broker port (default 1883) */
    const char* client_id;    /* Client ID (auto-generated if NULL) */
    const char* username;     /* Username (NULL if no auth) */
    const char* password;     /* Password (NULL if no auth) */
    const char* topic_prefix; /* Topic prefix (default "meshgrid/") */
    bool use_tls;             /* Use TLS (port 8883) */
};

/**
 * MQTT message callback
 */
typedef void (*mqtt_msg_cb)(const char* topic, const uint8_t* payload, size_t payload_len, void* user_data);

/**
 * Initialize MQTT bridge
 * Must call mqtt_set_config() before this
 */
int mqtt_init(void);

/**
 * Set MQTT configuration
 */
void mqtt_set_config(const struct mqtt_config* config);

/**
 * Connect to MQTT broker
 * Requires WiFi to be connected first
 */
int mqtt_connect(void);

/**
 * Disconnect from MQTT broker
 */
void mqtt_disconnect(void);

/**
 * Check if connected to broker
 */
bool mqtt_is_connected(void);

/**
 * Process MQTT events (call from main loop)
 */
void mqtt_loop(void);

/**
 * Publish message to broker
 * topic: Topic suffix (will be prefixed with topic_prefix)
 * payload: Message payload
 * len: Payload length
 * retain: Retain flag
 */
int mqtt_publish(const char* topic, const uint8_t* payload, size_t len, bool retain);

/**
 * Publish text message
 */
int mqtt_publish_text(const char* topic, const char* text, bool retain);

/**
 * Subscribe to topic
 * topic: Topic suffix (will be prefixed with topic_prefix)
 */
int mqtt_subscribe(const char* topic, mqtt_msg_cb callback, void* user_data);

/**
 * Unsubscribe from topic
 */
int mqtt_unsubscribe(const char* topic);

/**
 * Publish mesh packet to MQTT (for bridging)
 */
int mqtt_bridge_tx(const uint8_t* packet, size_t len);

/**
 * Set callback for receiving mesh packets from MQTT
 */
void mqtt_bridge_set_rx_callback(void (*callback)(const uint8_t* packet, size_t len));

/**
 * Standard topic suffixes
 */
#define MQTT_TOPIC_TX "tx"               /* Messages from mesh to MQTT */
#define MQTT_TOPIC_RX "rx"               /* Messages from MQTT to mesh */
#define MQTT_TOPIC_STATUS "status"       /* Node status */
#define MQTT_TOPIC_TELEMETRY "telemetry" /* Telemetry data */
#define MQTT_TOPIC_NEIGHBORS "neighbors" /* Neighbor list */

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_MQTT_BRIDGE_H */
