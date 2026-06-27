#include "storage/db.h"
#include "networking/client.h"
#include "server/memory_manager.h"

#include <charconv>
#include <limits>
#include <random>

namespace redisdb {

std::unique_ptr<Database> Database::clone() const {
  auto copy = std::make_unique<Database>();

  copy->expires_ = expires_;

  copy->data_.reserve(data_.size());
  for (const auto &[key, obj] : data_) {
    copy->data_[key] = obj.clone();
  }

  return copy;
}

void Database::set(const std::string &key, const std::string &value) {
  auto it = data_.find(key);
  if (it != data_.end()) {
    MemoryManager::instance().freeMemory(
        MemoryManager::estimateObjectSize(it->second));
  } else {
    MemoryManager::instance().addMemory(
        MemoryManager::estimateStringSize(key));
  }

  RedisObject obj(value);
  obj.updateLruClock(MemoryManager::instance().getLruClock());
  MemoryManager::instance().addMemory(
      MemoryManager::estimateObjectSize(obj));
  data_[key] = std::move(obj);

  expires_.erase(key);
  touchWatchedKey(key);
  dirty_++;
}

std::optional<std::string> Database::get(const std::string &key) const {
  auto *self = const_cast<Database *>(this);
  if (self->checkAndExpire(key)) {
    return std::nullopt;
  }

  auto it = data_.find(key);
  if (it == data_.end()) {
    misses_++;
    return std::nullopt;
  }

  hits_++;
  it->second.updateLruClock(MemoryManager::instance().getLruClock());
  if (it->second.type() == ObjectType::String) {
    return it->second.asString();
  }
  return std::nullopt;
}

const RedisObject *Database::getObject(const std::string &key) const {
  auto *self = const_cast<Database *>(this);
  if (self->checkAndExpire(key)) return nullptr;

  auto it = data_.find(key);
  if (it == data_.end()) {
    misses_++;
    return nullptr;
  }
  hits_++;
  it->second.updateLruClock(MemoryManager::instance().getLruClock());
  return &it->second;
}

RedisObject *Database::getObjectForWrite(const std::string &key) {
  if (checkAndExpire(key)) return nullptr;

  auto it = data_.find(key);
  if (it == data_.end()) {
    misses_++;
    return nullptr;
  }
  hits_++;
  it->second.updateLruClock(MemoryManager::instance().getLruClock());
  return &it->second;
}

void Database::setObject(const std::string &key, RedisObject &&obj) {
  auto it = data_.find(key);
  if (it != data_.end()) {
    MemoryManager::instance().freeMemory(
        MemoryManager::estimateObjectSize(it->second));
  } else {
    MemoryManager::instance().addMemory(
        MemoryManager::estimateStringSize(key));
  }

  obj.updateLruClock(MemoryManager::instance().getLruClock());
  MemoryManager::instance().addMemory(
      MemoryManager::estimateObjectSize(obj));
  data_[key] = std::move(obj);
  expires_.erase(key);
  touchWatchedKey(key);
  dirty_++;
}

bool Database::del(const std::string &key) {
  expires_.erase(key);
  auto it = data_.find(key);
  if (it != data_.end()) {
    MemoryManager::instance().freeMemory(
        MemoryManager::estimateObjectSize(it->second));
    MemoryManager::instance().freeMemory(
        MemoryManager::estimateStringSize(key));
    data_.erase(it);
    touchWatchedKey(key);
    dirty_++;
    return true;
  }
  return false;
}

bool Database::exists(const std::string &key) const {
  auto *self = const_cast<Database *>(this);
  if (self->checkAndExpire(key)) {
    return false;
  }
  return data_.count(key) > 0;
}

std::string Database::type(const std::string &key) const {
  auto *self = const_cast<Database *>(this);
  if (self->checkAndExpire(key)) {
    return "none";
  }

  auto it = data_.find(key);
  if (it != data_.end()) {
    return it->second.typeString();
  }
  return "none";
}

bool Database::rename(const std::string &oldKey, const std::string &newKey) {
  auto it = data_.find(oldKey);
  if (it == data_.end()) return false;

  MemoryManager::instance().freeMemory(
      MemoryManager::estimateObjectSize(it->second));
  MemoryManager::instance().freeMemory(
      MemoryManager::estimateStringSize(oldKey));

  RedisObject value = std::move(it->second);
  data_.erase(it);

  auto expIt = expires_.find(oldKey);
  int64_t expiry = -1;
  if (expIt != expires_.end()) {
    expiry = expIt->second;
    expires_.erase(expIt);
  }

  if (expiry >= 0) {
    expires_[newKey] = expiry;
  }

  touchWatchedKey(oldKey);

  setObject(newKey, std::move(value));

  return true;
}

std::optional<int64_t> Database::incrby(const std::string &key,
                                        int64_t increment) {
  int64_t current = 0;

  auto it = data_.find(key);
  if (it != data_.end()) {
    if (it->second.type() != ObjectType::String) {
      return std::nullopt;
    }

    const std::string &strValue = it->second.asString();
    auto [ptr, ec] = std::from_chars(
        strValue.data(), strValue.data() + strValue.size(), current);

    if (ec != std::errc() || ptr != strValue.data() + strValue.size()) {
      return std::nullopt;
    }
  }

  if ((increment > 0 &&
       current > (std::numeric_limits<int64_t>::max)() - increment) ||
      (increment < 0 &&
       current < (std::numeric_limits<int64_t>::min)() - increment)) {
    return std::nullopt;
  }

  int64_t newValue = current + increment;
  set(key, std::to_string(newValue));
  return newValue;
}

std::optional<size_t> Database::append(const std::string &key,
                                       const std::string &value) {
  auto it = data_.find(key);
  if (it != data_.end()) {
    if (it->second.type() != ObjectType::String) {
      return std::nullopt;
    }

    MemoryManager::instance().freeMemory(
        MemoryManager::estimateObjectSize(it->second));
    it->second.asString() += value;
    it->second.updateLruClock(MemoryManager::instance().getLruClock());
    MemoryManager::instance().addMemory(
        MemoryManager::estimateObjectSize(it->second));

    dirty_++;
    return it->second.asString().size();
  } else {
    set(key, value);
    return value.size();
  }
}

bool Database::setExpiry(const std::string &key, int64_t milliseconds) {
  if (data_.find(key) == data_.end()) return false;

  auto now = std::chrono::system_clock::now();
  auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch())
                   .count();

