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
  bool (*sync_inputs)(IoDriver *self);
  bool (*sync_outputs)(const IoDriver *self);
  bool (*digital_read)(const IoDriver *self, uint16_t pin, bool *value);
  bool (*digital_write)(IoDriver *self, uint16_t pin, bool value);
  bool (*analog_read)(const IoDriver *self, uint16_t pin, uint16_t *value);
  bool (*analog_write)(IoDriver *self, uint16_t pin, uint16_t value);
  void (*close)(IoDriver *self);
  size_t digital_pin_count;
  size_t analog_pin_count;
  bool initialized;
};

bool io_driver_sync_inputs(IoDriver *driver);
bool io_driver_sync_outputs(const IoDriver *driver);
bool io_driver_digital_read(const IoDriver *driver, uint16_t pin, bool *value);
bool io_driver_digital_write(IoDriver *driver, uint16_t pin, bool value);
bool io_driver_analog_read(const IoDriver *driver, uint16_t pin, uint16_t *value);
bool io_driver_analog_write(IoDriver *driver, uint16_t pin, uint16_t value);
void io_driver_close(IoDriver *driver);

#ifdef __cplusplus
}
#endif

#endif /* IO_DRIVER_H */
