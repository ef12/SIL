/**
 * @file can_transport_udp.h
 * @brief UDP transport adapter for CAN frames.
 */

#ifndef CAN_TRANSPORT_UDP_H
#define CAN_TRANSPORT_UDP_H

#include "can_frame.h"
#include "udp_socket.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of dotted IPv4 string including null terminator. */
#define CAN_TRANSPORT_UDP_MAX_IP_LEN 16U
/** Fixed CAN-over-UDP payload size in bytes. */
#define CAN_TRANSPORT_UDP_WIRE_SIZE  13U

/**
 * @brief Transport instance state for CAN-over-UDP communication.
 */
typedef struct
{
  /** Backing UDP socket. */
  UdpSocket *socket;
  /** Remote endpoint IPv4 address string. */
  char remote_ip[CAN_TRANSPORT_UDP_MAX_IP_LEN];
  /** Remote endpoint UDP port. */
  uint16_t remote_port;
  /** Initialization state flag. */
  bool initialized;
} CanTransportUdp;

/**
 * @brief Initializes transport state with a destination endpoint.
 *
 * @param transport Transport instance to initialize.
 * @param socket Initialized UDP socket to use.
 * @param remote_ip Destination IPv4 address.
 * @param remote_port Destination UDP port.
 * @return true on success, false otherwise.
 */
bool can_transport_udp_init(CanTransportUdp *transport, UdpSocket *socket, const char *remote_ip,
                            uint16_t remote_port);

/**
 * @brief Encodes one CAN frame into wire payload format.
 *
 * @param frame Input frame.
 * @param out_payload Output payload buffer.
 * @param payload_len Output buffer length.
 * @return true on success, false otherwise.
 */
bool can_transport_udp_encode(const CanFrame *frame, uint8_t *out_payload, size_t payload_len);

/**
 * @brief Decodes one wire payload into CAN frame format.
 *
 * @param payload Input payload bytes.
 * @param payload_len Input payload length.
 * @param out_frame Output frame.
 * @return true on success, false otherwise.
 */
bool can_transport_udp_decode(const uint8_t *payload, size_t payload_len, CanFrame *out_frame);

/**
 * @brief Encodes and sends one CAN frame to configured endpoint.
 *
 * @param transport Initialized transport instance.
 * @param frame Frame to send.
 * @return true on success, false otherwise.
 */
bool can_transport_udp_send_frame(const CanTransportUdp *transport, const CanFrame *frame);

/**
 * @brief Receives and decodes one CAN frame from UDP socket.
 *
 * @param transport Initialized transport instance.
 * @param out_frame Output frame buffer.
 * @param out_sender_ip Optional output sender IP buffer.
 * @param sender_ip_len Sender IP buffer size.
 * @param out_sender_port Optional output sender UDP port.
 * @return true if a valid frame was received and decoded.
 */
bool can_transport_udp_receive_frame(CanTransportUdp *transport, CanFrame *out_frame,
                                     char *out_sender_ip, size_t sender_ip_len,
                                     uint16_t *out_sender_port);

#ifdef __cplusplus
}
#endif

#endif /* CAN_TRANSPORT_UDP_H */
