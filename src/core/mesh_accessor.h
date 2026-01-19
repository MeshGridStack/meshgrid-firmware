/**
 * Mesh State Accessor - C interface
 *
 * Provides C functions to access the global mesh state
 * without bringing the 'mesh' symbol into scope.
 */

#ifndef MESH_ACCESSOR_H
#define MESH_ACCESSOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void mesh_increment_tx(void);
void mesh_increment_rx(void);
const uint8_t* mesh_get_privkey(void);
const char* mesh_get_name(void);

#ifdef __cplusplus
}
#endif

#endif // MESH_ACCESSOR_H
