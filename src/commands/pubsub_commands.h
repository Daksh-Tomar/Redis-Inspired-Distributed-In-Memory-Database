#pragma once

namespace redisdb {

class CommandRegistry;

void registerPubSubCommands(CommandRegistry &registry);

} // namespace redisdb
