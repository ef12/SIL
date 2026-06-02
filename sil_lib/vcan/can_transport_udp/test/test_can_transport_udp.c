#include "unity.h"

#include "can_frame.h"
#include "can_transport_udp.h"
#include "udp_socket.h"

#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

static CanFrame make_frame(uint32_t id, const uint8_t *data, uint8_t dlc)
{
  CanFrame frame;

  can_frame_clear(&frame);
  frame.id = id;
  frame.dlc = dlc;
  if (data != NULL && dlc > 0U)
  {
    (void)memcpy(frame.data, data, dlc);
  }

  return frame;
}

void test_can_transport_udp_encode_and_decode_roundtrip(void)
{
  uint8_t payload[CAN_TRANSPORT_UDP_WIRE_SIZE];
  CanFrame tx;
  CanFrame rx;
  const uint8_t data[] = {0x10U, 0x20U, 0x30U, 0x40U};

  tx = make_frame(0x18FFAA55U, data, 4U);

  TEST_ASSERT_TRUE(can_transport_udp_encode(&tx, payload, sizeof(payload)));
  TEST_ASSERT_TRUE(can_transport_udp_decode(payload, sizeof(payload), &rx));
  TEST_ASSERT_EQUAL_HEX32(tx.id, rx.id);
  TEST_ASSERT_EQUAL_UINT8(tx.dlc, rx.dlc);
  TEST_ASSERT_EQUAL_MEMORY(tx.data, rx.data, CAN_FRAME_MAX_DATA_LEN);
}

void test_can_transport_udp_rejects_invalid_inputs(void)
{
  uint8_t payload[CAN_TRANSPORT_UDP_WIRE_SIZE];
  CanFrame frame;
  CanTransportUdp transport;

  frame = make_frame(0x123U, NULL, 9U);

  TEST_ASSERT_FALSE(can_transport_udp_init(NULL, NULL, "127.0.0.1", 7101));
  TEST_ASSERT_FALSE(can_transport_udp_encode(NULL, payload, sizeof(payload)));
  TEST_ASSERT_FALSE(can_transport_udp_encode(&frame, payload, sizeof(payload)));
  TEST_ASSERT_FALSE(can_transport_udp_decode(NULL, sizeof(payload), &frame));
  TEST_ASSERT_FALSE(can_transport_udp_decode(payload, sizeof(payload) - 1U, &frame));
  TEST_ASSERT_FALSE(can_transport_udp_send_frame(&transport, &frame));
}

void test_can_transport_udp_sends_and_receives_over_loopback(void)
{
  UdpSocket sender_socket;
  UdpSocket receiver_socket;
  CanTransportUdp tx_transport;
  CanTransportUdp rx_transport;
  CanFrame tx;
  CanFrame rx;
  char sender_ip[32] = {0};
  uint16_t sender_port = 0;
  const uint8_t data[] = {0xABU, 0xCDU};

  tx = make_frame(0x7DFU, data, 2U);

  TEST_ASSERT_TRUE(udp_socket_init(&sender_socket, 7201));
  TEST_ASSERT_TRUE(udp_socket_init(&receiver_socket, 7202));

  TEST_ASSERT_TRUE(can_transport_udp_init(&tx_transport, &sender_socket, "127.0.0.1", 7202));
  TEST_ASSERT_TRUE(can_transport_udp_init(&rx_transport, &receiver_socket, "127.0.0.1", 7201));

  TEST_ASSERT_TRUE(can_transport_udp_send_frame(&tx_transport, &tx));
  TEST_ASSERT_TRUE(can_transport_udp_receive_frame(&rx_transport, &rx, sender_ip, sizeof(sender_ip),
                                                   &sender_port));

  TEST_ASSERT_EQUAL_HEX32(tx.id, rx.id);
  TEST_ASSERT_EQUAL_UINT8(tx.dlc, rx.dlc);
  TEST_ASSERT_EQUAL_MEMORY(tx.data, rx.data, CAN_FRAME_MAX_DATA_LEN);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", sender_ip);
  TEST_ASSERT_EQUAL_UINT16(7201U, sender_port);

  udp_socket_close(&receiver_socket);
  udp_socket_close(&sender_socket);
}

/* ----------------------------------------------------------------
 *  init — additional edge cases
 * ---------------------------------------------------------------- */

void test_can_transport_udp_init_rejects_empty_ip(void)
{
  CanTransportUdp transport;
  UdpSocket socket;

  (void)memset(&socket, 0, sizeof(socket));
  TEST_ASSERT_FALSE(can_transport_udp_init(&transport, &socket, "", 7210));
}

