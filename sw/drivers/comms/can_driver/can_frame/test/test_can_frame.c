#include "unity.h"

#include "can_frame.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_can_frame_is_valid_rejects_null_and_large_dlc(void)
{
  CanFrame frame;

  can_frame_clear(&frame);
  frame.dlc = 9U;

  TEST_ASSERT_FALSE(can_frame_is_valid(NULL));
  TEST_ASSERT_FALSE(can_frame_is_valid(&frame));
}

void test_can_frame_is_valid_accepts_max_payload(void)
{
  CanFrame frame;

  can_frame_clear(&frame);
  frame.id = 0x18FF50E5U;
  frame.dlc = CAN_FRAME_MAX_DATA_LEN;
  frame.data[0] = 0xAAU;
  frame.data[7] = 0x55U;

  TEST_ASSERT_TRUE(can_frame_is_valid(&frame));
}

void test_can_frame_clear_zeroes_frame(void)
{
  CanFrame frame;

  frame.id = 0x12345678U;
  frame.dlc = 8U;
  frame.data[0] = 0x11U;
  frame.data[7] = 0x22U;

  can_frame_clear(&frame);

  TEST_ASSERT_EQUAL_HEX32(0U, frame.id);
  TEST_ASSERT_EQUAL_UINT8(0U, frame.dlc);
  TEST_ASSERT_EQUAL_UINT8(0U, frame.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0U, frame.data[7]);
}

void test_can_frame_copy_copies_all_fields(void)
{
  CanFrame source;
  CanFrame destination;

  can_frame_clear(&source);
  source.id = 0x321U;
  source.dlc = 3U;
  source.data[0] = 0x01U;
  source.data[1] = 0x02U;
  source.data[2] = 0x03U;

  can_frame_clear(&destination);
  can_frame_copy(&destination, &source);

  TEST_ASSERT_EQUAL_HEX32(source.id, destination.id);
  TEST_ASSERT_EQUAL_UINT8(source.dlc, destination.dlc);
  TEST_ASSERT_EQUAL_MEMORY(source.data, destination.data, CAN_FRAME_MAX_DATA_LEN);
}
