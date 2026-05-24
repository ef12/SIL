#include "unity.h"

#include "io_transport_udp.h"
#include "udp_socket.h"

#include <string.h>

static bool g_tx_digital[4U];
static uint16_t g_tx_analog[3U];
static bool g_rx_digital[4U];
static uint16_t g_rx_analog[3U];

void setUp(void)
{
  (void)memset(g_tx_digital, 0, sizeof(g_tx_digital));
  (void)memset(g_tx_analog, 0, sizeof(g_tx_analog));
  (void)memset(g_rx_digital, 0, sizeof(g_rx_digital));
  (void)memset(g_rx_analog, 0, sizeof(g_rx_analog));
}

void tearDown(void)
{
}

void test_io_transport_udp_wire_size_matches_protocol_layout(void)
{
  TEST_ASSERT_EQUAL_UINT(6U, io_transport_udp_wire_size(0U, 0U));
  TEST_ASSERT_EQUAL_UINT(10U, io_transport_udp_wire_size(2U, 1U));
  TEST_ASSERT_EQUAL_UINT(16U, io_transport_udp_wire_size(4U, 3U));
}

void test_io_transport_udp_init_rejects_invalid_inputs(void)
{
  IoTransportUdp transport;
  UdpSocket socket;

  (void)memset(&transport, 0, sizeof(transport));
  (void)memset(&socket, 0, sizeof(socket));

  TEST_ASSERT_FALSE(
      io_transport_udp_init(NULL, &socket, "127.0.0.1", 7301, g_tx_digital, 4U, g_tx_analog, 3U));
  TEST_ASSERT_FALSE(io_transport_udp_init(&transport, NULL, "127.0.0.1", 7301, g_tx_digital, 4U,
                                          g_tx_analog, 3U));
  TEST_ASSERT_FALSE(
      io_transport_udp_init(&transport, &socket, NULL, 7301, g_tx_digital, 4U, g_tx_analog, 3U));
  TEST_ASSERT_FALSE(
      io_transport_udp_init(&transport, &socket, "", 7301, g_tx_digital, 4U, g_tx_analog, 3U));
  TEST_ASSERT_FALSE(
      io_transport_udp_init(&transport, &socket, "127.0.0.1", 7301, NULL, 1U, g_tx_analog, 3U));
  TEST_ASSERT_FALSE(
      io_transport_udp_init(&transport, &socket, "127.0.0.1", 7301, g_tx_digital, 4U, NULL, 1U));
}

void test_io_transport_udp_send_receive_loopback_syncs_buffers(void)
{
  UdpSocket tx_socket;
  UdpSocket rx_socket;
  IoTransportUdp tx_transport;
  IoTransportUdp rx_transport;
  char sender_ip[32] = {0};
  uint16_t sender_port = 0;

  g_tx_digital[0] = true;
  g_tx_digital[1] = false;
  g_tx_digital[2] = true;
  g_tx_digital[3] = true;
  g_tx_analog[0] = 100U;
  g_tx_analog[1] = 2048U;
  g_tx_analog[2] = 65535U;

  TEST_ASSERT_TRUE(udp_socket_init(&tx_socket, 7301));
  TEST_ASSERT_TRUE(udp_socket_init(&rx_socket, 7302));

  TEST_ASSERT_TRUE(io_transport_udp_init(&tx_transport, &tx_socket, "127.0.0.1", 7302, g_tx_digital,
                                         4U, g_tx_analog, 3U));
  TEST_ASSERT_TRUE(io_transport_udp_init(&rx_transport, &rx_socket, "127.0.0.1", 7301, g_rx_digital,
                                         4U, g_rx_analog, 3U));

  TEST_ASSERT_TRUE(io_transport_udp_send(&tx_transport));
  TEST_ASSERT_TRUE(
      io_transport_udp_receive(&rx_transport, sender_ip, sizeof(sender_ip), &sender_port));

  TEST_ASSERT_EQUAL_MEMORY(g_tx_digital, g_rx_digital, sizeof(g_tx_digital));
  TEST_ASSERT_EQUAL_MEMORY(g_tx_analog, g_rx_analog, sizeof(g_tx_analog));
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", sender_ip);
  TEST_ASSERT_EQUAL_UINT16(7301U, sender_port);

  udp_socket_close(&rx_socket);
  udp_socket_close(&tx_socket);
}

void test_io_transport_udp_rejects_payload_with_mismatched_buffer_sizes(void)
{
  UdpSocket tx_socket;
  UdpSocket rx_socket;
  IoTransportUdp tx_transport;
  IoTransportUdp rx_transport;
  bool rx_digital_small[2U] = {false};
  uint16_t rx_analog_small[1U] = {0U};

  g_tx_digital[0] = true;
  g_tx_analog[0] = 321U;

  TEST_ASSERT_TRUE(udp_socket_init(&tx_socket, 7311));
  TEST_ASSERT_TRUE(udp_socket_init(&rx_socket, 7312));

  TEST_ASSERT_TRUE(io_transport_udp_init(&tx_transport, &tx_socket, "127.0.0.1", 7312, g_tx_digital,
                                         4U, g_tx_analog, 3U));
  TEST_ASSERT_TRUE(io_transport_udp_init(&rx_transport, &rx_socket, "127.0.0.1", 7311,
                                         rx_digital_small, 2U, rx_analog_small, 1U));

  TEST_ASSERT_TRUE(io_transport_udp_send(&tx_transport));
  TEST_ASSERT_FALSE(io_transport_udp_receive(&rx_transport, NULL, 0U, NULL));

  udp_socket_close(&rx_socket);
  udp_socket_close(&tx_socket);
}
