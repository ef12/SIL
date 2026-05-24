#include "sil_vcan_config.h"

#include "can_transport_udp.h"
#include "sil_config_udp_socket.h"

#include <stdlib.h>

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
 *  SIL vCAN internal state — owns everything.
 * ================================================================ */

typedef struct
{
  UdpSocket socket;
  CanTransportUdp transport;
  SilCanDriver driver;
} SilVcanInternal;

static void cleanup(SilVcanInternal *si)
{
  if (si == NULL)
  {
    return;
  }

  udp_socket_close(&si->socket);
  free(si);
}

bool sil_vcan_config_init(SilVcanConfig *sil, const SilVcanConfigParams *params)
{
  SilVcanInternal *si;

  if (sil == NULL || params == NULL || params->remote_ip == NULL)
  {
    return false;
  }

  si = (SilVcanInternal *)calloc(1U, sizeof(SilVcanInternal));
  if (si == NULL)
  {
    return false;
  }

  if (!sil_config_udp_socket_init(&si->socket, params->local_port, params->timeout_ms))
  {
    free(si);
    return false;
  }

  if (!can_transport_udp_init(&si->transport, &si->socket, params->remote_ip, params->remote_port))
  {
    cleanup(si);
    return false;
  }

  /* Wire up the SIL CAN driver. */
  si->driver.base.send = sil_can_send;
  si->driver.base.receive = sil_can_receive;
  si->driver.base.close = NULL;
  si->driver.base.initialized = true;
  si->driver.transport = &si->transport;

  sil->internal = si;
  sil->initialized = true;

  return true;
}

CanDriver *sil_vcan_config_get_driver(const SilVcanConfig *sil)
{
  SilVcanInternal *si;

  if (sil == NULL || !sil->initialized)
  {
    return NULL;
  }

  si = (SilVcanInternal *)sil->internal;
  return &si->driver.base;
}

void sil_vcan_config_deinit(SilVcanConfig *sil)
{
  if (sil == NULL)
  {
    return;
  }

  cleanup((SilVcanInternal *)sil->internal);
  sil->internal = NULL;
  sil->initialized = false;
}
