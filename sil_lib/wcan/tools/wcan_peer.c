/*
 * A minimal WCAN node, used to demonstrate interoperation between processes
 * of different bitness and different languages.
 *
 *   wcan_peer <bus> send <id> <hex-bytes> [count]
 *   wcan_peer <bus> recv <count> [timeout_ms]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcan_shm.h"

static int parse_hex(const char *text, uint8_t *data, uint8_t *length)
{
    size_t index;
    size_t count = strlen(text);

    if ((count % 2u) != 0u || count > (WCAN_MAX_DATA * 2u)) {
        return 0;
    }
    for (index = 0; index < count; index += 2u) {
        unsigned int value;

        if (sscanf(&text[index], "%2x", &value) != 1) {
            return 0;
        }
        data[index / 2u] = (uint8_t)value;
    }
    *length = (uint8_t)(count / 2u);
    return 1;
}

static int do_send(const char *bus, const char *id_text, const char *hex,
                   unsigned int count)
{
    wcan_shm_socket_t socket = WCAN_SHM_SOCKET_INITIALIZER;
    wcan_frame_t frame;
    unsigned int index;
    int status;

    memset(&frame, 0, sizeof(frame));
    frame.can_id = (uint32_t)strtoul(id_text, NULL, 16);
    frame.flags = frame.can_id > 0x7ffu ? WCAN_FLAG_EXTENDED : 0u;
    if (!parse_hex(hex, frame.data, &frame.dlc)) {
        fprintf(stderr, "invalid hex payload: %s\n", hex);
        return 2;
    }

    status = wcan_shm_open(&socket, bus, 0);
    if (status != WCAN_OK) {
        fprintf(stderr, "open %s: %s\n", bus, wcan_strerror(status));
        return 1;
    }
    for (index = 0; index < count; ++index) {
        status = wcan_shm_send(&socket, &frame);
        if (status != WCAN_OK) {
            fprintf(stderr, "send: %s\n", wcan_strerror(status));
            (void)wcan_shm_close(&socket);
            return 1;
        }
        printf("TX %08lX [%u]\n", (unsigned long)frame.can_id,
               (unsigned int)frame.dlc);
    }
    fflush(stdout);
    (void)wcan_shm_close(&socket);
    return 0;
}

static int do_recv(const char *bus, unsigned int count, uint32_t timeout_ms)
{
    wcan_shm_socket_t socket = WCAN_SHM_SOCKET_INITIALIZER;
    unsigned int received = 0;
    int status = wcan_shm_open(&socket, bus, 0);

    if (status != WCAN_OK) {
        fprintf(stderr, "open %s: %s\n", bus, wcan_strerror(status));
        return 1;
    }
    printf("listening on %s\n", bus);
    fflush(stdout);

    while (received < count) {
        wcan_frame_t frame;
        unsigned int index;

        status = wcan_shm_recv_timeout(&socket, &frame, timeout_ms);
        if (status != WCAN_OK) {
            fprintf(stderr, "recv: %s\n", wcan_strerror(status));
            break;
        }
        printf("RX %08lX [%u]", (unsigned long)frame.can_id,
               (unsigned int)frame.dlc);
        for (index = 0; index < frame.dlc; ++index) {
            printf(" %02X", frame.data[index]);
        }
        printf("\n");
        fflush(stdout);
        received++;
    }

    (void)wcan_shm_close(&socket);
    return received == count ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[2], "send") == 0 && argc >= 5) {
        const unsigned int count =
            argc > 5 ? (unsigned int)strtoul(argv[5], NULL, 10) : 1u;

        return do_send(argv[1], argv[3], argv[4], count);
    }
    if (argc >= 4 && strcmp(argv[2], "recv") == 0) {
        const uint32_t timeout =
            argc > 4 ? (uint32_t)strtoul(argv[4], NULL, 10) : 5000u;

        return do_recv(argv[1], (unsigned int)strtoul(argv[3], NULL, 10),
                       timeout);
    }
    fprintf(stderr,
            "Usage:\n"
            "  %s <bus> send <hex-id> <hex-bytes> [count]\n"
            "  %s <bus> recv <count> [timeout_ms]\n",
            argv[0], argv[0]);
    return 2;
}