  expires_[key] = epoch + milliseconds;
  return true;
}

bool Database::setExpiryAt(const std::string &key, int64_t timestampMs) {
  if (data_.find(key) == data_.end()) return false;
  expires_[key] = timestampMs;
  return true;
}

int64_t Database::pttl(const std::string &key) const {
  if (data_.find(key) == data_.end()) return -2;

  auto it = expires_.find(key);
  if (it == expires_.end()) return -1;

  auto now = std::chrono::system_clock::now();
  auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch())
                   .count();

  int64_t remaining = it->second - epoch;
  if (remaining <= 0) return -2;

  return remaining;
}

bool Database::persist(const std::string &key) {
  return expires_.erase(key) > 0;
}

bool Database::checkAndExpire(const std::string &key) {
  if (isExpired(key)) {
    auto it = data_.find(key);
    if (it != data_.end()) {
      MemoryManager::instance().freeMemory(
          MemoryManager::estimateObjectSize(it->second));
      MemoryManager::instance().freeMemory(
          MemoryManager::estimateStringSize(key));
      data_.erase(it);
      touchWatchedKey(key);
    }
    expires_.erase(key);
    dirty_++;
    return true;
  }
  return false;
}

bool Database::isExpired(const std::string &key) const {
  auto it = expires_.find(key);
  if (it == expires_.end()) return false;

  auto now = std::chrono::system_clock::now();
  auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch())
                   .count();

  return epoch >= it->second;
}

