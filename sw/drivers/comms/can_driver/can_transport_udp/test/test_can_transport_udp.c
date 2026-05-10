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
