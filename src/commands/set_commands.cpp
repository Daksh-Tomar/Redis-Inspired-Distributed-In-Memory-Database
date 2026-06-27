#include "commands/set_commands.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"
#include "storage/db.h"


namespace redisdb {
namespace commands {

void saddCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  RedisObject *obj = db.getObjectForWrite(key);

  if (obj == nullptr) {
    auto set = std::make_unique<HashSet>();
    db.setObject(key, RedisObject(std::move(set)));
    obj = db.getObjectForWrite(key);
  } else if (obj->type() != ObjectType::Set) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  auto &set = obj->asSet();
  int added = 0;

  for (size_t i = 2; i < args.size(); i++) {
    if (set.add(args[i].asString())) {
      added++;
    }
  }

  client.addReply(RespSerializer::integer(added));
}

void sremCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  RedisObject *obj = db.getObjectForWrite(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::integer(0));
    return;
  }

  if (obj->type() != ObjectType::Set) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  auto &set = obj->asSet();
  int removed = 0;

  for (size_t i = 2; i < args.size(); i++) {
    if (set.remove(args[i].asString())) {
      removed++;
    }
  }

  if (set.empty()) {
    db.del(key);
  }

  client.addReply(RespSerializer::integer(removed));
}

void sismemberCommand(Server &server, Client &client,
                      const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();
  const std::string &member = args[2].asString();

  const RedisObject *obj = db.getObject(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::integer(0));
    return;
  }

  if (obj->type() != ObjectType::Set) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  const auto &set = obj->asSet();
  if (set.contains(member)) {
    client.addReply(RespSerializer::integer(1));
  } else {
    client.addReply(RespSerializer::integer(0));
  }
}

void smembersCommand(Server &server, Client &client,
                     const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  const RedisObject *obj = db.getObject(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::emptyArray());
    return;
  }

  if (obj->type() != ObjectType::Set) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  auto *nonConstSet = const_cast<HashSet *>(&obj->asSet());
  auto members = nonConstSet->members();

  client.addReply(RespSerializer::bulkStringArray(members));
}

} // namespace commands
} // namespace redisdb
