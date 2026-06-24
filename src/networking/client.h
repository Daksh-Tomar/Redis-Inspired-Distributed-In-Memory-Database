#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// Client Connection State
// ═══════════════════════════════════════════════════════════════════════════════

#include "networking/platform/socket_compat.h"
#include "protocol/resp_types.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace redisdb {

class Database;

enum ClientFlag : uint32_t {
  CLIENT_NONE = 0,
  CLIENT_MULTI = (1 << 0),
  CLIENT_DIRTY_EXEC = (1 << 1),
  CLIENT_SLAVE = (1 << 2),
  CLIENT_MASTER = (1 << 3),
  CLIENT_PUBSUB = (1 << 4),
  CLIENT_CLOSE_ASAP = (1 << 5),
  CLIENT_BLOCKED = (1 << 6),
};

class Client {
public:
  explicit Client(socket_t fd);
  ~Client();

  // Socket IO
  int readFromSocket();

  int writeToSocket();

  void addReply(const std::string &data);

  bool hasPendingOutput() const { return outputPos_ < outputBuffer_.size(); }

  const std::string &getOutputBuffer() const { return outputBuffer_; }
  void clearOutputBuffer() {
    outputBuffer_.clear();
    outputPos_ = 0;
  }

  // Input Buffer Access
  const std::string &inputBuffer() const { return inputBuffer_; }
  std::string &inputBuffer() { return inputBuffer_; }
  void consumeInput(size_t bytes);

  //  Accessors
  socket_t fd() const { return fd_; }
  int fdAsInt() const { return static_cast<int>(fd_); }

  uint32_t flags() const { return flags_; }
  void setFlag(ClientFlag flag) { flags_ |= flag; }
  void clearFlag(ClientFlag flag) { flags_ &= ~flag; }
  bool hasFlag(ClientFlag flag) const { return (flags_ & flag) != 0; }

  int selectedDb() const { return selectedDb_; }
  void selectDb(int db) { selectedDb_ = db; }

  // Timestamps
  std::chrono::steady_clock::time_point createdAt() const { return createdAt_; }
  std::chrono::steady_clock::time_point lastInteraction() const {
    return lastInteraction_;
  }
  void updateLastInteraction() {
    lastInteraction_ = std::chrono::steady_clock::now();
  }

  uint64_t id() const { return id_; }

  // Transaction State
  std::vector<std::vector<RespValue>> &transactionQueue() {
    return transactionQueue_;
  }
  void clearTransaction() {
    transactionQueue_.clear();
    clearFlag(CLIENT_MULTI);
    clearFlag(CLIENT_DIRTY_EXEC);
  }

  // Pub/Sub and Watch State
  std::unordered_set<std::string> &watchedKeys() { return watchedKeys_; }
  std::unordered_set<std::string> &pubsubChannels() { return pubsubChannels_; }
  std::unordered_set<std::string> &pubsubPatterns() { return pubsubPatterns_; }

  // Blocking State
  std::vector<std::string> &blockedKeys() { return blockedKeys_; }
  void setBlockTimeout(int64_t timeout) { blockTimeout_ = timeout; }
  int64_t getBlockTimeout() const { return blockTimeout_; }
  void setBlockTarget(const std::string &target) { blockTarget_ = target; }
  const std::string &getBlockTarget() const { return blockTarget_; }
  void setBlockCmd(const RespValue &cmd) { blockCmd_ = cmd; }
  const RespValue &getBlockCmd() const { return blockCmd_; }
  void clearBlockState() {
    blockedKeys_.clear();
    blockTimeout_ = 0;
    blockTarget_.clear();
    blockCmd_ = RespValue::null();
    clearFlag(CLIENT_BLOCKED);
  }

private:
  socket_t fd_;
  uint64_t id_;

  // Buffers
  std::string inputBuffer_;
  std::string outputBuffer_;
  size_t outputPos_ = 0;

  // State
  uint32_t flags_ = CLIENT_NONE;
  int selectedDb_ = 0;

  // Timestamps
  std::chrono::steady_clock::time_point createdAt_;
  std::chrono::steady_clock::time_point lastInteraction_;

  // Transaction and Pub/Sub State
  std::vector<std::vector<RespValue>> transactionQueue_;
  std::unordered_set<std::string> watchedKeys_;
  std::unordered_set<std::string> pubsubChannels_;
  std::unordered_set<std::string> pubsubPatterns_;

  // Blocking State
  std::vector<std::string> blockedKeys_;
  int64_t blockTimeout_ = 0;
  std::string blockTarget_;
  RespValue blockCmd_;

  static uint64_t nextId_;

  static constexpr size_t READ_BUFFER_SIZE = 16384;
  static constexpr size_t MAX_INPUT_BUFFER = 1024 * 1024 * 1024;
};

} // namespace redisdb
