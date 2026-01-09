/**
 * Channel management
 */

#ifndef MESHGRID_CHANNELS_H
#define MESHGRID_CHANNELS_H

/* Save custom channels to NVS */
void channels_save_to_nvs(void);

/* Load custom channels from NVS on boot */
void channels_load_from_nvs(void);

#endif /* MESHGRID_CHANNELS_H */
