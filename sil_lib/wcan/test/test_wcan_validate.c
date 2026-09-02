/**
 * @file test_wcan_validate.c
 * @brief Unit tests for bus name and frame validation.
 *
 * Pure logic, so these run anywhere and need no bus, no shared memory and no
 * Windows headers.
 */

#include "unity.h"

#include "wcan_validate.h"

#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

/* ---- bus names ---------------------------------------------------------- */

void test_bus_name_accepts_the_documented_character_set(void)
{
    TEST_ASSERT_EQUAL_INT(WCAN_OK, wcan_validate_bus_name("wcan0"));
    TEST_ASSERT_EQUAL_INT(WCAN_OK, wcan_validate_bus_name("plant.bus-1"));
    TEST_ASSERT_EQUAL_INT(WCAN_OK, wcan_validate_bus_name("A_b.C-9"));
}

void test_bus_name_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_ARGUMENT,
                          wcan_validate_bus_name(NULL));
}

void test_bus_name_rejects_empty(void)
{
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_BUS, wcan_validate_bus_name(""));
}

void test_bus_name_rejects_characters_unsafe_in_object_names(void)
{
    /* A backslash would escape the Local\ namespace the segment lives in. */
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_BUS,
                          wcan_validate_bus_name("bad\\bus"));
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_BUS,
                          wcan_validate_bus_name("bad/bus"));
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_BUS,
                          wcan_validate_bus_name("bad bus"));
}

void test_bus_name_boundary_is_inclusive(void)
{
    char name[WCAN_MAX_BUS_NAME + 2u];

    memset(name, 'a', sizeof(name) - 1u);
    name[sizeof(name) - 1u] = '\0';

    /* One character too many. */
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_BUS, wcan_validate_bus_name(name));

    name[WCAN_MAX_BUS_NAME] = '\0';
    TEST_ASSERT_EQUAL_INT(WCAN_OK, wcan_validate_bus_name(name));
}

/* ---- frames ------------------------------------------------------------- */

static wcan_frame_t make_frame(uint32_t can_id, uint8_t dlc, uint8_t flags)
{
    wcan_frame_t frame;

    memset(&frame, 0, sizeof(frame));
    frame.can_id = can_id;
    frame.dlc = dlc;
    frame.flags = flags;
    return frame;
}

void test_frame_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_ARGUMENT,
                          wcan_validate_frame(NULL));
}

void test_frame_accepts_standard_and_extended_identifiers(void)
{
    wcan_frame_t standard = make_frame(0x7ffu, 8u, 0u);
    wcan_frame_t extended = make_frame(0x1fffffffu, 8u, WCAN_FLAG_EXTENDED);

    TEST_ASSERT_EQUAL_INT(WCAN_OK, wcan_validate_frame(&standard));
    TEST_ASSERT_EQUAL_INT(WCAN_OK, wcan_validate_frame(&extended));
}

void test_frame_rejects_identifier_wider_than_its_format(void)
{
    wcan_frame_t standard = make_frame(0x800u, 0u, 0u);
    wcan_frame_t extended = make_frame(0x20000000u, 0u, WCAN_FLAG_EXTENDED);

    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_FRAME,
                          wcan_validate_frame(&standard));
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_FRAME,
                          wcan_validate_frame(&extended));
}

void test_frame_rejects_classical_payload_above_eight_bytes(void)
{
    wcan_frame_t frame = make_frame(0x100u, 9u, 0u);

    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_FRAME, wcan_validate_frame(&frame));
}

void test_frame_accepts_fd_payload_up_to_sixty_four_bytes(void)
{
    wcan_frame_t frame = make_frame(0x100u, WCAN_MAX_DATA, WCAN_FLAG_FD);

    TEST_ASSERT_EQUAL_INT(WCAN_OK, wcan_validate_frame(&frame));

    frame.dlc = WCAN_MAX_DATA + 1u;
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_FRAME, wcan_validate_frame(&frame));
}

void test_frame_rejects_remote_fd_frame(void)
{
    /* CAN FD has no remote frames. */
    wcan_frame_t frame = make_frame(0x100u, 0u, WCAN_FLAG_FD | WCAN_FLAG_RTR);

    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_FRAME, wcan_validate_frame(&frame));
}

void test_frame_rejects_fd_only_flags_on_a_classical_frame(void)
{
    wcan_frame_t brs = make_frame(0x100u, 0u, WCAN_FLAG_BRS);
    wcan_frame_t esi = make_frame(0x100u, 0u, WCAN_FLAG_ESI);

    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_FRAME, wcan_validate_frame(&brs));
    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_FRAME, wcan_validate_frame(&esi));
}

void test_frame_rejects_undefined_flag_bits(void)
{
    wcan_frame_t frame = make_frame(0x100u, 0u, 0x80u);

    TEST_ASSERT_EQUAL_INT(WCAN_ERROR_INVALID_FRAME, wcan_validate_frame(&frame));
}

/* ---- status descriptions ------------------------------------------------ */

void test_strerror_describes_every_defined_status(void)
{
    int status;

    for (status = 0; status >= WCAN_ERROR_TOO_MANY_CLIENTS; --status) {
        const char *text = wcan_strerror(status);

        TEST_ASSERT_NOT_NULL(text);
        TEST_ASSERT_TRUE(strlen(text) > 0u);
        /* A defined status must not fall through to the default branch. */
        TEST_ASSERT_TRUE_MESSAGE(0 != strcmp(text, "unknown status"),
                                 "defined status described as unknown");
    }
}

void test_strerror_reports_unknown_for_out_of_range_status(void)
{
    TEST_ASSERT_EQUAL_STRING("unknown status", wcan_strerror(-99));
}
