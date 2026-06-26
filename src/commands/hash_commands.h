#pragma once

#include "protocol/resp_types.h"
#include <vector>

namespace redisdb {
class Server;
class Client;

namespace commands {

void hsetCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void hgetCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void hgetallCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args);

} // namespace commands
} // namespace redisdb
