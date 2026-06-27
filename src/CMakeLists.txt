set(REDISDB_SOURCES
    server/server.cpp
    server/replication.cpp
    server/config.cpp
    server/memory_manager.cpp

    networking/event_loop.cpp
    networking/tcp_server.cpp
    networking/client.cpp
    networking/connection_manager.cpp

    networking/platform/event_loop_select.cpp

    protocol/resp_parser.cpp
    protocol/resp_serializer.cpp

    commands/command_registry.cpp
    commands/string_commands.cpp
    commands/key_commands.cpp
    commands/server_commands.cpp
    commands/list_commands.cpp
    commands/hash_commands.cpp
    commands/set_commands.cpp
    commands/zset_commands.cpp
    commands/transaction_commands.cpp
    commands/pubsub_commands.cpp
    commands/replication_commands.cpp

    storage/db.cpp
    storage/redis_object.cpp
    storage/rdb_persistence.cpp
    storage/aof_persistence.cpp
    storage/data_structures/linked_list.cpp
    storage/data_structures/skip_list.cpp
    storage/data_structures/sorted_set.cpp
)

if(UNIX AND NOT APPLE)
    list(APPEND REDISDB_SOURCES networking/platform/event_loop_epoll.cpp)
endif()

add_executable(redisdb main.cpp ${REDISDB_SOURCES})

target_include_directories(redisdb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_libraries(redisdb PRIVATE ws2_32)
endif()

add_library(redisdb_lib STATIC ${REDISDB_SOURCES})
target_include_directories(redisdb_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(redisdb_lib PUBLIC ws2_32)
endif()
