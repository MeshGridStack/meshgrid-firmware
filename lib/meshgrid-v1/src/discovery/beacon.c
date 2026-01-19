/**
 * meshgrid v1 Multi-TTL Beacon Scheduling Implementation
 */

#include "beacon.h"

/*
 * Initialization
 */

void meshgrid_beacon_init(struct meshgrid_beacon_schedule *schedule, uint32_t now) {
    meshgrid_beacon_init_custom(
        schedule,
        MESHGRID_BEACON_LOCAL_INTERVAL,
        MESHGRID_BEACON_DISCOVERY_INTERVAL,
        MESHGRID_BEACON_LOCAL_TTL,
        MESHGRID_BEACON_DISCOVERY_TTL,
        now
    );
}

void meshgrid_beacon_init_custom(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t local_interval,
    uint32_t discovery_interval,
    uint8_t local_ttl,
    uint8_t discovery_ttl,
    uint32_t now
) {
    schedule->local_interval = local_interval;
    schedule->discovery_interval = discovery_interval;
    schedule->local_ttl = local_ttl;
    schedule->discovery_ttl = discovery_ttl;
    schedule->next_local_beacon = now + local_interval;
    schedule->next_discovery_beacon = now + discovery_interval;
    schedule->user_triggered = false;
}

/*
 * Operations
 */

bool meshgrid_beacon_should_send_local(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t now
) {
    return (now >= schedule->next_local_beacon);
}

bool meshgrid_beacon_should_send_discovery(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t now
) {
    /* User triggered discovery has priority */
    if (schedule->user_triggered) {
        return true;
    }

    return (now >= schedule->next_discovery_beacon);
}

void meshgrid_beacon_local_sent(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t now
) {
    schedule->next_local_beacon = now + schedule->local_interval;
}

void meshgrid_beacon_discovery_sent(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t now
) {
    schedule->next_discovery_beacon = now + schedule->discovery_interval;
    schedule->user_triggered = false;
}

void meshgrid_beacon_trigger_discovery(struct meshgrid_beacon_schedule *schedule) {
    schedule->user_triggered = true;
}

/*
 * Query state
 */

uint32_t meshgrid_beacon_time_until_local(
    const struct meshgrid_beacon_schedule *schedule,
    uint32_t now
) {
    if (now >= schedule->next_local_beacon) {
        return 0;
    }
    return schedule->next_local_beacon - now;
}

uint32_t meshgrid_beacon_time_until_discovery(
    const struct meshgrid_beacon_schedule *schedule,
    uint32_t now
) {
    if (schedule->user_triggered) {
        return 0;
    }

    if (now >= schedule->next_discovery_beacon) {
        return 0;
    }

    return schedule->next_discovery_beacon - now;
}

uint8_t meshgrid_beacon_get_local_ttl(const struct meshgrid_beacon_schedule *schedule) {
    return schedule->local_ttl;
}

uint8_t meshgrid_beacon_get_discovery_ttl(const struct meshgrid_beacon_schedule *schedule) {
    return schedule->discovery_ttl;
}
