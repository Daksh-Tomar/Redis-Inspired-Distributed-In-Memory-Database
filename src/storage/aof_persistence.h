#pragma once

#include "storage/db.h"

#include <string>
#include <vector>

namespace redisdb {

class AofPersistence {
public:
  static bool loadFromFile(std::vector<Database> &databases,
                           const std::string &filename, class Server &server);

  static bool rewriteAof(const std::vector<Database> &databases,
                          const std::string &filename);
};

} // namespace redisdb
