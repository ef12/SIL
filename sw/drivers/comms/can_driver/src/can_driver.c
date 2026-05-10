#include "can_driver.h"

#include "can_emulator.h"

static bool is_valid_frame(const CanDriverFrame *frame)
{
  return can_frame_is_valid(frame);
}

static CanEmulator *driver_bus(const CanDriver *driver)
{
  if (driver == NULL)
  {
    return NULL;
  }

  return (CanEmulator *)driver->bus;
}

bool can_driver_init(CanDriver *driver, void *bus, CanDriverNodeId node_id)
{
  CanEmulator *emulator;

  if (driver == NULL || bus == NULL)
  {
    return false;
  }

  emulator = (CanEmulator *)bus;
  if (!can_emulator_register_node(emulator, node_id))
  {
    return false;
  }

  driver->bus = bus;
  driver->node_id = node_id;
  driver->initialized = true;
  return true;
}

bool can_driver_send(CanDriver *driver, const CanDriverFrame *frame)
{
  if (driver == NULL || !driver->initialized || !is_valid_frame(frame))
  {
    return false;
  }

  return can_emulator_submit(driver_bus(driver), driver->node_id, frame);
}

bool can_driver_receive(CanDriver *driver, CanDriverFrame *out_frame, CanDriverNodeId *out_sender)
{
  CanNodeId sender;

  if (driver == NULL || !driver->initialized || out_frame == NULL)
  {
    return false;
  }

  if (!can_emulator_receive(driver_bus(driver), driver->node_id, out_frame, &sender))
  {
    return false;
  }

  if (out_sender != NULL)
  {
    *out_sender = sender;
  }

  return true;
}

bool can_driver_step_bus(CanDriver *driver)
{
  if (driver == NULL || !driver->initialized)
  {
    return false;
  }

  return can_emulator_step(driver_bus(driver));
}

size_t can_driver_pending_tx_count(const CanDriver *driver)
{
  if (driver == NULL || !driver->initialized)
  {
    return 0U;
  }

  return can_emulator_pending_tx_count(driver_bus(driver));
}
