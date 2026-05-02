#include "unity.h"

#include "udp_socket.h"

#include <string.h>

void setUp(void) {}

void tearDown(void) {}

void test_udp_socket_init_and_close_succeeds(void) {
  UdpSocket socket_under_test;

  TEST_ASSERT_TRUE(udp_socket_init(&socket_under_test, 7101));
  TEST_ASSERT_TRUE(socket_under_test.initialized);

  udp_socket_close(&socket_under_test);
  TEST_ASSERT_FALSE(socket_under_test.initialized);
}

void test_udp_socket_send_and_receive_on_loopback(void) {
  UdpSocket sender;
  UdpSocket receiver;
  const char payload[] = "can-over-udp";
  char buffer[64] = {0};
  char sender_ip[32] = {0};
  uint16_t sender_port = 0;
  int received;

  TEST_ASSERT_TRUE(udp_socket_init(&sender, 7102));
  TEST_ASSERT_TRUE(udp_socket_init(&receiver, 7103));

  TEST_ASSERT_TRUE(
      udp_socket_send_to(&sender, "127.0.0.1", 7103, payload, strlen(payload)));

  received =
      udp_socket_receive_from(&receiver, buffer, sizeof(buffer), sender_ip,
                              sizeof(sender_ip), &sender_port);

  TEST_ASSERT_EQUAL_INT((int)strlen(payload), received);
  TEST_ASSERT_EQUAL_MEMORY(payload, buffer, strlen(payload));
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", sender_ip);
  TEST_ASSERT_EQUAL_UINT16(7102, sender_port);

  udp_socket_close(&receiver);
  udp_socket_close(&sender);
}

void test_udp_socket_send_fails_with_invalid_inputs(void) {
  UdpSocket socket_under_test;
  const char payload[] = "x";

  TEST_ASSERT_FALSE(udp_socket_send_to(NULL, "127.0.0.1", 7103, payload, 1));

  TEST_ASSERT_TRUE(udp_socket_init(&socket_under_test, 7104));
  TEST_ASSERT_FALSE(
      udp_socket_send_to(&socket_under_test, NULL, 7103, payload, 1));
  TEST_ASSERT_FALSE(
      udp_socket_send_to(&socket_under_test, "127.0.0.1", 7103, NULL, 1));
  TEST_ASSERT_FALSE(
      udp_socket_send_to(&socket_under_test, "not-an-ip", 7103, payload, 1));

  udp_socket_close(&socket_under_test);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_udp_socket_init_and_close_succeeds);
  RUN_TEST(test_udp_socket_send_and_receive_on_loopback);
  RUN_TEST(test_udp_socket_send_fails_with_invalid_inputs);
  return UNITY_END();
}