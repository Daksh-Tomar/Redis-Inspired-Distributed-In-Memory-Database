#pragma once

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace redisdb {

constexpr int SKIPLIST_MAXLEVEL = 32;
constexpr double SKIPLIST_P = 0.25;

struct SkipListLevel {
  struct SkipListNode *forward = nullptr;
  unsigned int span = 0;
};

struct SkipListNode {
  std::string member;
  double score;
  SkipListNode *backward;
  std::vector<SkipListLevel> level;

  SkipListNode(int levelCount, double score, std::string member)
      : member(std::move(member)),
        score(score),
        backward(nullptr),
        level(levelCount) {}
};

class SkipList {
public:
  SkipList();
  ~SkipList();

  SkipList(const SkipList &) = delete;
  SkipList &operator=(const SkipList &) = delete;

  SkipList(SkipList &&other) noexcept;
  SkipList &operator=(SkipList &&other) noexcept;

  std::unique_ptr<SkipList> clone() const;

  SkipListNode *insert(double score, const std::string &member);
  int deleteNode(double score, const std::string &member);

  unsigned long getRank(double score, const std::string &member) const;
  SkipListNode *getElementByRank(unsigned long rank) const;

  SkipListNode *getFirstInRange(double min, double max) const;
  SkipListNode *getLastInRange(double min, double max) const;

  SkipListNode *head() const { return head_; }
  SkipListNode *tail() const { return tail_; }
  unsigned long length() const { return length_; }
  int level() const { return level_; }

private:
  int randomLevel();
  void deleteNodeInternal(SkipListNode *x, SkipListNode **update);

  SkipListNode *head_;
  SkipListNode *tail_;
  unsigned long length_;
  int level_;

  std::mt19937 rng_;
  std::uniform_real_distribution<double> dist_;
};

} // namespace redisdb
