#include "server/memory_manager.h"

namespace redisdb {

size_t MemoryManager::estimateStringSize(const std::string &str) {
  return 32 + str.capacity();
}

size_t MemoryManager::estimateObjectSize(const RedisObject &obj) {
  size_t size = sizeof(RedisObject);

  switch (obj.type()) {
  case ObjectType::String:
    size += estimateStringSize(obj.asString());
    break;
  case ObjectType::List: {
    const auto &list = obj.asList();
    size += sizeof(LinkedList) + list.size() * (sizeof(void *) * 2 + 32);
    break;
  }
  case ObjectType::Hash: {
    const auto &hash = obj.asHash();
    size += sizeof(HashTable<std::string, std::string>) + hash.size() * 64;
    break;
  }
  case ObjectType::Set: {
    const auto &set = obj.asSet();
    size += sizeof(HashSet) + set.size() * 48;
    break;
  }
  case ObjectType::SortedSet: {
    const auto &zset = obj.asSortedSet();
    size += sizeof(SortedSet) + zset.size() * 128;
    break;
  }
  default:
    break;
  }
  return size;
}

} // namespace redisdb
