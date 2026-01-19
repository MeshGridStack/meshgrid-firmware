/**
 * meshgrid v1 Trickle Algorithm Implementation (RFC 6206)
 */

#include "trickle.h"
#include <stdlib.h>

/* Platform-specific random number generator */
#if defined(ARDUINO_ARCH_ESP32)
    #include <Arduino.h>
    #define GET_RANDOM_U32() esp_random()
#elif defined(ARDUINO_ARCH_NRF52)
    #include <Arduino.h>
    extern uint32_t nrf52_random(void);
    #define GET_RANDOM_U32() nrf52_random()
#else
    #include <stdlib.h>
    #define GET_RANDOM_U32() ((uint32_t)rand())
#endif

/*
 * Generate random number in range [min, max)
 */
static uint32_t random_range(uint32_t min, uint32_t max) {
    if (max <= min) {
        return min;
    }
    uint32_t range = max - min;
    return min + (GET_RANDOM_U32() % range);
}

/*
 * Start a new Trickle interval
 */
static void start_interval(struct meshgrid_trickle *trickle, uint32_t now) {
    trickle->interval_start = now;
    trickle->redundant_count = 0;
    trickle->suppress = false;

    /* Schedule beacon at random time t in [I/2, I) */
    uint32_t half_interval = trickle->interval_current / 2;
    trickle->next_beacon_time = now + random_range(half_interval, trickle->interval_current);
}

/*
 * Initialization
 */

void meshgrid_trickle_init(struct meshgrid_trickle *trickle) {
    meshgrid_trickle_init_custom(
        trickle,
        MESHGRID_TRICKLE_IMIN,
        MESHGRID_TRICKLE_IMAX,
        MESHGRID_TRICKLE_K
    );
}

void meshgrid_trickle_init_custom(
    struct meshgrid_trickle *trickle,
    uint32_t i_min,
    uint32_t i_max,
    uint8_t k
) {
    trickle->interval_min = i_min;
    trickle->interval_max = i_max;
    trickle->k_threshold = k;
    trickle->interval_current = i_min;
    trickle->interval_start = 0;
    trickle->next_beacon_time = 0;
    trickle->redundant_count = 0;
    trickle->suppress = false;
    trickle->interval_active = false;
}

/*
 * Operations
 */

void meshgrid_trickle_start(struct meshgrid_trickle *trickle, uint32_t now) {
    trickle->interval_current = trickle->interval_min;
    trickle->interval_active = true;
    start_interval(trickle, now);
}

void meshgrid_trickle_stop(struct meshgrid_trickle *trickle) {
    trickle->interval_active = false;
}

void meshgrid_trickle_reset(struct meshgrid_trickle *trickle, uint32_t now) {
    if (!trickle->interval_active) {
        return;
    }

    /* Reset to minimum interval */
    trickle->interval_current = trickle->interval_min;
    start_interval(trickle, now);
}

void meshgrid_trickle_heard_beacon(
    struct meshgrid_trickle *trickle,
    bool is_consistent
) {
    if (!trickle->interval_active) {
        return;
    }

    if (is_consistent) {
        trickle->redundant_count++;

        /* Suppress if redundant count >= k */
        if (trickle->redundant_count >= trickle->k_threshold) {
            trickle->suppress = true;
        }
    }
}

void meshgrid_trickle_update(struct meshgrid_trickle *trickle, uint32_t now) {
    if (!trickle->interval_active) {
        return;
    }

    /* Check if current interval has expired */
    uint32_t elapsed = now - trickle->interval_start;
    if (elapsed >= trickle->interval_current) {
        /* Double interval (up to Imax) */
        trickle->interval_current *= 2;
        if (trickle->interval_current > trickle->interval_max) {
            trickle->interval_current = trickle->interval_max;
        }

        /* Start new interval */
        start_interval(trickle, now);
    }
}

bool meshgrid_trickle_should_beacon(
    struct meshgrid_trickle *trickle,
    uint32_t now
) {
    if (!trickle->interval_active) {
        return false;
    }

    /* Check if it's time to beacon */
    if (now < trickle->next_beacon_time) {
        return false;
    }

    /* Check if suppressed */
    if (trickle->suppress) {
        return false;
    }

    return true;
}

void meshgrid_trickle_beacon_sent(struct meshgrid_trickle *trickle) {
    /* Set next_beacon_time to end of interval to prevent re-sending */
    trickle->next_beacon_time = trickle->interval_start + trickle->interval_current;
}

/*
 * Query state
 */

uint32_t meshgrid_trickle_get_interval(const struct meshgrid_trickle *trickle) {
    return trickle->interval_current;
}

uint32_t meshgrid_trickle_time_until_beacon(
    const struct meshgrid_trickle *trickle,
    uint32_t now
) {
    if (!trickle->interval_active) {
        return UINT32_MAX;
    }

    if (now >= trickle->next_beacon_time) {
        return 0;
    }

    return trickle->next_beacon_time - now;
}

bool meshgrid_trickle_is_suppressed(const struct meshgrid_trickle *trickle) {
    return trickle->suppress;
}

bool meshgrid_trickle_is_active(const struct meshgrid_trickle *trickle) {
    return trickle->interval_active;
}
