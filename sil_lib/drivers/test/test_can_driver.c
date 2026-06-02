/**
 * @file test_can_driver.c
 * @brief Unit tests for can_driver dispatch layer.
 */

#include "unity.h"

#include "can_driver.h"
#include "can_frame.h"

#include <string.h>

/* ----------------------------------------------------------------
 *  Stub function-pointer implementations
 * ---------------------------------------------------------------- */

static bool g_send_called;
static bool g_receive_called;
static bool g_close_called;

static bool stub_send(const CanDriver *self, const CanFrame *frame)
{
  (void)self;
  (void)frame;
  g_send_called = true;
  return true;
}

static bool stub_receive(CanDriver *self, CanFrame *out_frame)
{
  (void)self;
  can_frame_clear(out_frame);
  out_frame->id = 0x42U;
  out_frame->dlc = 1U;
  g_receive_called = true;
  return true;
}

static void stub_close(CanDriver *self)
{
  (void)self;
  g_close_called = true;
}

static CanDriver make_driver(void)
{
  CanDriver drv;

  memset(&drv, 0, sizeof(drv));
  drv.send = stub_send;
  drv.receive = stub_receive;
  drv.close = stub_close;
  drv.initialized = true;
  return drv;
}

void setUp(void)
{
  g_send_called = false;
  g_receive_called = false;
  g_close_called = false;
}

void tearDown(void)
{
}

/* ----------------------------------------------------------------
 *  can_driver_send
 * ---------------------------------------------------------------- */

void test_send_dispatches_to_function_pointer(void)
{
  CanDriver drv = make_driver();
  CanFrame frame;

  can_frame_clear(&frame);
  frame.id = 0x100U;
  frame.dlc = 0U;

  TEST_ASSERT_TRUE(can_driver_send(&drv, &frame));
  TEST_ASSERT_TRUE(g_send_called);
}

void test_send_returns_false_when_driver_is_null(void)
{
  CanFrame frame;

  can_frame_clear(&frame);
  TEST_ASSERT_FALSE(can_driver_send(NULL, &frame));
}

void test_send_returns_false_when_not_initialized(void)
{
  CanDriver drv = make_driver();
  CanFrame frame;

  drv.initialized = false;
  can_frame_clear(&frame);

  TEST_ASSERT_FALSE(can_driver_send(&drv, &frame));
  TEST_ASSERT_FALSE(g_send_called);
}

void test_send_returns_false_when_send_pointer_is_null(void)
{
  CanDriver drv = make_driver();
  CanFrame frame;

  drv.send = NULL;
  can_frame_clear(&frame);

  TEST_ASSERT_FALSE(can_driver_send(&drv, &frame));
}

void test_send_returns_false_when_frame_is_null(void)
{
  CanDriver drv = make_driver();

  TEST_ASSERT_FALSE(can_driver_send(&drv, NULL));
}

/* ----------------------------------------------------------------
 *  can_driver_receive
 * ---------------------------------------------------------------- */

void test_receive_dispatches_to_function_pointer(void)
{
  CanDriver drv = make_driver();
  CanFrame out;

  TEST_ASSERT_TRUE(can_driver_receive(&drv, &out));
  TEST_ASSERT_TRUE(g_receive_called);
  TEST_ASSERT_EQUAL_HEX32(0x42U, out.id);
}

void test_receive_returns_false_when_driver_is_null(void)
{
  CanFrame out;

  TEST_ASSERT_FALSE(can_driver_receive(NULL, &out));
}

void test_receive_returns_false_when_not_initialized(void)
{
  CanDriver drv = make_driver();
  CanFrame out;

  drv.initialized = false;

  TEST_ASSERT_FALSE(can_driver_receive(&drv, &out));
  TEST_ASSERT_FALSE(g_receive_called);
}

void test_receive_returns_false_when_receive_pointer_is_null(void)
{
  CanDriver drv = make_driver();
  CanFrame out;

  drv.receive = NULL;

  TEST_ASSERT_FALSE(can_driver_receive(&drv, &out));
}

void test_receive_returns_false_when_out_frame_is_null(void)
{
  CanDriver drv = make_driver();

  TEST_ASSERT_FALSE(can_driver_receive(&drv, NULL));
}

/* ----------------------------------------------------------------
 *  can_driver_close
 * ---------------------------------------------------------------- */

void test_close_dispatches_to_function_pointer(void)
{
  CanDriver drv = make_driver();

  can_driver_close(&drv);

  TEST_ASSERT_TRUE(g_close_called);
  TEST_ASSERT_FALSE(drv.initialized);
}

void test_close_is_safe_when_driver_is_null(void)
{
  can_driver_close(NULL); /* must not crash */
}

void test_close_is_safe_when_not_initialized(void)
{
  CanDriver drv = make_driver();

  drv.initialized = false;

  can_driver_close(&drv);
  TEST_ASSERT_FALSE(g_close_called);
}

void test_close_handles_null_close_pointer(void)
{
  CanDriver drv = make_driver();

  drv.close = NULL;

  can_driver_close(&drv);
  TEST_ASSERT_FALSE(g_close_called);
  TEST_ASSERT_FALSE(drv.initialized);
}
