#include "unity.h"

#include "can_driver.h"
#include "can_emulator.h"
#include "can_frame.h"


#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

static CanDriverFrame make_frame(uint32_t id, const uint8_t *data, uint8_t dlc)
{
  CanDriverFrame frame;

  frame.id = id;
  frame.dlc = dlc;
  (void)memset(frame.data, 0, sizeof(frame.data));
  if (data != NULL && dlc > 0U)
  {
    (void)memcpy(frame.data, data, dlc);
  }

  return frame;
}

void test_can_driver_init_registers_node_on_emulator(void)
{
  CanEmulator emulator;
  CanDriver driver;

  can_emulator_init(&emulator);

  TEST_ASSERT_TRUE(can_driver_init(&driver, &emulator, 7U));
  TEST_ASSERT_TRUE(driver.initialized);
}

void test_can_driver_send_step_and_receive_between_nodes(void)
{
  CanEmulator emulator;
  CanDriver sender_driver;
  CanDriver receiver_driver;
  CanDriverFrame tx_frame;
  CanDriverFrame rx_frame;
  CanDriverNodeId rx_sender;
  const uint8_t payload[] = {0x01U, 0x02U, 0x03U};

  can_emulator_init(&emulator);

  TEST_ASSERT_TRUE(can_driver_init(&sender_driver, &emulator, 1U));
  TEST_ASSERT_TRUE(can_driver_init(&receiver_driver, &emulator, 2U));

  tx_frame = make_frame(0x18FF50E5U, payload, 3U);

  TEST_ASSERT_TRUE(can_driver_send(&sender_driver, &tx_frame));
  TEST_ASSERT_EQUAL_UINT(1U, can_driver_pending_tx_count(&sender_driver));

  TEST_ASSERT_TRUE(can_driver_step_bus(&sender_driver));
  TEST_ASSERT_EQUAL_UINT(0U, can_driver_pending_tx_count(&sender_driver));

  TEST_ASSERT_TRUE(can_driver_receive(&receiver_driver, &rx_frame, &rx_sender));
  TEST_ASSERT_EQUAL_HEX32(tx_frame.id, rx_frame.id);
  TEST_ASSERT_EQUAL_UINT8(tx_frame.dlc, rx_frame.dlc);
  TEST_ASSERT_EQUAL_MEMORY(tx_frame.data, rx_frame.data, tx_frame.dlc);
  TEST_ASSERT_EQUAL_UINT8(1U, rx_sender);

  TEST_ASSERT_FALSE(can_driver_receive(&sender_driver, &rx_frame, &rx_sender));
}

void test_can_driver_enforces_arbitration_through_emulator(void)
{
  CanEmulator emulator;
  CanDriver node_a;
  CanDriver node_b;
  CanDriver observer;
  CanDriverFrame frame_a;
  CanDriverFrame frame_b;
  CanDriverFrame rx;
  CanDriverNodeId sender;

  can_emulator_init(&emulator);

  TEST_ASSERT_TRUE(can_driver_init(&node_a, &emulator, 10U));
  TEST_ASSERT_TRUE(can_driver_init(&node_b, &emulator, 11U));
  TEST_ASSERT_TRUE(can_driver_init(&observer, &emulator, 12U));

  frame_a = make_frame(0x300U, NULL, 0U);
  frame_b = make_frame(0x100U, NULL, 0U);

  TEST_ASSERT_TRUE(can_driver_send(&node_a, &frame_a));
  TEST_ASSERT_TRUE(can_driver_send(&node_b, &frame_b));

  TEST_ASSERT_TRUE(can_driver_step_bus(&observer));
  TEST_ASSERT_TRUE(can_driver_step_bus(&observer));

  TEST_ASSERT_TRUE(can_driver_receive(&observer, &rx, &sender));
  TEST_ASSERT_EQUAL_HEX32(0x100U, rx.id);
  TEST_ASSERT_EQUAL_UINT8(11U, sender);

  TEST_ASSERT_TRUE(can_driver_receive(&observer, &rx, &sender));
  TEST_ASSERT_EQUAL_HEX32(0x300U, rx.id);
  TEST_ASSERT_EQUAL_UINT8(10U, sender);
}

void test_can_driver_rejects_invalid_inputs(void)
{
  CanEmulator emulator;
  CanDriver driver;
  CanDriverFrame invalid_frame;
  CanDriverFrame frame;

  can_emulator_init(&emulator);

  invalid_frame = make_frame(0x123U, NULL, 9U);
  frame = make_frame(0x123U, NULL, 0U);

  TEST_ASSERT_FALSE(can_driver_init(NULL, &emulator, 1U));
  TEST_ASSERT_FALSE(can_driver_init(&driver, NULL, 1U));
  TEST_ASSERT_FALSE(can_driver_send(NULL, &frame));
  TEST_ASSERT_FALSE(can_driver_send(&driver, &frame));

  TEST_ASSERT_TRUE(can_driver_init(&driver, &emulator, 1U));
  TEST_ASSERT_FALSE(can_driver_send(&driver, &invalid_frame));
  TEST_ASSERT_FALSE(can_driver_receive(&driver, NULL, NULL));
}
