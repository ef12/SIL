/**
 * @file can_driver.h
 * @brief CAN driver abstraction for SIL modules.
 */

#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include "can_frame.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Driver alias for shared CAN frame maximum payload size. */
#define CAN_DRIVER_MAX_DATA_LEN CAN_FRAME_MAX_DATA_LEN

/** Logical node identifier on the virtual CAN bus. */
typedef uint8_t CanDriverNodeId;
/** Driver frame type alias. */
typedef CanFrame CanDriverFrame;

/**
 * @brief Driver instance state.
 */
typedef struct
{
  /** Opaque bus handle (currently emulator instance). */
  void *bus;
  /** Node ID registered on the bus. */
  CanDriverNodeId node_id;
  /** Initialization state flag. */
  bool initialized;
} CanDriver;

/**
 * @brief Initializes a driver instance and registers its node.
 *
 * @param driver Driver instance to initialize.
 * @param bus Bus handle used by the backend implementation.
 * @param node_id Node ID to register.
 * @return true on success, false on invalid input or registration failure.
 */
bool can_driver_init(CanDriver *driver, void *bus, CanDriverNodeId node_id);

/**
 * @brief Sends one CAN frame through the driver backend.
 *
 * @param driver Initialized driver instance.
 * @param frame Frame to send.
 * @return true on success, false otherwise.
 */
bool can_driver_send(CanDriver *driver, const CanDriverFrame *frame);

/**
 * @brief Receives one CAN frame for the driver's node.
 *
 * @param driver Initialized driver instance.
 * @param out_frame Output frame buffer.
 * @param out_sender Optional sender node output pointer.
 * @return true if a frame was received, false otherwise.
 */
bool can_driver_receive(CanDriver *driver, CanDriverFrame *out_frame, CanDriverNodeId *out_sender);

/**
 * @brief Advances one bus arbitration/routing step.
 *
 * @param driver Initialized driver instance.
 * @return true when one pending frame was processed.
 */
bool can_driver_step_bus(CanDriver *driver);

/**
 * @brief Gets the number of pending transmit frames on the backend bus.
 *
 * @param driver Initialized driver instance.
 * @return Pending transmit count, or 0 for invalid input.
 */
size_t can_driver_pending_tx_count(const CanDriver *driver);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DRIVER_H */
