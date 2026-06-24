#include "protocol/resp_parser.h"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace redisdb {

static constexpr size_t NPOS = std::string_view::npos;

RespParser::ParseResult RespParser::parse(std::string_view buffer) {
  ParseResult result;
  size_t pos = 0;

  while (pos < buffer.size()) {
    size_t startPos = pos;

    char firstByte = buffer[pos];

    std::optional<RespValue> value;

    if (firstByte == '*' || firstByte == '+' || firstByte == '-' ||
        firstByte == ':' || firstByte == '$') {
      value = parseValue(buffer, pos);
    } else {
      value = parseInline(buffer, pos);
    }

    if (value.has_value()) {
      result.commands.push_back(std::move(*value));
      result.bytesConsumed = pos;
    } else {
      break;
    }
  }

  return result;
}

std::optional<RespValue> RespParser::parseValue(std::string_view buffer,
                                                size_t &pos) {
  if (pos >= buffer.size()) return std::nullopt;

  switch (buffer[pos]) {
  case '+': return parseSimpleString(buffer, pos);
  case '-': return parseError(buffer, pos);
  case ':': return parseInteger(buffer, pos);
  case '$': return parseBulkString(buffer, pos);
  case '*': return parseArray(buffer, pos);
  default:  return parseInline(buffer, pos);
  }
}

std::optional<RespValue> RespParser::parseInline(std::string_view buffer,
                                                 size_t &pos) {
  auto line = readLine(buffer, pos);
  if (!line.has_value()) return std::nullopt;

  std::string_view content = *line;

  if (content.empty()) {
    return RespValue::array({});
  }

  std::vector<RespValue> args;
  size_t i = 0;
  while (i < content.size()) {
    while (i < content.size() && (content[i] == ' ' || content[i] == '\t'))
      i++;
    if (i >= content.size()) break;

    size_t start = i;

    if (content[i] == '"') {
      i++;
      start = i;
      while (i < content.size() && content[i] != '"') i++;
      args.push_back(
          RespValue::bulkString(std::string(content.substr(start, i - start))));
      if (i < content.size()) i++;
    } else if (content[i] == '\'') {
      i++;
      start = i;
      while (i < content.size() && content[i] != '\'') i++;
      args.push_back(
          RespValue::bulkString(std::string(content.substr(start, i - start))));
      if (i < content.size()) i++;
    } else {
      while (i < content.size() && content[i] != ' ' && content[i] != '\t')
        i++;
      args.push_back(
          RespValue::bulkString(std::string(content.substr(start, i - start))));
    }
  }

  return RespValue::array(std::move(args));
}

std::optional<RespValue>
RespParser::parseSimpleString(std::string_view buffer, size_t &pos) {
  size_t start = pos;
  pos++;

  auto line = readLine(buffer, pos);
  if (!line.has_value()) {
    pos = start;
    return std::nullopt;
  }

  return RespValue::simpleString(std::string(*line));
}

std::optional<RespValue> RespParser::parseError(std::string_view buffer,
                                                size_t &pos) {
  size_t start = pos;
  pos++;

  auto line = readLine(buffer, pos);
  if (!line.has_value()) {
    pos = start;
    return std::nullopt;
  }

  return RespValue::error(std::string(*line));
}

std::optional<RespValue> RespParser::parseInteger(std::string_view buffer,
                                                  size_t &pos) {
  size_t start = pos;
  pos++;

  auto line = readLine(buffer, pos);
  if (!line.has_value()) {
    pos = start;
    return std::nullopt;
  }

  int64_t value = 0;
  auto [ptr, ec] =
      std::from_chars(line->data(), line->data() + line->size(), value);
  if (ec != std::errc()) {
    try {
      value = std::stoll(std::string(*line));
    } catch (...) {
      pos = start;
      return std::nullopt;
    }
  }

  return RespValue::integer(value);
}

std::optional<RespValue> RespParser::parseBulkString(std::string_view buffer,
                                                     size_t &pos) {
  size_t start = pos;
  pos++;

  auto line = readLine(buffer, pos);
  if (!line.has_value()) {
    pos = start;
    return std::nullopt;
  }

  int64_t length = 0;
  auto [ptr, ec] =
      std::from_chars(line->data(), line->data() + line->size(), length);
  if (ec != std::errc()) {
    try {
      length = std::stoll(std::string(*line));
    } catch (...) {
      pos = start;
      return std::nullopt;
    }
  }

  if (length == -1) {
    return RespValue::null();
  }

  if (length < 0) {
    pos = start;
    return std::nullopt;
  }

  if (pos + static_cast<size_t>(length) + 2 > buffer.size()) {
    pos = start;
    return std::nullopt;
  }

  std::string data(buffer.substr(pos, static_cast<size_t>(length)));
  pos += static_cast<size_t>(length);

  if (pos + 1 >= buffer.size() || buffer[pos] != '\r' ||
      buffer[pos + 1] != '\n') {
    pos = start;
    return std::nullopt;
  }
  pos += 2;

  return RespValue::bulkString(std::move(data));
}

std::optional<RespValue> RespParser::parseArray(std::string_view buffer,
                                                size_t &pos) {
  size_t start = pos;
  pos++;

  auto line = readLine(buffer, pos);
  if (!line.has_value()) {
    pos = start;
    return std::nullopt;
  }

  int64_t count = 0;
  auto [ptr, ec] =
      std::from_chars(line->data(), line->data() + line->size(), count);
  if (ec != std::errc()) {
    try {
      count = std::stoll(std::string(*line));
    } catch (...) {
      pos = start;
      return std::nullopt;
    }
  }

  if (count == -1) {
    return RespValue::null();
  }

  if (count < 0) {
    pos = start;
    return std::nullopt;
  }

  std::vector<RespValue> elements;
  elements.reserve(static_cast<size_t>(count));

  for (int64_t i = 0; i < count; i++) {
    auto element = parseValue(buffer, pos);
    if (!element.has_value()) {
      pos = start;
      return std::nullopt;
    }
    elements.push_back(std::move(*element));
  }

  return RespValue::array(std::move(elements));
}

size_t RespParser::findCRLF(std::string_view buffer, size_t pos) {
  while (pos + 1 < buffer.size()) {
    if (buffer[pos] == '\r' && buffer[pos + 1] == '\n') {
      return pos;
    }
    pos++;
  }
  return NPOS;
}

std::optional<std::string_view> RespParser::readLine(std::string_view buffer,
                                                     size_t &pos) {
  size_t crlfPos = findCRLF(buffer, pos);
  if (crlfPos == NPOS) {
    return std::nullopt;
  }

  std::string_view line = buffer.substr(pos, crlfPos - pos);
  pos = crlfPos + 2;
  return line;
}

} // namespace redisdb
