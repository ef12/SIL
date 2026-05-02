#include "udp_socket.h"

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

static int g_wsa_started = 0;

static bool ensure_wsa_started(void) {
  WSADATA wsa_data;

  if (g_wsa_started > 0) {
    g_wsa_started++;
    return true;
  }

  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    return false;
  }

  g_wsa_started = 1;
  return true;
}

bool udp_socket_init(UdpSocket *udp, uint16_t local_port) {
  SOCKET sock = INVALID_SOCKET;
  struct sockaddr_in addr;

  if (udp == NULL) {
    return false;
  }

  udp->native_socket = (uintptr_t)INVALID_SOCKET;
  udp->initialized = false;

  if (!ensure_wsa_started()) {
    return false;
  }

  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    if (g_wsa_started > 0) {
      g_wsa_started--;
      if (g_wsa_started == 0) {
        WSACleanup();
      }
    }
    return false;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(local_port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    closesocket(sock);
    if (g_wsa_started > 0) {
      g_wsa_started--;
      if (g_wsa_started == 0) {
        WSACleanup();
      }
    }
    return false;
  }

  udp->native_socket = (uintptr_t)sock;
  udp->initialized = true;
  return true;
}

bool udp_socket_send_to(const UdpSocket *udp, const char *ip,
                        uint16_t remote_port, const void *data,
                        size_t data_len) {
  struct sockaddr_in remote_addr;
  int sent;

  if (udp == NULL || ip == NULL || data == NULL || !udp->initialized) {
    return false;
  }

  if (data_len > (size_t)INT_MAX) {
    return false;
  }

  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(remote_port);
  if (inet_pton(AF_INET, ip, &remote_addr.sin_addr) != 1) {
    return false;
  }

  sent = sendto((SOCKET)udp->native_socket, (const char *)data, (int)data_len,
                0, (const struct sockaddr *)&remote_addr, sizeof(remote_addr));
  return sent == (int)data_len;
}

int udp_socket_receive_from(UdpSocket *udp, void *buffer, size_t buffer_len,
                            char *sender_ip, size_t sender_ip_len,
                            uint16_t *sender_port) {
  struct sockaddr_in from_addr;
  int from_len = sizeof(from_addr);
  int received;

  if (udp == NULL || buffer == NULL || !udp->initialized) {
    return -1;
  }

  if (buffer_len > (size_t)INT_MAX) {
    return -1;
  }

  received =
      recvfrom((SOCKET)udp->native_socket, (char *)buffer, (int)buffer_len, 0,
               (struct sockaddr *)&from_addr, &from_len);

  if (received == SOCKET_ERROR) {
    return -1;
  }

  if (sender_ip != NULL && sender_ip_len > 0) {
    if (inet_ntop(AF_INET, &from_addr.sin_addr, sender_ip,
                  (DWORD)sender_ip_len) == NULL) {
      sender_ip[0] = '\0';
    }
  }

  if (sender_port != NULL) {
    *sender_port = ntohs(from_addr.sin_port);
  }

  return received;
}

void udp_socket_close(UdpSocket *udp) {
  if (udp == NULL || !udp->initialized) {
    return;
  }

  closesocket((SOCKET)udp->native_socket);
  udp->native_socket = (uintptr_t)INVALID_SOCKET;
  udp->initialized = false;

  if (g_wsa_started > 0) {
    g_wsa_started--;
    if (g_wsa_started == 0) {
      WSACleanup();
    }
  }
}

#else

bool udp_socket_init(UdpSocket *udp, uint16_t local_port) {
  (void)udp;
  (void)local_port;
  return false;
}

bool udp_socket_send_to(const UdpSocket *udp, const char *ip,
                        uint16_t remote_port, const void *data,
                        size_t data_len) {
  (void)udp;
  (void)ip;
  (void)remote_port;
  (void)data;
  (void)data_len;
  return false;
}

int udp_socket_receive_from(UdpSocket *udp, void *buffer, size_t buffer_len,
                            char *sender_ip, size_t sender_ip_len,
                            uint16_t *sender_port) {
  (void)udp;
  (void)buffer;
  (void)buffer_len;
  (void)sender_ip;
  (void)sender_ip_len;
  (void)sender_port;
  return -1;
}

void udp_socket_close(UdpSocket *udp) { (void)udp; }

#endif