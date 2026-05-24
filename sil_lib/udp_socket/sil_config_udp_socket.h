/**
 * @file sil_config_udp_socket.h
 * @brief SIL UDP socket configuration helper.
 *
 * Initializes a UdpSocket, binds it to a local port, and optionally
 * sets a receive timeout — the common setup shared by IO and CAN configs.
 */

#ifndef SIL_CONFIG_UDP_SOCKET_H
#define SIL_CONFIG_UDP_SOCKET_H

#include "udp_socket.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and configures a UDP socket for SIL use.
 *
 * @param socket     Socket to initialize (must not be NULL).
 * @param local_port Local UDP port to bind.
 * @param timeout_ms Receive timeout in ms (0 = no timeout).
 * @return true on success, false on failure (socket is closed on error).
 */
bool sil_config_udp_socket_init(UdpSocket *socket, uint16_t local_port, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SIL_CONFIG_UDP_SOCKET_H */
