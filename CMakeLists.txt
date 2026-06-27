cmake_minimum_required(VERSION 3.20)
project(RedisDB
    VERSION 0.1.0
    DESCRIPTION "A production-grade Redis-compatible in-memory database"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX- /permissive- /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

if(WIN32)
    add_compile_definitions(REDIS_PLATFORM_WINDOWS)
    message(STATUS "Platform: Windows — using select() event loop backend")
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(REDIS_PLATFORM_LINUX)
    message(STATUS "Platform: Linux — using epoll() event loop backend")
elseif(APPLE)
    add_compile_definitions(REDIS_PLATFORM_MACOS)
    message(STATUS "Platform: macOS — using select() event loop backend (kqueue planned)")
else()
    add_compile_definitions(REDIS_PLATFORM_GENERIC)
    message(STATUS "Platform: Unknown — using select() event loop backend")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(src)
add_subdirectory(tests)

message(STATUS "")
message(STATUS "RedisDB v${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "")
