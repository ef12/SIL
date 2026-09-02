#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcan_shm.h"
#include "wcan_airtime.h"
#include "wcan_validate.h"
#include "wcan_layout.h"

#define WCAN_SHM_DEFAULT_BITRATE 250000u
#define WCAN_SHM_DEFAULT_LEAD_US 2000u
#define WCAN_SHM_LOCK_TIMEOUT_MS 5000u
typedef struct {
    HANDLE section;
    HANDLE bus_mutex;
    HANDLE alive_mutex;
    HANDLE own_event;
    HANDLE cancel_event;
    HANDLE peer_events[WCAN_SHM_MAX_NODES];
    wcan_shm_bus_t *bus;
    uint32_t node_index;
    int claimed;
    int timer_raised;
    volatile LONG active_operations;
    volatile LONG closing;
    HANDLE no_operations;
    char bus_name[WCAN_MAX_BUS_NAME + 1u];
} wcan_shm_impl_t;

/*
 * Send and receive may run on different threads from close. These two guards
 * make close wait for in-flight calls to finish instead of freeing the state
 * underneath them, which is exactly what a CANHardwarePlugin does when its
 * owner shuts down while a receive thread is blocked.
 */
static int operation_begin(wcan_shm_impl_t *impl)
{
    if (InterlockedCompareExchange(&impl->closing, 0, 0) != 0) {
        return 0;
    }
    if (InterlockedIncrement(&impl->active_operations) == 1) {
        ResetEvent(impl->no_operations);
    }
    if (InterlockedCompareExchange(&impl->closing, 0, 0) != 0) {
        if (InterlockedDecrement(&impl->active_operations) == 0) {
            SetEvent(impl->no_operations);
        }
        return 0;
    }
    return 1;
}

static void operation_end(wcan_shm_impl_t *impl)
{
    if (InterlockedDecrement(&impl->active_operations) == 0) {
        SetEvent(impl->no_operations);
    }
}

/*
 * Mutex ownership on Windows is per thread and recursive, so a thread that
 * already holds a slot's "alive" mutex would silently re-acquire it when
 * opening a second socket on the same bus and both sockets would land on the
 * same node. The mutex is still what detects a crashed owner across
 * processes, so keep it and additionally track claims within this process.
 */
#define WCAN_SHM_MAX_CLAIMS (WCAN_SHM_MAX_NODES * 4u)

typedef struct {
    int used;
    uint32_t index;
    char bus_name[WCAN_MAX_BUS_NAME + 1u];
} wcan_shm_claim_t;

static wcan_shm_claim_t g_claims[WCAN_SHM_MAX_CLAIMS];
static CRITICAL_SECTION g_claims_lock;
static volatile LONG g_claims_state; /* 0 unset, 1 initializing, 2 ready */

static void claims_init(void)
{
    if (InterlockedCompareExchange(&g_claims_state, 1, 0) == 0) {
        InitializeCriticalSection(&g_claims_lock);
        InterlockedExchange(&g_claims_state, 2);
        return;
    }
    while (InterlockedCompareExchange(&g_claims_state, 2, 2) != 2) {
        Sleep(0);
    }
}

static int claims_reserve(const char *bus_name, uint32_t index)
{
    uint32_t slot;
    int free_slot = -1;

    claims_init();
    EnterCriticalSection(&g_claims_lock);
    for (slot = 0; slot < WCAN_SHM_MAX_CLAIMS; ++slot) {
        if (g_claims[slot].used == 0) {
            if (free_slot < 0) {
                free_slot = (int)slot;
            }
            continue;
        }
        if (g_claims[slot].index == index &&
            strcmp(g_claims[slot].bus_name, bus_name) == 0) {
            LeaveCriticalSection(&g_claims_lock);
            return 0;
        }
    }
    if (free_slot < 0) {
        LeaveCriticalSection(&g_claims_lock);
        return 0;
    }
    g_claims[free_slot].used = 1;
    g_claims[free_slot].index = index;
    (void)snprintf(g_claims[free_slot].bus_name,
                   sizeof(g_claims[free_slot].bus_name), "%s", bus_name);
    LeaveCriticalSection(&g_claims_lock);
    return 1;
}

static void claims_release(const char *bus_name, uint32_t index)
{
    uint32_t slot;

    claims_init();
    EnterCriticalSection(&g_claims_lock);
    for (slot = 0; slot < WCAN_SHM_MAX_CLAIMS; ++slot) {
        if (g_claims[slot].used != 0 && g_claims[slot].index == index &&
            strcmp(g_claims[slot].bus_name, bus_name) == 0) {
            g_claims[slot].used = 0;
            break;
        }
    }
    LeaveCriticalSection(&g_claims_lock);
}

