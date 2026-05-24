/**
 * @file io_driver.c
 * @brief Platform-independent dispatch — delegates to function pointers.
 */

#include "io_driver.h"

bool io_driver_sync_inputs(IoDriver *driver)
{
  if (driver == NULL || !driver->initialized || driver->sync_inputs == NULL)
  {
    return false;
  }

  return driver->sync_inputs(driver);
}

bool io_driver_sync_outputs(const IoDriver *driver)
{
  if (driver == NULL || !driver->initialized || driver->sync_outputs == NULL)
  {
    return false;
  }

  return driver->sync_outputs(driver);
}

bool io_driver_digital_read(const IoDriver *driver, uint16_t pin, bool *value)
{
  if (driver == NULL || !driver->initialized || driver->digital_read == NULL)
  {
    return false;
  }

  return driver->digital_read(driver, pin, value);
}

bool io_driver_digital_write(IoDriver *driver, uint16_t pin, bool value)
{
  if (driver == NULL || !driver->initialized || driver->digital_write == NULL)
  {
    return false;
  }

  return driver->digital_write(driver, pin, value);
}

bool io_driver_analog_read(const IoDriver *driver, uint16_t pin, uint16_t *value)
{
  if (driver == NULL || !driver->initialized || driver->analog_read == NULL)
  {
    return false;
  }

  return driver->analog_read(driver, pin, value);
}

bool io_driver_analog_write(IoDriver *driver, uint16_t pin, uint16_t value)
{
  if (driver == NULL || !driver->initialized || driver->analog_write == NULL)
  {
    return false;
  }

  return driver->analog_write(driver, pin, value);
}

void io_driver_close(IoDriver *driver)
{
  if (driver == NULL || !driver->initialized)
  {
    return;
  }

  if (driver->close != NULL)
  {
    driver->close(driver);
  }

  driver->initialized = false;
}
