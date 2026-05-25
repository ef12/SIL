/**
 * @file sil_config_udp_socket.c
 * @brief SIL UDP socket configuration — init, bind, and timeout setup.
 */

#include "sil_config_udp_socket.h"

bool sil_config_udp_socket_init(UdpSocket *socket, uint16_t local_port, uint32_t timeout_ms)
{
  if (socket == NULL)
  {
    return false;
  }

  if (!udp_socket_init(socket, local_port))
  {
    return false;
  }

  if (timeout_ms > 0U && !udp_socket_set_receive_timeout(socket, timeout_ms))
  {
    udp_socket_close(socket);
    return false;
  }

  return true;
}
