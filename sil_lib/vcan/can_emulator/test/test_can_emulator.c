#include "unity.h"

#include "can_emulator.h"
#include "can_frame.h"

#include <string.h>

static CanEmulator emulator;

static const CanEmulatorConfig default_config = {
    .max_nodes = 16U,
    .max_pending_tx = 64U,
    .max_rx_queue = 64U,
};

void setUp(void)
{
  TEST_ASSERT_TRUE(can_emulator_init(&emulator, &default_config));
}

void tearDown(void)
{
  can_emulator_deinit(&emulator);
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
  CanFrame rx;
  CanNodeId sender;
  CanFrame frame_a;
  CanFrame frame_b;
  const uint8_t data_a[] = {0xAA};
  const uint8_t data_b[] = {0xBB};

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
  CanFrame rx;
  CanNodeId sender;
  CanFrame frame_first;
  CanFrame frame_second;

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
  CanFrame rx;
  CanFrame frame;

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
  CanFrame invalid_frame;
  CanFrame valid_frame;
  CanFrame rx;

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

/* ------------------------------------------------------------------ */
/*  init / deinit edge cases                                          */
/* ------------------------------------------------------------------ */

void test_init_rejects_null_emulator(void)
{
  TEST_ASSERT_FALSE(can_emulator_init(NULL, &default_config));
}

void test_init_rejects_null_config(void)
{
  CanEmulator local;

  TEST_ASSERT_FALSE(can_emulator_init(&local, NULL));
}

void test_init_rejects_zero_max_nodes(void)
{
  CanEmulator local;
  CanEmulatorConfig cfg = {.max_nodes = 0U, .max_pending_tx = 4U, .max_rx_queue = 4U};

  TEST_ASSERT_FALSE(can_emulator_init(&local, &cfg));
}

void test_init_rejects_zero_max_pending_tx(void)
{
  CanEmulator local;
  CanEmulatorConfig cfg = {.max_nodes = 4U, .max_pending_tx = 0U, .max_rx_queue = 4U};

  TEST_ASSERT_FALSE(can_emulator_init(&local, &cfg));
}

void test_init_rejects_zero_max_rx_queue(void)
{
  CanEmulator local;
  CanEmulatorConfig cfg = {.max_nodes = 4U, .max_pending_tx = 4U, .max_rx_queue = 0U};

  TEST_ASSERT_FALSE(can_emulator_init(&local, &cfg));
}

void test_deinit_handles_null(void)
{
  can_emulator_deinit(NULL);
}

void test_deinit_handles_uninitialized(void)
{
  CanEmulator local;

  (void)memset(&local, 0, sizeof(local));
  can_emulator_deinit(&local);
}

/* ------------------------------------------------------------------ */
/*  register_node edge cases                                          */
/* ------------------------------------------------------------------ */

void test_register_node_on_uninitialized_returns_false(void)
{
  CanEmulator local;

  (void)memset(&local, 0, sizeof(local));
  TEST_ASSERT_FALSE(can_emulator_register_node(&local, 1U));
}

void test_register_node_duplicate_returns_true(void)
{
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 5U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 5U));
}

void test_register_node_all_slots_full_returns_false(void)
{
  CanEmulator local;
  CanEmulatorConfig cfg = {.max_nodes = 2U, .max_pending_tx = 4U, .max_rx_queue = 4U};

  TEST_ASSERT_TRUE(can_emulator_init(&local, &cfg));

  TEST_ASSERT_TRUE(can_emulator_register_node(&local, 1U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&local, 2U));
  TEST_ASSERT_FALSE(can_emulator_register_node(&local, 3U));

  can_emulator_deinit(&local);
}

/* ------------------------------------------------------------------ */
/*  submit edge cases                                                 */
/* ------------------------------------------------------------------ */

void test_submit_null_frame_returns_false(void)
{
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 1U));
  TEST_ASSERT_FALSE(can_emulator_submit(&emulator, 1U, NULL));
}

void test_submit_on_uninitialized_returns_false(void)
{
  CanEmulator local;
  CanFrame frame;

  (void)memset(&local, 0, sizeof(local));
  frame = make_frame(0x100U, NULL, 0U);
  TEST_ASSERT_FALSE(can_emulator_submit(&local, 1U, &frame));
}

void test_submit_when_tx_full_returns_false(void)
{
  CanEmulator local;
  CanEmulatorConfig cfg = {.max_nodes = 2U, .max_pending_tx = 1U, .max_rx_queue = 4U};
  CanFrame frame;

  TEST_ASSERT_TRUE(can_emulator_init(&local, &cfg));
  TEST_ASSERT_TRUE(can_emulator_register_node(&local, 1U));

  frame = make_frame(0x100U, NULL, 0U);
  TEST_ASSERT_TRUE(can_emulator_submit(&local, 1U, &frame));
  TEST_ASSERT_FALSE(can_emulator_submit(&local, 1U, &frame));

  can_emulator_deinit(&local);
}

/* ------------------------------------------------------------------ */
/*  step edge cases                                                   */
/* ------------------------------------------------------------------ */

void test_step_on_uninitialized_returns_false(void)
{
  CanEmulator local;

  (void)memset(&local, 0, sizeof(local));
  TEST_ASSERT_FALSE(can_emulator_step(&local));
}

