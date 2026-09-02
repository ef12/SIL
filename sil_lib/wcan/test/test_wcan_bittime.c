#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcan_shm.h"

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition);                                               \
            return 1;                                                          \
        }                                                                      \
    } while (0)

#define ISOBUS_BITRATE 250000u

static double now_seconds(void)
{
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}

static double process_cpu_seconds(void)
{
    FILETIME creation;
    FILETIME exit;
    FILETIME kernel;
    FILETIME user;
    ULARGE_INTEGER kernel_value;
    ULARGE_INTEGER user_value;

    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel,
                         &user)) {
        return 0.0;
    }
    kernel_value.LowPart = kernel.dwLowDateTime;
    kernel_value.HighPart = kernel.dwHighDateTime;
    user_value.LowPart = user.dwLowDateTime;
    user_value.HighPart = user.dwHighDateTime;
    return (double)(kernel_value.QuadPart + user_value.QuadPart) / 1e7;
}

static void make_frame(wcan_frame_t *frame, uint32_t identifier, uint8_t dlc,
                       int extended)
{
    uint8_t index;

    memset(frame, 0, sizeof(*frame));
    frame->can_id = identifier;
    frame->flags = extended ? WCAN_FLAG_EXTENDED : 0;
    frame->dlc = dlc;
    for (index = 0; index < dlc; ++index) {
        frame->data[index] = (uint8_t)(0x5au ^ index);
    }
}

/* ---------------------------------------------------------------- */

static int test_airtime(void)
{
    wcan_frame_t extended8;
    wcan_frame_t standard0;
    wcan_frame_t zeros;
    uint32_t exact;
    uint32_t worst;

    make_frame(&extended8, 0x18ff50e5u, 8, 1);
    make_frame(&standard0, 0x123u, 0, 0);

    /* Extended, 8 data bytes: 64 + 8N = 128 bits before stuffing, a stuffed
       region of 118 bits allowing 29 stuff bits, plus 3 bits of interframe
       space. */
    worst = wcan_shm_frame_bits(&extended8, 1);
    exact = wcan_shm_frame_bits(&extended8, 0);
    CHECK(worst == 160u);
    CHECK(exact >= 131u);
    CHECK(exact <= worst);
    printf("  extended/8 bytes : exact %3u bits (%6.1f us)   worst %3u bits "
           "(%6.1f us)\n",
           exact, (double)exact * 1e6 / ISOBUS_BITRATE, worst,
           (double)worst * 1e6 / ISOBUS_BITRATE);

    /* Standard, no data: 44 bits before stuffing, region of 34. */
    worst = wcan_shm_frame_bits(&standard0, 1);
    exact = wcan_shm_frame_bits(&standard0, 0);
    CHECK(worst == 55u);
    CHECK(exact >= 47u);
    CHECK(exact <= worst);
    printf("  standard/0 bytes : exact %3u bits (%6.1f us)   worst %3u bits "
           "(%6.1f us)\n",
           exact, (double)exact * 1e6 / ISOBUS_BITRATE, worst,
           (double)worst * 1e6 / ISOBUS_BITRATE);

    /* Identifier 0 with no data drives 19 dominant bits before the CRC, which
       forces a stuff bit after every fifth: at least three of them. */
    make_frame(&zeros, 0x000u, 0, 0);
    exact = wcan_shm_frame_bits(&zeros, 0);
    CHECK(exact >= 47u + 3u);
    printf("  all-dominant id  : exact %3u bits, %u stuff bits inserted\n",
           exact, exact - 47u);

    puts("airtime model passed");
    return 0;
}

/* ---------------------------------------------------------------- */

static int expect_id(wcan_shm_socket_t *socket, uint32_t identifier)
{
    wcan_frame_t frame;
    int status = wcan_shm_recv_timeout(socket, &frame, 3000);

    if (status != WCAN_OK) {
        fprintf(stderr, "receive: %s\n", wcan_strerror(status));
        return 0;
    }
    if (frame.can_id != identifier) {
        fprintf(stderr, "expected id %03lX, got %03lX\n",
                (unsigned long)identifier, (unsigned long)frame.can_id);
        return 0;
    }
    return 1;
}

