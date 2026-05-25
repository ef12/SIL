/**
 * @file can_transport_udp.c
 * @brief CAN-over-UDP transport — encode, decode, send, and receive.
 */

#include "can_transport_udp.h"

#include <string.h>

static bool is_valid_transport(const CanTransportUdp *transport)
{
  if (transport == NULL || !transport->initialized || transport->socket == NULL)
  {
    return false;
  }

  return transport->socket->initialized;
}

bool can_transport_udp_init(CanTransportUdp *transport, UdpSocket *socket, const char *remote_ip,
                            uint16_t remote_port)
{
  size_t ip_len;

  if (transport == NULL || socket == NULL || remote_ip == NULL)
  {
    return false;
  }

  ip_len = strlen(remote_ip);
  if (ip_len == 0U || ip_len >= CAN_TRANSPORT_UDP_MAX_IP_LEN)
  {
    return false;
  }

  (void)memset(transport, 0, sizeof(*transport));
  transport->socket = socket;
  (void)memcpy(transport->remote_ip, remote_ip, ip_len);
  transport->remote_ip[ip_len] = '\0';
  transport->remote_port = remote_port;
  transport->initialized = true;
  return true;
}

bool can_transport_udp_encode(const CanFrame *frame, uint8_t *out_payload, size_t payload_len)
{
  if (!can_frame_is_valid(frame) || out_payload == NULL
      || payload_len < CAN_TRANSPORT_UDP_WIRE_SIZE)
  {
    return false;
  }

  out_payload[0] = (uint8_t)((frame->id >> 24U) & 0xFFU);
  out_payload[1] = (uint8_t)((frame->id >> 16U) & 0xFFU);
  out_payload[2] = (uint8_t)((frame->id >> 8U) & 0xFFU);
  out_payload[3] = (uint8_t)(frame->id & 0xFFU);
  out_payload[4] = frame->dlc;
  (void)memcpy(&out_payload[5], frame->data, CAN_FRAME_MAX_DATA_LEN);
  return true;
}

bool can_transport_udp_decode(const uint8_t *payload, size_t payload_len, CanFrame *out_frame)
{
  if (payload == NULL || out_frame == NULL || payload_len < CAN_TRANSPORT_UDP_WIRE_SIZE)
  {
    return false;
  }

  out_frame->id = ((uint32_t)payload[0] << 24U) | ((uint32_t)payload[1] << 16U)
                  | ((uint32_t)payload[2] << 8U) | (uint32_t)payload[3];
  out_frame->dlc = payload[4];
  (void)memcpy(out_frame->data, &payload[5], CAN_FRAME_MAX_DATA_LEN);

  return can_frame_is_valid(out_frame);
}

bool can_transport_udp_send_frame(const CanTransportUdp *transport, const CanFrame *frame)
{
  uint8_t payload[CAN_TRANSPORT_UDP_WIRE_SIZE];

  if (!is_valid_transport(transport))
  {
    return false;
  }

  if (!can_transport_udp_encode(frame, payload, sizeof(payload)))
  {
    return false;
  }

  return udp_socket_send_to(transport->socket, transport->remote_ip, transport->remote_port,
                            payload, sizeof(payload));
}

bool can_transport_udp_receive_frame(CanTransportUdp *transport, CanFrame *out_frame,
                                     char *out_sender_ip, size_t sender_ip_len,
                                     uint16_t *out_sender_port)
{
  uint8_t payload[CAN_TRANSPORT_UDP_WIRE_SIZE];
  int received;

  if (!is_valid_transport(transport) || out_frame == NULL)
  {
    return false;
  }

  received = udp_socket_receive_from(transport->socket, payload, sizeof(payload), out_sender_ip,
                                     sender_ip_len, out_sender_port);
  if (received != (int)sizeof(payload))
  {
    return false;
  }

  return can_transport_udp_decode(payload, sizeof(payload), out_frame);
}
