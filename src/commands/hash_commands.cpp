#include "commands/hash_commands.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"
#include "storage/db.h"


namespace redisdb {
namespace commands {

void hsetCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  if ((args.size() - 2) % 2 != 0) {
    client.addReply(
        RespSerializer::error("wrong number of arguments for 'hset' command"));
    return;
  }

  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  RedisObject *obj = db.getObjectForWrite(key);

  if (obj == nullptr) {
    auto hash = std::make_unique<HashTable<std::string, std::string>>();
    db.setObject(key, RedisObject(std::move(hash)));
    obj = db.getObjectForWrite(key);
  } else if (obj->type() != ObjectType::Hash) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  auto &hash = obj->asHash();
  int added = 0;

  for (size_t i = 2; i < args.size(); i += 2) {
    const std::string &field = args[i].asString();
    const std::string &value = args[i + 1].asString();

    if (hash.set(field, value)) {
      added++;
    }
  }

  client.addReply(RespSerializer::integer(added));
}

void hgetCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();
  const std::string &field = args[2].asString();

  const RedisObject *obj = db.getObject(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::nullBulkString());
    return;
  }

  if (obj->type() != ObjectType::Hash) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  const auto &hash = obj->asHash();
  const std::string *val = hash.get(field);

  if (val != nullptr) {
    client.addReply(RespSerializer::bulkString(*val));
  } else {
    client.addReply(RespSerializer::nullBulkString());
  }
}

void hgetallCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  const RedisObject *obj = db.getObject(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::emptyArray());
    return;
  }

  if (obj->type() != ObjectType::Hash) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  auto *nonConstHash =
      const_cast<HashTable<std::string, std::string> *>(&obj->asHash());
  std::vector<std::string> results;
  results.reserve(nonConstHash->size() * 2);

  for (auto kv : *nonConstHash) {
    results.push_back(kv.first);
    results.push_back(kv.second);
  }

  client.addReply(RespSerializer::bulkStringArray(results));
}

} // namespace commands
} // namespace redisdb
