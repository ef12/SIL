/**
 * @file pi_controller.h
 * @brief Generic PI controller with output clamping.
 */

#ifndef PI_CONTROLLER_H
#define PI_CONTROLLER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  float kp;
  float ki;
  float integral;
  float output_min;
  float output_max;
} PiController;

bool pi_controller_init(PiController *pi, float kp, float ki, float output_min, float output_max);

float pi_controller_update(PiController *pi, float error, float dt);

void pi_controller_reset(PiController *pi);

#ifdef __cplusplus
}
#endif

#endif /* PI_CONTROLLER_H */
