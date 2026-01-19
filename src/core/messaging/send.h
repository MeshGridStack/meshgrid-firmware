/**
 * Message sending interface
 */

#ifndef MESSAGING_SEND_H
#define MESSAGING_SEND_H

#include <Arduino.h>
#include <stdint.h>

/* Advertisement */
void send_advertisement(uint8_t route_type);

/* Public channel shortcut */
void send_group_message(const char *text);

#endif /* MESSAGING_SEND_H */
