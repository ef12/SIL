#include "unity.h"

#include "can_emulator.h"
#include "can_frame.h"


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

  frame.id = id;
  frame.dlc = dlc;
  (void)memset(frame.data, 0, sizeof(frame.data));
  if (data != NULL && dlc > 0U)
  {
    (void)memcpy(frame.data, data, dlc);
  }

  return frame;
}

void test_can_emulator_arbitrates_lower_can_id_first(void)
{
  CanEmulator emulator;
  CanFrame rx;
  CanNodeId sender;
  CanFrame frame_a;
  CanFrame frame_b;
  const uint8_t data_a[] = {0xAA};
  const uint8_t data_b[] = {0xBB};

  can_emulator_init(&emulator);
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 1U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 2U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 3U));

  frame_a = make_frame(0x200U, data_a, 1U);
  frame_b = make_frame(0x100U, data_b, 1U);

  TEST_ASSERT_TRUE(can_emulator_submit(&emulator, 1U, &frame_a));
  TEST_ASSERT_TRUE(can_emulator_submit(&emulator, 2U, &frame_b));

  TEST_ASSERT_TRUE(can_emulator_step(&emulator));
  TEST_ASSERT_TRUE(can_emulator_step(&emulator));

  TEST_ASSERT_TRUE(can_emulator_receive(&emulator, 3U, &rx, &sender));
  TEST_ASSERT_EQUAL_HEX32(0x100U, rx.id);
  TEST_ASSERT_EQUAL_UINT8(2U, sender);

  TEST_ASSERT_TRUE(can_emulator_receive(&emulator, 3U, &rx, &sender));
  TEST_ASSERT_EQUAL_HEX32(0x200U, rx.id);
  TEST_ASSERT_EQUAL_UINT8(1U, sender);
}

void test_can_emulator_tie_break_is_submission_order(void)
{
  CanEmulator emulator;
  CanFrame rx;
  CanNodeId sender;
  CanFrame frame_first;
  CanFrame frame_second;

  can_emulator_init(&emulator);
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 10U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 11U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 12U));

  frame_first = make_frame(0x123U, NULL, 0U);
  frame_second = make_frame(0x123U, NULL, 0U);

  TEST_ASSERT_TRUE(can_emulator_submit(&emulator, 10U, &frame_first));
  TEST_ASSERT_TRUE(can_emulator_submit(&emulator, 11U, &frame_second));

  TEST_ASSERT_TRUE(can_emulator_step(&emulator));
  TEST_ASSERT_TRUE(can_emulator_step(&emulator));

  TEST_ASSERT_TRUE(can_emulator_receive(&emulator, 12U, &rx, &sender));
  TEST_ASSERT_EQUAL_UINT8(10U, sender);

  TEST_ASSERT_TRUE(can_emulator_receive(&emulator, 12U, &rx, &sender));
  TEST_ASSERT_EQUAL_UINT8(11U, sender);
}

void test_can_emulator_routes_to_other_nodes_but_not_sender(void)
{
  CanEmulator emulator;
  CanFrame rx;
  CanFrame frame;

  can_emulator_init(&emulator);
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 1U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 2U));

  frame = make_frame(0x321U, NULL, 0U);

  TEST_ASSERT_TRUE(can_emulator_submit(&emulator, 1U, &frame));
  TEST_ASSERT_TRUE(can_emulator_step(&emulator));

  TEST_ASSERT_TRUE(can_emulator_receive(&emulator, 2U, &rx, NULL));
  TEST_ASSERT_EQUAL_HEX32(0x321U, rx.id);

  TEST_ASSERT_FALSE(can_emulator_receive(&emulator, 1U, &rx, NULL));
}

void test_can_emulator_rejects_invalid_inputs(void)
{
  CanEmulator emulator;
  CanFrame invalid_frame;
  CanFrame valid_frame;
  CanFrame rx;

  can_emulator_init(&emulator);

  invalid_frame = make_frame(0x100U, NULL, 9U);
  valid_frame = make_frame(0x100U, NULL, 0U);

  TEST_ASSERT_FALSE(can_emulator_register_node(NULL, 1U));
  TEST_ASSERT_FALSE(can_emulator_submit(NULL, 1U, &invalid_frame));
  TEST_ASSERT_FALSE(can_emulator_submit(&emulator, 1U, &invalid_frame));
  TEST_ASSERT_FALSE(can_emulator_submit(&emulator, 1U, &valid_frame));
  TEST_ASSERT_FALSE(can_emulator_step(NULL));
  TEST_ASSERT_FALSE(can_emulator_step(&emulator));
  TEST_ASSERT_FALSE(can_emulator_receive(&emulator, 1U, NULL, NULL));
  TEST_ASSERT_FALSE(can_emulator_receive(NULL, 1U, &rx, NULL));
}
