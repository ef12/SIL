/**
 * @file wcan_airtime.c
 * @brief Bit-time model for classical CAN frames.
 *
 * Deliberately free of platform headers: the model is pure arithmetic over a
 * frame, so it can be unit tested on any host and reused by anything that
 * needs to reason about bus load.
 */

#include "wcan_airtime.h"

/**
 * Bits after the CRC that are never stuffed: CRC delimiter, ACK slot, ACK
 * delimiter, seven end-of-frame bits and three of interframe space.
 */
#define WCAN_UNSTUFFED_TAIL_BITS 13u

/** CRC-15 generator polynomial from ISO 11898-1. */
#define WCAN_CRC15_POLYNOMIAL 0x4599u

/** Longest classical frame bit stream, before stuffing. */
#define WCAN_MAX_FRAME_BITS 192u

static uint32_t append_bit(uint8_t *bits, uint32_t count, int value)
{
    bits[count] = (uint8_t)(value != 0);
    return count + 1u;
}

static uint32_t append_field(uint8_t *bits, uint32_t count, uint32_t value,
                             uint32_t width)
{
    uint32_t index;

    for (index = width; index > 0u; --index) {
        count = append_bit(bits, count, (int)((value >> (index - 1u)) & 1u));
    }
    return count;
}

/*
 * Serializes a classical CAN frame exactly as a controller drives it, so the
 * stuff bits can be counted rather than estimated. Field order per
 * ISO 11898-1:
 *
 *   standard: SOF ID(11) RTR IDE r0 DLC(4) data CRC(15)
 *   extended: SOF ID(11) SRR IDE ID(18) RTR r1 r0 DLC(4) data CRC(15)
 *
 * Stuffing covers SOF through CRC.
 */
static uint32_t build_frame_bits(const wcan_frame_t *frame, uint8_t *bits)
{
    const int extended = (frame->flags & WCAN_FLAG_EXTENDED) != 0;
    const int remote = (frame->flags & WCAN_FLAG_RTR) != 0;
    const uint32_t payload = remote ? 0u : frame->dlc;
    uint32_t count = 0;
    uint32_t crc = 0;
    uint32_t index;

    count = append_bit(bits, count, 0); /* SOF is dominant */
    if (extended) {
        count = append_field(bits, count, (frame->can_id >> 18) & 0x7ffu, 11u);
        count = append_bit(bits, count, 1); /* SRR */
        count = append_bit(bits, count, 1); /* IDE recessive */
        count = append_field(bits, count, frame->can_id & 0x3ffffu, 18u);
        count = append_bit(bits, count, remote);
        count = append_bit(bits, count, 0); /* r1 */
        count = append_bit(bits, count, 0); /* r0 */
    } else {
        count = append_field(bits, count, frame->can_id & 0x7ffu, 11u);
        count = append_bit(bits, count, remote);
        count = append_bit(bits, count, 0); /* IDE dominant */
        count = append_bit(bits, count, 0); /* r0 */
    }
    count = append_field(bits, count, frame->dlc & 0x0fu, 4u);
    for (index = 0; index < payload; ++index) {
        count = append_field(bits, count, frame->data[index], 8u);
    }

    for (index = 0; index < count; ++index) {
        const uint32_t feedback = ((crc >> 14) & 1u) ^ (uint32_t)bits[index];

        crc = (crc << 1) & 0x7fffu;
        if (feedback != 0u) {
            crc ^= WCAN_CRC15_POLYNOMIAL;
        }
    }
    return append_field(bits, count, crc, 15u);
}

static uint32_t count_stuff_bits(const uint8_t *bits, uint32_t count)
{
    uint32_t stuffed = 0;
    uint32_t run = 0;
    int last = -1;
    uint32_t index;

    for (index = 0; index < count; ++index) {
        const int value = (int)bits[index];

        if (value == last) {
            run++;
        } else {
            last = value;
            run = 1u;
        }
        if (run == 5u) {
            /* The transmitter inserts a complementary bit, which then starts a
               new run of its own. */
            stuffed++;
            last = (value == 0) ? 1 : 0;
            run = 1u;
        }
    }
    return stuffed;
}

uint32_t wcan_frame_bits(const wcan_frame_t *frame, int worst_case)
{
    uint8_t bits[WCAN_MAX_FRAME_BITS];
    uint32_t region;
    uint32_t result;

    if (frame == NULL) {
        result = 0u;
    } else if ((frame->flags & WCAN_FLAG_FD) != 0u) {
        /* Approximation. CAN FD uses different stuff rules and a wider CRC,
           and is out of scope for a 250 kbit/s ISOBUS. */
        result = 32u + (8u * (uint32_t)frame->dlc) + WCAN_UNSTUFFED_TAIL_BITS;
    } else {
        region = build_frame_bits(frame, bits);
        if (worst_case) {
            /* A stuff bit can follow at most every four identical bits. */
            result = region + ((region - 1u) / 4u) + WCAN_UNSTUFFED_TAIL_BITS;
        } else {
            result = region + count_stuff_bits(bits, region) +
                     WCAN_UNSTUFFED_TAIL_BITS;
        }
    }

    return result;
}

uint32_t wcan_frame_airtime_us(const wcan_frame_t *frame, uint32_t bitrate,
                               int worst_case)
{
    uint32_t microseconds = 0u;

    if (bitrate != 0u) {
        const uint32_t bits = wcan_frame_bits(frame, worst_case);

        microseconds = (uint32_t)(((uint64_t)bits * 1000000u) / bitrate);
    }
    return microseconds;
}
