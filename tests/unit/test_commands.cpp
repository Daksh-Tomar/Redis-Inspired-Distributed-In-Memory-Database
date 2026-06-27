#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"
#include <gtest/gtest.h>

using namespace redisdb;

class CommandsTest : public ::testing::Test {
protected:
  void SetUp() override { server.initialize(); }

  std::string executeCommand(const std::vector<std::string> &argsStr) {
    Client client(INVALID_SOCKET);
    std::vector<RespValue> args;
    for (const auto &arg : argsStr) {
      args.push_back(RespValue::bulkString(arg));
    }

    auto cmd = server.commandRegistry().lookupCommand(argsStr[0]);
    if (cmd) {
      if ((cmd->arity > 0 &&
           args.size() != static_cast<size_t>(cmd->arity)) ||
          (cmd->arity < 0 &&
           args.size() < static_cast<size_t>(-cmd->arity))) {
        client.addReply(RespSerializer::error(
            "wrong number of arguments for '" + argsStr[0] + "' command"));
      } else {
        cmd->handler(server, client, args);
      }
    } else {
      client.addReply(
          RespSerializer::error("unknown command '" + argsStr[0] + "'"));
    }
    return client.getOutputBuffer();
  }

  Server server;
};

TEST_F(CommandsTest, PingCommand) {
  std::string response = executeCommand({"PING"});
  EXPECT_EQ(response, "+PONG\r\n");

  std::string responseWithArg = executeCommand({"PING", "hello"});
  EXPECT_EQ(responseWithArg, "$5\r\nhello\r\n");
}

TEST_F(CommandsTest, EchoCommand) {
  std::string response = executeCommand({"ECHO", "hello world"});
  EXPECT_EQ(response, "$11\r\nhello world\r\n");

  std::string errorResponse = executeCommand({"ECHO"});
  EXPECT_EQ(errorResponse,
            "-ERR wrong number of arguments for 'ECHO' command\r\n");
}

TEST_F(CommandsTest, SetAndGetCommand) {
  std::string setResp = executeCommand({"SET", "mykey", "myval"});
  EXPECT_EQ(setResp, "+OK\r\n");

  std::string getResp = executeCommand({"GET", "mykey"});
  EXPECT_EQ(getResp, "$5\r\nmyval\r\n");

  std::string getNotFound = executeCommand({"GET", "nonexistent"});
  EXPECT_EQ(getNotFound, "$-1\r\n");
}

TEST_F(CommandsTest, DelCommand) {
  executeCommand({"SET", "key1", "val1"});
  executeCommand({"SET", "key2", "val2"});

  std::string delResp = executeCommand({"DEL", "key1", "key2", "key3"});
  EXPECT_EQ(delResp, ":2\r\n");

  std::string getResp = executeCommand({"GET", "key1"});
  EXPECT_EQ(getResp, "$-1\r\n");
}

TEST_F(CommandsTest, ExistsCommand) {
  executeCommand({"SET", "key1", "val1"});

  std::string existsResp = executeCommand({"EXISTS", "key1", "key2"});
  EXPECT_EQ(existsResp, ":1\r\n");
}

TEST_F(CommandsTest, SetFlagsCommand) {
  std::string setNx1 = executeCommand({"SET", "keynx", "val1", "NX"});
  EXPECT_EQ(setNx1, "+OK\r\n");
  std::string setNx2 = executeCommand({"SET", "keynx", "val2", "NX"});
  EXPECT_EQ(setNx2, "$-1\r\n");

  std::string setXx1 = executeCommand({"SET", "keyxx", "val1", "XX"});
  EXPECT_EQ(setXx1, "$-1\r\n");
  executeCommand({"SET", "keyxx", "val1"});
  std::string setXx2 = executeCommand({"SET", "keyxx", "val2", "XX"});
  EXPECT_EQ(setXx2, "+OK\r\n");

  std::string setEx = executeCommand({"SET", "keyex", "val", "EX", "10"});
  EXPECT_EQ(setEx, "+OK\r\n");
}

