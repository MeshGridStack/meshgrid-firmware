/**
 * meshgrid v1 Multi-TTL Beacon Scheduling
 *
 * Two types of beacons:
 * 1. Local beacons (TTL=2): Maintain local neighborhood, sent frequently
 * 2. Discovery beacons (TTL=max): Discover distant nodes, sent rarely
 *
 * Strategy:
 * - Local: Every 2 minutes (keeps neighbors updated)
 * - Discovery: Every 30 minutes (long-range exploration)
 * - User-triggered: Immediate discovery beacon on demand
 *
 * Benefits:
 * - Local network remains stable (frequent local beacons)
 * - Long-range discovery without flooding (rare high-TTL beacons)
 * - User can force discovery when needed
 */

#ifndef MESHGRID_V1_BEACON_H
#define MESHGRID_V1_BEACON_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Beacon schedule constants
 */
#define MESHGRID_BEACON_LOCAL_INTERVAL      120000   /* 2 minutes */
#define MESHGRID_BEACON_DISCOVERY_INTERVAL  1800000  /* 30 minutes */
#define MESHGRID_BEACON_LOCAL_TTL           2        /* Local neighborhood */
#define MESHGRID_BEACON_DISCOVERY_TTL       16       /* Maximum reasonable TTL */

/*
 * Beacon scheduling state
 */
struct meshgrid_beacon_schedule {
    /* Configuration */
    uint32_t local_interval;       /* Local beacon interval (ms) */
    uint32_t discovery_interval;   /* Discovery beacon interval (ms) */
    uint8_t local_ttl;             /* TTL for local beacons */
    uint8_t discovery_ttl;         /* TTL for discovery beacons */

    /* State */
    uint32_t next_local_beacon;    /* When to send next local beacon */
    uint32_t next_discovery_beacon;/* When to send next discovery beacon */
    bool user_triggered;           /* User requested immediate discovery */
};

/*
 * Initialization
 */

/**
 * Initialize beacon schedule with default parameters
 *
 * @param schedule  Beacon schedule structure
 * @param now       Current time (ms)
 */
void meshgrid_beacon_init(struct meshgrid_beacon_schedule *schedule, uint32_t now);

/**
 * Initialize beacon schedule with custom parameters
 *
 * @param schedule           Beacon schedule structure
 * @param local_interval     Local beacon interval (ms)
 * @param discovery_interval Discovery beacon interval (ms)
 * @param local_ttl          TTL for local beacons
 * @param discovery_ttl      TTL for discovery beacons
 * @param now                Current time (ms)
 */
void meshgrid_beacon_init_custom(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t local_interval,
    uint32_t discovery_interval,
    uint8_t local_ttl,
    uint8_t discovery_ttl,
    uint32_t now
);

/*
 * Beacon scheduling operations
 */

/**
 * Check if it's time to send a local beacon (TTL=2)
 *
 * @param schedule  Beacon schedule
 * @param now       Current time (ms)
 *
 * @return true if should send local beacon now
 */
bool meshgrid_beacon_should_send_local(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t now
);

/**
 * Check if it's time to send a discovery beacon (TTL=max)
 *
 * @param schedule  Beacon schedule
 * @param now       Current time (ms)
 *
 * @return true if should send discovery beacon now
 */
bool meshgrid_beacon_should_send_discovery(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t now
);

/**
 * Mark local beacon as sent
 *
 * Schedules next local beacon.
 *
 * @param schedule  Beacon schedule
 * @param now       Current time (ms)
 */
void meshgrid_beacon_local_sent(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t now
);

/**
 * Mark discovery beacon as sent
 *
 * Schedules next discovery beacon and clears user_triggered flag.
 *
 * @param schedule  Beacon schedule
 * @param now       Current time (ms)
 */
void meshgrid_beacon_discovery_sent(
    struct meshgrid_beacon_schedule *schedule,
    uint32_t now
);

/**
 * Trigger immediate discovery beacon (user-initiated)
 *
 * Sets user_triggered flag so next check will send discovery beacon immediately.
 *
 * @param schedule  Beacon schedule
 */
void meshgrid_beacon_trigger_discovery(struct meshgrid_beacon_schedule *schedule);

/*
 * Query state
 */

/**
 * Get time until next local beacon
 *
 * @param schedule  Beacon schedule
 * @param now       Current time (ms)
 *
 * @return Milliseconds until next local beacon (0 if due now)
 */
uint32_t meshgrid_beacon_time_until_local(
    const struct meshgrid_beacon_schedule *schedule,
    uint32_t now
);

/**
 * Get time until next discovery beacon
 *
 * @param schedule  Beacon schedule
 * @param now       Current time (ms)
 *
 * @return Milliseconds until next discovery beacon (0 if due now or user-triggered)
 */
uint32_t meshgrid_beacon_time_until_discovery(
    const struct meshgrid_beacon_schedule *schedule,
    uint32_t now
);

/**
 * Get local TTL value
 *
 * @param schedule  Beacon schedule
 * @return Local beacon TTL
 */
uint8_t meshgrid_beacon_get_local_ttl(const struct meshgrid_beacon_schedule *schedule);

/**
 * Get discovery TTL value
 *
 * @param schedule  Beacon schedule
 * @return Discovery beacon TTL
 */
uint8_t meshgrid_beacon_get_discovery_ttl(const struct meshgrid_beacon_schedule *schedule);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_V1_BEACON_H */