void Database::flushDb() {
  for (const auto &[key, obj] : data_) {
    MemoryManager::instance().freeMemory(
        MemoryManager::estimateObjectSize(obj));
    MemoryManager::instance().freeMemory(
        MemoryManager::estimateStringSize(key));
  }

  for (const auto &[key, clients] : watchedKeys_) {
    for (Client *client : clients) {
      client->setFlag(CLIENT_DIRTY_EXEC);
    }
  }

  data_.clear();
  expires_.clear();
  dirty_++;
}

std::vector<std::string> Database::keys(const std::string &pattern) const {
  std::vector<std::string> result;

  for (auto &[key, _] : data_) {
    if (isExpired(key)) continue;

    if (pattern == "*" || globMatch(pattern, key)) {
      result.push_back(key);
    }
  }

  return result;
}

int Database::activeExpireCycle(int maxSamples) {
  if (expires_.empty()) return 0;

  int expired = 0;
  int sampled = 0;

  auto it = expires_.begin();
  while (it != expires_.end() && sampled < maxSamples) {
    sampled++;
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now.time_since_epoch())
                     .count();

    if (epoch >= it->second) {
      auto dataIt = data_.find(it->first);
      if (dataIt != data_.end()) {
        MemoryManager::instance().freeMemory(
            MemoryManager::estimateObjectSize(dataIt->second));
        MemoryManager::instance().freeMemory(
            MemoryManager::estimateStringSize(it->first));
        data_.erase(dataIt);
        touchWatchedKey(it->first);
      }
      it = expires_.erase(it);
      expired++;
    } else {
      ++it;
    }
  }

  return expired;
}

std::vector<std::string> Database::randomSample(int count,
                                                bool volatileOnly) const {
  std::vector<std::string> result;
  if (count <= 0) return result;

  std::random_device rd;
  std::mt19937 gen(rd());

  for (int i = 0; i < count; ++i) {
    if (volatileOnly) {
      if (expires_.empty()) break;
      std::uniform_int_distribution<size_t> dist(0, expires_.size() - 1);
      auto it = expires_.begin();
      std::advance(it, dist(gen));
      result.push_back(it->first);
    } else {
      if (data_.empty()) break;
      std::uniform_int_distribution<size_t> dist(0, data_.size() - 1);
      auto it = data_.begin();
      std::advance(it, dist(gen));
      result.push_back(it->first);
    }
  }
  return result;
}

bool Database::globMatch(const std::string &pattern, const std::string &str) {
  size_t pi = 0, si = 0;
  size_t starP = std::string::npos, starS = 0;

  while (si < str.size()) {
    if (pi < pattern.size() &&
        (pattern[pi] == '?' || pattern[pi] == str[si])) {
      pi++;
      si++;
    } else if (pi < pattern.size() && pattern[pi] == '*') {
      starP = pi;
      starS = si;
      pi++;
    } else if (starP != std::string::npos) {
      pi = starP + 1;
      starS++;
      si = starS;
    } else {
      return false;
    }
  }

  while (pi < pattern.size() && pattern[pi] == '*') pi++;

  return pi == pattern.size();
}

void Database::addWatchedKey(const std::string &key, Client *client) {
  watchedKeys_[key].push_back(client);
}

void Database::removeWatchedKey(const std::string &key, Client *client) {
  auto it = watchedKeys_.find(key);
  if (it != watchedKeys_.end()) {
    auto &clients = it->second;
    clients.erase(
        std::remove(clients.begin(), clients.end(), client), clients.end());
    if (clients.empty()) {
      watchedKeys_.erase(it);
    }
  }
}

void Database::touchWatchedKey(const std::string &key) {
  auto it = watchedKeys_.find(key);
  if (it != watchedKeys_.end()) {
    for (Client *client : it->second) {
      client->setFlag(CLIENT_DIRTY_EXEC);
    }
  }
}

} // namespace redisdb
