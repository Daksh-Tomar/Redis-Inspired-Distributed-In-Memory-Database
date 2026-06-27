#include "commands/zset_commands.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"
#include "storage/db.h"


namespace redisdb {
namespace commands {

void zaddCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  if ((args.size() - 2) % 2 != 0) {
    client.addReply(
        RespSerializer::error("wrong number of arguments for 'zadd' command"));
    return;
  }

  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  RedisObject *obj = db.getObjectForWrite(key);

  if (obj == nullptr) {
    auto zset = std::make_unique<SortedSet>();
    db.setObject(key, RedisObject(std::move(zset)));
    obj = db.getObjectForWrite(key);
  } else if (obj->type() != ObjectType::SortedSet) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  auto &zset = obj->asSortedSet();
  int added = 0;

  for (size_t i = 2; i < args.size(); i += 2) {
    double score;
    try {
      score = std::stod(args[i].asString());
    } catch (...) {
      client.addReply(RespSerializer::error("value is not a valid float"));
      return;
    }

    const std::string &member = args[i + 1].asString();

    if (zset.add(member, score) == 1) {
      added++;
    }
  }

  client.addReply(RespSerializer::integer(added));
}

void zrangeCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {
  bool withScores = false;
  if (args.size() > 4) {
    std::string opt = args[4].asString();
    std::transform(opt.begin(), opt.end(), opt.begin(), ::toupper);
    if (opt == "WITHSCORES") {
      withScores = true;
    } else {
      client.addReply(RespSerializer::error("syntax error"));
      return;
    }
  }

  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  long start, stop;
  try {
    start = std::stol(args[2].asString());
    stop = std::stol(args[3].asString());
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

  if (obj->type() != ObjectType::SortedSet) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  const auto &zset = obj->asSortedSet();
  auto elements = zset.rangeByRank(start, stop);

  std::vector<std::string> results;
  results.reserve(elements.size() * (withScores ? 2 : 1));

  for (const auto &pair : elements) {
    results.push_back(pair.first);
    if (withScores) {
      std::string scoreStr = std::to_string(pair.second);
      scoreStr.erase(scoreStr.find_last_not_of('0') + 1, std::string::npos);
      if (scoreStr.back() == '.') {
        scoreStr.pop_back();
      }
      results.push_back(scoreStr);
    }
  }

  client.addReply(RespSerializer::bulkStringArray(results));
}

void zrankCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();
  const std::string &member = args[2].asString();

  const RedisObject *obj = db.getObject(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::nullBulkString());
    return;
  }

  if (obj->type() != ObjectType::SortedSet) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  const auto &zset = obj->asSortedSet();

  if (!zset.getScore(member).has_value()) {
    client.addReply(RespSerializer::nullBulkString());
    return;
  }

  unsigned long rank = zset.getRank(member);
  client.addReply(RespSerializer::integer(rank));
}

void zscoreCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();
  const std::string &member = args[2].asString();

  const RedisObject *obj = db.getObject(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::nullBulkString());
    return;
  }

  if (obj->type() != ObjectType::SortedSet) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  const auto &zset = obj->asSortedSet();
  std::optional<double> score = zset.getScore(member);

  if (score.has_value()) {
    std::string scoreStr = std::to_string(*score);
    scoreStr.erase(scoreStr.find_last_not_of('0') + 1, std::string::npos);
    if (scoreStr.back() == '.') {
      scoreStr.pop_back();
    }
    client.addReply(RespSerializer::bulkString(scoreStr));
  } else {
    client.addReply(RespSerializer::nullBulkString());
  }
}

void zremCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args) {
  auto &db = server.getDatabase(client.selectedDb());
  const std::string &key = args[1].asString();

  RedisObject *obj = db.getObjectForWrite(key);

  if (obj == nullptr) {
    client.addReply(RespSerializer::integer(0));
    return;
  }

  if (obj->type() != ObjectType::SortedSet) {
    client.addReply(RespSerializer::wrongType());
    return;
  }

  auto &zset = obj->asSortedSet();
  int removed = 0;

  for (size_t i = 2; i < args.size(); i++) {
    if (zset.remove(args[i].asString()) == 1) {
      removed++;
    }
  }

  if (zset.empty()) {
    db.del(key);
  }

  client.addReply(RespSerializer::integer(removed));
}

} // namespace commands
} // namespace redisdb
