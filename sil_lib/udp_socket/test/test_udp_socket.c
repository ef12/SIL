#include "unity.h"

#include "udp_socket.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

void setUp(void)
{
}

void tearDown(void)
{
}

void test_udp_socket_init_and_close_succeeds(void)
{
  UdpSocket socket_under_test;

  TEST_ASSERT_TRUE(udp_socket_init(&socket_under_test, 7101));
  TEST_ASSERT_TRUE(socket_under_test.initialized);

  udp_socket_close(&socket_under_test);
  TEST_ASSERT_FALSE(socket_under_test.initialized);
}

void test_udp_socket_send_and_receive_on_loopback(void)
{
  UdpSocket sender;
  UdpSocket receiver;
  const char payload[] = "can-over-udp";
  char buffer[64] = {0};
  char sender_ip[32] = {0};
  uint16_t sender_port = 0;
  int received;

  TEST_ASSERT_TRUE(udp_socket_init(&sender, 7102));
  TEST_ASSERT_TRUE(udp_socket_init(&receiver, 7103));

  TEST_ASSERT_TRUE(udp_socket_send_to(&sender, "127.0.0.1", 7103, payload, strlen(payload)));

  received = udp_socket_receive_from(&receiver, buffer, sizeof(buffer), sender_ip,
                                     sizeof(sender_ip), &sender_port);

  TEST_ASSERT_EQUAL_INT((int)strlen(payload), received);
  TEST_ASSERT_EQUAL_MEMORY(payload, buffer, strlen(payload));
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", sender_ip);
  TEST_ASSERT_EQUAL_UINT16(7102, sender_port);

  udp_socket_close(&receiver);
  udp_socket_close(&sender);
}

void test_udp_socket_send_fails_with_invalid_inputs(void)
{
  UdpSocket socket_under_test;
  const char payload[] = "x";

  TEST_ASSERT_FALSE(udp_socket_send_to(NULL, "127.0.0.1", 7103, payload, 1));

  TEST_ASSERT_TRUE(udp_socket_init(&socket_under_test, 7104));
  TEST_ASSERT_FALSE(udp_socket_send_to(&socket_under_test, NULL, 7103, payload, 1));
  TEST_ASSERT_FALSE(udp_socket_send_to(&socket_under_test, "127.0.0.1", 7103, NULL, 1));
  TEST_ASSERT_FALSE(udp_socket_send_to(&socket_under_test, "not-an-ip", 7103, payload, 1));

  udp_socket_close(&socket_under_test);
}

void test_udp_socket_non_blocking_receive_returns_immediately_when_no_data(void)
{
  UdpSocket receiver;
  char buffer[8] = {0};
  ULONGLONG t0;
  ULONGLONG t1;
  int received;

  TEST_ASSERT_TRUE(udp_socket_init(&receiver, 7105));
  TEST_ASSERT_TRUE(udp_socket_set_non_blocking(&receiver, true));

  t0 = GetTickCount64();
  received = udp_socket_receive_from(&receiver, buffer, sizeof(buffer), NULL, 0, NULL);
  t1 = GetTickCount64();

  TEST_ASSERT_EQUAL_INT(-1, received);
  TEST_ASSERT_TRUE((t1 - t0) < 100ULL);

  udp_socket_close(&receiver);
}

void test_udp_socket_receive_timeout_expires_when_no_data(void)
{
  UdpSocket receiver;
  char buffer[8] = {0};
  ULONGLONG t0;
  ULONGLONG t1;
  int received;

  TEST_ASSERT_TRUE(udp_socket_init(&receiver, 7106));
  TEST_ASSERT_TRUE(udp_socket_set_non_blocking(&receiver, false));
  TEST_ASSERT_TRUE(udp_socket_set_receive_timeout(&receiver, 120));

  t0 = GetTickCount64();
  received = udp_socket_receive_from(&receiver, buffer, sizeof(buffer), NULL, 0, NULL);
  t1 = GetTickCount64();

  TEST_ASSERT_EQUAL_INT(-1, received);
  TEST_ASSERT_TRUE((t1 - t0) >= 80ULL);
  TEST_ASSERT_TRUE((t1 - t0) < 1000ULL);

  udp_socket_close(&receiver);
}

void test_udp_socket_mode_configuration_fails_for_invalid_inputs(void)
{
  UdpSocket receiver;

  TEST_ASSERT_FALSE(udp_socket_set_non_blocking(NULL, true));
  TEST_ASSERT_FALSE(udp_socket_set_receive_timeout(NULL, 100));

  receiver.initialized = false;
  receiver.native_socket = 0;
  TEST_ASSERT_FALSE(udp_socket_set_non_blocking(&receiver, true));
  TEST_ASSERT_FALSE(udp_socket_set_receive_timeout(&receiver, 100));
}

void test_udp_socket_init_returns_false_when_null(void)
{
  TEST_ASSERT_FALSE(udp_socket_init(NULL, 7110));
}

void test_udp_socket_receive_from_returns_neg1_for_null_socket(void)
{
  char buffer[8];

  TEST_ASSERT_EQUAL_INT(-1, udp_socket_receive_from(NULL, buffer, sizeof(buffer), NULL, 0, NULL));
}

void test_udp_socket_receive_from_returns_neg1_for_null_buffer(void)
{
  UdpSocket sock;

  TEST_ASSERT_TRUE(udp_socket_init(&sock, 7111));
  TEST_ASSERT_EQUAL_INT(-1, udp_socket_receive_from(&sock, NULL, 8, NULL, 0, NULL));
  udp_socket_close(&sock);
}

void test_udp_socket_receive_from_returns_neg1_when_not_initialized(void)
{
  UdpSocket sock;
  char buffer[8];

  sock.initialized = false;
  sock.native_socket = 0;

  TEST_ASSERT_EQUAL_INT(-1, udp_socket_receive_from(&sock, buffer, sizeof(buffer), NULL, 0, NULL));
}

void test_udp_socket_close_is_safe_when_null(void)
{
  udp_socket_close(NULL); /* must not crash */
}

void test_udp_socket_close_is_safe_when_not_initialized(void)
{
  UdpSocket sock;

  sock.initialized = false;
  sock.native_socket = 0;

  udp_socket_close(&sock); /* must not crash */
}

void test_udp_socket_send_to_fails_when_not_initialized(void)
{
  UdpSocket sock;
  const char payload[] = "x";

  sock.initialized = false;
  sock.native_socket = 0;

  TEST_ASSERT_FALSE(udp_socket_send_to(&sock, "127.0.0.1", 7112, payload, 1));
}