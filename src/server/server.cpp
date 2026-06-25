#include "server/server.h"
#include "protocol/resp_serializer.h"
#include "server/memory_manager.h"
#include "storage/aof_persistence.h"
#include "storage/rdb_persistence.h"

#include <algorithm>
#include <iostream>
#include <thread>

namespace redisdb {

Server::Server(ServerConfig config)
    : config_(std::move(config)),
      startTime_(std::chrono::steady_clock::now()) {}

Server::~Server() { shutdown(); }

bool Server::initialize() {
  std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
  std::cout << "║         RedisDB v0.1.0 — Starting           ║" << std::endl;
  std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
  std::cout << std::endl;

  if (!platform::initializeSockets()) {
    std::cerr << "[Server] Failed to initialize sockets" << std::endl;
    return false;
  }
  std::cout << "[Server] Socket layer initialized" << std::endl;

  databases_.resize(config_.databases);
  std::cout << "[Server] Created " << config_.databases << " databases"
            << std::endl;

  commandRegistry_.registerBuiltinCommands();

  if (config_.appendOnly) {
    std::cout << "[Server] AOF Persistence is ENABLED (fsync="
              << config_.appendFsync << ")" << std::endl;

    aofStream_.open(config_.aofFilename, std::ios::app | std::ios::binary);
    if (!aofStream_) {
      std::cerr << "[Server] Failed to open AOF file for writing!"
                << std::endl;
      return false;
    }

    if (AofPersistence::loadFromFile(databases_, config_.aofFilename, *this)) {
      std::cout << "[Server] Restored dataset from AOF file" << std::endl;
    } else if (RdbPersistence::loadFromFile(databases_, config_.rdbFilename)) {
      std::cout << "[Server] Restored dataset from " << config_.rdbFilename
                << std::endl;
    }
  } else {
    std::cout << "[Server] AOF Persistence is DISABLED" << std::endl;
    if (RdbPersistence::loadFromFile(databases_, config_.rdbFilename)) {
      std::cout << "[Server] Restored dataset from " << config_.rdbFilename
                << std::endl;
    } else {
      std::cout << "[Server] No existing " << config_.rdbFilename
                << " found, starting fresh" << std::endl;
    }
  }

  eventLoop_ = EventLoop::create(config_.maxClients + 128);
  std::cout << "[Server] Event loop created" << std::endl;

  connManager_ =
      std::make_unique<ConnectionManager>(*eventLoop_, config_.maxClients);
  connManager_->setCommandCallback(
      [this](Client &client) { processClientInput(client); });
  connManager_->setDisconnectCallback(
      [this](Client &client) { closeClient(client); });
  std::cout << "[Server] Connection manager ready (max " << config_.maxClients
            << " clients)" << std::endl;

  tcpServer_ = std::make_unique<TcpServer>(*eventLoop_, *connManager_);
  if (!tcpServer_->listen(config_.bindAddress, config_.port)) {
    std::cerr << "[Server] Failed to start TCP server" << std::endl;
    return false;
  }

  eventLoop_->addTimeEvent(100, [this]() { return serverCron(); });

  std::cout << std::endl;
  std::cout << "[Server] Ready to accept connections on "
            << config_.bindAddress << ":" << config_.port << std::endl;
  std::cout << std::endl;

  return true;
}

void Server::run() { eventLoop_->run(); }

void Server::shutdown() {
  std::cout << std::endl;
  std::cout << "[Server] Shutting down..." << std::endl;

  if (eventLoop_) {
    eventLoop_->stop();
  }
  if (tcpServer_) {
    tcpServer_->close();
  }

  platform::cleanupSockets();
  std::cout << "[Server] Goodbye!" << std::endl;
}

Database &Server::getDatabase(int index) {
  if (index < 0 || index >= static_cast<int>(databases_.size())) {
    return databases_[0];
  }
  return databases_[index];
}

int64_t Server::uptimeSeconds() const {
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(now - startTime_)
      .count();
}

size_t Server::connectedClients() const {
  return connManager_ ? connManager_->clientCount() : 0;
}

uint64_t Server::totalConnectionsReceived() const {
  return connManager_ ? connManager_->totalConnectionsReceived() : 0;
}

void Server::processClientInput(Client &client) {
  auto result = parser_.parse(std::string_view(client.inputBuffer()));

  if (result.bytesConsumed > 0) {
    client.consumeInput(result.bytesConsumed);
  }

  for (auto &command : result.commands) {
    if (client.hasFlag(CLIENT_BLOCKED)) break;

    if (client.hasFlag(CLIENT_MASTER) &&
        replState_ != REPL_STATE_CONNECTED) {
      syncWithMaster(client, command);
    } else {
      executeCommand(client, command);
    }

    if (client.hasFlag(CLIENT_CLOSE_ASAP)) break;
  }
}

void Server::closeClient(Client &client) {
  for (const auto &channel : client.pubsubChannels()) {
    auto it = pubsubChannels_.find(channel);
    if (it != pubsubChannels_.end()) {
      auto &clients = it->second;
      clients.erase(
          std::remove(clients.begin(), clients.end(), &client), clients.end());
      if (clients.empty()) {
        pubsubChannels_.erase(it);
      }
    }
  }
  client.pubsubChannels().clear();

  for (const auto &pattern : client.pubsubPatterns()) {
    auto it = std::remove_if(
        pubsubPatterns_.begin(), pubsubPatterns_.end(),
        [&](const std::pair<std::string, Client *> &p) {
          return p.first == pattern && p.second == &client;
        });
    pubsubPatterns_.erase(it, pubsubPatterns_.end());
  }
  client.pubsubPatterns().clear();

  if (client.hasFlag(CLIENT_BLOCKED)) {
    unblockClient(client);
  }

  for (const auto &key : client.watchedKeys()) {
    for (auto &db : databases_) {
      db.removeWatchedKey(key, &client);
    }
  }
  client.watchedKeys().clear();

  if (client.hasFlag(CLIENT_SLAVE)) {
    removeReplica(&client);
  }
  if (&client == masterClient_) {
    masterClient_ = nullptr;
    replState_ = REPL_STATE_CONNECT;
  }
}

void Server::executeCommand(Client &client, const RespValue &command) {
  if (command.type != RespType::Array) {
    client.addReply(RespSerializer::error("invalid command format"));
    return;
  }

  const auto &args = command.asArray();
  if (args.empty()) {
    client.addReply(RespSerializer::error("empty command"));
    return;
  }

  std::string cmdName = args[0].asString();

  const CommandEntry *entry = commandRegistry_.lookupCommand(cmdName);
  if (!entry) {
    client.addReply(RespSerializer::error(
        "unknown command '" + cmdName + "', with args beginning with: "));
    return;
  }

  if ((entry->flags & CMD_WRITE) && config_.maxMemory > 0) {
    if (MemoryManager::instance().getUsedMemory() >
        static_cast<size_t>(config_.maxMemory)) {
      evictKeysIfNeeded();

      if (MemoryManager::instance().getUsedMemory() >
          static_cast<size_t>(config_.maxMemory)) {
        client.addReply(RespSerializer::error(
            "OOM command not allowed when used memory > 'maxmemory'"));
        return;
      }
    }
  }

  int argCount = static_cast<int>(args.size());

  if (entry->arity > 0 && argCount != entry->arity) {
    client.addReply(RespSerializer::error(
        "wrong number of arguments for '" + cmdName + "' command"));
    return;
  }
  if (entry->arity < 0 && argCount < -entry->arity) {
    client.addReply(RespSerializer::error(
        "wrong number of arguments for '" + cmdName + "' command"));
    return;
  }

  if (client.hasFlag(CLIENT_MULTI) && cmdName != "EXEC" &&
      cmdName != "DISCARD" && cmdName != "MULTI" && cmdName != "WATCH" &&
      cmdName != "UNWATCH") {
    client.transactionQueue().push_back(args);
    client.addReply(RespSerializer::simpleString("QUEUED"));
    return;
  }

  try {
    entry->handler(*this, client, args);
    totalCommandsProcessed_++;

    if (entry->flags & CMD_WRITE) {
      if (config_.appendOnly) {
        std::vector<std::string> strArgs;
        for (const auto &arg : args) {
          strArgs.push_back(arg.asString());
        }
        if (!strArgs.empty()) {
          std::string respCmd = RespSerializer::bulkStringArray(strArgs);
          aofBuffer_ += respCmd;
        }
      }

      propagateToReplicas(args);
    }
  } catch (const std::exception &e) {
    client.addReply(
        RespSerializer::error(std::string("internal error: ") + e.what()));
  }
}

int Server::serverCron() {
  auto nowLru = std::chrono::steady_clock::now();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                     nowLru.time_since_epoch())
                     .count();
  MemoryManager::instance().updateLruClock(
      static_cast<uint32_t>(seconds & 0xFFFFFFFF));

