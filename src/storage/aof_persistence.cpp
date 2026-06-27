#include "storage/aof_persistence.h"
#include "commands/command_registry.h"
#include "networking/client.h"
#include "protocol/resp_parser.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace redisdb {

bool AofPersistence::loadFromFile(std::vector<Database> &databases,
                                  const std::string &filename,
                                  Server &server) {
  (void)server;
  std::ifstream in(filename, std::ios::binary);
  if (!in) {
    return false;
  }

  std::cout << "[AOF] Loading append only file " << filename << "..."
            << std::endl;

  for (auto &db : databases) {
    db.flushDb();
  }

  std::stringstream buffer;
  buffer << in.rdbuf();
  std::string contents = buffer.str();

  if (contents.empty()) {
    return true;
  }

  RespParser parser;
  auto result = parser.parse(contents);

  Client fakeClient(INVALID_SOCKET_VAL);
  fakeClient.setFlag(CLIENT_MASTER);

  std::cout << "[AOF] Executing " << result.commands.size()
            << " commands from AOF..." << std::endl;

  for (const auto &cmd : result.commands) {
    if (cmd.type != RespType::Array || cmd.asArray().empty()) continue;

    std::string cmdName = cmd.asArray()[0].asString();
    const CommandEntry *entry =
        server.commandRegistry().lookupCommand(cmdName);

    if (entry) {
      try {
        entry->handler(server, fakeClient, cmd.asArray());
      } catch (const std::exception &e) {
        std::cerr << "[AOF] Error executing command " << cmdName << ": "
                  << e.what() << std::endl;
      }
    } else {
      std::cerr << "[AOF] Unknown command in AOF: " << cmdName << std::endl;
    }
  }

  return true;
}

bool AofPersistence::rewriteAof(const std::vector<Database> &databases,
                                const std::string &filename) {
  std::ofstream out(filename, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[AOF] Failed to open file for rewriting: " << filename
              << std::endl;
    return false;
  }

  for (size_t i = 0; i < databases.size(); ++i) {
    const auto &db = databases[i];
    if (db.size() == 0) continue;

    std::vector<std::string> selectArgs = {"SELECT", std::to_string(i)};
    out << RespSerializer::bulkStringArray(selectArgs);

    for (const auto &key : db.keys("*")) {
      const RedisObject *obj = db.getObject(key);
      if (!obj) continue;

      switch (obj->type()) {
      case ObjectType::String: {
        std::vector<std::string> args = {"SET", key, obj->asString()};
        out << RespSerializer::bulkStringArray(args);
        break;
      }
      case ObjectType::List: {
        const auto &list = obj->asList();
        std::vector<std::string> args = {"RPUSH", key};
        for (int j = 0; j < static_cast<int>(list.size()); ++j) {
          auto val = list.get(j);
          if (val) args.push_back(*val);
        }
        out << RespSerializer::bulkStringArray(args);
        break;
      }
      case ObjectType::Hash: {
        auto &hash =
            const_cast<HashTable<std::string, std::string> &>(obj->asHash());
        std::vector<std::string> args = {"HSET", key};
        for (auto kv : hash) {
          args.push_back(kv.first);
          args.push_back(kv.second);
        }
        out << RespSerializer::bulkStringArray(args);
        break;
      }
      case ObjectType::Set: {
        auto &set = const_cast<HashSet &>(obj->asSet());
        std::vector<std::string> args = {"SADD", key};
        for (const auto &member : set.members()) {
          args.push_back(member);
        }
        out << RespSerializer::bulkStringArray(args);
        break;
      }
      case ObjectType::SortedSet: {
        const auto &zset = obj->asSortedSet();
        std::vector<std::string> args = {"ZADD", key};
        auto elements = zset.rangeByRank(0, -1);
        for (const auto &pair : elements) {
          args.push_back(std::to_string(pair.second));
          args.push_back(pair.first);
        }
        out << RespSerializer::bulkStringArray(args);
        break;
      }
      default:
        break;
      }

      int64_t pttl = db.pttl(key);
      if (pttl >= 0) {
        auto now = std::chrono::system_clock::now();
        auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
        uint64_t expireMs = epoch + pttl;

        std::vector<std::string> args = {"PEXPIREAT", key,
                                         std::to_string(expireMs)};
        out << RespSerializer::bulkStringArray(args);
      }
    }
  }

  return true;
}

} // namespace redisdb
