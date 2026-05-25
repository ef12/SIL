/**
 * @file can_frame.c
 * @brief CAN frame validation, clearing, and copying helpers.
 */

#include "can_frame.h"

#include <string.h>

bool can_frame_is_valid(const CanFrame *frame)
{
  if (frame == NULL)
  {
    return false;
  }

  return frame->dlc <= CAN_FRAME_MAX_DATA_LEN;
}

void can_frame_clear(CanFrame *frame)
{
  if (frame == NULL)
  {
    return;
  }

  (void)memset(frame, 0, sizeof(*frame));
}

void can_frame_copy(CanFrame *destination, const CanFrame *source)
{
  if (destination == NULL || source == NULL)
  {
    return;
  }

  *destination = *source;
}
