#include "commands/server_commands.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/memory_manager.h"
#include "server/server.h"

#include <algorithm>
#include <sstream>

namespace redisdb {
namespace commands {

void pingCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {

  if (args.size() >= 2) {
    client.addReply(RespSerializer::bulkString(args[1].asString()));
  } else {
    client.addReply(RespSerializer::pong());
  }
}

void echoCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  client.addReply(RespSerializer::bulkString(args[1].asString()));
}

void commandCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args) {

  auto &registry = server.commandRegistry();
  auto &cmds = registry.commands();

  if (args.size() >= 2) {
    std::string subcommand = args[1].asString();

    std::transform(subcommand.begin(), subcommand.end(), subcommand.begin(),
                   ::toupper);

    if (subcommand == "DOCS") {
      client.addReply(RespSerializer::emptyArray());
      return;
    } else if (subcommand == "COUNT") {
      client.addReply(
          RespSerializer::integer(static_cast<int64_t>(cmds.size())));
      return;
    } else if (subcommand == "INFO") {
      // COMMAND INFO <cmd-name> [cmd-name ...]
      // Return info for specific commands in the standard 7-element format
      std::string response = "*" + std::to_string(args.size() - 2) + "\r\n";
      for (size_t i = 2; i < args.size(); i++) {
        std::string cmdName = args[i].asString();
        std::transform(cmdName.begin(), cmdName.end(), cmdName.begin(),
                       ::toupper);
        auto it = cmds.find(cmdName);
        if (it != cmds.end()) {
          const auto &entry = it->second;
          response += "*7\r\n";
          response += RespSerializer::bulkString(entry.name);
          response += RespSerializer::integer(entry.arity);
          std::vector<std::string> flagStrs;
          if (entry.flags & CMD_WRITE)
            flagStrs.push_back("write");
          if (entry.flags & CMD_READONLY)
            flagStrs.push_back("readonly");
          if (entry.flags & CMD_FAST)
            flagStrs.push_back("fast");
          if (entry.flags & CMD_ADMIN)
            flagStrs.push_back("admin");
          if (entry.flags & CMD_STALE)
            flagStrs.push_back("stale");
          if (entry.flags & CMD_LOADING)
            flagStrs.push_back("loading");
          if (flagStrs.empty())
            flagStrs.push_back("readonly");
          response += "*" + std::to_string(flagStrs.size()) + "\r\n";
          for (const auto &f : flagStrs) {
            response += RespSerializer::simpleString(f);
          }

          response += RespSerializer::integer(1);
          response += RespSerializer::integer(1);
          response += RespSerializer::integer(1);
        } else {
          response += RespSerializer::nullBulkString();
        }
      }
      client.addReply(response);
      return;
    }
  }

  std::string response = "*" + std::to_string(cmds.size()) + "\r\n";
  for (const auto &[name, entry] : cmds) {
    response += "*7\r\n";
    response += RespSerializer::bulkString(entry.name);
    response += RespSerializer::integer(entry.arity);

    std::vector<std::string> flagStrs;
    if (entry.flags & CMD_WRITE)
      flagStrs.push_back("write");
    if (entry.flags & CMD_READONLY)
      flagStrs.push_back("readonly");
    if (entry.flags & CMD_FAST)
      flagStrs.push_back("fast");
    if (entry.flags & CMD_ADMIN)
      flagStrs.push_back("admin");
    if (entry.flags & CMD_STALE)
      flagStrs.push_back("stale");
    if (entry.flags & CMD_LOADING)
      flagStrs.push_back("loading");
    if (flagStrs.empty())
      flagStrs.push_back("readonly");
    response += "*" + std::to_string(flagStrs.size()) + "\r\n";
    for (const auto &f : flagStrs) {
      response += RespSerializer::simpleString(f);
    }

    response += RespSerializer::integer(1);
    response += RespSerializer::integer(1);
    response += RespSerializer::integer(1);
  }

  client.addReply(response);
}

void dbsizeCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  client.addReply(RespSerializer::integer(static_cast<int64_t>(db.size())));
}

void flushdbCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  db.flushDb();
  client.addReply(RespSerializer::ok());
}

void selectCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {

  int64_t dbIndex = 0;
  try {
    dbIndex = std::stoll(args[1].asString());
  } catch (...) {
    client.addReply(RespSerializer::error("invalid DB index"));
    return;
  }

  if (dbIndex < 0 || dbIndex >= server.databaseCount()) {
    client.addReply(RespSerializer::error("DB index is out of range"));
    return;
  }

  client.selectDb(static_cast<int>(dbIndex));
  client.addReply(RespSerializer::ok());
}

void quitCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {

  client.addReply(RespSerializer::ok());
  client.setFlag(CLIENT_CLOSE_ASAP);
}

void infoCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  std::ostringstream info;

  info << "# Server\r\n";
  info << "redis_version:0.1.0 (RedisDB)\r\n";
  info << "tcp_port:" << server.port() << "\r\n";
  info << "uptime_in_seconds:" << server.uptimeSeconds() << "\r\n";

  info << "\r\n# Clients\r\n";
  info << "connected_clients:" << server.connectedClients() << "\r\n";

  info << "\r\n# Memory\r\n";
  info << "used_memory:" << MemoryManager::instance().getUsedMemory() << "\r\n";
  info << "maxmemory:" << server.config().maxMemory << "\r\n";
  info << "maxmemory_policy:" << server.config().maxMemoryPolicy << "\r\n";

  info << "\r\n# Persistence\r\n";
  info << "aof_enabled:" << (server.config().appendOnly ? "1" : "0") << "\r\n";
  info << "rdb_changes_since_last_save:" << server.dirty() << "\r\n";

  info << "\r\n# Stats\r\n";
  info << "total_connections_received:" << server.totalConnectionsReceived()
       << "\r\n";
  info << "total_commands_processed:" << server.totalCommandsProcessed()
       << "\r\n";
  info << "keyspace_hits:" << server.keyspaceHits() << "\r\n";
  info << "keyspace_misses:" << server.keyspaceMisses() << "\r\n";

  info << "\r\n# Replication\r\n";
  if (server.config().replicaOf.empty()) {
    info << "role:master\r\n";
  } else {
    info << "role:slave\r\n";
    info << "master_host:" << server.config().replicaOf << "\r\n";
    info << "master_port:" << server.config().replicaOfPort << "\r\n";
  }

  info << "\r\n# Keyspace\r\n";
  for (int i = 0; i < server.databaseCount(); i++) {
    auto &db = server.getDatabase(i);
    if (db.size() > 0) {
      info << "db" << i << ":keys=" << db.size()
           << ",expires=" << db.expiresSize() << "\r\n";
    }
  }

  std::string infoStr = info.str();
  client.addReply(RespSerializer::bulkString(infoStr));
}

void saveCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  if (server.save()) {
    client.addReply(RespSerializer::ok());
  } else {
    client.addReply(
        RespSerializer::error("Background save already in progress"));
  }
}

void bgsaveCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {
  if (server.bgsave()) {
    client.addReply(RespSerializer::simpleString("Background saving started"));
  } else {
    client.addReply(
        RespSerializer::error("Background save already in progress"));
  }
}

void bgrewriteaofCommand(Server &server, Client &client,
                         const std::vector<RespValue> &args) {
  if (server.bgrewriteaof()) {
    client.addReply(RespSerializer::simpleString(
        "Background append only file rewriting started"));
  } else {
    client.addReply(RespSerializer::error(
        "Background append only file rewriting already in progress"));
  }
}

void configCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {
  if (args.size() < 3) {
    client.addReply(RespSerializer::error(
        "ERR wrong number of arguments for 'config' command"));
    return;
  }

  std::string subCommand = args[1].asString();
  std::transform(subCommand.begin(), subCommand.end(), subCommand.begin(),
                 ::toupper);
  std::string param = args[2].asString();

  if (subCommand == "GET") {
    if (param == "maxmemory") {
      client.addReply(RespSerializer::bulkStringArray(
          {"maxmemory", std::to_string(server.config().maxMemory)}));
    } else if (param == "maxmemory-policy") {
      client.addReply(RespSerializer::bulkStringArray(
          {"maxmemory-policy", server.config().maxMemoryPolicy}));
    } else if (param == "appendonly") {
      client.addReply(RespSerializer::bulkStringArray(
          {"appendonly", server.config().appendOnly ? "yes" : "no"}));
    } else {
      client.addReply(RespSerializer::emptyArray());
    }
  } else if (subCommand == "SET") {
    if (args.size() < 4) {
      client.addReply(RespSerializer::error(
          "ERR wrong number of arguments for 'config set' command"));
      return;
    }
    std::string value = args[3].asString();

    if (param == "maxmemory") {
      server.config().maxMemory = std::stoll(value);
      client.addReply(RespSerializer::ok());
    } else if (param == "maxmemory-policy") {
      server.config().maxMemoryPolicy = value;
      client.addReply(RespSerializer::ok());
    } else if (param == "appendonly") {
      server.config().appendOnly = (value == "yes" || value == "1");
      client.addReply(RespSerializer::ok());
    } else {
      client.addReply(
          RespSerializer::error("ERR Unsupported CONFIG parameter"));
    }
  } else {
    client.addReply(RespSerializer::error("ERR unknown CONFIG subcommand"));
  }
}

void debugCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args) {
  if (args.size() < 2) {
    client.addReply(RespSerializer::error(
        "ERR wrong number of arguments for 'debug' command"));
    return;
  }
  std::string subCommand = args[1].asString();
  std::transform(subCommand.begin(), subCommand.end(), subCommand.begin(),
                 ::toupper);

  if (subCommand == "OBJECT" && args.size() == 3) {
    std::string key = args[2].asString();
    const RedisObject *obj =
        server.getDatabase(client.selectedDb()).getObject(key);
    if (!obj) {
      client.addReply(RespSerializer::error("ERR no such key"));
      return;
    }
    std::ostringstream out;
    out << "Value at:" << obj
        << " refcount:1 encoding:raw serializedlength:0 lru:" << obj->lruClock()
        << " lru_seconds_idle:0";
    client.addReply(RespSerializer::simpleString(out.str()));
  } else if (subCommand == "SEGFAULT") {
    int *p = nullptr;
    *p = 42; // Intentionally crash
  } else {
    client.addReply(RespSerializer::error(
        "ERR Unknown DEBUG subcommand or wrong number of arguments"));
  }
}

} // namespace commands
} // namespace redisdb
