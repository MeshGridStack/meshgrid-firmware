/**
 * meshgrid v1 Trickle Algorithm (RFC 6206)
 *
 * Adaptive beacon rate algorithm:
 * - Fast beaconing when network changes (new neighbor, lost neighbor)
 * - Slow beaconing when network is stable
 * - Suppresses redundant beacons (if k similar beacons heard, don't send)
 *
 * Benefits:
 * - Reduces airtime in stable networks
 * - Fast convergence when topology changes
 * - Self-regulating (no central coordination needed)
 *
 * Example:
 * - Network stable: Beacons every 10 minutes
 * - New node joins: Interval resets to 30 seconds
 * - Node leaves: Interval resets to 30 seconds
 * - Gradually increases back to 10 minutes
 */

#ifndef MESHGRID_V1_TRICKLE_H
#define MESHGRID_V1_TRICKLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Trickle algorithm constants (RFC 6206)
 */
#define MESHGRID_TRICKLE_IMIN      30000    /* Minimum interval: 30 seconds */
#define MESHGRID_TRICKLE_IMAX      600000   /* Maximum interval: 10 minutes */
#define MESHGRID_TRICKLE_K         3        /* Redundancy threshold */

/*
 * Trickle state machine
 */
struct meshgrid_trickle {
    /* Configuration */
    uint32_t interval_min;      /* Imin: Minimum interval (ms) */
    uint32_t interval_max;      /* Imax: Maximum interval (ms) */
    uint8_t k_threshold;        /* k: Redundancy threshold */

    /* Current state */
    uint32_t interval_current;  /* I: Current interval length (ms) */
    uint32_t interval_start;    /* When current interval started */
    uint32_t next_beacon_time;  /* t: When to beacon in this interval */
    uint8_t redundant_count;    /* c: Count of redundant messages heard */
    bool suppress;              /* Should we suppress this beacon? */
    bool interval_active;       /* Is Trickle running? */
};

/*
 * Initialization
 */

/**
 * Initialize Trickle state with default parameters
 *
 * @param trickle  Trickle state structure
 */
void meshgrid_trickle_init(struct meshgrid_trickle *trickle);

/**
 * Initialize Trickle state with custom parameters
 *
 * @param trickle  Trickle state structure
 * @param i_min    Minimum interval (ms)
 * @param i_max    Maximum interval (ms)
 * @param k        Redundancy threshold
 */
void meshgrid_trickle_init_custom(
    struct meshgrid_trickle *trickle,
    uint32_t i_min,
    uint32_t i_max,
    uint8_t k
);

/*
 * Trickle algorithm operations
 */

/**
 * Start Trickle algorithm
 *
 * @param trickle  Trickle state
 * @param now      Current time (ms)
 */
void meshgrid_trickle_start(struct meshgrid_trickle *trickle, uint32_t now);

/**
 * Stop Trickle algorithm
 *
 * @param trickle  Trickle state
 */
void meshgrid_trickle_stop(struct meshgrid_trickle *trickle);

/**
 * Reset Trickle interval (when network changes)
 *
 * Called when:
 * - New neighbor discovered
 * - Neighbor lost
 * - Major topology change detected
 *
 * This resets interval to Imin and triggers fast beaconing.
 *
 * @param trickle  Trickle state
 * @param now      Current time (ms)
 */
void meshgrid_trickle_reset(struct meshgrid_trickle *trickle, uint32_t now);

/**
 * Heard a beacon from another node
 *
 * If the beacon is "consistent" (similar to what we would send),
 * increment redundant count. If redundant count >= k, suppress our beacon.
 *
 * @param trickle        Trickle state
 * @param is_consistent  True if beacon is similar to ours (same neighbor set)
 */
void meshgrid_trickle_heard_beacon(
    struct meshgrid_trickle *trickle,
    bool is_consistent
);

/**
 * Update Trickle timer (call periodically, e.g., every loop iteration)
 *
 * This advances the Trickle state machine:
 * - If interval expired, start new interval (double interval if < Imax)
 * - Schedule random beacon time within interval
 * - Reset redundant counter
 *
 * @param trickle  Trickle state
 * @param now      Current time (ms)
 */
void meshgrid_trickle_update(struct meshgrid_trickle *trickle, uint32_t now);

/**
 * Should we send a beacon now?
 *
 * @param trickle  Trickle state
 * @param now      Current time (ms)
 *
 * @return true if it's time to beacon and not suppressed, false otherwise
 */
bool meshgrid_trickle_should_beacon(
    struct meshgrid_trickle *trickle,
    uint32_t now
);

/**
 * Mark beacon as sent (called after sending beacon)
 *
 * This prevents sending again in this interval.
 *
 * @param trickle  Trickle state
 */
void meshgrid_trickle_beacon_sent(struct meshgrid_trickle *trickle);

/*
 * Query state
 */

/**
 * Get current interval length
 *
 * @param trickle  Trickle state
 * @return Current interval in milliseconds
 */
uint32_t meshgrid_trickle_get_interval(const struct meshgrid_trickle *trickle);

/**
 * Get time until next potential beacon
 *
 * @param trickle  Trickle state
 * @param now      Current time (ms)
 *
 * @return Milliseconds until next beacon check (0 if now or past)
 */
uint32_t meshgrid_trickle_time_until_beacon(
    const struct meshgrid_trickle *trickle,
    uint32_t now
);

/**
 * Is Trickle currently suppressing beacons?
 *
 * @param trickle  Trickle state
 * @return true if suppressed (heard too many redundant beacons)
 */
bool meshgrid_trickle_is_suppressed(const struct meshgrid_trickle *trickle);

/**
 * Is Trickle running?
 *
 * @param trickle  Trickle state
 * @return true if active, false if stopped
 */
bool meshgrid_trickle_is_active(const struct meshgrid_trickle *trickle);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_V1_TRICKLE_H */
