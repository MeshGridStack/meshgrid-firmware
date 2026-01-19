/**
 * Common definitions for command handlers
 */

#ifndef MESHGRID_COMMANDS_COMMON_H
#define MESHGRID_COMMANDS_COMMON_H

#include <Arduino.h>

/**
 * Response builder - all command handlers use these
 * These are C++ functions with overloading, not C functions
 */
void response_start();
void response_print(const String &str);
void response_print(const char *str);
void response_print(char c);
void response_print(int val);
void response_print(unsigned int val);
void response_print(long val);
void response_print(unsigned long val);
void response_print(float val, int decimals);
void response_println(const String &str);
void response_println(const char *str);
void response_println(int val);
void response_send();

/**
 * JSON string escaping helper
 */
void print_json_string(const char *str);

#endif // MESHGRID_COMMANDS_COMMON_H
