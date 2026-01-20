/**
 * Debug output via COBS
 * meshgrid-cli can capture debug output to file
 *
 * All debug output uses COBS framing (zero-byte delimited)
 * Format: {"type":"debug","level":"INFO","msg":"message"}
 */

#ifndef MESHGRID_DEBUG_H
#define MESHGRID_DEBUG_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Debug levels
 */
typedef enum {
    DEBUG_LEVEL_ERROR = 0, // Critical errors
    DEBUG_LEVEL_WARN = 1,  // Warnings
    DEBUG_LEVEL_INFO = 2,  // General info
    DEBUG_LEVEL_DEBUG = 3  // Detailed debug
} debug_level_t;

/**
 * Enable/disable debug output at compile time
 * Set to 0 to completely remove debug code
 */
#ifndef DEBUG_ENABLED
#    define DEBUG_ENABLED 1
#endif

/**
 * Set minimum debug level (compile-time filter)
 * Only messages at or above this level are output
 */
#ifndef DEBUG_MIN_LEVEL
#    define DEBUG_MIN_LEVEL DEBUG_LEVEL_DEBUG // Show all by default
#endif

/**
 * Core debug output function
 * Sends COBS-encoded JSON: {"type":"debug","level":"INFO","msg":"..."}
 */
void debug_output(debug_level_t level, const char* msg);

/**
 * Formatted debug output (printf-style)
 */
void debug_printf(debug_level_t level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

/**
 * Convenience macros for each log level
 */
#if DEBUG_ENABLED && (DEBUG_MIN_LEVEL >= DEBUG_LEVEL_ERROR)
#    define DEBUG_ERROR(msg) debug_output(DEBUG_LEVEL_ERROR, msg)
#    define DEBUG_ERRORF(fmt, ...) debug_printf(DEBUG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#else
#    define DEBUG_ERROR(msg) ((void)0)
#    define DEBUG_ERRORF(fmt, ...) ((void)0)
#endif

#if DEBUG_ENABLED && (DEBUG_MIN_LEVEL >= DEBUG_LEVEL_WARN)
#    define DEBUG_WARN(msg) debug_output(DEBUG_LEVEL_WARN, msg)
#    define DEBUG_WARNF(fmt, ...) debug_printf(DEBUG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#else
#    define DEBUG_WARN(msg) ((void)0)
#    define DEBUG_WARNF(fmt, ...) ((void)0)
#endif

#if DEBUG_ENABLED && (DEBUG_MIN_LEVEL >= DEBUG_LEVEL_INFO)
#    define DEBUG_INFO(msg) debug_output(DEBUG_LEVEL_INFO, msg)
#    define DEBUG_INFOF(fmt, ...) debug_printf(DEBUG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#else
#    define DEBUG_INFO(msg) ((void)0)
#    define DEBUG_INFOF(fmt, ...) ((void)0)
#endif

#if DEBUG_ENABLED && (DEBUG_MIN_LEVEL >= DEBUG_LEVEL_DEBUG)
#    define DEBUG_DEBUG(msg) debug_output(DEBUG_LEVEL_DEBUG, msg)
#    define DEBUG_DEBUGF(fmt, ...) debug_printf(DEBUG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
#    define DEBUG_DEBUG(msg) ((void)0)
#    define DEBUG_DEBUGF(fmt, ...) ((void)0)
#endif

/**
 * Legacy compatibility (deprecated - use level-specific macros)
 */
#define debug_print(msg) DEBUG_INFO(msg)
#define debug_println(msg) DEBUG_INFO(msg)

#ifdef __cplusplus
}
#endif

#endif // MESHGRID_DEBUG_H
