#include "commands/transaction_commands.h"
#include "commands/command_registry.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"


namespace redisdb {

void registerTransactionCommands(CommandRegistry &registry) {
  registry.registerCommand(
      "multi",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        if (client.hasFlag(CLIENT_MULTI)) {
          client.addReply(
              RespSerializer::error("ERR MULTI calls can not be nested"));
          return;
        }
        client.setFlag(CLIENT_MULTI);
        client.addReply(RespSerializer::simpleString("OK"));
      },
      1, CMD_FAST, "Start a transaction");

  registry.registerCommand(
      "exec",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        if (!client.hasFlag(CLIENT_MULTI)) {
          client.addReply(RespSerializer::error("ERR EXEC without MULTI"));
          return;
        }

        if (client.hasFlag(CLIENT_DIRTY_EXEC)) {
          client.clearTransaction();
          client.addReply(RespSerializer::nullArray());
          return;
        }

        auto queue = client.transactionQueue();
        client.clearTransaction();

        client.addReply("*" + std::to_string(queue.size()) + "\r\n");

        for (const auto &cmd : queue) {

          std::string cmdName = cmd[0].asString();
          const CommandEntry *entry =
              server.commandRegistry().lookupCommand(cmdName);
          if (entry) {
            try {
              entry->handler(server, client, cmd);
              server.incrementCommandsProcessed();
              if (entry->flags & CMD_WRITE) {
                server.propagateToAof(cmd);
              }
            } catch (const std::exception &e) {
              client.addReply(RespSerializer::error(
                  std::string("internal error: ") + e.what()));
            }
          } else {
            client.addReply(RespSerializer::error("unknown command"));
          }
        }
      },
      1, CMD_FAST, "Execute all commands issued after MULTI");

  registry.registerCommand(
      "discard",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        (void)args;
        (void)server;
        if (!client.hasFlag(CLIENT_MULTI)) {
          client.addReply(RespSerializer::error("ERR DISCARD without MULTI"));
          return;
        }

        for (const auto &key : client.watchedKeys()) {
          for (int i = 0; i < server.databaseCount(); ++i) {
            server.getDatabase(i).removeWatchedKey(key, &client);
          }
        }
        client.watchedKeys().clear();
        client.clearTransaction();
        client.addReply(RespSerializer::simpleString("OK"));
      },
      1, CMD_FAST, "Discard all commands issued after MULTI");

  registry.registerCommand(
      "watch",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        if (client.hasFlag(CLIENT_MULTI)) {
          client.addReply(
              RespSerializer::error("ERR WATCH inside MULTI is not allowed"));
          return;
        }
        Database &db = server.getDatabase(client.selectedDb());
        for (size_t i = 1; i < args.size(); ++i) {
          std::string key = args[i].asString();
          client.watchedKeys().insert(key);
          db.addWatchedKey(key, &client);
        }
        client.addReply(RespSerializer::simpleString("OK"));
      },
      -2, CMD_FAST,
      "Watch the given keys to determine execution of the MULTI/EXEC block");

  registry.registerCommand(
      "unwatch",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        (void)args;
        for (const auto &key : client.watchedKeys()) {
          for (int i = 0; i < server.databaseCount(); ++i) {
            server.getDatabase(i).removeWatchedKey(key, &client);
          }
        }
        client.watchedKeys().clear();
        client.clearFlag(CLIENT_DIRTY_EXEC);
        client.addReply(RespSerializer::simpleString("OK"));
      },
      1, CMD_FAST, "Forget about all watched keys");
}

} // namespace redisdb