  for (auto &db : databases_) {
    db.activeExpireCycle(20);
  }

  if (config_.clientTimeout > 0 && connManager_) {
    connManager_->closeIdleClients(config_.clientTimeout);
  }

  if (config_.appendOnly && config_.appendFsync == "everysec") {
    auto now = std::chrono::steady_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                       now.time_since_epoch())
                       .count();
    if (seconds > lastAofFsyncTime_) {
      flushAof(false);
      lastAofFsyncTime_ = seconds;
    }
  }

  auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                   nowLru.time_since_epoch())
                   .count();
  std::vector<Client *> timedOutClients;
  for (auto &pair : blockedClients_) {
    for (Client *client : pair.second) {
      if (client->getBlockTimeout() > 0 &&
          nowMs >= client->getBlockTimeout()) {
        timedOutClients.push_back(client);
      }
    }
  }

  for (Client *client : timedOutClients) {
    if (client->hasFlag(CLIENT_BLOCKED)) {
      client->addReply(RespSerializer::nullArray());
      unblockClient(*client);
    }
  }

  replicationCron();

  return 100;
}

bool Server::save(const std::string &filename) {
  if (bgsaveInProgress_) {
    return false;
  }

  std::cout << "[Server] Synchronous SAVE to " << filename << " starting..."
            << std::endl;
  bool success = RdbPersistence::saveToFile(databases_, filename);

  if (success) {
    auto now = std::chrono::system_clock::now();
    lastSaveTime_ = std::chrono::duration_cast<std::chrono::seconds>(
                        now.time_since_epoch())
                        .count();
    for (auto &db : databases_) db.clearDirty();
    std::cout << "[Server] SAVE completed successfully." << std::endl;
  } else {
    std::cerr << "[Server] SAVE failed." << std::endl;
  }

  return success;
}

