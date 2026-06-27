#pragma once

#include "storage/redis_object.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace redisdb {

class Client;

class Database {
public:
  Database() = default;
  ~Database() = default;

  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  Database(Database &&) noexcept = default;
  Database &operator=(Database &&) noexcept = default;

  std::unique_ptr<Database> clone() const;

  void set(const std::string &key, const std::string &value);
  std::optional<std::string> get(const std::string &key) const;
  bool del(const std::string &key);
  bool exists(const std::string &key) const;
  std::string type(const std::string &key) const;
  bool rename(const std::string &oldKey, const std::string &newKey);

  std::optional<int64_t> incrby(const std::string &key, int64_t increment);
  std::optional<size_t> append(const std::string &key,
                               const std::string &value);

  const RedisObject *getObject(const std::string &key) const;
  RedisObject *getObjectForWrite(const std::string &key);
  void setObject(const std::string &key, RedisObject &&obj);

  bool setExpiry(const std::string &key, int64_t milliseconds);
  bool setExpiryAt(const std::string &key, int64_t timestampMs);
  int64_t pttl(const std::string &key) const;
  bool persist(const std::string &key);
  bool checkAndExpire(const std::string &key);

  size_t size() const { return data_.size(); }
  size_t expiresSize() const { return expires_.size(); }
  void flushDb();
  std::vector<std::string> keys(const std::string &pattern) const;

  uint64_t hits() const { return hits_; }
  uint64_t misses() const { return misses_; }
  int64_t dirty() const { return dirty_; }
  void clearDirty() { dirty_ = 0; }

  int activeExpireCycle(int maxSamples = 20);
  std::vector<std::string> randomSample(int count,
                                        bool volatileOnly = false) const;

  void addWatchedKey(const std::string &key, Client *client);
  void removeWatchedKey(const std::string &key, Client *client);
  void touchWatchedKey(const std::string &key);

private:
  bool isExpired(const std::string &key) const;
  static bool globMatch(const std::string &pattern, const std::string &str);

  std::unordered_map<std::string, RedisObject> data_;
  std::unordered_map<std::string, int64_t> expires_;
  std::unordered_map<std::string, std::vector<Client *>> watchedKeys_;

  mutable uint64_t hits_ = 0;
  mutable uint64_t misses_ = 0;
  int64_t dirty_ = 0;

  friend class Database;
};

} // namespace redisdb