static void build_object_name(wchar_t *destination, size_t destination_length,
                              const char *bus_name, const char *suffix,
                              int index)
{
    char ascii[192];

    if (index >= 0) {
        (void)snprintf(ascii, sizeof(ascii), "Local\\wcan.shm.v1.%s.%s%d",
                       bus_name, suffix, index);
    } else if (suffix != NULL) {
        (void)snprintf(ascii, sizeof(ascii), "Local\\wcan.shm.v1.%s.%s",
                       bus_name, suffix);
    } else {
        (void)snprintf(ascii, sizeof(ascii), "Local\\wcan.shm.v1.%s", bus_name);
    }
    (void)MultiByteToWideChar(CP_ACP, 0, ascii, -1, destination,
                              (int)destination_length);
}

/*
 * A previous owner dying while holding the bus mutex surfaces as
 * WAIT_ABANDONED. Every mutation below writes payload first and publishes
 * the index last, so an interrupted writer leaves the ring consistent and
 * ownership can simply be taken over.
 */
static int bus_lock(wcan_shm_impl_t *impl)
{
    DWORD result = WaitForSingleObject(impl->bus_mutex,
                                       WCAN_SHM_LOCK_TIMEOUT_MS);

    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) {
        return WCAN_OK;
    }
    return result == WAIT_TIMEOUT ? WCAN_ERROR_TIMEOUT : WCAN_ERROR_IO;
}

static void bus_unlock(wcan_shm_impl_t *impl)
{
    (void)ReleaseMutex(impl->bus_mutex);
}

static void ring_push(wcan_shm_node_t *node, const wcan_shm_slot_t *slot)
{
    uint32_t write_index;

    if (node->count >= WCAN_SHM_RING_CAPACITY) {
        /* Receive overrun, exactly as a real controller reports it. */
        node->overrun++;
        return;
    }
    write_index = (node->head + node->count) % WCAN_SHM_RING_CAPACITY;
    node->ring[write_index] = *slot;
    node->count++;
}

static int ring_pop(wcan_shm_node_t *node, wcan_shm_slot_t *out_slot)
{
    if (node->count == 0u) {
        return 0;
    }
    *out_slot = node->ring[node->head];
    node->head = (node->head + 1u) % WCAN_SHM_RING_CAPACITY;
    node->count--;
    return 1;
}

/* ================================================================
 *  Performance counter helpers
 * ================================================================ */

