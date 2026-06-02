/**
 * @file test_io_driver.c
 * @brief Unit tests for io_driver dispatch layer.
 */

#include "unity.h"

#include "io_driver.h"

#include <string.h>

/* ----------------------------------------------------------------
 *  Stub function-pointer implementations
 * ---------------------------------------------------------------- */

static bool g_digital_read_called;
static bool g_digital_write_called;
static bool g_analog_read_called;
static bool g_analog_write_called;
static bool g_close_called;

static bool stub_digital_read(const IoDriver *self, uint16_t pin, bool *value)
{
  (void)self;
  (void)pin;
  *value = true;
  g_digital_read_called = true;
  return true;
}

static bool stub_digital_write(IoDriver *self, uint16_t pin, bool value)
{
  (void)self;
  (void)pin;
  (void)value;
  g_digital_write_called = true;
  return true;
}

static bool stub_analog_read(const IoDriver *self, uint16_t pin, uint16_t *value)
{
  (void)self;
  (void)pin;
  *value = 1023U;
  g_analog_read_called = true;
  return true;
}

static bool stub_analog_write(IoDriver *self, uint16_t pin, uint16_t value)
{
  (void)self;
  (void)pin;
  (void)value;
  g_analog_write_called = true;
  return true;
}

static void stub_close(IoDriver *self)
{
  (void)self;
  g_close_called = true;
}

static IoDriver make_driver(void)
{
  IoDriver drv;

  memset(&drv, 0, sizeof(drv));
  drv.digital_read = stub_digital_read;
  drv.digital_write = stub_digital_write;
  drv.analog_read = stub_analog_read;
  drv.analog_write = stub_analog_write;
  drv.close = stub_close;
  drv.digital_pin_count = 4U;
  drv.analog_pin_count = 2U;
  drv.initialized = true;
  return drv;
}

void setUp(void)
{
  g_digital_read_called = false;
  g_digital_write_called = false;
  g_analog_read_called = false;
  g_analog_write_called = false;
  g_close_called = false;
}

void tearDown(void)
{
}

/* ----------------------------------------------------------------
 *  io_driver_digital_read
 * ---------------------------------------------------------------- */

void test_digital_read_dispatches_to_function_pointer(void)
{
  IoDriver drv = make_driver();
  bool value = false;

  TEST_ASSERT_TRUE(io_driver_digital_read(&drv, 0U, &value));
  TEST_ASSERT_TRUE(g_digital_read_called);
  TEST_ASSERT_TRUE(value);
}

void test_digital_read_returns_false_when_driver_is_null(void)
{
  bool value;

  TEST_ASSERT_FALSE(io_driver_digital_read(NULL, 0U, &value));
}

void test_digital_read_returns_false_when_not_initialized(void)
{
  IoDriver drv = make_driver();
  bool value;

  drv.initialized = false;

  TEST_ASSERT_FALSE(io_driver_digital_read(&drv, 0U, &value));
  TEST_ASSERT_FALSE(g_digital_read_called);
}

void test_digital_read_returns_false_when_pointer_is_null(void)
{
  IoDriver drv = make_driver();
  bool value;

  drv.digital_read = NULL;

  TEST_ASSERT_FALSE(io_driver_digital_read(&drv, 0U, &value));
}

/* ----------------------------------------------------------------
 *  io_driver_digital_write
 * ---------------------------------------------------------------- */

void test_digital_write_dispatches_to_function_pointer(void)
{
  IoDriver drv = make_driver();

  TEST_ASSERT_TRUE(io_driver_digital_write(&drv, 1U, true));
  TEST_ASSERT_TRUE(g_digital_write_called);
}

void test_digital_write_returns_false_when_driver_is_null(void)
{
  TEST_ASSERT_FALSE(io_driver_digital_write(NULL, 0U, false));
}

void test_digital_write_returns_false_when_not_initialized(void)
{
  IoDriver drv = make_driver();

  drv.initialized = false;

  TEST_ASSERT_FALSE(io_driver_digital_write(&drv, 0U, false));
  TEST_ASSERT_FALSE(g_digital_write_called);
}

void test_digital_write_returns_false_when_pointer_is_null(void)
{
  IoDriver drv = make_driver();

  drv.digital_write = NULL;

  TEST_ASSERT_FALSE(io_driver_digital_write(&drv, 0U, false));
}

/* ----------------------------------------------------------------
 *  io_driver_analog_read
 * ---------------------------------------------------------------- */

void test_analog_read_dispatches_to_function_pointer(void)
{
  IoDriver drv = make_driver();
  uint16_t value = 0U;

  TEST_ASSERT_TRUE(io_driver_analog_read(&drv, 0U, &value));
  TEST_ASSERT_TRUE(g_analog_read_called);
  TEST_ASSERT_EQUAL_UINT16(1023U, value);
}

void test_analog_read_returns_false_when_driver_is_null(void)
{
  uint16_t value;

  TEST_ASSERT_FALSE(io_driver_analog_read(NULL, 0U, &value));
}

void test_analog_read_returns_false_when_not_initialized(void)
{
  IoDriver drv = make_driver();
  uint16_t value;

  drv.initialized = false;

  TEST_ASSERT_FALSE(io_driver_analog_read(&drv, 0U, &value));
  TEST_ASSERT_FALSE(g_analog_read_called);
}

void test_analog_read_returns_false_when_pointer_is_null(void)
{
  IoDriver drv = make_driver();
  uint16_t value;

  drv.analog_read = NULL;

  TEST_ASSERT_FALSE(io_driver_analog_read(&drv, 0U, &value));
}

/* ----------------------------------------------------------------
 *  io_driver_analog_write
 * ---------------------------------------------------------------- */

void test_analog_write_dispatches_to_function_pointer(void)
{
  IoDriver drv = make_driver();

  TEST_ASSERT_TRUE(io_driver_analog_write(&drv, 0U, 512U));
  TEST_ASSERT_TRUE(g_analog_write_called);
}

void test_analog_write_returns_false_when_driver_is_null(void)
{
  TEST_ASSERT_FALSE(io_driver_analog_write(NULL, 0U, 0U));
}

void test_analog_write_returns_false_when_not_initialized(void)
{
  IoDriver drv = make_driver();

  drv.initialized = false;

  TEST_ASSERT_FALSE(io_driver_analog_write(&drv, 0U, 0U));
  TEST_ASSERT_FALSE(g_analog_write_called);
}

void test_analog_write_returns_false_when_pointer_is_null(void)
{
  IoDriver drv = make_driver();

  drv.analog_write = NULL;

  TEST_ASSERT_FALSE(io_driver_analog_write(&drv, 0U, 0U));
}

/* ----------------------------------------------------------------
 *  io_driver_close
 * ---------------------------------------------------------------- */

void test_close_dispatches_to_function_pointer(void)
{
  IoDriver drv = make_driver();

  io_driver_close(&drv);

  TEST_ASSERT_TRUE(g_close_called);
  TEST_ASSERT_FALSE(drv.initialized);
}

void test_close_is_safe_when_driver_is_null(void)
{
  io_driver_close(NULL); /* must not crash */
}

void test_close_is_safe_when_not_initialized(void)
{
  IoDriver drv = make_driver();

  drv.initialized = false;

  io_driver_close(&drv);
  TEST_ASSERT_FALSE(g_close_called);
}

void test_close_handles_null_close_pointer(void)
{
  IoDriver drv = make_driver();

  drv.close = NULL;

  io_driver_close(&drv);
  TEST_ASSERT_FALSE(g_close_called);
  TEST_ASSERT_FALSE(drv.initialized);
}
