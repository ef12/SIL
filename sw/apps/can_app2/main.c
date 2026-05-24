#include "can_frame.h"
#include "sil_config.h"

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#define CAN_PORT         7402U
#define CAN_PEER         7401U
#define MY_MSG_ID        0x18FF60E5U
#define SEND_INTERVAL_MS 2000U
#define RX_TIMEOUT_MS    100U
#define RX_LOOPS         (SEND_INTERVAL_MS / RX_TIMEOUT_MS)

/** Bit 8 of the CAN ID (LSB of J1939 Group Extension) marks an echo. */
#define ECHO_FLAG        0x00000100U
#define IS_ECHO(id)      (((id) & ECHO_FLAG) != 0U)
#define MAKE_ECHO_ID(id) ((id) | ECHO_FLAG)

int main(void)
{
  /* ---- Phase 1: BSP — configure SIL peripherals ---- */

  SilConfig sil = {0};

  SilConfigParams sil_params = {
      .can =
          {
              .local_port = CAN_PORT,
              .remote_ip = "127.0.0.1",
              .remote_port = CAN_PEER,
              .timeout_ms = RX_TIMEOUT_MS,
          },
  };

  if (!sil_config_init(&sil, &sil_params))
  {
    printf("[App2] Failed to init SIL peripherals\n");
    return 1;
  }

  /* ---- Phase 2: HAL — get driver from BSP ---- */

  CanDriver *can = sil_config_get_can_driver(&sil);

  if (can == NULL)
  {
    printf("[App2] Failed to get CAN driver\n");
    sil_config_deinit(&sil);
    return 1;
  }

  /* ---- Phase 3: Application — CAN echo ---- */

  CanFrame tx_frame;
  CanFrame rx_frame;
  CanFrame echo_frame;
  uint8_t counter = 0U;

  printf("[App2] Starting on port %u\n", CAN_PORT);

  while (1)
  {
    /* Send our periodic message. */
    can_frame_clear(&tx_frame);
    tx_frame.id = MY_MSG_ID;
    tx_frame.dlc = 3U;
    tx_frame.data[0] = 0xBEU;
    tx_frame.data[1] = 0xEFU;
    tx_frame.data[2] = counter;

    printf("[App2] TX -> ID=0x%08X DLC=%u Data=[%02X %02X %02X]\n", tx_frame.id, tx_frame.dlc,
           tx_frame.data[0], tx_frame.data[1], tx_frame.data[2]);
    (void)fflush(stdout);
    (void)can_driver_send(can, &tx_frame);

    /* Receive loop: process incoming frames for ~SEND_INTERVAL_MS. */
    for (uint32_t i = 0U; i < RX_LOOPS; i++)
    {
      if (can_driver_receive(can, &rx_frame))
      {
        printf("[App2] RX <- ID=0x%08X DLC=%u Data=[%02X %02X %02X]\n", rx_frame.id, rx_frame.dlc,
               rx_frame.data[0], rx_frame.data[1], rx_frame.data[2]);
        (void)fflush(stdout);

        if (!IS_ECHO(rx_frame.id))
        {
          /* Echo the original message back. */
          can_frame_copy(&echo_frame, &rx_frame);
          echo_frame.id = MAKE_ECHO_ID(rx_frame.id);
          printf("[App2] ECHO -> ID=0x%08X DLC=%u Data=[%02X %02X %02X]\n", echo_frame.id,
                 echo_frame.dlc, echo_frame.data[0], echo_frame.data[1], echo_frame.data[2]);
          (void)fflush(stdout);
          (void)can_driver_send(can, &echo_frame);
        }
      }
    }

    counter++;
  }

  can_driver_close(can);
  sil_config_deinit(&sil);
  return 0;
}
