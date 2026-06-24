#include "networking/tcp_server.h"

#include <cstring>
#include <iostream>

namespace redisdb {

TcpServer::TcpServer(EventLoop &eventLoop, ConnectionManager &connManager)
    : eventLoop_(eventLoop), connManager_(connManager) {}

TcpServer::~TcpServer() { close(); }

bool TcpServer::listen(const std::string &bindAddr, int port) {

  listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd_ == INVALID_SOCKET_VAL) {
    std::cerr << "[TcpServer] Failed to create socket: "
              << platform::getErrorString(platform::getLastError())
              << std::endl;
    return false;
  }

  if (!platform::setReuseAddr(listenFd_)) {
    std::cerr << "[TcpServer] Failed to set SO_REUSEADDR" << std::endl;
  }

  if (!platform::setNonBlocking(listenFd_)) {
    std::cerr << "[TcpServer] Failed to set non-blocking mode" << std::endl;
    close();
    return false;
  }

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (bindAddr == "0.0.0.0" || bindAddr.empty()) {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    if (inet_pton(AF_INET, bindAddr.c_str(), &addr.sin_addr) <= 0) {
      std::cerr << "[TcpServer] Invalid bind address: " << bindAddr
                << std::endl;
      close();
      return false;
    }
  }

  if (bind(listenFd_, reinterpret_cast<struct sockaddr *>(&addr),
           sizeof(addr)) != 0) {
    std::cerr << "[TcpServer] Failed to bind to " << bindAddr << ":" << port
              << " — " << platform::getErrorString(platform::getLastError())
              << std::endl;
    close();
    return false;
  }

  if (::listen(listenFd_, TCP_BACKLOG) != 0) {
    std::cerr << "[TcpServer] Failed to listen: "
              << platform::getErrorString(platform::getLastError())
              << std::endl;
    close();
    return false;
  }

  int fdInt = static_cast<int>(listenFd_);
  eventLoop_.addFileEvent(fdInt, EVENT_READABLE,
                          [this](int fd, int mask) { onAccept(fd, mask); });

  std::cout << "[TcpServer] Listening on " << bindAddr << ":" << port
            << " (fd=" << fdInt << ", backlog=" << TCP_BACKLOG << ")"
            << std::endl;

  return true;
}

void TcpServer::close() {
  if (listenFd_ != INVALID_SOCKET_VAL) {
    eventLoop_.removeFileEvent(static_cast<int>(listenFd_), EVENT_READABLE);
    platform::closeSocket(listenFd_);
    listenFd_ = INVALID_SOCKET_VAL;
    std::cout << "[TcpServer] Listening socket closed" << std::endl;
  }
}

void TcpServer::onAccept(int fd, int mask) {

  constexpr int MAX_ACCEPTS_PER_CALL = 1000;

  for (int i = 0; i < MAX_ACCEPTS_PER_CALL; i++) {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    socket_t clientFd = accept(
        listenFd_, reinterpret_cast<struct sockaddr *>(&clientAddr), &addrLen);

    if (clientFd == INVALID_SOCKET_VAL) {
      if (platform::wouldBlock()) {

        break;
      }
      std::cerr << "[TcpServer] Accept error: "
                << platform::getErrorString(platform::getLastError())
                << std::endl;
      break;
    }

    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
    int clientPort = ntohs(clientAddr.sin_port);

    std::cout << "[TcpServer] Accepted connection from " << clientIp << ":"
              << clientPort << std::endl;

    connManager_.addClient(clientFd);
  }
}

} // namespace redisdb
