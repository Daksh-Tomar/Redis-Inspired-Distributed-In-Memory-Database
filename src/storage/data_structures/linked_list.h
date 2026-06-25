#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace redisdb {

struct ListNode {
  std::string value;
  ListNode *prev = nullptr;
  ListNode *next = nullptr;

  explicit ListNode(std::string val) : value(std::move(val)) {}
};

class LinkedList {
public:
  LinkedList() = default;
  ~LinkedList();

  LinkedList(const LinkedList &) = delete;
  LinkedList &operator=(const LinkedList &) = delete;

  LinkedList(LinkedList &&other) noexcept;
  LinkedList &operator=(LinkedList &&other) noexcept;

  std::unique_ptr<LinkedList> clone() const;

  void pushFront(const std::string &value);
  void pushBack(const std::string &value);

  std::optional<std::string> popFront();
  std::optional<std::string> popBack();

  size_t size() const { return length_; }
  bool empty() const { return length_ == 0; }
  void clear();

  std::optional<std::string> get(int index) const;

  std::vector<std::string> range(int start, int stop) const;

private:
  ListNode *head_ = nullptr;
  ListNode *tail_ = nullptr;
  size_t length_ = 0;
};

} // namespace redisdb
