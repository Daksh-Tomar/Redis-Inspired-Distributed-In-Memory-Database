#pragma once

#include "protocol/resp_types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace redisdb {

class RespParser {
public:
  RespParser() = default;

  struct ParseResult {
    std::vector<RespValue> commands;
    size_t bytesConsumed = 0;
  };

  ParseResult parse(std::string_view buffer);

  static std::optional<RespValue> parseValue(std::string_view buffer,
                                             size_t &pos);

private:
  static std::optional<RespValue> parseInline(std::string_view buffer,
                                              size_t &pos);

  static std::optional<RespValue> parseSimpleString(std::string_view buffer,
                                                    size_t &pos);

  static std::optional<RespValue> parseError(std::string_view buffer,
                                             size_t &pos);

  static std::optional<RespValue> parseInteger(std::string_view buffer,
                                               size_t &pos);

  static std::optional<RespValue> parseBulkString(std::string_view buffer,
                                                  size_t &pos);

  static std::optional<RespValue> parseArray(std::string_view buffer,
                                             size_t &pos);

  static size_t findCRLF(std::string_view buffer, size_t pos);

  static std::optional<std::string_view> readLine(std::string_view buffer,
                                                  size_t &pos);
};

} // namespace redisdb
