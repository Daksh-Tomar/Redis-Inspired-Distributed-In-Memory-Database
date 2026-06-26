#include "server/config.h"
#include "server/server.h"

#include <csignal>
#include <iostream>
#include <string>

static redisdb::Server *g_server = nullptr;

void signalHandler(int sig) {
  std::cout << "\n[Signal] Received signal " << sig << ", shutting down..."
            << std::endl;
  if (g_server) {
    g_server->shutdown();
  }
}

int main(int argc, char *argv[]) {
  redisdb::ServerConfig config;
  std::string configFile;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
      config.port = std::stoi(argv[++i]);
    } else if ((arg == "--bind" || arg == "-b") && i + 1 < argc) {
      config.bindAddress = argv[++i];
    } else if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
      configFile = argv[++i];
    } else if (arg == "--maxclients" && i + 1 < argc) {
      config.maxClients = std::stoi(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "RedisDB — A Redis-compatible in-memory database"
                << std::endl;
      std::cout << std::endl;
      std::cout << "Usage: redisdb [options]" << std::endl;
      std::cout << "  --port, -p <port>       Port to listen on (default: 6379)"
                << std::endl;
      std::cout << "  --bind, -b <addr>       Address to bind to (default: 0.0.0.0)"
                << std::endl;
      std::cout << "  --config, -c <file>     Configuration file path"
                << std::endl;
      std::cout << "  --maxclients <n>        Maximum concurrent clients (default: 10000)"
                << std::endl;
      std::cout << "  --help, -h              Show this help message"
                << std::endl;
      return 0;
    } else if (arg[0] != '-' && configFile.empty()) {
      configFile = arg;
    }
  }

  if (!configFile.empty()) {
    config = redisdb::loadConfig(configFile);

    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
        config.port = std::stoi(argv[++i]);
      } else if ((arg == "--bind" || arg == "-b") && i + 1 < argc) {
        config.bindAddress = argv[++i];
      }
    }
  }

  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
#ifndef _WIN32
  signal(SIGHUP, SIG_IGN);
#endif

  redisdb::Server server(config);
  g_server = &server;

  if (!server.initialize()) {
    std::cerr << "[Main] Server initialization failed" << std::endl;
    return 1;
  }

  server.run();

  g_server = nullptr;
  return 0;
}
