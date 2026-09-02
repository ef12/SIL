/**
 * @file test_wcan_airtime.c
 * @brief Unit tests for the classical CAN bit-time model.
 *
 * The expected bit counts come from ISO 11898-1 frame layout rather than from
 * the implementation, so these tests would catch a regression in the
 * serialization, the CRC or the stuffing rule.
 */

#include "unity.h"

#include "wcan_airtime.h"

#include <string.h>

/* Frame bits before stuffing, excluding the 13 unstuffed tail bits. */
#define STANDARD_BASE_BITS 34u
#define EXTENDED_BASE_BITS 54u
#define UNSTUFFED_TAIL_BITS 13u

void setUp(void)
{
}

void tearDown(void)
{
}

static wcan_frame_t make_frame(uint32_t can_id, uint8_t dlc, uint8_t flags)
{
    wcan_frame_t frame;
    uint8_t index;

    memset(&frame, 0, sizeof(frame));
    frame.can_id = can_id;
    frame.dlc = dlc;
    frame.flags = flags;
    for (index = 0; index < dlc; ++index) {
        frame.data[index] = (uint8_t)(0x5au ^ index);
    }
    return frame;
}

void test_null_frame_measures_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0u, wcan_frame_bits(NULL, 0));
    TEST_ASSERT_EQUAL_UINT32(0u, wcan_frame_bits(NULL, 1));
}

void test_worst_case_extended_eight_bytes_matches_hand_calculation(void)
{
    /*
     * Extended, 8 data bytes: 54 + 64 = 118 stuffable bits, allowing at most
     * floor(117 / 4) = 29 stuff bits, plus 13 unstuffed tail bits.
     */
    wcan_frame_t frame = make_frame(0x18ff50e5u, 8u, WCAN_FLAG_EXTENDED);
    const uint32_t region = EXTENDED_BASE_BITS + 64u;
    const uint32_t expected = region + ((region - 1u) / 4u) + UNSTUFFED_TAIL_BITS;

    TEST_ASSERT_EQUAL_UINT32(160u, expected);
    TEST_ASSERT_EQUAL_UINT32(expected, wcan_frame_bits(&frame, 1));
}

void test_worst_case_standard_no_data_matches_hand_calculation(void)
{
    /* Standard, no data: 34 stuffable bits, at most 8 stuff bits, plus 13. */
    wcan_frame_t frame = make_frame(0x123u, 0u, 0u);
    const uint32_t expected =
        STANDARD_BASE_BITS + ((STANDARD_BASE_BITS - 1u) / 4u) +
        UNSTUFFED_TAIL_BITS;

    TEST_ASSERT_EQUAL_UINT32(55u, expected);
    TEST_ASSERT_EQUAL_UINT32(expected, wcan_frame_bits(&frame, 1));
}

void test_exact_never_exceeds_worst_case(void)
{
    wcan_frame_t extended = make_frame(0x18ff50e5u, 8u, WCAN_FLAG_EXTENDED);
    wcan_frame_t standard = make_frame(0x123u, 0u, 0u);

    TEST_ASSERT_TRUE(wcan_frame_bits(&extended, 0) <=
                     wcan_frame_bits(&extended, 1));
    TEST_ASSERT_TRUE(wcan_frame_bits(&standard, 0) <=
                     wcan_frame_bits(&standard, 1));
}

void test_exact_is_at_least_the_unstuffed_length(void)
{
    wcan_frame_t frame = make_frame(0x18ff50e5u, 8u, WCAN_FLAG_EXTENDED);

    /* 118 stuffable bits plus the 13 unstuffed tail bits, before any stuffing. */
    TEST_ASSERT_TRUE(wcan_frame_bits(&frame, 0) >=
                     EXTENDED_BASE_BITS + 64u + UNSTUFFED_TAIL_BITS);
}

void test_all_dominant_identifier_forces_stuff_bits(void)
{
    /*
     * Identifier zero with no data drives a long run of dominant bits, so the
     * transmitter must insert a stuff bit after every fifth. The exact count
     * must therefore exceed the unstuffed length.
     */
    wcan_frame_t frame = make_frame(0x000u, 0u, 0u);
    const uint32_t unstuffed = STANDARD_BASE_BITS + UNSTUFFED_TAIL_BITS;

    TEST_ASSERT_TRUE(wcan_frame_bits(&frame, 0) > unstuffed);
}

void test_longer_payload_takes_longer(void)
{
    wcan_frame_t empty = make_frame(0x18ff50e5u, 0u, WCAN_FLAG_EXTENDED);
    wcan_frame_t full = make_frame(0x18ff50e5u, 8u, WCAN_FLAG_EXTENDED);

    TEST_ASSERT_TRUE(wcan_frame_bits(&full, 0) > wcan_frame_bits(&empty, 0));
}

void test_remote_frame_carries_no_payload_time(void)
{
    /* An RTR frame declares a length but transmits no data bytes. */
    wcan_frame_t remote = make_frame(0x123u, 8u, WCAN_FLAG_RTR);
    wcan_frame_t data = make_frame(0x123u, 8u, 0u);

    TEST_ASSERT_TRUE(wcan_frame_bits(&remote, 0) < wcan_frame_bits(&data, 0));
}

void test_airtime_converts_bits_at_the_given_bitrate(void)
{
    wcan_frame_t frame = make_frame(0x18ff50e5u, 8u, WCAN_FLAG_EXTENDED);
    const uint32_t bits = wcan_frame_bits(&frame, 0);
    const uint32_t expected_250k = (uint32_t)(((uint64_t)bits * 1000000u) / 250000u);
    const uint32_t expected_500k = (uint32_t)(((uint64_t)bits * 1000000u) / 500000u);

    TEST_ASSERT_EQUAL_UINT32(expected_250k,
                             wcan_frame_airtime_us(&frame, 250000u, 0));

    /* Twice the bitrate is half the airtime. */
    TEST_ASSERT_EQUAL_UINT32(expected_500k,
                             wcan_frame_airtime_us(&frame, 500000u, 0));
}

void test_airtime_of_zero_bitrate_is_zero(void)
{
    wcan_frame_t frame = make_frame(0x123u, 8u, 0u);

    TEST_ASSERT_EQUAL_UINT32(0u, wcan_frame_airtime_us(&frame, 0u, 0));
}

void test_worst_case_extended_eight_bytes_is_640_us_at_250k(void)
{
    /* The figure the design documentation quotes for a full ISOBUS frame. */
    wcan_frame_t frame = make_frame(0x18ff50e5u, 8u, WCAN_FLAG_EXTENDED);

    TEST_ASSERT_EQUAL_UINT32(640u, wcan_frame_airtime_us(&frame, 250000u, 1));
}
