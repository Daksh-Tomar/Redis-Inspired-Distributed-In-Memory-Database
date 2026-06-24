#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace redisdb {

enum class RespType {
  SimpleString,
  Error,
  Integer,
  BulkString,
  Array,
  Null,
};

struct RespValue {
  RespType type;

  std::variant<std::string, int64_t, std::vector<RespValue>, std::monostate>
      data;

  RespValue() : type(RespType::Null), data(std::monostate{}) {}

  static RespValue simpleString(const std::string &s) {
    RespValue v;
    v.type = RespType::SimpleString;
    v.data = s;
    return v;
  }

  static RespValue error(const std::string &msg) {
    RespValue v;
    v.type = RespType::Error;
    v.data = msg;
    return v;
  }

  static RespValue integer(int64_t n) {
    RespValue v;
    v.type = RespType::Integer;
    v.data = n;
    return v;
  }

  static RespValue bulkString(const std::string &s) {
    RespValue v;
    v.type = RespType::BulkString;
    v.data = s;
    return v;
  }

  static RespValue array(std::vector<RespValue> elements) {
    RespValue v;
    v.type = RespType::Array;
    v.data = std::move(elements);
    return v;
  }

  static RespValue null() {
    RespValue v;
    v.type = RespType::Null;
    v.data = std::monostate{};
    return v;
  }

  const std::string &asString() const { return std::get<std::string>(data); }
  int64_t asInteger() const { return std::get<int64_t>(data); }
  const std::vector<RespValue> &asArray() const {
    return std::get<std::vector<RespValue>>(data);
  }
  bool isNull() const { return type == RespType::Null; }
  bool isError() const { return type == RespType::Error; }
};

} // namespace redisdb
