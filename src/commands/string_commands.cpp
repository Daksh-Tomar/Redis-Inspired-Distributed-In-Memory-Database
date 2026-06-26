#include "commands/string_commands.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"
#include "storage/db.h"

namespace redisdb {
namespace commands {

void setCommand(Server &server, Client &client,
                const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();
  const std::string &value = args[2].asString();

  bool nx = false, xx = false;
  int64_t expiryMs = -1;
  for (size_t i = 3; i < args.size(); i++) {
    std::string flag = args[i].asString();
    for (auto &c : flag)
      c = static_cast<char>(toupper(c));

    if (flag == "NX") {
      nx = true;
    } else if (flag == "XX") {
      xx = true;
    } else if (flag == "EX" && i + 1 < args.size()) {
      i++;
      try {
        expiryMs = std::stoll(args[i].asString()) * 1000;
      } catch (...) {
        client.addReply(
            RespSerializer::error("value is not an integer or out of range"));
        return;
      }
      if (expiryMs <= 0) {
        client.addReply(
            RespSerializer::error("invalid expire time in 'set' command"));
        return;
      }
    } else if (flag == "PX" && i + 1 < args.size()) {
      i++;
      try {
        expiryMs = std::stoll(args[i].asString());
      } catch (...) {
        client.addReply(
            RespSerializer::error("value is not an integer or out of range"));
        return;
      }
      if (expiryMs <= 0) {
        client.addReply(
            RespSerializer::error("invalid expire time in 'set' command"));
        return;
      }
    }
  }

  if (nx && db.exists(key)) {
    client.addReply(RespSerializer::nullBulkString());
    return;
  }
  if (xx && !db.exists(key)) {
    client.addReply(RespSerializer::nullBulkString());
    return;
  }

  db.set(key, value);

  if (expiryMs > 0) {
    db.setExpiry(key, expiryMs);
  }

  server.incrementCommandsProcessed();
  client.addReply(RespSerializer::ok());
}

void getCommand(Server &server, Client &client,
                const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  auto value = db.get(args[1].asString());

  if (value.has_value()) {
    client.addReply(RespSerializer::bulkString(*value));
  } else {
    client.addReply(RespSerializer::nullBulkString());
  }
}

void delCommand(Server &server, Client &client,
                const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  int64_t deleted = 0;

  for (size_t i = 1; i < args.size(); i++) {
    if (db.del(args[i].asString())) {
      deleted++;
    }
  }

  client.addReply(RespSerializer::integer(deleted));
}

void existsCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  int64_t count = 0;

  for (size_t i = 1; i < args.size(); i++) {
    if (db.exists(args[i].asString())) {
      count++;
    }
  }

  client.addReply(RespSerializer::integer(count));
}

void msetCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {

  if ((args.size() - 1) % 2 != 0) {
    client.addReply(
        RespSerializer::error("wrong number of arguments for MSET"));
    return;
  }

  auto &db = server.getDatabase(client.selectedDb());

  for (size_t i = 1; i < args.size(); i += 2) {
    db.set(args[i].asString(), args[i + 1].asString());
  }

  client.addReply(RespSerializer::ok());
}

void mgetCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());

  std::string response = "*" + std::to_string(args.size() - 1) + "\r\n";
  for (size_t i = 1; i < args.size(); i++) {
    auto value = db.get(args[i].asString());
    if (value.has_value()) {
      response += RespSerializer::bulkString(*value);
    } else {
      response += RespSerializer::nullBulkString();
    }
  }

  client.addReply(response);
}

void incrCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  auto result = db.incrby(args[1].asString(), 1);

  if (result.has_value()) {
    client.addReply(RespSerializer::integer(*result));
  } else {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
  }
}

void decrCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  // DECR key — decrement by 1

  auto &db = server.getDatabase(client.selectedDb());
  auto result = db.incrby(args[1].asString(), -1);

  if (result.has_value()) {
    client.addReply(RespSerializer::integer(*result));
  } else {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
  }
}

void incrbyCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {

  int64_t increment;
  try {
    increment = std::stoll(args[2].asString());
  } catch (...) {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
    return;
  }

  auto &db = server.getDatabase(client.selectedDb());
  auto result = db.incrby(args[1].asString(), increment);

  if (result.has_value()) {
    client.addReply(RespSerializer::integer(*result));
  } else {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
  }
}

void decrbyCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {

  int64_t decrement;
  try {
    decrement = std::stoll(args[2].asString());
  } catch (...) {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
    return;
  }

  auto &db = server.getDatabase(client.selectedDb());
  auto result = db.incrby(args[1].asString(), -decrement);

  if (result.has_value()) {
    client.addReply(RespSerializer::integer(*result));
  } else {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
  }
}

void appendCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  auto result = db.append(args[1].asString(), args[2].asString());

  if (result.has_value()) {
    client.addReply(RespSerializer::integer(static_cast<int64_t>(*result)));
  } else {
    client.addReply(RespSerializer::error("internal error"));
  }
}

void strlenCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());
  auto value = db.get(args[1].asString());

  if (value.has_value()) {
    client.addReply(
        RespSerializer::integer(static_cast<int64_t>(value->size())));
  } else {
    client.addReply(RespSerializer::integer(0));
  }
}

void setnxCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args) {

  auto &db = server.getDatabase(client.selectedDb());

  if (db.exists(args[1].asString())) {
    client.addReply(RespSerializer::integer(0));
  } else {
    db.set(args[1].asString(), args[2].asString());
    client.addReply(RespSerializer::integer(1));
  }
}

} // namespace commands
} // namespace redisdb
