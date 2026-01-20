/**
 * Config command handlers
 * CONFIG SAVE, CONFIG RESET, SET *, SET PRESET *
 */

#ifndef MESHGRID_COMMANDS_CONFIG_H
#define MESHGRID_COMMANDS_CONFIG_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void cmd_config_save();
void cmd_config_reset();
void cmd_set_name(const String& name);
void cmd_set_freq(float freq);
void cmd_set_bw(float bw);
void cmd_set_sf(int sf);
void cmd_set_cr(int cr);
void cmd_set_power(int power);
void cmd_set_preamble(int preamble);
void cmd_set_preset(const String& preset);

#ifdef __cplusplus
}
#endif

#endif // MESHGRID_COMMANDS_CONFIG_H