void test_can_transport_udp_init_rejects_oversized_ip(void)
{
  CanTransportUdp transport;
  UdpSocket socket;

  (void)memset(&socket, 0, sizeof(socket));
  /* 16 chars = CAN_TRANSPORT_UDP_MAX_IP_LEN, must be rejected */
  TEST_ASSERT_FALSE(can_transport_udp_init(&transport, &socket, "1234567890123456", 7211));
}

/* ----------------------------------------------------------------
 *  encode — additional edge cases
 * ---------------------------------------------------------------- */

void test_can_transport_udp_encode_rejects_null_payload(void)
{
  CanFrame frame = make_frame(0x100U, NULL, 0U);

  TEST_ASSERT_FALSE(can_transport_udp_encode(&frame, NULL, CAN_TRANSPORT_UDP_WIRE_SIZE));
}

void test_can_transport_udp_encode_rejects_short_buffer(void)
{
  uint8_t payload[CAN_TRANSPORT_UDP_WIRE_SIZE - 1U];
  CanFrame frame = make_frame(0x100U, NULL, 0U);

  TEST_ASSERT_FALSE(can_transport_udp_encode(&frame, payload, sizeof(payload)));
}

/* ----------------------------------------------------------------
 *  decode — additional edge cases
 * ---------------------------------------------------------------- */

void test_can_transport_udp_decode_rejects_null_out_frame(void)
{
  uint8_t payload[CAN_TRANSPORT_UDP_WIRE_SIZE] = {0};

  TEST_ASSERT_FALSE(can_transport_udp_decode(payload, sizeof(payload), NULL));
}

void test_can_transport_udp_decode_rejects_invalid_dlc_in_payload(void)
{
  uint8_t payload[CAN_TRANSPORT_UDP_WIRE_SIZE] = {0};
  CanFrame out;

  /* DLC = 9 in byte[4] — exceeds CAN_FRAME_MAX_DATA_LEN */
  payload[4] = 9U;

  TEST_ASSERT_FALSE(can_transport_udp_decode(payload, sizeof(payload), &out));
}

/* ----------------------------------------------------------------
 *  receive_frame — additional edge cases
 * ---------------------------------------------------------------- */

void test_can_transport_udp_receive_frame_returns_false_when_null_transport(void)
{
  CanFrame out;

  TEST_ASSERT_FALSE(can_transport_udp_receive_frame(NULL, &out, NULL, 0, NULL));
}

void test_can_transport_udp_receive_frame_returns_false_when_null_out_frame(void)
{
  UdpSocket socket;
  CanTransportUdp transport;

  TEST_ASSERT_TRUE(udp_socket_init(&socket, 7220));
  TEST_ASSERT_TRUE(can_transport_udp_init(&transport, &socket, "127.0.0.1", 7221));

  TEST_ASSERT_FALSE(can_transport_udp_receive_frame(&transport, NULL, NULL, 0, NULL));

  udp_socket_close(&socket);
}

void test_can_transport_udp_receive_frame_returns_false_when_no_data(void)
{
  UdpSocket socket;
  CanTransportUdp transport;
  CanFrame out;

  TEST_ASSERT_TRUE(udp_socket_init(&socket, 7222));
  TEST_ASSERT_TRUE(udp_socket_set_non_blocking(&socket, true));
  TEST_ASSERT_TRUE(can_transport_udp_init(&transport, &socket, "127.0.0.1", 7223));

  TEST_ASSERT_FALSE(can_transport_udp_receive_frame(&transport, &out, NULL, 0, NULL));

  udp_socket_close(&socket);
}

/* ----------------------------------------------------------------
 *  send_frame — uninitialised transport
 * ---------------------------------------------------------------- */

void test_can_transport_udp_send_frame_fails_when_not_initialized(void)
{
  CanTransportUdp transport;
  CanFrame frame = make_frame(0x100U, NULL, 0U);

  (void)memset(&transport, 0, sizeof(transport));
  transport.initialized = false;

  TEST_ASSERT_FALSE(can_transport_udp_send_frame(&transport, &frame));
}

void test_can_transport_udp_send_frame_fails_with_null_socket(void)
{
  CanTransportUdp transport;
  CanFrame frame = make_frame(0x100U, NULL, 0U);

  (void)memset(&transport, 0, sizeof(transport));
  transport.initialized = true;
  transport.socket = NULL;

  TEST_ASSERT_FALSE(can_transport_udp_send_frame(&transport, &frame));
}
