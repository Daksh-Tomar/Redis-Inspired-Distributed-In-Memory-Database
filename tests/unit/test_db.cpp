#include "storage/db.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace redisdb;

class DatabaseTest : public ::testing::Test {
protected:
  Database db;
};

TEST_F(DatabaseTest, SetAndGetRoundTrip) {
  db.set("key1", "value1");
  auto val = db.get("key1");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), "value1");

  auto val2 = db.get("non_existent_key");
  EXPECT_FALSE(val2.has_value());
}

TEST_F(DatabaseTest, KeyDeletion) {
  db.set("key1", "value1");
  EXPECT_TRUE(db.exists("key1"));

  bool deleted = db.del("key1");
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(db.exists("key1"));

  bool deletedAgain = db.del("key1");
  EXPECT_FALSE(deletedAgain);
}

TEST_F(DatabaseTest, TypeChecking) {
  db.set("key1", "value1");
  EXPECT_EQ(db.type("key1"), "string");

  EXPECT_EQ(db.type("non_existent"), "none");
}

TEST_F(DatabaseTest, ExpireAndTTL) {
  db.set("key1", "value1");

  bool expireSet = db.setExpiry("key1", 50);
  EXPECT_TRUE(expireSet);

  int64_t ttl = db.pttl("key1");
  EXPECT_GT(ttl, 0);
  EXPECT_LE(ttl, 50);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto val = db.get("key1");
  EXPECT_FALSE(val.has_value());

  EXPECT_FALSE(db.exists("key1"));
}

TEST_F(DatabaseTest, ExpireKeyNotFound) {
  bool expireSet = db.setExpiry("not_found", 1000);
  EXPECT_FALSE(expireSet);
}

TEST_F(DatabaseTest, Persist) {
  db.set("key1", "value1");
  db.setExpiry("key1", 5000);

  EXPECT_GT(db.pttl("key1"), 0);

  bool persisted = db.persist("key1");
  EXPECT_TRUE(persisted);
  EXPECT_EQ(db.pttl("key1"), -1);
}
