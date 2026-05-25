/**
 * @file can_driver.h
 * @brief Abstract CAN driver interface.
 *
 * Platform-independent.  Each target (SIL, MCU, …) provides its own
 * implementation by embedding this struct and filling in the function
 * pointers.  Application code calls the dispatch functions below;
 * it never touches platform internals.
 */

#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include "can_frame.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration — implementations extend this struct. */
typedef struct CanDriver CanDriver;

/**
 * @brief Abstract CAN driver.
 *
 * Platform code embeds this as the first member of a larger struct,
 * fills in every function pointer, and sets initialized = true.
 */
struct CanDriver
{
  bool (*send)(const CanDriver *self, const CanFrame *frame);
  bool (*receive)(CanDriver *self, CanFrame *out_frame);
  void (*close)(CanDriver *self);
  bool initialized;
};

/**
 * @brief Sends a CAN frame via the driver.
 *
 * @param driver Initialized driver (must not be NULL).
 * @param frame  Frame to send (must not be NULL).
 * @return true on success, false if any argument is NULL or the driver
 *         is not initialized.
 */
bool can_driver_send(const CanDriver *driver, const CanFrame *frame);

/**
 * @brief Receives a CAN frame from the driver.
 *
 * @param driver    Initialized driver (must not be NULL).
 * @param out_frame Destination for the received frame (must not be NULL).
 * @return true on success, false if any argument is NULL, the driver
 *         is not initialized, or no frame is available.
 */
bool can_driver_receive(CanDriver *driver, CanFrame *out_frame);

/**
 * @brief Closes the driver and releases resources.
 *
 * After this call the driver is marked as not initialized.
 * Safe to call with a NULL or already-closed driver (no-op).
 *
 * @param driver Driver to close.
 */
void can_driver_close(CanDriver *driver);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DRIVER_H */
