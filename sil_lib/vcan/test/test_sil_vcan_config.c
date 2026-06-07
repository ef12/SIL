/**
 * @file test_sil_vcan_config.c
 * @brief Unit tests for sil_vcan_config — mocks udp_socket, can_transport_udp,
 *        can_emulator, and sil_config_udp_socket via CMock.
 */

#include "unity.h"

#include "sil_vcan_config.h"

#include "Mockcan_emulator.h"
#include "Mockcan_transport_udp.h"
#include "Mocksil_config_udp_socket.h"
#include "Mockudp_socket.h"

/* can_frame helpers are real (tiny, no side effects). */
#include "can_frame.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* ----------------------------------------------------------------
 *  Helpers
 * ---------------------------------------------------------------- */

static const SilVcanConfigParams DEFAULT_PARAMS = {
    .local_port = 6000U,
    .remote_ip = "127.0.0.1",
    .remote_port = 6001U,
    .timeout_ms = 100U,
    .max_pending_tx = 16U,
    .max_rx_queue = 16U,
};

/**
 * @brief Stubs all subsystem inits to succeed — use IgnoreArg variants
 *        because internal pointers are allocated by the function under test.
 */
static void stub_all_init_success(void)
{
  sil_config_udp_socket_init_IgnoreAndReturn(true);
  can_transport_udp_init_IgnoreAndReturn(true);
  can_emulator_init_IgnoreAndReturn(true);
  can_emulator_register_node_IgnoreAndReturn(true);
}

/**
 * @brief Stubs cleanup calls that may occur on failure or deinit.
 */
static void stub_cleanup(void)
{
  can_emulator_deinit_Ignore();
  udp_socket_close_Ignore();
}

/* ----------------------------------------------------------------
 *  NULL / invalid parameter guards
 * ---------------------------------------------------------------- */

void test_init_returns_false_when_sil_is_null(void)
{
  TEST_ASSERT_FALSE(sil_vcan_config_init(NULL, &DEFAULT_PARAMS));
}

void test_init_returns_false_when_params_is_null(void)
{
  SilVcanConfig sil = {0};

  TEST_ASSERT_FALSE(sil_vcan_config_init(&sil, NULL));
}

void test_init_returns_false_when_remote_ip_is_null(void)
{
  SilVcanConfig sil = {0};
  SilVcanConfigParams bad_params = DEFAULT_PARAMS;
  bad_params.remote_ip = NULL;

  TEST_ASSERT_FALSE(sil_vcan_config_init(&sil, &bad_params));
}

/* ----------------------------------------------------------------
 *  Happy path
 * ---------------------------------------------------------------- */

void test_init_and_get_driver_succeeds(void)
{
  SilVcanConfig sil = {0};
  CanDriver *driver;

  stub_all_init_success();

  TEST_ASSERT_TRUE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  TEST_ASSERT_TRUE(sil.initialized);

  driver = sil_vcan_config_get_driver(&sil);
  TEST_ASSERT_NOT_NULL(driver);
  TEST_ASSERT_TRUE(driver->initialized);
  TEST_ASSERT_NOT_NULL(driver->send);
  TEST_ASSERT_NOT_NULL(driver->receive);

  stub_cleanup();
  sil_vcan_config_deinit(&sil);
  TEST_ASSERT_FALSE(sil.initialized);
  TEST_ASSERT_NULL(sil.internal);
}

/* ----------------------------------------------------------------
 *  get_driver guards
 * ---------------------------------------------------------------- */

void test_get_driver_returns_null_when_sil_is_null(void)
{
  TEST_ASSERT_NULL(sil_vcan_config_get_driver(NULL));
}

void test_get_driver_returns_null_when_not_initialized(void)
{
  SilVcanConfig sil = {0};

  TEST_ASSERT_NULL(sil_vcan_config_get_driver(&sil));
}

/* ----------------------------------------------------------------
 *  deinit guards
 * ---------------------------------------------------------------- */

void test_deinit_is_safe_when_sil_is_null(void)
{
  sil_vcan_config_deinit(NULL); /* must not crash */
}

void test_deinit_is_safe_when_not_initialized(void)
{
  SilVcanConfig sil = {0};

  sil_vcan_config_deinit(&sil); /* must not crash */
}

/* ----------------------------------------------------------------
 *  Socket init failure
 * ---------------------------------------------------------------- */

