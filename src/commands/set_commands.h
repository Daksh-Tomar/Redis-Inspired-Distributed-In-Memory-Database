#pragma once

#include "protocol/resp_types.h"
#include <vector>

namespace redisdb {
class Server;
class Client;

namespace commands {

void saddCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void sremCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void sismemberCommand(Server &server, Client &client,
                      const std::vector<RespValue> &args);
void smembersCommand(Server &server, Client &client,
                     const std::vector<RespValue> &args);

} // namespace commands
} // namespace redisdb
