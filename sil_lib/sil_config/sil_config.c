#include "sil_config.h"

#include "can_transport_udp.h"
#include "io_transport_udp.h"
#include "udp_socket.h"

#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  SIL IO driver — extends IoDriver with transport pointer.
 * ================================================================ */

typedef struct
{
  IoDriver base;
  IoTransportUdp *transport;
} SilIoDriver;

static SilIoDriver *io_to_sil(IoDriver *self)
{
  return (SilIoDriver *)self;
}

static const SilIoDriver *io_to_sil_const(const IoDriver *self)
{
  return (const SilIoDriver *)self;
}

static bool sil_io_sync_inputs(IoDriver *self)
{
  return io_transport_udp_receive(io_to_sil(self)->transport, NULL, 0U, NULL);
}

static bool sil_io_sync_outputs(const IoDriver *self)
{
  return io_transport_udp_send(io_to_sil_const(self)->transport);
}

static bool sil_io_digital_read(const IoDriver *self, uint16_t pin, bool *value)
{
  return io_transport_udp_digital_get(io_to_sil_const(self)->transport, (size_t)pin, value);
}

static bool sil_io_digital_write(IoDriver *self, uint16_t pin, bool value)
{
  return io_transport_udp_digital_set(io_to_sil(self)->transport, (size_t)pin, value);
}

static bool sil_io_analog_read(const IoDriver *self, uint16_t pin, uint16_t *value)
{
  return io_transport_udp_analog_get(io_to_sil_const(self)->transport, (size_t)pin, value);
}

static bool sil_io_analog_write(IoDriver *self, uint16_t pin, uint16_t value)
{
  return io_transport_udp_analog_set(io_to_sil(self)->transport, (size_t)pin, value);
}

/* ================================================================
 *  SIL CAN driver — extends CanDriver with transport pointer.
 * ================================================================ */

typedef struct
{
  CanDriver base;
  CanTransportUdp *transport;
} SilCanDriver;

static SilCanDriver *can_to_sil(CanDriver *self)
{
  return (SilCanDriver *)self;
}

static const SilCanDriver *can_to_sil_const(const CanDriver *self)
{
  return (const SilCanDriver *)self;
}

static bool sil_can_send(const CanDriver *self, const CanFrame *frame)
{
  return can_transport_udp_send_frame(can_to_sil_const(self)->transport, frame);
}

static bool sil_can_receive(CanDriver *self, CanFrame *out_frame)
{
  return can_transport_udp_receive_frame(can_to_sil(self)->transport, out_frame, NULL, 0U, NULL);
}

/* ================================================================
 *  SIL internal state — owns everything.
 * ================================================================ */

typedef struct
{
  /* IO peripheral. */
  UdpSocket io_socket;
  IoTransportUdp io_transport;
  bool *digital_buf;
  uint16_t *analog_buf;
  SilIoDriver io_driver;
  bool io_configured;

  /* CAN peripheral. */
  UdpSocket can_socket;
  CanTransportUdp can_transport;
  SilCanDriver can_driver;
  bool can_configured;
} SilInternal;

static void cleanup(SilInternal *si)
{
  if (si == NULL)
  {
    return;
  }

  if (si->can_configured)
  {
    udp_socket_close(&si->can_socket);
  }

  if (si->io_configured)
  {
    udp_socket_close(&si->io_socket);
  }

  free(si->digital_buf);
  free(si->analog_buf);
  free(si);
}

