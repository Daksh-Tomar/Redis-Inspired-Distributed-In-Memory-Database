#include "storage/data_structures/sorted_set.h"

#include <algorithm>

namespace redisdb {

int SortedSet::add(const std::string &member, double score) {
  const double *oldScorePtr = dict_.get(member);

  if (oldScorePtr != nullptr) {
    double oldScore = *oldScorePtr;
    if (oldScore == score) {
      return 0;
    }

    zsl_.deleteNode(oldScore, member);
    zsl_.insert(score, member);
    dict_.set(member, score);
    return 0;
  }

  dict_.set(member, score);
  zsl_.insert(score, member);
  return 1;
}

int SortedSet::remove(const std::string &member) {
  const double *scorePtr = dict_.get(member);

  if (scorePtr == nullptr) {
    return 0;
  }

  double score = *scorePtr;
  zsl_.deleteNode(score, member);
  dict_.del(member);
  return 1;
}

std::optional<double> SortedSet::getScore(const std::string &member) const {
  const double *scorePtr = dict_.get(member);
  if (scorePtr != nullptr) {
    return *scorePtr;
  }
  return std::nullopt;
}

unsigned long SortedSet::getRank(const std::string &member) const {
  const double *scorePtr = dict_.get(member);
  if (scorePtr == nullptr) {
    return 0;
  }

  unsigned long rank = zsl_.getRank(*scorePtr, member);
  return rank > 0 ? rank - 1 : 0;
}

std::vector<std::pair<std::string, double>>
SortedSet::rangeByRank(long start, long stop) const {
  std::vector<std::pair<std::string, double>> result;

  long len = static_cast<long>(size());

  if (start < 0) start = len + start;
  if (stop < 0) stop = len + stop;

  if (start < 0) start = 0;

  if (start > stop || start >= len) {
    return result;
  }

  if (stop >= len) stop = len - 1;

  long rangeLen = stop - start + 1;
  result.reserve(rangeLen);

  SkipListNode *node = zsl_.getElementByRank(start + 1);

  while (rangeLen-- > 0 && node != nullptr) {
    result.push_back({node->member, node->score});
    node = node->level[0].forward;
  }

  return result;
}

} // namespace redisdb
