#include "storage/data_structures/skip_list.h"
#include <gtest/gtest.h>

using namespace redisdb;

TEST(SkipListTest, InsertAndRank) {
  SkipList sl;

  sl.insert(10.0, "member1");
  sl.insert(20.0, "member2");
  sl.insert(15.0, "member3");

  EXPECT_EQ(sl.length(), 3u);

  EXPECT_EQ(sl.getRank(10.0, "member1"), 1);
  EXPECT_EQ(sl.getRank(15.0, "member3"), 2);
  EXPECT_EQ(sl.getRank(20.0, "member2"), 3);

  auto node = sl.getElementByRank(2);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->member, "member3");
}

TEST(SkipListTest, Deletion) {
  SkipList sl;

  sl.insert(10.0, "member1");
  sl.insert(20.0, "member2");

  int deleted = sl.deleteNode(10.0, "member1");
  EXPECT_EQ(deleted, 1);
  EXPECT_EQ(sl.length(), 1u);

  EXPECT_EQ(sl.getRank(10.0, "member1"), 0);

  int deletedAgain = sl.deleteNode(10.0, "member1");
  EXPECT_EQ(deletedAgain, 0);
}

TEST(SkipListTest, RangeQueries) {
  SkipList sl;

  sl.insert(10.0, "member1");
  sl.insert(20.0, "member2");
  sl.insert(30.0, "member3");
  sl.insert(40.0, "member4");

  auto first = sl.getFirstInRange(15.0, 35.0);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->member, "member2");

  auto last = sl.getLastInRange(15.0, 35.0);
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(last->member, "member3");
}