static long long qpc_now(void)
{
    LARGE_INTEGER counter;

    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

static long long qpc_frequency(void)
{
    LARGE_INTEGER frequency;

    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

/* ================================================================
 *  Arbitration and scheduling
 * ================================================================ */

/*
 * Compares the bits a node actually drives onto the wire rather than the
 * numeric identifier: a standard frame beats an extended frame sharing the
 * same base identifier because IDE is dominant, and a data frame beats a
 * remote frame.
 */
static int arbitration_wins(const wcan_shm_pending_t *candidate,
                            const wcan_shm_pending_t *incumbent)
{
    const int candidate_extended =
        (candidate->slot.flags & WCAN_FLAG_EXTENDED) != 0;
    const int incumbent_extended =
        (incumbent->slot.flags & WCAN_FLAG_EXTENDED) != 0;
    const uint32_t candidate_base =
        candidate_extended ? (candidate->slot.can_id >> 18) & 0x7ffu
                           : candidate->slot.can_id & 0x7ffu;
    const uint32_t incumbent_base =
        incumbent_extended ? (incumbent->slot.can_id >> 18) & 0x7ffu
                           : incumbent->slot.can_id & 0x7ffu;
    int candidate_remote;
    int incumbent_remote;

    if (candidate_base != incumbent_base) {
        return candidate_base < incumbent_base;
    }
    if (candidate_extended != incumbent_extended) {
        return incumbent_extended;
    }
    if (candidate_extended) {
        const uint32_t candidate_ext = candidate->slot.can_id & 0x3ffffu;
        const uint32_t incumbent_ext = incumbent->slot.can_id & 0x3ffffu;

        if (candidate_ext != incumbent_ext) {
            return candidate_ext < incumbent_ext;
        }
    }
    candidate_remote = (candidate->slot.flags & WCAN_FLAG_RTR) != 0;
    incumbent_remote = (incumbent->slot.flags & WCAN_FLAG_RTR) != 0;
    if (candidate_remote != incumbent_remote) {
        return incumbent_remote;
    }
    return candidate->slot.sequence < incumbent->slot.sequence;
}

/*
 * Only a sender's oldest queued frame may contend. Reordering within one
 * sender would corrupt ISOBUS transport protocol sequences.
 */
static int is_sender_head(const wcan_shm_bus_t *bus, uint32_t index)
{
    uint32_t other;

    for (other = 0; other < bus->pending_count; ++other) {
        if (other == index) {
            continue;
        }
        if (bus->pending[other].sender == bus->pending[index].sender &&
            bus->pending[other].slot.sequence <
                bus->pending[index].slot.sequence) {
            return 0;
        }
    }
    return 1;
}

static void publish_frame(wcan_shm_bus_t *bus, uint32_t sender,
                          const wcan_shm_slot_t *slot)
{
    uint32_t index;

    for (index = 0; index < WCAN_SHM_MAX_NODES; ++index) {
        wcan_shm_node_t *node = &bus->nodes[index];

        if (node->active != 1u) {
            continue;
        }
        if (index == sender && node->echo == 0u) {
            continue;
        }
        ring_push(node, slot);
    }
}

/*
 * Commits every frame whose transmission has actually begun by "now". A frame
 * scheduled into the future deliberately stays pending so a higher priority
 * frame arriving meanwhile can still win that slot, which is what makes the
 * ordering match a real bus rather than arrival order.
 */
static int schedule_pending(wcan_shm_bus_t *bus, uint64_t now)
{
    const int paced = (bus->flags & (WCAN_SHM_BUS_PACE_ADMISSION |
                                     WCAN_SHM_BUS_PACE_DELIVERY)) != 0u;
    int committed = 0;

    while (bus->pending_count > 0u) {
        uint32_t index;
        uint32_t best = WCAN_SHM_MAX_PENDING;
        uint64_t earliest = 0;
        uint64_t start;
        uint64_t airtime;
        int have_earliest = 0;

        for (index = 0; index < bus->pending_count; ++index) {
            if (!have_earliest || bus->pending[index].arrival < earliest) {
                earliest = bus->pending[index].arrival;
                have_earliest = 1;
            }
        }
        start = earliest;
        if (paced && bus->bus_free_at > start) {
            start = bus->bus_free_at;
        }
        if (start > now) {
            break;
        }

        for (index = 0; index < bus->pending_count; ++index) {
            if (bus->pending[index].arrival > start) {
                continue;
            }
            if (!is_sender_head(bus, index)) {
                continue;
            }
            if (best == WCAN_SHM_MAX_PENDING ||
                arbitration_wins(&bus->pending[index], &bus->pending[best])) {
                best = index;
            }
        }
        if (best == WCAN_SHM_MAX_PENDING) {
            break;
        }

        airtime = ((uint64_t)bus->pending[best].bits * bus->qpc_frequency) /
                  (uint64_t)bus->bitrate;
        bus->bus_free_at = start + airtime;
        bus->pending[best].slot.bus_time = bus->bus_free_at;
        if (bus->total_frames == 0u) {
            bus->first_frame = start;
        }
        bus->total_frames++;
        bus->total_bits += bus->pending[best].bits;
        bus->queued_bits -= bus->pending[best].bits;

        publish_frame(bus, bus->pending[best].sender, &bus->pending[best].slot);
        committed = 1;

        for (index = best; (index + 1u) < bus->pending_count; ++index) {
            bus->pending[index] = bus->pending[index + 1u];
        }
        bus->pending_count--;
    }
    return committed;
}

/*
 * Waits until the virtual bus reaches "target".
 *
 * Admission pacing only has to get the aggregate rate right: the scheduler
 * commits every frame whose start time has passed, so waking late simply
 * commits a batch and the long-run rate is unaffected. Those callers pass
 * precise = 0 and sleep, which costs almost no CPU.
 *
 * Delivery pacing has to release each frame at its own virtual completion
 * instant. At 250 kbit/s that is a 536 us cadence, far below what Windows can
 * sleep to, so those callers pass precise = 1 and spin the final approach.
 */
static void wait_until_tick(long long target, long long frequency, int precise)
{
    for (;;) {
        const long long remaining = target - qpc_now();
        double milliseconds;

        if (remaining <= 0) {
            return;
        }
        milliseconds = ((double)remaining * 1000.0) / (double)frequency;
        if (milliseconds > 2.0) {
            Sleep((DWORD)(milliseconds - 1.0));
            continue;
        }
        if (!precise) {
            /* One tick of the raised timer resolution; the caller re-evaluates
               and the scheduler catches up on whatever became due. */
            Sleep(1);
            return;
        }
        Sleep(0);
    }
}

static HANDLE peer_event(wcan_shm_impl_t *impl, uint32_t index)
{
    wchar_t name[256];

    if (impl->peer_events[index] != NULL) {
        return impl->peer_events[index];
    }
    build_object_name(name, sizeof(name) / sizeof(name[0]), impl->bus_name,
                      "evt", (int)index);
    impl->peer_events[index] =
        OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name);
    return impl->peer_events[index];
}

static void impl_destroy(wcan_shm_impl_t *impl)
{
    uint32_t index;

    if (impl == NULL) {
        return;
    }
    for (index = 0; index < WCAN_SHM_MAX_NODES; ++index) {
        if (impl->peer_events[index] != NULL) {
            CloseHandle(impl->peer_events[index]);
        }
    }
    if (impl->alive_mutex != NULL) {
        (void)ReleaseMutex(impl->alive_mutex);
        CloseHandle(impl->alive_mutex);
    }
    if (impl->claimed) {
        claims_release(impl->bus_name, impl->node_index);
    }
    if (impl->timer_raised) {
        (void)timeEndPeriod(1);
    }
    if (impl->no_operations != NULL) {
        CloseHandle(impl->no_operations);
    }
    if (impl->own_event != NULL) {
        CloseHandle(impl->own_event);
    }
    if (impl->cancel_event != NULL) {
        CloseHandle(impl->cancel_event);
    }
    if (impl->bus != NULL) {
        UnmapViewOfFile(impl->bus);
    }
    if (impl->section != NULL) {
        CloseHandle(impl->section);
    }
    if (impl->bus_mutex != NULL) {
        CloseHandle(impl->bus_mutex);
    }
    free(impl);
}

/*
 * Claims a node slot. A slot is available when its "alive" mutex can be
 * acquired: either it was never held, or the owning process died and the
 * kernel reports abandonment. This avoids heartbeat timeouts, which would
 * evict a simulation parked on a breakpoint.
 */
static int claim_node_slot(wcan_shm_impl_t *impl)
{
    wchar_t name[256];
    uint32_t index;

    for (index = 0; index < WCAN_SHM_MAX_NODES; ++index) {
        HANDLE candidate;
        DWORD wait_result;

        /* Skip slots this process already owns; the mutex alone cannot tell
           us apart from a dead peer. */
        if (!claims_reserve(impl->bus_name, index)) {
            continue;
        }

        build_object_name(name, sizeof(name) / sizeof(name[0]), impl->bus_name,
                          "alive", (int)index);
        candidate = CreateMutexW(NULL, FALSE, name);
        if (candidate == NULL) {
            claims_release(impl->bus_name, index);
            continue;
        }
        wait_result = WaitForSingleObject(candidate, 0);
        if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED) {
            impl->alive_mutex = candidate;
            impl->node_index = index;
            impl->claimed = 1;
            return WCAN_OK;
        }
        CloseHandle(candidate);
        claims_release(impl->bus_name, index);
    }
    return WCAN_ERROR_TOO_MANY_CLIENTS;
}

