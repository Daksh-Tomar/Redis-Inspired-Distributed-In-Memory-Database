#pragma once

#include "protocol/resp_types.h"
#include <vector>

namespace redisdb {
class Server;
class Client;

namespace commands {

void setCommand(Server &server, Client &client,
                const std::vector<RespValue> &args);
void getCommand(Server &server, Client &client,
                const std::vector<RespValue> &args);
void delCommand(Server &server, Client &client,
                const std::vector<RespValue> &args);
void existsCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void msetCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void mgetCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void incrCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void decrCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void incrbyCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void decrbyCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void appendCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void strlenCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void setnxCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args);

} // namespace commands
} // namespace redisdb
