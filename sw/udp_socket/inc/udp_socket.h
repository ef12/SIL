#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uintptr_t native_socket;
  bool initialized;
} UdpSocket;

bool udp_socket_init(UdpSocket *udp, uint16_t local_port);

bool udp_socket_send_to(const UdpSocket *udp, const char *ip,
                        uint16_t remote_port, const void *data,
                        size_t data_len);

int udp_socket_receive_from(UdpSocket *udp, void *buffer, size_t buffer_len,
                            char *sender_ip, size_t sender_ip_len,
                            uint16_t *sender_port);

void udp_socket_close(UdpSocket *udp);

#ifdef __cplusplus
}
#endif

#endif