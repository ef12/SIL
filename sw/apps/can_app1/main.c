#include "fan_controller.h"
#include "sil_config.h"

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#define IO_PORT      7501U
#define IO_PEER_PORT 7502U
#define CAN_PORT     7401U
#define CAN_PEER     7402U
#define LOOP_MS      100U
#define LOOP_DT      0.1f

int main(void)
{
  /* ---- Phase 1: BSP — configure SIL peripherals ---- */

  SilConfig sil = {0};

  SilConfigParams sil_params = {
      .io =
          {
              .local_port = IO_PORT,
              .remote_ip = "127.0.0.1",
              .remote_port = IO_PEER_PORT,
              .timeout_ms = LOOP_MS,
              .digital_pin_count = 1U,
              .analog_pin_count = 2U,
          },
      .can =
          {
              .local_port = CAN_PORT,
              .remote_ip = "127.0.0.1",
              .remote_port = CAN_PEER,
              .timeout_ms = 1U, /* non-blocking poll */
          },
  };

  if (!sil_config_init(&sil, &sil_params))
  {
    printf("[FanCtrl] Failed to init SIL peripherals\n");
    return 1;
  }

  /* ---- Phase 2: HAL — get drivers from BSP ---- */

  IoDriver *io = sil_config_get_io_driver(&sil);
  CanDriver *can = sil_config_get_can_driver(&sil);

  if (io == NULL || can == NULL)
  {
    printf("[FanCtrl] Failed to get drivers\n");
    sil_config_deinit(&sil);
    return 1;
  }

  /* ---- Phase 3: Application ---- */

  FanController fan;

  FanControllerConfig fan_cfg = {
      .enable_pin = 0U,
      .feedback_pin = 0U,
      .output_pin = 1U,
      .setpoint = 32768U,
      .kp = 1.0f,
      .ki = 0.5f,
      .output_min = 0.0f,
      .output_max = 65535.0f,
      .status_can_id = 0x18FF50E5U,
      .command_can_id = 0x18FF60E5U,
  };

  if (!fan_controller_init(&fan, &fan_cfg, io, can))
  {
    printf("[FanCtrl] Failed to init fan controller\n");
    sil_config_deinit(&sil);
    return 1;
  }

  printf("[FanCtrl] Starting — IO port %u, CAN port %u\n", IO_PORT, CAN_PORT);

  /* ---- Main loop ---- */

  while (1)
  {
    (void)io_driver_sync_inputs(io);
    fan_controller_update(&fan, LOOP_DT);
    (void)io_driver_sync_outputs(io);

    (void)fan_controller_send_status(&fan);
    (void)fan_controller_receive_commands(&fan);

    printf("[FanCtrl] EN=%d SP=%5u FB=%5u OUT=%5u\n", fan.enabled, fan.setpoint, fan.feedback,
           fan.output);
    (void)fflush(stdout);

    sleep_ms(LOOP_MS);
  }

  io_driver_close(io);
  can_driver_close(can);
  sil_config_deinit(&sil);
  return 0;
}