int wcan_shm_open(wcan_shm_socket_t *socket, const char *bus_name,
                  uint32_t flags)
{
    wcan_shm_params_t params;

    memset(&params, 0, sizeof(params));
    params.flags = flags;
    return wcan_shm_open_ex(socket, bus_name, &params);
}

int wcan_shm_open_ex(wcan_shm_socket_t *socket, const char *bus_name,
                     const wcan_shm_params_t *params)
{
    wchar_t name[256];
    wcan_shm_impl_t *impl;
    wcan_shm_node_t *node;
    wcan_shm_params_t effective;
    int status;

    if (socket == NULL || bus_name == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    if (socket->impl != NULL) {
        return WCAN_ERROR_ALREADY_OPEN;
    }
    status = wcan_validate_bus_name(bus_name);
    if (status != WCAN_OK) {
        return status;
    }

    memset(&effective, 0, sizeof(effective));
    if (params != NULL) {
        effective = *params;
    }
    /* A bitrate of zero means "whatever the bus already runs at", so joining
       an existing bus never has to restate its configuration. */
    if (effective.bitrate != 0u &&
        (effective.bitrate < 10000u || effective.bitrate > 8000000u)) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    if (effective.max_lead_us == 0u) {
        effective.max_lead_us = WCAN_SHM_DEFAULT_LEAD_US;
    }

    impl = (wcan_shm_impl_t *)calloc(1, sizeof(*impl));
    if (impl == NULL) {
        return WCAN_ERROR_NO_MEMORY;
    }
    (void)snprintf(impl->bus_name, sizeof(impl->bus_name), "%s", bus_name);

    build_object_name(name, sizeof(name) / sizeof(name[0]), bus_name, "mtx",
                      -1);
    impl->bus_mutex = CreateMutexW(NULL, FALSE, name);
    if (impl->bus_mutex == NULL) {
        impl_destroy(impl);
        return WCAN_ERROR_IO;
    }

    build_object_name(name, sizeof(name) / sizeof(name[0]), bus_name, NULL, -1);
    impl->section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
                                       PAGE_READWRITE, 0,
                                       (DWORD)sizeof(wcan_shm_bus_t), name);
    if (impl->section == NULL) {
        impl_destroy(impl);
        return WCAN_ERROR_IO;
    }
    impl->bus = (wcan_shm_bus_t *)MapViewOfFile(impl->section,
                                                FILE_MAP_ALL_ACCESS, 0, 0,
                                                sizeof(wcan_shm_bus_t));
    if (impl->bus == NULL) {
        impl_destroy(impl);
        return WCAN_ERROR_IO;
    }

    impl->cancel_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (impl->cancel_event == NULL) {
        impl_destroy(impl);
        return WCAN_ERROR_IO;
    }

    impl->no_operations = CreateEventW(NULL, TRUE, TRUE, NULL);
    if (impl->no_operations == NULL) {
        impl_destroy(impl);
        return WCAN_ERROR_IO;
    }

    status = bus_lock(impl);
    if (status != WCAN_OK) {
        impl_destroy(impl);
        return status;
    }

    /* The section is zero filled on creation, so the first process through
       here initializes it. Publishing "initialized" last keeps joiners from
       observing a half-built header. */
    if (impl->bus->initialized != 1u || impl->bus->magic != WCAN_SHM_MAGIC) {
        memset(impl->bus, 0, sizeof(*impl->bus));
        impl->bus->magic = WCAN_SHM_MAGIC;
        impl->bus->version = WCAN_SHM_VERSION;
        impl->bus->bitrate = effective.bitrate != 0u ? effective.bitrate
                                                     : WCAN_SHM_DEFAULT_BITRATE;
        impl->bus->flags = effective.flags & ~WCAN_SHM_OPEN_ECHO;
        impl->bus->max_lead_us = effective.max_lead_us;
        impl->bus->qpc_frequency = (uint64_t)qpc_frequency();
        impl->bus->bus_free_at = (uint64_t)qpc_now();
        (void)snprintf(impl->bus->bus_name, sizeof(impl->bus->bus_name), "%s",
                       bus_name);
        impl->bus->initialized = 1u;
    } else if (impl->bus->version != WCAN_SHM_VERSION) {
        bus_unlock(impl);
        impl_destroy(impl);
        return WCAN_ERROR_PROTOCOL;
    } else if (effective.bitrate != 0u &&
               effective.bitrate != impl->bus->bitrate) {
        /* Joining at the wrong bitrate is a configuration error, not something
           to paper over: the node would misjudge every timing on the bus. */
        bus_unlock(impl);
        impl_destroy(impl);
        return WCAN_ERROR_PROTOCOL;
    }

    status = claim_node_slot(impl);
    if (status != WCAN_OK) {
        bus_unlock(impl);
        impl_destroy(impl);
        return status;
    }

    build_object_name(name, sizeof(name) / sizeof(name[0]), bus_name, "evt",
                      (int)impl->node_index);
    impl->own_event = CreateEventW(NULL, FALSE, FALSE, name);
    if (impl->own_event == NULL) {
        bus_unlock(impl);
        impl_destroy(impl);
        return WCAN_ERROR_IO;
    }
    (void)ResetEvent(impl->own_event);

    node = &impl->bus->nodes[impl->node_index];
    memset(node, 0, sizeof(*node));
    node->owner_pid = (uint32_t)GetCurrentProcessId();
    node->echo = (effective.flags & WCAN_SHM_OPEN_ECHO) != 0u ? 1u : 0u;
    node->active = 1u;

    /* A paced bus schedules on a sub-millisecond cadence, so raise the timer
       resolution rather than spin through every wait. */
    if ((impl->bus->flags & (WCAN_SHM_BUS_PACE_ADMISSION |
                             WCAN_SHM_BUS_PACE_DELIVERY)) != 0u) {
        if (timeBeginPeriod(1) == TIMERR_NOERROR) {
            impl->timer_raised = 1;
        }
    }

    bus_unlock(impl);

    socket->impl = impl;
    return WCAN_OK;
}

