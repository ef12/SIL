/**
 * @file udp_socket.h
 * @brief Public API for UDP socket operations used by CAN-over-UDP modules.
 */

#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UDP socket wrapper.
 *
 * This structure stores platform-native socket state and initialization status.
 */
typedef struct
{
  /** Platform-native socket handle value. */
  uintptr_t native_socket;
  /** Indicates whether the socket has been successfully initialized. */
  bool initialized;
} UdpSocket;

/**
 * @brief Initializes a UDP socket and binds it to a local port.
 *
 * @param udp Pointer to socket wrapper to initialize.
 * @param local_port Local UDP port to bind.
 * @return true on success, false on invalid input or socket/bind failure.
 */
bool udp_socket_init(UdpSocket *udp, uint16_t local_port);

/**
 * @brief Sends a UDP datagram to a remote IPv4 endpoint.
 *
 * @param udp Initialized UDP socket instance.
 * @param ip Remote IPv4 address string (for example, "127.0.0.1").
 * @param remote_port Remote UDP port.
 * @param data Pointer to payload bytes.
 * @param data_len Payload length in bytes.
 * @return true if all bytes are sent, otherwise false.
 */
bool udp_socket_send_to(const UdpSocket *udp, const char *ip, uint16_t remote_port,
                        const void *data, size_t data_len);

/**
 * @brief Enables or disables non-blocking mode on the socket.
 *
 * @param udp Initialized UDP socket instance.
 * @param enabled true to enable non-blocking mode, false for blocking mode.
 * @return true on success, false on invalid input or platform call failure.
 */
bool udp_socket_set_non_blocking(UdpSocket *udp, bool enabled);

/**
 * @brief Sets receive timeout for blocking receive calls.
 *
 * @param udp Initialized UDP socket instance.
 * @param timeout_ms Timeout in milliseconds.
 * @return true on success, false on invalid input or platform call failure.
 */
bool udp_socket_set_receive_timeout(UdpSocket *udp, uint32_t timeout_ms);

/**
 * @brief Receives one UDP datagram and optionally returns sender metadata.
 *
 * @param udp Initialized UDP socket instance.
 * @param buffer Destination buffer for received payload.
 * @param buffer_len Size of buffer in bytes.
 * @param sender_ip Optional IPv4 string output buffer.
 * @param sender_ip_len Size of sender_ip buffer.
 * @param sender_port Optional sender port output.
 * @return Number of bytes received on success, or -1 on failure.
 */
int udp_socket_receive_from(UdpSocket *udp, void *buffer, size_t buffer_len, char *sender_ip,
                            size_t sender_ip_len, uint16_t *sender_port);

/**
 * @brief Closes the UDP socket and releases associated resources.
 *
 * @param udp Socket wrapper to close.
 */
void udp_socket_close(UdpSocket *udp);

#ifdef __cplusplus
}
#endif

#endif /* UDP_SOCKET_H */