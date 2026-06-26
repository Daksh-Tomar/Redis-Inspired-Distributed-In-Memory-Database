#pragma once

#include "storage/redis_object.h"

#include <atomic>
#include <string>

namespace redisdb {

class MemoryManager {
public:
  static MemoryManager &instance() {
    static MemoryManager mgr;
    return mgr;
  }

  void addMemory(size_t bytes) {
    usedMemory_.fetch_add(bytes, std::memory_order_relaxed);
  }

  void freeMemory(size_t bytes) {
    size_t current = usedMemory_.load(std::memory_order_relaxed);
    if (bytes > current) {
      usedMemory_.store(0, std::memory_order_relaxed);
    } else {
      usedMemory_.fetch_sub(bytes, std::memory_order_relaxed);
    }
  }

  size_t getUsedMemory() const {
    return usedMemory_.load(std::memory_order_relaxed);
  }

  uint32_t getLruClock() const {
    return lruClock_.load(std::memory_order_relaxed);
  }

  void updateLruClock(uint32_t clock) {
    lruClock_.store(clock, std::memory_order_relaxed);
  }

  static size_t estimateObjectSize(const RedisObject &obj);
  static size_t estimateStringSize(const std::string &str);

private:
  MemoryManager() = default;
  std::atomic<size_t> usedMemory_{0};
  std::atomic<uint32_t> lruClock_{0};
};

} // namespace redisdb
