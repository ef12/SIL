/**
 * @file test_sil_io_config.c
 * @brief Unit tests for sil_io_config — mocks io_transport_udp,
 *        sil_config_udp_socket, and udp_socket via CMock.
 *
 * The full init path spawns a real OS thread, so we test:
 *   - all NULL/invalid parameter guards (no allocation, no threading)
 *   - get_driver / deinit safety on uninitialised instances
 *   - socket-init failure (returns before thread creation)
 *   - transport-init failure (returns before thread creation)
 */

#include "unity.h"

#include "sil_io_config.h"

#include "Mockio_transport_udp.h"
#include "Mocksil_config_udp_socket.h"
#include "Mockudp_socket.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* ----------------------------------------------------------------
 *  Helpers
 * ---------------------------------------------------------------- */

static const SilIoConfigParams DEFAULT_PARAMS = {
    .local_port = 8000U,
    .remote_ip = "127.0.0.1",
    .remote_port = 8001U,
    .sync_interval_ms = 10U,
    .digital_pin_count = 4U,
    .analog_pin_count = 2U,
};

/* ----------------------------------------------------------------
 *  NULL / invalid parameter guards
 * ---------------------------------------------------------------- */

void test_init_returns_false_when_sil_is_null(void)
{
  TEST_ASSERT_FALSE(sil_io_config_init(NULL, &DEFAULT_PARAMS));
}

void test_init_returns_false_when_params_is_null(void)
{
  SilIoConfig sil = {0};

  TEST_ASSERT_FALSE(sil_io_config_init(&sil, NULL));
}

void test_init_returns_false_when_remote_ip_is_null(void)
{
  SilIoConfig sil = {0};
  SilIoConfigParams bad = DEFAULT_PARAMS;
  bad.remote_ip = NULL;

  TEST_ASSERT_FALSE(sil_io_config_init(&sil, &bad));
}

/* ----------------------------------------------------------------
 *  get_driver guards
 * ---------------------------------------------------------------- */

void test_get_driver_returns_null_when_sil_is_null(void)
{
  TEST_ASSERT_NULL(sil_io_config_get_driver(NULL));
}

void test_get_driver_returns_null_when_not_initialized(void)
{
  SilIoConfig sil = {0};

  TEST_ASSERT_NULL(sil_io_config_get_driver(&sil));
}

/* ----------------------------------------------------------------
 *  deinit guards
 * ---------------------------------------------------------------- */

void test_deinit_is_safe_when_sil_is_null(void)
{
  sil_io_config_deinit(NULL); /* must not crash */
}

void test_deinit_is_safe_when_not_initialized(void)
{
  SilIoConfig sil = {0};

  sil_io_config_deinit(&sil); /* must not crash */
}

/* ----------------------------------------------------------------
 *  Socket init failure — returns before threading
 * ---------------------------------------------------------------- */

void test_init_fails_when_socket_init_fails(void)
{
  SilIoConfig sil = {0};

  sil_config_udp_socket_init_IgnoreAndReturn(false);

  TEST_ASSERT_FALSE(sil_io_config_init(&sil, &DEFAULT_PARAMS));
  TEST_ASSERT_FALSE(sil.initialized);
}

/* ----------------------------------------------------------------
 *  Transport init failure — cleanup runs, no thread
 * ---------------------------------------------------------------- */

void test_init_fails_when_transport_init_fails(void)
{
  SilIoConfig sil = {0};

  sil_config_udp_socket_init_IgnoreAndReturn(true);
  io_transport_udp_init_IgnoreAndReturn(false);
  udp_socket_close_Ignore();

  TEST_ASSERT_FALSE(sil_io_config_init(&sil, &DEFAULT_PARAMS));
  TEST_ASSERT_FALSE(sil.initialized);
}

/* ----------------------------------------------------------------
 *  Full init → get_driver → deinit lifecycle (with real thread)
 * ---------------------------------------------------------------- */

