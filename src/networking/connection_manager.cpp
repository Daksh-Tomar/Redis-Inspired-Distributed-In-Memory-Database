#include "networking/connection_manager.h"

#include <iostream>

namespace redisdb {

ConnectionManager::ConnectionManager(EventLoop &eventLoop, int maxClients)
    : eventLoop_(eventLoop), maxClients_(maxClients) {}

Client *ConnectionManager::addClient(socket_t fd) {

  if (static_cast<int>(clients_.size()) >= maxClients_) {
    std::cerr << "[ConnectionManager] Max clients reached (" << maxClients_
              << "), rejecting connection" << std::endl;

    const char *err = "-ERR max number of clients reached\r\n";
    send(fd, err, static_cast<int>(strlen(err)), 0);
    platform::closeSocket(fd);
    return nullptr;
  }

  if (!platform::setNonBlocking(fd)) {
    std::cerr << "[ConnectionManager] Failed to set non-blocking mode"
              << std::endl;
    platform::closeSocket(fd);
    return nullptr;
  }

  platform::setTcpNoDelay(fd);

  int fdInt = static_cast<int>(fd);
  auto client = std::make_unique<Client>(fd);
  Client *clientPtr = client.get();
  clients_[fdInt] = std::move(client);

  totalConnectionsReceived_++;

  eventLoop_.addFileEvent(fdInt, EVENT_READABLE,
                          [this](int fd, int mask) { onReadable(fd, mask); });

  std::cout << "[ConnectionManager] Client #" << clientPtr->id()
            << " connected (fd=" << fdInt << ", total=" << clients_.size()
            << ")" << std::endl;

  return clientPtr;
}

Client *ConnectionManager::createOutboundClient(socket_t fd) {
  int fdInt = static_cast<int>(fd);
  auto client = std::make_unique<Client>(fd);
  Client *clientPtr = client.get();
  clients_[fdInt] = std::move(client);

  totalConnectionsReceived_++;

  eventLoop_.addFileEvent(fdInt, EVENT_WRITABLE | EVENT_READABLE,
                          [this](int fd, int mask) {
                            if (mask & EVENT_READABLE) {
                              onReadable(fd, mask);
                            }
                            if (mask & EVENT_WRITABLE) {
                              onWritable(fd, mask);
                            }
                          });

  std::cout << "[ConnectionManager] Outbound client #" << clientPtr->id()
            << " connecting (fd=" << fdInt << ", total=" << clients_.size()
            << ")" << std::endl;

  return clientPtr;
}

void ConnectionManager::removeClient(int fd) {
  auto it = clients_.find(fd);
  if (it == clients_.end())
    return;

  if (disconnectCallback_) {
    disconnectCallback_(*it->second);
  }

  std::cout << "[ConnectionManager] Client #" << it->second->id()
            << " disconnected (fd=" << fd << ", total=" << (clients_.size() - 1)
            << ")" << std::endl;

  eventLoop_.removeFileEvent(fd, EVENT_READABLE | EVENT_WRITABLE);

  clients_.erase(it);
}

Client *ConnectionManager::getClient(int fd) {
  auto it = clients_.find(fd);
  return it != clients_.end() ? it->second.get() : nullptr;
}

void ConnectionManager::onReadable(int fd, int mask) {

  Client *client = getClient(fd);
  if (!client)
    return;

  int nread = client->readFromSocket();

  if (nread <= 0) {
    if (nread == 0) {

      removeClient(fd);
    }

    return;
  }

  if (commandCallback_) {
    commandCallback_(*client);
  }
  if (client->hasPendingOutput()) {
    installWriteHandler(fd);
  }
}

void ConnectionManager::onWritable(int fd, int mask) {

  Client *client = getClient(fd);
  if (!client)
    return;

  int nwritten = client->writeToSocket();

  if (nwritten == 0 && client->hasPendingOutput()) {

    removeClient(fd);
    return;
  }

  if (!client->hasPendingOutput()) {
    removeWriteHandler(fd);

    if (client->hasFlag(CLIENT_CLOSE_ASAP)) {
      removeClient(fd);
    }
  }
}

void ConnectionManager::installWriteHandler(int fd) {
  eventLoop_.addFileEvent(fd, EVENT_WRITABLE,
                          [this](int fd, int mask) { onWritable(fd, mask); });
}

void ConnectionManager::removeWriteHandler(int fd) {
  eventLoop_.removeFileEvent(fd, EVENT_WRITABLE);
}

int ConnectionManager::closeIdleClients(int timeoutSeconds) {
  if (timeoutSeconds <= 0)
    return 0;

  auto now = std::chrono::steady_clock::now();
  int closed = 0;

  std::vector<int> toClose;
  for (auto &[fd, client] : clients_) {
    if (client->hasFlag(CLIENT_SLAVE) || client->hasFlag(CLIENT_MASTER))
      continue;
    auto idle = std::chrono::duration_cast<std::chrono::seconds>(
        now - client->lastInteraction());
    if (idle.count() >= timeoutSeconds) {
      toClose.push_back(fd);
    }
  }

  for (int fd : toClose) {
    std::cout << "[ConnectionManager] Closing idle client (fd=" << fd << ")"
              << std::endl;
    removeClient(fd);
    closed++;
  }

  return closed;
}

std::vector<int> ConnectionManager::getAllClientFds() const {
  std::vector<int> fds;
  fds.reserve(clients_.size());
  for (auto &[fd, _] : clients_) {
    fds.push_back(fd);
  }
  return fds;
}

} // namespace redisdb
