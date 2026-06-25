#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// Socket Compatibility Layer
// ═══════════════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <string>

#ifdef REDIS_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

using socket_t = SOCKET;
constexpr socket_t INVALID_SOCKET_VAL = INVALID_SOCKET;

#else

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef REDIS_PLATFORM_LINUX
#include <sys/epoll.h>
#endif

using socket_t = int;
constexpr socket_t INVALID_SOCKET_VAL = -1;
#endif

namespace redisdb {
namespace platform {

inline bool initializeSockets() {
#ifdef REDIS_PLATFORM_WINDOWS
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  return result == 0;
#else

  signal(SIGPIPE, SIG_IGN);
  return true;
#endif
}

inline void cleanupSockets() {
#ifdef REDIS_PLATFORM_WINDOWS
  WSACleanup();
#endif
}

// Socket op.

inline int closeSocket(socket_t fd) {
#ifdef REDIS_PLATFORM_WINDOWS
  return closesocket(fd);
#else
  return close(fd);
#endif
}

inline bool setNonBlocking(socket_t fd) {
#ifdef REDIS_PLATFORM_WINDOWS
  u_long mode = 1;
  return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

inline bool setReuseAddr(socket_t fd) {
  int optval = 1;
  return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                    reinterpret_cast<const char *>(&optval),
                    sizeof(optval)) == 0;
}

inline bool setTcpNoDelay(socket_t fd) {
  int optval = 1;
  return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                    reinterpret_cast<const char *>(&optval),
                    sizeof(optval)) == 0;
}

inline bool wouldBlock() {
#ifdef REDIS_PLATFORM_WINDOWS
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

inline int getLastError() {
#ifdef REDIS_PLATFORM_WINDOWS
  return WSAGetLastError();
#else
  return errno;
#endif
}

inline std::string getErrorString(int err) {
#ifdef REDIS_PLATFORM_WINDOWS
  char buf[256] = {0};
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                 sizeof(buf), nullptr);
  return std::string(buf);
#else
  return std::string(strerror(err));
#endif
}

inline socket_t connectAsync(const std::string &host, int port) {
  socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET_VAL)
    return INVALID_SOCKET_VAL;

  setNonBlocking(s);
  setTcpNoDelay(s);

  struct sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(static_cast<uint16_t>(port));
  inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);

  int result = ::connect(s, reinterpret_cast<struct sockaddr *>(&serverAddr),
                         sizeof(serverAddr));

  if (result != 0) {
    int err = getLastError();
#ifdef REDIS_PLATFORM_WINDOWS
    if (err != WSAEWOULDBLOCK) {
#else
    if (err != EINPROGRESS && err != EWOULDBLOCK && err != EAGAIN) {
#endif
      closeSocket(s);
      return INVALID_SOCKET_VAL;
    }
  }
  return s;
}

} // namespace platform
} // namespace redisdb
