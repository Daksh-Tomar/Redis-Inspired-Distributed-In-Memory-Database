#pragma once

#include "protocol/resp_types.h"
#include <vector>

namespace redisdb {
class Server;
class Client;

namespace commands {

void pingCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void echoCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void commandCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args);
void dbsizeCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void flushdbCommand(Server &server, Client &client,
                    const std::vector<RespValue> &args);
void selectCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void quitCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void infoCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);

void saveCommand(Server &server, Client &client,
                 const std::vector<RespValue> &args);
void bgsaveCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void bgrewriteaofCommand(Server &server, Client &client,
                         const std::vector<RespValue> &args);

void configCommand(Server &server, Client &client,
                   const std::vector<RespValue> &args);
void debugCommand(Server &server, Client &client,
                  const std::vector<RespValue> &args);

} // namespace commands
} // namespace redisdb