void test_init_succeeds_and_deinit_cleans_up(void)
{
  SilIoConfig sil = {0};
  IoDriver *driver;

  sil_config_udp_socket_init_IgnoreAndReturn(true);
  io_transport_udp_init_IgnoreAndReturn(true);

  /* The sync thread will call these in a loop — allow any number. */
  io_transport_udp_receive_IgnoreAndReturn(false);
  io_transport_udp_send_IgnoreAndReturn(false);

  TEST_ASSERT_TRUE(sil_io_config_init(&sil, &DEFAULT_PARAMS));
  TEST_ASSERT_TRUE(sil.initialized);

  driver = sil_io_config_get_driver(&sil);
  TEST_ASSERT_NOT_NULL(driver);
  TEST_ASSERT_TRUE(driver->initialized);
  TEST_ASSERT_NOT_NULL(driver->digital_read);
  TEST_ASSERT_NOT_NULL(driver->digital_write);
  TEST_ASSERT_NOT_NULL(driver->analog_read);
  TEST_ASSERT_NOT_NULL(driver->analog_write);
  TEST_ASSERT_EQUAL_UINT(4U, driver->digital_pin_count);
  TEST_ASSERT_EQUAL_UINT(2U, driver->analog_pin_count);

  udp_socket_close_Ignore();
  sil_io_config_deinit(&sil);
  TEST_ASSERT_FALSE(sil.initialized);
  TEST_ASSERT_NULL(sil.internal);
}

/* ----------------------------------------------------------------
 *  Init with digital-only params (analog_pin_count == 0)
 * ---------------------------------------------------------------- */

void test_init_succeeds_with_digital_only(void)
{
  SilIoConfig sil = {0};
  SilIoConfigParams digital_only = DEFAULT_PARAMS;

  digital_only.analog_pin_count = 0U;

  sil_config_udp_socket_init_IgnoreAndReturn(true);
  io_transport_udp_init_IgnoreAndReturn(true);
  io_transport_udp_receive_IgnoreAndReturn(false);
  io_transport_udp_send_IgnoreAndReturn(false);

  TEST_ASSERT_TRUE(sil_io_config_init(&sil, &digital_only));

  udp_socket_close_Ignore();
  sil_io_config_deinit(&sil);
}

/* ----------------------------------------------------------------
 *  Init with analog-only params (digital_pin_count == 0)
 * ---------------------------------------------------------------- */

void test_init_succeeds_with_analog_only(void)
{
  SilIoConfig sil = {0};
  SilIoConfigParams analog_only = DEFAULT_PARAMS;

  analog_only.digital_pin_count = 0U;

  sil_config_udp_socket_init_IgnoreAndReturn(true);
  io_transport_udp_init_IgnoreAndReturn(true);
  io_transport_udp_receive_IgnoreAndReturn(false);
  io_transport_udp_send_IgnoreAndReturn(false);

  TEST_ASSERT_TRUE(sil_io_config_init(&sil, &analog_only));

  udp_socket_close_Ignore();
  sil_io_config_deinit(&sil);
}

/* ----------------------------------------------------------------
 *  Init with zero pins (no buffers allocated)
 * ---------------------------------------------------------------- */

void test_init_succeeds_with_zero_pin_counts(void)
{
  SilIoConfig sil = {0};
  SilIoConfigParams zero_pins = DEFAULT_PARAMS;

  zero_pins.digital_pin_count = 0U;
  zero_pins.analog_pin_count = 0U;

  sil_config_udp_socket_init_IgnoreAndReturn(true);
  io_transport_udp_init_IgnoreAndReturn(true);
  io_transport_udp_receive_IgnoreAndReturn(false);
  io_transport_udp_send_IgnoreAndReturn(false);

  TEST_ASSERT_TRUE(sil_io_config_init(&sil, &zero_pins));

  udp_socket_close_Ignore();
  sil_io_config_deinit(&sil);
}

/* ----------------------------------------------------------------
 *  Socket init failure with only digital pins (covers free path)
 * ---------------------------------------------------------------- */

void test_init_frees_digital_buf_when_socket_init_fails(void)
{
  SilIoConfig sil = {0};
  SilIoConfigParams digital_only = DEFAULT_PARAMS;

  digital_only.analog_pin_count = 0U;

  sil_config_udp_socket_init_IgnoreAndReturn(false);

  TEST_ASSERT_FALSE(sil_io_config_init(&sil, &digital_only));
  TEST_ASSERT_FALSE(sil.initialized);
}