bool Server::bgsave(const std::string &filename) {
  if (bgsaveInProgress_.exchange(true)) {
    return false;
  }

  std::cout << "[Server] Background saving started..." << std::endl;

  auto dbCopies = std::make_shared<std::vector<Database>>();
  dbCopies->reserve(databases_.size());
  for (const auto &db : databases_) {
    dbCopies->push_back(std::move(*db.clone()));
  }

  std::thread([this, dbCopies, filename]() {
    bool success = RdbPersistence::saveToFile(*dbCopies, filename);

    if (success) {
      auto now = std::chrono::system_clock::now();
      lastSaveTime_ = std::chrono::duration_cast<std::chrono::seconds>(
                          now.time_since_epoch())
                          .count();
      std::cout << "\n[Server] Background saving completed successfully."
                << std::endl;
    } else {
      std::cerr << "\n[Server] Background saving failed." << std::endl;
    }

    bgsaveInProgress_ = false;
  }).detach();

  return true;
}

void Server::propagateToAof(const std::vector<RespValue> &args) {}

void Server::flushAof(bool force) {
  if (!config_.appendOnly || aofBuffer_.empty() || !aofStream_.is_open()) {
    return;
  }

  aofStream_.write(aofBuffer_.data(), aofBuffer_.size());
  aofBuffer_.clear();

  if (force || config_.appendFsync == "always" ||
      config_.appendFsync == "everysec") {
    aofStream_.flush();
  }
}

