#include "server/config.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace redisdb {

ServerConfig loadConfig(const std::string &filename) {
  ServerConfig config;

  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cout << "[Config] No config file found at '" << filename
              << "', using defaults" << std::endl;
    return config;
  }

  std::string line;
  int lineNum = 0;

  while (std::getline(file, line)) {
    lineNum++;

    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line.empty() || line[0] == '#') continue;

    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) continue;
    line = line.substr(start);

    std::istringstream iss(line);
    std::string key;
    iss >> key;

    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    if (key == "port") {
      iss >> config.port;
    } else if (key == "bind") {
      iss >> config.bindAddress;
    } else if (key == "maxclients") {
      iss >> config.maxClients;
    } else if (key == "databases") {
      iss >> config.databases;
    } else if (key == "maxmemory") {
      std::string val;
      iss >> val;
      if (val.size() > 2) {
        std::string suffix = val.substr(val.size() - 2);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                       ::tolower);
        std::string numPart = val.substr(0, val.size() - 2);
        try {
          int64_t num = std::stoll(numPart);
          if (suffix == "mb")
            config.maxMemory = num * 1024 * 1024;
          else if (suffix == "gb")
            config.maxMemory = num * 1024 * 1024 * 1024;
          else if (suffix == "kb")
            config.maxMemory = num * 1024;
          else
            config.maxMemory = std::stoll(val);
        } catch (...) {
          std::cerr << "[Config] Invalid maxmemory value at line " << lineNum
                    << std::endl;
        }
      } else {
        try {
          config.maxMemory = std::stoll(val);
        } catch (...) {
        }
      }
    } else if (key == "maxmemory-policy") {
      iss >> config.maxMemoryPolicy;
    } else if (key == "appendonly") {
      std::string val;
      iss >> val;
      config.appendOnly = (val == "yes");
    } else if (key == "appendfsync") {
      iss >> config.appendFsync;
    } else if (key == "dbfilename") {
      iss >> config.rdbFilename;
    } else if (key == "appendfilename") {
      iss >> config.aofFilename;
    } else if (key == "timeout") {
      iss >> config.clientTimeout;
    } else if (key == "tcp-keepalive") {
      iss >> config.tcpKeepAlive;
    } else if (key == "loglevel") {
      iss >> config.logLevel;
    } else if (key == "replicaof" || key == "slaveof") {
      iss >> config.replicaOf >> config.replicaOfPort;
    }
  }

  std::cout << "[Config] Loaded configuration from '" << filename << "'"
            << std::endl;
  return config;
}

} // namespace redisdb