TEST_F(CommandsTest, IncrDecrOverflowCommand) {
  executeCommand({"SET", "counter", "9223372036854775806"});
  std::string incrResp1 = executeCommand({"INCR", "counter"});
  EXPECT_EQ(incrResp1, ":9223372036854775807\r\n");

  std::string incrResp2 = executeCommand({"INCR", "counter"});
  EXPECT_EQ(incrResp2, "-ERR value is not an integer or out of range\r\n");

  executeCommand({"SET", "counter2", "-9223372036854775807"});
  std::string decrResp1 = executeCommand({"DECR", "counter2"});
  EXPECT_EQ(decrResp1, ":-9223372036854775808\r\n");

  std::string decrResp2 = executeCommand({"DECR", "counter2"});
  EXPECT_EQ(decrResp2, "-ERR value is not an integer or out of range\r\n");
}

TEST_F(CommandsTest, ListCommands) {
  std::string rpushResp = executeCommand({"RPUSH", "mylist", "a", "b"});
  EXPECT_EQ(rpushResp, ":2\r\n");

  std::string lpushResp = executeCommand({"LPUSH", "mylist", "first"});
  EXPECT_EQ(lpushResp, ":3\r\n");

  std::string llenResp = executeCommand({"LLEN", "mylist"});
  EXPECT_EQ(llenResp, ":3\r\n");

  std::string lrangeResp = executeCommand({"LRANGE", "mylist", "0", "-1"});
  EXPECT_EQ(lrangeResp, "*3\r\n$5\r\nfirst\r\n$1\r\na\r\n$1\r\nb\r\n");
}

TEST_F(CommandsTest, HashCommands) {
  std::string hsetResp =
      executeCommand({"HSET", "myhash", "field1", "val1", "field2", "val2"});
  EXPECT_EQ(hsetResp, ":2\r\n");

  std::string hgetResp = executeCommand({"HGET", "myhash", "field1"});
  EXPECT_EQ(hgetResp, "$4\r\nval1\r\n");

  std::string hgetallResp = executeCommand({"HGETALL", "myhash"});
  EXPECT_EQ(hgetallResp.substr(0, 4), "*4\r\n");
}

TEST_F(CommandsTest, SetCommands) {
  std::string saddResp =
      executeCommand({"SADD", "myset", "member1", "member2", "member1"});
  EXPECT_EQ(saddResp, ":2\r\n");

  std::string sismemberResp1 =
      executeCommand({"SISMEMBER", "myset", "member1"});
  EXPECT_EQ(sismemberResp1, ":1\r\n");

  std::string sismemberResp2 =
      executeCommand({"SISMEMBER", "myset", "nonexistent"});
  EXPECT_EQ(sismemberResp2, ":0\r\n");

  std::string smembersResp = executeCommand({"SMEMBERS", "myset"});
  EXPECT_EQ(smembersResp.substr(0, 4), "*2\r\n");
}

TEST_F(CommandsTest, SortedSetCommands) {
  std::string zaddResp = executeCommand(
      {"ZADD", "myzset", "10", "member1", "20", "member2", "15", "member3"});
  EXPECT_EQ(zaddResp, ":3\r\n");

  std::string zrankResp = executeCommand({"ZRANK", "myzset", "member3"});
  EXPECT_EQ(zrankResp, ":1\r\n");

  std::string zscoreResp = executeCommand({"ZSCORE", "myzset", "member1"});
  EXPECT_EQ(zscoreResp, "$2\r\n10\r\n");

  std::string zrangeResp = executeCommand({"ZRANGE", "myzset", "0", "-1"});
  EXPECT_EQ(zrangeResp,
            "*3\r\n$7\r\nmember1\r\n$7\r\nmember3\r\n$7\r\nmember2\r\n");
}
