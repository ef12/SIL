#include "sil_io_config.h"

#include "io_transport_udp.h"
#include "sil_config_udp_socket.h"

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
 *  SIL IO internal state — owns everything.
 * ================================================================ */

typedef struct
{
  UdpSocket socket;
  IoTransportUdp transport;
  bool *digital_buf;
  uint16_t *analog_buf;
  SilIoDriver driver;
} SilIoInternal;

static void cleanup(SilIoInternal *si)
{
  if (si == NULL)
  {
    return;
  }

  udp_socket_close(&si->socket);
  free(si->digital_buf);
  free(si->analog_buf);
  free(si);
}

bool sil_io_config_init(SilIoConfig *sil, const SilIoConfigParams *params)
{
  SilIoInternal *si;

  if (sil == NULL || params == NULL || params->remote_ip == NULL)
  {
    return false;
  }

  si = (SilIoInternal *)calloc(1U, sizeof(SilIoInternal));
  if (si == NULL)
  {
    return false;
  }

  if (params->digital_pin_count > 0U)
  {
    si->digital_buf = (bool *)calloc(params->digital_pin_count, sizeof(bool));
    if (si->digital_buf == NULL)
    {
      free(si);
      return false;
    }
  }

  if (params->analog_pin_count > 0U)
  {
    si->analog_buf = (uint16_t *)calloc(params->analog_pin_count, sizeof(uint16_t));
    if (si->analog_buf == NULL)
    {
      free(si->digital_buf);
      free(si);
      return false;
    }
  }

  if (!sil_config_udp_socket_init(&si->socket, params->local_port, params->timeout_ms))
  {
    free(si->digital_buf);
    free(si->analog_buf);
    free(si);
    return false;
  }

  if (!io_transport_udp_init(&si->transport, &si->socket, params->remote_ip, params->remote_port,
                             si->digital_buf, params->digital_pin_count, si->analog_buf,
                             params->analog_pin_count))
  {
    cleanup(si);
    return false;
  }

  /* Wire up the SIL IO driver. */
  si->driver.base.sync_inputs = sil_io_sync_inputs;
  si->driver.base.sync_outputs = sil_io_sync_outputs;
  si->driver.base.digital_read = sil_io_digital_read;
  si->driver.base.digital_write = sil_io_digital_write;
  si->driver.base.analog_read = sil_io_analog_read;
  si->driver.base.analog_write = sil_io_analog_write;
  si->driver.base.close = NULL;
  si->driver.base.digital_pin_count = params->digital_pin_count;
  si->driver.base.analog_pin_count = params->analog_pin_count;
  si->driver.base.initialized = true;
  si->driver.transport = &si->transport;

  sil->internal = si;
  sil->initialized = true;

  return true;
}

IoDriver *sil_io_config_get_driver(const SilIoConfig *sil)
{
  SilIoInternal *si;

  if (sil == NULL || !sil->initialized)
  {
    return NULL;
  }

  si = (SilIoInternal *)sil->internal;
  return &si->driver.base;
}

void sil_io_config_deinit(SilIoConfig *sil)
{
  if (sil == NULL)
  {
    return;
  }

  cleanup((SilIoInternal *)sil->internal);
  sil->internal = NULL;
  sil->initialized = false;
}
