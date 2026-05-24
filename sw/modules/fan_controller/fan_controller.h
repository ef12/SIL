/**
 * @file fan_controller.h
 * @brief Fan speed controller with PI loop, IO and CAN abstraction.
 *
 * Receives drivers via Dependency Injection at init time.
 * Owns its CAN protocol (status publication, command reception).
 */

#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include "can_driver.h"
#include "io_driver.h"
#include "pi_controller.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Static configuration supplied once at init. */
typedef struct
{
  uint16_t enable_pin;
  uint16_t feedback_pin;
  uint16_t output_pin;
  uint16_t setpoint;
  float kp;
  float ki;
  float output_min;
  float output_max;
  uint32_t status_can_id;
  uint32_t command_can_id;
} FanControllerConfig;

/** Runtime state of the fan controller. */
typedef struct
{
  PiController pi;
  IoDriver *io;
  CanDriver *can;
  uint16_t enable_pin;
  uint16_t feedback_pin;
  uint16_t output_pin;
  uint16_t setpoint;
  uint16_t feedback;
  uint16_t output;
  uint32_t status_can_id;
  uint32_t command_can_id;
  bool enabled;
} FanController;

bool fan_controller_init(FanController *fc, const FanControllerConfig *config, IoDriver *io,
                         CanDriver *can);

bool fan_controller_update(FanController *fc, float dt);

void fan_controller_set_setpoint(FanController *fc, uint16_t setpoint);

/** Publishes the current fan status on CAN. */
bool fan_controller_send_status(const FanController *fc);

/** Polls for incoming CAN commands and applies them. */
bool fan_controller_receive_commands(FanController *fc);

#ifdef __cplusplus
}
#endif

#endif /* FAN_CONTROLLER_H */
