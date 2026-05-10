#include "unity.h"

#include "io.h"
#include "io_transport_udp.h"
#include "udp_socket.h"

#include <string.h>

static bool g_digital_storage[16U];
static uint16_t g_analog_storage[16U];
static bool g_digital_storage_alt[8U];
static uint16_t g_analog_storage_alt[8U];

static UdpSocket g_socket;
static IoTransportUdp g_transport;
static UdpSocket g_socket_alt;
static IoTransportUdp g_transport_alt;

void setUp(void)
{
  (void)memset(g_digital_storage, 0xFF, sizeof(g_digital_storage));
  (void)memset(g_analog_storage, 0xFF, sizeof(g_analog_storage));
  (void)memset(g_digital_storage_alt, 0xAA, sizeof(g_digital_storage_alt));
  (void)memset(g_analog_storage_alt, 0xAA, sizeof(g_analog_storage_alt));
  (void)memset(&g_socket, 0, sizeof(g_socket));
  (void)memset(&g_transport, 0, sizeof(g_transport));
  (void)memset(&g_socket_alt, 0, sizeof(g_socket_alt));
  (void)memset(&g_transport_alt, 0, sizeof(g_transport_alt));
}

void tearDown(void)
{
  if (g_socket.initialized)
  {
    udp_socket_close(&g_socket);
  }
  if (g_socket_alt.initialized)
  {
    udp_socket_close(&g_socket_alt);
  }
}

void test_io_init_placeholder(void)
{
  TEST_ASSERT_TRUE(io_init());
}

void test_io_transport_udp_digital_access(void)
{
  bool digital_value = true;

  TEST_ASSERT_TRUE(udp_socket_init(&g_socket, 0U));
  TEST_ASSERT_TRUE(io_transport_udp_init(&g_transport, &g_socket, "127.0.0.1", 0U,
                                         g_digital_storage, 16U, g_analog_storage, 16U));

  /* Read default zero value. */
  TEST_ASSERT_TRUE(io_transport_udp_digital_get(&g_transport, 0U, &digital_value));
  TEST_ASSERT_FALSE(digital_value);

  /* Write and read back. */
  TEST_ASSERT_TRUE(io_transport_udp_digital_set(&g_transport, 5U, true));
  TEST_ASSERT_TRUE(io_transport_udp_digital_get(&g_transport, 5U, &digital_value));
  TEST_ASSERT_TRUE(digital_value);
}

void test_io_transport_udp_analog_access(void)
{
  uint16_t analog_value = 1U;

  TEST_ASSERT_TRUE(udp_socket_init(&g_socket, 0U));
  TEST_ASSERT_TRUE(io_transport_udp_init(&g_transport, &g_socket, "127.0.0.1", 0U,
                                         g_digital_storage, 16U, g_analog_storage, 16U));

  /* Read default zero value. */
  TEST_ASSERT_TRUE(io_transport_udp_analog_get(&g_transport, 0U, &analog_value));
  TEST_ASSERT_EQUAL_UINT16(0U, analog_value);

  /* Write and read back. */
  TEST_ASSERT_TRUE(io_transport_udp_analog_set(&g_transport, 7U, 4095U));
  TEST_ASSERT_TRUE(io_transport_udp_analog_get(&g_transport, 7U, &analog_value));
  TEST_ASSERT_EQUAL_UINT16(4095U, analog_value);
}

void test_io_transport_udp_rejects_invalid_pins(void)
{
  bool digital_value = false;
  uint16_t analog_value = 0U;

  TEST_ASSERT_TRUE(udp_socket_init(&g_socket, 0U));
  TEST_ASSERT_TRUE(io_transport_udp_init(&g_transport, &g_socket, "127.0.0.1", 0U,
                                         g_digital_storage, 16U, g_analog_storage, 16U));

  TEST_ASSERT_FALSE(io_transport_udp_digital_set(&g_transport, 16U, true));
  TEST_ASSERT_FALSE(io_transport_udp_digital_get(&g_transport, 16U, &digital_value));

  TEST_ASSERT_FALSE(io_transport_udp_analog_set(&g_transport, 16U, 1U));
  TEST_ASSERT_FALSE(io_transport_udp_analog_get(&g_transport, 16U, &analog_value));
}

void test_io_transport_udp_rejects_invalid_resource_bindings(void)
{
  TEST_ASSERT_FALSE(io_transport_udp_init(NULL, &g_socket, "127.0.0.1", 0U, g_digital_storage, 16U,
                                          g_analog_storage, 16U));
  TEST_ASSERT_FALSE(io_transport_udp_init(&g_transport, NULL, "127.0.0.1", 0U, g_digital_storage,
                                          16U, g_analog_storage, 16U));
  TEST_ASSERT_FALSE(io_transport_udp_init(&g_transport, &g_socket, NULL, 0U, g_digital_storage, 16U,
                                          g_analog_storage, 16U));
  TEST_ASSERT_FALSE(io_transport_udp_init(&g_transport, &g_socket, "", 0U, g_digital_storage, 16U,
                                          g_analog_storage, 16U));
}
