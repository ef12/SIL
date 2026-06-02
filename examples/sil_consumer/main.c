/**
 * @file main.c
 * @brief Minimal SilLib consumer — validates that the installed package links
 *        and the public API is callable.
 */

#include "sil_io_config.h"
#include "sil_lib_version.h"
#include "sil_vcan_config.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  SilVcanConfig vcan = {0};
  SilIoConfig io = {0};

  const SilVcanConfigParams vcan_params = {
      .local_port = 9000U,
      .remote_ip = "127.0.0.1",
      .remote_port = 9001U,
      .timeout_ms = 50U,
  };

  const SilIoConfigParams io_params = {
      .local_port = 9010U,
      .remote_ip = "127.0.0.1",
      .remote_port = 9011U,
      .sync_interval_ms = 10U,
      .digital_pin_count = 4U,
      .analog_pin_count = 2U,
  };

  printf("SilLib version: %s\n", SIL_LIB_VERSION_STRING);

  if (!sil_vcan_config_init(&vcan, &vcan_params))
  {
    fprintf(stderr, "sil_vcan_config_init failed\n");
    return EXIT_FAILURE;
  }
  printf("vcan: initialized, driver=%p\n", (void *)sil_vcan_config_get_driver(&vcan));

  if (!sil_io_config_init(&io, &io_params))
  {
    fprintf(stderr, "sil_io_config_init failed\n");
    sil_vcan_config_deinit(&vcan);
    return EXIT_FAILURE;
  }
  printf("io:   initialized, driver=%p\n", (void *)sil_io_config_get_driver(&io));

  sil_io_config_deinit(&io);
  sil_vcan_config_deinit(&vcan);

  printf("cleanup complete\n");
  return EXIT_SUCCESS;
}
