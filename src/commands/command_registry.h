#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// Command Registry — Command Lookup and Dispatch
// ═══════════════════════════════════════════════════════════════════════════════

#include "protocol/resp_types.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>


namespace redisdb {

// Forward declarations
class Client;
class Server;

using CommandHandler = std::function<void(Server &server, Client &client,
                                          const std::vector<RespValue> &args)>;

// Command flags
enum CommandFlag : uint32_t {
  CMD_READONLY = (1 << 0),
  CMD_WRITE = (1 << 1),
  CMD_ADMIN = (1 << 2),
  CMD_PUBSUB = (1 << 3),
  CMD_FAST = (1 << 4),
  CMD_LOADING = (1 << 5),
  CMD_STALE = (1 << 6),
};

struct CommandEntry {
  std::string name;
  CommandHandler handler;
  int arity;
  uint32_t flags;
  std::string description;
};

class CommandRegistry {
public:
  CommandRegistry();

  void registerCommand(const std::string &name, CommandHandler handler,
                       int arity, uint32_t flags,
                       const std::string &description = "");

  const CommandEntry *lookupCommand(const std::string &name) const;

  const std::unordered_map<std::string, CommandEntry> &commands() const {
    return commands_;
  }

  void registerBuiltinCommands();

private:
  std::unordered_map<std::string, CommandEntry> commands_;

  static std::string toUpper(const std::string &str);
};

} // namespace redisdb
