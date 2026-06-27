#pragma once

#include "protocol/resp_types.h"
#include <vector>

namespace redisdb {
class Server;
class Client;

namespace commands {

void typeCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void renameCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void keysCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void expireCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void pexpireCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args);
void expireatCommand(Server &server, Client &client,
                     const std::vector<RespValue> &args);
void pexpireatCommand(Server &server, Client &client,
                      const std::vector<RespValue> &args);
void ttlCommand(Server &server, Client &client,
                const std::vector<RespValue> &args);
void pttlCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void persistCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args);

} // namespace commands
} // namespace redisdb
