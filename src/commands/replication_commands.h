#pragma once

namespace redisdb {

class CommandRegistry;

void registerReplicationCommands(CommandRegistry &registry);

} // namespace redisdb
