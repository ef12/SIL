#include "fan_controller.h"

#include <stddef.h>
#include <stdio.h>

bool fan_controller_init(FanController *fc, const FanControllerConfig *config, IoDriver *io,
                         CanDriver *can)
{
  if (fc == NULL || config == NULL || io == NULL || can == NULL)
  {
    return false;
  }

  if (!io->initialized || !can->initialized)
  {
    return false;
  }

  if (!pi_controller_init(&fc->pi, config->kp, config->ki, config->output_min, config->output_max))
  {
    return false;
  }

  fc->io = io;
  fc->can = can;
  fc->enable_pin = config->enable_pin;
  fc->feedback_pin = config->feedback_pin;
  fc->output_pin = config->output_pin;
  fc->setpoint = config->setpoint;
  fc->status_can_id = config->status_can_id;
  fc->command_can_id = config->command_can_id;
  fc->feedback = 0U;
  fc->output = 0U;
  fc->enabled = false;

  return true;
}

bool fan_controller_update(FanController *fc, float dt)
{
  bool enable_val = false;
  uint16_t feedback_val = 0U;
  float error;
  float pi_out;

  if (fc == NULL)
  {
    return false;
  }

  /* Read enable input. */
  if (!io_driver_digital_read(fc->io, fc->enable_pin, &enable_val))
  {
    return false;
  }

  fc->enabled = enable_val;

  if (!fc->enabled)
  {
    pi_controller_reset(&fc->pi);
    fc->output = 0U;
    fc->feedback = 0U;
    return io_driver_analog_write(fc->io, fc->output_pin, 0U);
  }

  /* Read speed feedback. */
  if (!io_driver_analog_read(fc->io, fc->feedback_pin, &feedback_val))
  {
    return false;
  }

  fc->feedback = feedback_val;

  /* Compute PI output. */
  error = (float)fc->setpoint - (float)feedback_val;
  pi_out = pi_controller_update(&fc->pi, error, dt);
  fc->output = (uint16_t)pi_out;

  return io_driver_analog_write(fc->io, fc->output_pin, fc->output);
}

void fan_controller_set_setpoint(FanController *fc, uint16_t setpoint)
{
  if (fc != NULL)
  {
    fc->setpoint = setpoint;
  }
}

bool fan_controller_send_status(const FanController *fc)
{
  CanFrame frame;

  if (fc == NULL || fc->can == NULL)
  {
    return false;
  }

  can_frame_clear(&frame);
  frame.id = fc->status_can_id;
  frame.dlc = 7U;
  frame.data[0] = fc->enabled ? 1U : 0U;
  frame.data[1] = (uint8_t)(fc->setpoint & 0xFFU);
  frame.data[2] = (uint8_t)((fc->setpoint >> 8U) & 0xFFU);
  frame.data[3] = (uint8_t)(fc->feedback & 0xFFU);
  frame.data[4] = (uint8_t)((fc->feedback >> 8U) & 0xFFU);
  frame.data[5] = (uint8_t)(fc->output & 0xFFU);
  frame.data[6] = (uint8_t)((fc->output >> 8U) & 0xFFU);

  return can_driver_send(fc->can, &frame);
}

bool fan_controller_receive_commands(FanController *fc)
{
  CanFrame frame;

  if (fc == NULL || fc->can == NULL)
  {
    return false;
  }

  if (!can_driver_receive(fc->can, &frame))
  {
    return false;
  }

  if (frame.id == fc->command_can_id && frame.dlc >= 2U)
  {
    uint16_t sp = (uint16_t)frame.data[0] | (uint16_t)((uint16_t)frame.data[1] << 8U);
    fan_controller_set_setpoint(fc, sp);
    (void)fan_controller_send_status(fc);
    return true;
  }

  return false;
}
