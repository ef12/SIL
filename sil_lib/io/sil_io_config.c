#include "sil_io_config.h"

#include "io_transport_udp.h"
#include "sil_config_udp_socket.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

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
  volatile bool running;
  uint32_t sync_interval_ms;
#ifdef _WIN32
  HANDLE thread;
#else
  pthread_t thread;
#endif
} SilIoInternal;

static void cleanup(SilIoInternal *si)
{
  if (si == NULL)
  {
    return;
  }

  si->running = false;

#ifdef _WIN32
  if (si->thread != NULL)
  {
    WaitForSingleObject(si->thread, INFINITE);
    CloseHandle(si->thread);
  }
#else
  pthread_join(si->thread, NULL);
#endif

  udp_socket_close(&si->socket);
  free(si->digital_buf);
  free(si->analog_buf);
  free(si);
}

#ifdef _WIN32
static DWORD WINAPI sync_thread(LPVOID arg)
#else
static void *sync_thread(void *arg)
#endif
{
  SilIoInternal *si = (SilIoInternal *)arg;

  while (si->running)
  {
    (void)io_transport_udp_receive(&si->transport, NULL, 0U, NULL);
    (void)io_transport_udp_send(&si->transport);

#ifdef _WIN32
    Sleep(si->sync_interval_ms);
#else
    usleep((useconds_t)si->sync_interval_ms * 1000U);
#endif
  }

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
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

  if (!sil_config_udp_socket_init(&si->socket, params->local_port, 1U))
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
  si->driver.base.digital_read = sil_io_digital_read;
  si->driver.base.digital_write = sil_io_digital_write;
  si->driver.base.analog_read = sil_io_analog_read;
  si->driver.base.analog_write = sil_io_analog_write;
  si->driver.base.close = NULL;
  si->driver.base.digital_pin_count = params->digital_pin_count;
  si->driver.base.analog_pin_count = params->analog_pin_count;
  si->driver.base.initialized = true;
  si->driver.transport = &si->transport;

  /* Start sync thread. */
  si->sync_interval_ms = params->sync_interval_ms;
  si->running = true;

#ifdef _WIN32
  si->thread = CreateThread(NULL, 0, sync_thread, si, 0, NULL);
  if (si->thread == NULL)
  {
    cleanup(si);
    return false;
  }
#else
  if (pthread_create(&si->thread, NULL, sync_thread, si) != 0)
  {
    cleanup(si);
    return false;
  }
#endif

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
