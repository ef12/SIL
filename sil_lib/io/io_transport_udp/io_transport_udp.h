/**
 * @file io_transport_udp.h
 * @brief UDP transport adapter for IO buffers.
 */

#ifndef IO_TRANSPORT_UDP_H
#define IO_TRANSPORT_UDP_H

#include "udp_socket.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of dotted IPv4 string including null terminator. */
#define IO_TRANSPORT_UDP_MAX_IP_LEN 16U
/** Protocol version byte. */
#define IO_TRANSPORT_UDP_VERSION 1U

/**
 * @brief Transport instance state for IO-over-UDP communication.
 */
typedef struct
{
  /** Backing UDP socket used for communication. */
  UdpSocket *socket;
  /** Remote endpoint IPv4 address. */
  char remote_ip[IO_TRANSPORT_UDP_MAX_IP_LEN];
  /** Remote endpoint UDP port. */
  uint16_t remote_port;
  /** Caller-supplied digital buffer. */
  bool *digital_values;
  /** Number of digital pins in @p digital_values. */
  size_t digital_pin_count;
  /** Caller-supplied analog buffer. */
  uint16_t *analog_values;
  /** Number of analog pins in @p analog_values. */
  size_t analog_pin_count;
  /** Initialization state flag. */
  bool initialized;
} IoTransportUdp;

/**
 * @brief Initializes IO transport with endpoint and buffer bindings.
 *
 * @param transport Transport instance to initialize.
 * @param socket Initialized UDP socket used for send/receive.
 * @param remote_ip Remote endpoint IPv4 address.
 * @param remote_port Remote endpoint UDP port.
 * @param digital_values Caller-supplied digital IO buffer.
 * @param digital_pin_count Number of digital pins.
 * @param analog_values Caller-supplied analog IO buffer.
 * @param analog_pin_count Number of analog pins.
 * @return true on success, false otherwise.
 */
bool io_transport_udp_init(IoTransportUdp *transport, UdpSocket *socket, const char *remote_ip,
                           uint16_t remote_port, bool *digital_values, size_t digital_pin_count,
                           uint16_t *analog_values, size_t analog_pin_count);

/**
 * @brief Returns required payload size for given IO buffer sizes.
 *
 * @param digital_pin_count Number of digital pins.
 * @param analog_pin_count Number of analog pins.
 * @return Required payload size in bytes, or 0 when size overflows protocol limits.
 */
size_t io_transport_udp_wire_size(size_t digital_pin_count, size_t analog_pin_count);

/**
 * @brief Sends bound IO buffers to remote endpoint.
 *
 * @param transport Initialized transport instance.
 * @return true on success, false otherwise.
 */
bool io_transport_udp_send(const IoTransportUdp *transport);

/**
 * @brief Receives one IO payload and updates bound buffers.
 *
 * @param transport Initialized transport instance.
 * @param out_sender_ip Optional sender IP output buffer.
 * @param sender_ip_len Size of @p out_sender_ip.
 * @param out_sender_port Optional sender port output pointer.
 * @return true on success, false otherwise.
 */
bool io_transport_udp_receive(IoTransportUdp *transport, char *out_sender_ip, size_t sender_ip_len,
                              uint16_t *out_sender_port);

/**
 * @brief Gets a digital value from transport's buffer.
 *
 * @param transport Initialized transport instance.
 * @param pin Digital pin index.
 * @param out_value Output pointer for digital value.
 * @return true on success, false otherwise.
 */
bool io_transport_udp_digital_get(const IoTransportUdp *transport, size_t pin, bool *out_value);

/**
 * @brief Sets a digital value in transport's buffer.
 *
 * @param transport Initialized transport instance.
 * @param pin Digital pin index.
 * @param value Digital value to set.
 * @return true on success, false otherwise.
 */
bool io_transport_udp_digital_set(IoTransportUdp *transport, size_t pin, bool value);

/**
 * @brief Gets an analog value from transport's buffer.
 *
 * @param transport Initialized transport instance.
 * @param pin Analog pin index.
 * @param out_value Output pointer for analog value.
 * @return true on success, false otherwise.
 */
bool io_transport_udp_analog_get(const IoTransportUdp *transport, size_t pin, uint16_t *out_value);

/**
 * @brief Sets an analog value in transport's buffer.
 *
 * @param transport Initialized transport instance.
 * @param pin Analog pin index.
 * @param value Analog value to set.
 * @return true on success, false otherwise.
 */
bool io_transport_udp_analog_set(IoTransportUdp *transport, size_t pin, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* IO_TRANSPORT_UDP_H */
