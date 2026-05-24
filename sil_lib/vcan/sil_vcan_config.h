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
} SilVcanConfigParams;

/** Opaque SIL virtual CAN state. */
typedef struct
{
  void *internal;
  bool initialized;
} SilVcanConfig;

bool sil_vcan_config_init(SilVcanConfig *sil, const SilVcanConfigParams *params);

/** Returns the CAN driver, or NULL if not initialized. */
CanDriver *sil_vcan_config_get_driver(const SilVcanConfig *sil);

void sil_vcan_config_deinit(SilVcanConfig *sil);

#ifdef __cplusplus
}
#endif

#endif /* SIL_VCAN_CONFIG_H */
