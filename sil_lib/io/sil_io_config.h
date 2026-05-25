/**
 * @file sil_io_config.h
 * @brief SIL IO configuration — BSP layer for IO peripherals.
 *
 * Call sil_io_config_init() once at startup to create the IO transport
 * infrastructure (socket, buffers, transport) and a fully initialized
 * IoDriver.  Then retrieve the driver via sil_io_config_get_driver().
 */

#ifndef SIL_IO_CONFIG_H
#define SIL_IO_CONFIG_H

#include "io_driver.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** IO peripheral configuration. */
typedef struct
{
  uint16_t local_port;
  const char *remote_ip;
  uint16_t remote_port;
  uint32_t sync_interval_ms;
  size_t digital_pin_count;
  size_t analog_pin_count;
} SilIoConfigParams;

/** Opaque SIL IO state. */
typedef struct
{
  void *internal;
  bool initialized;
} SilIoConfig;

/**
 * @brief Initializes the SIL IO subsystem.
 *
 * Creates the UDP socket, IO transport, and IO driver according to @p params.
 *
 * @param sil    SIL IO state to initialize (must not be NULL).
 * @param params Configuration parameters (must not be NULL).
 * @return true on success, false on failure.
 */
bool sil_io_config_init(SilIoConfig *sil, const SilIoConfigParams *params);

/**
 * @brief Returns the IO driver, or NULL if not initialized.
 *
 * @param sil Initialized SIL IO state.
 * @return Pointer to the IO driver, or NULL.
 */
IoDriver *sil_io_config_get_driver(const SilIoConfig *sil);

/**
 * @brief Tears down the SIL IO subsystem and releases resources.
 *
 * Safe to call on an uninitialized or already-deinitialized instance (no-op).
 *
 * @param sil SIL IO state to deinitialize.
 */
void sil_io_config_deinit(SilIoConfig *sil);

#ifdef __cplusplus
}
#endif

#endif /* SIL_IO_CONFIG_H */
