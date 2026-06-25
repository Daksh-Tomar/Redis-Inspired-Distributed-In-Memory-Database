#pragma once

#include "storage/data_structures/hash_table.h"

#include <memory>
#include <string>
#include <vector>

namespace redisdb {

struct EmptyStruct {};

class HashSet {
public:
  HashSet() = default;
  ~HashSet() = default;

  HashSet(const HashSet &) = delete;
  HashSet &operator=(const HashSet &) = delete;

  HashSet(HashSet &&) noexcept = default;
  HashSet &operator=(HashSet &&) noexcept = default;

  std::unique_ptr<HashSet> clone() const {
    auto copy = std::make_unique<HashSet>();
    copy->table_ = std::move(*table_.clone());
    return copy;
  }

  bool add(const std::string &member) {
    return table_.set(member, EmptyStruct{});
  }

  bool remove(const std::string &member) { return table_.del(member); }

  bool contains(const std::string &member) const {
    return table_.exists(member);
  }

  size_t size() const { return table_.size(); }

  bool empty() const { return table_.empty(); }

  std::vector<std::string> members() {
    std::vector<std::string> result;
    result.reserve(size());
    for (auto kv : table_) {
      result.push_back(kv.first);
    }
    return result;
  }

private:
  HashTable<std::string, EmptyStruct> table_;
};

} // namespace redisdb
