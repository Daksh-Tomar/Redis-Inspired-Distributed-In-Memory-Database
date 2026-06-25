#pragma once

#include <cstdint>
#include <string>

namespace redisdb {

struct ServerConfig {
  std::string bindAddress = "0.0.0.0";
  int port = 6379;
  int maxClients = 10000;
  int tcpKeepAlive = 300;
  int clientTimeout = 0;

  int databases = 16;

  int64_t maxMemory = 0;
  std::string maxMemoryPolicy = "noeviction";
  int maxMemorySamples = 5;

  bool appendOnly = false;
  std::string appendFsync = "everysec";
  std::string rdbFilename = "dump.rdb";
  std::string aofFilename = "appendonly.aof";

  std::string replicaOf = "";
  int replicaOfPort = 0;

  std::string logLevel = "notice";
};

ServerConfig loadConfig(const std::string &filename);

} // namespace redisdb
