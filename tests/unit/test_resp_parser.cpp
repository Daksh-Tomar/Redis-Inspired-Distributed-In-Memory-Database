#include "protocol/resp_parser.h"
#include "protocol/resp_serializer.h"

#include <gtest/gtest.h>

using namespace redisdb;

TEST(RespParserTest, ParseSimpleString) {
  RespParser parser;
  auto result = parser.parse("+OK\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  ASSERT_EQ(result.commands[0].type, RespType::SimpleString);
  EXPECT_EQ(result.commands[0].asString(), "OK");
  EXPECT_EQ(result.bytesConsumed, 5);
}

TEST(RespParserTest, ParseError) {
  RespParser parser;
  auto result = parser.parse("-ERR unknown command\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  ASSERT_EQ(result.commands[0].type, RespType::Error);
  EXPECT_EQ(result.commands[0].asString(), "ERR unknown command");
}

TEST(RespParserTest, ParseInteger) {
  RespParser parser;
  auto result = parser.parse(":1000\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  ASSERT_EQ(result.commands[0].type, RespType::Integer);
  EXPECT_EQ(result.commands[0].asInteger(), 1000);
}

TEST(RespParserTest, ParseNegativeInteger) {
  RespParser parser;
  auto result = parser.parse(":-42\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  EXPECT_EQ(result.commands[0].asInteger(), -42);
}

TEST(RespParserTest, ParseBulkString) {
  RespParser parser;
  auto result = parser.parse("$6\r\nfoobar\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  ASSERT_EQ(result.commands[0].type, RespType::BulkString);
  EXPECT_EQ(result.commands[0].asString(), "foobar");
}

TEST(RespParserTest, ParseNullBulkString) {
  RespParser parser;
  auto result = parser.parse("$-1\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  EXPECT_TRUE(result.commands[0].isNull());
}

TEST(RespParserTest, ParseEmptyBulkString) {
  RespParser parser;
  auto result = parser.parse("$0\r\n\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  ASSERT_EQ(result.commands[0].type, RespType::BulkString);
  EXPECT_EQ(result.commands[0].asString(), "");
}

TEST(RespParserTest, ParseBulkStringWithCRLF) {
  RespParser parser;
  auto result = parser.parse("$8\r\nfoo\r\nbar\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  EXPECT_EQ(result.commands[0].asString(), "foo\r\nbar");
}

TEST(RespParserTest, ParseSimpleArray) {
  RespParser parser;
  auto result = parser.parse("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  ASSERT_EQ(result.commands[0].type, RespType::Array);

  const auto &elements = result.commands[0].asArray();
  ASSERT_EQ(elements.size(), 2);
  EXPECT_EQ(elements[0].asString(), "foo");
  EXPECT_EQ(elements[1].asString(), "bar");
}

TEST(RespParserTest, ParsePingCommand) {
  RespParser parser;
  auto result = parser.parse("*1\r\n$4\r\nPING\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  const auto &args = result.commands[0].asArray();
  ASSERT_EQ(args.size(), 1);
  EXPECT_EQ(args[0].asString(), "PING");
}

TEST(RespParserTest, ParseSetCommand) {
  RespParser parser;
  auto result =
      parser.parse("*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  const auto &args = result.commands[0].asArray();
  ASSERT_EQ(args.size(), 3);
  EXPECT_EQ(args[0].asString(), "SET");
  EXPECT_EQ(args[1].asString(), "mykey");
  EXPECT_EQ(args[2].asString(), "myvalue");
}

TEST(RespParserTest, ParseNullArray) {
  RespParser parser;
  auto result = parser.parse("*-1\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  EXPECT_TRUE(result.commands[0].isNull());
}

TEST(RespParserTest, ParseEmptyArray) {
  RespParser parser;
  auto result = parser.parse("*0\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  ASSERT_EQ(result.commands[0].type, RespType::Array);
  EXPECT_EQ(result.commands[0].asArray().size(), 0);
}

TEST(RespParserTest, ParseInlinePing) {
  RespParser parser;
  auto result = parser.parse("PING\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  const auto &args = result.commands[0].asArray();
  ASSERT_EQ(args.size(), 1);
  EXPECT_EQ(args[0].asString(), "PING");
}

TEST(RespParserTest, ParseInlineSetCommand) {
  RespParser parser;
  auto result = parser.parse("SET mykey myvalue\r\n");

  ASSERT_EQ(result.commands.size(), 1);
  const auto &args = result.commands[0].asArray();
  ASSERT_EQ(args.size(), 3);
  EXPECT_EQ(args[0].asString(), "SET");
  EXPECT_EQ(args[1].asString(), "mykey");
  EXPECT_EQ(args[2].asString(), "myvalue");
}

TEST(RespParserTest, ParsePipelinedCommands) {
  RespParser parser;
  std::string input = "*1\r\n$4\r\nPING\r\n"
                      "*3\r\n$3\r\nSET\r\n$1\r\na\r\n$1\r\nb\r\n";

  auto result = parser.parse(input);

  ASSERT_EQ(result.commands.size(), 2);

  const auto &cmd1 = result.commands[0].asArray();
  ASSERT_EQ(cmd1.size(), 1);
  EXPECT_EQ(cmd1[0].asString(), "PING");

  const auto &cmd2 = result.commands[1].asArray();
  ASSERT_EQ(cmd2.size(), 3);
  EXPECT_EQ(cmd2[0].asString(), "SET");
  EXPECT_EQ(cmd2[1].asString(), "a");
  EXPECT_EQ(cmd2[2].asString(), "b");

  EXPECT_EQ(result.bytesConsumed, input.size());
}

TEST(RespParserTest, PartialBulkString) {
  RespParser parser;
  auto result = parser.parse("$6\r\nfoo");

  EXPECT_EQ(result.commands.size(), 0);
  EXPECT_EQ(result.bytesConsumed, 0);
}

TEST(RespParserTest, PartialArray) {
  RespParser parser;
  auto result = parser.parse("*2\r\n$3\r\nfoo\r\n");

  EXPECT_EQ(result.commands.size(), 0);
  EXPECT_EQ(result.bytesConsumed, 0);
}

TEST(RespParserTest, PartialLineNoEndCRLF) {
  RespParser parser;
  auto result = parser.parse("+OK");

  EXPECT_EQ(result.commands.size(), 0);
  EXPECT_EQ(result.bytesConsumed, 0);
}

TEST(RespSerializerTest, SerializeSimpleString) {
  auto result = RespSerializer::serialize(RespValue::simpleString("OK"));
  EXPECT_EQ(result, "+OK\r\n");
}

TEST(RespSerializerTest, SerializeError) {
  auto result =
      RespSerializer::serialize(RespValue::error("ERR bad stuff"));
  EXPECT_EQ(result, "-ERR bad stuff\r\n");
}

TEST(RespSerializerTest, SerializeInteger) {
  auto result = RespSerializer::serialize(RespValue::integer(42));
  EXPECT_EQ(result, ":42\r\n");
}

TEST(RespSerializerTest, SerializeBulkString) {
  auto result =
      RespSerializer::serialize(RespValue::bulkString("hello"));
  EXPECT_EQ(result, "$5\r\nhello\r\n");
}

TEST(RespSerializerTest, SerializeNull) {
  auto result = RespSerializer::serialize(RespValue::null());
  EXPECT_EQ(result, "$-1\r\n");
}

TEST(RespSerializerTest, SerializeArray) {
  std::vector<RespValue> elements;
  elements.push_back(RespValue::bulkString("foo"));
  elements.push_back(RespValue::bulkString("bar"));

  auto result = RespSerializer::serialize(RespValue::array(elements));
  EXPECT_EQ(result, "*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
}

TEST(RespSerializerTest, ConvenienceMethods) {
  EXPECT_EQ(RespSerializer::ok(), "+OK\r\n");
  EXPECT_EQ(RespSerializer::pong(), "+PONG\r\n");
  EXPECT_EQ(RespSerializer::nullBulkString(), "$-1\r\n");
  EXPECT_EQ(RespSerializer::integer(0), ":0\r\n");
  EXPECT_EQ(RespSerializer::error("test"), "-ERR test\r\n");
}

TEST(RespRoundTripTest, BulkStringRoundTrip) {
  std::string original = "hello world";
  std::string serialized = RespSerializer::bulkString(original);

  RespParser parser;
  auto result = parser.parse(serialized);

  ASSERT_EQ(result.commands.size(), 1);
  EXPECT_EQ(result.commands[0].asString(), original);
}

TEST(RespRoundTripTest, ArrayRoundTrip) {
  std::vector<RespValue> elements;
  elements.push_back(RespValue::bulkString("SET"));
  elements.push_back(RespValue::bulkString("key"));
  elements.push_back(RespValue::bulkString("value"));
  auto array = RespValue::array(elements);

  std::string serialized = RespSerializer::serialize(array);

  RespParser parser;
  auto result = parser.parse(serialized);

  ASSERT_EQ(result.commands.size(), 1);
  const auto &parsed = result.commands[0].asArray();
  ASSERT_EQ(parsed.size(), 3);
  EXPECT_EQ(parsed[0].asString(), "SET");
  EXPECT_EQ(parsed[1].asString(), "key");
  EXPECT_EQ(parsed[2].asString(), "value");
}
