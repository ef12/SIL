/**
 * @file io_driver.h
 * @brief Abstract IO driver interface.
 *
 * Platform-independent.  Each target (SIL, MCU, …) provides its own
 * implementation by embedding this struct and filling in the function
 * pointers.  Application code calls the dispatch functions below;
 * it never touches platform internals.
 */

#ifndef IO_DRIVER_H
#define IO_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration — implementations extend this struct. */
typedef struct IoDriver IoDriver;

/**
 * @brief Abstract IO driver.
 *
 * Platform code embeds this as the first member of a larger struct,
 * fills in every function pointer, and sets initialized = true.
 */
struct IoDriver
{
  bool (*digital_read)(const IoDriver *self, uint16_t pin, bool *value);
  bool (*digital_write)(IoDriver *self, uint16_t pin, bool value);
  bool (*analog_read)(const IoDriver *self, uint16_t pin, uint16_t *value);
  bool (*analog_write)(IoDriver *self, uint16_t pin, uint16_t value);
  void (*close)(IoDriver *self);
  size_t digital_pin_count;
  size_t analog_pin_count;
  bool initialized;
};

/**
 * @brief Reads a digital pin.
 *
 * @param driver Initialized driver (must not be NULL).
 * @param pin    Pin index to read.
 * @param value  Output: true = high, false = low (must not be NULL).
 * @return true on success, false if any argument is NULL or the driver
 *         is not initialized.
 */
bool io_driver_digital_read(const IoDriver *driver, uint16_t pin, bool *value);

/**
 * @brief Writes a digital pin.
 *
 * @param driver Initialized driver (must not be NULL).
 * @param pin    Pin index to write.
 * @param value  true = high, false = low.
 * @return true on success, false if the driver is NULL or not initialized.
 */
bool io_driver_digital_write(IoDriver *driver, uint16_t pin, bool value);

/**
 * @brief Reads an analog pin.
 *
 * @param driver Initialized driver (must not be NULL).
 * @param pin    Pin index to read.
 * @param value  Output: raw ADC value (must not be NULL).
 * @return true on success, false if any argument is NULL or the driver
 *         is not initialized.
 */
bool io_driver_analog_read(const IoDriver *driver, uint16_t pin, uint16_t *value);

/**
 * @brief Writes an analog pin.
 *
 * @param driver Initialized driver (must not be NULL).
 * @param pin    Pin index to write.
 * @param value  Raw DAC value.
 * @return true on success, false if the driver is NULL or not initialized.
 */
bool io_driver_analog_write(IoDriver *driver, uint16_t pin, uint16_t value);

/**
 * @brief Closes the driver and releases resources.
 *
 * After this call the driver is marked as not initialized.
 * Safe to call with a NULL or already-closed driver (no-op).
 *
 * @param driver Driver to close.
 */
void io_driver_close(IoDriver *driver);

#ifdef __cplusplus
}
#endif

#endif /* IO_DRIVER_H */
