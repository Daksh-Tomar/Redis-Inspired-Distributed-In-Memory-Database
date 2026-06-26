#include "commands/list_commands.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"
#include "storage/db.h"

namespace redisdb {
namespace commands {

void pushGeneric(Server &server, Client &client,
                 const std::vector<RespValue> &args, bool left) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  RedisObject *obj = db.getObjectForWrite(key);

  if (obj == nullptr) {
    auto list = std::make_unique<LinkedList>();
    db.setObject(key, RedisObject(std::move(list)));
    obj = db.getObjectForWrite(key);
  } else if (obj->type() != ObjectType::List) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  LinkedList &list = obj->asList();
  int pushed = 0;

  for (size_t i = 2; i < args.size(); i++) {
    if (left) {
      list.pushFront(args[i].asString());
    } else {
      list.pushBack(args[i].asString());
    }
    pushed++;
  }

  client.addReply(RespSerializer::integer(static_cast<int64_t>(list.size())));

  if (pushed > 0) {
    server.handleClientsBlockedOnKey(key);
  }
}

void lpushCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args) {
  pushGeneric(server, client, args, true);
}

void rpushCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args) {
  pushGeneric(server, client, args, false);
  pushGeneric(server, client, args, false);
}

void popGeneric(Server &server, Client &client,
                const std::vector<RespValue> &args, bool left) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  RedisObject *obj = db.getObjectForWrite(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::nullBulkString());
    return;
  }

  if (obj->type() != ObjectType::List) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  LinkedList &list = obj->asList();
  std::optional<std::string> val = left ? list.popFront() : list.popBack();

  if (val.has_value()) {
    client.addReply(RespSerializer::bulkString(*val));

    if (list.empty()) {
      db.del(key);
    }
  } else {
    client.addReply(RespSerializer::nullBulkString());
  }
}

void lpopCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  popGeneric(server, client, args, true);
}

void rpopCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  popGeneric(server, client, args, false);
}

void lrangeCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  int start, stop;
  try {
    start = std::stoi(args[2].asString());
    stop = std::stoi(args[3].asString());
  } catch (...) {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
    return;
  }

  const RedisObject *obj = db.getObject(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::emptyArray());
    return;
  }

  if (obj->type() != ObjectType::List) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  const LinkedList &list = obj->asList();
  auto elements = list.range(start, stop);

  client.addReply(RespSerializer::bulkStringArray(elements));
}

void llenCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  const RedisObject *obj = db.getObject(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::integer(0));
    return;
  }

  if (obj->type() != ObjectType::List) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  const LinkedList &list = obj->asList();
  client.addReply(RespSerializer::integer(static_cast<int64_t>(list.size())));
}

void bpopGeneric(Server &server, Client &client,
                 const std::vector<RespValue> &args, bool left) {
  auto &db = server.getDatabase(client.selectedDb());

  int64_t timeout = 0;
  try {
    timeout = std::stoll(args.back().asString());
    if (timeout < 0) {
      client.addReply(RespSerializer::error("timeout is negative"));
      return;
    }
    timeout *= 1000;
  } catch (...) {
    client.addReply(
        RespSerializer::error("timeout is not an integer or out of range"));
    return;
  }

  std::vector<std::string> keys;
  for (size_t i = 1; i < args.size() - 1; ++i) {
    keys.push_back(args[i].asString());
  }

  for (const auto &key : keys) {
    RedisObject *obj = db.getObjectForWrite(key);
    if (obj && obj->type() == ObjectType::List) {
      LinkedList &list = obj->asList();
      if (!list.empty()) {
        std::optional<std::string> val =
            left ? list.popFront() : list.popBack();
        if (val.has_value()) {
          client.addReply(RespSerializer::bulkStringArray({key, *val}));
          if (list.empty())
            db.del(key);
          return;
        }
      }
    }
  }
  server.blockClient(client, keys, timeout, RespValue::array(args));
}

void blpopCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args) {
  bpopGeneric(server, client, args, true);
}

void brpopCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args) {
  bpopGeneric(server, client, args, false);
}

} // namespace commands
} // namespace redisdb
