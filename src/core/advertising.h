/**
 * Advertisement scheduling
 */

#ifndef MESHGRID_ADVERTISING_H
#define MESHGRID_ADVERTISING_H

/**
 * Process periodic advertisement sending
 * Should be called every loop iteration
 *
 * Sends two types of advertisements:
 * - Local (ROUTE_DIRECT): Every 2 minutes, neighbors only
 * - Flood (ROUTE_FLOOD): Every 12 hours, network-wide
 */
void advertising_process(void);

#endif // MESHGRID_ADVERTISING_H
