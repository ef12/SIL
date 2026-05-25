/**
 * @file can_driver.c
 * @brief Platform-independent dispatch — delegates to function pointers.
 */

#include "can_driver.h"

bool can_driver_send(const CanDriver *driver, const CanFrame *frame)
{
  if (driver == NULL || !driver->initialized || driver->send == NULL || frame == NULL)
  {
    return false;
  }

  return driver->send(driver, frame);
}

bool can_driver_receive(CanDriver *driver, CanFrame *out_frame)
{
  if (driver == NULL || !driver->initialized || driver->receive == NULL || out_frame == NULL)
  {
    return false;
  }

  return driver->receive(driver, out_frame);
}

void can_driver_close(CanDriver *driver)
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
