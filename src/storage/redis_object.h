#pragma once

#include "storage/data_structures/hash_set.h"
#include "storage/data_structures/hash_table.h"
#include "storage/data_structures/linked_list.h"
#include "storage/data_structures/sorted_set.h"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace redisdb {

enum class ObjectType {
  String,
  List,
  Hash,
  Set,
  SortedSet,
  None
};

class RedisObject {
public:
  explicit RedisObject(std::string str);
  explicit RedisObject(std::unique_ptr<LinkedList> list);
  explicit RedisObject(std::unique_ptr<HashTable<std::string, std::string>> hash);
  explicit RedisObject(std::unique_ptr<HashSet> set);
  explicit RedisObject(std::unique_ptr<SortedSet> zset);

  RedisObject();

  ~RedisObject() = default;

  RedisObject(RedisObject &&) noexcept = default;
  RedisObject &operator=(RedisObject &&) noexcept = default;

  RedisObject(const RedisObject &) = delete;

  RedisObject clone() const;

  ObjectType type() const { return type_; }
  std::string typeString() const;

  std::string &asString();
  const std::string &asString() const;

  LinkedList &asList();
  const LinkedList &asList() const;

  HashTable<std::string, std::string> &asHash();
  const HashTable<std::string, std::string> &asHash() const;

  HashSet &asSet();
  const HashSet &asSet() const;

  SortedSet &asSortedSet();
  const SortedSet &asSortedSet() const;

  uint32_t lruClock() const { return lruClock_; }
  void updateLruClock(uint32_t clock) const { lruClock_ = clock; }

private:
  ObjectType type_;
  mutable uint32_t lruClock_ = 0;

  std::variant<
      std::monostate,
      std::string,
      std::unique_ptr<LinkedList>,
      std::unique_ptr<HashTable<std::string, std::string>>,
      std::unique_ptr<HashSet>,
      std::unique_ptr<SortedSet>>
      value_;
};

} // namespace redisdb