void test_step_returns_false_when_rx_queue_full(void)
{
  CanEmulator local;
  CanEmulatorConfig cfg = {.max_nodes = 2U, .max_pending_tx = 4U, .max_rx_queue = 1U};
  CanFrame frame;

  TEST_ASSERT_TRUE(can_emulator_init(&local, &cfg));
  TEST_ASSERT_TRUE(can_emulator_register_node(&local, 1U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&local, 2U));

  frame = make_frame(0x100U, NULL, 0U);
  TEST_ASSERT_TRUE(can_emulator_submit(&local, 1U, &frame));
  TEST_ASSERT_TRUE(can_emulator_step(&local));

  /* Node 2 RX queue is now full (capacity 1). Next step should fail. */
  TEST_ASSERT_TRUE(can_emulator_submit(&local, 1U, &frame));
  TEST_ASSERT_FALSE(can_emulator_step(&local));

  can_emulator_deinit(&local);
}

/* ------------------------------------------------------------------ */
/*  receive edge cases                                                */
/* ------------------------------------------------------------------ */

void test_receive_on_uninitialized_returns_false(void)
{
  CanEmulator local;
  CanFrame rx;

  (void)memset(&local, 0, sizeof(local));
  TEST_ASSERT_FALSE(can_emulator_receive(&local, 1U, &rx, NULL));
}

void test_receive_unregistered_node_returns_false(void)
{
  CanFrame rx;

  TEST_ASSERT_FALSE(can_emulator_receive(&emulator, 99U, &rx, NULL));
}

/* ------------------------------------------------------------------ */
/*  pending_tx_count                                                  */
/* ------------------------------------------------------------------ */

void test_pending_tx_count_null_returns_zero(void)
{
  TEST_ASSERT_EQUAL_UINT(0U, can_emulator_pending_tx_count(NULL));
}

void test_pending_tx_count_uninitialized_returns_zero(void)
{
  CanEmulator local;

  (void)memset(&local, 0, sizeof(local));
  TEST_ASSERT_EQUAL_UINT(0U, can_emulator_pending_tx_count(&local));
}

void test_pending_tx_count_tracks_submissions(void)
{
  CanFrame frame;

  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 1U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&emulator, 2U));

  TEST_ASSERT_EQUAL_UINT(0U, can_emulator_pending_tx_count(&emulator));

  frame = make_frame(0x100U, NULL, 0U);
  TEST_ASSERT_TRUE(can_emulator_submit(&emulator, 1U, &frame));
  TEST_ASSERT_EQUAL_UINT(1U, can_emulator_pending_tx_count(&emulator));

  TEST_ASSERT_TRUE(can_emulator_submit(&emulator, 1U, &frame));
  TEST_ASSERT_EQUAL_UINT(2U, can_emulator_pending_tx_count(&emulator));

  TEST_ASSERT_TRUE(can_emulator_step(&emulator));
  TEST_ASSERT_EQUAL_UINT(1U, can_emulator_pending_tx_count(&emulator));

  TEST_ASSERT_TRUE(can_emulator_step(&emulator));
  TEST_ASSERT_EQUAL_UINT(0U, can_emulator_pending_tx_count(&emulator));
}

/* ------------------------------------------------------------------ */
/*  circular RX queue wrap-around                                     */
/* ------------------------------------------------------------------ */

void test_rx_queue_wraps_around_correctly(void)
{
  CanEmulator local;
  CanEmulatorConfig cfg = {.max_nodes = 2U, .max_pending_tx = 4U, .max_rx_queue = 2U};
  CanFrame frame;
  CanFrame rx;
  CanNodeId sender;
  const uint8_t data_a[] = {0xAA};
  const uint8_t data_b[] = {0xBB};
  const uint8_t data_c[] = {0xCC};

  TEST_ASSERT_TRUE(can_emulator_init(&local, &cfg));
  TEST_ASSERT_TRUE(can_emulator_register_node(&local, 1U));
  TEST_ASSERT_TRUE(can_emulator_register_node(&local, 2U));

  /* Fill node 2 RX queue to capacity (2 entries). */
  frame = make_frame(0x100U, data_a, 1U);
  TEST_ASSERT_TRUE(can_emulator_submit(&local, 1U, &frame));
  TEST_ASSERT_TRUE(can_emulator_step(&local));

  frame = make_frame(0x101U, data_b, 1U);
  TEST_ASSERT_TRUE(can_emulator_submit(&local, 1U, &frame));
  TEST_ASSERT_TRUE(can_emulator_step(&local));

  /* Consume one to make room and advance rx_head. */
  TEST_ASSERT_TRUE(can_emulator_receive(&local, 2U, &rx, &sender));
  TEST_ASSERT_EQUAL_HEX32(0x100U, rx.id);
  TEST_ASSERT_EQUAL_HEX8(0xAA, rx.data[0]);

  /* Submit another — this write wraps around to index 0. */
  frame = make_frame(0x102U, data_c, 1U);
  TEST_ASSERT_TRUE(can_emulator_submit(&local, 1U, &frame));
  TEST_ASSERT_TRUE(can_emulator_step(&local));

  /* Drain remaining two in order. */
  TEST_ASSERT_TRUE(can_emulator_receive(&local, 2U, &rx, &sender));
  TEST_ASSERT_EQUAL_HEX32(0x101U, rx.id);
  TEST_ASSERT_EQUAL_HEX8(0xBB, rx.data[0]);

  TEST_ASSERT_TRUE(can_emulator_receive(&local, 2U, &rx, &sender));
  TEST_ASSERT_EQUAL_HEX32(0x102U, rx.id);
  TEST_ASSERT_EQUAL_HEX8(0xCC, rx.data[0]);

  TEST_ASSERT_FALSE(can_emulator_receive(&local, 2U, &rx, NULL));

  can_emulator_deinit(&local);
}