static int test_arbitration(void)
{
    wcan_shm_params_t params;
    wcan_shm_socket_t observer = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_socket_t a = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_socket_t b = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_socket_t c = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_frame_t frame;

    memset(&params, 0, sizeof(params));
    params.bitrate = ISOBUS_BITRATE;
    params.flags = WCAN_SHM_BUS_PACE_ADMISSION;

    CHECK(wcan_shm_open_ex(&observer, "bt.arb", &params) == WCAN_OK);
    CHECK(wcan_shm_open_ex(&a, "bt.arb", &params) == WCAN_OK);
    CHECK(wcan_shm_open_ex(&b, "bt.arb", &params) == WCAN_OK);
    CHECK(wcan_shm_open_ex(&c, "bt.arb", &params) == WCAN_OK);

    /* The first frame seizes an idle bus. The next two are offered while it is
       still transmitting, so they arbitrate against each other rather than
       being relayed in arrival order. */
    make_frame(&frame, 0x300u, 8, 0);
    CHECK(wcan_shm_send(&a, &frame) == WCAN_OK);
    make_frame(&frame, 0x200u, 8, 0);
    CHECK(wcan_shm_send(&c, &frame) == WCAN_OK);
    make_frame(&frame, 0x100u, 8, 0);
    CHECK(wcan_shm_send(&b, &frame) == WCAN_OK);

    CHECK(expect_id(&observer, 0x300u));
    CHECK(expect_id(&observer, 0x100u));
    CHECK(expect_id(&observer, 0x200u));
    printf("  priority order 0x300, 0x100, 0x200 (submitted 300, 200, 100)\n");

    /*
     * A top-priority primer seizes the bus so the following three frames are
     * all offered while it is still transmitting. Node A queues 0x600 and then
     * 0x050: per-sender FIFO must keep 0x600 first even though 0x050 outranks
     * it, which is what protects transport protocol sequences. Node B's 0x700
     * loses both arbitrations and goes last.
     */
    make_frame(&frame, 0x001u, 8, 0);
    CHECK(wcan_shm_send(&c, &frame) == WCAN_OK);
    make_frame(&frame, 0x700u, 8, 0);
    CHECK(wcan_shm_send(&b, &frame) == WCAN_OK);
    make_frame(&frame, 0x600u, 8, 0);
    CHECK(wcan_shm_send(&a, &frame) == WCAN_OK);
    make_frame(&frame, 0x050u, 8, 0);
    CHECK(wcan_shm_send(&a, &frame) == WCAN_OK);

    CHECK(expect_id(&observer, 0x001u));
    CHECK(expect_id(&observer, 0x600u));
    CHECK(expect_id(&observer, 0x050u));
    CHECK(expect_id(&observer, 0x700u));
    printf("  per-sender FIFO held: node A's 0x600 precedes its own 0x050\n");
    printf("  cross-sender priority held: 0x600 and 0x050 both beat 0x700\n");

    CHECK(wcan_shm_close(&c) == WCAN_OK);
    CHECK(wcan_shm_close(&b) == WCAN_OK);
    CHECK(wcan_shm_close(&a) == WCAN_OK);
    CHECK(wcan_shm_close(&observer) == WCAN_OK);
    puts("arbitration passed");
    return 0;
}

/* ---------------------------------------------------------------- */

typedef struct {
    wcan_shm_socket_t *socket;
    unsigned int target;
    volatile LONG received;
    HANDLE ready;
} consumer_t;

static DWORD WINAPI consumer_thread(void *argument)
{
    consumer_t *consumer = (consumer_t *)argument;

    SetEvent(consumer->ready);
    for (;;) {
        wcan_frame_t frame;

        if (wcan_shm_recv_timeout(consumer->socket, &frame, 2000) != WCAN_OK) {
            break;
        }
        if ((unsigned int)InterlockedIncrement(&consumer->received) >=
            consumer->target) {
            break;
        }
    }
    return 0;
}

