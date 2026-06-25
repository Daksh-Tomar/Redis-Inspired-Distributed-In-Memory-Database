#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace redisdb {

template <typename K, typename V>
class HashTable {
public:
  HashTable() { buckets_.resize(16); }

  ~HashTable() = default;

  bool set(const K &key, const V &value) {
    checkRehash();
    return insertInternal(key, value, buckets_, size_);
  }

  bool set(const K &key, V &&value) {
    checkRehash();
    return insertInternal(key, std::move(value), buckets_, size_);
  }

  bool del(const K &key) {
    size_t idx = findIndex(key, buckets_);
    if (idx == NPOS) return false;

    buckets_[idx].active = false;
    buckets_[idx].key = K{};
    buckets_[idx].value = V{};
    size_--;
    buckets_[idx].tombstone = true;
    tombstoneCount_++;
    return true;
  }

  std::unique_ptr<HashTable<K, V>> clone() const {
    auto copy = std::make_unique<HashTable<K, V>>();
    copy->buckets_ = buckets_;
    copy->size_ = size_;
    copy->hasher_ = hasher_;
    return copy;
  }

  V *get(const K &key) {
    size_t idx = findIndex(key, buckets_);
    if (idx == NPOS) return nullptr;
    return &buckets_[idx].value;
  }

  const V *get(const K &key) const {
    size_t idx = findIndex(key, buckets_);
    if (idx == NPOS) return nullptr;
    return &buckets_[idx].value;
  }

  bool exists(const K &key) const {
    return findIndex(key, buckets_) != NPOS;
  }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  void clear() {
    buckets_.clear();
    buckets_.resize(16);
    size_ = 0;
  }

  class Iterator {
  public:
    Iterator(typename std::vector<struct Bucket>::iterator it,
             typename std::vector<struct Bucket>::iterator end)
        : it_(it), end_(end) {
      advanceToValid();
    }

    Iterator &operator++() {
      ++it_;
      advanceToValid();
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return it_ != other.it_;
    }
    std::pair<const K &, V &> operator*() {
      return {it_->key, it_->value};
    }

  private:
    void advanceToValid() {
      while (it_ != end_ && !it_->active) {
        ++it_;
      }
    }
    typename std::vector<struct Bucket>::iterator it_;
    typename std::vector<struct Bucket>::iterator end_;
  };

  Iterator begin() { return Iterator(buckets_.begin(), buckets_.end()); }
  Iterator end() { return Iterator(buckets_.end(), buckets_.end()); }

private:
  struct Bucket {
    K key;
    V value;
    bool active = false;
    bool tombstone = false;
  };

  static constexpr size_t NPOS = static_cast<size_t>(-1);

  std::vector<Bucket> buckets_;
  size_t size_ = 0;
  size_t tombstoneCount_ = 0;
  std::hash<K> hasher_;

  size_t hash(const K &key) const { return hasher_(key); }

  void checkRehash() {
    if (size_ >= buckets_.size() / 2) {
      rehash(buckets_.size() * 2);
    } else if (size_ + tombstoneCount_ >= buckets_.size() / 2) {
      rehash(buckets_.size());
    }
  }

  void rehash(size_t newCapacity) {
    std::vector<Bucket> newBuckets(newCapacity);
    size_t newSize = 0;

    for (auto &bucket : buckets_) {
      if (bucket.active) {
        insertInternal(bucket.key, std::move(bucket.value), newBuckets,
                       newSize);
      }
    }

    buckets_ = std::move(newBuckets);
    size_ = newSize;
    tombstoneCount_ = 0;
  }

  template <typename ValType>
  bool insertInternal(const K &key, ValType &&value,
                      std::vector<Bucket> &targetBuckets,
                      size_t &targetSize) {
    size_t h = hash(key);
    size_t idx = h % targetBuckets.size();
    size_t startIdx = idx;

    while (true) {
      if (!targetBuckets[idx].active && !targetBuckets[idx].tombstone) {
        targetBuckets[idx].key = key;
        targetBuckets[idx].value = std::forward<ValType>(value);
        targetBuckets[idx].active = true;
        targetSize++;
        return true;
      } else if (targetBuckets[idx].active &&
                 targetBuckets[idx].key == key) {
        targetBuckets[idx].value = std::forward<ValType>(value);
        return false;
      }

      idx = (idx + 1) % targetBuckets.size();
      if (idx == startIdx) {
        return false;
      }
    }
  }

  size_t findIndex(const K &key,
                   const std::vector<Bucket> &targetBuckets) const {
    if (targetBuckets.empty()) return NPOS;

    size_t h = hash(key);
    size_t idx = h % targetBuckets.size();
    size_t startIdx = idx;

    while (true) {
      if (!targetBuckets[idx].active && !targetBuckets[idx].tombstone) {
        return NPOS;
      } else if (targetBuckets[idx].active &&
                 targetBuckets[idx].key == key) {
        return idx;
      }

      idx = (idx + 1) % targetBuckets.size();
      if (idx == startIdx) {
        return NPOS;
      }
    }
  }
};

} // namespace redisdb
