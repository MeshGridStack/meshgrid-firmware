/**
 * meshgrid MQTT Bridge implementation
 *
 * Optional feature (like BLE) for bridging mesh to MQTT broker.
 * Only compiled when MESHGRID_MQTT_SUPPORT is defined.
 */

#include "mqtt_bridge.h"
#include "utils/debug.h"

#ifdef MESHGRID_MQTT_SUPPORT

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <string.h>
#include <stdio.h>

/* Maximum topic length */
#define MAX_TOPIC_LEN 128
#define MAX_SUBSCRIPTIONS 8

/* Internal state */
static WiFiClient wifi_client;
static PubSubClient mqtt_client(wifi_client);
static struct mqtt_config current_config;
static char client_id[32];
static char topic_buffer[MAX_TOPIC_LEN];

/* Subscription tracking */
static struct {
    char topic[64];
    mqtt_msg_cb callback;
    void *user_data;
} subscriptions[MAX_SUBSCRIPTIONS];
static int subscription_count = 0;

/* Bridge callback */
static void (*bridge_rx_callback)(const uint8_t *packet, size_t len) = nullptr;

/* Forward declarations */
static void mqtt_callback(char *topic, uint8_t *payload, unsigned int length);
static void build_full_topic(char *buf, size_t buf_len, const char *suffix);

int mqtt_init(void)
{
    /* Generate default client ID from MAC */
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(client_id, sizeof(client_id), "meshgrid-%02X%02X%02X",
             mac[3], mac[4], mac[5]);

    /* Set default config if not already set */
    if (current_config.broker_host == nullptr) {
        current_config.broker_host = "localhost";
        current_config.broker_port = 1883;
        current_config.client_id = client_id;
        current_config.topic_prefix = "meshgrid/";
        current_config.use_tls = false;
    }

    mqtt_client.setServer(current_config.broker_host, current_config.broker_port);
    mqtt_client.setCallback(mqtt_callback);
    mqtt_client.setBufferSize(512);  /* Allow larger messages */

    return 0;
}

void mqtt_set_config(const struct mqtt_config *config)
{
    memcpy(&current_config, config, sizeof(current_config));

    if (current_config.client_id == nullptr) {
        current_config.client_id = client_id;
    }
    if (current_config.topic_prefix == nullptr) {
        current_config.topic_prefix = "meshgrid/";
    }
    if (current_config.broker_port == 0) {
        current_config.broker_port = current_config.use_tls ? 8883 : 1883;
    }

    mqtt_client.setServer(current_config.broker_host, current_config.broker_port);
}

int mqtt_connect(void)
{
    if (!WiFi.isConnected()) {
        DEBUG_INFO("[MQTT] WiFi not connected");
        return -1;
    }

    SerialOutput.print("[MQTT] Connecting to ");
    SerialOutput.print(current_config.broker_host);
    SerialOutput.print(":");
    DEBUG_INFO(current_config.broker_port);

    bool connected;
    if (current_config.username != nullptr) {
        connected = mqtt_client.connect(current_config.client_id,
                                         current_config.username,
                                         current_config.password);
    } else {
        connected = mqtt_client.connect(current_config.client_id);
    }

    if (connected) {
        DEBUG_INFO("[MQTT] Connected");

        /* Publish online status */
        char status_topic[MAX_TOPIC_LEN];
        build_full_topic(status_topic, sizeof(status_topic), MQTT_TOPIC_STATUS);
        mqtt_client.publish(status_topic, "online", true);

        /* Re-subscribe to previously subscribed topics */
        for (int i = 0; i < subscription_count; i++) {
            char full_topic[MAX_TOPIC_LEN];
            build_full_topic(full_topic, sizeof(full_topic), subscriptions[i].topic);
            mqtt_client.subscribe(full_topic);
        }

        /* Subscribe to RX topic for bridge */
        char rx_topic[MAX_TOPIC_LEN];
        build_full_topic(rx_topic, sizeof(rx_topic), MQTT_TOPIC_RX);
        mqtt_client.subscribe(rx_topic);

        return 0;
    } else {
        SerialOutput.print("[MQTT] Connection failed, rc=");
        DEBUG_INFO(mqtt_client.state());
        return -1;
    }
}

void mqtt_disconnect(void)
{
    if (mqtt_client.connected()) {
        /* Publish offline status */
        char status_topic[MAX_TOPIC_LEN];
        build_full_topic(status_topic, sizeof(status_topic), MQTT_TOPIC_STATUS);
        mqtt_client.publish(status_topic, "offline", true);

        mqtt_client.disconnect();
        DEBUG_INFO("[MQTT] Disconnected");
    }
}

bool mqtt_is_connected(void)
{
    return mqtt_client.connected();
}

void mqtt_loop(void)
{
    if (mqtt_client.connected()) {
        mqtt_client.loop();
    }
}

