#include "storage/data_structures/linked_list.h"
#include <gtest/gtest.h>

using namespace redisdb;

TEST(LinkedListTest, PushAndPop) {
  LinkedList ll;
  EXPECT_TRUE(ll.empty());

  ll.pushBack("value1");
  ll.pushFront("value2");

  EXPECT_EQ(ll.size(), 2);

  auto valFront = ll.popFront();
  ASSERT_TRUE(valFront.has_value());
  EXPECT_EQ(valFront.value(), "value2");

  auto valBack = ll.popBack();
  ASSERT_TRUE(valBack.has_value());
  EXPECT_EQ(valBack.value(), "value1");

  EXPECT_TRUE(ll.empty());
}

TEST(LinkedListTest, GetAndRange) {
  LinkedList ll;
  ll.pushBack("v1");
  ll.pushBack("v2");
  ll.pushBack("v3");

  auto val0 = ll.get(0);
  ASSERT_TRUE(val0.has_value());
  EXPECT_EQ(val0.value(), "v1");

  auto valNeg1 = ll.get(-1);
  ASSERT_TRUE(valNeg1.has_value());
  EXPECT_EQ(valNeg1.value(), "v3");

  auto r = ll.range(0, -1);
  ASSERT_EQ(r.size(), 3);
  EXPECT_EQ(r[0], "v1");
  EXPECT_EQ(r[2], "v3");
}
