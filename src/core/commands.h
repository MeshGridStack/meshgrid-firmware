/**
 * Serial command handling
 *
 * Handles both uppercase CLI commands (INFO, NEIGHBORS, SEND, etc.)
 * and human-readable /slash commands (/mode, /neighbors, /stats, etc.)
 */

#ifndef MESHGRID_COMMANDS_H
#define MESHGRID_COMMANDS_H

#include <Arduino.h>

/**
 * Handle CLI command (uppercase, MeshCore-compatible)
 */
void handle_cli_command(const String &cmd);

/**
 * Handle serial input
 * Call from loop() to process incoming serial data
 */
void handle_serial(void);

#endif /* MESHGRID_COMMANDS_H */
