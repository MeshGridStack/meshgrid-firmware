/**
 * Message command handlers
 * MESSAGES, INBOX, MESSAGES CLEAR
 */

#ifndef MESHGRID_COMMANDS_MESSAGE_H
#define MESHGRID_COMMANDS_MESSAGE_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void cmd_messages();
void cmd_messages_clear();

#ifdef __cplusplus
}
#endif

#endif // MESHGRID_COMMANDS_MESSAGE_H
