#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace redisdb;

class AdvancedFeaturesTest : public ::testing::Test {
protected:
  void SetUp() override { server.initialize(); }

  std::string executeCommand(Client &client,
                             const std::vector<std::string> &argsStr) {
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
      } else if (client.hasFlag(CLIENT_MULTI) && argsStr[0] != "EXEC" &&
                 argsStr[0] != "DISCARD" && argsStr[0] != "MULTI" &&
                 argsStr[0] != "WATCH") {
        client.transactionQueue().push_back(args);
        client.addReply(RespSerializer::simpleString("QUEUED"));
      } else {
        cmd->handler(server, client, args);
      }
    } else {
      client.addReply(
          RespSerializer::error("unknown command '" + argsStr[0] + "'"));
    }
    std::string output = client.getOutputBuffer();
    return output;
  }

  std::string executeCommandSingle(const std::vector<std::string> &argsStr) {
    Client client(INVALID_SOCKET);
    return executeCommand(client, argsStr);
  }

  Server server;
};

TEST_F(AdvancedFeaturesTest, MultiExecHappyPath) {
  Client client(INVALID_SOCKET);

  std::string multiResp = executeCommand(client, {"MULTI"});
  EXPECT_EQ(multiResp, "+OK\r\n");
  client.clearOutputBuffer();

  std::string setResp = executeCommand(client, {"SET", "txn_key", "txn_val"});
  EXPECT_EQ(setResp, "+QUEUED\r\n");
  client.clearOutputBuffer();

  std::string getResp = executeCommand(client, {"GET", "txn_key"});
  EXPECT_EQ(getResp, "+QUEUED\r\n");
  client.clearOutputBuffer();

  std::string execResp = executeCommand(client, {"EXEC"});
  EXPECT_EQ(execResp, "*2\r\n+OK\r\n$7\r\ntxn_val\r\n");
}

TEST_F(AdvancedFeaturesTest, WatchDirtyDetection) {
  Client client(INVALID_SOCKET);

  executeCommandSingle({"SET", "watched_key", "val1"});

  std::string watchResp = executeCommand(client, {"WATCH", "watched_key"});
  EXPECT_EQ(watchResp, "+OK\r\n");
  client.clearOutputBuffer();

  executeCommand(client, {"MULTI"});
  client.clearOutputBuffer();

  executeCommand(client, {"SET", "watched_key", "val2"});
  client.clearOutputBuffer();

  executeCommandSingle({"SET", "watched_key", "val3"});

  std::string execResp = executeCommand(client, {"EXEC"});
  EXPECT_EQ(execResp, "*-1\r\n");
}

TEST_F(AdvancedFeaturesTest, PubSubMessageDelivery) {
  Client subClient(INVALID_SOCKET);

  std::string subResp = executeCommand(subClient, {"SUBSCRIBE", "channel1"});
  EXPECT_EQ(subResp, "*3\r\n$9\r\nsubscribe\r\n$8\r\nchannel1\r\n:1\r\n");
  subClient.clearOutputBuffer();

  Client pubClient(INVALID_SOCKET);
  std::string pubResp =
      executeCommand(pubClient, {"PUBLISH", "channel1", "hello_pubsub"});
  EXPECT_EQ(pubResp, ":1\r\n");

  std::string msgResp = subClient.getOutputBuffer();
  EXPECT_EQ(msgResp, "*3\r\n$7\r\nmessage\r\n$8\r\nchannel1\r\n$12\r\nhello_pubsub\r\n");
}

TEST_F(AdvancedFeaturesTest, BlpopUnblocking) {
  Client blockedClient(INVALID_SOCKET);

  std::string blpopResp = executeCommand(blockedClient, {"BLPOP", "b_list", "1"});
  EXPECT_EQ(blpopResp, "");
  blockedClient.clearOutputBuffer();
  EXPECT_TRUE(blockedClient.hasFlag(CLIENT_BLOCKED));

  Client pusherClient(INVALID_SOCKET);
  std::string rpushResp =
      executeCommand(pusherClient, {"RPUSH", "b_list", "b_val"});
  EXPECT_EQ(rpushResp, ":1\r\n");

  EXPECT_FALSE(blockedClient.hasFlag(CLIENT_BLOCKED));
  std::string unblockedResp = blockedClient.getOutputBuffer();
  EXPECT_EQ(unblockedResp, "*2\r\n$6\r\nb_list\r\n$5\r\nb_val\r\n");
}

TEST_F(AdvancedFeaturesTest, ActiveExpiryServerCron) {
  executeCommandSingle({"SET", "exp_key", "val", "PX", "1"});

  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  server.serverCron();

  std::string getResp = executeCommandSingle({"GET", "exp_key"});
  EXPECT_EQ(getResp, "$-1\r\n");

  EXPECT_EQ(server.getDatabase(0).size(), 0);
}
