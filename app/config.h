/**
 * Configuration persistence
 */

#ifndef MESHGRID_CONFIG_H
#define MESHGRID_CONFIG_H

/**
 * Initialize public channel (decode PSK)
 */
void init_public_channel(void);

/**
 * Load configuration from NVS
 */
void config_load(void);

/**
 * Save configuration to NVS
 */
void config_save(void);

#endif /* MESHGRID_CONFIG_H */
