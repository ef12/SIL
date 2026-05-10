/**
 * @file can_frame.h
 * @brief Shared CAN frame type and helpers.
 */

#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum payload bytes for a classical CAN frame. */
#define CAN_FRAME_MAX_DATA_LEN 8U

/**
 * @brief Shared CAN frame type.
 */
typedef struct
{
  /** Arbitration ID. */
  uint32_t id;
  /** Payload length in bytes (0..8). */
  uint8_t dlc;
  /** Payload bytes. */
  uint8_t data[CAN_FRAME_MAX_DATA_LEN];
} CanFrame;

/**
 * @brief Validates a CAN frame.
 *
 * @param frame Frame to validate.
 * @return true when frame pointer is valid and DLC is within range.
 */
bool can_frame_is_valid(const CanFrame *frame);

/**
 * @brief Clears a frame to zero.
 *
 * @param frame Frame to clear.
 */
void can_frame_clear(CanFrame *frame);

/**
 * @brief Copies frame content from source to destination.
 *
 * @param destination Destination frame.
 * @param source Source frame.
 */
void can_frame_copy(CanFrame *destination, const CanFrame *source);

#ifdef __cplusplus
}
#endif

#endif /* CAN_FRAME_H */
