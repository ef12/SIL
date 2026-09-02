/**
 * @file wcan_validate.c
 * @brief Bus name and frame validation, and status code descriptions.
 *
 * Kept free of platform headers so the rules can be unit tested on any host,
 * and so a consumer can validate a frame before it opens a bus.
 */

#include "wcan_validate.h"

#include <string.h>

/** Widest identifier an 11-bit standard frame can carry. */
#define WCAN_STANDARD_ID_MAX 0x7ffu

/** Widest identifier a 29-bit extended frame can carry. */
#define WCAN_EXTENDED_ID_MAX 0x1fffffffu

/** Payload limit for a classical CAN frame. */
#define WCAN_CLASSIC_DATA_MAX 8u

int wcan_validate_bus_name(const char *bus_name)
{
    size_t index;
    size_t length;

    if (bus_name == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    length = strlen(bus_name);
    if (length == 0 || length > WCAN_MAX_BUS_NAME) {
        return WCAN_ERROR_INVALID_BUS;
    }
    for (index = 0; index < length; ++index) {
        const unsigned char character = (unsigned char)bus_name[index];
        const int alphanumeric = (character >= 'a' && character <= 'z') ||
                                 (character >= 'A' && character <= 'Z') ||
                                 (character >= '0' && character <= '9');

        /* The name becomes part of a Windows object name, so the character set
           is restricted to what is safe there. */
        if (!alphanumeric && character != '_' && character != '-' &&
            character != '.') {
            return WCAN_ERROR_INVALID_BUS;
        }
    }
    return WCAN_OK;
}

int wcan_validate_frame(const wcan_frame_t *frame)
{
    const uint8_t allowed_flags = WCAN_FLAG_EXTENDED | WCAN_FLAG_RTR |
                                  WCAN_FLAG_FD | WCAN_FLAG_BRS | WCAN_FLAG_ESI;

    if (frame == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    if ((frame->flags & (uint8_t)~allowed_flags) != 0) {
        return WCAN_ERROR_INVALID_FRAME;
    }
    if ((frame->flags & WCAN_FLAG_EXTENDED) != 0) {
        if (frame->can_id > WCAN_EXTENDED_ID_MAX) {
            return WCAN_ERROR_INVALID_FRAME;
        }
    } else if (frame->can_id > WCAN_STANDARD_ID_MAX) {
        return WCAN_ERROR_INVALID_FRAME;
    }

    if ((frame->flags & WCAN_FLAG_FD) != 0) {
        /* CAN FD has no remote frames. */
        if (frame->dlc > WCAN_MAX_DATA || (frame->flags & WCAN_FLAG_RTR) != 0) {
            return WCAN_ERROR_INVALID_FRAME;
        }
    } else {
        /* Bit-rate switch and error state indicator are FD-only. */
        if (frame->dlc > WCAN_CLASSIC_DATA_MAX ||
            (frame->flags & (WCAN_FLAG_BRS | WCAN_FLAG_ESI)) != 0) {
            return WCAN_ERROR_INVALID_FRAME;
        }
    }
    return WCAN_OK;
}

const char *wcan_strerror(int status)
{
    const char *description;

    switch (status) {
    case WCAN_OK:
        description = "success";
        break;

    case WCAN_ERROR_INVALID_ARGUMENT:
        description = "invalid argument";
        break;

    case WCAN_ERROR_NO_MEMORY:
        description = "out of memory";
        break;

    case WCAN_ERROR_UNAVAILABLE:
        description = "bus is unavailable";
        break;

    case WCAN_ERROR_IO:
        description = "I/O error";
        break;

    case WCAN_ERROR_TIMEOUT:
        description = "operation timed out";
        break;

    case WCAN_ERROR_PROTOCOL:
        description = "protocol or layout mismatch";
        break;

    case WCAN_ERROR_CLOSED:
        description = "socket is closed";
        break;

    case WCAN_ERROR_ALREADY_OPEN:
        description = "socket is already open";
        break;

    case WCAN_ERROR_INVALID_BUS:
        description = "invalid bus name";
        break;

    case WCAN_ERROR_INVALID_FRAME:
        description = "invalid frame";
        break;

    case WCAN_ERROR_ACCESS_DENIED:
        description = "access denied";
        break;

    case WCAN_ERROR_TOO_MANY_CLIENTS:
        description = "too many nodes on the bus";
        break;

    default:
        description = "unknown status";
        break;
    }

    return description;
}
