#include "pi_controller.h"

#include <stddef.h>

bool pi_controller_init(PiController *pi, float kp, float ki, float output_min, float output_max)
{
  if (pi == NULL || output_min > output_max)
  {
    return false;
  }

  pi->kp = kp;
  pi->ki = ki;
  pi->integral = 0.0f;
  pi->output_min = output_min;
  pi->output_max = output_max;

  return true;
}

float pi_controller_update(PiController *pi, float error, float dt)
{
  float output;

  if (pi == NULL || dt <= 0.0f)
  {
    return 0.0f;
  }

  pi->integral += error * dt;
  output = (pi->kp * error) + (pi->ki * pi->integral);

  if (output > pi->output_max)
  {
    output = pi->output_max;
  }
  else if (output < pi->output_min)
  {
    output = pi->output_min;
  }

  return output;
}

void pi_controller_reset(PiController *pi)
{
  if (pi != NULL)
  {
    pi->integral = 0.0f;
  }
}
