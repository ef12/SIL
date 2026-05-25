/**
 * @file io_transport_udp.c
 * @brief IO-over-UDP transport — serialization, send, and receive.
 */

#include "io_transport_udp.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define IO_TRANSPORT_UDP_HEADER_SIZE 6U

static bool is_valid_transport(const IoTransportUdp *transport)
{
  if (transport == NULL || !transport->initialized || transport->socket == NULL)
  {
    return false;
  }

  if (!transport->socket->initialized)
  {
    return false;
  }

  if (transport->digital_pin_count > 0U && transport->digital_values == NULL)
  {
    return false;
  }

  if (transport->analog_pin_count > 0U && transport->analog_values == NULL)
  {
    return false;
  }

  return true;
}

static bool encode_payload(const IoTransportUdp *transport, uint8_t *payload, size_t payload_len)
{
  size_t i;

  if (!is_valid_transport(transport) || payload == NULL)
  {
    return false;
  }

  if (payload_len
      != io_transport_udp_wire_size(transport->digital_pin_count, transport->analog_pin_count))
  {
    return false;
  }

  payload[0] = 'I';
  payload[1] = 'O';
  payload[2] = IO_TRANSPORT_UDP_VERSION;
  payload[3] = (uint8_t)(transport->digital_pin_count & 0xFFU);
  payload[4] = (uint8_t)(transport->analog_pin_count & 0xFFU);
  payload[5] = 0U;

  for (i = 0; i < transport->digital_pin_count; ++i)
  {
    payload[IO_TRANSPORT_UDP_HEADER_SIZE + i] = transport->digital_values[i] ? 1U : 0U;
  }

  for (i = 0; i < transport->analog_pin_count; ++i)
  {
    size_t offset = IO_TRANSPORT_UDP_HEADER_SIZE + transport->digital_pin_count + (2U * i);
    uint16_t value = transport->analog_values[i];
    payload[offset] = (uint8_t)(value & 0xFFU);
    payload[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
  }

  return true;
}

static bool decode_payload(IoTransportUdp *transport, const uint8_t *payload, size_t payload_len)
{
  size_t i;
  size_t expected_size;
  size_t digital_count;
  size_t analog_count;

  if (!is_valid_transport(transport) || payload == NULL
      || payload_len < IO_TRANSPORT_UDP_HEADER_SIZE)
  {
    return false;
  }

  if (payload[0] != 'I' || payload[1] != 'O' || payload[2] != IO_TRANSPORT_UDP_VERSION)
  {
    return false;
  }

  digital_count = (size_t)payload[3];
  analog_count = (size_t)payload[4];

  if (digital_count != transport->digital_pin_count || analog_count != transport->analog_pin_count)
  {
    return false;
  }

  expected_size = io_transport_udp_wire_size(digital_count, analog_count);
  if (expected_size == 0U || payload_len != expected_size)
  {
    return false;
  }

  for (i = 0; i < transport->digital_pin_count; ++i)
  {
    transport->digital_values[i] = (payload[IO_TRANSPORT_UDP_HEADER_SIZE + i] != 0U);
  }

  for (i = 0; i < transport->analog_pin_count; ++i)
  {
    size_t offset = IO_TRANSPORT_UDP_HEADER_SIZE + transport->digital_pin_count + (2U * i);
    transport->analog_values[i] =
        (uint16_t)payload[offset] | (uint16_t)(((uint16_t)payload[offset + 1U]) << 8U);
  }

  return true;
}

bool io_transport_udp_init(IoTransportUdp *transport, UdpSocket *socket, const char *remote_ip,
                           uint16_t remote_port, bool *digital_values, size_t digital_pin_count,
                           uint16_t *analog_values, size_t analog_pin_count)
{
  size_t ip_len;

  if (transport == NULL || socket == NULL || remote_ip == NULL)
  {
    return false;
  }

  if (io_transport_udp_wire_size(digital_pin_count, analog_pin_count) == 0U)
  {
    return false;
  }

  if ((digital_pin_count > 0U && digital_values == NULL)
      || (analog_pin_count > 0U && analog_values == NULL))
  {
    return false;
  }

  ip_len = strlen(remote_ip);
  if (ip_len == 0U || ip_len >= IO_TRANSPORT_UDP_MAX_IP_LEN)
  {
    return false;
  }

  (void)memset(transport, 0, sizeof(*transport));
  transport->socket = socket;
  (void)memcpy(transport->remote_ip, remote_ip, ip_len);
  transport->remote_ip[ip_len] = '\0';
  transport->remote_port = remote_port;
  transport->digital_values = digital_values;
  transport->digital_pin_count = digital_pin_count;
  transport->analog_values = analog_values;
  transport->analog_pin_count = analog_pin_count;

  /* Initialize buffers to zero. */
  if (digital_pin_count > 0U && digital_values != NULL)
  {
    (void)memset(digital_values, 0, digital_pin_count * sizeof(*digital_values));
  }
  if (analog_pin_count > 0U && analog_values != NULL)
  {
    (void)memset(analog_values, 0, analog_pin_count * sizeof(*analog_values));
  }

  transport->initialized = true;
  return true;
}

size_t io_transport_udp_wire_size(size_t digital_pin_count, size_t analog_pin_count)
{
  size_t analog_bytes;

  if (digital_pin_count > UCHAR_MAX || analog_pin_count > UCHAR_MAX)
  {
    return 0U;
  }

  if (analog_pin_count > (SIZE_MAX / 2U))
  {
    return 0U;
  }

  analog_bytes = analog_pin_count * 2U;

  if (IO_TRANSPORT_UDP_HEADER_SIZE > (SIZE_MAX - digital_pin_count))
  {
    return 0U;
  }

  if ((IO_TRANSPORT_UDP_HEADER_SIZE + digital_pin_count) > (SIZE_MAX - analog_bytes))
  {
    return 0U;
  }

  return IO_TRANSPORT_UDP_HEADER_SIZE + digital_pin_count + analog_bytes;
}

bool io_transport_udp_send(const IoTransportUdp *transport)
{
  uint8_t *payload;
  size_t payload_len;
  bool success;

  if (!is_valid_transport(transport))
  {
    return false;
  }

  payload_len =
      io_transport_udp_wire_size(transport->digital_pin_count, transport->analog_pin_count);
  if (payload_len == 0U)
  {
    return false;
  }

  payload = (uint8_t *)malloc(payload_len);
  if (payload == NULL)
  {
    return false;
  }

  success = encode_payload(transport, payload, payload_len)
            && udp_socket_send_to(transport->socket, transport->remote_ip, transport->remote_port,
                                  payload, payload_len);

  free(payload);
  return success;
}

bool io_transport_udp_receive(IoTransportUdp *transport, char *out_sender_ip, size_t sender_ip_len,
                              uint16_t *out_sender_port)
{
  uint8_t *payload;
  size_t payload_len;
  int received;
  bool success;

  if (!is_valid_transport(transport))
  {
    return false;
  }

  payload_len =
      io_transport_udp_wire_size(transport->digital_pin_count, transport->analog_pin_count);
  if (payload_len == 0U)
  {
    return false;
  }

  payload = (uint8_t *)malloc(payload_len);
  if (payload == NULL)
  {
    return false;
  }

  received = udp_socket_receive_from(transport->socket, payload, payload_len, out_sender_ip,
                                     sender_ip_len, out_sender_port);
  success = (received == (int)payload_len) && decode_payload(transport, payload, payload_len);

  free(payload);
  return success;
}

bool io_transport_udp_digital_get(const IoTransportUdp *transport, size_t pin, bool *out_value)
{
  if (transport == NULL || !transport->initialized || out_value == NULL)
  {
    return false;
  }

  if (pin >= transport->digital_pin_count)
  {
    return false;
  }

  if (transport->digital_values == NULL)
  {
    return false;
  }

  *out_value = transport->digital_values[pin];
  return true;
}

bool io_transport_udp_digital_set(IoTransportUdp *transport, size_t pin, bool value)
{
  if (transport == NULL || !transport->initialized)
  {
    return false;
  }

  if (pin >= transport->digital_pin_count)
  {
    return false;
  }

  if (transport->digital_values == NULL)
  {
    return false;
  }

  transport->digital_values[pin] = value;
  return true;
}

bool io_transport_udp_analog_get(const IoTransportUdp *transport, size_t pin, uint16_t *out_value)
{
  if (transport == NULL || !transport->initialized || out_value == NULL)
  {
    return false;
  }

  if (pin >= transport->analog_pin_count)
  {
    return false;
  }

  if (transport->analog_values == NULL)
  {
    return false;
  }

  *out_value = transport->analog_values[pin];
  return true;
}

bool io_transport_udp_analog_set(IoTransportUdp *transport, size_t pin, uint16_t value)
{
  if (transport == NULL || !transport->initialized)
  {
    return false;
  }

  if (pin >= transport->analog_pin_count)
  {
    return false;
  }

  if (transport->analog_values == NULL)
  {
    return false;
  }

  transport->analog_values[pin] = value;
  return true;
}
