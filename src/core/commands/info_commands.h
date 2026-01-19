/**
 * Info command handlers
 * INFO, TELEMETRY, STATS, CONFIG
 */

#ifndef MESHGRID_COMMANDS_INFO_H
#define MESHGRID_COMMANDS_INFO_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void cmd_info();
void cmd_telemetry();
void cmd_stats();
void cmd_config();

#ifdef __cplusplus
}
#endif

#endif // MESHGRID_COMMANDS_INFO_H
