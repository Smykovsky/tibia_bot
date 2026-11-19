cmake_minimum_required(VERSION 3.20)
project(TibiaBotEngine VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Homebrew paths on macOS Apple Silicon
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
endif()

find_package(Threads REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

# Find Lua
find_path(LUA_INCLUDE_DIR lua.h
    PATHS
    /opt/homebrew/include/lua
    /opt/homebrew/include/lua5.5
    /opt/homebrew/include
    /usr/local/include
)

find_library(LUA_LIBRARY
    NAMES lua lua5.5 liblua
    PATHS
    /opt/homebrew/lib
    /usr/local/lib
)

if(NOT LUA_INCLUDE_DIR OR NOT LUA_LIBRARY)
    message(FATAL_ERROR "Lua library or headers not found!")
endif()

message(STATUS "Found Lua Include: ${LUA_INCLUDE_DIR}")
message(STATUS "Found Lua Library: ${LUA_LIBRARY}")

# Engine Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LUA_INCLUDE_DIR}
)

# Engine Executable
add_executable(tibia_engine
    src/main.cpp
)

target_link_libraries(tibia_engine PRIVATE
    Threads::Threads
    nlohmann_json::nlohmann_json
    ${LUA_LIBRARY}
)

# Integration / Unit Test Executable
add_executable(engine_test
    tests/test_all.cpp
)

target_link_libraries(engine_test PRIVATE
    Threads::Threads
    nlohmann_json::nlohmann_json
    ${LUA_LIBRARY}
)
