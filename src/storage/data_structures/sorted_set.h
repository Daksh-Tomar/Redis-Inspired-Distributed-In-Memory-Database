#pragma once

#include "storage/data_structures/hash_table.h"
#include "storage/data_structures/skip_list.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace redisdb {

class SortedSet {
public:
  SortedSet() = default;
  ~SortedSet() = default;

  SortedSet(const SortedSet &) = delete;
  SortedSet &operator=(const SortedSet &) = delete;

  SortedSet(SortedSet &&) noexcept = default;
  SortedSet &operator=(SortedSet &&) noexcept = default;

  std::unique_ptr<SortedSet> clone() const {
    auto copy = std::make_unique<SortedSet>();
    copy->dict_ = std::move(*dict_.clone());
    copy->zsl_ = std::move(*zsl_.clone());
    return copy;
  }

  int add(const std::string &member, double score);
  int remove(const std::string &member);

  std::optional<double> getScore(const std::string &member) const;
  unsigned long getRank(const std::string &member) const;

  size_t size() const { return dict_.size(); }
  bool empty() const { return dict_.empty(); }

  std::vector<std::pair<std::string, double>> rangeByRank(long start,
                                                          long stop) const;

private:
  HashTable<std::string, double> dict_;
  SkipList zsl_;
};

} // namespace redisdb
