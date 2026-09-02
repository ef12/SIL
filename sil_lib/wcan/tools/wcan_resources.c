#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>

#include <stdio.h>
#include <string.h>

#include "wcan_shm.h"

#define MAX_SOCKETS 16u

typedef struct {
    SIZE_T private_bytes;
    SIZE_T working_set;
    DWORD handles;
} snapshot_t;

static void take_snapshot(snapshot_t *snapshot)
{
    PROCESS_MEMORY_COUNTERS_EX counters;

    memset(snapshot, 0, sizeof(*snapshot));
    memset(&counters, 0, sizeof(counters));
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             (PROCESS_MEMORY_COUNTERS *)&counters,
                             sizeof(counters))) {
        snapshot->private_bytes = counters.PrivateUsage;
        snapshot->working_set = counters.WorkingSetSize;
    }
    (void)GetProcessHandleCount(GetCurrentProcess(), &snapshot->handles);
}

static void report(const char *label, const snapshot_t *before,
                   const snapshot_t *after, unsigned int sockets)
{
    const double private_delta =
        (double)after->private_bytes - (double)before->private_bytes;
    const double working_delta =
        (double)after->working_set - (double)before->working_set;
    const long handle_delta = (long)after->handles - (long)before->handles;

    printf("  %-28s private %8.1f KB  workingset %8.1f KB  handles %3ld"
           "   (%4.1f KB + %2.1f handles per socket)\n",
           label, private_delta / 1024.0, working_delta / 1024.0, handle_delta,
           private_delta / 1024.0 / (double)sockets,
           (double)handle_delta / (double)sockets);
}

static int open_group(wcan_shm_socket_t *sockets, unsigned int count,
                      int distinct_buses)
{
    unsigned int index;

    for (index = 0; index < count; ++index) {
        char bus[64];

        if (distinct_buses) {
            (void)snprintf(bus, sizeof(bus), "res.bus%u", index);
        } else {
            (void)snprintf(bus, sizeof(bus), "res.shared");
        }
        if (wcan_shm_open(&sockets[index], bus, 0) != WCAN_OK) {
            fprintf(stderr, "open failed at %u\n", index);
            return 0;
        }
    }
    return 1;
}

static void close_group(wcan_shm_socket_t *sockets, unsigned int count)
{
    unsigned int index;

    for (index = 0; index < count; ++index) {
        (void)wcan_shm_close(&sockets[index]);
    }
}

int main(void)
{
    wcan_shm_socket_t sockets[MAX_SOCKETS];
    wcan_shm_socket_t probe = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_bus_stats_t stats;
    snapshot_t before;
    snapshot_t after;

    memset(sockets, 0, sizeof(sockets));

    if (wcan_shm_open(&probe, "res.probe", 0) != WCAN_OK ||
        wcan_shm_bus_stats(&probe, &stats) != WCAN_OK) {
        fprintf(stderr, "probe open failed\n");
        return 1;
    }
    printf("shared segment per bus: %u bytes (%.0f KB)\n", stats.segment_bytes,
           (double)stats.segment_bytes / 1024.0);
    printf("  capacity: 64 nodes, 128 frame receive ring each, 128 pending\n");
    printf("  kernel objects per bus: 1 section, 1 bus mutex, and one mutex\n");
    printf("                          plus one event per occupied node slot\n");
    printf("  background threads created by the library: 0\n\n");
    (void)wcan_shm_close(&probe);

    puts("per-process cost (deltas measured around the calls)");

    take_snapshot(&before);
    if (!open_group(sockets, 1u, 1)) {
        return 1;
    }
    take_snapshot(&after);
    report("1 socket, 1 bus", &before, &after, 1u);
    close_group(sockets, 1u);

    take_snapshot(&before);
    if (!open_group(sockets, 4u, 1)) {
        return 1;
    }
    take_snapshot(&after);
    report("4 sockets, 4 buses", &before, &after, 4u);
    close_group(sockets, 4u);

    take_snapshot(&before);
    if (!open_group(sockets, MAX_SOCKETS, 0)) {
        return 1;
    }
    take_snapshot(&after);
    report("16 sockets, 1 shared bus", &before, &after, MAX_SOCKETS);
    close_group(sockets, MAX_SOCKETS);

    /* Handles to peer notification events are opened lazily on first
       transmission, so an idle socket understates the steady-state count. */
    take_snapshot(&before);
    if (!open_group(sockets, 4u, 0)) {
        return 1;
    }
    {
        wcan_frame_t frame;
        unsigned int index;

        memset(&frame, 0, sizeof(frame));
        frame.can_id = 0x18ff50e5u;
        frame.flags = WCAN_FLAG_EXTENDED;
        frame.dlc = 8;
        for (index = 0; index < 4u; ++index) {
            (void)wcan_shm_send(&sockets[index], &frame);
        }
    }
    take_snapshot(&after);
    report("4 sockets after traffic", &before, &after, 4u);
    close_group(sockets, 4u);

    return 0;
}
