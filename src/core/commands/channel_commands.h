/**
 * Channel command handlers
 * CHANNELS, CHANNEL JOIN, CHANNEL SEND
 */

#ifndef MESHGRID_COMMANDS_CHANNEL_H
#define MESHGRID_COMMANDS_CHANNEL_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void cmd_channels();
void cmd_channel_join(const String &args);
void cmd_channel_send(const String &args);

#ifdef __cplusplus
}
#endif

#endif // MESHGRID_COMMANDS_CHANNEL_H
