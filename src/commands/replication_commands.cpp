#include "commands/replication_commands.h"
#include "commands/command_registry.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"
#include <iostream>


namespace redisdb {

void registerReplicationCommands(CommandRegistry &registry) {
  registry.registerCommand(
      "replicaof",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        std::string host = args[1].asString();
        std::string portStr = args[2].asString();

        if (host == "NO" && portStr == "ONE") {
          server.disconnectFromMaster();
          client.addReply(RespSerializer::simpleString("OK"));
          return;
        }

        try {
          int port = std::stoi(portStr);
          server.setReplicationMaster(host, port);
          client.addReply(RespSerializer::simpleString("OK"));
        } catch (...) {
          client.addReply(RespSerializer::error("ERR invalid port"));
        }
      },
      3, CMD_ADMIN,
      "Make the server a replica of another instance, or promote it as "
      "master.");

  registry.registerCommand(
      "replconf",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        (void)server;
        // Just ack OK for now. In real Redis this tracks listening port,
        // capabilties, and ACK offsets.
        if (args.size() >= 3 && args[1].asString() == "ACK") {
          return;
        }
        client.addReply(RespSerializer::simpleString("OK"));
      },
      -3, CMD_ADMIN, "Used by replicas to configure the replication link");

  registry.registerCommand(
      "psync",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        std::string replId = args[1].asString();
        std::string offsetStr = args[2].asString();

        std::cout << "[Replication] Received PSYNC " << replId << " "
                  << offsetStr << std::endl;

        if (server.getReplId().empty()) {
          server.generateReplId();
        }

        if (replId == "?" || replId != server.getReplId()) {
          std::string reply = "+FULLRESYNC " + server.getReplId() + " " +
                              std::to_string(server.getReplOffset()) + "\r\n";
          client.addReply(reply);

          std::string emptyRdb = "REDIS0009\377";
          client.addReply(RespSerializer::bulkString(emptyRdb));

          server.addReplica(&client);
        } else {
          client.addReply(RespSerializer::simpleString("CONTINUE"));
          server.addReplica(&client);
          // In real Redis we would send the backlog here
        }
      },
      3, CMD_ADMIN, "Replication synchronization command");

  registry.registerCommand(
      "wait",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        client.addReply(RespSerializer::integer(server.getReplicaCount()));
      },
      3, CMD_READONLY,
      "Wait for the synchronous replication of all the write commands sent in "
      "the context of the current connection");
}

} // namespace redisdb
