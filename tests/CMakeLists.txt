enable_testing()

# ─── Unit Tests ──────────────────────────────────────────────────────────────

# RESP Parser Tests
add_executable(test_resp_parser unit/test_resp_parser.cpp)
target_link_libraries(test_resp_parser PRIVATE redisdb_lib GTest::gtest_main)
add_test(NAME RespParserTest COMMAND test_resp_parser)

# Database & Commands Tests
add_executable(test_db unit/test_db.cpp)
target_link_libraries(test_db PRIVATE redisdb_lib GTest::gtest_main)
add_test(NAME DatabaseTest COMMAND test_db)

add_executable(test_commands unit/test_commands.cpp)
target_link_libraries(test_commands PRIVATE redisdb_lib GTest::gtest_main)
add_test(NAME CommandsTest COMMAND test_commands)

add_executable(test_advanced_features unit/test_advanced_features.cpp)
target_link_libraries(test_advanced_features PRIVATE redisdb_lib GTest::gtest_main)
add_test(NAME AdvancedFeaturesTest COMMAND test_advanced_features)


# Persistence Tests
add_executable(test_persistence unit/test_persistence.cpp)
target_link_libraries(test_persistence PRIVATE redisdb_lib GTest::gtest_main)
add_test(NAME PersistenceTest COMMAND test_persistence)

# Data Structures Tests
add_executable(test_hash_table unit/test_hash_table.cpp)
target_link_libraries(test_hash_table PRIVATE redisdb_lib GTest::gtest_main)
add_test(NAME HashTableTest COMMAND test_hash_table)

add_executable(test_skip_list unit/test_skip_list.cpp)
target_link_libraries(test_skip_list PRIVATE redisdb_lib GTest::gtest_main)
add_test(NAME SkipListTest COMMAND test_skip_list)

add_executable(test_linked_list unit/test_linked_list.cpp)
target_link_libraries(test_linked_list PRIVATE redisdb_lib GTest::gtest_main)
add_test(NAME LinkedListTest COMMAND test_linked_list)

