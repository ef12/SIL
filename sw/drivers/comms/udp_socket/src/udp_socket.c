/**
 * @file udp_socket.c
 * @brief UDP socket implementation for Windows, with fallback stubs for non-Windows builds.
 */

#include "udp_socket.h"

#include <winsock2.h>
#include <ws2tcpip.h>

static int g_wsa_started = 0;

/**
 * @brief Ensures Winsock is initialized for UDP socket operations.
 *
 * This function reference-counts startup calls so multiple UdpSocket instances
 * can share the same Winsock lifetime safely.
 *
 * @return true if Winsock is ready to use, otherwise false.
 */
static bool ensure_wsa_started(void)
{
  WSADATA wsa_data;

  if (g_wsa_started > 0)
  {
    g_wsa_started++;
    return true;
  }

  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
  {
    return false;
  }

  g_wsa_started = 1;
  return true;
}

/**
 * @brief Initializes a UDP socket and binds it to a local port.
 *
 * @param udp Pointer to the socket wrapper to initialize.
 * @param local_port Local UDP port to bind.
 * @return true on success, false on invalid input or socket/bind failure.
 */
bool udp_socket_init(UdpSocket *udp, uint16_t local_port)
{
  SOCKET sock = INVALID_SOCKET;
  struct sockaddr_in addr;

  if (udp == NULL)
  {
    return false;
  }

  udp->native_socket = (uintptr_t)INVALID_SOCKET;
  udp->initialized = false;

  if (!ensure_wsa_started())
  {
    return false;
  }

  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET)
  {
    if (g_wsa_started > 0)
    {
      g_wsa_started--;
      if (g_wsa_started == 0)
      {
        WSACleanup();
      }
    }
    return false;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(local_port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
  {
    closesocket(sock);
    if (g_wsa_started > 0)
    {
      g_wsa_started--;
      if (g_wsa_started == 0)
      {
        WSACleanup();
      }
    }
    return false;
  }

  udp->native_socket = (uintptr_t)sock;
  udp->initialized = true;
  return true;
}

/**
 * @brief Sends a UDP datagram to a remote IPv4 endpoint.
 *
 * @param udp Initialized UDP socket instance.
 * @param ip Remote IPv4 address string (for example, "127.0.0.1").
 * @param remote_port Remote UDP port.
 * @param data Pointer to payload bytes.
 * @param data_len Payload length in bytes.
 * @return true if all bytes are sent, otherwise false.
 */
bool udp_socket_send_to(const UdpSocket *udp, const char *ip, uint16_t remote_port,
                        const void *data, size_t data_len)
{
  struct sockaddr_in remote_addr;
  int sent;

  if (udp == NULL || ip == NULL || data == NULL || !udp->initialized)
  {
    return false;
  }

  if (data_len > (size_t)INT_MAX)
  {
    return false;
  }

  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(remote_port);
  if (inet_pton(AF_INET, ip, &remote_addr.sin_addr) != 1)
  {
    return false;
  }

  sent = sendto((SOCKET)udp->native_socket, (const char *)data, (int)data_len, 0,
                (const struct sockaddr *)&remote_addr, sizeof(remote_addr));
  return sent == (int)data_len;
}

/**
 * @brief Enables or disables non-blocking mode on the socket.
 *
 * @param udp Initialized UDP socket instance.
 * @param enabled true to enable non-blocking mode, false for blocking mode.
 * @return true on success, false on invalid input or platform call failure.
 */
bool udp_socket_set_non_blocking(UdpSocket *udp, bool enabled)
{
  u_long mode;

  if (udp == NULL || !udp->initialized)
  {
    return false;
  }

  mode = enabled ? 1UL : 0UL;
  return ioctlsocket((SOCKET)udp->native_socket, FIONBIO, &mode) == 0;
}

/**
 * @brief Sets receive timeout for blocking receive calls.
 *
 * @param udp Initialized UDP socket instance.
 * @param timeout_ms Timeout in milliseconds.
 * @return true on success, false on invalid input or platform call failure.
 */
bool udp_socket_set_receive_timeout(UdpSocket *udp, uint32_t timeout_ms)
{
  DWORD timeout;

  if (udp == NULL || !udp->initialized)
  {
    return false;
  }

  timeout = (DWORD)timeout_ms;
  return setsockopt((SOCKET)udp->native_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
                    sizeof(timeout))
         == 0;
}

/**
 * @brief Receives one UDP datagram and optionally returns sender metadata.
 *
 * @param udp Initialized UDP socket instance.
 * @param buffer Destination buffer for received payload.
 * @param buffer_len Size of buffer in bytes.
 * @param sender_ip Optional IPv4 string output buffer.
 * @param sender_ip_len Size of sender_ip buffer.
 * @param sender_port Optional sender port output.
 * @return Number of bytes received on success, or -1 on failure.
 */
int udp_socket_receive_from(UdpSocket *udp, void *buffer, size_t buffer_len, char *sender_ip,
                            size_t sender_ip_len, uint16_t *sender_port)
{
  struct sockaddr_in from_addr;
  int from_len = sizeof(from_addr);
  int received;

  if (udp == NULL || buffer == NULL || !udp->initialized)
  {
    return -1;
  }

  if (buffer_len > (size_t)INT_MAX)
  {
    return -1;
  }

  received = recvfrom((SOCKET)udp->native_socket, (char *)buffer, (int)buffer_len, 0,
                      (struct sockaddr *)&from_addr, &from_len);

  if (received == SOCKET_ERROR)
  {
    return -1;
  }

  if (sender_ip != NULL && sender_ip_len > 0)
  {
    if (inet_ntop(AF_INET, &from_addr.sin_addr, sender_ip, (DWORD)sender_ip_len) == NULL)
    {
      sender_ip[0] = '\0';
    }
  }

  if (sender_port != NULL)
  {
    *sender_port = ntohs(from_addr.sin_port);
  }

  return received;
}

/**
 * @brief Closes the UDP socket and releases associated resources.
 *
 * @param udp Socket wrapper to close.
 */
void udp_socket_close(UdpSocket *udp)
{
  if (udp == NULL || !udp->initialized)
  {
    return;
  }

  closesocket((SOCKET)udp->native_socket);
  udp->native_socket = (uintptr_t)INVALID_SOCKET;
  udp->initialized = false;

  if (g_wsa_started > 0)
  {
    g_wsa_started--;
    if (g_wsa_started == 0)
    {
      WSACleanup();
    }
  }
}