int mqtt_publish(const char *topic, const uint8_t *payload, size_t len, bool retain)
{
    if (!mqtt_client.connected()) {
        return -1;
    }

    char full_topic[MAX_TOPIC_LEN];
    build_full_topic(full_topic, sizeof(full_topic), topic);

    if (mqtt_client.publish(full_topic, payload, len, retain)) {
        return 0;
    }
    return -1;
}

int mqtt_publish_text(const char *topic, const char *text, bool retain)
{
    return mqtt_publish(topic, (const uint8_t *)text, strlen(text), retain);
}

int mqtt_subscribe(const char *topic, mqtt_msg_cb callback, void *user_data)
{
    if (subscription_count >= MAX_SUBSCRIPTIONS) {
        return -1;
    }

    /* Add to subscription list */
    strncpy(subscriptions[subscription_count].topic, topic,
            sizeof(subscriptions[0].topic) - 1);
    subscriptions[subscription_count].callback = callback;
    subscriptions[subscription_count].user_data = user_data;
    subscription_count++;

    /* Subscribe if connected */
    if (mqtt_client.connected()) {
        char full_topic[MAX_TOPIC_LEN];
        build_full_topic(full_topic, sizeof(full_topic), topic);
        mqtt_client.subscribe(full_topic);
    }

    return 0;
}

int mqtt_unsubscribe(const char *topic)
{
    /* Find and remove subscription */
    for (int i = 0; i < subscription_count; i++) {
        if (strcmp(subscriptions[i].topic, topic) == 0) {
            /* Unsubscribe */
            if (mqtt_client.connected()) {
                char full_topic[MAX_TOPIC_LEN];
                build_full_topic(full_topic, sizeof(full_topic), topic);
                mqtt_client.unsubscribe(full_topic);
            }

            /* Remove from list */
            for (int j = i; j < subscription_count - 1; j++) {
                subscriptions[j] = subscriptions[j + 1];
            }
            subscription_count--;
            return 0;
        }
    }
    return -1;
}

int mqtt_bridge_tx(const uint8_t *packet, size_t len)
{
    return mqtt_publish(MQTT_TOPIC_TX, packet, len, false);
}

void mqtt_bridge_set_rx_callback(void (*callback)(const uint8_t *packet, size_t len))
{
    bridge_rx_callback = callback;
}

/* Internal callback for all MQTT messages */
static void mqtt_callback(char *topic, uint8_t *payload, unsigned int length)
{
    /* Check if it's the RX topic (for bridge) */
    char rx_topic[MAX_TOPIC_LEN];
    build_full_topic(rx_topic, sizeof(rx_topic), MQTT_TOPIC_RX);

    if (strcmp(topic, rx_topic) == 0 && bridge_rx_callback != nullptr) {
        bridge_rx_callback(payload, length);
        return;
    }

    /* Check subscriptions */
    for (int i = 0; i < subscription_count; i++) {
        char full_topic[MAX_TOPIC_LEN];
        build_full_topic(full_topic, sizeof(full_topic), subscriptions[i].topic);

        if (strcmp(topic, full_topic) == 0) {
            if (subscriptions[i].callback != nullptr) {
                subscriptions[i].callback(subscriptions[i].topic, payload, length,
                                          subscriptions[i].user_data);
            }
            return;
        }
    }
}

static void build_full_topic(char *buf, size_t buf_len, const char *suffix)
{
    snprintf(buf, buf_len, "%s%s", current_config.topic_prefix, suffix);
}

#else /* MESHGRID_MQTT_SUPPORT not defined */

/* Stub implementations when MQTT support is disabled */

int mqtt_init(void) { return -1; }
void mqtt_set_config(const struct mqtt_config *config) { (void)config; }
int mqtt_connect(void) { return -1; }
void mqtt_disconnect(void) {}
bool mqtt_is_connected(void) { return false; }
void mqtt_loop(void) {}
int mqtt_publish(const char *topic, const uint8_t *payload, size_t len, bool retain)
{
    (void)topic; (void)payload; (void)len; (void)retain;
    return -1;
}
int mqtt_publish_text(const char *topic, const char *text, bool retain)
{
    (void)topic; (void)text; (void)retain;
    return -1;
}
int mqtt_subscribe(const char *topic, mqtt_msg_cb callback, void *user_data)
{
    (void)topic; (void)callback; (void)user_data;
    return -1;
}
int mqtt_unsubscribe(const char *topic)
{
    (void)topic;
    return -1;
}
int mqtt_bridge_tx(const uint8_t *packet, size_t len)
{
    (void)packet; (void)len;
    return -1;
}
void mqtt_bridge_set_rx_callback(void (*callback)(const uint8_t *packet, size_t len))
{
    (void)callback;
}

#endif /* MESHGRID_MQTT_SUPPORT */
