/**
 * @file test_sil_config_udp_socket.c
 * @brief Unit tests for sil_config_udp_socket — mocks udp_socket to isolate BSP wiring.
 */

#include "unity.h"

#include "sil_config_udp_socket.h"

#include "Mockudp_socket.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* ----------------------------------------------------------------
 *  NULL guard
 * ---------------------------------------------------------------- */

void test_init_returns_false_when_socket_is_null(void)
{
  TEST_ASSERT_FALSE(sil_config_udp_socket_init(NULL, 5000U, 100U));
}

/* ----------------------------------------------------------------
 *  Happy path — no timeout
 * ---------------------------------------------------------------- */

void test_init_succeeds_without_timeout(void)
{
  UdpSocket socket;

  udp_socket_init_ExpectAndReturn(&socket, 5000U, true);
  udp_socket_set_non_blocking_ExpectAndReturn(&socket, true, true);

  TEST_ASSERT_TRUE(sil_config_udp_socket_init(&socket, 5000U, 0U));
}

/* ----------------------------------------------------------------
 *  Happy path — with timeout (ignored, non-blocking used instead)
 * ---------------------------------------------------------------- */

void test_init_succeeds_with_timeout(void)
{
  UdpSocket socket;

  udp_socket_init_ExpectAndReturn(&socket, 5001U, true);
  udp_socket_set_non_blocking_ExpectAndReturn(&socket, true, true);

  TEST_ASSERT_TRUE(sil_config_udp_socket_init(&socket, 5001U, 200U));
}

/* ----------------------------------------------------------------
 *  udp_socket_init fails
 * ---------------------------------------------------------------- */

void test_init_returns_false_when_socket_init_fails(void)
{
  UdpSocket socket;

  udp_socket_init_ExpectAndReturn(&socket, 5002U, false);

  TEST_ASSERT_FALSE(sil_config_udp_socket_init(&socket, 5002U, 0U));
}

/* ----------------------------------------------------------------
 *  udp_socket_set_non_blocking fails — socket must be closed
 * ---------------------------------------------------------------- */

void test_init_closes_socket_when_non_blocking_setup_fails(void)
{
  UdpSocket socket;

  udp_socket_init_ExpectAndReturn(&socket, 5003U, true);
  udp_socket_set_non_blocking_ExpectAndReturn(&socket, true, false);
  udp_socket_close_Expect(&socket);

  TEST_ASSERT_FALSE(sil_config_udp_socket_init(&socket, 5003U, 50U));
}