bool sil_config_init(SilConfig *sil, const SilConfigParams *params)
{
  SilInternal *si;

  if (sil == NULL || params == NULL)
  {
    return false;
  }

  if (params->io.remote_ip == NULL && params->can.remote_ip == NULL)
  {
    return false;
  }

  si = (SilInternal *)calloc(1U, sizeof(SilInternal));
  if (si == NULL)
  {
    return false;
  }

  /* ---- IO peripheral (optional) ---- */

  if (params->io.remote_ip != NULL)
  {
    if (params->io.digital_pin_count > 0U)
    {
      si->digital_buf = (bool *)calloc(params->io.digital_pin_count, sizeof(bool));
      if (si->digital_buf == NULL)
      {
        free(si);
        return false;
      }
    }

    if (params->io.analog_pin_count > 0U)
    {
      si->analog_buf = (uint16_t *)calloc(params->io.analog_pin_count, sizeof(uint16_t));
      if (si->analog_buf == NULL)
      {
        free(si->digital_buf);
        free(si);
        return false;
      }
    }

    if (!udp_socket_init(&si->io_socket, params->io.local_port))
    {
      free(si->digital_buf);
      free(si->analog_buf);
      free(si);
      return false;
    }

    if (!io_transport_udp_init(&si->io_transport, &si->io_socket, params->io.remote_ip,
                               params->io.remote_port, si->digital_buf,
                               params->io.digital_pin_count, si->analog_buf,
                               params->io.analog_pin_count))
    {
      cleanup(si);
      return false;
    }

    if (params->io.timeout_ms > 0U
        && !udp_socket_set_receive_timeout(&si->io_socket, params->io.timeout_ms))
    {
      cleanup(si);
      return false;
    }

    /* Wire up the SIL IO driver. */
    si->io_driver.base.sync_inputs = sil_io_sync_inputs;
    si->io_driver.base.sync_outputs = sil_io_sync_outputs;
    si->io_driver.base.digital_read = sil_io_digital_read;
    si->io_driver.base.digital_write = sil_io_digital_write;
    si->io_driver.base.analog_read = sil_io_analog_read;
    si->io_driver.base.analog_write = sil_io_analog_write;
    si->io_driver.base.close = NULL;
    si->io_driver.base.digital_pin_count = params->io.digital_pin_count;
    si->io_driver.base.analog_pin_count = params->io.analog_pin_count;
    si->io_driver.base.initialized = true;
    si->io_driver.transport = &si->io_transport;

    si->io_configured = true;
  }

  /* ---- CAN peripheral (optional) ---- */

  if (params->can.remote_ip != NULL)
  {
    if (!udp_socket_init(&si->can_socket, params->can.local_port))
    {
      cleanup(si);
      return false;
    }

    if (!can_transport_udp_init(&si->can_transport, &si->can_socket, params->can.remote_ip,
                                params->can.remote_port))
    {
      cleanup(si);
      return false;
    }

    if (params->can.timeout_ms > 0U
        && !udp_socket_set_receive_timeout(&si->can_socket, params->can.timeout_ms))
    {
      cleanup(si);
      return false;
    }

    /* Wire up the SIL CAN driver. */
    si->can_driver.base.send = sil_can_send;
    si->can_driver.base.receive = sil_can_receive;
    si->can_driver.base.close = NULL;
    si->can_driver.base.initialized = true;
    si->can_driver.transport = &si->can_transport;

    si->can_configured = true;
  }

  sil->internal = si;
  sil->initialized = true;

  return true;
}

IoDriver *sil_config_get_io_driver(const SilConfig *sil)
{
  SilInternal *si;

  if (sil == NULL || !sil->initialized)
  {
    return NULL;
  }

  si = (SilInternal *)sil->internal;
  return si->io_configured ? &si->io_driver.base : NULL;
}

CanDriver *sil_config_get_can_driver(const SilConfig *sil)
{
  SilInternal *si;

  if (sil == NULL || !sil->initialized)
  {
    return NULL;
  }

  si = (SilInternal *)sil->internal;
  return si->can_configured ? &si->can_driver.base : NULL;
}

void sil_config_deinit(SilConfig *sil)
{
  if (sil == NULL)
  {
    return;
  }

  cleanup((SilInternal *)sil->internal);
  sil->internal = NULL;
  sil->initialized = false;
}
