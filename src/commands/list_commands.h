#pragma once

#include "protocol/resp_types.h"
#include <vector>

namespace redisdb {
class Server;
class Client;

namespace commands {

void lpushCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args);
void rpushCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args);
void lpopCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void rpopCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void lrangeCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void llenCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void blpopCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args);
void brpopCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args);

} // namespace commands
} // namespace redisdb
