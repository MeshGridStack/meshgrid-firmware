/**
 * meshgrid-firmware Memory Configuration
 *
 * Centralized memory limits for all platforms.
 * Scales buffer sizes based on available DRAM.
 */

#ifndef MESHGRID_MEMORY_H
#define MESHGRID_MEMORY_H

/* ========================================================================= */
/* Platform-Specific Memory Limits                                          */
/* ========================================================================= */

#if defined(ARCH_ESP32)
    /* ESP32 (original) - Limited DRAM (~160KB usable)
     * Boards: T-Beam, T-LoRa V2.1, Nano G1, Station G1, RAK11200, etc. */
    #define MAX_NEIGHBORS 50
    #define MAX_CUSTOM_CHANNELS 10
    #define PUBLIC_MESSAGE_BUFFER_SIZE 50
    #define CHANNEL_MESSAGE_BUFFER_SIZE 3
    #define DIRECT_MESSAGE_BUFFER_SIZE 25

#elif defined(ARCH_ESP32S3)
    /* ESP32-S3 - More DRAM (~320KB usable)
     * Boards: T3S3, Heltec V3/V4, T-Beam Supreme, T-Deck, Station G2, etc. */
    #define MAX_NEIGHBORS 512
    #define MAX_CUSTOM_CHANNELS 50
    #define PUBLIC_MESSAGE_BUFFER_SIZE 100
    #define CHANNEL_MESSAGE_BUFFER_SIZE 5
    #define DIRECT_MESSAGE_BUFFER_SIZE 50

#elif defined(ARCH_ESP32C3)
    /* ESP32-C3 - Limited DRAM (~256KB usable)
     * Boards: Heltec HT62 */
    #define MAX_NEIGHBORS 128
    #define MAX_CUSTOM_CHANNELS 20
    #define PUBLIC_MESSAGE_BUFFER_SIZE 75
    #define CHANNEL_MESSAGE_BUFFER_SIZE 4
    #define DIRECT_MESSAGE_BUFFER_SIZE 35

#elif defined(ARCH_ESP32C6)
    /* ESP32-C6 - Similar to C3 (~256KB usable)
     * Boards: M5Stack Unit C6L */
    #define MAX_NEIGHBORS 128
    #define MAX_CUSTOM_CHANNELS 20
    #define PUBLIC_MESSAGE_BUFFER_SIZE 75
    #define CHANNEL_MESSAGE_BUFFER_SIZE 4
    #define DIRECT_MESSAGE_BUFFER_SIZE 35

#elif defined(ARCH_NRF52840)
    /* nRF52840 - Good DRAM (~256KB)
     * Boards: RAK4631, T-Echo, T1000E, Mesh Node T114, etc. */
    #define MAX_NEIGHBORS 256
    #define MAX_CUSTOM_CHANNELS 30
    #define PUBLIC_MESSAGE_BUFFER_SIZE 80
    #define CHANNEL_MESSAGE_BUFFER_SIZE 4
    #define DIRECT_MESSAGE_BUFFER_SIZE 40

#elif defined(ARCH_RP2040)
    /* RP2040 - Good DRAM (~264KB)
     * Boards: RAK11310, Pico, Pico W */
    #define MAX_NEIGHBORS 256
    #define MAX_CUSTOM_CHANNELS 30
    #define PUBLIC_MESSAGE_BUFFER_SIZE 80
    #define CHANNEL_MESSAGE_BUFFER_SIZE 4
    #define DIRECT_MESSAGE_BUFFER_SIZE 40

#else
    /* Conservative defaults for unknown platforms */
    #define MAX_NEIGHBORS 50
    #define MAX_CUSTOM_CHANNELS 10
    #define PUBLIC_MESSAGE_BUFFER_SIZE 50
    #define CHANNEL_MESSAGE_BUFFER_SIZE 3
    #define DIRECT_MESSAGE_BUFFER_SIZE 25
#endif

/* ========================================================================= */
/* Global Memory Constants (Platform-Independent)                           */
/* ========================================================================= */

/* Event log buffer */
#define LOG_BUFFER_SIZE 50

/* Packet deduplication table (seen packets) */
#define SEEN_TABLE_SIZE 64

/* TX queue size */
#define TX_QUEUE_SIZE 16

/* ========================================================================= */
/* Compile-Time Memory Usage Estimation                                     */
/* ========================================================================= */

/*
 * Neighbor table: MAX_NEIGHBORS × ~100 bytes
 * Channel table: MAX_CUSTOM_CHANNELS × ~50 bytes
 * Message buffers:
 *   - Public: PUBLIC_MESSAGE_BUFFER_SIZE × ~100 bytes
 *   - Direct: DIRECT_MESSAGE_BUFFER_SIZE × ~100 bytes
 *   - Channels: MAX_CUSTOM_CHANNELS × CHANNEL_MESSAGE_BUFFER_SIZE × ~100 bytes
 * Log buffer: LOG_BUFFER_SIZE × ~50 bytes
 * Seen table: SEEN_TABLE_SIZE × 36 bytes
 *
 * Estimated static RAM usage by platform:
 *   ESP32:     ~15 KB (fits in 160KB DRAM)
 *   ESP32-S3:  ~103 KB (fits in 320KB DRAM)
 *   ESP32-C3:  ~38 KB (fits in 256KB DRAM)
 *   nRF52840:  ~52 KB (fits in 256KB DRAM)
 *   RP2040:    ~52 KB (fits in 264KB DRAM)
 */

#endif /* MESHGRID_MEMORY_H */