static int measure_pacing(const char *bus, uint32_t flags, const char *label,
                          unsigned int frames)
{
    wcan_shm_params_t params;
    wcan_shm_socket_t producer = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_socket_t consumer_socket = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_bus_stats_t stats;
    consumer_t consumer;
    wcan_frame_t frame;
    HANDLE thread;
    double start;
    double send_done;
    double all_done;
    double cpu_start;
    double cpu_used;
    double theoretical;
    uint32_t bits;
    unsigned int index;

    memset(&params, 0, sizeof(params));
    params.bitrate = ISOBUS_BITRATE;
    params.flags = flags;

    CHECK(wcan_shm_open_ex(&producer, bus, &params) == WCAN_OK);
    CHECK(wcan_shm_open_ex(&consumer_socket, bus, &params) == WCAN_OK);

    memset(&consumer, 0, sizeof(consumer));
    consumer.socket = &consumer_socket;
    consumer.target = frames;
    consumer.ready = CreateEventW(NULL, TRUE, FALSE, NULL);

    make_frame(&frame, 0x18ff50e5u, 8, 1);
    bits = wcan_shm_frame_bits(&frame, 0);
    theoretical = (double)frames * (double)bits / (double)ISOBUS_BITRATE;

    thread = CreateThread(NULL, 0, consumer_thread, &consumer, 0, NULL);
    WaitForSingleObject(consumer.ready, 2000);

    cpu_start = process_cpu_seconds();
    start = now_seconds();
    for (index = 0; index < frames; ++index) {
        if (wcan_shm_send(&producer, &frame) != WCAN_OK) {
            break;
        }
    }
    send_done = now_seconds();
    WaitForSingleObject(thread, 20000);
    all_done = now_seconds();
    cpu_used = process_cpu_seconds() - cpu_start;
    CloseHandle(thread);

    CHECK(wcan_shm_bus_stats(&producer, &stats) == WCAN_OK);

    printf("  %-22s frames %u x %u bits\n", label, frames, bits);
    printf("      theoretical bus time %8.3f s   send loop %8.3f s   "
           "wall %8.3f s\n",
           theoretical, send_done - start, all_done - start);
    if ((flags & WCAN_SHM_BUS_PACE_ADMISSION) != 0u) {
        printf("      pacing error %+6.2f %%\n",
               100.0 * ((all_done - start) - theoretical) / theoretical);
    }
    printf("      delivered %ld / %u   rate %7.1f frames/s\n",
           (long)consumer.received, frames,
           (double)consumer.received / (all_done - start));
    printf("      bus utilization %5.1f %%   cpu %5.3f s (%4.1f %% of one "
           "core)\n",
           100.0 * stats.utilization, cpu_used,
           100.0 * cpu_used / (all_done - start));

    CHECK(wcan_shm_close(&consumer_socket) == WCAN_OK);
    CHECK(wcan_shm_close(&producer) == WCAN_OK);
    CloseHandle(consumer.ready);
    return 0;
}

/* ---------------------------------------------------------------- */

static int test_bitrate_config(void)
{
    wcan_shm_params_t params;
    wcan_shm_socket_t creator = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_socket_t inheritor = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_socket_t mismatched = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_bus_stats_t stats;

    memset(&params, 0, sizeof(params));
    params.bitrate = 9u; /* below the accepted range */
    CHECK(wcan_shm_open_ex(&creator, "bt.cfg", &params) ==
          WCAN_ERROR_INVALID_ARGUMENT);

    params.bitrate = 500000u;
    CHECK(wcan_shm_open_ex(&creator, "bt.cfg", &params) == WCAN_OK);

    /* Zero means inherit, so a joiner never has to restate the bus setup. */
    memset(&params, 0, sizeof(params));
    CHECK(wcan_shm_open_ex(&inheritor, "bt.cfg", &params) == WCAN_OK);
    CHECK(wcan_shm_bus_stats(&inheritor, &stats) == WCAN_OK);
    CHECK(stats.bitrate == 500000u);

    /* Joining at the wrong bitrate must fail loudly rather than silently
       adopting the bus and misjudging every timing on it. */
    params.bitrate = 250000u;
    CHECK(wcan_shm_open_ex(&mismatched, "bt.cfg", &params) ==
          WCAN_ERROR_PROTOCOL);

    CHECK(wcan_shm_close(&inheritor) == WCAN_OK);
    CHECK(wcan_shm_close(&creator) == WCAN_OK);
    printf("  inherit, validate and reject mismatched bitrates\n");
    puts("bitrate configuration passed");
    return 0;
}