bool Server::bgrewriteaof() {
  if (bgrewriteaofInProgress_.exchange(true)) {
    return false;
  }
  if (bgsaveInProgress_) {
    return false;
  }

  std::cout << "[Server] Background AOF rewrite started..." << std::endl;

  auto dbCopies = std::make_shared<std::vector<Database>>();
  dbCopies->reserve(databases_.size());
  for (const auto &db : databases_) {
    dbCopies->push_back(std::move(*db.clone()));
  }

  std::string filename = config_.aofFilename + ".temp";

  std::thread([this, dbCopies, filename]() {
    bool success = AofPersistence::rewriteAof(*dbCopies, filename);

    if (success) {
      std::cout << "\n[Server] Background AOF rewrite completed successfully."
                << std::endl;
      std::remove(config_.aofFilename.c_str());
      std::rename(filename.c_str(), config_.aofFilename.c_str());
    } else {
      std::cerr << "\n[Server] Background AOF rewrite failed." << std::endl;
    }

    bgrewriteaofInProgress_ = false;
  }).detach();

  return true;
}

void Server::blockClient(Client &client,
                         const std::vector<std::string> &keys,
                         int64_t timeout, const RespValue &cmd,
                         const std::string &target) {
  client.setFlag(CLIENT_BLOCKED);
  client.blockedKeys() = keys;

  if (timeout > 0) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    client.setBlockTimeout(now + timeout);
  } else {
    client.setBlockTimeout(0);
  }
  client.setBlockTarget(target);
  client.setBlockCmd(cmd);

  for (const auto &key : keys) {
    blockedClients_[key].push_back(&client);
  }
}

void Server::unblockClient(Client &client) {
  if (!client.hasFlag(CLIENT_BLOCKED)) return;

  for (const auto &key : client.blockedKeys()) {
    auto it = blockedClients_.find(key);
    if (it != blockedClients_.end()) {
      auto &clients = it->second;
      clients.erase(
          std::remove(clients.begin(), clients.end(), &client), clients.end());
      if (clients.empty()) {
        blockedClients_.erase(it);
      }
    }
  }

  client.clearBlockState();
}

void Server::handleClientsBlockedOnKey(const std::string &key) {
  auto it = blockedClients_.find(key);
  if (it == blockedClients_.end() || it->second.empty()) return;

  Client *client = it->second.front();

  RespValue cmd = client->getBlockCmd();

  unblockClient(*client);

  executeCommand(*client, cmd);

  if (connManager_) {
    connManager_->installWriteHandler(client->fd());
  }
}

