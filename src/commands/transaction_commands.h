#pragma once

namespace redisdb {

class CommandRegistry;

void registerTransactionCommands(CommandRegistry &registry);

} // namespace redisdb
