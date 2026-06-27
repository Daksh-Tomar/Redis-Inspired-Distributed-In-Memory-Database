#pragma once

#include "storage/db.h"

#include <string>
#include <vector>

namespace redisdb {

class RdbPersistence {
public:
  static bool saveToFile(const std::vector<Database> &databases,
                         const std::string &filename);

  static bool loadFromFile(std::vector<Database> &databases,
                           const std::string &filename);

private:
  static constexpr uint8_t RDB_OPCODE_EOF = 255;
  static constexpr uint8_t RDB_OPCODE_SELECTDB = 254;
  static constexpr uint8_t RDB_OPCODE_EXPIRETIME_MS = 252;
  static constexpr uint8_t RDB_OPCODE_EXPIRETIME = 253;
  static constexpr uint8_t RDB_OPCODE_RESIZEDB = 251;

  static constexpr uint8_t RDB_TYPE_STRING = 0;
  static constexpr uint8_t RDB_TYPE_LIST = 1;
  static constexpr uint8_t RDB_TYPE_SET = 2;
  static constexpr uint8_t RDB_TYPE_ZSET = 3;
  static constexpr uint8_t RDB_TYPE_HASH = 4;
};

} // namespace redisdb
