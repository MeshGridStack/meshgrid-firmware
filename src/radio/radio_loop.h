/**
 * Radio receive loop handling
 */

#ifndef MESHGRID_RADIO_LOOP_H
#define MESHGRID_RADIO_LOOP_H

/**
 * Process radio RX and ensure radio is in receive mode
 * Should be called every loop iteration
 *
 * Implements MeshCore's RX pattern:
 * - Check for interrupt flag
 * - Read packet if available
 * - Ensure radio returns to RX mode
 */
void radio_loop_process(void);

#endif // MESHGRID_RADIO_LOOP_H
