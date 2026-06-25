#pragma once

#include "commands/command_registry.h"
#include "networking/connection_manager.h"
#include "networking/event_loop.h"
#include "networking/tcp_server.h"
#include "protocol/resp_parser.h"
#include "server/config.h"
#include "storage/db.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace redisdb {

class Server {
public:
  explicit Server(ServerConfig config = ServerConfig{});
  ~Server();

  bool initialize();
  void run();
  void shutdown();

  Database &getDatabase(int index);
  int databaseCount() const { return static_cast<int>(databases_.size()); }

  CommandRegistry &commandRegistry() { return commandRegistry_; }

  void evictKeysIfNeeded();

  bool save(const std::string &filename = "dump.rdb");
  bool bgsave(const std::string &filename = "dump.rdb");
  bool bgrewriteaof();
  bool isBgsaveInProgress() const { return bgsaveInProgress_; }
  bool isBgrewriteaofInProgress() const { return bgrewriteaofInProgress_; }
  int64_t lastSaveTime() const { return lastSaveTime_; }

  void propagateToAof(const std::vector<RespValue> &args);
  void flushAof(bool force = false);

  ServerConfig &config() { return config_; }
  int port() const { return config_.port; }
  int64_t uptimeSeconds() const;
  size_t connectedClients() const;
  uint64_t totalConnectionsReceived() const;
  uint64_t totalCommandsProcessed() const { return totalCommandsProcessed_; }
  void incrementCommandsProcessed() { totalCommandsProcessed_++; }
  int64_t dirty() const {
    int64_t d = 0;
    for (const auto &db : databases_) d += db.dirty();
    return d;
  }
  uint64_t keyspaceHits() const {
    uint64_t h = 0;
    for (const auto &db : databases_) h += db.hits();
    return h;
  }
  uint64_t keyspaceMisses() const {
    uint64_t m = 0;
    for (const auto &db : databases_) m += db.misses();
    return m;
  }
  const std::vector<Client *> &replicas() const { return replicas_; }

  void subscribeChannel(Client &client, const std::string &channel);
  void unsubscribeChannel(Client &client, const std::string &channel);
  int publishMessage(const std::string &channel, const std::string &message);

  void psubscribePattern(Client &client, const std::string &pattern);
  void punsubscribePattern(Client &client, const std::string &pattern);

  void blockClient(Client &client, const std::vector<std::string> &keys,
                   int64_t timeout, const RespValue &cmd,
                   const std::string &target = "");
  void unblockClient(Client &client);
  void handleClientsBlockedOnKey(const std::string &key);

  void setReplicationMaster(const std::string &host, int port);
  void addReplica(Client *client);
  void removeReplica(Client *client);
  int getReplicaCount() const { return static_cast<int>(replicas_.size()); }
  std::string getReplId() const { return replId_; }
  int64_t getReplOffset() const { return masterReplOffset_; }
  void generateReplId();
  void disconnectFromMaster();

private:
  void processClientInput(Client &client);
  void closeClient(Client &client);

public:
  int serverCron();

private:
  void acceptTcpHandler(int fd, int mask);
  void executeCommand(Client &client, const RespValue &command);

  ServerConfig config_;
  std::unique_ptr<EventLoop> eventLoop_;
  std::unique_ptr<TcpServer> tcpServer_;
  std::unique_ptr<ConnectionManager> connManager_;
  CommandRegistry commandRegistry_;
  std::vector<Database> databases_;
  RespParser parser_;

  std::chrono::steady_clock::time_point startTime_;
  std::atomic<uint64_t> totalCommandsProcessed_{0};

  std::atomic<bool> bgsaveInProgress_{false};
  std::atomic<bool> bgrewriteaofInProgress_{false};
  std::atomic<int64_t> lastSaveTime_{0};

  std::string aofBuffer_;
  std::ofstream aofStream_;
  int64_t lastAofFsyncTime_ = 0;

  std::unordered_map<std::string, std::vector<Client *>> pubsubChannels_;
  std::vector<std::pair<std::string, Client *>> pubsubPatterns_;

  std::unordered_map<std::string, std::vector<Client *>> blockedClients_;

  enum ReplState {
    REPL_STATE_NONE = 0,
    REPL_STATE_CONNECT,
    REPL_STATE_CONNECTING,
    REPL_STATE_RECEIVE_PONG,
    REPL_STATE_SEND_PSYNC,
    REPL_STATE_RECEIVE_PSYNC,
    REPL_STATE_RECEIVE_RDB,
    REPL_STATE_CONNECTED
  };

  std::string masterHost_;
  int masterPort_ = 0;
  Client *masterClient_ = nullptr;
  ReplState replState_ = REPL_STATE_NONE;

  std::string replId_;
  int64_t masterReplOffset_ = 0;
  std::vector<Client *> replicas_;

  std::vector<char> replBacklog_;
  size_t replBacklogSize_ = 1024 * 1024;
  int64_t replBacklogOff_ = 0;
  int64_t replBacklogHistLen_ = 0;
  size_t replBacklogIdx_ = 0;

  void replicationCron();
  void connectToMaster();
  void syncWithMaster(Client &client, const RespValue &reply);
  void feedReplicationBacklog(const std::string &data);
  void propagateToReplicas(const std::vector<RespValue> &cmd);
};

} // namespace redisdb
