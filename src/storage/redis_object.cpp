#include "storage/redis_object.h"

namespace redisdb {

RedisObject::RedisObject(std::string str)
    : type_(ObjectType::String), value_(std::move(str)) {}

RedisObject::RedisObject(std::unique_ptr<LinkedList> list)
    : type_(ObjectType::List), value_(std::move(list)) {}

RedisObject::RedisObject(
    std::unique_ptr<HashTable<std::string, std::string>> hash)
    : type_(ObjectType::Hash), value_(std::move(hash)) {}

RedisObject::RedisObject(std::unique_ptr<HashSet> set)
    : type_(ObjectType::Set), value_(std::move(set)) {}

RedisObject::RedisObject(std::unique_ptr<SortedSet> zset)
    : type_(ObjectType::SortedSet), value_(std::move(zset)) {}

RedisObject::RedisObject()
    : type_(ObjectType::None), value_(std::monostate{}) {}

RedisObject RedisObject::clone() const {
  switch (type_) {
  case ObjectType::String:
    return RedisObject(std::get<std::string>(value_));
  case ObjectType::List:
    return RedisObject(std::get<std::unique_ptr<LinkedList>>(value_)->clone());
  case ObjectType::Hash:
    return RedisObject(
        std::get<std::unique_ptr<HashTable<std::string, std::string>>>(value_)
            ->clone());
  case ObjectType::Set:
    return RedisObject(std::get<std::unique_ptr<HashSet>>(value_)->clone());
  case ObjectType::SortedSet:
    return RedisObject(
        std::get<std::unique_ptr<SortedSet>>(value_)->clone());
  default:
    return RedisObject();
  }
}

std::string RedisObject::typeString() const {
  switch (type_) {
  case ObjectType::String:    return "string";
  case ObjectType::List:      return "list";
  case ObjectType::Hash:      return "hash";
  case ObjectType::Set:       return "set";
  case ObjectType::SortedSet: return "zset";
  default:                    return "none";
  }
}

std::string &RedisObject::asString() {
  return std::get<std::string>(value_);
}

const std::string &RedisObject::asString() const {
  return std::get<std::string>(value_);
}

LinkedList &RedisObject::asList() {
  return *std::get<std::unique_ptr<LinkedList>>(value_);
}

const LinkedList &RedisObject::asList() const {
  return *std::get<std::unique_ptr<LinkedList>>(value_);
}

HashTable<std::string, std::string> &RedisObject::asHash() {
  return *std::get<std::unique_ptr<HashTable<std::string, std::string>>>(
      value_);
}

const HashTable<std::string, std::string> &RedisObject::asHash() const {
  return *std::get<std::unique_ptr<HashTable<std::string, std::string>>>(
      value_);
}

HashSet &RedisObject::asSet() {
  return *std::get<std::unique_ptr<HashSet>>(value_);
}

const HashSet &RedisObject::asSet() const {
  return *std::get<std::unique_ptr<HashSet>>(value_);
}

SortedSet &RedisObject::asSortedSet() {
  return *std::get<std::unique_ptr<SortedSet>>(value_);
}

const SortedSet &RedisObject::asSortedSet() const {
  return *std::get<std::unique_ptr<SortedSet>>(value_);
}

} // namespace redisdb
