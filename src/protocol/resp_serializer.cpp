#include "protocol/resp_serializer.h"

namespace redisdb {

std::string RespSerializer::serialize(const RespValue &value) {
  switch (value.type) {
  case RespType::SimpleString:
    return "+" + value.asString() + "\r\n";

  case RespType::Error:
    return "-" + value.asString() + "\r\n";

  case RespType::Integer:
    return ":" + std::to_string(value.asInteger()) + "\r\n";

  case RespType::BulkString:
    return "$" + std::to_string(value.asString().size()) + "\r\n" +
           value.asString() + "\r\n";

  case RespType::Array: {
    const auto &elements = value.asArray();
    std::string result = "*" + std::to_string(elements.size()) + "\r\n";
    for (const auto &elem : elements) {
      result += serialize(elem);
    }
    return result;
  }

  case RespType::Null:
    return "$-1\r\n";

  default:
    return "-ERR unknown type\r\n";
  }
}

std::string RespSerializer::array(const std::vector<std::string> &elements) {
  std::string result = "*" + std::to_string(elements.size()) + "\r\n";
  for (const auto &elem : elements) {
    result += elem;
  }
  return result;
}

std::string
RespSerializer::bulkStringArray(const std::vector<std::string> &strings) {
  std::string result = "*" + std::to_string(strings.size()) + "\r\n";
  for (const auto &s : strings) {
    result += bulkString(s);
  }
  return result;
}

} // namespace redisdb
