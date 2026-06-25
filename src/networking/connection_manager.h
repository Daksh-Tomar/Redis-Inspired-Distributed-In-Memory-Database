#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// Connection Manager — Client Lifecycle Management
// ═══════════════════════════════════════════════════════════════════════════════

#include "networking/client.h"
#include "networking/event_loop.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace redisdb {

using CommandCallback = std::function<void(Client &client)>;
using DisconnectCallback = std::function<void(Client &client)>;

class ConnectionManager {
public:
  ConnectionManager(EventLoop &eventLoop, int maxClients = 10000);
  ~ConnectionManager() = default;

  Client *addClient(socket_t fd);

  Client *createOutboundClient(socket_t fd);

  void removeClient(int fd);

  Client *getClient(int fd);

  void setCommandCallback(CommandCallback cb) {
    commandCallback_ = std::move(cb);
  }
  void setDisconnectCallback(DisconnectCallback cb) {
    disconnectCallback_ = std::move(cb);
  }

  int closeIdleClients(int timeoutSeconds);

  size_t clientCount() const { return clients_.size(); }
  uint64_t totalConnectionsReceived() const {
    return totalConnectionsReceived_;
  }

  std::vector<int> getAllClientFds() const;

  void installWriteHandler(int fd);

private:
  void onReadable(int fd, int mask);
  void onWritable(int fd, int mask);

  void removeWriteHandler(int fd);

  EventLoop &eventLoop_;
  int maxClients_;
  std::unordered_map<int, std::unique_ptr<Client>> clients_;
  CommandCallback commandCallback_;
  DisconnectCallback disconnectCallback_;
  uint64_t totalConnectionsReceived_ = 0;
};

} // namespace redisdb
