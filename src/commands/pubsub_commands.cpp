#include "commands/pubsub_commands.h"
#include "commands/command_registry.h"
#include "networking/client.h"
#include "protocol/resp_serializer.h"
#include "server/server.h"


namespace redisdb {

void registerPubSubCommands(CommandRegistry &registry) {
  registry.registerCommand(
      "subscribe",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        client.setFlag(CLIENT_PUBSUB);
        for (size_t i = 1; i < args.size(); ++i) {
          std::string channel = args[i].asString();
          server.subscribeChannel(client, channel);

          std::vector<std::string> parts = {
              "subscribe", channel,
              std::to_string(client.pubsubChannels().size() +
                             client.pubsubPatterns().size())};
          std::string reply = "*3\r\n$9\r\nsubscribe\r\n$" +
                              std::to_string(channel.length()) + "\r\n" +
                              channel + "\r\n:" +
                              std::to_string(client.pubsubChannels().size() +
                                             client.pubsubPatterns().size()) +
                              "\r\n";
          client.addReply(reply);
        }
      },
      -2, CMD_FAST | CMD_PUBSUB,
      "Listen for messages published to the given channels");

  registry.registerCommand(
      "unsubscribe",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        if (args.size() == 1) {
          auto channels = client.pubsubChannels();
          for (const auto &channel : channels) {
            server.unsubscribeChannel(client, channel);
            std::string reply = "*3\r\n$11\r\nunsubscribe\r\n$" +
                                std::to_string(channel.length()) + "\r\n" +
                                channel + "\r\n:" +
                                std::to_string(client.pubsubChannels().size() +
                                               client.pubsubPatterns().size()) +
                                "\r\n";
            client.addReply(reply);
          }
        } else {
          for (size_t i = 1; i < args.size(); ++i) {
            std::string channel = args[i].asString();
            server.unsubscribeChannel(client, channel);
            std::string reply = "*3\r\n$11\r\nunsubscribe\r\n$" +
                                std::to_string(channel.length()) + "\r\n" +
                                channel + "\r\n:" +
                                std::to_string(client.pubsubChannels().size() +
                                               client.pubsubPatterns().size()) +
                                "\r\n";
            client.addReply(reply);
          }
        }

        if (client.pubsubChannels().empty() &&
            client.pubsubPatterns().empty()) {
          client.clearFlag(CLIENT_PUBSUB);
        }
      },
      -1, CMD_FAST | CMD_PUBSUB,
      "Stop listening for messages posted to the given channels");

  registry.registerCommand(
      "psubscribe",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        client.setFlag(CLIENT_PUBSUB);
        for (size_t i = 1; i < args.size(); ++i) {
          std::string pattern = args[i].asString();
          server.psubscribePattern(client, pattern);

          std::string reply = "*3\r\n$10\r\npsubscribe\r\n$" +
                              std::to_string(pattern.length()) + "\r\n" +
                              pattern + "\r\n:" +
                              std::to_string(client.pubsubChannels().size() +
                                             client.pubsubPatterns().size()) +
                              "\r\n";
          client.addReply(reply);
        }
      },
      -2, CMD_FAST | CMD_PUBSUB,
      "Listen for messages published to channels matching the given patterns");

  registry.registerCommand(
      "punsubscribe",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        if (args.size() == 1) {
          auto patterns = client.pubsubPatterns();
          for (const auto &pattern : patterns) {
            server.punsubscribePattern(client, pattern);
            std::string reply = "*3\r\n$12\r\npunsubscribe\r\n$" +
                                std::to_string(pattern.length()) + "\r\n" +
                                pattern + "\r\n:" +
                                std::to_string(client.pubsubChannels().size() +
                                               client.pubsubPatterns().size()) +
                                "\r\n";
            client.addReply(reply);
          }
        } else {
          for (size_t i = 1; i < args.size(); ++i) {
            std::string pattern = args[i].asString();
            server.punsubscribePattern(client, pattern);
            std::string reply = "*3\r\n$12\r\npunsubscribe\r\n$" +
                                std::to_string(pattern.length()) + "\r\n" +
                                pattern + "\r\n:" +
                                std::to_string(client.pubsubChannels().size() +
                                               client.pubsubPatterns().size()) +
                                "\r\n";
            client.addReply(reply);
          }
        }

        if (client.pubsubChannels().empty() &&
            client.pubsubPatterns().empty()) {
          client.clearFlag(CLIENT_PUBSUB);
        }
      },
      -1, CMD_FAST | CMD_PUBSUB,
      "Stop listening for messages posted to channels matching the given "
      "patterns");

  registry.registerCommand(
      "publish",
      [](Server &server, Client &client, const std::vector<RespValue> &args) {
        std::string channel = args[1].asString();
        std::string message = args[2].asString();

        int receivers = server.publishMessage(channel, message);
        client.addReply(":" + std::to_string(receivers) + "\r\n");
      },
      3, CMD_FAST | CMD_PUBSUB, "Post a message to a channel");
}

} // namespace redisdb
