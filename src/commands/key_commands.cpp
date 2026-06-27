#include "commands/key_commands.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"
#include "storage/db.h"


namespace redisdb {
namespace commands {

void typeCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  std::string type = db.type(args[1].asString());
  client.addReply(RespSerializer::simpleString(type));
}

void renameCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());

  if (!db.rename(args[1].asString(), args[2].asString())) {
    client.addReply(RespSerializer::error("no such key"));
    return;
  }

  client.addReply(RespSerializer::ok());
}

void keysCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  auto keys = db.keys(args[1].asString());

  client.addReply(RespSerializer::bulkStringArray(keys));
}

// Expiration Commands
void expireCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());

  int64_t seconds;
  try {
    seconds = std::stoll(args[2].asString());
  } catch (...) {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
    return;
  }

  if (db.setExpiry(args[1].asString(), seconds * 1000)) {
    client.addReply(RespSerializer::integer(1));
  } else {
    client.addReply(RespSerializer::integer(0));
  }
}

void pexpireCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());

  int64_t milliseconds;
  try {
    milliseconds = std::stoll(args[2].asString());
  } catch (...) {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
    return;
  }

  if (db.setExpiry(args[1].asString(), milliseconds)) {
    client.addReply(RespSerializer::integer(1));
  } else {
    client.addReply(RespSerializer::integer(0));
  }
}

void expireatCommand(Server &server, Client &client,
                     const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());

  int64_t timestamp;
  try {
    timestamp = std::stoll(args[2].asString());
  } catch (...) {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
    return;
  }

  if (db.setExpiryAt(args[1].asString(), timestamp * 1000)) {
    client.addReply(RespSerializer::integer(1));
  } else {
    client.addReply(RespSerializer::integer(0));
  }
}

void pexpireatCommand(Server &server, Client &client,
                      const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());

  int64_t timestampMs;
  try {
    timestampMs = std::stoll(args[2].asString());
  } catch (...) {
    client.addReply(
        RespSerializer::error("value is not an integer or out of range"));
    return;
  }

  if (db.setExpiryAt(args[1].asString(), timestampMs)) {
    client.addReply(RespSerializer::integer(1));
  } else {
    client.addReply(RespSerializer::integer(0));
  }
}

void ttlCommand(Server &server, Client &client,
                const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());

  int64_t pttl = db.pttl(args[1].asString());
  if (pttl < 0) {
    client.addReply(RespSerializer::integer(pttl));
  } else {
    client.addReply(RespSerializer::integer(pttl / 1000));
  }
}

void pttlCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  client.addReply(RespSerializer::integer(db.pttl(args[1].asString())));
}

void persistCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());

  if (db.persist(args[1].asString())) {
    client.addReply(RespSerializer::integer(1));
  } else {
    client.addReply(RespSerializer::integer(0));
  }
}

} // namespace commands
} // namespace redisdb
