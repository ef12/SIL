/**
 * @file sil_config.h
 * @brief SIL system configuration — the "BSP" layer.
 *
 * Call sil_config_init() once at startup to create all SIL transport
 * infrastructure (sockets, buffers, transports) and fully initialized
 * drivers.  Then retrieve the drivers via the getter functions and
 * pass them to application modules.
 *
 * This mirrors the MCU pattern where the BSP configures peripherals
 * before the application initializes its drivers.
 */

#ifndef SIL_CONFIG_H
#define SIL_CONFIG_H

#include "can_driver.h"
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
  uint32_t timeout_ms;
  size_t digital_pin_count;
  size_t analog_pin_count;
} SilIoConfig;

/** CAN peripheral configuration. */
typedef struct
{
  uint16_t local_port;
  const char *remote_ip;
  uint16_t remote_port;
  uint32_t timeout_ms;
} SilCanConfig;

/** Top-level SIL system configuration. */
typedef struct
{
  SilIoConfig io;
  SilCanConfig can;
} SilConfigParams;

/** Opaque SIL system state. */
typedef struct
{
  void *internal;
  bool initialized;
} SilConfig;

bool sil_config_init(SilConfig *sil, const SilConfigParams *params);

/** Returns the IO driver, or NULL if IO was not configured. */
IoDriver *sil_config_get_io_driver(const SilConfig *sil);

/** Returns the CAN driver, or NULL if CAN was not configured. */
CanDriver *sil_config_get_can_driver(const SilConfig *sil);

void sil_config_deinit(SilConfig *sil);

#ifdef __cplusplus
}
#endif

#endif /* SIL_CONFIG_H */
