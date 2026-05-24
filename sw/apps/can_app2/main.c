#include "can_frame.h"
#include "can_transport_udp.h"
#include "udp_socket.h"

#include <stdio.h>
#include <string.h>

#define MY_PORT          7402U
#define PEER_PORT        7401U
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
  UdpSocket socket;
  CanTransportUdp transport;
  CanFrame tx_frame;
  CanFrame rx_frame;
  CanFrame echo_frame;
  uint8_t counter = 0U;
  char sender_ip[32];
  uint16_t sender_port = 0;

  printf("[App2] Starting on port %u\n", MY_PORT);

  if (!udp_socket_init(&socket, MY_PORT))
  {
    printf("[App2] Failed to init socket\n");
    return 1;
  }

  if (!can_transport_udp_init(&transport, &socket, "127.0.0.1", PEER_PORT))
  {
    printf("[App2] Failed to init transport\n");
    udp_socket_close(&socket);
    return 1;
  }

  if (!udp_socket_set_receive_timeout(&socket, RX_TIMEOUT_MS))
  {
    printf("[App2] Failed to set receive timeout\n");
    udp_socket_close(&socket);
    return 1;
  }

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
    (void)can_transport_udp_send_frame(&transport, &tx_frame);

    /* Receive loop: process incoming frames for ~SEND_INTERVAL_MS. */
    for (uint32_t i = 0U; i < RX_LOOPS; i++)
    {
      memset(sender_ip, 0, sizeof(sender_ip));
      if (can_transport_udp_receive_frame(&transport, &rx_frame, sender_ip, sizeof(sender_ip),
                                          &sender_port))
      {
        printf("[App2] RX <- ID=0x%08X DLC=%u Data=[%02X %02X %02X] from %s:%u\n", rx_frame.id,
               rx_frame.dlc, rx_frame.data[0], rx_frame.data[1], rx_frame.data[2], sender_ip,
               sender_port);
        (void)fflush(stdout);

        if (!IS_ECHO(rx_frame.id))
        {
          /* Echo the original message back. */
          echo_frame = rx_frame;
          echo_frame.id = MAKE_ECHO_ID(rx_frame.id);
          printf("[App2] ECHO -> ID=0x%08X DLC=%u Data=[%02X %02X %02X]\n", echo_frame.id,
                 echo_frame.dlc, echo_frame.data[0], echo_frame.data[1], echo_frame.data[2]);
          (void)fflush(stdout);
          (void)can_transport_udp_send_frame(&transport, &echo_frame);
        }
      }
    }

    counter++;
  }

  udp_socket_close(&socket);
  return 0;
}