int wcan_shm_send(wcan_shm_socket_t *socket, const wcan_frame_t *frame)
{
    wcan_shm_impl_t *impl;
    wcan_shm_pending_t entry;
    uint32_t targets[WCAN_SHM_MAX_NODES];
    uint32_t target_count = 0;
    uint32_t index;
    int status;

    if (socket == NULL || frame == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    status = wcan_validate_frame(frame);
    if (status != WCAN_OK) {
        return status;
    }
    impl = (wcan_shm_impl_t *)socket->impl;
    if (impl == NULL || !operation_begin(impl)) {
        return WCAN_ERROR_CLOSED;
    }

    memset(&entry, 0, sizeof(entry));
    entry.slot.can_id = frame->can_id;
    entry.slot.dlc = frame->dlc;
    entry.slot.flags = frame->flags;
    if ((frame->flags & WCAN_FLAG_RTR) == 0u && frame->dlc != 0u) {
        memcpy(entry.slot.data, frame->data, frame->dlc);
    }
    entry.sender = impl->node_index;

    for (;;) {
        uint64_t now;
        uint64_t base;
        uint64_t projected;
        long long lead_ticks;
        long long frequency;
        int paced;

        status = bus_lock(impl);
        if (status != WCAN_OK) {
            operation_end(impl);
            return status;
        }
        now = (uint64_t)qpc_now();
        (void)schedule_pending(impl->bus, now);

        frequency = (long long)impl->bus->qpc_frequency;
        paced = (impl->bus->flags & WCAN_SHM_BUS_PACE_ADMISSION) != 0u;
        lead_ticks = (long long)(((uint64_t)impl->bus->max_lead_us *
                                  impl->bus->qpc_frequency) /
                                 1000000u);

        /*
         * Admission pacing: throttle on when the bus would actually finish
         * everything already offered, which is the committed clock plus the
         * airtime still sitting in the pending queue. Gating on bus_free_at
         * alone ignores queued work, so the queue just overflows.
         */
        base = impl->bus->bus_free_at > now ? impl->bus->bus_free_at : now;
        projected = base + ((impl->bus->queued_bits *
                             impl->bus->qpc_frequency) /
                            (uint64_t)impl->bus->bitrate);

        if (paced &&
            projected > (uint64_t)((long long)now + lead_ticks)) {
            const long long target = (long long)projected - lead_ticks;

            bus_unlock(impl);
            wait_until_tick(target, frequency, 0);
            continue;
        }

        if (impl->bus->pending_count >= WCAN_SHM_MAX_PENDING) {
            const long long target = (long long)impl->bus->bus_free_at;

            bus_unlock(impl);
            if (!paced) {
                operation_end(impl);
                return WCAN_ERROR_TIMEOUT;
            }
            wait_until_tick(target, frequency, 0);
            continue;
        }

        entry.slot.sequence = impl->bus->sequence++;
        entry.arrival = now;
        entry.bits = wcan_frame_bits(
            frame, (impl->bus->flags & WCAN_SHM_BUS_WORST_CASE_STUFFING) != 0u);
        impl->bus->pending[impl->bus->pending_count] = entry;
        impl->bus->pending_count++;
        impl->bus->queued_bits += entry.bits;

        (void)schedule_pending(impl->bus, now);

        for (index = 0; index < WCAN_SHM_MAX_NODES; ++index) {
            if (impl->bus->nodes[index].active != 1u) {
                continue;
            }
            if (index == impl->node_index &&
                impl->bus->nodes[index].echo == 0u) {
                continue;
            }
            targets[target_count++] = index;
        }
        bus_unlock(impl);
        break;
    }

    for (index = 0; index < target_count; ++index) {
        HANDLE event_handle = peer_event(impl, targets[index]);

        if (event_handle != NULL) {
            (void)SetEvent(event_handle);
        }
    }

    operation_end(impl);
    return WCAN_OK;
}

int wcan_shm_recv(wcan_shm_socket_t *socket, wcan_frame_t *frame)
{
    return wcan_shm_recv_timeout(socket, frame, WCAN_INFINITE);
}

int wcan_shm_recv_timeout(wcan_shm_socket_t *socket, wcan_frame_t *frame,
                          uint32_t timeout_ms)
{
    wcan_shm_impl_t *impl;
    ULONGLONG start = GetTickCount64();
    int result = WCAN_ERROR_IO;

    if (socket == NULL || frame == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    impl = (wcan_shm_impl_t *)socket->impl;
    if (impl == NULL || !operation_begin(impl)) {
        return WCAN_ERROR_CLOSED;
    }

    for (;;) {
        wcan_shm_slot_t slot;
        HANDLE wait_handles[3];
        DWORD wait_result;
        DWORD remaining;
        long long wake_target = 0;
        long long frequency;
        int precise_wake;
        uint64_t now;
        int popped = 0;
        int status = bus_lock(impl);

        if (status != WCAN_OK) {
            result = status;
            break;
        }

        now = (uint64_t)qpc_now();
        (void)schedule_pending(impl->bus, now);

        {
            wcan_shm_node_t *node = &impl->bus->nodes[impl->node_index];

            if (node->count > 0u) {
                const wcan_shm_slot_t *head = &node->ring[node->head];

                if ((impl->bus->flags & WCAN_SHM_BUS_PACE_DELIVERY) != 0u &&
                    head->bus_time > now) {
                    /* The frame exists but has not finished transmitting on
                       the virtual bus yet. */
                    wake_target = (long long)head->bus_time;
                } else {
                    popped = ring_pop(node, &slot);
                }
            }
            if (!popped && impl->bus->pending_count > 0u) {
                const long long commit_at = (long long)impl->bus->bus_free_at;

                if (wake_target == 0 || commit_at < wake_target) {
                    wake_target = commit_at;
                }
            }
        }
        frequency = (long long)impl->bus->qpc_frequency;
        precise_wake =
            (impl->bus->flags & WCAN_SHM_BUS_PACE_DELIVERY) != 0u ? 1 : 0;
        bus_unlock(impl);

        if (popped) {
            memset(frame, 0, sizeof(*frame));
            frame->can_id = slot.can_id;
            frame->dlc = slot.dlc;
            frame->flags = slot.flags;
            memcpy(frame->data, slot.data, WCAN_MAX_DATA);
            result = WCAN_OK;
            break;
        }

        if (timeout_ms == WCAN_INFINITE) {
            remaining = INFINITE;
        } else {
            const ULONGLONG elapsed = GetTickCount64() - start;

            if (elapsed >= timeout_ms) {
                result = WCAN_ERROR_TIMEOUT;
                break;
            }
            remaining = (DWORD)(timeout_ms - elapsed);
        }

        /*
         * Receivers double as the bus pump: with no daemon, whoever is waiting
         * must wake at the next scheduling instant to advance the clock.
         */
        if (wake_target != 0) {
            const long long delta = wake_target - qpc_now();
            const double milliseconds =
                delta > 0 ? ((double)delta * 1000.0) / (double)frequency : 0.0;

            if (milliseconds <= 2.0) {
                wait_until_tick(wake_target, frequency, precise_wake);
                continue;
            }
            if (remaining == INFINITE || (double)remaining > milliseconds) {
                remaining = (DWORD)(milliseconds - 1.0);
            }
        }

        /* Waiting on the closing event too means a concurrent close releases a
           blocked receiver promptly instead of after the full timeout. */
        wait_handles[0] = impl->own_event;
        wait_handles[1] = impl->cancel_event;
        wait_handles[2] = impl->no_operations;
        wait_result = WaitForMultipleObjects(3, wait_handles, FALSE, remaining);
        if (wait_result == WAIT_OBJECT_0) {
            continue;
        }
        if (wait_result == WAIT_OBJECT_0 + 1) {
            result = WCAN_ERROR_CLOSED;
            break;
        }
        if (wait_result == WAIT_OBJECT_0 + 2) {
            if (InterlockedCompareExchange(&impl->closing, 0, 0) != 0) {
                result = WCAN_ERROR_CLOSED;
                break;
            }
            continue;
        }
        if (wait_result == WAIT_TIMEOUT) {
            if (wake_target != 0) {
                continue;
            }
            result = WCAN_ERROR_TIMEOUT;
            break;
        }
        result = WCAN_ERROR_IO;
        break;
    }

    operation_end(impl);
    return result;
}

int wcan_shm_bus_stats(wcan_shm_socket_t *socket, wcan_shm_bus_stats_t *stats)
{
    wcan_shm_impl_t *impl;
    int status;

    if (socket == NULL || stats == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    impl = (wcan_shm_impl_t *)socket->impl;
    if (impl == NULL) {
        return WCAN_ERROR_CLOSED;
    }
    status = bus_lock(impl);
    if (status != WCAN_OK) {
        return status;
    }

    memset(stats, 0, sizeof(*stats));
    stats->frames = impl->bus->total_frames;
    stats->bits = impl->bus->total_bits;
    stats->bitrate = impl->bus->bitrate;
    stats->segment_bytes = (uint32_t)sizeof(wcan_shm_bus_t);
    stats->bus_seconds =
        (double)impl->bus->total_bits / (double)impl->bus->bitrate;
    if (impl->bus->total_frames > 0u) {
        const double frequency = (double)impl->bus->qpc_frequency;

        stats->elapsed_seconds =
            (double)((uint64_t)qpc_now() - impl->bus->first_frame) / frequency;
        if (stats->elapsed_seconds > 0.0) {
            stats->utilization = stats->bus_seconds / stats->elapsed_seconds;
        }
    }
    bus_unlock(impl);
    return WCAN_OK;
}

int wcan_shm_cancel(wcan_shm_socket_t *socket)
{
    wcan_shm_impl_t *impl;

    if (socket == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    impl = (wcan_shm_impl_t *)socket->impl;
    if (impl == NULL) {
        return WCAN_ERROR_CLOSED;
    }
    return SetEvent(impl->cancel_event) ? WCAN_OK : WCAN_ERROR_IO;
}

int wcan_shm_close(wcan_shm_socket_t *socket)
{
    wcan_shm_impl_t *impl;

    if (socket == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    impl = (wcan_shm_impl_t *)socket->impl;
    if (impl == NULL) {
        return WCAN_OK;
    }
    socket->impl = NULL;

    /* Stop new calls, wake anything blocked, then wait for in-flight calls to
       leave before the state they are using is freed. */
    InterlockedExchange(&impl->closing, 1);
    SetEvent(impl->cancel_event);
    SetEvent(impl->no_operations);
    while (InterlockedCompareExchange(&impl->active_operations, 0, 0) != 0) {
        Sleep(1);
    }

    if (bus_lock(impl) == WCAN_OK) {
        memset(&impl->bus->nodes[impl->node_index], 0,
               sizeof(impl->bus->nodes[impl->node_index]));
        bus_unlock(impl);
    }
    impl_destroy(impl);
    return WCAN_OK;
}

int wcan_shm_node_index(wcan_shm_socket_t *socket, uint32_t *out_index)
{
    wcan_shm_impl_t *impl;

    if (socket == NULL || out_index == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    impl = (wcan_shm_impl_t *)socket->impl;
    if (impl == NULL) {
        return WCAN_ERROR_CLOSED;
    }
    *out_index = impl->node_index;
    return WCAN_OK;
}

int wcan_shm_overruns(wcan_shm_socket_t *socket, uint32_t *out_overruns)
{
    wcan_shm_impl_t *impl;
    int status;

    if (socket == NULL || out_overruns == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    impl = (wcan_shm_impl_t *)socket->impl;
    if (impl == NULL) {
        return WCAN_ERROR_CLOSED;
    }
    status = bus_lock(impl);
    if (status != WCAN_OK) {
        return status;
    }
    *out_overruns = impl->bus->nodes[impl->node_index].overrun;
    bus_unlock(impl);
    return WCAN_OK;
}

int wcan_shm_node_count(wcan_shm_socket_t *socket, uint32_t *out_count)
{
    wcan_shm_impl_t *impl;
    uint32_t index;
    uint32_t total = 0;
    int status;

    if (socket == NULL || out_count == NULL) {
        return WCAN_ERROR_INVALID_ARGUMENT;
    }
    impl = (wcan_shm_impl_t *)socket->impl;
    if (impl == NULL) {
        return WCAN_ERROR_CLOSED;
    }
    status = bus_lock(impl);
    if (status != WCAN_OK) {
        return status;
    }
    for (index = 0; index < WCAN_SHM_MAX_NODES; ++index) {
        if (impl->bus->nodes[index].active == 1u) {
            total++;
        }
    }
    bus_unlock(impl);
    *out_count = total;
    return WCAN_OK;
}

/*
 * ctypes and similar runtimes cannot portably express wcan_shm_socket_t, so
 * the library allocates and frees it on their behalf and they only ever hold
 * an opaque pointer.
 */
wcan_shm_socket_t *wcan_shm_handle_alloc(void)
{
    return (wcan_shm_socket_t *)calloc(1, sizeof(wcan_shm_socket_t));
}

void wcan_shm_handle_free(wcan_shm_socket_t *socket)
{
    if (socket != NULL) {
        if (socket->impl != NULL) {
            (void)wcan_shm_close(socket);
        }
        free(socket);
    }
}

uint32_t wcan_shm_abi_version(void)
{
    return WCAN_SHM_VERSION;
}

uint32_t wcan_shm_segment_size(void)
{
    return (uint32_t)sizeof(wcan_shm_bus_t);
}
