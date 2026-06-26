#include "commands/command_registry.h"
#include "commands/hash_commands.h"
#include "commands/key_commands.h"
#include "commands/list_commands.h"
#include "commands/pubsub_commands.h"
#include "commands/replication_commands.h"
#include "commands/server_commands.h"
#include "commands/set_commands.h"
#include "commands/string_commands.h"
#include "commands/transaction_commands.h"
#include "commands/zset_commands.h"


#include <algorithm>
#include <iostream>

namespace redisdb {

CommandRegistry::CommandRegistry() {}

void CommandRegistry::registerCommand(const std::string &name,
                                      CommandHandler handler, int arity,
                                      uint32_t flags,
                                      const std::string &description) {
  std::string upperName = toUpper(name);

  CommandEntry entry;
  entry.name = upperName;
  entry.handler = std::move(handler);
  entry.arity = arity;
  entry.flags = flags;
  entry.description = description;

  commands_[upperName] = std::move(entry);
}

const CommandEntry *
CommandRegistry::lookupCommand(const std::string &name) const {
  std::string upperName = toUpper(name);
  auto it = commands_.find(upperName);
  if (it != commands_.end()) {
    return &it->second;
  }
  return nullptr;
}

void CommandRegistry::registerBuiltinCommands() {
  // Server Commands
  registerCommand("PING", commands::pingCommand, -1,
                  CMD_READONLY | CMD_FAST | CMD_STALE, "Ping the server");
  registerCommand("ECHO", commands::echoCommand, 2, CMD_READONLY | CMD_FAST,
                  "Echo the given string");
  registerCommand("COMMAND", commands::commandCommand, -1,
                  CMD_READONLY | CMD_LOADING | CMD_STALE,
                  "Get array of Redis command details");
  registerCommand("DBSIZE", commands::dbsizeCommand, 1, CMD_READONLY | CMD_FAST,
                  "Return the number of keys in the selected database");
  registerCommand("FLUSHDB", commands::flushdbCommand, -1, CMD_WRITE,
                  "Remove all keys from the current database");
  registerCommand("SELECT", commands::selectCommand, 2,
                  CMD_READONLY | CMD_FAST | CMD_LOADING,
                  "Change the selected database");
  registerCommand("QUIT", commands::quitCommand, 1,
                  CMD_READONLY | CMD_FAST | CMD_LOADING | CMD_STALE,
                  "Close the connection");
  registerCommand("INFO", commands::infoCommand, -1,
                  CMD_READONLY | CMD_STALE | CMD_LOADING,
                  "Get information and statistics about the server");

  registerCommand("SAVE", commands::saveCommand, 1, CMD_ADMIN,
                  "Synchronously save the dataset to disk");
  registerCommand("BGSAVE", commands::bgsaveCommand, 1, CMD_ADMIN,
                  "Asynchronously save the dataset to disk");
  registerCommand("BGREWRITEAOF", commands::bgrewriteaofCommand, 1, CMD_ADMIN,
                  "Asynchronously rewrite the append-only file");

  registerCommand("CONFIG", commands::configCommand, -2, CMD_ADMIN | CMD_STALE,
                  "A container for server configuration commands");
  registerCommand("DEBUG", commands::debugCommand, -2, CMD_ADMIN,
                  "A container for debugging commands");

  // List Commands
  registerCommand("LPUSH", commands::lpushCommand, -3, CMD_WRITE,
                  "Prepend one or multiple values to a list");
  registerCommand("RPUSH", commands::rpushCommand, -3, CMD_WRITE,
                  "Append one or multiple values to a list");
  registerCommand("LPOP", commands::lpopCommand, 2, CMD_WRITE,
                  "Remove and get the first element in a list");
  registerCommand("RPOP", commands::rpopCommand, 2, CMD_WRITE,
                  "Remove and get the last element in a list");
  registerCommand("BLPOP", commands::blpopCommand, -3, CMD_WRITE,
                  "Remove and get the first element in a list, or block until "
                  "one is available");
  registerCommand("BRPOP", commands::brpopCommand, -3, CMD_WRITE,
                  "Remove and get the last element in a list, or block until "
                  "one is available");
  registerCommand("LRANGE", commands::lrangeCommand, 4, CMD_READONLY,
                  "Get a range of elements from a list");
  registerCommand("LLEN", commands::llenCommand, 2, CMD_READONLY,
                  "Get the length of a list");

  // Hash Commands
  registerCommand("HSET", commands::hsetCommand, -4, CMD_WRITE,
                  "Set field value");
  registerCommand("HGET", commands::hgetCommand, 3, CMD_READONLY,
                  "Get field value");
  registerCommand("HGETALL", commands::hgetallCommand, 2, CMD_READONLY,
                  "Get all fields and values");

  // Set Commands
  registerCommand("SADD", commands::saddCommand, -3, CMD_WRITE,
                  "Add members to set");
  registerCommand("SREM", commands::sremCommand, -3, CMD_WRITE,
                  "Remove members from set");
  registerCommand("SISMEMBER", commands::sismemberCommand, 3, CMD_READONLY,
                  "Check membership");
  registerCommand("SMEMBERS", commands::smembersCommand, 2, CMD_READONLY,
                  "Get all members");

  // Sorted Set Commands
  registerCommand("ZADD", commands::zaddCommand, -4, CMD_WRITE,
                  "Add members with scores to sorted set");
  registerCommand("ZRANGE", commands::zrangeCommand, -4, CMD_READONLY,
                  "Get a range of members by rank");
  registerCommand("ZRANK", commands::zrankCommand, 3, CMD_READONLY,
                  "Get the rank of a member");
  registerCommand("ZSCORE", commands::zscoreCommand, 3, CMD_READONLY,
                  "Get the score of a member");
  registerCommand("ZREM", commands::zremCommand, -3, CMD_WRITE,
                  "Remove members from sorted set");

  // String Commands
  registerCommand("SET", commands::setCommand, -3, CMD_WRITE,
                  "Set the string value of a key");
  registerCommand("GET", commands::getCommand, 2, CMD_READONLY | CMD_FAST,
                  "Get the value of a key");
  registerCommand("DEL", commands::delCommand, -2, CMD_WRITE,
                  "Delete one or more keys");
  registerCommand("EXISTS", commands::existsCommand, -2,
                  CMD_READONLY | CMD_FAST, "Determine if a key exists");
  registerCommand("MSET", commands::msetCommand, -3, CMD_WRITE,
                  "Set multiple keys to multiple values");
  registerCommand("MGET", commands::mgetCommand, -2, CMD_READONLY | CMD_FAST,
                  "Get the values of all the given keys");
  registerCommand("INCR", commands::incrCommand, 2, CMD_WRITE | CMD_FAST,
                  "Increment the integer value of a key by one");
  registerCommand("DECR", commands::decrCommand, 2, CMD_WRITE | CMD_FAST,
                  "Decrement the integer value of a key by one");
  registerCommand("INCRBY", commands::incrbyCommand, 3, CMD_WRITE | CMD_FAST,
                  "Increment the integer value of a key by the given amount");
  registerCommand("DECRBY", commands::decrbyCommand, 3, CMD_WRITE | CMD_FAST,
                  "Decrement the integer value of a key by the given number");
  registerCommand("APPEND", commands::appendCommand, 3, CMD_WRITE,
                  "Append a value to a key");
  registerCommand("STRLEN", commands::strlenCommand, 2, CMD_READONLY | CMD_FAST,
                  "Get the length of the value stored at a key");
  registerCommand("SETNX", commands::setnxCommand, 3, CMD_WRITE | CMD_FAST,
                  "Set the value of a key, only if the key does not exist");

  // Key Commands
  registerCommand("TYPE", commands::typeCommand, 2, CMD_READONLY,
                  "Determine the type stored at key");
  registerCommand("RENAME", commands::renameCommand, 3, CMD_WRITE,
                  "Rename a key");
  registerCommand("KEYS", commands::keysCommand, 2, CMD_READONLY,
                  "Find all keys matching the given pattern");

  registerCommand("EXPIRE", commands::expireCommand, 3, CMD_WRITE,
                  "Set a key's time to live in seconds");
  registerCommand("PEXPIRE", commands::pexpireCommand, 3, CMD_WRITE,
                  "Set a key's time to live in milliseconds");
  registerCommand("EXPIREAT", commands::expireatCommand, 3, CMD_WRITE,
                  "Set the expiration for a key as a UNIX timestamp");
  registerCommand("PEXPIREAT", commands::pexpireatCommand, 3, CMD_WRITE,
                  "Set the expiration for a key as a UNIX timestamp specified "
                  "in milliseconds");
  registerCommand("TTL", commands::ttlCommand, 2, CMD_READONLY,
                  "Get the time to live for a key in seconds");
  registerCommand("PTTL", commands::pttlCommand, 2, CMD_READONLY,
                  "Get the time to live for a key in milliseconds");
  registerCommand("PERSIST", commands::persistCommand, 2, CMD_WRITE,
                  "Remove the expiration from a key");

  // Transactions and Pub/Sub
  registerTransactionCommands(*this);
  registerPubSubCommands(*this);
  registerReplicationCommands(*this);

  std::cout << "[CommandRegistry] Registered " << commands_.size()
            << " commands" << std::endl;
}

std::string CommandRegistry::toUpper(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}

} // namespace redisdb
