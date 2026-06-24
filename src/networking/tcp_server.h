#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// TCP Server — Listening Socket Manager
// ═══════════════════════════════════════════════════════════════════════════════

#include "networking/connection_manager.h"
#include "networking/event_loop.h"
#include "networking/platform/socket_compat.h"


#include <string>

namespace redisdb {

class TcpServer {
public:
  TcpServer(EventLoop &eventLoop, ConnectionManager &connManager);
  ~TcpServer();

  bool listen(const std::string &bindAddr, int port);

  void close();

  socket_t listenFd() const { return listenFd_; }

private:
  void onAccept(int fd, int mask);

  EventLoop &eventLoop_;
  ConnectionManager &connManager_;
  socket_t listenFd_ = INVALID_SOCKET_VAL;

  static constexpr int TCP_BACKLOG = 511;
};

} // namespace redisdb