static int measure_rate(uint32_t bitrate)
{
    wcan_shm_params_t params;
    wcan_shm_socket_t producer = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_socket_t consumer_socket = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_shm_bus_stats_t stats;
    consumer_t consumer;
    wcan_frame_t frame;
    HANDLE thread;
    char bus[64];
    double start;
    double all_done;
    double theoretical;
    uint32_t bits;
    unsigned int frames = bitrate / 250u; /* about half a second of traffic */
    unsigned int index;

    (void)snprintf(bus, sizeof(bus), "bt.r%lu", (unsigned long)bitrate);

    memset(&params, 0, sizeof(params));
    params.bitrate = bitrate;
    params.flags = WCAN_SHM_BUS_PACE_ADMISSION;

    CHECK(wcan_shm_open_ex(&producer, bus, &params) == WCAN_OK);
    CHECK(wcan_shm_open_ex(&consumer_socket, bus, &params) == WCAN_OK);

    memset(&consumer, 0, sizeof(consumer));
    consumer.socket = &consumer_socket;
    consumer.target = frames;
    consumer.ready = CreateEventW(NULL, TRUE, FALSE, NULL);

    make_frame(&frame, 0x18ff50e5u, 8, 1);
    bits = wcan_shm_frame_bits(&frame, 0);
    theoretical = (double)frames * (double)bits / (double)bitrate;

    thread = CreateThread(NULL, 0, consumer_thread, &consumer, 0, NULL);
    WaitForSingleObject(consumer.ready, 2000);

    start = now_seconds();
    for (index = 0; index < frames; ++index) {
        if (wcan_shm_send(&producer, &frame) != WCAN_OK) {
            break;
        }
    }
    WaitForSingleObject(thread, 20000);
    all_done = now_seconds();
    CloseHandle(thread);

    CHECK(wcan_shm_bus_stats(&producer, &stats) == WCAN_OK);

    printf("  %7lu bit/s  frame %6.1f us  %5u frames  theoretical %6.3f s  "
           "measured %6.3f s  error %+5.2f %%  util %5.1f %%\n",
           (unsigned long)bitrate, (double)bits * 1e6 / (double)bitrate, frames,
           theoretical, all_done - start,
           100.0 * ((all_done - start) - theoretical) / theoretical,
           100.0 * stats.utilization);

    CHECK((unsigned int)consumer.received == frames);
    CHECK(wcan_shm_close(&consumer_socket) == WCAN_OK);
    CHECK(wcan_shm_close(&producer) == WCAN_OK);
    CloseHandle(consumer.ready);
    return 0;
}

int main(void)
{
    puts("== bit-time model ==");
    if (test_airtime() != 0) {
        return 1;
    }

    puts("\n== arbitration ==");
    if (test_arbitration() != 0) {
        return 1;
    }

    puts("\n== pacing feasibility at 250 kbit/s ==");
    if (measure_pacing("bt.none", 0, "unpaced (reference)", 4000) != 0) {
        return 1;
    }
    if (measure_pacing("bt.adm", WCAN_SHM_BUS_PACE_ADMISSION, "admission pacing",
                       4000) != 0) {
        return 1;
    }
    if (measure_pacing("bt.del",
                       WCAN_SHM_BUS_PACE_ADMISSION | WCAN_SHM_BUS_PACE_DELIVERY,
                       "admission + delivery", 4000) != 0) {
        return 1;
    }

    puts("\n== bitrate configuration ==");
    if (test_bitrate_config() != 0) {
        return 1;
    }

    puts("\n== pacing across standard CAN bitrates ==");
    if (measure_rate(125000u) != 0) {
        return 1;
    }
    if (measure_rate(250000u) != 0) {
        return 1;
    }
    if (measure_rate(500000u) != 0) {
        return 1;
    }
    if (measure_rate(1000000u) != 0) {
        return 1;
    }

    puts("\nwcan_bittime: all checks passed");
    return 0;
}
