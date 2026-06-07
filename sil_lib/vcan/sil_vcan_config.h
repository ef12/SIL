/**
 * @file sil_vcan_config.h
 * @brief SIL virtual CAN configuration — BSP layer for CAN peripherals.
 *
 * Call sil_vcan_config_init() once at startup to create the CAN transport
 * infrastructure (socket, transport) and a fully initialized CanDriver.
 * Then retrieve the driver via sil_vcan_config_get_driver().
 */

#ifndef SIL_VCAN_CONFIG_H
#define SIL_VCAN_CONFIG_H

#include "can_driver.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Virtual CAN peripheral configuration. */
typedef struct
{
  uint16_t local_port;
  const char *remote_ip;
  uint16_t remote_port;
  uint32_t timeout_ms;
  size_t max_pending_tx;
  size_t max_rx_queue;
  /** When true, frames pass through the CAN bus emulator (arbitration,
   *  TX/RX queuing).  When false, frames go directly over UDP — this is
   *  thread-safe and suited for cross-process communication. */
  bool use_emulator;
} SilVcanConfigParams;

/** Opaque SIL virtual CAN state. */
typedef struct
{
  void *internal;
  bool initialized;
} SilVcanConfig;

/**
 * @brief Initializes the SIL virtual CAN subsystem.
 *
 * Creates the UDP socket, CAN transport, and CAN driver according to @p params.
 *
 * @param sil    SIL virtual CAN state to initialize (must not be NULL).
 * @param params Configuration parameters (must not be NULL).
 * @return true on success, false on failure.
 */
bool sil_vcan_config_init(SilVcanConfig *sil, const SilVcanConfigParams *params);

/**
 * @brief Returns the CAN driver, or NULL if not initialized.
 *
 * @param sil Initialized SIL virtual CAN state.
 * @return Pointer to the CAN driver, or NULL.
 */
CanDriver *sil_vcan_config_get_driver(const SilVcanConfig *sil);

/**
 * @brief Tears down the SIL virtual CAN subsystem and releases resources.
 *
 * Safe to call on an uninitialized or already-deinitialized instance (no-op).
 *
 * @param sil SIL virtual CAN state to deinitialize.
 */
void sil_vcan_config_deinit(SilVcanConfig *sil);

#ifdef __cplusplus
}
#endif

#endif /* SIL_VCAN_CONFIG_H */
