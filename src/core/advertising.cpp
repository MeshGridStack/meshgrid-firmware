/**
 * Advertisement scheduling
 */

#include "advertising.h"
#include <Arduino.h>

extern "C" {
#include "network/protocol.h"
}

#include "messaging.h"

/* Advertisement intervals (from protocol.h) */
#define MESHGRID_LOCAL_ADVERT_MS (2 * 60 * 1000)          /* 2 minutes */
#define MESHGRID_ADVERT_INTERVAL_MS (12 * 60 * 60 * 1000) /* 12 hours */

void advertising_process(void) {
    static uint32_t last_advert = 0;
    static uint32_t last_local_advert = 0;

    /* Local advertisement (every 2 minutes) - ROUTE_DIRECT for nearby discovery */
    if (millis() - last_local_advert > MESHGRID_LOCAL_ADVERT_MS) {
        send_advertisement(ROUTE_DIRECT); /* Zero-hop, neighbors only */
        last_local_advert = millis();
    }

    /* Flood advertisement (every 12 hours) - ROUTE_FLOOD for network-wide presence */
    if (millis() - last_advert > MESHGRID_ADVERT_INTERVAL_MS) {
        send_advertisement(ROUTE_FLOOD); /* Network-wide */
        last_advert = millis();
    }
}
