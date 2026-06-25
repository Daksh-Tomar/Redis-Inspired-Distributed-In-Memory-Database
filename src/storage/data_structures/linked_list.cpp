#include "storage/data_structures/linked_list.h"

#include <algorithm>

namespace redisdb {

LinkedList::~LinkedList() { clear(); }

LinkedList::LinkedList(LinkedList &&other) noexcept
    : head_(other.head_), tail_(other.tail_), length_(other.length_) {
  other.head_ = nullptr;
  other.tail_ = nullptr;
  other.length_ = 0;
}

LinkedList &LinkedList::operator=(LinkedList &&other) noexcept {
  if (this != &other) {
    clear();
    head_ = other.head_;
    tail_ = other.tail_;
    length_ = other.length_;

    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.length_ = 0;
  }
  return *this;
}

std::unique_ptr<LinkedList> LinkedList::clone() const {
  auto copy = std::make_unique<LinkedList>();
  ListNode *curr = head_;
  while (curr) {
    copy->pushBack(curr->value);
    curr = curr->next;
  }
  return copy;
}

void LinkedList::pushFront(const std::string &value) {
  auto *node = new ListNode(value);
  if (length_ == 0) {
    head_ = tail_ = node;
  } else {
    node->next = head_;
    head_->prev = node;
    head_ = node;
  }
  length_++;
}

void LinkedList::pushBack(const std::string &value) {
  auto *node = new ListNode(value);
  if (length_ == 0) {
    head_ = tail_ = node;
  } else {
    node->prev = tail_;
    tail_->next = node;
    tail_ = node;
  }
  length_++;
}

std::optional<std::string> LinkedList::popFront() {
  if (length_ == 0) return std::nullopt;

  ListNode *node = head_;
  std::string val = std::move(node->value);

  head_ = head_->next;
  if (head_) {
    head_->prev = nullptr;
  } else {
    tail_ = nullptr;
  }

  delete node;
  length_--;
  return val;
}

std::optional<std::string> LinkedList::popBack() {
  if (length_ == 0) return std::nullopt;

  ListNode *node = tail_;
  std::string val = std::move(node->value);

  tail_ = tail_->prev;
  if (tail_) {
    tail_->next = nullptr;
  } else {
    head_ = nullptr;
  }

  delete node;
  length_--;
  return val;
}

void LinkedList::clear() {
  ListNode *current = head_;
  while (current) {
    ListNode *next = current->next;
    delete current;
    current = next;
  }
  head_ = tail_ = nullptr;
  length_ = 0;
}

std::optional<std::string> LinkedList::get(int index) const {
  if (length_ == 0) return std::nullopt;

  if (index < 0) {
    index = static_cast<int>(length_) + index;
  }

  if (index < 0 || index >= static_cast<int>(length_)) {
    return std::nullopt;
  }

  ListNode *current;
  if (index > static_cast<int>(length_ / 2)) {
    current = tail_;
    for (int i = static_cast<int>(length_) - 1; i > index; i--) {
      current = current->prev;
    }
  } else {
    current = head_;
    for (int i = 0; i < index; i++) {
      current = current->next;
    }
  }

  return current->value;
}

std::vector<std::string> LinkedList::range(int start, int stop) const {
  std::vector<std::string> result;
  if (length_ == 0) return result;

  if (start < 0) start = static_cast<int>(length_) + start;
  if (stop < 0) stop = static_cast<int>(length_) + stop;

  if (start < 0) start = 0;
  if (stop < 0) stop = 0;
  if (start >= static_cast<int>(length_)) return result;
  if (stop >= static_cast<int>(length_))
    stop = static_cast<int>(length_) - 1;

  if (start > stop) return result;

  ListNode *current = head_;
  for (int i = 0; i < start; i++) {
    current = current->next;
  }

  int count = stop - start + 1;
  result.reserve(count);
  while (count-- > 0 && current) {
    result.push_back(current->value);
    current = current->next;
  }

  return result;
}

} // namespace redisdb
