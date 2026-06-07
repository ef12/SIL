/**
 * @file sil_vcan_config.c
 * @brief SIL virtual CAN configuration — assembles socket, transport, emulator, and CAN driver.
 */

#include "sil_vcan_config.h"

#include "can_emulator.h"
#include "can_transport_udp.h"
#include "sil_config_udp_socket.h"

#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* ================================================================
 *  Internal node IDs for the emulated CAN bus.
 * ================================================================ */

#define LOCAL_NODE_ID  ((CanNodeId)0U)
#define REMOTE_NODE_ID ((CanNodeId)1U)

/* ================================================================
 *  SIL CAN driver — extends CanDriver with emulator + transport.
 * ================================================================ */

typedef struct SilVcanInternal SilVcanInternal;

typedef struct
{
  CanDriver base;
  CanEmulator *emulator;
  CanTransportUdp *transport;
  SilVcanInternal *owner;
} SilCanDriver;

static SilCanDriver *can_to_sil(CanDriver *self)
{
  return (SilCanDriver *)self;
}

static const SilCanDriver *can_to_sil_const(const CanDriver *self)
{
  return (const SilCanDriver *)self;
}

/* ================================================================
 *  Platform mutex helpers.
 * ================================================================ */

#ifdef _WIN32
typedef CRITICAL_SECTION SilMutex;
static void sil_mutex_init(SilMutex *m)
{
  InitializeCriticalSection(m);
}
static void sil_mutex_lock(SilMutex *m)
{
  EnterCriticalSection(m);
}
static void sil_mutex_unlock(SilMutex *m)
{
  LeaveCriticalSection(m);
}
static void sil_mutex_destroy(SilMutex *m)
{
  DeleteCriticalSection(m);
}
#else
typedef pthread_mutex_t SilMutex;
static void sil_mutex_init(SilMutex *m)
{
  pthread_mutex_init(m, NULL);
}
static void sil_mutex_lock(SilMutex *m)
{
  pthread_mutex_lock(m);
}
static void sil_mutex_unlock(SilMutex *m)
{
  pthread_mutex_unlock(m);
}
static void sil_mutex_destroy(SilMutex *m)
{
  pthread_mutex_destroy(m);
}
#endif

/* ================================================================
 *  SIL vCAN internal state — owns everything.
 * ================================================================ */

struct SilVcanInternal
{
  UdpSocket socket;
  CanTransportUdp transport;
  CanEmulator emulator;
  SilCanDriver driver;
  SilMutex mutex;
};

static void cleanup(SilVcanInternal *si)
{
  if (si == NULL)
  {
    return;
  }

  can_emulator_deinit(&si->emulator);
  sil_mutex_destroy(&si->mutex);
  udp_socket_close(&si->socket);
  free(si);
}

/*
 * Send: lock → submit + step + drain emulator → unlock → send over UDP.
 * The socket I/O (sendto) is never inside the lock.
 */
static bool sil_can_send(const CanDriver *self, const CanFrame *frame)
{
  const SilCanDriver *sil = can_to_sil_const(self);
  CanFrame drain_buf[16];
  size_t drain_count = 0U;
  bool ok = true;

  sil_mutex_lock(&sil->owner->mutex);

  if (!can_emulator_submit(sil->emulator, LOCAL_NODE_ID, frame))
  {
    sil_mutex_unlock(&sil->owner->mutex);
    return false;
  }

  while (can_emulator_step(sil->emulator))
  {
    /* intentionally empty */
  }

  while (drain_count < (sizeof(drain_buf) / sizeof(drain_buf[0]))
         && can_emulator_receive(sil->emulator, REMOTE_NODE_ID, &drain_buf[drain_count], NULL))
  {
    drain_count++;
  }

  sil_mutex_unlock(&sil->owner->mutex);

  /* Forward drained frames over UDP — outside the lock. */
  for (size_t i = 0U; i < drain_count; i++)
  {
    if (!can_transport_udp_send_frame(sil->transport, &drain_buf[i]))
    {
      ok = false;
      break;
    }
  }

  return ok;
}

/*
 * Receive: recvfrom (may block) outside the lock, then
 * lock → submit + step + dequeue → unlock.
 */
static bool sil_can_receive(CanDriver *self, CanFrame *out_frame)
{
  SilCanDriver *sil = can_to_sil(self);
  CanFrame incoming_buf[16];
  size_t incoming_count = 0U;
  bool result;

  /* Pull UDP frames — socket I/O outside the lock. */
  while (incoming_count < (sizeof(incoming_buf) / sizeof(incoming_buf[0]))
         && can_transport_udp_receive_frame(sil->transport, &incoming_buf[incoming_count], NULL, 0U,
                                            NULL))
  {
    incoming_count++;
  }

  sil_mutex_lock(&sil->owner->mutex);

  for (size_t i = 0U; i < incoming_count; i++)
  {
    if (!can_emulator_submit(sil->emulator, REMOTE_NODE_ID, &incoming_buf[i]))
    {
      break;
    }
  }

  while (can_emulator_step(sil->emulator))
  {
    /* intentionally empty */
  }

  result = can_emulator_receive(sil->emulator, LOCAL_NODE_ID, out_frame, NULL);

  sil_mutex_unlock(&sil->owner->mutex);

  return result;
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

  sil_mutex_init(&si->mutex);

  /* Wire up the SIL CAN driver. */
  si->driver.base.send = sil_can_send;
  si->driver.base.receive = sil_can_receive;
  si->driver.base.close = NULL;
  si->driver.base.initialized = true;
  si->driver.emulator = &si->emulator;
  si->driver.transport = &si->transport;
  si->driver.owner = si;

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