void test_init_fails_when_socket_init_fails(void)
{
  SilVcanConfig sil = {0};

  sil_config_udp_socket_init_IgnoreAndReturn(false);

  TEST_ASSERT_FALSE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  TEST_ASSERT_FALSE(sil.initialized);
}

/* ----------------------------------------------------------------
 *  Transport init failure — socket must be cleaned up
 * ---------------------------------------------------------------- */

void test_init_fails_when_transport_init_fails(void)
{
  SilVcanConfig sil = {0};

  sil_config_udp_socket_init_IgnoreAndReturn(true);
  can_transport_udp_init_IgnoreAndReturn(false);
  stub_cleanup();

  TEST_ASSERT_FALSE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  TEST_ASSERT_FALSE(sil.initialized);
}

/* ----------------------------------------------------------------
 *  Emulator init failure
 * ---------------------------------------------------------------- */

void test_init_fails_when_emulator_init_fails(void)
{
  SilVcanConfig sil = {0};

  sil_config_udp_socket_init_IgnoreAndReturn(true);
  can_transport_udp_init_IgnoreAndReturn(true);
  can_emulator_init_IgnoreAndReturn(false);
  stub_cleanup();

  TEST_ASSERT_FALSE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  TEST_ASSERT_FALSE(sil.initialized);
}

/* ----------------------------------------------------------------
 *  First register_node failure
 * ---------------------------------------------------------------- */

void test_init_fails_when_first_register_node_fails(void)
{
  SilVcanConfig sil = {0};

  sil_config_udp_socket_init_IgnoreAndReturn(true);
  can_transport_udp_init_IgnoreAndReturn(true);
  can_emulator_init_IgnoreAndReturn(true);
  can_emulator_register_node_IgnoreAndReturn(false);
  stub_cleanup();

  TEST_ASSERT_FALSE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  TEST_ASSERT_FALSE(sil.initialized);
}

/* ----------------------------------------------------------------
 *  Second register_node failure
 * ---------------------------------------------------------------- */

void test_init_fails_when_second_register_node_fails(void)
{
  SilVcanConfig sil = {0};

  sil_config_udp_socket_init_IgnoreAndReturn(true);
  can_transport_udp_init_IgnoreAndReturn(true);
  can_emulator_init_IgnoreAndReturn(true);

  /* First call succeeds, second fails. */
  can_emulator_register_node_ExpectAnyArgsAndReturn(true);
  can_emulator_register_node_ExpectAnyArgsAndReturn(false);

  stub_cleanup();

  TEST_ASSERT_FALSE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  TEST_ASSERT_FALSE(sil.initialized);
}

/* ----------------------------------------------------------------
 *  Driver send — exercises sil_can_send via function pointer
 * ---------------------------------------------------------------- */

void test_driver_send_routes_through_emulator_and_transport(void)
{
  SilVcanConfig sil = {0};
  CanDriver *driver;
  CanFrame frame;

  stub_all_init_success();
  TEST_ASSERT_TRUE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  driver = sil_vcan_config_get_driver(&sil);
  TEST_ASSERT_NOT_NULL(driver);

  can_frame_clear(&frame);
  frame.id = 0x100U;
  frame.dlc = 2U;
  frame.data[0] = 0xAAU;
  frame.data[1] = 0xBBU;

  /* submit into emulator succeeds */
  can_emulator_submit_ExpectAnyArgsAndReturn(true);

  /* step: first call routes one frame, second call returns false (queue empty) */
  can_emulator_step_ExpectAnyArgsAndReturn(true);
  can_emulator_step_ExpectAnyArgsAndReturn(false);

  /* drain remote RX: first call returns a frame, second returns false (empty) */
  can_emulator_receive_ExpectAnyArgsAndReturn(true);
  can_emulator_receive_ExpectAnyArgsAndReturn(false);

  /* buffered frames sent over UDP outside the lock */
  can_transport_udp_send_frame_ExpectAnyArgsAndReturn(true);

  TEST_ASSERT_TRUE(driver->send(driver, &frame));

  stub_cleanup();
  sil_vcan_config_deinit(&sil);
}

/* ----------------------------------------------------------------
 *  Driver send — submit fails
 * ---------------------------------------------------------------- */

