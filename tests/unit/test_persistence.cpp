#include "server/server.h"
#include "storage/aof_persistence.h"
#include "storage/rdb_persistence.h"
#include <cstdio>
#include <gtest/gtest.h>

using namespace redisdb;

class PersistenceTest : public ::testing::Test {
protected:
  void SetUp() override {
    databases.emplace_back();
    databases[0].set("key1", "value1");
    databases[0].set("key2", "value2");
    databases[0].setExpiry("key2", 100000);
  }

  void TearDown() override {
    std::remove("test_dump.rdb");
    std::remove("test_appendonly.aof");
  }

  std::vector<Database> databases;
};

TEST_F(PersistenceTest, RdbSaveAndLoad) {
  bool saved = RdbPersistence::saveToFile(databases, "test_dump.rdb");
  ASSERT_TRUE(saved);

  std::vector<Database> loadedDatabases;
  loadedDatabases.emplace_back();

  bool loaded =
      RdbPersistence::loadFromFile(loadedDatabases, "test_dump.rdb");
  ASSERT_TRUE(loaded);

  ASSERT_EQ(loadedDatabases.size(), databases.size());
  EXPECT_EQ(loadedDatabases[0].size(), databases[0].size());

  auto val1 = loadedDatabases[0].get("key1");
  ASSERT_TRUE(val1.has_value());
  EXPECT_EQ(val1.value(), "value1");

  auto val2 = loadedDatabases[0].get("key2");
  ASSERT_TRUE(val2.has_value());
  EXPECT_EQ(val2.value(), "value2");

  EXPECT_GT(loadedDatabases[0].pttl("key2"), 0);
}

TEST_F(PersistenceTest, AofRewriteAndLoad) {
  bool rewritten =
      AofPersistence::rewriteAof(databases, "test_appendonly.aof");
  ASSERT_TRUE(rewritten);

  std::vector<Database> loadedDatabases;
  loadedDatabases.emplace_back();

  Server mockServer;
  mockServer.initialize();

  bool loaded = AofPersistence::loadFromFile(
      loadedDatabases, "test_appendonly.aof", mockServer);
  ASSERT_TRUE(loaded);

  EXPECT_EQ(mockServer.getDatabase(0).size(), databases[0].size());

  auto val1 = mockServer.getDatabase(0).get("key1");
  ASSERT_TRUE(val1.has_value());
  EXPECT_EQ(val1.value(), "value1");

  auto val2 = mockServer.getDatabase(0).get("key2");
  ASSERT_TRUE(val2.has_value());
  EXPECT_EQ(val2.value(), "value2");
}
