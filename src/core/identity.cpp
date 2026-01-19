/**
 * Identity management
 */

#include "identity.h"
#include "utils/serial_output.h"
#include <Arduino.h>
#include <Preferences.h>

extern "C" {
#include "network/protocol.h"
#include "hardware/crypto/crypto.h"
}

/* External state from main.cpp */
extern struct meshgrid_state mesh;

void identity_init(void) {
    /* Initialize crypto subsystem */
    crypto_init();

    /* Try to load identity from NVS first */
    Preferences prefs;
    prefs.begin("meshgrid", true);  // Read-only
    bool has_identity = prefs.getBool("has_identity", false);

    if (has_identity) {
        /* Load saved keypair */
        if (prefs.getBytes("pubkey", mesh.pubkey, MESHGRID_PUBKEY_SIZE) == MESHGRID_PUBKEY_SIZE &&
            prefs.getBytes("privkey", mesh.privkey, MESHGRID_PRIVKEY_SIZE) == MESHGRID_PRIVKEY_SIZE) {
            /* Loaded successfully */
        } else {
            has_identity = false;  // Load failed, regenerate
        }
    }
    prefs.end();

    if (!has_identity) {
        /* Generate new Ed25519 keypair */
        crypto_generate_keypair(mesh.pubkey, mesh.privkey);

        /* Save to NVS for persistence */
        prefs.begin("meshgrid", false);  // Read-write
        prefs.putBool("has_identity", true);
        prefs.putBytes("pubkey", mesh.pubkey, MESHGRID_PUBKEY_SIZE);
        prefs.putBytes("privkey", mesh.privkey, MESHGRID_PRIVKEY_SIZE);
        prefs.end();
    }

    /* Compute hash (MeshCore uses first byte of pubkey) */
    mesh.our_hash = crypto_hash_pubkey(mesh.pubkey);

    /* Generate name from hash */
    snprintf(mesh.name, sizeof(mesh.name), "mg-%02X", mesh.our_hash);
}
