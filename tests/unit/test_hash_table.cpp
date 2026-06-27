#include "storage/data_structures/hash_table.h"
#include <gtest/gtest.h>
#include <string>

using namespace redisdb;

TEST(HashTableTest, SetAndGet) {
  HashTable<std::string, std::string> ht;

  EXPECT_TRUE(ht.empty());
  EXPECT_EQ(ht.size(), 0);

  bool isNew = ht.set("key1", "value1");
  EXPECT_TRUE(isNew);
  EXPECT_EQ(ht.size(), 1);

  auto val = ht.get("key1");
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, "value1");

  bool isNew2 = ht.set("key1", "value2");
  EXPECT_FALSE(isNew2);
  EXPECT_EQ(ht.size(), 1);

  val = ht.get("key1");
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, "value2");
}

TEST(HashTableTest, Deletion) {
  HashTable<std::string, std::string> ht;
  ht.set("key1", "value1");
  EXPECT_TRUE(ht.exists("key1"));

  bool deleted = ht.del("key1");
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(ht.exists("key1"));
  EXPECT_EQ(ht.size(), 0);

  bool deletedAgain = ht.del("key1");
  EXPECT_FALSE(deletedAgain);
}

TEST(HashTableTest, Rehashing) {
  HashTable<std::string, std::string> ht;

  for (int i = 0; i < 100; ++i) {
    ht.set("key" + std::to_string(i), "value" + std::to_string(i));
  }

  EXPECT_EQ(ht.size(), 100);

  for (int i = 0; i < 100; ++i) {
    auto val = ht.get("key" + std::to_string(i));
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "value" + std::to_string(i));
  }
}

TEST(HashTableTest, Iteration) {
  HashTable<std::string, std::string> ht;
  ht.set("a", "1");
  ht.set("b", "2");
  ht.set("c", "3");

  int count = 0;
  for (auto it : ht) {
    count++;
    EXPECT_TRUE(it.first == "a" || it.first == "b" || it.first == "c");
    EXPECT_TRUE(it.second == "1" || it.second == "2" || it.second == "3");
  }
  EXPECT_EQ(count, 3);
}
