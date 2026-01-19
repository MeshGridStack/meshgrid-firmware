/**
 * Mesh State Accessor - Implementation
 *
 * This C file can safely access the global 'mesh' variable
 * without namespace conflicts since it's not C++.
 */

#include "mesh_accessor.h"

// Include full type definitions
#include "network/protocol.h"
#include "utils/constants.h"

// External global from main.cpp
extern struct meshgrid_state mesh;

void mesh_increment_tx(void) {
    mesh.packets_tx++;
}

void mesh_increment_rx(void) {
    mesh.packets_rx++;
}

const uint8_t* mesh_get_privkey(void) {
    return mesh.privkey;
}

const char* mesh_get_name(void) {
    return mesh.name;
}
