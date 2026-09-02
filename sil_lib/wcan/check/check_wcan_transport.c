#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "wcan.h"

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition);                                               \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static void make_frame(wcan_frame_t *frame, uint32_t identifier)
{
    memset(frame, 0, sizeof(*frame));
    frame->can_id = identifier;
    frame->flags = WCAN_FLAG_EXTENDED;
    frame->dlc = 1;
    frame->data[0] = (uint8_t)identifier;
}

static int expect_frame(wcan_socket_t *socket, uint32_t identifier)
{
    wcan_frame_t frame;
    int status = wcan_recv_timeout(socket, &frame, 2000);

    if (status != WCAN_OK) {
        fprintf(stderr, "receive: %s\n", wcan_strerror(status));
        return 0;
    }
    if (frame.can_id != identifier || frame.dlc != 1 ||
        frame.data[0] != (uint8_t)identifier) {
        fprintf(stderr, "unexpected frame %08lX\n",
                (unsigned long)frame.can_id);
        return 0;
    }
    return 1;
}

static int expect_silence(wcan_socket_t *socket)
{
    wcan_frame_t frame;
    int status = wcan_recv_timeout(socket, &frame, 150);

    if (status != WCAN_ERROR_TIMEOUT) {
        fprintf(stderr, "expected silence, got %s\n", wcan_strerror(status));
        return 0;
    }
    return 1;
}

/* Opens a node and exits without closing it, leaving the slot's "alive"
   mutex abandoned exactly as a crashed simulation would. */
static int run_crash_child(const char *bus_name)
{
    wcan_socket_t socket = WCAN_SOCKET_INITIALIZER;
    uint32_t index = 0;

    if (wcan_open(&socket, bus_name, 0) != WCAN_OK) {
        return 1;
    }
    (void)wcan_node_index(&socket, &index);
    printf("crash child holds node %u\n", index);
    fflush(stdout);
    ExitProcess(0);
    return 0;
}

static int test_crash_reclaim(void)
{
    wcan_socket_t keeper = WCAN_SOCKET_INITIALIZER;
    wcan_socket_t reclaimed = WCAN_SOCKET_INITIALIZER;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    char executable[MAX_PATH];
    char command[MAX_PATH + 64];
    uint32_t count_before = 0;
    uint32_t count_stale = 0;
    uint32_t count_after = 0;
    uint32_t index = 0;

    CHECK(wcan_open(&keeper, "shmcrash", 0) == WCAN_OK);
    CHECK(wcan_node_count(&keeper, &count_before) == WCAN_OK);

    CHECK(GetModuleFileNameA(NULL, executable, MAX_PATH) != 0);
    (void)snprintf(command, sizeof(command), "\"%s\" --crash-child shmcrash",
                   executable);

    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    ZeroMemory(&process, sizeof(process));
    CHECK(CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL,
                         &startup, &process) != 0);
    CHECK(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

    /* The dead node is still marked active: nothing cleaned up after it. */
    CHECK(wcan_node_count(&keeper, &count_stale) == WCAN_OK);
    CHECK(count_stale == count_before + 1u);

    /* Opening again must reclaim the dead slot rather than consume a new one. */
    CHECK(wcan_open(&reclaimed, "shmcrash", 0) == WCAN_OK);
    CHECK(wcan_node_index(&reclaimed, &index) == WCAN_OK);
    CHECK(wcan_node_count(&reclaimed, &count_after) == WCAN_OK);
    CHECK(count_after == count_stale);

    CHECK(wcan_close(&reclaimed) == WCAN_OK);
    CHECK(wcan_close(&keeper) == WCAN_OK);
    printf("crash reclaim: dead node %u reused, node count stable at %u\n",
           index, count_after);
    return 0;
}

int main(int argc, char **argv)
{
    wcan_socket_t a = WCAN_SOCKET_INITIALIZER;
    wcan_socket_t b = WCAN_SOCKET_INITIALIZER;
    wcan_socket_t c = WCAN_SOCKET_INITIALIZER;
    wcan_socket_t echoed = WCAN_SOCKET_INITIALIZER;
    wcan_socket_t isolated = WCAN_SOCKET_INITIALIZER;
    wcan_frame_t frame;

    if (argc >= 3 && strcmp(argv[1], "--crash-child") == 0) {
        return run_crash_child(argv[2]);
    }

    CHECK(wcan_open(&a, "shm0", 0) == WCAN_OK);
    CHECK(wcan_open(&b, "shm0", 0) == WCAN_OK);
    CHECK(wcan_open(&c, "shm0", 0) == WCAN_OK);
    CHECK(wcan_open(&isolated, "shm1", 0) == WCAN_OK);

    /* Echo defaults to off, so the sender must not see its own frame. */
    make_frame(&frame, 0x18ff50e5u);
    CHECK(wcan_send(&a, &frame) == WCAN_OK);
    CHECK(expect_frame(&b, 0x18ff50e5u));
    CHECK(expect_frame(&c, 0x18ff50e5u));
    CHECK(expect_silence(&a));
    CHECK(expect_silence(&isolated));
    printf("default echo off, delivery, and bus isolation passed\n");

    /* Opting in restores the old broker behaviour for this socket only. */
    CHECK(wcan_open(&echoed, "shm0", WCAN_OPEN_ECHO) == WCAN_OK);
    make_frame(&frame, 0x123u);
    frame.flags = 0;
    CHECK(wcan_send(&echoed, &frame) == WCAN_OK);
    CHECK(expect_frame(&echoed, 0x123u));
    CHECK(expect_frame(&a, 0x123u));
    printf("opt-in echo passed\n");

    CHECK(wcan_close(&b) == WCAN_OK);
    make_frame(&frame, 0x124u);
    frame.flags = 0;
    CHECK(wcan_send(&c, &frame) == WCAN_OK);
    CHECK(expect_frame(&a, 0x124u));
    printf("delivery after peer close passed\n");

    CHECK(wcan_close(&echoed) == WCAN_OK);
    CHECK(wcan_close(&isolated) == WCAN_OK);
    CHECK(wcan_close(&c) == WCAN_OK);
    CHECK(wcan_close(&a) == WCAN_OK);

    if (test_crash_reclaim() != 0) {
        return 1;
    }

    puts("check_wcan_transport: all checks passed");
    return 0;
}
