#pragma once

#include "protocol/resp_types.h"

#include <string>
#include <vector>

namespace redisdb {

class RespSerializer {
public:
  static std::string serialize(const RespValue &value);

  static std::string ok() { return "+OK\r\n"; }

  static std::string pong() { return "+PONG\r\n"; }

  static std::string simpleString(const std::string &str) {
    return "+" + str + "\r\n";
  }

  static std::string error(const std::string &msg) {
    return "-ERR " + msg + "\r\n";
  }

  static std::string wrongType() {
    return "-WRONGTYPE Operation against a key holding the wrong kind of "
           "value\r\n";
  }

  static std::string integer(int64_t n) {
    return ":" + std::to_string(n) + "\r\n";
  }

  static std::string bulkString(const std::string &str) {
    return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
  }

  static std::string nullBulkString() { return "$-1\r\n"; }

  static std::string nullArray() { return "*-1\r\n"; }

  static std::string emptyArray() { return "*0\r\n"; }

  static std::string array(const std::vector<std::string> &elements);

  static std::string bulkStringArray(const std::vector<std::string> &strings);
};

} // namespace redisdb
