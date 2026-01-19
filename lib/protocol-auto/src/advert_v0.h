/**
 * Advertisement v0 - MeshCore Integration Adapter
 * Routes advertisement calls to MeshCore integration
 */

#ifndef ADVERT_V0_H
#define ADVERT_V0_H

#include <stdint.h>

/* Forward declaration of packet structure */
struct meshgrid_packet;

/**
 * Send v0 advertisement via MeshCore
 */
void advert_v0_send(uint8_t route_type);

/**
 * Handle received v0 advertisement via MeshCore
 */
void advert_v0_receive(struct meshgrid_packet *pkt, int16_t rssi, int8_t snr);

#endif // ADVERT_V0_H