void test_driver_send_returns_false_when_submit_fails(void)
{
  SilVcanConfig sil = {0};
  CanDriver *driver;
  CanFrame frame;

  stub_all_init_success();
  TEST_ASSERT_TRUE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  driver = sil_vcan_config_get_driver(&sil);

  can_frame_clear(&frame);
  frame.id = 0x200U;
  frame.dlc = 0U;

  can_emulator_submit_ExpectAnyArgsAndReturn(false);

  TEST_ASSERT_FALSE(driver->send(driver, &frame));

  stub_cleanup();
  sil_vcan_config_deinit(&sil);
}

/* ----------------------------------------------------------------
 *  Driver send — transport send fails mid-drain
 * ---------------------------------------------------------------- */

void test_driver_send_returns_false_when_transport_send_fails(void)
{
  SilVcanConfig sil = {0};
  CanDriver *driver;
  CanFrame frame;

  stub_all_init_success();
  TEST_ASSERT_TRUE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  driver = sil_vcan_config_get_driver(&sil);

  can_frame_clear(&frame);
  frame.id = 0x300U;
  frame.dlc = 1U;

  can_emulator_submit_ExpectAnyArgsAndReturn(true);
  can_emulator_step_ExpectAnyArgsAndReturn(false);
  can_emulator_receive_ExpectAnyArgsAndReturn(true);
  can_emulator_receive_ExpectAnyArgsAndReturn(false);

  /* buffered frame sent over UDP — transport fails */
  can_transport_udp_send_frame_ExpectAnyArgsAndReturn(false);

  TEST_ASSERT_FALSE(driver->send(driver, &frame));

  stub_cleanup();
  sil_vcan_config_deinit(&sil);
}

/* ----------------------------------------------------------------
 *  Driver receive — happy path
 * ---------------------------------------------------------------- */

void test_driver_receive_pulls_from_transport_and_emulator(void)
{
  SilVcanConfig sil = {0};
  CanDriver *driver;
  CanFrame out;

  stub_all_init_success();
  TEST_ASSERT_TRUE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  driver = sil_vcan_config_get_driver(&sil);

  /* No incoming UDP frames. */
  can_transport_udp_receive_frame_ExpectAnyArgsAndReturn(false);

  /* Step: nothing pending. */
  can_emulator_step_ExpectAnyArgsAndReturn(false);

  /* Dequeue one frame for local node succeeds. */
  can_emulator_receive_ExpectAnyArgsAndReturn(true);

  TEST_ASSERT_TRUE(driver->receive(driver, &out));

  stub_cleanup();
  sil_vcan_config_deinit(&sil);
}

/* ----------------------------------------------------------------
 *  Driver receive — no frames available
 * ---------------------------------------------------------------- */

void test_driver_receive_returns_false_when_no_frames(void)
{
  SilVcanConfig sil = {0};
  CanDriver *driver;
  CanFrame out;

  stub_all_init_success();
  TEST_ASSERT_TRUE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  driver = sil_vcan_config_get_driver(&sil);

  can_transport_udp_receive_frame_ExpectAnyArgsAndReturn(false);
  can_emulator_step_ExpectAnyArgsAndReturn(false);
  can_emulator_receive_ExpectAnyArgsAndReturn(false);

  TEST_ASSERT_FALSE(driver->receive(driver, &out));

  stub_cleanup();
  sil_vcan_config_deinit(&sil);
}

/* ----------------------------------------------------------------
 *  Driver receive — UDP submit break path
 * ---------------------------------------------------------------- */

void test_driver_receive_stops_injecting_when_submit_fails(void)
{
  SilVcanConfig sil = {0};
  CanDriver *driver;
  CanFrame out;

  stub_all_init_success();
  TEST_ASSERT_TRUE(sil_vcan_config_init(&sil, &DEFAULT_PARAMS));
  driver = sil_vcan_config_get_driver(&sil);

  /* Both UDP frames pulled into buffer first, then submitted. */
  can_transport_udp_receive_frame_ExpectAnyArgsAndReturn(true);
  can_transport_udp_receive_frame_ExpectAnyArgsAndReturn(true);
  can_transport_udp_receive_frame_ExpectAnyArgsAndReturn(false);

  can_emulator_submit_ExpectAnyArgsAndReturn(true);
  can_emulator_submit_ExpectAnyArgsAndReturn(false);

  can_emulator_step_ExpectAnyArgsAndReturn(false);
  can_emulator_receive_ExpectAnyArgsAndReturn(false);

  TEST_ASSERT_FALSE(driver->receive(driver, &out));

  stub_cleanup();
  sil_vcan_config_deinit(&sil);
}
