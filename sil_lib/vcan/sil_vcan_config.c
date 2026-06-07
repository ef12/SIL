/**
 * @file sil_vcan_config.c
 * @brief SIL virtual CAN configuration — assembles socket, transport, emulator, and CAN driver.
 */

#include "sil_vcan_config.h"

#include "can_emulator.h"
#include "can_transport_udp.h"
#include "sil_config_udp_socket.h"

#include <stdlib.h>

/* ================================================================
 *  Internal node IDs for the emulated CAN bus.
 * ================================================================ */

#define LOCAL_NODE_ID  ((CanNodeId)0U)
#define REMOTE_NODE_ID ((CanNodeId)1U)

/* ================================================================
 *  SIL CAN driver — extends CanDriver with emulator + transport.
 * ================================================================ */

typedef struct
{
  CanDriver base;
  CanEmulator *emulator;
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
  const SilCanDriver *sil = can_to_sil_const(self);
  CanFrame routed;

  /* Submit frame into the emulator TX queue. */
  if (!can_emulator_submit(sil->emulator, LOCAL_NODE_ID, frame))
  {
    return false;
  }

  /* Run arbitration — route all pending frames. */
  while (can_emulator_step(sil->emulator))
  {
    /* intentionally empty */
  }

  /* Drain the remote node's RX queue and forward over UDP. */
  while (can_emulator_receive(sil->emulator, REMOTE_NODE_ID, &routed, NULL))
  {
    if (!can_transport_udp_send_frame(sil->transport, &routed))
    {
      return false;
    }
  }

  return true;
}

static bool sil_can_receive(CanDriver *self, CanFrame *out_frame)
{
  SilCanDriver *sil = can_to_sil(self);
  CanFrame incoming;

  /* Pull any frames from UDP and inject as remote-node submissions. */
  while (can_transport_udp_receive_frame(sil->transport, &incoming, NULL, 0U, NULL))
  {
    if (!can_emulator_submit(sil->emulator, REMOTE_NODE_ID, &incoming))
    {
      break;
    }
  }

  /* Run arbitration — route all pending frames. */
  while (can_emulator_step(sil->emulator))
  {
    /* intentionally empty */
  }

  /* Dequeue one frame from the local node's RX queue. */
  return can_emulator_receive(sil->emulator, LOCAL_NODE_ID, out_frame, NULL);
}

/* ================================================================
 *  Direct-UDP callbacks — bypass the emulator entirely.
 *  sendto / recvfrom on the same socket are thread-safe, so these
 *  can be called concurrently from different threads.
 * ================================================================ */

static bool sil_can_send_direct(const CanDriver *self, const CanFrame *frame)
{
  const SilCanDriver *sil = can_to_sil_const(self);

  return can_transport_udp_send_frame(sil->transport, frame);
}

static bool sil_can_receive_direct(CanDriver *self, CanFrame *out_frame)
{
  SilCanDriver *sil = can_to_sil(self);

  return can_transport_udp_receive_frame(sil->transport, out_frame, NULL, 0U, NULL);
}

/* ================================================================
 *  SIL vCAN internal state — owns everything.
 * ================================================================ */

typedef struct
{
  UdpSocket socket;
  CanTransportUdp transport;
  CanEmulator emulator;
  SilCanDriver driver;
} SilVcanInternal;

static void cleanup(SilVcanInternal *si)
{
  if (si == NULL)
  {
    return;
  }

  can_emulator_deinit(&si->emulator);
  udp_socket_close(&si->socket);
  free(si);
}

bool sil_vcan_config_init(SilVcanConfig *sil, const SilVcanConfigParams *params)
{
  SilVcanInternal *si;
  CanEmulatorConfig emu_cfg;

  if (sil == NULL || params == NULL || params->remote_ip == NULL)
  {
    return false;
  }

  emu_cfg.max_nodes = 2U;
  emu_cfg.max_pending_tx = params->max_pending_tx;
  emu_cfg.max_rx_queue = params->max_rx_queue;

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

  if (params->use_emulator)
  {
    /* Initialize the CAN bus emulator with local and remote nodes. */
    if (!can_emulator_init(&si->emulator, &emu_cfg))
    {
      cleanup(si);
      return false;
    }

    if (!can_emulator_register_node(&si->emulator, LOCAL_NODE_ID))
    {
      cleanup(si);
      return false;
    }

    if (!can_emulator_register_node(&si->emulator, REMOTE_NODE_ID))
    {
      cleanup(si);
      return false;
    }

    si->driver.base.send = sil_can_send;
    si->driver.base.receive = sil_can_receive;
  }
  else
  {
    si->driver.base.send = sil_can_send_direct;
    si->driver.base.receive = sil_can_receive_direct;
  }

  /* Wire up the SIL CAN driver. */
  si->driver.base.close = NULL;
  si->driver.base.initialized = true;
  si->driver.emulator = &si->emulator;
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
