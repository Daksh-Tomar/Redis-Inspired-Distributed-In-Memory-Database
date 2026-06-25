#include "storage/data_structures/skip_list.h"

#include <random>

namespace redisdb {

SkipList::SkipList()
    : length_(0), level_(1), rng_(std::random_device{}()), dist_(0.0, 1.0) {
  head_ = new SkipListNode(SKIPLIST_MAXLEVEL, 0.0, "");
  for (int j = 0; j < SKIPLIST_MAXLEVEL; j++) {
    head_->level[j].forward = nullptr;
    head_->level[j].span = 0;
  }
  head_->backward = nullptr;
  tail_ = nullptr;
}

SkipList::~SkipList() {
  SkipListNode *node = head_->level[0].forward;
  SkipListNode *next;

  delete head_;

  while (node) {
    next = node->level[0].forward;
    delete node;
    node = next;
  }
}

SkipList::SkipList(SkipList &&other) noexcept
    : head_(other.head_),
      tail_(other.tail_),
      length_(other.length_),
      level_(other.level_),
      rng_(std::move(other.rng_)),
      dist_(std::move(other.dist_)) {
  other.head_ = new SkipListNode(SKIPLIST_MAXLEVEL, 0.0, "");
  other.tail_ = nullptr;
  other.length_ = 0;
  other.level_ = 1;
}

SkipList &SkipList::operator=(SkipList &&other) noexcept {
  if (this != &other) {
    SkipListNode *node = head_->level[0].forward;
    SkipListNode *next;
    while (node) {
      next = node->level[0].forward;
      delete node;
      node = next;
    }
    delete head_;

    head_ = other.head_;
    tail_ = other.tail_;
    length_ = other.length_;
    level_ = other.level_;
    rng_ = std::move(other.rng_);
    dist_ = std::move(other.dist_);

    other.head_ = new SkipListNode(SKIPLIST_MAXLEVEL, 0.0, "");
    other.tail_ = nullptr;
    other.length_ = 0;
    other.level_ = 1;
  }
  return *this;
}

std::unique_ptr<SkipList> SkipList::clone() const {
  auto copy = std::make_unique<SkipList>();
  SkipListNode *curr = head_->level[0].forward;
  while (curr) {
    copy->insert(curr->score, curr->member);
    curr = curr->level[0].forward;
  }
  return copy;
}

int SkipList::randomLevel() {
  int level = 1;
  while (dist_(rng_) < SKIPLIST_P && level < SKIPLIST_MAXLEVEL) {
    level++;
  }
  return level;
}

SkipListNode *SkipList::insert(double score, const std::string &member) {
  SkipListNode *update[SKIPLIST_MAXLEVEL];
  unsigned int rank[SKIPLIST_MAXLEVEL];
  SkipListNode *x = head_;

  for (int i = level_ - 1; i >= 0; i--) {
    rank[i] = i == (level_ - 1) ? 0 : rank[i + 1];

    while (x->level[i].forward &&
           (x->level[i].forward->score < score ||
            (x->level[i].forward->score == score &&
             x->level[i].forward->member.compare(member) < 0))) {
      rank[i] += x->level[i].span;
      x = x->level[i].forward;
    }
    update[i] = x;
  }

  int level = randomLevel();

  if (level > level_) {
    for (int i = level_; i < level; i++) {
      rank[i] = 0;
      update[i] = head_;
      update[i]->level[i].span = length_;
    }
    level_ = level;
  }

  x = new SkipListNode(level, score, member);

  for (int i = 0; i < level; i++) {
    x->level[i].forward = update[i]->level[i].forward;
    update[i]->level[i].forward = x;

    x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
    update[i]->level[i].span = (rank[0] - rank[i]) + 1;
  }

  for (int i = level; i < level_; i++) {
    update[i]->level[i].span++;
  }

  x->backward = (update[0] == head_) ? nullptr : update[0];

  if (x->level[0].forward) {
    x->level[0].forward->backward = x;
  } else {
    tail_ = x;
  }

  length_++;
  return x;
}

void SkipList::deleteNodeInternal(SkipListNode *x, SkipListNode **update) {
  for (int i = 0; i < level_; i++) {
    if (update[i]->level[i].forward == x) {
      update[i]->level[i].span += x->level[i].span - 1;
      update[i]->level[i].forward = x->level[i].forward;
    } else {
      update[i]->level[i].span -= 1;
    }
  }

  if (x->level[0].forward) {
    x->level[0].forward->backward = x->backward;
  } else {
    tail_ = x->backward;
  }

  while (level_ > 1 && head_->level[level_ - 1].forward == nullptr) {
    level_--;
  }

  length_--;
}

int SkipList::deleteNode(double score, const std::string &member) {
  SkipListNode *update[SKIPLIST_MAXLEVEL];
  SkipListNode *x = head_;

  for (int i = level_ - 1; i >= 0; i--) {
    while (x->level[i].forward &&
           (x->level[i].forward->score < score ||
            (x->level[i].forward->score == score &&
             x->level[i].forward->member.compare(member) < 0))) {
      x = x->level[i].forward;
    }
    update[i] = x;
  }

  x = x->level[0].forward;

  if (x && score == x->score && x->member == member) {
    deleteNodeInternal(x, update);
    delete x;
    return 1;
  }

  return 0;
}

unsigned long SkipList::getRank(double score,
                                const std::string &member) const {
  unsigned long rank = 0;
  SkipListNode *x = head_;

  for (int i = level_ - 1; i >= 0; i--) {
    while (x->level[i].forward &&
           (x->level[i].forward->score < score ||
            (x->level[i].forward->score == score &&
             x->level[i].forward->member.compare(member) <= 0))) {
      rank += x->level[i].span;
      x = x->level[i].forward;
    }

    if (x->member == member) {
      return rank;
    }
  }

  return 0;
}

SkipListNode *SkipList::getElementByRank(unsigned long rank) const {
  unsigned long traversed = 0;
  SkipListNode *x = head_;

  for (int i = level_ - 1; i >= 0; i--) {
    while (x->level[i].forward &&
           (traversed + x->level[i].span) <= rank) {
      traversed += x->level[i].span;
      x = x->level[i].forward;
    }
    if (traversed == rank) {
      return x;
    }
  }

  return nullptr;
}

SkipListNode *SkipList::getFirstInRange(double min, double max) const {
  SkipListNode *x = head_;
  for (int i = level_ - 1; i >= 0; i--) {
    while (x->level[i].forward && x->level[i].forward->score < min) {
      x = x->level[i].forward;
    }
  }
  x = x->level[0].forward;
  if (!x || x->score > max) {
    return nullptr;
  }
  return x;
}

SkipListNode *SkipList::getLastInRange(double min, double max) const {
  SkipListNode *x = head_;
  for (int i = level_ - 1; i >= 0; i--) {
    while (x->level[i].forward && x->level[i].forward->score <= max) {
      x = x->level[i].forward;
    }
  }
  if (!x || x == head_ || x->score < min) {
    return nullptr;
  }
  return x;
}

} // namespace redisdb
