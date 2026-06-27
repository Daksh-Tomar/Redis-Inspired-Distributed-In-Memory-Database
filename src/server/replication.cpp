#include "server/server.h"
#include "networking/platform/socket_compat.h"
#include "protocol/resp_serializer.h"
#include "storage/rdb_persistence.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>

namespace redisdb {

void Server::setReplicationMaster(const std::string &host, int port) {
  masterHost_ = host;
  masterPort_ = port;
  replState_ = REPL_STATE_CONNECT;
  disconnectFromMaster();
  for (auto &db : databases_) {
    db.flushDb();
  }
}

void Server::disconnectFromMaster() {
  if (masterClient_) {
    int fd = static_cast<int>(masterClient_->fd());
    masterClient_ = nullptr;
    connManager_->removeClient(fd);
  }
  replState_ = REPL_STATE_CONNECT;
}

void Server::addReplica(Client *client) {
  client->setFlag(CLIENT_SLAVE);
  replicas_.push_back(client);
  std::cout << "[Replication] Added replica fd=" << client->fd() << std::endl;
}

void Server::removeReplica(Client *client) {
  auto it = std::find(replicas_.begin(), replicas_.end(), client);
  if (it != replicas_.end()) {
    replicas_.erase(it);
    std::cout << "[Replication] Removed replica fd=" << client->fd()
              << std::endl;
  }
}

void Server::generateReplId() {
  static const char alphanum[] = "0123456789"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

  replId_.clear();
  for (int i = 0; i < 40; ++i) {
    replId_ += alphanum[dis(gen)];
  }
}

void Server::feedReplicationBacklog(const std::string &data) {
  if (replBacklog_.empty()) {
    replBacklog_.resize(replBacklogSize_);
  }

  for (char c : data) {
    replBacklog_[replBacklogIdx_] = c;
    replBacklogIdx_ = (replBacklogIdx_ + 1) % replBacklogSize_;

    if (replBacklogHistLen_ < static_cast<int64_t>(replBacklogSize_)) {
      replBacklogHistLen_++;
    } else {
      replBacklogOff_++;
    }
  }
  masterReplOffset_ += data.size();
}

void Server::propagateToReplicas(const std::vector<RespValue> &cmd) {
  if (replicas_.empty() && replBacklogHistLen_ == 0 && replId_.empty())
    return;

  std::string resp = "*" + std::to_string(cmd.size()) + "\r\n";
  for (const auto &arg : cmd) {
    std::string str = arg.asString();
    resp += "$" + std::to_string(str.length()) + "\r\n" + str + "\r\n";
  }

  if (!replicas_.empty() || replBacklogHistLen_ > 0 || !replId_.empty()) {
    feedReplicationBacklog(resp);
  }

  for (Client *replica : replicas_) {
    replica->addReply(resp);
  }
}

void Server::replicationCron() {
  if (masterHost_.empty()) return;

  if (replState_ == REPL_STATE_CONNECT) {
    connectToMaster();
  }
}

void Server::connectToMaster() {
  std::cout << "[Replication] Connecting to master " << masterHost_ << ":"
            << masterPort_ << std::endl;
  socket_t s = platform::connectAsync(masterHost_, masterPort_);
  if (s == INVALID_SOCKET_VAL) {
    std::cerr << "[Replication] Failed to initiate connection to master"
              << std::endl;
    return;
  }

  masterClient_ = connManager_->createOutboundClient(s);
  masterClient_->setFlag(CLIENT_MASTER);
  replState_ = REPL_STATE_RECEIVE_PONG;

  masterClient_->addReply(RespSerializer::bulkStringArray({"PING"}));
}

void Server::syncWithMaster(Client &client, const RespValue &reply) {
  std::string replyStr = reply.asString();

  if (replState_ == REPL_STATE_RECEIVE_PONG) {
    if (replyStr == "PONG") {
      std::cout << "[Replication] Master replied to PING" << std::endl;
      client.addReply(RespSerializer::bulkStringArray(
          {"REPLCONF", "listening-port", std::to_string(config_.port)}));
      replState_ = REPL_STATE_SEND_PSYNC;
    } else {
      std::cerr << "[Replication] Unexpected reply to PING: " << replyStr
                << std::endl;
      disconnectFromMaster();
    }
  } else if (replState_ == REPL_STATE_SEND_PSYNC) {
    if (replyStr == "OK") {
      std::cout << "[Replication] Master replied to REPLCONF" << std::endl;
      client.addReply(RespSerializer::bulkStringArray({"PSYNC", "?", "-1"}));
      replState_ = REPL_STATE_RECEIVE_PSYNC;
    } else {
      std::cerr << "[Replication] Unexpected reply to REPLCONF: " << replyStr
                << std::endl;
      disconnectFromMaster();
    }
  } else if (replState_ == REPL_STATE_RECEIVE_PSYNC) {
    if (replyStr.find("FULLRESYNC") == 0) {
      std::cout << "[Replication] Master requested FULLRESYNC: " << replyStr
                << std::endl;
      replState_ = REPL_STATE_RECEIVE_RDB;
    } else if (replyStr.find("CONTINUE") == 0) {
      std::cout << "[Replication] Master requested CONTINUE: " << replyStr
                << std::endl;
      replState_ = REPL_STATE_CONNECTED;
    } else {
      std::cerr << "[Replication] Unexpected reply to PSYNC: " << replyStr
                << std::endl;
      disconnectFromMaster();
    }
  } else if (replState_ == REPL_STATE_RECEIVE_RDB) {
    if (reply.type == RespType::BulkString ||
        reply.type == RespType::SimpleString) {
      std::cout << "[Replication] Received RDB file from master ("
                << replyStr.length() << " bytes)" << std::endl;

      std::string tempFilename =
          "temp_repl_" + std::to_string(config_.port) + ".rdb";
      std::ofstream out(tempFilename, std::ios::binary);
      if (out) {
        out.write(replyStr.data(), replyStr.size());
        out.close();

        if (RdbPersistence::loadFromFile(databases_, tempFilename)) {
          std::cout << "[Replication] Successfully loaded RDB from master."
                    << std::endl;
          replState_ = REPL_STATE_CONNECTED;
          std::cout
              << "[Replication] SYNC complete. Online and accepting commands."
              << std::endl;
        } else {
          std::cerr
              << "[Replication] Failed to load RDB from master. Disconnecting."
              << std::endl;
          disconnectFromMaster();
        }
      } else {
        std::cerr
            << "[Replication] Failed to write temp RDB file. Disconnecting."
            << std::endl;
        disconnectFromMaster();
      }
    } else {
      std::cerr << "[Replication] Expected RDB file, got type code: "
                << static_cast<int>(reply.type) << std::endl;
      disconnectFromMaster();
    }
  }
}

} // namespace redisdb
