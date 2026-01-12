/**
 * Identity management
 * Handles Ed25519 keypair generation, storage, and loading
 */

#ifndef MESHGRID_IDENTITY_H
#define MESHGRID_IDENTITY_H

/**
 * Initialize identity subsystem
 * - Loads existing keypair from NVS if available
 * - Generates new keypair if none exists
 * - Computes hash and generates node name
 */
void identity_init(void);

#endif // MESHGRID_IDENTITY_H