void Server::evictKeysIfNeeded() {
  if (config_.maxMemory == 0 || config_.maxMemoryPolicy == "noeviction") {
    return;
  }

  size_t targetMemory = static_cast<size_t>(config_.maxMemory);
  int samples = config_.maxMemorySamples > 0 ? config_.maxMemorySamples : 5;

  bool isVolatile = (config_.maxMemoryPolicy.find("volatile") == 0);
  bool isRandom =
      (config_.maxMemoryPolicy.find("random") != std::string::npos);
  bool isLfu = (config_.maxMemoryPolicy.find("lfu") != std::string::npos);
  bool isTtl = (config_.maxMemoryPolicy == "volatile-ttl");

  while (MemoryManager::instance().getUsedMemory() > targetMemory) {
    bool evictedSomething = false;

    for (auto &db : databases_) {
      if (MemoryManager::instance().getUsedMemory() <= targetMemory) break;

      auto keysToSample = db.randomSample(samples, isVolatile);
      if (keysToSample.empty()) continue;

      std::string bestKey;

      if (isRandom) {
        bestKey = keysToSample[0];
      } else if (isTtl) {
        int64_t minTtl = INT64_MAX;
        for (const auto &key : keysToSample) {
          int64_t ttl = db.pttl(key);
          if (ttl >= 0 && ttl < minTtl) {
            minTtl = ttl;
            bestKey = key;
          }
        }
      } else if (isLfu) {
        uint32_t minCounter = 256;
        for (const auto &key : keysToSample) {
          const RedisObject *obj = db.getObject(key);
          if (obj) {
            uint32_t counter = obj->lruClock() & 0xFF;
            if (counter < minCounter) {
              minCounter = counter;
              bestKey = key;
            }
          }
        }
      } else {
        uint32_t currentClock = MemoryManager::instance().getLruClock();
        uint32_t maxIdle = 0;
        for (const auto &key : keysToSample) {
          const RedisObject *obj = db.getObject(key);
          if (obj) {
            uint32_t idle = currentClock - obj->lruClock();
            if (idle >= maxIdle) {
              maxIdle = idle;
              bestKey = key;
            }
          }
        }
        if (bestKey.empty()) {
          bestKey = keysToSample[0];
        }
      }

      if (!bestKey.empty()) {
        db.del(bestKey);
        evictedSomething = true;
      }
    }

    if (!evictedSomething) {
      break;
    }
  }
}

void Server::subscribeChannel(Client &client, const std::string &channel) {
  if (client.pubsubChannels().insert(channel).second) {
    pubsubChannels_[channel].push_back(&client);
  }
}

void Server::unsubscribeChannel(Client &client, const std::string &channel) {
  if (client.pubsubChannels().erase(channel)) {
    auto it = pubsubChannels_.find(channel);
    if (it != pubsubChannels_.end()) {
      auto &clients = it->second;
      clients.erase(
          std::remove(clients.begin(), clients.end(), &client), clients.end());
      if (clients.empty()) {
        pubsubChannels_.erase(it);
      }
    }
  }
}

void Server::psubscribePattern(Client &client, const std::string &pattern) {
  if (client.pubsubPatterns().insert(pattern).second) {
    pubsubPatterns_.push_back({pattern, &client});
  }
}

void Server::punsubscribePattern(Client &client, const std::string &pattern) {
  if (client.pubsubPatterns().erase(pattern)) {
    auto it = std::remove_if(
        pubsubPatterns_.begin(), pubsubPatterns_.end(),
        [&](const std::pair<std::string, Client *> &p) {
          return p.first == pattern && p.second == &client;
        });
    pubsubPatterns_.erase(it, pubsubPatterns_.end());
  }
}

int Server::publishMessage(const std::string &channel,
                           const std::string &message) {
  int receivers = 0;
  std::string respMessage = RespSerializer::bulkString(message);
  std::string respChannel = RespSerializer::bulkString(channel);

  auto it = pubsubChannels_.find(channel);
  if (it != pubsubChannels_.end()) {
    std::vector<std::string> parts = {"message", channel, message};
    std::string reply = RespSerializer::bulkStringArray(parts);
    for (Client *client : it->second) {
      client->addReply(reply);
      if (connManager_) {
        connManager_->installWriteHandler(client->fd());
      }
      receivers++;
    }
  }

  for (const auto &[pattern, client] : pubsubPatterns_) {
    bool match = false;
    if (pattern.back() == '*') {
      std::string prefix = pattern.substr(0, pattern.length() - 1);
      if (channel.substr(0, prefix.length()) == prefix) {
        match = true;
      }
    } else if (pattern == channel) {
      match = true;
    }

    if (match) {
      std::vector<std::string> parts = {"pmessage", pattern, channel, message};
      std::string reply = RespSerializer::bulkStringArray(parts);
      client->addReply(reply);
      if (connManager_) {
        connManager_->installWriteHandler(client->fd());
      }
      receivers++;
    }
  }

  return receivers;
}

} // namespace redisdb
