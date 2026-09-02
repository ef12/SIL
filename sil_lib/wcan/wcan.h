/**
 * @file wcan.h
 * @brief Umbrella header for the WCAN virtual CAN bus.
 *
 * WCAN gives several local processes a shared software CAN bus on Windows,
 * without CAN hardware and without a kernel driver. Nodes meet in a named
 * shared-memory segment; there is no broker process to install or start.
 *
 * Including this header brings in the portable core plus the transport API.
 * A consumer that only needs frames, validation or the airtime model can
 * include wcan_types.h, wcan_validate.h or wcan_airtime.h instead and stay
 * platform independent.
 */

#ifndef WCAN_H
#define WCAN_H

#include "wcan_types.h"
#include "wcan_validate.h"
#include "wcan_airtime.h"

#if defined(_WIN32)
#include "wcan_shm.h"
#endif

#endif /* WCAN_H */