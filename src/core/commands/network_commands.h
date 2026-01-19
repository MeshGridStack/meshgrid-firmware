/**
 * Network command handlers
 * NEIGHBORS, ADVERT, SEND, SEND GROUP, TRACE
 */

#ifndef MESHGRID_COMMANDS_NETWORK_H
#define MESHGRID_COMMANDS_NETWORK_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void cmd_neighbors();
void cmd_advert();
void cmd_advert_local();
void cmd_advert_flood();
void cmd_send(const String &args);
void cmd_send_group(const String &message);
void cmd_trace(const String &target);

#ifdef __cplusplus
}
#endif

#endif // MESHGRID_COMMANDS_NETWORK_H
