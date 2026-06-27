#pragma once

#include "protocol/resp_types.h"
#include <vector>

namespace redisdb {
class Server;
class Client;

namespace commands {

void zaddCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void zrangeCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void zrankCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args);
void zscoreCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void zremCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);

} // namespace commands
} // namespace redisdb
