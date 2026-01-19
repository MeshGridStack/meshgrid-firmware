/**
 * System command handlers
 * REBOOT, IDENTITY ROTATE, AUTH, PING
 * BLE, /mode, /test, /time, /pin
 */

#ifndef MESHGRID_COMMANDS_SYSTEM_H
#define MESHGRID_COMMANDS_SYSTEM_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void cmd_reboot();
void cmd_identity_rotate();

#ifdef ENABLE_BLE
void cmd_ble_status();
void cmd_ble();
#endif

/* Slash commands */
void cmd_mode(const String &mode);
void cmd_test(const String &test_type);
void cmd_time(const String &timestr);
void cmd_pin(const String &subcmd);

#ifdef __cplusplus
}
#endif

#endif // MESHGRID_COMMANDS_SYSTEM_H
